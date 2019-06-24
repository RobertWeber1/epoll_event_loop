#pragma once

#include "linux_epoll/util.h"
#include "linux_epoll/timeout.h"
#include "linux_epoll/list.h"
#include "linux_epoll/sockets.h"
#include "linux_epoll/pollable.h"

#include <deque>
#include <vector>
#include <algorithm>
#include <tr1/functional>

#include <stdint.h>
#include <string.h>

#include <sys/epoll.h>
#include <sys/types.h>


namespace linux_epoll
{


template<uint32_t SIZE>
class Epoll
{
public:
	Epoll()
	: event_count_(0)
	{
		printf("Epoll CTor\n");
		fd_ = epoll_create(SIZE);
		if(fd_ == -1)
		{
			perror("epoll_create");
			exit(EXIT_FAILURE);
		}
	}

	template<class T>
	void register_timeout(
		DurationMs duration,
		std::tr1::function<void()> callback,
		T const* dependencies)
	{
		timeouts_.add(duration, callback, dependencies);
	}

	template<class T>
	void remove_timeouts(T const* dependency)
	{
		timeouts_.remove(dependency);
	}

	void wait()
	{
		event_count_ = epoll_wait(
			fd_,
			events_,
			SIZE,
			timeouts_.wait_interval());

		if(event_count_ == -1)
		{
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}
	}

	void process()
	{
		printf("-->process()\n");

		timeouts_.process();

		for (int n = 0; n < event_count_; ++n)
		{
			reinterpret_cast<Pollable *>(events_[n].data.ptr)->
				process_events(events_[n].events);
		}
	}

	template<class T>
	bool add(T & t, int event_mask = EPOLLIN|EPOLLOUT|EPOLLHUP|EPOLLET)
	{
		Pollable * p = pollables_.add(Pollable(t));

		if(p)
		{
			printf("epoll_fd:%d add fd:%d\n", fd_, p->get_fd());

			if(epoll_ctl(fd_, EPOLL_CTL_ADD, p->get_fd(), ev(event_mask, p)) == -1)
			{
				perror("epoll_ctl: add fd");
				exit(EXIT_FAILURE);
			}

			p->added();
			return true;
		}
		return false;
	}

	template<class T>
	void remove(T & t)
	{
		Pollable tmp(t);

		Pollable * pollable = pollables_.find_if(FdPred(tmp.get_fd()));

		if(pollable)
		{
			printf("epoll_fd:%d remove fd:%d\n", fd_, pollable->get_fd());

			if(epoll_ctl(fd_, EPOLL_CTL_DEL, pollable->get_fd(), ev()) == -1)
			{
				perror("epoll_ctl: remove fd");
				exit(EXIT_FAILURE);
			}

			pollables_.remove(pollable);
			remove_timeouts(&t);
			pollable->removed();
		}
	}

	bool is_full() const
	{
		return pollables_.is_full();
	}

private:
	int                  fd_;
	int                  event_count_;
	struct epoll_event   events_[SIZE];
	List<Pollable, SIZE> pollables_;
	TimeoutList          timeouts_;

	struct FdPred
	{
		int fd;

		FdPred(int fd_)
		: fd(fd_)
		{}

		bool operator()(Pollable const& pollable) const
		{
			return pollable.get_fd() == fd;
		}
	};

	epoll_event * ev(int mask=0, void* ptr=NULL)
	{
		static epoll_event ev;
		memset(&ev, 0, sizeof(ev));
		ev.events = mask;
		ev.data.ptr = ptr;
		return &ev;
	}
};


} //namespace linux_epoll
