#pragma once

#include <atomic>
#include <limits>
#include <vector>
class Job_ifc;


class Job_queue
{
public:
    std::atomic<void*>& reserve_front_buffer_ptr__thread_safe();
    bool append_jobs_back__thread_safe(std::vector<Job_ifc*> jobs);
    void flush_for_shutdown__thread_safe();

    inline static bool check_if_job_is_valid__thread_safe(void* job_ptr)
    {
        return (job_ptr != nullptr &&
            job_ptr != &k_some_byte_in_static_memory);
    }

private:
    // @NOTE: To prevent the need for atomic adding w/ modulus, have the
    //        type automatically loop as an increment overflows it.
    // I think `uint16_t` will be the largest allowable size for a looping type
    // ngl.  -Thea 2024/12/14
    using looping_numeric_t = uint16_t;
    inline static const size_t k_pointer_buffer_indices{ std::numeric_limits<looping_numeric_t>::max() + 1 };
    std::atomic<void*> m_pointer_buffer[k_pointer_buffer_indices];
    std::atomic<looping_numeric_t> m_front_idx{ 0 };  // For reserving front buffer ptr.
    std::atomic<looping_numeric_t> m_reservation_back_idx{ 0 };  // For reserving an index to write to the back.

    // For holding some location in memory for a bogus job.
    inline static const volatile uint8_t k_some_byte_in_static_memory{ 69 };
};
