#pragma once

#include <atomic>


extern char JOJODEBUG_actions[294967295];
extern std::atomic_size_t JOJODEBUG_actions_idx;

#define USE_JOJODEBUG 0
#if USE_JOJODEBUG
#define JOJODEBUG_LOG_ACTION(x) JOJODEBUG_actions[JOJODEBUG_actions_idx++] = x
#else
#define JOJODEBUG_LOG_ACTION(x)
#endif // USE_JOJODEBUG
