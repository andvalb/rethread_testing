#include <rethread/thread.hpp>
#include <rethread/cancellation_token.hpp>
#include <rethread/condition_variable.hpp>

#include <benchmark/benchmark_api.h>

#include <string>
#include <mutex>
#include <condition_variable>


static void ConcurrentQueue(benchmark::State& state)
{
	std::mutex m;
	std::condition_variable empty_cond;
	std::condition_variable full_cond;
	bool has_object = false;
	bool done = false;
	std::thread t([&] ()
	{
		std::unique_lock<std::mutex> l(m);
		while (!done)
		{
			while (has_object)
			{
				full_cond.wait(l);
				continue;
			}
			has_object = true;
			empty_cond.notify_all();
		}
	});

	std::unique_lock<std::mutex> l(m);
	while (state.KeepRunning())
	{
		while (!has_object)
		{
			empty_cond.wait_for(l, std::chrono::milliseconds(100));
			continue;
		}
		has_object = false;
		full_cond.notify_all();
	}
	done = true;
	l.unlock();

	t.join();
}
BENCHMARK(ConcurrentQueue);


static void CancellableConcurrentQueue(benchmark::State& state)
{
	std::mutex m;
	std::condition_variable empty_cond;
	std::condition_variable full_cond;
	bool has_object = false;
	rethread::thread t([&] (const rethread::cancellation_token& t)
	              {
		              std::unique_lock<std::mutex> l(m);
		              while (t)
		              {
			              while (t && has_object)
			              {
				              rethread::wait(full_cond, l, t);
				              continue;
			              }
			              has_object = true;
			              empty_cond.notify_all();
		              }
	              });

	rethread::cancellation_token_atomic token;
	std::unique_lock<std::mutex> l(m);
	while (state.KeepRunning())
	{
		while (!has_object)
		{
			rethread::wait(empty_cond, l, token);
			continue;
		}
		has_object = false;
		full_cond.notify_all();
	}
	l.unlock();

	t.reset();
}
BENCHMARK(CancellableConcurrentQueue);


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
