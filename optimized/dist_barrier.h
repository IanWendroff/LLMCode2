#pragma once
#include <mutex>
#include <atomic>

extern std::mutex barrier_mtx;
extern std::atomic<int> barrier_ready;

void distributed_barrier();
