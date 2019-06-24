#pragma once
#include "linux_epoll/util.h"
#include "linux_epoll/list.h"

#include <tr1/functional>
#include <algorithm>
#include <string>
#include <queue>
#include <stdexcept>

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>



#include <iostream>


namespace linux_epoll
{



/* EXAMPLE:
struct LocalEndpoint
{
	uint8_t * get_buffer() = 0;
	uint32_t get_buffer_size() = 0;
	void connected() = 0;
	void process_read_data(uint8_t const* data, uint32_t size) = 0;
	void disconnected() = 0;
	void set_write_function(std::tr1::function<void(uint8_t const*, uint32_t)> write_)
	{
		write = write_;
	}

	std::tr1::function<void(uint8_t const*, uint32_t)> write;
};*/


// void register_timeout(
// 		DurationMs duration,
// 		std::tr1::function<void()> callback,
// 		T const* dependencies)
// void remove_timeouts(T const* dependency)
// bool add(T & t, int event_mask = EPOLLIN|EPOLLHUP|EPOLLET)
// void remove(T & t)
// bool is_full() const



struct SystemFunctions
{
	struct Result
	{
		Result(int ret_val)
		: result_(ret_val)
		, error_((ret_val == -1)?errno:0)
		{}

		operator bool() const
		{
			return result_ != -1;
		}

		char const* error_description const
		{
			return strerr(error_);
		}

		int value() const
		{
			return result_;
		}

		int error_code() const
		{
			return error_;
		}
	private:
		int error_;
		int result_;
	};


	inline
	Result ioctl_(int fd, unsigned long request, int * ret )
	{
		return ioctl(fd, request, ret);
	}

	inline
	Result socket_(int domain, int type, int protocol)
	{
		return socket(domain, type, protocol);
	}

	inline
	void close_(int fd)
	{
		::close(fd);
	}

	inline
	Result read_(int fd, void *buf, size_t count)
	{
		return ::read(fd, buf, count);
	}

	inline
	Result write_(int fd, const void *buf, size_t count)
	{
		return ::write(fd, buf, count);
	}

	inline
	Result connect_(int fd, const sockaddr *addr, socklen_t addrlen)
	{
		return connect(fd, addr, addrlen);
	}

	inline
	Result setsockopt_(
		int fd,
		int level,
		int optname,
		const void *optval,
		socklen_t optlen)
	{
		return setsockopt(fd, level, optname, optval, optlen);
	}

	inline
	Result bind_(int fd, const sockaddr *addr, socklen_t addrlen)
	{
		return bind(fd, addr, addrlen);
	}

	inline
	Result listen_(int fd, int backlog)
	{
		return listen(fd, backlog);
	}

	inline
	Result accept_(int fd, struct sockaddr *addr, socklen_t *addrlen)
	{
		return accept(fd, addr, addrlen);
	}

	inline
	char * strerror_()
	{
		return strerror(errno);
	}
};




template<class LOCAL_ENDPOINT, class SYS = SystemFunctions>
class TcpSocket : private SYS
{
public:
	typedef TcpSocket<LOCAL_ENDPOINT, SYS> Self_t;

	TcpSocket()
	: endpoint_(NULL)
	, fd_(-1)
	, connected_(false)
	{}

	TcpSocket(int fd)
	: endpoint_(NULL)
	, fd_(fd)
	, connected_(false)
	{}

	~TcpSocket()
	{
		close();
	}

	int get_fd() const
	{
		return fd_;
	}

	uint32_t available_data()
	{
		if(fd_ != -1 )
		{
			int count = 0;
			SYS::ioctl_(fd_, FIONREAD, &count);
			if(count > 0)
			{
				return count;
			}
		}
		return 0;
	}

	void open()
	{
		if(fd_ == -1 )
		{
			open_(SYS::socket_(AF_INET, SOCK_STREAM, 0));
		}
	}

	void close()
	{
		if(fd_ != -1)
		{
			SYS::close_(fd_);
			fd_ = -1;
		}
	}

	void reopen()
	{
		close();
		open();
	}

	void set(LOCAL_ENDPOINT * endpoint)
	{
		endpoint_ = endpoint;
		endpoint_->set_write_function(
			std::tr1::bind(
				&Self_t::write,
				this,
				std::tr1::placeholders::_1,
				std::tr1::placeholders::_2));
	}

	void set(std::tr1::function<void(Self_t*)> handle_terminated_connection)
	{
		handle_terminated_connection_ = handle_terminated_connection;
	}

	void set(sockaddr_in const& addr)
	{
		std::copy(&addr, &addr+1, &addr_);
	}

	void set(std::string const& ip, uint16_t port)
	{
		memset(&addr_, 0, sizeof(addr_));
		addr_.sin_addr.s_addr = inet_addr(ip.c_str());
		addr_.sin_port        = htons(port);
		addr_.sin_family      = AF_INET;
	}

	void set_connected()
	{
		if( not connected_)
		{
			connected_ = true;
			endpoint_->connected();
		}
	}

	void set_disconnected()
	{
		if(connected_)
		{
			connected_ = false;
			endpoint_->disconnected();
		}
		handle_terminated_connection_(this);
	}

