#pragma once

#include "job_ifc.h"


class Simple_job : public Job_ifc
{
public:
    Simple_job(Job_source& source)
        : Job_ifc("Simple Job", source)
    {
    }

    int32_t execute() override;
};
