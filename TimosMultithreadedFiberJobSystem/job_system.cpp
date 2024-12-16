#include "job_system.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include "job_ifc.h"


Job_system::Job_system(uint32_t num_threads, std::vector<Job_source*>&& job_sources)
    : m_job_sources(std::move(job_sources))
{
    assert(num_threads > 0);
    m_thread_construct_datas.resize(num_threads, {});
    for (size_t i = 0; i < m_job_sources.size(); i++)
    {
        auto job_source{ m_job_sources[i] };
        auto& tcd{
            m_thread_construct_datas[i % m_thread_construct_datas.size()]
        };
        tcd.responsible_job_sources.emplace_back(job_source);
    }
}

/* STATIC */ void Job_system::thread_run_fn(std::vector<Job_source*> job_sources_to_check, Job_queue* job_queue)
{
    while (s_is_running)
    {
        // Check job source(s) heartbeat.
        for (auto job_src_ptr : job_sources_to_check)
        {
            auto new_jobs{
                job_src_ptr->fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()
            };
            if (!new_jobs.empty())
                if (!job_queue->append_jobs_back__thread_safe(new_jobs))
                {
                    std::cerr << "ERROR: appending jobs to job queue failed" << std::endl;
                    assert(false);
                }
        }

        // Fetch and execute one job.
        if (auto job{ job_queue->pop_front_job__thread_safe_weak() })
        {
            job->execute_and_record_completion__thread_safe();
        }
    }
}

bool Job_system::run()
{
    std::unique_ptr<Job_queue> job_queue{
        std::make_unique<Job_queue>()
    };

    // Spin up multithreading equal to all threads requested.
    std::vector<std::thread> threads;
    threads.reserve(m_thread_construct_datas.size());
    for (auto& tcd : m_thread_construct_datas)
    {
        threads.emplace_back(
            Job_system::thread_run_fn,
            tcd.responsible_job_sources,
            job_queue.get()
        );
    }

    // Wait until all threads are complete (they exit from `s_is_running` flag).
    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    // Job system running complete.
    return true;
}
