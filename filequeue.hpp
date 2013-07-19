#pragma once

#include <unistd.h>
#include <stdexcept>
#include "generics.hpp"
#include "scopedlock.hpp"

namespace dyplo
{
	template <class T> class FileOutputQueue
	{
	public:
		typedef T Element;

		FileOutputQueue(int file_handle, unsigned int capacity):
			m_buff(new T[capacity]),
			m_capacity(capacity),
			m_file_handle(file_handle)
		{
		}
		~FileOutputQueue()
		{
			delete [] m_buff;
		}

		/* Return pointer to memory of "count" elements. Will block
		* if no room in buffer for count_min items */
		unsigned int begin_write(T* &buffer, unsigned int count_min)
		{
			buffer = m_buff;
			return m_capacity;
		}

		/* Informs queue that data as issued by begin_write is
		* now valid and can be processed by the next node. count
		* may be less than previously requested. */
		void end_write(unsigned int count)
		{
			int result = ::write(m_file_handle, m_buff, count * sizeof(T));
			if (result < 0)
				throw std::runtime_error("Failed to write to file");
			if (result != count * sizeof(T))
				throw std::runtime_error("Write returned something else");
		}


		void push_one(const T data)
		{
			T* buffer;
			begin_write(buffer, 1);
			*buffer = data;
			end_write(1);
		}

	protected:
		T* m_buff;
		unsigned int m_capacity;
		int m_file_handle;
	};

	template <class T> class FileInputQueue
	{
	public:
		typedef T Element;

		FileInputQueue(int file_handle, unsigned int capacity):
			m_buff(new T[capacity]),
			m_capacity(capacity),
			m_carry(0),
			m_size(0),
			m_file_handle(file_handle)
		{
		}
		~FileInputQueue()
		{
			delete [] m_buff;
		}

		unsigned int begin_read(T* &buffer, unsigned int count_min)
		{
			buffer = m_buff;
			if (m_carry)
			{
				if (m_carry >= count_min)
					return m_carry;
				buffer += m_carry;
				count_min -= m_carry;
			}
			int result = ::read(m_file_handle, buffer, count_min * sizeof(T));
			if (result < 0)
				throw std::runtime_error("Failed to read file");
			m_size = m_carry + (result / sizeof(T));
			buffer = m_buff;
			return m_size;
		}

		void end_read(unsigned int count)
		{
			m_carry = m_size - count;
			if (m_carry && count)
			{
				memcpy(m_buff, m_buff + count, m_carry * sizeof(T));
			}
		}

		T pop_one()
		{
			T* buffer;
			begin_read(buffer, 1);
			T result = *buffer;
			end_read(1);
			return result;
		}

	protected:
		T* m_buff;
		unsigned int m_capacity;
		unsigned int m_carry;
		unsigned int m_size;
		int m_file_handle;
	};
}
