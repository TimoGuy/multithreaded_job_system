#include "job_queue.h"

#include <cassert>
#include <iostream>
#include "tester_tester_mo_bester.h"
#include "tracy_impl.h"


Job_ifc* Job_queue::pop_front_job__thread_safe_weak()
{
    ZoneScoped;

    void* ptr;
    size_t queue_size{ m_queue_size };

    // Check if buffer is empty.
    if (queue_size == 0)
    {
        // Buffer is empty.
        ptr = nullptr;
    }
    else
    {
        // Attempt to decrement queue size.
        if (m_queue_size.compare_exchange_weak(queue_size, queue_size - 1))
        {
            // Successful queue size decrement. Take `m_front_idx`.
            looping_numeric_t front_idx_orig{ m_front_idx++ };
            ptr = m_pointer_buffer[front_idx_orig];
#if _DEBUG
            // @DEBUG: I think that somewhere along the lines it's getting all
            //         messed up and I need to go in and move all this back to
            //         nullptr to test it out!
            // @NOTE: I realized that the reason why there were extra decrements
            //        was because a valid job was getting popped off the front and
            //        it was actually garbage. Since it had info tho bc it's a pointer
            //        to static memory, it was still able to access the job source
            //        to decrement the remaining jobs count!  -Thea 2024/12/19
            // @TODO: hopefully in the future this feels unnecessary and two atomic
            //        operations don't have to be done on the same index and garbage
            //        can just be left in.
            m_pointer_buffer[front_idx_orig] = nullptr;
#endif
            char jojo = JOJODEBUG_JJJJJJ_("123");  // Just to see what the issue is lol.
            JOJODEBUG_LOG_ACTION("asd");
            assert(ptr != nullptr, "Ptr from successful pop should not be null.");
        }
        else
        {
            // `queue_size` changed from load. CAS failed, so return null.
            ptr = nullptr;
        }
    }

    return reinterpret_cast<Job_ifc*>(ptr);
}

bool Job_queue::append_jobs_back__thread_safe(std::vector<Job_ifc*> jobs)
{
    ZoneScoped;

    // Reserve write amount.
    looping_numeric_t next_back_idx{
        static_cast<looping_numeric_t>(m_reservation_back_idx += jobs.size())
    };
    looping_numeric_t reserved_idx_base{
        static_cast<looping_numeric_t>(next_back_idx - jobs.size())
    };

    // Write.
    for (size_t i = 0; i < jobs.size(); i++)
    {
        size_t write_idx{
            (reserved_idx_base + i) % k_pointer_buffer_indices
        };
        m_pointer_buffer[write_idx] = reinterpret_cast<void*>(jobs[i]);
        JOJODEBUG_LOG_ACTION("123");
    }

    // Update queue size (in order) once write has finished.
    constexpr size_t k_weak_check_loops{ 100 };
    looping_numeric_t reserved_idx_copy;

    for (size_t i = 0; i < k_weak_check_loops; i++)
    {
        // @NOTE: Due to the possibility that multiple append writing jobs could
        //        be happening at the same time, the buffer must not expand the
        //        `m_queue_size` into a group of half-written pointers, given the
        //        hypothetical situation where two append func calls are racing
        //        and the later-reserving one finishes and writes to `m_queue_size`
        //        first.  -Thea 2024/12/14 (2024/12/20)
        if (m_writing_back_idx.compare_exchange_weak(reserved_idx_copy, next_back_idx))
        {
            // Successfully able to update queue size now.
            m_queue_size += jobs.size();
            JOJODEBUG_LOG_ACTION("qwe");
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














                            // @TODO: Try creating a simple char array with a bunch of characters and then an atomic counter that goes thru and starts listing events that happened up until the break. Then search the string for irregularities.