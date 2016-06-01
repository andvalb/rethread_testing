#ifndef RETHREAD_TESTING_BENCHMARK_H
#define RETHREAD_TESTING_BENCHMARK_H

#include <rethread/thread.hpp>
#include <rethread/cancellation_token.hpp>
#include <rethread/condition_variable.hpp>

#include <benchmark/benchmark_api.h>

#include <string>
#include <mutex>
#include <condition_variable>


struct mutex_mock
{
	void lock()   { }
	void unlock() { }
};


struct cv_mock
{
	void notify_all() { }
	void wait(std::unique_lock<mutex_mock>& l)
	{ benchmark::DoNotOptimize(&l); }
};


struct cv_mock_noinline
{
	// Defined in different translation unit to prevent virtual functions inlining
	void notify_all();
	void wait(std::unique_lock<mutex_mock>& l);
};

#endif
