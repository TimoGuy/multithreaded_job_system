#include <thread>
#include "job_source.h"
#include "job_system.h"
#include "simple_job_source.h"


int32_t main()
{
    uint32_t num_threads{
        std::thread::hardware_concurrency()
    };

    Simple_job_source simple_js;

    std::vector<Job_source*> job_sources{
        &simple_js,
    };

    Job_system job_system{
        num_threads,
        std::move(job_sources)
    };

    if (job_system.run())
    {
        return 0;
    }
    return 1;
}
