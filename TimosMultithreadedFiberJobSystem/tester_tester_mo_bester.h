#pragma once

#define USE_JOJODEBUG 0
#if USE_JOJODEBUG
#include <atomic>
#include <thread>
#include <unordered_map>

extern std::unordered_map<std::thread::id, size_t> JOJODEBUG_thread_to_thread_idx_map;
extern char JOJODEBUG_actions[294967295];
extern std::atomic_size_t JOJODEBUG_actions_idx;

#define JOJODEBUG_REGISTER_THREAD(thread_idx) JOJODEBUG_thread_to_thread_idx_map[std::this_thread::get_id()] = (thread_idx)
#define JOJODEBUG_LOG_ACTION(x) JOJODEBUG_actions[JOJODEBUG_actions_idx++] = (x)[JOJODEBUG_thread_to_thread_idx_map.at(std::this_thread::get_id())]
#define JOJODEBUG_JJJJJJ_(x) (x)[JOJODEBUG_thread_to_thread_idx_map.at(std::this_thread::get_id())]
#else
#define JOJODEBUG_REGISTER_THREAD(thread_idx)
#define JOJODEBUG_LOG_ACTION(x)
#endif // USE_JOJODEBUG
