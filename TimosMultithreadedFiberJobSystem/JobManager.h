#pragma once

#include <array>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <cassert>
#include "Job.h"
#include "JobGroupType.h"


class JobManager
{
public:
    JobManager(std::function<void(JobManager&)>&& on_empty_jobs_fn);

    // To be executed during job execution or in `on_empty_jobs_fn`.
    void emplaceJob(Job* job);  // @TODO: if executed during job execution, need to add a mutex on emplacing jobs.

    // To be executed by each individual worker thread.
    void executeNextJob();

private:
    void emplaceJobNoLock(Job* job);

    enum Mode_e : uint8_t
    {
        MODE_MUTATE_PARTY_LIST = 0,
        MODE_GATHER_JOBS,
        MODE_RESERVE_AND_EXECUTE_JOBS,
        MODE_WAIT_UNTIL_EXECUTION_FINISHED,

        NUM_MODES,
    };
    std::atomic<Mode_e> m_current_mode{ MODE_MUTATE_PARTY_LIST };
    std::atomic_uint8_t m_num_threads_gathering_jobs{ 0 };

    /*enum FetchResult_e
    {
        RESULT_ALL_JOBS_COMPLETE = 0,
        RESULT_WAIT_FOR_NEXT_BATCH,
        RESULT_JOB_RESERVED,
        RESULT_NO_JOB_RESERVED,
    };*/

    bool reserveAndExecuteNextJob();

    inline bool isAllJobExecutionFinished()
    {
        return (m_executing_queue.remaining_unfinished_jobs == 0);
    }

    //FetchResult_e fetchExecutingJob(
    //    uint8_t& out_job_group_idx,
    //    size_t& out_jobspan_idx,
    //    Job*& out_job_obj);

    /*void reportJobFinishExecuting(uint8_t job_group_idx, size_t jobspan_idx);*/

    //void waitUntilExecutingQueueUnused();
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

    class RingQueue
    {
    public:
        bool is_empty()
        {
            return (m_front_idx == m_back_idx);
        }

        void push_back(Job* elem)
        {
            m_jobs[m_back_idx] = elem;
            m_back_idx++;
            assert(m_front_idx != m_back_idx);
        }

        Job* pop_front()
        {
            assert(m_front_idx != m_back_idx);
            Job* ret{ m_jobs[m_front_idx] };
            m_front_idx++;
            return ret;
        }

    private:
        inline uint32_t offset_idx(uint32_t start, uint32_t offset)
        {
            return (start + offset) % k_max_entries;
        }

        inline static constexpr uint32_t k_max_entries{ 1024 };
        Job* m_jobs[k_max_entries];
        uint32_t m_front_idx{ 0 };
        uint32_t m_back_idx{ 0 };
    };

    struct ThreadSafeJobConsumerQueue
    {
        std::mutex access_mutex;

        RingQueue jobs;
    };
    std::vector<ThreadSafeJobConsumerQueue> m_consumer_queues;

    Job* getJobFromConsumerQueue(bool block, uint32_t queue_idx);


    // When queuing up jobs, the currently executing packed
    // set of jobs `executingGroups` must be immutable.
    // If new jobs are added such as deleting an object, the
    // job will be only added into the next executing group.
    struct JobQueue
    {
        std::array<JobGroup, JobGroup_e::NUM_JOB_GROUPS> groups;
        std::atomic_size_t remaining_unreserved_jobs;
        std::atomic_size_t remaining_unfinished_jobs;
        //std::atomic_size_t num_threads_using_executing_queue;
    } m_executing_queue;
    std::array<std::vector<Job*>, JobGroup_e::NUM_JOB_GROUPS> m_pending_joblists;
    std::atomic_bool m_refill_executing_queue_claimed{ false };

    std::function<void(JobManager&)> m_on_empty_jobs_fn;
#ifdef _DEBUG
    std::atomic_bool m_is_in_job_switch{ false };
#endif
};

