#include "JobStatistics.h"


void job_statistics::register_job_name(const std::string& name)
{
    std::lock_guard<std::mutex> lock{ job_name_to_stat_cong_map_mutex };
    job_name_to_stat_cong_map[name];  // Adds entry to map if doesn't exist.
    // @NOTE: if all jobs get registered at very beginning of program,
    //        then `job_name_to_stat_cong_map` will be thread-safe due to
    //        no mutations of the map itself.
}

std::string job_statistics::generate_stats_report()
{
    std::stringstream sstr;
    for (auto it = job_name_to_stat_cong_map.begin(); it != job_name_to_stat_cong_map.end(); it++)
    {
        auto& job_stat{ it->second };
        std::lock_guard<std::mutex> lock{ job_stat };
        sstr << it->first << ": " << job_stat.avg_duration_stat << "ns" << std::endl;
    }

    return sstr.str();
}
