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

    void reserveAndExecuteNextJob(uint32_t thread_idx);

    inline bool isAllJobsReserved()
    {
        return (m_remaining_unreserved_jobs == 0);
    }

    inline bool isAllJobExecutionFinished()
    {
        return (m_remaining_unfinished_jobs == 0);
    }

    void movePendingJobsIntoExecQueue();

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
            m_debug_counter++;  // @DEBUG: just to see if buffer overflows again (view value in debugger).
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

