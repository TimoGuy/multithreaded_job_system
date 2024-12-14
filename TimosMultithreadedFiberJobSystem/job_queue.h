#pragma once

#include <atomic>
#include <limits>
#include <vector>
class Job;


class Job_queue
{
public:
    Job* pop_front_job__thread_safe_weak();
    bool append_jobs_back__thread_safe(std::vector<Job*> jobs);

private:
    // @NOTE: to prevent the need for atomic adding w/ modulus, have the
    //        type automatically loop as an increment overflows it.
    using looping_numeric_t = uint16_t;  // I think this will be the largest allowable size for `m_pointer_buffer` ngl.  -Thea 2024/12/14
    std::atomic<void*> m_pointer_buffer[std::numeric_limits<looping_numeric_t>::max()];
    std::atomic<looping_numeric_t> m_front_idx{ 0 };
    std::atomic<looping_numeric_t> m_back_idx{ 0 };  // For indicating that back has been finished being written to.
    std::atomic<looping_numeric_t> m_reservation_back_idx{ 0 };  // For reserving an index to write to the back.
};
