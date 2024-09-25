#pragma once

#include <array>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include "Job.h"
#include "JobGroupType.h"


class JobManager
{
public:
    JobManager(std::function<void()>&& on_empty_jobs_fn);

    // To be executed during job execution or in `on_empty_jobs_fn`.
    void emplaceJob(Job* job);

    // To be executed by each individual worker thread.
    void executeNextJob();

private:
    enum FetchResult_e
    {
        RESULT_ALL_JOBS_COMPLETE = 0,
        RESULT_SUCCESS,
        RESULT_BUSY,
    };

    FetchResult_e fetchExecutingJob(
        uint8_t& out_job_group_idx,
        size_t& out_jobspan_idx,
        size_t& out_job_idx);

    void reportJobFinishExecuting(
        uint8_t job_group_idx,
        size_t jobspan_idx,
        size_t job_idx);

    void movePendingJobsIntoExecQueue();

    struct JobGroup
    {
        struct JobSpan
        {
            JobSpan(std::vector<Job*>&& jobs, std::size_t total_jobs)
                : jobs(std::move(jobs))
                , remaining_unreserved_jobs(total_jobs)
                , remaining_unfinished_jobs(total_jobs)
            {
            }

            JobSpan(JobSpan&& other) noexcept
                : jobs(std::move(other.jobs))
                , remaining_unreserved_jobs(static_cast<size_t>(other.remaining_unreserved_jobs))
                , remaining_unfinished_jobs(static_cast<size_t>(other.remaining_unfinished_jobs))
            {
            }

            std::vector<Job*> jobs;
            std::atomic_size_t remaining_unreserved_jobs;
            std::atomic_size_t remaining_unfinished_jobs;
        };
        std::vector<JobSpan> jobspans;
        std::atomic_size_t current_jobspan_idx;
    };

    // When queuing up jobs, the currently executing packed
    // set of jobs `executingGroups` must be immutable.
    // If new jobs are added such as deleting an object, the
    // job will be only added into the next executing group.
    struct JobQueue
    {
        std::array<JobGroup, JobGroup_e::NUM_JOB_GROUPS> groups;
        std::atomic_size_t remaining_unreserved_jobs;
        std::atomic_size_t remaining_unfinished_jobs;
    } m_executing_queue;
    std::array<std::vector<Job*>, JobGroup_e::NUM_JOB_GROUPS> m_pending_joblists;

    std::mutex m_handle_job_switch_mutex;
    std::function<void()> m_on_empty_jobs_fn;
#ifdef _DEBUG
    std::atomic_bool m_is_in_job_switch{ false };
#endif
};

