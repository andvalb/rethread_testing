// Copyright (c) 2016, Boris Sazonov
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted,
// provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#if defined(RETHREAD_HAS_POLL)
#include <test/poll.hpp>
#endif

#include <rethread/cancellation_token.hpp>
#include <rethread/condition_variable.hpp>
#include <rethread/thread.hpp>
#include <rethread/cancellation_token.hpp>

#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using namespace rethread;

class cancellation_token_mock : public cancellation_token
{
public:
	MOCK_METHOD0(cancel, void());
	MOCK_METHOD0(reset, void());

	MOCK_CONST_METHOD1(do_sleep_for, void(const std::chrono::nanoseconds& duration));

	MOCK_CONST_METHOD1(unregister_cancellation_handler, void(cancellation_handler&));
};


class cancellation_handler_mock : public cancellation_handler
{
public:
	MOCK_METHOD0(cancel, void());
	MOCK_METHOD0(reset, void());
};

using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::_;

TEST(cancellation_guard_test, basic)
{
	standalone_cancellation_token token;
	cancellation_handler_mock handler;

	token.cancel();

	cancellation_guard guard(token, handler);
	EXPECT_TRUE(guard.is_cancelled());
}


TEST(cancellation_token, handler_cancel_test)
{
	standalone_cancellation_token token;
	cancellation_handler_mock handler;

	{
		InSequence seq;

		EXPECT_CALL(handler, cancel()).Times(1);
		EXPECT_CALL(handler, reset()).Times(1);
	}

	cancellation_guard guard(token, handler);
	EXPECT_FALSE(guard.is_cancelled());
	token.cancel();
	EXPECT_TRUE(token.is_cancelled());
}


struct testing_flag
{
	bool                            _value{false};
	mutable std::mutex              _mutex;
	mutable std::condition_variable _cv;

	void set()
	{
		std::unique_lock<std::mutex> l(_mutex);
		if (_value)
			return;

		_value = true;
		_cv.notify_all();
	}

	bool is_set() const
	{
		std::unique_lock<std::mutex> l(_mutex);
		return _value;
	}

	template <typename Duration_>
	bool is_set(const Duration_& duration) const
	{
		std::unique_lock<std::mutex> l(_mutex);
		auto end_time = std::chrono::steady_clock::now() + duration;
		while (!_value)
		{
			if (_cv.wait_until(l, end_time) == std::cv_status::timeout)
				return false;
		}
		return true;
	}
};

struct cancellation_token_fixture : public ::testing::Test
{
	std::mutex                    _mutex;
	std::condition_variable       _cv;
	standalone_cancellation_token _token;

	testing_flag                  _started;
	testing_flag                  _finished;
};


TEST_F(cancellation_token_fixture, basic_thread_test)
{
	std::thread t([this] ()
	{
		while (_token)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		_finished.set();
	});

	EXPECT_FALSE(_finished.is_set());

	_token.cancel();

	EXPECT_TRUE(_finished.is_set(std::chrono::seconds(3)));

	t.join();
}


TEST(thread, default_ctor_and_reset)
{
	rethread::thread t;
	t.reset();
}


TEST_F(cancellation_token_fixture, thread_reset_test)
{
	rethread::thread t([this] (const cancellation_token& t)
	{
		while (t)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		_finished.set();
	});

	EXPECT_FALSE(_finished.is_set());
	t.reset();
	EXPECT_TRUE(_finished.is_set());
}


TEST_F(cancellation_token_fixture, thread_dtor_test)
{
	{
		rethread::thread t([this] (const cancellation_token& t)
		{
			while (t)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			_finished.set();
		});

		EXPECT_FALSE(_finished.is_set());
	}
	EXPECT_TRUE(_finished.is_set());
}


TEST_F(cancellation_token_fixture, cv_test)
{
	rethread::thread t([this] (const cancellation_token&)
	{
		std::unique_lock<std::mutex> l(_mutex);
		while (_token)
		{
			_started.set();
			wait(_cv, l, _token);
		}
		_finished.set();
	});

	EXPECT_TRUE(_started.is_set(std::chrono::seconds(3)));

	_token.cancel();

	EXPECT_TRUE(_finished.is_set(std::chrono::seconds(3)));
}


