#pragma once

#include <vector>
#include <atomic>
class Job_ifc;


class Job_source
{
public:
    std::vector<Job_ifc*> fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak();
    void notify_one_job_complete__thread_safe();
    inline bool is_running() { return m_is_job_source_running; }

    struct Job_next_jobs_return_data
    {
        std::vector<Job_ifc*> jobs;
        bool signal_end_of_life{ false };
    };

private:
    void signal_end_of_life();
    virtual Job_next_jobs_return_data fetch_next_jobs_callback() = 0;

    std::atomic_uint32_t m_num_jobs_incomplete{ 0 };
    std::atomic_bool m_is_job_source_running{ true };
};
