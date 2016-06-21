// Copyright (c) 2016, Boris Sazonov
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted,
// provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <benchmark/benchmark.h>

static void old_concurrent_queue(benchmark::State& state)
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
			if (has_object)
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
BENCHMARK(old_concurrent_queue);


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
			if (has_object)
			{
				rethread::wait(full_cond, l, t);
				continue;
			}
			has_object = true;
			empty_cond.notify_all();
		}
	});

	rethread::standalone_cancellation_token token;
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


static void cv_wait_standalone(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	rethread::standalone_cancellation_token token;
	while (state.KeepRunning())
		rethread::wait(cv, l, token);
}
BENCHMARK(cv_wait_standalone);


static void cv_wait_noinline(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	rethread::standalone_cancellation_token token;
	cv_wait_noinline_impl::impl(state, cv, l, token);
}
BENCHMARK(cv_wait_noinline);


static void cv_wait_sourced(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	rethread::cancellation_token_source source;
	rethread::sourced_cancellation_token token(source.create_token());
	while (state.KeepRunning())
		rethread::wait(cv, l, token);
}
BENCHMARK(cv_wait_sourced);


static void cv_wait_dummy(benchmark::State& state)
{
	cv_mock cv;
	mutex_mock m;
	std::unique_lock<mutex_mock> l(m);
	rethread::dummy_cancellation_token token;
	while (state.KeepRunning())
		rethread::wait(cv, l, token);
}
BENCHMARK(cv_wait_dummy);


static void is_cancelled(benchmark::State& state)
{
	rethread::standalone_cancellation_token token;
	while (state.KeepRunning())
		benchmark::DoNotOptimize(token.is_cancelled());
}
BENCHMARK(is_cancelled);


static void atomic_exchange(benchmark::State& state)
{
	std::atomic<int*> a{nullptr};
	int *value = (int*)1;
	while (state.KeepRunning())
	{
		RETHREAD_CONSTEXPR size_t Count = 5;
		for (size_t i = 0; i < Count; ++i)
		{
			value = a.exchange(value, std::memory_order_release);
			value = a.exchange(value, std::memory_order_acquire);
		}
	}
}
BENCHMARK(atomic_exchange);


static void atomic_compare_exchange(benchmark::State& state)
{
	std::atomic<int*> a{nullptr};
	int *value1 = nullptr;
	int *value2 = (int*)1;
	while (state.KeepRunning())
	{
		RETHREAD_CONSTEXPR size_t Count = 5;
		for (size_t i = 0; i < Count; ++i)
		{
			int* val = value1;
			while (!a.compare_exchange_weak(val, value2, std::memory_order_release, std::memory_order_relaxed))
				;
			val = value2;
			while (!a.compare_exchange_weak(val, value1, std::memory_order_acquire, std::memory_order_relaxed))
				;
		}
	}
}
BENCHMARK(atomic_compare_exchange);


static void atomic_fetch_add(benchmark::State& state)
{
	std::atomic<int*> a{nullptr};
	while (state.KeepRunning())
	{
		RETHREAD_CONSTEXPR size_t Count = 5;
		for (size_t i = 0; i < Count; ++i)
		{
			a.fetch_add(123, std::memory_order_release);
			a.fetch_sub(123, std::memory_order_acquire);
		}
	}
}
BENCHMARK(atomic_fetch_add);


static void create_dummy_token(benchmark::State& state)
{
	while (state.KeepRunning())
	{
		rethread::dummy_cancellation_token token;
		benchmark::DoNotOptimize(token);
	}
}
BENCHMARK(create_dummy_token);


static void create_standalone_token(benchmark::State& state)
{
	while (state.KeepRunning())
	{
		rethread::standalone_cancellation_token token;
		benchmark::DoNotOptimize(token);
	}
}
BENCHMARK(create_standalone_token);


static void create_cancellation_token_source(benchmark::State& state)
{
	while (state.KeepRunning())
	{
		rethread::cancellation_token_source source;
		benchmark::DoNotOptimize(source);
	}
}
BENCHMARK(create_cancellation_token_source);


static void create_sourced_cancellation_token(benchmark::State& state)
{
	rethread::cancellation_token_source source;
	while (state.KeepRunning())
	{
		rethread::sourced_cancellation_token token(source.create_token());
		benchmark::DoNotOptimize(token);
	}
}
BENCHMARK(create_sourced_cancellation_token);


BENCHMARK_MAIN()
