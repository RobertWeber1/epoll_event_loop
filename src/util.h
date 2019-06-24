#pragma once
#include <netinet/in.h>

#include <string>
#include <time.h>

namespace linux_epoll
{


struct DurationMs
{
	uint32_t value;

	DurationMs(uint32_t v)
	: value(v)
	{}
};

struct timespec operator+(struct timespec const& rhs, DurationMs const& d);

std::string to_string(struct timespec const& t);

std::string to_string(DurationMs const& d);

struct timespec make_ts(DurationMs const& d);

struct timespec make_ts(uint32_t s, uint32_t ns);

struct itimerspec make_its(struct timespec const& ts);

DurationMs make_dur(uint32_t s, uint32_t ns);

DurationMs operator-(DurationMs const& lhs, DurationMs const& rhs);

} //namespace linux_epoll


bool operator==(struct sockaddr_in const& lhs, struct sockaddr_in const& rhs);

linux_epoll::DurationMs operator-(struct timespec const& rhs, struct timespec const& lhs);

bool operator==(struct timespec const& rhs, struct timespec const& lhs);

bool operator>(struct timespec const& rhs, struct timespec const& lhs);

bool operator<(struct timespec const& rhs, struct timespec const& lhs);
