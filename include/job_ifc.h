#pragma once

#include <atomic>
#include <string>
class Job_source;


// `Job_ifc` is the abstract form.
class Job_ifc
{
public:
    Job_ifc(std::string&& name, Job_source& source, uint32_t thread_key = 0);

    uint32_t get_thread_key_idx();
    int32_t execute_and_record_completion__thread_safe();
    std::string to_string() const;

protected:
    virtual int32_t execute() = 0;

private:
    inline static std::atomic_uint64_t s_job_id_counter{ 0 };

    uint64_t m_job_id;
    std::string m_name;
    Job_source& m_source;
    uint32_t m_thread_key_idx;
};
