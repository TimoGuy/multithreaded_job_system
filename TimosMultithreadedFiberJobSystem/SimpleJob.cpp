#include "SimpleJob.h"

#include <thread>
#include <iostream>
#include "TracyImpl.h"


int32_t SimpleJob::execute()
{
    ZoneScoped;
 
    int32_t jojo = 69;
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));

    return 0;
}
