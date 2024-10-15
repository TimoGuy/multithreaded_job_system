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
    JobManager(std::function<void(JobManager&)>&& on_empty_jobs_fn, uint32_t num_consumer_queues);

    // To be executed during job execution or in `on_empty_jobs_fn`.
    void emplaceJob(Job* job);  // @TODO: if executed during job execution, need to add a mutex on emplacing jobs.

    // To be executed by each individual worker thread.
    void executeNextJob(uint32_t thread_idx);

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
    std::mutex m_gather_jobs_mutex;

    inline void transitionCurrentModeAtomic(Mode_e from, Mode_e to)
    {
        (void)m_current_mode.compare_exchange_strong(from, to);
    }

    /*enum FetchResult_e
    {
        RESULT_ALL_JOBS_COMPLETE = 0,
        RESULT_WAIT_FOR_NEXT_BATCH,
        RESULT_JOB_RESERVED,
        RESULT_NO_JOB_RESERVED,
    };*/

    void reserveAndExecuteNextJob(uint32_t thread_idx);

    inline bool isAllJobsReserved()
    {
        return (m_remaining_unreserved_jobs == 0);
    }

    inline bool isAllJobExecutionFinished()
    {
        return (m_remaining_unfinished_jobs == 0);
    }

    //FetchResult_e fetchExecutingJob(
    //    uint8_t& out_job_group_idx,
    //    size_t& out_jobspan_idx,
    //    Job*& out_job_obj);

    /*void reportJobFinishExecuting(uint8_t job_group_idx, size_t jobspan_idx);*/

    //void waitUntilExecutingQueueUnused();
    void movePendingJobsIntoExecQueue();

    /*struct JobGroup
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
    };*/

    template <class T>
    class LockableRingQueue : public std::mutex
    {
    public:
        LockableRingQueue() = default;

        LockableRingQueue(LockableRingQueue<T>&& other)
            : m_elements(std::move(other.m_elements))
            , m_front_idx(other.m_front_idx)
            , m_back_idx(other.m_back_idx)
#ifdef _DEBUG
            , m_debug_counter(other.m_debug_counter.load())
#endif
        {
        }

        bool is_empty()
        {
            return (m_front_idx == m_back_idx);
        }

        void push_back(T elem)
        {
            m_elements[m_back_idx] = elem;
            m_back_idx = offset_idx(m_back_idx, 1);
#ifdef _DEBUG
            m_debug_counter++;  // @FIXME: even tho the jobs should all get popped off before filling in more jobs, occasionally (like around 1/25 times) the consumer queues get filled with over 1000 jobs even though the job solicitation thingo is just inserting 100 jobs, which should get divided up into multiple cores. However, the pending joblists indicate up to 900 jobs coming from the 100 job thing. idk what is happening imo.
#endif
            assert(!is_empty());
        }

        T pop_front()
        {
            assert(!is_empty());
            T ret{ m_elements[m_front_idx] };
            m_front_idx = offset_idx(m_front_idx, 1);
#ifdef _DEBUG
            m_debug_counter--;
#endif
            return ret;
        }

    private:
        inline uint32_t offset_idx(uint32_t start, uint32_t offset)
        {
            return (start + offset) % k_max_elements;
        }

        inline static constexpr uint32_t k_max_elements{ 1024 };
        std::array<T, k_max_elements> m_elements;
        uint32_t m_front_idx{ 0 };
        uint32_t m_back_idx{ 0 };
#ifdef _DEBUG
        std::atomic_uint32_t m_debug_counter{ 0 };
#endif
    };

    std::vector<LockableRingQueue<Job*>> m_consumer_queues;

    Job* getJobFromConsumerQueue(bool block, uint32_t queue_idx);
    void insertJobsIntoConsumerQueues(std::vector<Job*>&& jobs);

    template <class T>
    class LockableVector : public std::vector<T>, public std::mutex { };

    // When queuing up jobs, the currently executing packed
    // set of jobs `executingGroups` must be immutable.
    // If new jobs are added such as deleting an object, the
    // job will be only added into the next executing group.

    //struct JobQueue
    //{
    //    std::array<JobGroup, JobGroup_e::NUM_JOB_GROUPS> groups;
    //    std::atomic_size_t remaining_unreserved_jobs;
    //    std::atomic_size_t remaining_unfinished_jobs;
    //    //std::atomic_size_t num_threads_using_executing_queue;
    //} m_executing_queue;

    std::atomic_size_t m_remaining_unreserved_jobs{ 0 };
    std::atomic_size_t m_remaining_unfinished_jobs{ 0 };

    struct LockableJobGroupIterationState : public std::mutex
    {
        uint32_t group_idx;
        uint32_t remaining_unfinished_jobs;
    };
    std::array<LockableJobGroupIterationState, JobGroup_e::NUM_JOB_GROUPS> m_executing_iteration_state;
    std::array<LockableVector<std::vector<Job*>>, JobGroup_e::NUM_JOB_GROUPS> m_executing_grouped_joblists;

    // @NOTE: This is a non-threadsafe method!
    void loadJoblistGroupIntoConsumerQueues(JobGroup_e job_group, uint32_t group_idx);

    std::array<LockableVector<Job*>, JobGroup_e::NUM_JOB_GROUPS> m_pending_joblists;
    //std::atomic_bool m_refill_executing_queue_claimed{ false };

    std::function<void(JobManager&)> m_on_empty_jobs_fn;
#ifdef _DEBUG
    std::atomic_bool m_is_in_job_switch{ false };
#endif
};

