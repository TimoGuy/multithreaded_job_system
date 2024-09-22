#pragma once

#include "Job.h"


class SimpleJob : public Job
{
public:
    SimpleJob()
        : Job("SimpleJob", JobGroup_e::JOB_GROUP_DEFAULT, 0)
    {
    }

    int32_t execute() override;
};

