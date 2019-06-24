#include "util.h"
#include <iostream>
#include <limits.h>
#include <limits>



#include <stdio.h>



namespace linux_epoll
{


struct timespec operator+(struct timespec const& rhs, DurationMs const& d)
{
	struct timespec result;

	uint32_t ns = rhs.tv_nsec + (d.value%1000)*1000000;

	result.tv_sec = rhs.tv_sec + d.value/1000 + ns/1000000000;
	result.tv_nsec = ns%1000000000;

	return result;
}


std::string to_string(struct timespec const& t)
{
	char buffer[26];

	size_t size = sprintf(buffer, "%lds, %ldns", t.tv_sec, t.tv_nsec);

	return std::string(buffer, size);
}


std::string to_string(DurationMs const& d)
{
	char buffer[13];

	size_t size = sprintf(buffer, "%ums", d.value);

	return std::string(buffer, size);
}


struct timespec make_ts(DurationMs const& d)
{
	return make_ts(0,0) + d;
}


struct timespec make_ts(uint32_t s, uint32_t ns)
{
	struct timespec t;
	t.tv_sec = s;
	t.tv_nsec = ns;
	return t;
}


struct itimerspec make_its(struct timespec const& ts)
{
	struct itimerspec result;
	result.it_interval.tv_sec = 0;
	result.it_interval.tv_nsec = 0;
	result.it_value = ts;
	return result;
}


linux_epoll::DurationMs make_dur(uint32_t s, uint32_t ns)
{
	uint32_t ms_from_s = s * 1000;
	uint32_t ms_from_ns = ns/1000000;

	uint32_t max_gap = std::numeric_limits<uint32_t>::max() - ms_from_s;

	if(max_gap > ms_from_ns)
	{
		return DurationMs(ms_from_s + ms_from_ns);
	}

	return DurationMs(std::numeric_limits<uint32_t>::max());
}


DurationMs operator-(DurationMs const& lhs, DurationMs const& rhs)
{
	return DurationMs(lhs.value - rhs.value);
}


} //namespace linux_epoll


bool operator==(sockaddr_in const& lhs, sockaddr_in const& rhs)
{
	return (
		(lhs.sin_addr.s_addr == rhs.sin_addr.s_addr) and
		(lhs.sin_port        == rhs.sin_port       ) and
		(lhs.sin_family      == rhs.sin_family     ));
}



bool operator==(struct timespec const& lhs, struct timespec const& rhs)
{
	return (rhs.tv_sec == lhs.tv_sec and rhs.tv_nsec == lhs.tv_nsec);
}


bool operator>(struct timespec const& lhs, struct timespec const& rhs)
{
	if(rhs.tv_sec > lhs.tv_sec)
	{
		return true;
	}
	else if(rhs.tv_sec == lhs.tv_sec)
	{
		if(rhs.tv_nsec > lhs.tv_nsec)
		{
			return true;
		}
	}

	return false;
}


bool operator<(struct timespec const& lhs, struct timespec const& rhs)
{
	if(rhs>lhs or rhs==lhs)
	{
		return false;
	}

	return true;
}


linux_epoll::DurationMs operator-(struct timespec const& lhs, struct timespec const& rhs)
{
	uint32_t sec = 0;
	uint32_t nsec = 0;

	if( lhs.tv_sec > rhs.tv_sec )
	{
		sec = lhs.tv_sec - rhs.tv_sec;

		if(lhs.tv_nsec >= rhs.tv_nsec)
		{
			nsec = lhs.tv_nsec - rhs.tv_nsec;
		}
		else
		{
			sec -= 1;
			nsec = 1000000000 - (rhs.tv_nsec - lhs.tv_nsec);
		}
	}
	else if(lhs.tv_sec == rhs.tv_sec)
	{
		if(lhs.tv_nsec >= rhs.tv_nsec)
		{
			nsec = lhs.tv_nsec - rhs.tv_nsec;
		}
	}

	return linux_epoll::make_dur(sec, nsec);
}
