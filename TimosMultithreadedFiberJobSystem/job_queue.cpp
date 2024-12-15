#include "job_queue.h"

#include <cassert>
#include <iostream>


Job* Job_queue::pop_front_job__thread_safe_weak()
{
    void* ptr;

    looping_numeric_t front_idx{ m_front_idx };
    looping_numeric_t back_idx{ m_back_idx };

    // Check if buffer is empty.
    if (front_idx == back_idx)
    {
        // Size is zero (Assume size of zero, however, size could be full, which,
        // in that case, should definitely not return null, but I'm just eating
        // this edge case to reduce the number of atomic operations).
        // @NOTE: Unfortunately, this janky CAS size check causes more atomic
        //        operations (we could've done a pop in 1 atomic op instead of
        //        the 4 you see in this function, on top of CAS weak causing an
        //        intermittent failure), but I think this is the best we'll get
        //        bc I don't wanna use locks, and idk if fences can accomplish
        //        a better memory contiguity.  -Thea 2024/12/14
        ptr = nullptr;
    }
    else
    {
        // Increment front index atomically.
        looping_numeric_t front_idx_orig{ front_idx };  // Make a copy since it gets mutated on the CAS.
        if (m_front_idx.compare_exchange_weak(front_idx, front_idx + 1))
        {
            // `m_front_idx` successfully moved. 
            ptr = m_pointer_buffer[front_idx_orig];
            assert(ptr != nullptr, "Ptr from successful pop should not be null.");
        }
        else
        {
            // `front_idx` changed from load. CAS failed, so return null.
            ptr = nullptr;
        }
    }

    return reinterpret_cast<Job*>(ptr);
}

bool Job_queue::append_jobs_back__thread_safe(std::vector<Job*> jobs)
{
    // Reserve write amount.
    looping_numeric_t reserved_idx_base{
        static_cast<looping_numeric_t>((m_reservation_back_idx += jobs.size()) - jobs.size())
    };

    // Write.
    for (size_t i = 0; i < jobs.size(); i++)
    {
        m_pointer_buffer[reserved_idx_base + i] = reinterpret_cast<void*>(jobs[i]);
    }

    // Update back idx once write has finished.
    looping_numeric_t reserved_idx_base_copy;
    looping_numeric_t desired_back_idx{ reserved_idx_base + jobs.size()};
    constexpr size_t k_weak_check_loops{ 100 };

    for (size_t i = 0; i < k_weak_check_loops; i++)
    {
        // @NOTE: Due to the possibility that multiple append writing jobs could
        //        be happening at the same time, the buffer must not expand the
        //        `m_back_idx` into a group of half-written pointers, given the
        //        hypothetical situation where two append func calls are racing
        //        and the later-reserving one finishes and writes to `m_back_idx`
        //        first.  -Thea 2024/12/14
        reserved_idx_base_copy = reserved_idx_base;
        if (m_back_idx.compare_exchange_weak(reserved_idx_base_copy, desired_back_idx))
        {
            // Successfully able to update back idx.
            break;
        }
        assert(i != k_weak_check_loops - 1);  // @TODO: this might be good to be an error message in release.
    }

    // @TODO: Add debug level check that the size isn't getting too large to
    //        start overwriting the looping array, so then the return bool provides
    //        some real information.
    //        Could consider copying the pattern of `pop_front...()`
    return true;
}
