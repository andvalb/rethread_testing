#ifndef TEST_POLL_HPP
#define TEST_POLL_HPP

// Copyright (c) 2016, Boris Sazonov
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted,
// provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <rethread/cancellation_token.hpp>
#include <rethread/condition_variable.hpp>
#include <rethread/poll.hpp>
#include <rethread/thread.hpp>

#include <gtest/gtest.h>

#include <exception>

template <typename Func>
class scope_exit_holder
{
private:
	bool _valid;
	Func _f;

public:
	scope_exit_holder(Func f) :
		_valid(true), _f(std::move(f))
	{ }

	scope_exit_holder(scope_exit_holder&& other) :
		_valid(other._valid), _f(std::move(other._f))
	{ other._valid = false; }

	~scope_exit_holder()
	{
		if (_valid)
			_f();
	}
};


template <typename Func>
scope_exit_holder<Func> scope_exit(Func f)
{ return scope_exit_holder<Func>(std::move(f)); }


TEST(helpers, poll)
{
	using namespace rethread;

	int pipe[2];
	RETHREAD_CHECK(::pipe(pipe) == 0, std::system_error(errno, std::system_category()));

	auto scope_guard = scope_exit([&pipe] { RETHREAD_CHECK(::close(pipe[0]) == 0 || ::close(pipe[1]) == 0, std::system_error(errno, std::system_category())); } );

	std::atomic<bool> started{false}, readData{false}, finished{false};

	rethread::thread t([&] (const cancellation_token& token)
	{
		started = true;
		while (token)
		{
			if (rethread::poll(pipe[0], POLLIN, token) != POLLIN)
				continue;

			char dummy = 0;
			RETHREAD_CHECK(::read(pipe[0], &dummy, 1) == 1, std::runtime_error("Can't read data!"));

			readData = true;
		}
		finished = true;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	EXPECT_TRUE(started);
	EXPECT_FALSE(readData);
	EXPECT_FALSE(finished);

	char dummy = 0;
	RETHREAD_CHECK(::write(pipe[1], &dummy, 1) == 1, std::runtime_error("Can't write data!"));
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	EXPECT_TRUE(readData);
	EXPECT_FALSE(finished);

	t.reset();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	EXPECT_TRUE(finished);
}

#endif
