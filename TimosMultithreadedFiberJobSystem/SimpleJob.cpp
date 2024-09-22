#include "SimpleJob.h"

#include <thread>
#include <iostream>


int32_t SimpleJob::execute()
{
    std::cout << "START JOB" << m_job_id << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "FINISHEED JOB" << m_job_id << std::endl;
}
