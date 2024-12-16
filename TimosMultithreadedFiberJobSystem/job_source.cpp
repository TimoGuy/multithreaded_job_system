#include "job_source.h"

#include <cassert>


std::vector<Job_ifc*> Job_source::fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()
{
    std::vector<Job_ifc*> jobs;

    // Check if no more jobs.
    uint32_t zero{ 0 };
    if (m_num_jobs_incomplete.compare_exchange_weak(zero, (uint32_t)-1))
    {
        // Go fetch jobs.
        jobs = fetch_next_jobs_callback();

        // Mark number incomplete jobs (allows another thread to enter this block again).
        m_num_jobs_incomplete = static_cast<uint32_t>(jobs.size());  // @TODO: @THEA: I guess this value isn't getting written to fast enough??? Figure out why `notify_one_job_complete__thread_safe()` keeps getting run.
    }

    return jobs;
}


void Job_source::notify_one_job_complete__thread_safe()
{
#if _DEBUG
    uint32_t before_decrement_val =
#endif
        m_num_jobs_incomplete--;
#if _DEBUG
    assert(before_decrement_val != 0);
#endif
}