TEST_F(cancellation_token_fixture, cv_predicate_test)
{
	bool flag = false;
	bool result = false;
	rethread::thread t([this, &flag, &result] (const cancellation_token&)
	{
	   std::unique_lock<std::mutex> l(_mutex);
	   _started.set();
	   result = wait(_cv, l, _token, [&flag] { return flag; });
	   _finished.set();
	});

	EXPECT_TRUE(_started.is_set(std::chrono::seconds(3)));

	for (int i = 0; i < 10; ++i)
	{
		std::unique_lock<std::mutex> l(_mutex);
		_cv.notify_all();
	}

	EXPECT_FALSE(_finished.is_set());

	std::unique_lock<std::mutex> l(_mutex);
	flag = true;
	_cv.notify_all();
	l.unlock();

	EXPECT_TRUE(_finished.is_set(std::chrono::seconds(3)));

	l.lock();
	EXPECT_TRUE(result);
}


TEST_F(cancellation_token_fixture, cv_predicate_cancel)
{
	bool flag = false;
	bool result = false;
	rethread::thread t([this, &flag, &result] (const cancellation_token&)
	{
	   std::unique_lock<std::mutex> l(_mutex);
	   _started.set();
	   result = wait(_cv, l, _token, [&flag] { return flag; });
	   _finished.set();
	});

	EXPECT_TRUE(_started.is_set(std::chrono::seconds(3)));

	for (int i = 0; i < 10; ++i)
	{
		std::unique_lock<std::mutex> l(_mutex);
		_cv.notify_all();
	}

	EXPECT_FALSE(_finished.is_set());

	_token.cancel();

	EXPECT_TRUE(_finished.is_set(std::chrono::seconds(3)));

	std::unique_lock<std::mutex> l(_mutex);
	EXPECT_FALSE(result);
}


TEST_F(cancellation_token_fixture, cv_predicate_nowait)
{
	bool flag = true;
	bool result = false;
	rethread::thread t([this, &flag, &result] (const cancellation_token&)
	{
	   std::unique_lock<std::mutex> l(_mutex);
	   _started.set();
	   result = wait(_cv, l, _token, [&flag] { return flag; });
	   _finished.set();
	});

	EXPECT_TRUE(_started.is_set(std::chrono::seconds(3)));
	EXPECT_TRUE(_finished.is_set(std::chrono::seconds(3)));

	std::unique_lock<std::mutex> l(_mutex);
	EXPECT_TRUE(result);
}


TEST_F(cancellation_token_fixture, sleep_test)
{
	rethread::thread t([this] (const cancellation_token&)
	{
		std::unique_lock<std::mutex> l(_mutex);
		while (_token)
		{
			_started.set();
			rethread::this_thread::sleep_for(std::chrono::minutes(1), _token);
		}
		_finished.set();
	});

	EXPECT_TRUE(_started.is_set(std::chrono::seconds(3)));

	_token.cancel();

	EXPECT_TRUE(_finished.is_set(std::chrono::seconds(3)));
}


TEST(cancellation_token, source)
{
	std::mutex                m;
	std::condition_variable   cv;
	std::atomic<size_t>       finished_counter{0};
	cancellation_token_source source;

	auto thread_fun = [&](const cancellation_token& t)
	{
		std::unique_lock<std::mutex> l(m);
		while (t)
			wait(cv, l, t);
		++finished_counter;
	};

	const size_t Count = 10;
	std::vector<std::thread> v;
	for (size_t i = 0; i < Count; ++i)
		v.emplace_back(std::bind(thread_fun, source.create_token()));

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(finished_counter.load(), 0u);

	source.cancel();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(finished_counter.load(), Count);

	std::for_each(begin(v), end(v), std::bind(&std::thread::join, std::placeholders::_1));
}


