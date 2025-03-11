#include "job_ifc.h"

#if _DEBUG
#include <atomic>
#endif  // _DEBUG
#include <cassert>
#include <mutex>
#include <vector>
#include <sstream>
#include "job_source.h"


namespace Job_ifc_data
{

#if _DEBUG
static std::atomic_bool s_allow_new_thread_keys{ true };
#endif  // _DEBUG
// @TODO: change these to atomic bools. vv
static std::vector<std::mutex*> s_thread_mutexes;  // @NOTE: These are created on heap... and they're just lost in memory unless you feel like releasing them at program shutdown.  -Thea 2025/03/07

}  // namespace Job_ifc_data


Job_ifc::Job_ifc(std::string&& name, Job_source& source, uint32_t thread_key /*= 0*/)
    : m_job_id(s_job_id_counter++)
    , m_name(name)
    , m_source(source)
{
    // Take thread key and assign thread index.
    static std::mutex s_key_registration_mutex;
    static std::vector<uint32_t> s_key_registration{ 0 };  // @NOTE: Default key of 0 as first thing.

    std::lock_guard<std::mutex> lock{ s_key_registration_mutex };
    assert(!s_key_registration.empty());
    assert(s_key_registration.front() == 0);

    uint32_t found_idx{ (uint32_t)-1 };
    for (size_t i = 0; i < s_key_registration.size(); i++)
    {
        if (s_key_registration[i] == thread_key)
        {
            assert(static_cast<uint32_t>(i) != (uint32_t)-1);
            found_idx = static_cast<uint32_t>(i);
            break;
        }
    }
    if (found_idx == (uint32_t)-1)
    {
        // Assert that job execution has not started when the thread-key-based mutexes are starting to be used.
        assert(Job_ifc_data::s_allow_new_thread_keys);

        // Create new key and thread mutex.
        found_idx = static_cast<uint32_t>(s_key_registration.size());
        s_key_registration.emplace_back(thread_key);
        Job_ifc_data::s_thread_mutexes.emplace_back(new std::mutex);
        assert(s_key_registration.size() - 1 ==
            Job_ifc_data::s_thread_mutexes.size());
    }
    m_thread_idx = found_idx;
}

uint32_t Job_ifc::get_assigned_thread_idx()
{
    return m_thread_idx;
}

int32_t Job_ifc::execute_and_record_completion__thread_safe()
{
    // Now that job execution has started, thread keys are not allowed anymore.
    Job_ifc_data::s_allow_new_thread_keys = false;

    // Execute job, locking the thread if desired.
    auto mutex_ptr{
        (m_thread_idx > 0 ?
            Job_ifc_data::s_thread_mutexes[m_thread_idx - 1] :
            nullptr) };

    if (mutex_ptr) mutex_ptr->lock();
    int32_t status{ execute() };
    if (mutex_ptr) mutex_ptr->unlock();

    // Finish and notify.
    m_source.notify_one_job_complete__thread_safe();
    return status;
}

std::string Job_ifc::to_string() const
{
    std::stringstream sstr;
    sstr << "Job #" << m_job_id << ":\tname=\"" << m_name << "\"";
    return sstr.str();
}