	LOCAL_ENDPOINT * endpoint()
	{
		return endpoint_;
	}

	void added()
	{}

	void removed()
	{}

	void process_events(int event_mask)
	{
		if(event_mask & EPOLLHUP)
		{
			set_disconnected();
		}
		else
		{
			if(event_mask & EPOLLIN)
			{
				process_read();
			}
		}
	}

	void process_read()
	{
		if(available_data() == 0)
		{
			set_disconnected();
		}
		else
		{
			while(available_data() != 0)
			{
				process_read_(
					SYS::read_(
						fd_,
						endpoint_->get_buffer(),
						std::min(
							available_data(),
							endpoint_->get_buffer_size())));
			}
		}
	}

	void write(uint8_t const* data, uint32_t size)
	{
		if(connected_)
		{
			write_(SYS::write_(fd_, data, size));
			// if(SYS::write_(fd_, data, size) == -1)
			// {
			// 	set_disconnected();
			// }
		}
	}

	LOCAL_ENDPOINT * endpoint() const
	{
		return endpoint_;
	}

	bool is_connected() const
	{
		return connected_;
	}

	sockaddr_in * addr()
	{
		return &addr_;
	}

protected:
	LOCAL_ENDPOINT                  * endpoint_;
	int                               fd_;
	bool                              connected_;
	sockaddr_in                       addr_;
	std::tr1::function<void(Self_t*)> handle_terminated_connection_;

private:
	void open_(SYS::Result const& result)
	{
		if(result)
		{
			fd_ = result.value;
		}
		else
		{
			throw std::runtime_error(
				std::string(
					"acquire socket fd failed with: ") +
					result.error_description());
		}
	}

	void process_read_(SYS::Result const& result)
	{
		if(result)
		{
			endpoint_->process_read_data(endpoint_->get_buffer(), result.value);
		}
	}

	void write_(SYS::Result const& result)
	{
		if(not result)
		{
			set_disconnected();
		}
	}
};


//----------------------------------------------------------------------------//


template<
	class POLL_INTERFACE,
	class LOCAL_ENDPOINT,
	class SYS = SystemFunctions>
class ActiveSocket : private SYS
{
public:
	ActiveSocket(
		POLL_INTERFACE * poll_interface,
		LOCAL_ENDPOINT * endpoint,
		DurationMs retry_interval,
		std::string const& ip,
 		uint16_t port)
	: retry_interval_(retry_interval)
	, poll_interface_(poll_interface)
	, socket_()
	{
		if(poll_interface_->is_full())
		{
			throw std::runtime_error(
				"ActiveSocket can not be added to poll_interface");
		}

		socket_.set(endpoint);
		socket_.set(ip, port);
		socket_.set(
			std::tr1::bind(
				&Self_t::handle_terminated_connection_,
				this,
				std::tr1::placeholders::_1));
		socket_.open();

		poll_interface_->add(*this);
		connect();
	}

	void added()
	{}

	void removed()
	{}

	void connect()
	{
		std::cout << "ActiveSocket connect" << std::endl;
		if(not socket_.is_connected())
		{
			if(SYS::connect_(
				socket_.get_fd(),
				(sockaddr*) socket_.addr(),
				sizeof(*socket_.addr())) != 0)
			{
				std::cout << "connect_ failed" << std::endl;
				poll_interface_->remove(*this);
				socket_.reopen();
				poll_interface_->add(*this);

				poll_interface_->register_timeout(
					retry_interval_,
					std::tr1::bind(&Self_t::connect, this),
					this);
			}
		}
	}

	int get_fd() const
	{
		return socket_.get_fd();
	}

	void process_events(int event_mask)
	{
		if(event_mask & EPOLLHUP)
		{
			std::cout << "HUP" << std::endl;
			socket_.set_disconnected();
			return;
		}
		else
		{
			if(event_mask & EPOLLIN)
			{
				if(not socket_.is_connected())
				{
					socket_.set_connected();
				}

				socket_.process_read();
			}
			if(event_mask & EPOLLOUT)
			{
				if(not socket_.is_connected())
				{
					socket_.set_connected();
				}
			}
		}
	}


private:
	typedef TcpSocket<LOCAL_ENDPOINT> Socket_t;
	typedef ActiveSocket<POLL_INTERFACE, LOCAL_ENDPOINT, SYS> Self_t;

	DurationMs       retry_interval_;
	POLL_INTERFACE * poll_interface_;
	Socket_t socket_;

