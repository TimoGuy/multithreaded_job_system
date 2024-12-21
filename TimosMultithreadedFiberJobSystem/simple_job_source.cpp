#include "simple_job_source.h"

#include "simple_job.h"


Simple_job_source::Simple_job_source()
{
    // @TODO: check and think about if a better pattern could be better than
    //        leaving undeleted memory (also who owns this anyway?).
    // @REPLY: Well, I know that this instance of the job source is supposed to
    //         own this memory, but maybe writing a new type of container should
    //         be done?????
    m_all_job_batches.reserve(1);
    m_all_job_batches.emplace_back(
        std::vector<Job_ifc*>{
            new Simple_job{ *this },
            new Simple_job{ *this },
            new Simple_job{ *this },
            new Simple_job{ *this },
        }
    );
}

std::vector<Job_ifc*> Simple_job_source::fetch_next_jobs_callback()
{
    return m_all_job_batches[0];
}
