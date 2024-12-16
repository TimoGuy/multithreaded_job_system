#pragma once

#include "job_source.h"


class Simple_job_source : public Job_source
{
private:
    std::vector<Job_ifc*> fetch_next_jobs_callback() override;
};

