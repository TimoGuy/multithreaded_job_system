#include <thread>
#include "job_source.h"
#include "job_system.h"
#include "simple_job_source.h"
#include "tracy_impl.h"


int32_t main()
{
    TracySetProgramName("Thea_Multithreaded_Job_System_x64");
    ZoneScoped;

    uint32_t num_threads{
        std::thread::hardware_concurrency()
    };

    Simple_job_source simple_js;
    Simple_job_source simple_js2;
    Simple_job_source simple_js3;
    Simple_job_source simple_js4;

    std::vector<Job_source*> job_sources{
        &simple_js,
        &simple_js2,
        &simple_js3,
        &simple_js4,
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
