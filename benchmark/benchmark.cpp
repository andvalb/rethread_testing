// Copyright (c) 2016, Boris Sazonov
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted,
// provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <benchmark/benchmark.h>

static void concurrent_queue(benchmark::State& state)
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
BENCHMARK(concurrent_queue);


static void cancellable_concurrent_queue(benchmark::State& state)
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
BENCHMARK(cancellable_concurrent_queue);


static void cv_wait_baseline(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	while (state.KeepRunning())
		cv.wait(l);
}
BENCHMARK(cv_wait_baseline);


static void cv_wait(benchmark::State& state)
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
BENCHMARK(cv_wait);


static void cv_wait_noinline(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	rethread::cancellation_token_atomic token;
	cv_wait_noinline_impl::impl(state, cv, l, token);
}
BENCHMARK(cv_wait_noinline);


BENCHMARK_MAIN()
