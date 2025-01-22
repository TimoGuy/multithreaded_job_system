#pragma once

#include <atomic>

#if _WIN64
#include <chrono>
using clock_t = std::chrono::high_resolution_clock;
using time_point_t = std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::duration<double_t>>;
using duration_t = std::chrono::duration<double_t>;
#else
#error "OS not supported."
#endif // _WIN64, etc.


class Job_timekeeper
{
public:
    Job_timekeeper(uint32_t hertz, bool ignore_time_overflow);

    bool check_timeout_and_reset();

private:
    inline time_point_t get_time_now()
    {
        return clock_t::now();
    }

    duration_t m_interval_ms;
    std::atomic<time_point_t> m_interval_timeout;
    bool m_print_error_on_time_overflow;
};
