#pragma once

#include <vector>
#include <atomic>
class Job;


class Job_source
{
public:
    std::vector<Job*> fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak();
    void notify_one_job_complete__thread_safe();

private:
    virtual std::vector<Job*> fetch_next_jobs_callback() = 0;

    std::atomic_uint32_t m_num_jobs_incomplete{ 0 };
};
