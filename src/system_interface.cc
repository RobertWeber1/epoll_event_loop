#include "e37/system_interface.h"
#include "util.h"

#include <deque>
#include <vector>
#include <algorithm>
#include <tr1/functional>

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <iostream>


namespace e37
{


struct timespec now_ts()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t;
}


struct Timeout_t
{
	std::tr1::function<void()> callback;
	struct timespec deadline;

	Timeout_t(struct timespec const& t, std::tr1::function<void()> c)
	: callback(c)
	, deadline(t)
	{}

	bool operator<(Timeout_t const& other) const
	{
		return deadline < other.deadline;
	}
};


struct Socket
{
	Socket(
		Endpoint const& ep_,
		DurationMs retry_interval_,
		std::tr1::function<void(LowLevelConnection&)> connect_callback_)
	: ep(ep_)
	, retry_interval(retry_interval_)
	, connect_callback(connect_callback_)
	, socket_fd(-1)
	, state(TryToOpen)
	{}

	int init()
	{
		if(socket_fd == -1)
		{
			socket_fd = socket(AF_INET, SOCK_STREAM, 0);
			if(socket_fd == -1)
			{
				perror("create socket");
				exit(EXIT_FAILURE);
			}

			if(fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) | O_NONBLOCK) == -1)
			{
				perror("fcntl make socket non-blocking");
				exit(EXIT_FAILURE);
			}
		}

		return socket_fd;
	}

	bool connect()
	{
		if(socket_fd != -1)
		{
			sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_addr.s_addr = inet_addr(ep.ip.c_str());
			addr.sin_port		 = htons(ep.port);
			addr.sin_family		 = AF_INET;


			if(::connect(socket_fd, (sockaddr*) &addr, sizeof(addr)) == -1)
			{
				if(errno == EINPROGRESS)
				{
					std::cout << "CONNECT IN PROGRESS\n";
					return true;
				}
			}
		}
		return false;
	}

	void listen()
	{}

	~Socket()
	{
		if(socket_fd != -1)
		{
			close(socket_fd);
		}
	}

	Endpoint ep;
	DurationMs retry_interval;
	std::tr1::function<void(LowLevelConnection&)> connect_callback;
	int socket_fd;

	enum State
	{
		TryToOpen,
		Opened,
		Closed
	}state;
};


struct SystemInterface::Impl
{
	static const uint32_t MAX_EVENTS = 10;

	int                       epoll_fd;
	int                       event_count;
	struct epoll_event        events[MAX_EVENTS];
	std::deque<Timeout_t>     timeouts;
	std::vector<Socket> endpoints;
	typedef std::vector<Socket>::const_iterator CIter;
	typedef std::vector<Socket>::iterator Iter;




	Impl()
	: epoll_fd(-1)
	, event_count(0)
	{
		if((epoll_fd = epoll_create(MAX_EVENTS)) == -1)
		{
			perror("epoll_create1");
			exit(EXIT_FAILURE);
		}
	}

	~Impl()
	{
		close(epoll_fd);
	}

	void add_poll_fd(int fd, uint32_t event_mask)
	{
		struct epoll_event ev;
		memset(&ev, 0, sizeof(ev));
		ev.events = event_mask;
		ev.data.fd = fd;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
		{
			perror("epoll_ctl: add fd");
			exit(EXIT_FAILURE);
		}
	}

	void del_poll_fd(int fd)
	{
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	}

	// bool is_duplicate(Endpoint const& ep) const
	// {
	// 	for(CIter it = endpoints.begin(); it != endpoints.end(); ++it)
	// 	{
	// 		if(it->ep == ep)
	// 		{
	// 			return true;
	// 		}
	// 	}

	// 	return false;
	// }

	Socket & add_socket(
		Endpoint const& ep,
		DurationMs retry_interval,
		std::tr1::function<void(LowLevelConnection &)> connect_callback)
	{
		if(endpoints.size() >= MAX_EVENTS)
		{
			perror("reached maximum number of endpoints");
			exit(EXIT_FAILURE);
		}

		Iter it;
		for(it = endpoints.begin(); it != endpoints.end(); ++it)
		{
			if(it->ep == ep)
			{
				break;
			}
		}

		if(it == endpoints.end())
		{
			endpoints.push_back(Socket(ep, retry_interval, connect_callback));
			return endpoints.back();
		}

		return *it;
	}
};


