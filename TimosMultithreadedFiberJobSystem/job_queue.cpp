#include "job_queue.h"

#include <cassert>
#include <iostream>
#include "tester_tester_mo_bester.h"
#include "tracy_impl.h"


std::atomic<void*>& Job_queue::reserve_front_buffer_ptr__thread_safe()
{
    ZoneScoped;

    // Just immediately get the front buffer position without worrying about
    // size, if the contents are null, etc. Because, this reference will be
    // constantly checked until the contents are not null and result in an
    // execution that will get another buffer position.  -Thea 2024/12/21
    // @NOTE: there is the chance that two threads reserve the same position
    //        with integer wrapping, but using a CAS to check the contents of
    //        the atomic variable, it prevents the job handle from getting
    //        executed multiple times.  -Thea 2024/12/21
    return m_pointer_buffer[m_front_idx++];
}

bool Job_queue::append_jobs_back__thread_safe(std::vector<Job_ifc*> jobs)
{
    ZoneScoped;

    // Reserve write amount.
    looping_numeric_t reserved_idx_base{
        static_cast<looping_numeric_t>((m_reservation_back_idx += jobs.size()) - jobs.size())
    };

    // Write.
    // @NOTE: The moment the buffer is written to it will get picked up by the
    //        thread that has this position reserved and is checking for a job
    //        to show up.
    for (size_t i = 0; i < jobs.size(); i++)
    {
        size_t write_idx{
            (reserved_idx_base + i) % k_pointer_buffer_indices
        };
        m_pointer_buffer[write_idx] = reinterpret_cast<void*>(jobs[i]);
        JOJODEBUG_LOG_ACTION("123");
    }

    // @TODO: Add debug level check that the size isn't getting too large to
    //        start overwriting the looping array, so then the return bool provides
    //        some real information.
    //        Could consider copying the pattern of `pop_front...()`
    return true;
}
