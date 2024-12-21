#include "job_queue.h"

#include <cassert>
#include <iostream>
#include "tester_tester_mo_bester.h"
#include "tracy_impl.h"


std::atomic<void*>& Job_queue::pop_front_buffer_ptr__thread_safe()
{
    ZoneScoped;

    // Just immediately get the front buffer position without worrying about
    // size, if the contents are null, etc. Because, this reference will be
    // constantly checked until the contents are not null and result in an
    // execution that will get another buffer position.  -Thea 2024/12/21
    return m_pointer_buffer[m_front_idx++];
////////
////////    void* ptr;
////////    size_t queue_size{ m_queue_size };
////////
////////    // Check if buffer is empty.
////////    if (queue_size == 0)
////////    {
////////        // Buffer is empty.
////////        ptr = nullptr;
////////    }
////////    else
////////    {
////////        // Attempt to decrement queue size.
////////        if (m_queue_size.compare_exchange_weak(queue_size, queue_size - 1))
////////        {
////////            // Successful queue size decrement. Take `m_front_idx`.
////////            looping_numeric_t front_idx_orig{ m_front_idx++ };
////////            ptr = m_pointer_buffer[front_idx_orig];
////////#if _DEBUG
////////            // @DEBUG: I think that somewhere along the lines it's getting all
////////            //         messed up and I need to go in and move all this back to
////////            //         nullptr to test it out!
////////            // @NOTE: I realized that the reason why there were extra decrements
////////            //        was because a valid job was getting popped off the front and
////////            //        it was actually garbage. Since it had info tho bc it's a pointer
////////            //        to static memory, it was still able to access the job source
////////            //        to decrement the remaining jobs count!  -Thea 2024/12/19
////////            // @TODO: hopefully in the future this feels unnecessary and two atomic
////////            //        operations don't have to be done on the same index and garbage
////////            //        can just be left in.
////////            m_pointer_buffer[front_idx_orig] = nullptr;
////////#endif
////////            char jojo = JOJODEBUG_JJJJJJ_("123");  // Just to see what the issue is lol.
////////            JOJODEBUG_LOG_ACTION("asd");
////////            assert(ptr != nullptr, "Ptr from successful pop should not be null.");
////////        }
////////        else
////////        {
////////            // `queue_size` changed from load. CAS failed, so return null.
////////            ptr = nullptr;
////////        }
////////    }
////////
////////    return reinterpret_cast<Job_ifc*>(ptr);
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














                            // @TODO: Try creating a simple char array with a bunch of characters and then an atomic counter that goes thru and starts listing events that happened up until the break. Then search the string for irregularities.