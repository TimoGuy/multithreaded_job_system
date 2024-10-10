#include "JobManager.h"

#include <cassert>
#include <algorithm>
#include <iostream>  // @TEMP
#include "TracyImpl.h"


JobManager::JobManager(std::function<void(JobManager&)>&& on_empty_jobs_fn)
    : m_on_empty_jobs_fn(on_empty_jobs_fn)
{
    ZoneScoped;

    // @NOTE: Setting this to 0 will trigger populating a new
    //        round of jobs upon the first `executeNextJob()`.
    m_remaining_unfinished_jobs = 0;
}

void JobManager::emplaceJob(Job* job)
{
    ZoneScoped;

    // @TODO: add lock if `emplaceJob()` is called
    //        from inside a job.

    emplaceJobNoLock(job);
}

// @NOTE: it's assumed that this isn't executed while the pending
//        group to executing switch is being made.
void JobManager::executeNextJob(uint32_t thread_idx)
{
    ZoneScoped;

    // State Machine.
    switch (m_current_mode)
    {
    case MODE_MUTATE_PARTY_LIST:
        // @TODO: add entities and stuff and mutate the entity list
        //        here. For now, just move to the next state.

        m_num_threads_gathering_jobs = 0;
        m_current_mode = MODE_GATHER_JOBS;
        break;

    case MODE_GATHER_JOBS:
    {
        constexpr uint8_t k_allowed_gathering_threads{ 1 };
        uint8_t gathering_thread_idx{ m_num_threads_gathering_jobs++ };
        if (gathering_thread_idx < k_allowed_gathering_threads)
        {
            // Solicit jobs and perform sorting.
            if (m_on_empty_jobs_fn)
                m_on_empty_jobs_fn(*this);

            //waitUntilExecutingQueueUnused();  @CHECK: this should be unnecessary.
            movePendingJobsIntoExecQueue();

            m_current_mode = MODE_RESERVE_AND_EXECUTE_JOBS;
        }
        break;
    }

    case MODE_RESERVE_AND_EXECUTE_JOBS:
        // If no more jobs to reserve, move to waiting
        // until job execution finishes.
        if (isAllJobsReserved())
        {
            m_current_mode = MODE_WAIT_UNTIL_EXECUTION_FINISHED;
        }
        else
        {
            reserveAndExecuteNextJob(thread_idx);
        }
        break;

    case MODE_WAIT_UNTIL_EXECUTION_FINISHED:
        // Wait until there are no more unfinished jobs.
        if (isAllJobExecutionFinished())
        {
            m_current_mode = MODE_MUTATE_PARTY_LIST;
        }
        break;
    }
}

void JobManager::emplaceJobNoLock(Job* job)
{
    ZoneScoped;
    m_pending_joblists[job->m_group].push_back(job);
}

void JobManager::reserveAndExecuteNextJob(uint32_t thread_idx)
{
    // Reserve a job.
    uint32_t queue_idx{ thread_idx };
    Job* job{ getJobFromConsumerQueue(true, queue_idx) };
    while (job == nullptr)
    {
        queue_idx = (queue_idx + 1) % m_consumer_queues.size();

        // Finished loop without getting job. Abort.
        if (queue_idx == thread_idx)
            return;

        // Try again (without blocking).
        job = getJobFromConsumerQueue(false, queue_idx);
    }

    // Publish reservation of job.
    assert(job != nullptr);
    m_remaining_unreserved_jobs--;

    // Execute job.
    job->execute();

    // Publish finish of job.
    auto& s{ m_executing_iteration_state[job->m_group] };
    {
        std::lock_guard<std::mutex> lock{ s };

        s.remaining_unfinished_jobs--;
        if (s.remaining_unfinished_jobs == 0)
        {
            // Load next group into consumer queues.
            loadJoblistGroupIntoConsumerQueues(job->m_group, s.group_idx + 1);
        }
    }

    m_remaining_unfinished_jobs--;  // Update global stat last.

















#if 0
    // @TODO: have the biased job group idx (maybe a ref)
    //        so that there could be less incrementing and fighting.
    for (uint8_t group_idx = 0; group_idx < m_executing_queue.groups.size(); group_idx++)
    {
        auto& group{ m_executing_queue.groups[group_idx] };

        if (group.jobspans.empty())
        {
            // Don't search thru empty group.
            continue;
        }

        size_t jobspan_idx{ group.current_jobspan_idx };  // Capture so it's immutable from another thread.

        if (jobspan_idx >= group.jobspans.size())
        {
            // Don't search thru non-existant jobspan.
            continue;
        }

        auto& jobspan{ group.jobspans[jobspan_idx] };

        // This is what's happening in the line below:
        //   Step 1: Decrement atomic unreserved jobs, then get the result.
        //           Ex: 0 -> ret(1.8e19), 1 -> ret(0), 32 -> ret(31)
        //   Step 2: Check that the returned number is less than the jobs
        //           size. If there are 0 jobs left, it wraps around since
        //           it's unsigned. If it's successful, then indicates
        //           successful job reservation.
        // @NOTE: this job reservation system allows us to not need a mutex
        //        or any sync struct to manage access to the current jobspan.
        size_t attempt_to_reserve_idx{ --jobspan.remaining_unreserved_jobs };
        if (attempt_to_reserve_idx < jobspan.jobs.size())
        {
            // Successfully was able to reserve a job!
            m_executing_queue.remaining_unreserved_jobs--;

            // Execute reserved job.
            m_executing_queue
                .groups[group_idx]
                .jobspans[jobspan_idx]
                .jobs[attempt_to_reserve_idx]
                ->execute();
            
            // Log job as finished.
            size_t remaining_jobs{ --jobspan.remaining_unfinished_jobs };
            if (remaining_jobs == 0)
            {
                // Move to the next span.
                // @NOTE: due to fetching jobs using the `remaining_unreserved_jobs`
                //        atomic counter, if the jobspan is incremented to be out
                //        of range, then the jobspan index is actually never used,
                //        so no errors!
                group.current_jobspan_idx++;
            }

            m_executing_queue.remaining_unfinished_jobs--;  // @NOTE: update very last.

            break;  // Leave the search. Fetch is finished.
        }
        else
        {
            // No jobs left in group. Do some housekeeping and reset the
            // remaining unreserved jobs to 0 to minimize the chances
            // of too many decrements happening at the same time and
            // the program thinking it was able to successfully reserve
            // a job.
            jobspan.remaining_unreserved_jobs = 0;
        }
    }
#endif
}

