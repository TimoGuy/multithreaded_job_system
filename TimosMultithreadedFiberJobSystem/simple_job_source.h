#pragma once

#include "job_source.h"


class Simple_job_source : public Job_source
{
public:
    Simple_job_source();
private:
    std::vector<Job_ifc*> fetch_next_jobs_callback() override;
    std::vector<std::vector<Job_ifc*>> m_all_job_batches;
};

