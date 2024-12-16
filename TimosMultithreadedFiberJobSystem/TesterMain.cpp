// TesterMain.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <memory>
#include <vector>
#include <thread>

#include "JobManager.h"
#include "simple_job.h"
#include "TracyImpl.h"


static bool workerThreadFn(JobManager& job_mgr, uint32_t thread_idx)
{
    ZoneScoped;

    while (true)
    {
        job_mgr.executeNextJob(thread_idx);
    }
}

static std::vector<std::unique_ptr<Job_ifc>> all_jobs;

static job_manager_callback_fn_t solicit_jobs_fn =
    [&]()
    {
        std::vector<Job_ifc*> all_jobs_non_owning;
        all_jobs_non_owning.reserve(all_jobs.size());

        for (auto& job : all_jobs)
        {
            all_jobs_non_owning.emplace_back(job.get());
        }

        return all_jobs_non_owning;
    };

int32_t main()
{
    TracySetProgramName("Timo_Multithreaded_Job_System_x64");
    ZoneScoped;

    // @TODO: name this something along with crank, slop, engine, hog.
    const uint32_t num_cores = std::thread::hardware_concurrency();

    // Create job manager.
    std::unique_ptr<JobManager> job_mgr{
        std::make_unique<JobManager>(solicit_jobs_fn, num_cores)
    };

    // Create a bunch of jobs.
    constexpr size_t k_num_jobs = 100;
    all_jobs.clear();
    all_jobs.reserve(k_num_jobs);
    for (size_t i = 0; i < k_num_jobs; i++)
    {
        all_jobs.emplace_back(std::make_unique<Simple_job>());
    }

    // Spin up multithreading equal to all cores of CPU.
    std::vector<std::thread> threads;
    threads.reserve(num_cores);
    for (uint32_t i = 0; i < num_cores; i++)
    {
        threads.emplace_back(workerThreadFn, std::ref(*job_mgr), i);
    }

    // Wait until all threads are complete before closing program.
    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    // Program complete.
    return 0;
}
