#include <benchmark/benchmark.h>

void cv_mock_noinline::notify_all()
{ }

void cv_mock_noinline::wait(std::unique_lock<mutex_mock>& l)
{ benchmark::DoNotOptimize(&l); }
