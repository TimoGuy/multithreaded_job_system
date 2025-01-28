#include "job_queue.h"

#include <cassert>


std::atomic<void*>& Job_queue::reserve_front_buffer_ptr__thread_safe()
{
    // Just immediately get the front buffer position without worrying about
    // size, if the contents are null, etc. Because, this reference will be
    // constantly checked until the contents are not null and result in an
    // execution that will get another buffer position.  -Thea 2024/12/21
    // @NOTE: there is the chance that two threads reserve the same position
    //        with integer wrapping, but using a CAS to check the contents of
    //        the atomic variable, it prevents the job handle from getting
    //        executed multiple times.  -Thea 2024/12/21
    return m_pointer_buffer[m_front_idx++];  // @NOTE: Idk if relaxed fetch-add would be faster or interlocked increment. It probably doesn't matter haha  -Thea 2025/1/2
}

bool Job_queue::append_jobs_back__thread_safe(std::vector<Job_ifc*> jobs)
{
    // Reserve write amount.
    looping_numeric_t reserved_idx_base{
        m_reservation_back_idx.fetch_add(
            static_cast<looping_numeric_t>(jobs.size()),
            std::memory_order_relaxed
        )
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

        // Assert that job to insert is a valid job.
        assert(jobs[i] != nullptr);

        m_pointer_buffer[write_idx].store(
            reinterpret_cast<void*>(jobs[i]),
            std::memory_order_relaxed
        );
        m_pointer_buffer[write_idx].notify_all();
    }

    // @TODO: Add debug level check that the size isn't getting too large to
    //        start overwriting the looping array, so then the return bool provides
    //        some real information.
    //        Could consider copying the pattern of `pop_front...()`
    return true;
}

void Job_queue::flush_for_shutdown__thread_safe()
{
    // Fill whole entire job queue with bogus jobs so that runners
    // waiting for a job to fill in will detect a job incoming.
    // @NOTE: if you could just assign bogus jobs to jobs that are being waited for
    //        that would be a lot more efficient I'm sure. But, this is good for now.  -Thea 2025/01/28
    void* bogus_job{
        reinterpret_cast<void*>(const_cast<uint8_t*>(&k_some_byte_in_static_memory))
    };

    for (size_t i = 0; i < k_pointer_buffer_indices; i++)
    {
        m_pointer_buffer[i].store(bogus_job);
        m_pointer_buffer[i].notify_all();
    }
}


