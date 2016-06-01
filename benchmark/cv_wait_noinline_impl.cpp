#include <benchmark/benchmark.h>

void cv_wait_noinline_impl::impl(benchmark::State& state, cv_mock& cv, std::unique_lock<mutex_mock>& l, const rethread::cancellation_token& t)
{
	while (state.KeepRunning())
	{
		constexpr size_t Count = 10;
		for (size_t i = 0; i < Count; ++i)
			rethread::wait(cv, l, t);
	}
}
