#include "job_system.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include "job_ifc.h"
#include "timing_reporter_public.h"


Job_system::Job_system(uint32_t num_threads, std::vector<Job_source*>&& job_sources)
    : m_num_threads(num_threads)
    , m_job_sources(std::move(job_sources))
{
    assert(num_threads > 0);
    assert(m_job_sources.size() > 0);
}

/* STATIC */ void Job_system::thread_run_fn(size_t thread_idx,
                                            uint16_t num_threads,
                                            std::vector<Job_source*>& all_job_sources,
                                            std::vector<Job_queue*>& job_queues,
                                            std::atomic_uint16_t& busy_job_sources_count)
{
    assert(num_threads > 1);  // @NOTE: for now, just require at least 2 threads.... idk how to re-arch this to allow for single threaded job systems.
    std::atomic<void*>* checking_job_buffer_ptr{ nullptr };
    uint16_t round_robin_idx{ 0 };

    while (s_is_running)
    {
        bool reserve_and_execute_job{ true };

        // Check all job sources' for new jobs.
        if (thread_idx == 0)  // Only if first thread.
        {
            TIMING_REPORT_START(thread0_stuff);
            // @NOTE: Priority is to get the job sources checked in a timely manner.
            reserve_and_execute_job = false;

            bool any_job_sources_running{ false };
            for (auto job_src_ptr : all_job_sources)
            {
                if (job_src_ptr->is_running())
                {
                    // Mark that >0 job sources are running.
                    any_job_sources_running = true;
                }
                else
                {
                    // Skip job source since it's not running.
                    continue;
                }

                std::vector<Job_ifc*> new_jobs{
                    job_src_ptr->fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()
                };

                if (!new_jobs.empty())
                {
                    // @TODO: @HACKY: @INCOMPLETE: The below vv is very very hacky. I wanna be able to have all jobs run on all cores.
                    uint32_t num_buckets{ static_cast<uint32_t>(num_threads - 1) };
                    uint32_t bucket_capacity{
                        static_cast<uint32_t>(new_jobs.size() / num_buckets + 1) };

                    std::vector<std::vector<Job_ifc*>> bucketed_jobs;
                    bucketed_jobs.resize(num_buckets);

                    for (auto& bucket : bucketed_jobs)
                        bucket.reserve(bucket_capacity);

                    for (size_t i = 0; i < new_jobs.size(); i++)
                    {
                        auto jobs{ new_jobs[i] };
                        auto assigned_thread_idx{ jobs->get_assigned_thread_idx() };
                        if (assigned_thread_idx == 0)
                        {
                            // Assign job to any thread; use round robin.
                            // @NOTE: Avoid 0th thread.
                            bucketed_jobs[round_robin_idx].emplace_back(new_jobs[i]);
                            round_robin_idx = (round_robin_idx + 1) % num_buckets;
                        }
                        else
                        {
                            // Assign job to specific thread.
                            bucketed_jobs[assigned_thread_idx].emplace_back(new_jobs[i]);
                        }
                    }

                    for (size_t i = 0; i < num_buckets; i++)
                        if (!job_queues[i + 1]->append_jobs_back__thread_safe(bucketed_jobs[i]))
                        {
                            std::cerr << "ERROR: appending jobs to job queue failed" << std::endl;
                            assert(false);
                        }
                }
            }

            if (!any_job_sources_running)
            {
                // Set job system as not running if no sources are running.
                s_is_running = false;

                // Force job queue to awaken all job runners for shutting down.
                for (auto job_queue : job_queues)
                    job_queue->flush_for_shutdown__thread_safe();
            }
            else
            {
                ///////////// @NOTE: if you can figure out how to re-arch the system to allow for single system cpus or with this kind of thing.,,,, then go ahead and do it.
                ///////////// // Only execute jobs if all other job runners are busy.
                ///////////// @TODO: @THEA: GET THE FIRST THREAD ALSO DOING STUFF SO THAT THIS CAN BE RUN ON SINGLE THREADSSSSSS~!!!!!!! AND SO THAT THE JOB SOURCE CHECKING THREAD CAN JOIN IN ON THE WORK OH WORKIE WORK.
                ///////////if (busy_job_sources_count == (num_threads - 1))
                ///////////    // reserve_and_execute_job = true;
                ///////////    std::cout << "SEND OUT THE THEA" << std::endl;
            }
            TIMING_REPORT_END_AND_PRINT(thread0_stuff, "Thread 0 job allocation: ");
        }

        // Reserve and execute a job.
        if (reserve_and_execute_job)
        {
            // Check if job buffer position not reserved yet.
            if (checking_job_buffer_ptr == nullptr)
            {
                // Reserve new job buffer position.
                checking_job_buffer_ptr =
                    &job_queues[thread_idx]->reserve_front_buffer_ptr__thread_safe();
                assert(checking_job_buffer_ptr != nullptr);
            }

            // Check if job is ready to execute.
            // @NOTE: CAS is required to prevent race conditions causing a job
            //        handle to get executed multiple times (Though this is very
            //        rare and increasing the job queue buffer size would proably
            //        also reduce the chances of this happening. But CAS is there to
            //        keep execution airtight).  -Thea 2024/12/21
            checking_job_buffer_ptr->wait(nullptr, std::memory_order_seq_cst);
            void* job_handle{ checking_job_buffer_ptr->load(std::memory_order_seq_cst) };
            if (Job_queue::check_if_job_is_valid__thread_safe(job_handle))
            {
                // Only execute job once, but guarantee job has been executed.
                // (in case if 2 threads have the same position reserved and contend)
                if (checking_job_buffer_ptr->compare_exchange_strong(job_handle, nullptr, std::memory_order_relaxed))
                {
                    // Execute job handle.
                    busy_job_sources_count++;
                    reinterpret_cast<Job_ifc*>(job_handle)->execute_and_record_completion__thread_safe();
                    busy_job_sources_count--;
                }

                // Clear job buffer reserved position.
                // (in case if this is the losing contending thread, it won't keep
                //  trying to look for a job to appear)
                checking_job_buffer_ptr = nullptr;
            }
        }
    }
}

bool Job_system::run()
{
    // Create queues.
    std::vector<std::unique_ptr<Job_queue>> job_queues;
    job_queues.reserve(m_num_threads);
    std::vector<Job_queue*> job_queue_ptrs;
    job_queue_ptrs.reserve(m_num_threads);

    for (size_t i = 0; i < m_num_threads; i++)
    {
        job_queues.emplace_back(std::make_unique<Job_queue>());
        job_queue_ptrs.emplace_back(job_queues[i].get());
    }

    // Spin up multithreading equal to all threads requested.
    std::vector<std::thread> threads;
    threads.reserve(m_num_threads);
    for (size_t i = 0; i < m_num_threads; i++)
    {
        threads.emplace_back(
            Job_system::thread_run_fn,
            i,
            m_num_threads,
            std::ref(m_job_sources),
            std::ref(job_queue_ptrs),
            std::ref(m_busy_job_sources_count)
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
