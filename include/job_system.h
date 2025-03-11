#pragma once

#include <vector>
#include "job_queue.h"
#include "job_source.h"


class Job_system
{
public:
    Job_system(uint32_t num_threads, std::vector<Job_source*>&& job_sources);

    bool run();

    inline static void stop_all_job_systems()
    {
        s_is_running = false;
    }

private:
    // Set this static flag to false to stop execution of all job systems.
    inline static bool s_is_running{ true };

    static void thread_run_fn(size_t thread_idx,
                              uint16_t num_threads,
                              std::vector<Job_source*>& all_job_sources,
                              std::vector<Job_queue*>& job_queues,
                              std::atomic_uint16_t& busy_job_sources_count);

    uint16_t m_num_threads;
    std::vector<Job_source*> m_job_sources;
    std::atomic_uint16_t m_busy_job_sources_count{ 0 };
};
