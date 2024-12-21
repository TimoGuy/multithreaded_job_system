#include "tester_tester_mo_bester.h"


#if USE_JOJODEBUG
std::unordered_map<std::thread::id, size_t> JOJODEBUG_thread_to_thread_idx_map;
char JOJODEBUG_actions[294967295];
std::atomic_size_t JOJODEBUG_actions_idx{ 0 };
#endif // USE_JOJODEBUG
