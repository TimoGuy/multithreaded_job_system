#include "job_source.h"

#include <cassert>
#include "tester_tester_mo_bester.h"
#include "tracy_impl.h"

// @DEBUG: DELETE ME
//#include <iostream>


std::vector<Job_ifc*> Job_source::fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()
{
    ZoneScoped;
    std::vector<Job_ifc*> jobs;

    // Check if no more jobs.
    uint32_t zero{ 0 };
    if (m_num_jobs_incomplete.compare_exchange_weak(zero, (uint32_t)-1))
    {
        // Go fetch jobs.
        jobs = fetch_next_jobs_callback();
        JOJODEBUG_LOG_ACTION("!@#");

        // @DEBUG
        /*static std::atomic_size_t debug_thingothingo{ 0 };
        size_t debug_tt_copy{ debug_thingothingo++ };
        if (debug_tt_copy % 100000 == 0)
            std::cout << "Added jobs: " << debug_tt_copy << std::endl;*/

        // Mark number incomplete jobs (allows another thread to enter this block again).
        m_num_jobs_incomplete = static_cast<uint32_t>(jobs.size());
    }

    return jobs;
}


void Job_source::notify_one_job_complete__thread_safe()
{
    ZoneScoped;
#if _DEBUG
    uint32_t before_decrement_val =
#endif
        m_num_jobs_incomplete--;
    JOJODEBUG_LOG_ACTION("zxc");
#if _DEBUG
    assert(before_decrement_val != 0);  // This assert is MVP assert lol.
#endif
}
