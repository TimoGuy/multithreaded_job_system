#include "job_system.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include "job_ifc.h"
#include "timing_reporter_public.h"


namespace
{

inline void check_job_sources__thread_safe(uint16_t num_threads,
                                           bool& s_is_running,
                                           uint16_t& round_robin_idx,
                                           std::vector<Job_source*>& all_job_sources,
                                           std::vector<Job_queue*>& job_queues,
                                           std::mutex& feed_job_queues_mutex)
{
    std::unique_lock<std::mutex> feed_job_queues_lock{
        feed_job_queues_mutex, std::try_to_lock };
    if (feed_job_queues_lock.owns_lock())
    {
        // Check all job sources' for new jobs (only one thread at a time).
        // @NOTE: Priority is to get the job sources checked in a timely manner,
        //   so whichever thread is available to check should check.

        // TIMING_REPORT_START(job_source_polling);  @DEBUG.

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
                uint32_t num_buckets{ static_cast<uint32_t>(num_threads) };
                uint32_t bucket_capacity{
                    static_cast<uint32_t>(new_jobs.size() / num_buckets + 1) };

                std::vector<std::vector<Job_ifc*>> bucketed_jobs;
                bucketed_jobs.resize(num_buckets);

                for (auto& bucket : bucketed_jobs)
                    bucket.reserve(bucket_capacity);

                for (size_t i = 0; i < new_jobs.size(); i++)
                {
                    auto jobs{ new_jobs[i] };
                    auto thread_key_idx{ jobs->get_thread_key_idx() };
                    if (thread_key_idx == 0)
                    {
                        // Assign job to any thread; use round robin.
                        bucketed_jobs[round_robin_idx].emplace_back(new_jobs[i]);
                        round_robin_idx = (round_robin_idx + 1) % num_buckets;
                    }
                    else
                    {
                        // Assign job to specific thread.
                        uint32_t actual_thread_idx{ 0 };
                        if (num_buckets > 1)
                        {
                            // Loop assigned thread idx over [1-n].
                            // (@NOTE: Bc thread 0 is already primary job source poller.)
                            actual_thread_idx =
                                (thread_key_idx % (num_buckets - 1)) + 1;
                        }
                        bucketed_jobs[actual_thread_idx].emplace_back(new_jobs[i]);
                    }
                }

                for (size_t i = 0; i < num_buckets; i++)
                    if (!job_queues[i]->append_jobs_back__thread_safe(bucketed_jobs[i]))
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
        // TIMING_REPORT_END_AND_PRINT(job_source_polling, "Job source polling: ");  @DEBUG.
    }
}

}  // namespace


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
                                            std::atomic_uint16_t& busy_job_sources_count,
                                            std::mutex& feed_job_queues_mutex)
{
    assert(num_threads > 0);
    std::atomic<void*>* checking_job_buffer_ptr{ nullptr };
    uint16_t round_robin_idx{ 0 };  // These are thread local only bc atomic ops are slow af.

    while (s_is_running)
    {
        check_job_sources__thread_safe(num_threads,
                                       s_is_running,
                                       round_robin_idx,
                                       all_job_sources,
                                       job_queues,
                                       feed_job_queues_mutex);

        // Reserve job if not reserved yet.
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
        if (thread_idx > 0)  // Wait if not first thread (so that there's still checking on job sources).
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

    std::mutex feed_job_queues_mutex;

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
            std::ref(m_busy_job_sources_count),
            std::ref(feed_job_queues_mutex)
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