struct cancellation_handler_dummy : cancellation_handler
{
	std::atomic<bool> _cancelled{false};
	std::atomic<bool> _reset{false};

	void cancel() override
	{ _cancelled = true; }

	void reset() override
	{ _reset = true; }
};


template <typename CancelFunctor_>
void do_stress_test(std::chrono::nanoseconds delay1, std::chrono::nanoseconds delay2, const cancellation_token& token, const CancelFunctor_& cancel)
{
	testing_flag                  started1;
	testing_flag                  started2;

	std::atomic<bool>          guard_cancelled{false};
	cancellation_handler_dummy handler;

	std::thread t([&] ()
	{
		started2.set();
		started1.is_set(std::chrono::seconds(3));

		std::this_thread::sleep_for(delay1);

		cancellation_guard guard(token, handler);
		guard_cancelled = guard.is_cancelled();

		while (token)
			std::this_thread::sleep_for(std::chrono::microseconds(20));
	});

	started1.set();
	started2.is_set(std::chrono::seconds(3));

	std::this_thread::sleep_for(delay2);

	cancel();
	t.join();

	if (!guard_cancelled)
	{
		EXPECT_TRUE(handler._cancelled);
		EXPECT_TRUE(handler._reset);
	}
	else
	{
		EXPECT_FALSE(handler._cancelled);
		EXPECT_FALSE(handler._reset);
	}
}


TEST(cancellation_token, stress_test_standalone)
{
	const std::chrono::nanoseconds MaxDelay{10000};
	const std::chrono::nanoseconds DelayStep{10};
	for (std::chrono::nanoseconds delay{0}; delay < MaxDelay; delay += DelayStep)
	{
		standalone_cancellation_token token;
		do_stress_test(delay, MaxDelay - delay, token, [&token] { token.cancel(); });
	}
}


TEST(cancellation_token, stress_test_sourced)
{
	const std::chrono::nanoseconds MaxDelay{10000};
	const std::chrono::nanoseconds DelayStep{10};
	for (std::chrono::nanoseconds delay{0}; delay < MaxDelay; delay += DelayStep)
	{
		cancellation_token_source source;
		sourced_cancellation_token token(source.create_token());
		do_stress_test(delay, MaxDelay - delay, token, [&source] { source.cancel(); });
	}
}


TEST(cancellation_token, dummy_copy_test)
{
	dummy_cancellation_token token;
	dummy_cancellation_token copy(token); // dummy_cancellation_token should be copyable
}


TEST(chain_cancellation_tokens, standalone)
{
	standalone_cancellation_token token1;
	standalone_cancellation_token token2;
	chain_cancellation_tokens chain(token1, token2);
	cancellation_handler_mock handler;

	{
		InSequence seq;
		EXPECT_CALL(handler, cancel()).Times(1);
		EXPECT_CALL(handler, reset()).Times(1);
	}

	cancellation_guard guard(token2, handler);

	EXPECT_TRUE(token1);
	EXPECT_TRUE(token2);
	EXPECT_FALSE(guard.is_cancelled());

	token1.cancel();

	EXPECT_FALSE(token1);
	EXPECT_FALSE(token2);
}


TEST(chain_cancellation_tokens, source)
{
	standalone_cancellation_token token1;
	cancellation_token_source source;
	sourced_cancellation_token token2(source.create_token());
	chain_cancellation_tokens chain(token1, source);

	cancellation_handler_mock handler;

	{
		InSequence seq;

		EXPECT_CALL(handler, cancel()).Times(1);
		EXPECT_CALL(handler, reset()).Times(1);
	}

	cancellation_guard guard(token2, handler);

	EXPECT_TRUE(token1);
	EXPECT_TRUE(token2);
	EXPECT_FALSE(guard.is_cancelled());

	token1.cancel();

	EXPECT_FALSE(token1);
	EXPECT_FALSE(token2);
}


int main(int argc, char** argv)
{
	// The following line must be executed to initialize Google Mock
	// (and Google Test) before running the tests.
	::testing::InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
}

