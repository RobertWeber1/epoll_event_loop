#pragma once

#include <stdint.h>
#include <string.h>
#include <cstdlib>

namespace linux_epoll
{


template<class T, uint32_t SIZE>
class List
{
public:
	List()
	{
		elements_ = reinterpret_cast<T*>(data);
		for(uint32_t i=0; i<SIZE; ++i)
		{
			valid_[i] = false;
		}
	}

	~List()
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			remove_(i);
		}
	}

	bool is_empty() const
	{
		return count() == 0;
	}

	bool is_full() const
	{
		return count() == SIZE;
	}

	uint32_t count() const
	{
		uint32_t result = 0;

		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(valid_[i])
			{
				++result;
			}
		}

		return result;
	}

	T * add()
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(not valid_[i])
			{
				valid_[i] = true;
				return new (&elements_[i]) T();
			}
		}

		return NULL;
	}

	template<class ARG>
	T * add(ARG & arg)
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(not valid_[i])
			{
				valid_[i] = true;
				return new (&elements_[i]) T(arg);
			}
		}

		return NULL;
	}

	template<class ARG>
	T * add(ARG const& arg)
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(not valid_[i])
			{
				valid_[i] = true;
				return new (&elements_[i]) T(arg);
			}
		}

		return NULL;
	}

	template<class ARG1, class ARG2>
	T * add(ARG1 & arg1, ARG2 & arg2)
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(not valid_[i])
			{
				valid_[i] = true;
				return new (&elements_[i]) T(arg1, arg2);
			}
		}

		return NULL;
	}

	void remove(T * element)
	{
		if(is_in_range_(element))
		{
			remove_(index_of_(element));
		}
	}

	template<class PRED>
	T * find_if(PRED const& pred)
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(valid_[i])
			{
				if(pred(elements_[i]))
				{
					return &elements_[i];
				}
			}
		}
		return NULL;
	}

	template<class PRED>
	void remove_if(PRED const& pred)
	{
		remove( find_if(pred) );
	}

	template<class FUNC>
	void for_each(FUNC & func)
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(valid_[i])
			{
				func(elements_[i]);
			}
		}
	}

	void clear()
	{
		for(uint32_t i=0; i<SIZE; ++i)
		{
			if(valid_[i])
			{
				remove_(i);
			}
		}
	}

private:
	bool valid_[SIZE];
	char data[sizeof(T)*SIZE];
	T * elements_;

	bool is_in_range_(T * element) const
	{
		return (
			element and
			(element >= elements_) and
			(element < (elements_ + SIZE)) );
	}

	uint32_t index_of_(T * element) const
	{
		return (element - elements_);
	}

	void remove_(uint32_t index)
	{
		if(valid_[index])
		{
			elements_[index].~T();
			valid_[index] = false;
		}
	}
};


} //namespace linux_epoll
