#include "job_timekeeper.h"

#include <iostream>


Job_timekeeper::Job_timekeeper(uint32_t hertz, bool print_error_on_time_overflow)
    : m_interval_ms(1.0 / static_cast<double_t>(hertz))  // @CHECK: I THINK THAT THIS IS CALC'd correctly, but check it.
    , m_interval_timeout(time_point_t::min())
    , m_print_error_on_time_overflow(print_error_on_time_overflow)
{

}

bool Job_timekeeper::check_timeout_and_reset()
{
    const time_point_t now{ get_time_now() };
    time_point_t timeout{ m_interval_timeout.load() };

    // Check if timer is not unset nor expired.
    if (timeout != time_point_t::min() &&
        now < timeout)
        return false;

    // Set new timeout.
    time_point_t new_timeout{
        (timeout == time_point_t::min() ? now : timeout) + m_interval_ms
    };

    if (m_interval_timeout.compare_exchange_weak(timeout, new_timeout))
    {
        if (m_print_error_on_time_overflow &&
            now >= new_timeout)
        {
            // Time overflow detected.
            std::cerr << "ERROR: Time overflow detected." << std::endl;
        }

        // Successfully set new timeout.
        return true;
    }

    // Set new timeout failed due to another thread race condition.
    return false;
}
