#include "job_ifc.h"

#include <sstream>
#include "job_source.h"
#include "tracy_impl.h"


Job_ifc::Job_ifc(std::string&& name, Job_source& source)
    : m_job_id(s_job_id_counter++)
    , m_name(name)
    , m_source(source)
{
}

int32_t Job_ifc::execute_and_record_completion__thread_safe()
{
    int32_t status{ execute() };
    m_source.notify_one_job_complete__thread_safe();
    return status;
}

std::string Job_ifc::toString() const
{
    ZoneScoped;
    std::stringstream sstr;
    sstr << "Job #" << m_job_id << ":\tname=\"" << m_name << "\"";
    return sstr.str();
}