	void handle_terminated_connection_(TcpSocket<LOCAL_ENDPOINT> *)
	{
		poll_interface_->register_timeout(
			retry_interval_,
			std::tr1::bind(&Self_t::connect, this),
			this);
	}
};


//----------------------------------------------------------------------------//


template<
	class POLL_INTERFACE,
	class LOCAL_ENDPOINT,
	uint32_t MAX_CONNECTIONS,
	class SYS = SystemFunctions
	>
class PassiveSocket : private SYS
{
public:
	PassiveSocket(
		POLL_INTERFACE * poll_interface,
		std::tr1::function<LOCAL_ENDPOINT *()> connect_callback,
		uint32_t port,
		std::string const& ip = "0.0.0.0",
		DurationMs retry_interval = DurationMs(3000))
	: listening_(false)
	, poll_interface_(poll_interface)
	, connect_callback_(connect_callback)
	, retry_interval_(retry_interval)
	{
		if(poll_interface_->is_full())
		{
			throw std::runtime_error(
				"PassiveSocket can not be added to poll_interface");
		}

		memset(&addr_, 0, sizeof(struct sockaddr_in));
		addr_.sin_addr.s_addr = inet_addr(ip.c_str());
		addr_.sin_port        = htons(port);
		addr_.sin_family      = AF_INET;

		fd_ = socket_(AF_INET, SOCK_STREAM, 0);
		if(fd_ == -1)
		{
			throw std::runtime_error(
				std::string("acquire socket fd failed with: ") + SYS::strerror_());
		}

		int on = 1;
		if(SYS::setsockopt_(fd_, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0)
		{
			throw std::runtime_error(
				std::string("set socket options failed with: ") + SYS::strerror_());
		}

		poll_interface_->add(*this);
	}

	~PassiveSocket()
	{
		removed();
	}

	void close()
	{
		if(fd_ != -1)
		{
			SYS::close_(fd_);
			fd_ = -1;
		}
	}

	int get_fd() const
	{
		return fd_;
	}

	void added()
	{
		if(not listening_)
		{
			process_bind_(
				SYS::bind_(fd_, (struct sockaddr *)&addr_, sizeof(addr_)));
		}
	}

	void removed()
	{
		if(listening_)
		{
			listening_ = false;
		}

		RemoveFunc r(poll_interface_);
		connected_sockets_.for_each(r);
		connected_sockets_.clear();

		poll_interface_->remove(*this);
		close(fd_);
	}

	void process_events(int event_mask)
	{
		if(listening_)
		{
			if(event_mask & EPOLLIN)
			{
				if(not connected_sockets_.is_full() and
				   not poll_interface_->is_full())
				{
					struct sockaddr_in addr;
					socklen_t len = sizeof(addr);
					memset(&addr, 0, len);

					process_accept_(
						SYS::accept_(fd_, (struct sockaddr *) &addr, &len),
						addr);

					// int cfd = SYS::accept_(fd_, (struct sockaddr *) &addr, &len);

					// if(cfd != -1)
					// {
					// 	Socket_t * s = connected_sockets_.add(cfd);
					// 	poll_interface_->add(*s);

					// 	s->set(addr);
					// 	s->set(connect_callback_());
					// 	s->set(
					// 		std::tr1::bind(
					// 			&Self_t::handle_terminated_connection_,
					// 			this,
					// 			std::tr1::placeholders::_1));
					// 	s->set_connected();
					// }
					// else
					// {
					// 	perror("PassiveSocket accept");
					// }
				}
			}
		}
	}

private:
	using SYS::setsockopt_;
	using SYS::bind_;
	using SYS::listen_;
	using SYS::socket_;
	using SYS::accept_;
	using SYS::close_;
	using SYS::strerror_;

	typedef PassiveSocket<
		POLL_INTERFACE,
		LOCAL_ENDPOINT,
		MAX_CONNECTIONS,
		SYS> Self_t;
	typedef TcpSocket<LOCAL_ENDPOINT> Socket_t;

	bool                                    listening_;
	POLL_INTERFACE                        * poll_interface_;
	std::tr1::function<LOCAL_ENDPOINT *()>  connect_callback_;
	int                                     fd_;
	sockaddr_in                             addr_;
	DurationMs                              retry_interval_;
	Socket_t                                socket_;

	List<TcpSocket<LOCAL_ENDPOINT>, MAX_CONNECTIONS>  connected_sockets_;


	struct RemoveFunc
	{
		RemoveFunc(POLL_INTERFACE * poll_interface)
		: poll_interface_(poll_interface)
		{}

		Socket_t * operator()(Socket_t & s)
		{
			poll_interface_->remove(s);
			return NULL;
		}

	private:
		POLL_INTERFACE * poll_interface_;
	};

	friend class RemoveFunc;

	void handle_terminated_connection_(TcpSocket<LOCAL_ENDPOINT> * s)
	{
		poll_interface_->remove(*s);
		connected_sockets_.remove(s);
	}

	void process_bind_(SYS::Result const& result)
	{
		if(not result)
		{
			poll_interface_->register_timeout(
				retry_interval_,
				std::tr1::bind(&Self_t::added, this),
				this);
			return;
		}

		SYS::listen_(fd_, MAX_CONNECTIONS);

		listening_ = true;
	}

	void process_accept_(SYS::Result const& result, sockaddr_in const& addr)
	{
		if(result)
		{
			Socket_t * s = connected_sockets_.add(result.value);
			poll_interface_->add(*s);

			s->set(addr);
			s->set(connect_callback_());
			s->set(
				std::tr1::bind(
					&Self_t::handle_terminated_connection_,
					this,
					std::tr1::placeholders::_1));
			s->set_connected();
		}
		else
		{
			perror("PassiveSocket accept");
		}
	}
};


} //namespace linux_epoll
