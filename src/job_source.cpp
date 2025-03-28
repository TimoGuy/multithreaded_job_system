#include "job_source.h"

#include <cassert>


std::vector<Job_ifc*> Job_source::fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()
{
    Job_next_jobs_return_data return_data;

    // Check if no more jobs.
    uint32_t zero{ 0 };
    if (m_num_jobs_incomplete.compare_exchange_weak(zero, (uint32_t)-1))
    {
        // Go fetch jobs.
        return_data = fetch_next_jobs_callback();

        if (return_data.signal_end_of_life)
        {
            // @NOTE: Do not add jobs when signaling end of life.
            // Granted, these jobs won't get actually added, but it would definitely
            // cause undefined behavior.
            assert(return_data.jobs.empty());

            // Notify end of life.
            signal_end_of_life();
        }
        else
        {
#if _DEBUG
            for (auto job : return_data.jobs)
            {
                assert(job != nullptr);
            }
#endif  // _DEBUG

            // Mark number incomplete jobs (allows another thread to enter this block again).
            m_num_jobs_incomplete = static_cast<uint32_t>(return_data.jobs.size());
        }
    }

    return return_data.jobs;
}

void Job_source::notify_one_job_complete__thread_safe()
{
#if _DEBUG
    uint32_t before_decrement_val =
#endif
        m_num_jobs_incomplete--;
#if _DEBUG
    assert(before_decrement_val != 0);  // This assert is MVP assert lol.
#endif
}

void Job_source::signal_end_of_life()
{
    // Set flag for job system to stop querying job source for jobs.
    // @NOTE: Once all job sources are not running, job system shuts down.
    m_is_job_source_running = false;
}
