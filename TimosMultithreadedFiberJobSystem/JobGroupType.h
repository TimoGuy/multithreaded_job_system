#pragma once

#include <cinttypes>
#include <string>


using order_t = uint32_t;

enum JobGroup_e : uint8_t
{
    JOB_GROUP_DEFAULT = 0,
    JOB_GROUP_RENDERING,
    NUM_JOB_GROUPS
};

static std::string job_group_to_str[] = {
    "DEFAULT",
    "RENDERING",
};
