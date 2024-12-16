#include "simple_job_source.h"

#include "simple_job.h"


std::vector<Job_ifc*> Simple_job_source::fetch_next_jobs_callback()
{
    // @TODO: check and think about if a better pattern could be better than
    //        leaving undeleted memory (also who owns this anyway?).
    static const std::vector<std::vector<Job_ifc*>> all_job_batches{
        {
            new Simple_job{ *this },
            new Simple_job{ *this },
            new Simple_job{ *this },
            new Simple_job{ *this },
        },
    };

    return all_job_batches[0];
}
