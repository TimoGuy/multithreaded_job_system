#pragma once

#include <string>
#include <atomic>

#include "JobGroupType.h"


// The Job is the abstract form.
class Job
{
public:
    Job(std::string&& name, JobGroup_e group, order_t order);

    virtual int32_t execute() = 0;
    std::string&& toString() const;

protected:
    uint64_t m_job_id;
    std::string m_name;
    JobGroup_e m_group;
    order_t m_order;

private:
    inline static std::atomic_uint64_t s_job_id_counter = 0;

    friend class JobManager;
};

