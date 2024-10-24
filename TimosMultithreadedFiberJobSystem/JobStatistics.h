#pragma once

#define JOBSTATS_ENABLE 1
#if JOBSTATS_ENABLE

#include <string>
#include <sstream>
#include <chrono>
#include <unordered_map>
#include <mutex>

#define JOBSTATS_RECORD_START(x) job_statistics::JobStatEntry new_jobstat_record{ x }
#define JOBSTATS_RECORD_END      new_jobstat_record.endTimer()
#define JOBSTATS_GENERATE_REPORT job_statistics::generate_stats_report()

namespace job_statistics
{

class JobStatEntry;

using duration_stat_t = int64_t;

struct LockableJobStatConglomeration : public std::mutex
{
    duration_stat_t avg_duration_stat{ 0 };
    uint32_t num_entries{ 0 };
};

inline static std::unordered_map<std::string, LockableJobStatConglomeration> job_name_to_stat_cong_map;
inline static std::mutex job_name_to_stat_cong_map_mutex;

std::string&& generate_stats_report()
{
    std::stringstream sstr;
    std::lock_guard<std::mutex> lock{ job_name_to_stat_cong_map_mutex };
    for (auto it = job_name_to_stat_cong_map.begin(); it != job_name_to_stat_cong_map.end(); it++)
    {
        auto& job_stat{ it->second };
        std::lock_guard<std::mutex> lock{ job_stat };
        sstr << it->first << ": " << job_stat.avg_duration_stat << "ns" << std::endl;
    }

    return std::move(sstr.str());
}

class JobStatEntry
{
public:
    JobStatEntry(std::string job_name)
        : m_job_name(job_name)
        , m_start_time(std::chrono::high_resolution_clock::now())
    {
    }

    inline void endTimer()
    {
        duration_stat_t dur{
            std::chrono::nanoseconds(std::chrono::high_resolution_clock::now() - m_start_time).count()
        };

        // Mix stat entry into stat conglomeration.
        job_name_to_stat_cong_map_mutex.lock();
        auto& stat{ job_name_to_stat_cong_map[m_job_name] };
        job_name_to_stat_cong_map_mutex.unlock();

        std::lock_guard<std::mutex> lock{ stat };

        if (stat.num_entries == 0)
        {
            stat.avg_duration_stat = dur;
            stat.num_entries = 1;
        }
        else
        {
            auto expanded_avg{ stat.avg_duration_stat * stat.num_entries };
            expanded_avg += dur;
            stat.num_entries++;
            stat.avg_duration_stat = expanded_avg / stat.num_entries;
        }
    }

private:
    std::string m_job_name;
    std::chrono::high_resolution_clock::time_point m_start_time;
};

}

#else
#define JOBSTATS_RECORD_START(x)
#define JOBSTATS_RECORD_END
#define JOBSTATS_GENERATE_REPORT
#endif
