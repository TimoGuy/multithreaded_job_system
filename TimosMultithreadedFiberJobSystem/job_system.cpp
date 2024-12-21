#include "job_system.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include "job_ifc.h"
#include "tester_tester_mo_bester.h"


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

/* STATIC */ void Job_system::thread_run_fn(size_t thread_idx, std::vector<Job_source*> job_sources_to_check, Job_queue* job_queue)
{
    JOJODEBUG_REGISTER_THREAD(thread_idx);

    std::atomic<void*>* checking_job_buffer_ptr{ nullptr };

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

        // Check if job buffer position not reserved yet.
        if (checking_job_buffer_ptr == nullptr)
        {
            // Reserve new job buffer position.
            checking_job_buffer_ptr = &job_queue->reserve_front_buffer_ptr__thread_safe();
            assert(checking_job_buffer_ptr != nullptr);
        }

        // Check if job is ready to execute.
        // @NOTE: CAS is required to prevent race conditions causing a job
        //        handle to get executed multiple times (Though this is very
        //        rare and increasing the job queue buffer size would proably
        //        also reduce the chances of this happening. But CAS is there to
        //        keep execution airtight).  -Thea 2024/12/21
        void* job_handle{ *checking_job_buffer_ptr };
        if (job_handle != nullptr &&
            checking_job_buffer_ptr->compare_exchange_weak(job_handle, nullptr))
        {
            // Execute job handle.
            reinterpret_cast<Job_ifc*>(job_handle)->execute_and_record_completion__thread_safe();

            // Clear job buffer position.
            checking_job_buffer_ptr = nullptr;
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
    for (size_t i = 0; i < m_thread_construct_datas.size(); i++)
    {
        auto& tcd{ m_thread_construct_datas[i] };
        threads.emplace_back(
            Job_system::thread_run_fn,
            i,
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
