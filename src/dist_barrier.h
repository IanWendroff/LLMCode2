#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>

extern std::mutex barrier_mtx;
extern int barrier_ready;
extern std::condition_variable barrier_cv;

void distributed_barrier();
