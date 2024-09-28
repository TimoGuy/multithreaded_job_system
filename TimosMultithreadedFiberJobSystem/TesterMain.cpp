// TesterMain.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <memory>
#include <vector>
#include <thread>

#include "JobManager.h"
#include "SimpleJob.h"
#include "TracyImpl.h"


static bool workerThreadFn(JobManager& job_mgr)
{
    ZoneScoped;

    while (true)
    {
        job_mgr.executeNextJob();
    }
}

static std::vector<std::unique_ptr<Job>> all_jobs;

static void solicitJobsFn(JobManager& job_mgr)
{
    ZoneScoped;

    for (auto& job : all_jobs)
    {
        job_mgr.emplaceJob(job.get());
    }
}

int32_t main()
{
    TracySetProgramName("Timo_Multithreaded_Job_System_x64");
    ZoneScoped;

    // @TODO: name this something along with crank, slop, engine, hog.

    // Create job manager.
    std::unique_ptr<JobManager> job_mgr{
        std::make_unique<JobManager>(std::move(solicitJobsFn))
    };

    // Create a bunch of jobs.
    constexpr size_t k_num_jobs = 100;
    all_jobs.clear();
    all_jobs.reserve(k_num_jobs);
    for (size_t i = 0; i < k_num_jobs; i++)
    {
        all_jobs.emplace_back(std::make_unique<SimpleJob>());
    }

    // Spin up multithreading equal to all cores of CPU.
    const uint32_t num_cores = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    threads.reserve(num_cores);
    for (uint32_t i = 0; i < num_cores; i++)
    {
        threads.emplace_back(workerThreadFn, std::ref(*job_mgr));
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
