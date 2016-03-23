#include <test/poll.hpp>

#include <rethread/cancellation_token.hpp>
#include <rethread/condition_variable.hpp>
#include <rethread/thread.hpp>

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
	MOCK_CONST_METHOD0(is_cancelled, bool());

	MOCK_CONST_METHOD1(do_sleep_for, void(const std::chrono::nanoseconds& duration));

	MOCK_CONST_METHOD1(try_register_cancellation_handler, bool(cancellation_handler& handler));
	MOCK_CONST_METHOD0(try_unregister_cancellation_handler, bool());
	MOCK_CONST_METHOD0(unregister_cancellation_handler, void());
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
	cancellation_token_mock token;
	cancellation_handler_mock handler;

	EXPECT_CALL(token, try_register_cancellation_handler(_))
		.WillOnce(Return(false));

	cancellation_guard guard(token, handler);
	EXPECT_TRUE(guard.is_cancelled());
}


TEST(cancellation_guard_test, try_unregister)
{
	cancellation_token_mock token;
	cancellation_handler_mock handler;

	{
		InSequence seq;
		EXPECT_CALL(token, try_register_cancellation_handler(_))
			.WillOnce(Return(true));
		EXPECT_CALL(token, try_unregister_cancellation_handler())
			.WillOnce(Return(true));
	}

	cancellation_guard guard(token, handler);
	EXPECT_FALSE(guard.is_cancelled());
}


TEST(cancellation_guard_test, unregister)
{
	cancellation_token_mock token;
	cancellation_handler_mock handler;

	{
		InSequence seq;
		EXPECT_CALL(token, try_register_cancellation_handler(_))
			.WillOnce(Return(true));
		EXPECT_CALL(token, try_unregister_cancellation_handler())
			.WillOnce(Return(false));
		EXPECT_CALL(token, unregister_cancellation_handler())
			.Times(1);
	}

	cancellation_guard guard(token, handler);
	EXPECT_FALSE(guard.is_cancelled());
}


TEST(cancellation_token, handler_cancel_test)
{
	cancellation_token_impl token;
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


struct cancellation_token_fixture : public ::testing::Test
{
	std::mutex              _mutex;
	std::condition_variable _cv;
	cancellation_token_impl _token;
	std::atomic<bool>       _finished_flag{false};
};


TEST_F(cancellation_token_fixture, basic_thread_test)
{
	std::thread t([this] ()
	{
		while (_token)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		_finished_flag = true;
	});

	EXPECT_FALSE(_finished_flag);

	_token.cancel();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_TRUE(_finished_flag);

	t.join();
}


TEST_F(cancellation_token_fixture, thread_reset_test)
{
	rethread::thread t([this] (const cancellation_token& t)
	{
		while (t)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		_finished_flag = true;
	});

	EXPECT_FALSE(_finished_flag);
	t.reset();
	EXPECT_TRUE(_finished_flag);
}


TEST_F(cancellation_token_fixture, thread_dtor_test)
{
	{
		rethread::thread t([this] (const cancellation_token& t)
		{
			while (t)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			_finished_flag = true;
		});

		EXPECT_FALSE(_finished_flag);
	}
	EXPECT_TRUE(_finished_flag);
}


TEST_F(cancellation_token_fixture, cv_test)
{
	rethread::thread t([this] (const cancellation_token&)
	{
		std::unique_lock<std::mutex> l(_mutex);
		while (_token)
			wait(_cv, l, _token);
		_finished_flag = true;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(_finished_flag);

	_token.cancel();

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_TRUE(_finished_flag);
}


TEST_F(cancellation_token_fixture, sleep_test)
{
	rethread::thread t([this] (const cancellation_token&)
	{
		std::unique_lock<std::mutex> l(_mutex);
		while (_token)
			rethread::this_thread::sleep_for(std::chrono::minutes(1), _token);
		_finished_flag = true;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(_finished_flag);

	_token.cancel();

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_TRUE(_finished_flag);
}


struct cancellation_delay_tester : cancellation_handler
{
	std::chrono::microseconds _check_delay;
	cancellation_token_impl   _token;
	std::thread               _thread;
	std::atomic<bool>         _alive{true};

	std::atomic<bool>         _cancelled{false};
	std::atomic<bool>         _reset{false};
	std::atomic<bool>         _guard_cancelled{false};

	cancellation_delay_tester(std::chrono::microseconds checkDelay) :
		_check_delay(checkDelay)
	{ _thread = std::thread(std::bind(&cancellation_delay_tester::thread_func, this)); }

	~cancellation_delay_tester()
	{
		EXPECT_FALSE(_guard_cancelled);
		EXPECT_FALSE(_cancelled);
		EXPECT_FALSE(_reset);

		_token.cancel();
		_alive = false;
		_thread.join();

		if (!_guard_cancelled)
		{
			EXPECT_TRUE(_cancelled);
			EXPECT_TRUE(_reset);
		}
		else
		{
			EXPECT_FALSE(_cancelled);
			EXPECT_FALSE(_reset);
		}
	}

	void cancel() override
	{ _cancelled = true; }

	void reset() override
	{ _reset = true; }

	void thread_func()
	{
		std::this_thread::sleep_for(_check_delay);

		cancellation_guard guard(_token, *this);
		_guard_cancelled = guard.is_cancelled();

		while (_alive)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
};


TEST(cancellation_token, delay_test)
{
	const std::chrono::microseconds MaxDelay{100000};
	const std::chrono::microseconds DelayStep{200};
	for (std::chrono::microseconds delay{0}; delay < MaxDelay; delay += DelayStep)
	{
		cancellation_delay_tester t(delay);
		std::this_thread::sleep_for(MaxDelay - delay);
	}
}


int main(int argc, char** argv)
{
	// The following line must be executed to initialize Google Mock
	// (and Google Test) before running the tests.
	::testing::InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
}

