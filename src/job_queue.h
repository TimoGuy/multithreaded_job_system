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

private:
    // @NOTE: To prevent the need for atomic adding w/ modulus, have the
    //        type automatically loop as an increment overflows it.
    // I think `uint16_t` will be the largest allowable size for a looping type
    // ngl.  -Thea 2024/12/14
    using looping_numeric_t = uint8_t;
    inline static const size_t k_pointer_buffer_indices{ std::numeric_limits<looping_numeric_t>::max() + 1 };
    std::atomic<void*> m_pointer_buffer[k_pointer_buffer_indices];
    std::atomic<looping_numeric_t> m_front_idx{ 0 };  // For reserving front buffer ptr.
    std::atomic<looping_numeric_t> m_reservation_back_idx{ 0 };  // For reserving an index to write to the back.
};