void JobManager::movePendingJobsIntoExecQueue()
{
    ZoneScoped;

    size_t total_jobs{ 0 };

    // @TODO: figure out how you wanna do the job consumption!!!!! And how to manage jobs across multiple threads.
    //        maybe have a fetch to get new jobs whenever a group of jobs gets finished?

    for (uint8_t group_idx = 0; group_idx < JobGroup_e::NUM_JOB_GROUPS; group_idx++)
    {
        // @TODO: START HERE TIMOOOOOOOOOOOO!!!!!!!!!!!!!!!
        //        Figure out how to group the jobs into their lockable group lists.
#if 0
        auto& exec_jobgroup{ m_executing_queue.groups[group_idx] };
        auto& joblist{ m_pending_joblists[group_idx] };

        // Add to total count.
        total_jobs += joblist.size();

        // Reset jobgroup.
        exec_jobgroup.jobspans.clear();
        exec_jobgroup.current_jobspan_idx = 0;

        // Order joblist into jobspans.
        std::map<order_t, std::vector<Job*>> ordered_joblist;
        for (auto job : joblist)
        {
            ordered_joblist[job->m_order].push_back(job);
        }

        // Create jobspans.
        exec_jobgroup.jobspans.reserve(ordered_joblist.size());
        for (auto it = ordered_joblist.begin(); it != ordered_joblist.end(); it++)
        {
            JobGroup::JobSpan new_jobspan{
                std::move(it->second),
                it->second.size()
            };
            exec_jobgroup.jobspans.emplace_back(std::move(new_jobspan));
        }

        // Cleanup.
        joblist.clear();
#endif

        // Load initial joblist group.
        std::lock_guard<std::mutex> lock{ m_executing_iteration_state[group_idx] };
        loadJoblistGroupIntoConsumerQueues(static_cast<JobGroup_e>(group_idx), 0);
    }

    // Write global progress.
    m_remaining_unreserved_jobs = total_jobs;
    m_remaining_unfinished_jobs = total_jobs;
}

Job* JobManager::getJobFromConsumerQueue(bool block, uint32_t queue_idx)
{
    auto& queue = m_consumer_queues[queue_idx];
    Job* job{ nullptr };

    // Start access.
    bool lock_success;
    if (block)
    {
        queue.lock();
        lock_success = true;
    }
    else
        lock_success = queue.try_lock();

    if (lock_success)
    {
        // Pull one job from queue.
        if (!queue.is_empty())
        {
            job = queue.pop_front();
        }

        // End access.
        queue.unlock();
    }

    return job;
}

void JobManager::insertJobsIntoConsumerQueues(std::vector<Job*>&& jobs)
{
    uint32_t total_jobs{ static_cast<uint32_t>(jobs.size()) };
    uint32_t num_jobs_per_queue{
        static_cast<uint32_t>(std::ceilf(static_cast<float_t>(total_jobs) / m_consumer_queues.size())) };
    uint32_t current_idx{ 0 };

    for (auto& queue : m_consumer_queues)
    {
        uint32_t last_idx{ std::min(current_idx + num_jobs_per_queue, total_jobs) };

        std::lock_guard<std::mutex> lock{ queue };
        for (uint32_t i = 0; i < last_idx; i++)
        {
            queue.push_back(jobs[i]);
        }
    }
}

void JobManager::loadJoblistGroupIntoConsumerQueues(JobGroup_e job_group, uint32_t group_idx)
{
    auto& s{ m_executing_iteration_state[job_group] };
    auto& groups{ m_executing_grouped_joblists[job_group] };

    s.group_idx = group_idx;

    std::lock_guard<std::mutex> lock{ groups };

    // Group exists.
    if (s.group_idx < groups.size())
    {
        auto& group{ groups[s.group_idx] };
        s.remaining_unfinished_jobs = static_cast<uint32_t>(group.size());
        insertJobsIntoConsumerQueues(std::move(group));
    }
}
