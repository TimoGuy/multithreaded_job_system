#pragma once

#include <vector>
#include "job_queue.h"
#include "job_source.h"


class Job_system
{
public:
    Job_system(uint32_t num_threads, std::vector<Job_source>&& job_sources);

    bool run();

    inline static void stop_all_job_systems()
    {
        s_is_running = false;
    }

private:
    // Set this static flag to false to stop execution of all job systems.
    inline static bool s_is_running{ true };

    static void thread_run_fn(std::vector<Job_source*> job_sources_to_check, Job_queue* job_queue);

    struct Thread_construction_data
    {
        std::vector<Job_source*> responsible_job_sources;
    };
    std::vector<Thread_construction_data> m_thread_construct_datas;
    std::vector<Job_source> m_job_sources;
};
