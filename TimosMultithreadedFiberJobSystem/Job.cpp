#include "Job.h"

#include <sstream>
#include "TracyImpl.h"
#include "JobStatistics.h"


Job::Job(std::string&& name, JobGroup_e group, order_t order)
    : m_job_id(s_job_id_counter++)
    , m_name(name)
    , m_group(group)
    , m_order(order)
{
    JOBSTATS_REGISTER_JOB_NAME(m_name);
}

std::string Job::toString() const
{
    ZoneScoped;
    std::stringstream sstr;
    sstr << "Job " << m_job_id << ":\tname=\"" << m_name
        << "\"\tgroup=" << job_group_to_str[m_group] << "\torder=" << m_order;
    return sstr.str();
}