SystemInterface::SystemInterface()
: impl_(new Impl())
{}


SystemInterface::~SystemInterface()
{
	delete impl_;
}


void SystemInterface::register_timeout(
	DurationMs duration,
	std::tr1::function<void()> callback)
{
	impl_->timeouts.push_back(Timeout_t(now_ts()+duration, callback));
	std::sort(impl_->timeouts.begin(), impl_->timeouts.end());
}


void SystemInterface::connect_to(
	Endpoint const& ep,
	DurationMs retry_interval,
	std::tr1::function<void(LowLevelConnection &)> connect_callback)
{
	Socket & sock = impl_->add_socket(ep, retry_interval, connect_callback);
	impl_->add_poll_fd(sock.init(), EPOLLIN|EPOLLOUT|EPOLLHUP|EPOLLET);

	sock.connect();
}


void SystemInterface::listen_on(
	Endpoint const& /*ep*/,
	DurationMs /*retry_interval*/,
	std::tr1::function<void(LowLevelConnection &)> /*connect_callback*/)
{}


int wait_interval(
	struct timespec const& now,
	std::deque<Timeout_t> const& timeouts)
{
	if(not timeouts.empty())
	{
		return (timeouts.front().deadline - now).value;
	}

	return -1;
}


void SystemInterface::wait()
{
	impl_->event_count = epoll_wait(
		impl_->epoll_fd,
		impl_->events,
		Impl::MAX_EVENTS,
		wait_interval(now_ts(), impl_->timeouts));

	if(impl_->event_count == -1)
	{
		perror("epoll_wait");
		exit(EXIT_FAILURE);
	}
}


void SystemInterface::process()
{
	while(wait_interval(now_ts(), impl_->timeouts) == 0)
	{
		impl_->timeouts.front().callback();
		impl_->timeouts.pop_front();
	}

	for (int n = 0; n < impl_->event_count; ++n)
	{
		std::cout << "process socket(" << impl_->events[n].data.fd << ") events ";
		if((impl_->events[n].events & EPOLLHUP) != 0)
		{
			std::cout << "hup ";
		}
		else
		{
			if((impl_->events[n].events & EPOLLIN) != 0)
			{
				std::cout << "readable ";
			}
			if((impl_->events[n].events & EPOLLOUT) != 0)
			{
				std::cout << "writeable ";
			}
		}

		std::cout << std::endl;
		/*TODO: process read/write-able fds*/
	}

	impl_->event_count = 0;
}


//----------------------------------------------------------------------------//


struct Watchdog::Impl
{
	Impl(
		SystemInterface & sys,
		DurationMs interval,
		std::tr1::function<void()> callback)
	: sys_(sys)
	, interval_(interval)
	, callback_(callback)
	, last_reset_(now_ts())
	{
		sys_.register_timeout(interval_, std::tr1::bind(&Watchdog::Impl::check, this));
	}

	void check()
	{
		DurationMs delta = now_ts() - last_reset_;

		if(delta.value >= interval_.value)
		{
			callback_();
			last_reset_ = now_ts();
			sys_.register_timeout(interval_, std::tr1::bind(&Watchdog::Impl::check, this));
		}
		else
		{
			sys_.register_timeout((interval_- delta), std::tr1::bind(&Watchdog::Impl::check, this));
		}
	}

	SystemInterface          & sys_;
	DurationMs                 interval_;
	std::tr1::function<void()> callback_;
	struct timespec            last_reset_;
};


Watchdog::Watchdog(
	SystemInterface & sys,
	DurationMs interval,
	std::tr1::function<void()> callback)
: impl_(new Watchdog::Impl(sys, interval, callback))
{}


Watchdog::~Watchdog()
{
	delete impl_;
}


void Watchdog::reset()
{
	impl_->last_reset_ = now_ts();
}


} //namespace e37
