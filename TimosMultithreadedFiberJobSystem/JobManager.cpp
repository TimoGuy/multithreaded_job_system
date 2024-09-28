#include "JobManager.h"

#include <cassert>
#include <iostream>  // @TEMP
#include "TracyImpl.h"


JobManager::JobManager(std::function<void(JobManager&)>&& on_empty_jobs_fn)
    : m_on_empty_jobs_fn(on_empty_jobs_fn)
{
    ZoneScoped;

    // @NOTE: Setting this to 0 will trigger populating a new
    //        round of jobs upon the first `executeNextJob()`.
    m_executing_queue.remaining_unfinished_jobs = 0;

    // Initial value just needs to be set.
    m_executing_queue.num_threads_using_executing_queue = 0;
}

void JobManager::emplaceJob(Job* job)
{
    ZoneScoped;

    // @CHECK: may have to add editing mutex,
    //         but I'm pretty sure it's not needed.
    /*bool check{ m_is_in_job_switch };  // @NOTE: `m_is_in_job_switch` is atomic, but it's invisible amongst the different threads, so commneting out this assert for now.
    assert(!m_is_in_job_switch);*/
    m_pending_joblists[job->m_group].push_back(job);
}

// @DEBUG
inline static std::atomic_uint8_t jojo = 0;
inline static uint8_t kkkk[256];

// @NOTE: it's assumed that this isn't executed while the pending
//        group to executing switch is being made.
void JobManager::executeNextJob()
{
    ZoneScoped;

    // Reserve job.
    uint8_t job_group_idx;
    size_t jobspan_idx;
    size_t job_idx;
    FetchResult_e res{
        fetchExecutingJob(job_group_idx, jobspan_idx, job_idx) };

    // @DEBUG
    kkkk[jojo++] = static_cast<uint8_t>(res);

    switch (res)
    {
    case FetchResult_e::RESULT_ALL_JOBS_COMPLETE:
    {
        // No jobs to fetch. Prepare pending group.
        // @NOTE: if one thread made it into this branch, then it means
        //        that all other threads will try to make it into this
        //        branch. Since only one thread* can do this job, there's
        //        a unique lock that only allows in one thread* to do the
        //        work.
        //     *: maybe in the future multiple threads can work on
        //        separate jobqueue groups instead of just one thread
        //        doing everything.

        // Execute empty jobs callback (last minute pending jobs).
        // @NOTE: this callback can be used to solicit new
        //        jobs from various parts of the program.
        if (m_on_empty_jobs_fn)
            m_on_empty_jobs_fn(*this);

#ifdef _DEBUG
        m_is_in_job_switch = true;
#endif
        waitUntilExecutingQueueUnused();
        movePendingJobsIntoExecQueue();
#ifdef _DEBUG
        m_is_in_job_switch = false;
#endif
        break;
    }

    case FetchResult_e::RESULT_JOB_RESERVED:
        // Execute fetched job.
        (void)m_executing_queue
            .groups[job_group_idx]
            .jobspans[jobspan_idx]
            .jobs[job_idx]
            ->execute();  // @TODO: don't ignoore the returned int.
        reportJobFinishExecuting(job_group_idx, jobspan_idx, job_idx);
        break;

    case FetchResult_e::RESULT_NO_JOB_RESERVED:
        m_executing_queue.num_threads_using_executing_queue--;
    case FetchResult_e::RESULT_WAIT_FOR_NEXT_BATCH:
    default:
        break;
    }
}

JobManager::FetchResult_e JobManager::fetchExecutingJob(
    uint8_t& out_job_group_idx,
    size_t& out_jobspan_idx,
    size_t& out_job_idx)
{
    ZoneScoped;

    FetchResult_e ret{ FetchResult_e::RESULT_NO_JOB_RESERVED };

    // Completion reached.
    if (m_executing_queue.remaining_unfinished_jobs == 0)
    {
        bool expected = false;
        if (m_refill_executing_queue_claimed.compare_exchange_weak(expected, true))
        {
            ret = FetchResult_e::RESULT_ALL_JOBS_COMPLETE;
        }
        else
        {
            ret = FetchResult_e::RESULT_WAIT_FOR_NEXT_BATCH;
        }
    }
    // No more jobs to reserve. Wait for completion/next batch.
    else if (m_executing_queue.remaining_unreserved_jobs == 0)
    {
        ret = FetchResult_e::RESULT_WAIT_FOR_NEXT_BATCH;
    }
    // Possibly another job to reserve.
    else
    {
        // Put hold on exec queue as accessing occurs.
        m_executing_queue.num_threads_using_executing_queue++;

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
                out_job_group_idx = group_idx;
                out_jobspan_idx = jobspan_idx;
                out_job_idx = attempt_to_reserve_idx;
                ret = FetchResult_e::RESULT_JOB_RESERVED;

                m_executing_queue.remaining_unreserved_jobs--;

                break;  // Leave the search. Fetch is finished.
            }
            else
            {
                // No jobs left in group. Do some housekeeping and reset the
                // remaining unreserved jobs to 0 to minimize the chances
                // of too many decrements happening at the same time and
                // the program thinking it was able to successfully reserve
                // a job.
                ret = FetchResult_e::RESULT_NO_JOB_RESERVED;
                jobspan.remaining_unreserved_jobs = 0;
            }
        }
    }

    return ret;
}

void JobManager::reportJobFinishExecuting(
    uint8_t job_group_idx,
    size_t jobspan_idx,
    size_t job_idx)
{
    ZoneScoped;

    (void)job_idx;  // @TODO: maybe remove this as an arg?

    auto& group{ m_executing_queue.groups[job_group_idx] };
    auto& jobspan{ group.jobspans[jobspan_idx] };

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

    m_executing_queue.num_threads_using_executing_queue--;
    m_executing_queue.remaining_unfinished_jobs--;  // @NOTE: update very last.
}

void JobManager::waitUntilExecutingQueueUnused()
{
    ZoneScoped;
    while (m_executing_queue.num_threads_using_executing_queue > 0)
    {
    }
}

void JobManager::movePendingJobsIntoExecQueue()
{
    ZoneScoped;

    //std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t total_jobs{ 0 };

    for (uint8_t group_idx = 0; group_idx < JobGroup_e::NUM_JOB_GROUPS; group_idx++)
    {
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
    }

    // Unlock executing queue.
    m_refill_executing_queue_claimed = false;
    m_executing_queue.remaining_unreserved_jobs = total_jobs;
    m_executing_queue.remaining_unfinished_jobs = total_jobs;  // Do this last since it's the first checked value!
}
