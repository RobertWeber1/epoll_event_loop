#pragma once

#include "linux_epoll/util.h"

#include <deque>
#include <vector>
#include <algorithm>
#include <tr1/functional>

#include <tr1/functional>
#include <time.h>



#include <stdio.h>



namespace linux_epoll
{


inline
struct timespec now()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t;
}


struct Timeout_t
{
	struct timespec deadline;
	std::tr1::function<void()> callback;
	void const* dependency;

	Timeout_t(struct timespec const& t, std::tr1::function<void()> c, void const* d)
	: deadline(t)
	, callback(c)
	, dependency(d)
	{}

	bool operator<(Timeout_t const& other) const
	{
		return deadline < other.deadline;
	}
};

struct TimeoutPred
{
	void const* dependency;

	TimeoutPred(void const* d)
	: dependency(d)
	{}

	bool operator()(Timeout_t const& t)
	{
		if(t.dependency == dependency)
		{
			printf("remove timeout for %p\n", dependency);
			return true;
		}

		return false;
	}
};


class TimeoutList
{
public:

	template<class T>
	void add(
		DurationMs duration,
		std::tr1::function<void()> callback,
		T const* dependency)
	{
		printf("add timeout for %p, %d\n", dependency, duration.value);
		timeouts_.push_back(
			Timeout_t(now()+duration, callback, static_cast<const void*>(dependency)));
		std::sort(timeouts_.begin(), timeouts_.end());
	}

	template<class T>
	void remove(T const* dependency)
	{
		timeouts_.erase(
			std::remove_if(
				timeouts_.begin(),
				timeouts_.end(),
				TimeoutPred(static_cast<const void*>(dependency))),
			timeouts_.end());
		std::sort(timeouts_.begin(), timeouts_.end());
	}

	int wait_interval()
	{
		if(not timeouts_.empty())
		{
			return (timeouts_.front().deadline - now()).value;
		}

		return -1;
	}

	void process()
	{
		while(wait_interval() == 0)
		{
			timeouts_.front().callback();
			timeouts_.pop_front();
		}
	}

private:
	std::deque<Timeout_t> timeouts_;
};


} //linux_epoll
