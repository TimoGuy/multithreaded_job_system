#include "JobManager.h"

#include <cassert>
#include <algorithm>
#include <iostream>  // @TEMP
#include "TracyImpl.h"
#include "JobStatistics.h"


JobManager::JobManager(job_manager_callback_fn_t& on_empty_jobs_fn, uint32_t num_consumer_queues)
    : m_on_empty_jobs_fn(on_empty_jobs_fn)
{
    ZoneScoped;

    m_consumer_queues.resize(num_consumer_queues);
}

// @NOTE: it's assumed that this isn't executed while the pending
//        group to executing switch is being made.
void JobManager::executeNextJob(uint32_t thread_idx)
{
    ZoneScoped;

    // State Machine.
    switch (m_current_mode)
    {
    case MODE_GATHER_JOBS:
    {
        // Force only one thread to execute this mode by forcing
        // sequential execution and 1st thread invalidating the
        // mode before 2nd+ threads can enter.
        std::lock_guard<std::mutex> lock{ m_gather_jobs_mutex };
        if (m_current_mode == MODE_GATHER_JOBS)
        {
#if JOBSTATS_ENABLE
            // Get stats report.
            std::cout
                << "======================================================"
                << JOBSTATS_GENERATE_REPORT;
#endif

            // Solicit jobs and perform sorting.
            std::vector<Job*> solicited_all_jobs{ std::move(m_on_empty_jobs_fn()) };
            for (auto& pending_joblist : m_pending_joblists)
            {
                pending_joblist.lock();
            }
            for (auto job : solicited_all_jobs)
            {
                m_pending_joblists[job->m_group].push_back(job);
            }
            for (auto& pending_joblist : m_pending_joblists)
            {
                pending_joblist.unlock();
            }

            movePendingJobsIntoExecQueue();

            transitionCurrentModeAtomic(
                MODE_GATHER_JOBS,
                MODE_RESERVE_AND_EXECUTE_JOBS
            );
        }
        break;
    }

    case MODE_RESERVE_AND_EXECUTE_JOBS:
        // If no more jobs to reserve, move to waiting
        // until job execution finishes.
        if (isAllJobsReserved())
        {
            transitionCurrentModeAtomic(
                MODE_RESERVE_AND_EXECUTE_JOBS,
                MODE_WAIT_UNTIL_EXECUTION_FINISHED
            );
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
            transitionCurrentModeAtomic(
                MODE_WAIT_UNTIL_EXECUTION_FINISHED,
                MODE_GATHER_JOBS
            );
        }
        break;
    }
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
    JOBSTATS_RECORD_START(job->m_name);
    job->execute();
    JOBSTATS_RECORD_END;

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
}

void JobManager::movePendingJobsIntoExecQueue()
{
    ZoneScoped;

    size_t total_jobs{ 0 };

    for (uint8_t group_idx = 0; group_idx < JobGroup_e::NUM_JOB_GROUPS; group_idx++)
    {
        {
            // Lock reading and writing buffers.
            auto& joblists{ m_executing_grouped_joblists[group_idx] };
            std::lock_guard<std::mutex> lock_joblists{ joblists };

            auto& pending_joblist{ m_pending_joblists[group_idx] };
            std::lock_guard<std::mutex> lock_pending_joblist{ pending_joblist };

            // Add to total count.
            total_jobs += pending_joblist.size();

            // Order joblist order into spans.
            std::map<order_t, std::vector<Job*>> joblist_spans;
            for (auto job : pending_joblist)
            {
                joblist_spans[job->m_order].push_back(job);
            }

            // Read spans in order into new joblists.
            joblists.clear();
            joblists.reserve(joblist_spans.size());
            for (auto it = joblist_spans.begin(); it != joblist_spans.end(); it++)
            {
                joblists.emplace_back(std::move(it->second));
            }

            // Clear pending joblist.
            pending_joblist.clear();
        }

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
        for (uint32_t i = current_idx; i < last_idx; i++)
        {
            queue.push_back(jobs[i]);
        }

        current_idx = last_idx;
    }
}

void JobManager::loadJoblistGroupIntoConsumerQueues(JobGroup_e job_group, uint32_t group_idx)
{
    auto& s{ m_executing_iteration_state[job_group] };

    s.group_idx = group_idx;

    auto& joblists{ m_executing_grouped_joblists[job_group] };
    std::lock_guard<std::mutex> lock{ joblists };

    // Group exists.
    if (s.group_idx < joblists.size())
    {
        auto& joblist{ joblists[s.group_idx] };
        s.remaining_unfinished_jobs = static_cast<uint32_t>(joblist.size());
        insertJobsIntoConsumerQueues(std::move(joblist));
    }
}
