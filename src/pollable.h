#pragma once
#include <memory>

namespace linux_epoll
{



class Pollable
{
public:
	template<class T>
	Pollable(T & t)
	{
		new(&buffer_[0]) Model<T>(t);
	}

	void added()
	{
		reinterpret_cast<BaseModel*>(buffer_)->added();
	}

	void removed()
	{
		reinterpret_cast<BaseModel*>(buffer_)->removed();
	}

	void process_events(int event_mask)
	{
		reinterpret_cast<BaseModel*>(buffer_)->process_events(event_mask);
	}

	int get_fd() const
	{
		return reinterpret_cast<BaseModel const*>(buffer_)->get_fd();
	}

private:
	struct BaseModel
	{
		virtual ~BaseModel() {}
		virtual void added() = 0;
		virtual void removed() = 0;
		virtual void process_events(int) = 0;
		virtual int get_fd() const = 0;
	};

	template<class T>
	struct Model : public BaseModel
	{
		Model(T & t_)
		: t(t_)
		{}

		void added()
		{
			t.added();
		}

		void removed()
		{
			t.removed();
		}

		void process_events(int event_mask)
		{
			t.process_events(event_mask);
		}

		int get_fd() const
		{
			return t.get_fd();
		}

	private:
		T & t;
	};

	uint8_t buffer_[16];
};


} //namespace linux_epoll
