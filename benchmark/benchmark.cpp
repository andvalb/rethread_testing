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


static void WaitMock(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	while (state.KeepRunning())
		cv.wait(l);
}
BENCHMARK(WaitMock);


static void CancellableWaitMock(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	rethread::cancellation_token_atomic token;
	while (state.KeepRunning())
	{
		constexpr size_t Count = 10;
		for (size_t i = 0; i < Count; ++i)
			rethread::wait(cv, l, token);
	}
}
BENCHMARK(CancellableWaitMock);


BENCHMARK_MAIN()
