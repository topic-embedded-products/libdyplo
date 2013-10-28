#pragma once

#include <iostream>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "generics.hpp"
#include "exceptions.hpp"
#include "scopedlock.hpp"
#include "noopscheduler.hpp"

namespace dyplo
{
	int set_non_blocking(int file_handle);
	
	struct Pipe
	{
		int m_handles[2];
		Pipe()
		{
			int result = ::pipe(m_handles);
			if (result != 0)
				throw std::runtime_error("Failed to create pipe");
		}
		~Pipe()
		{
			::close(m_handles[0]);
			::close(m_handles[1]);
		}
		int read_handle() const { return m_handles[0]; }
		int write_handle() const { return m_handles[1]; }
	};

	class FilePollScheduler
	{
		protected:
			Pipe m_internal_pipe;
			bool m_interrupted;
		public:
			FilePollScheduler();
			void wait_readable(int filehandle);
			void wait_writeable(int filehandle);
			void interrupt();
			void reset();
	};

	template <class T> class FileOutputQueue
	{
	public:
		typedef T Element;

		FileOutputQueue(FilePollScheduler& scheduler, int file_handle, unsigned int capacity):
			m_buff(new T[capacity]),
			m_capacity(capacity),
			m_file_handle(file_handle),
			m_writeout_position((char*)m_buff),
			m_bytes_to_write(0),
			m_scheduler(scheduler)
		{
			if (set_non_blocking(file_handle) != 0)
				throw std::runtime_error("Failed to set non-blocking mode");
		}
		~FileOutputQueue()
		{
			delete [] m_buff;
		}
		
		/* Returns whether caller needs to wait for more */
		bool write_buffer()
		{
			while (m_bytes_to_write > 0)
			{
				int result = ::write(m_file_handle, m_writeout_position, m_bytes_to_write);
				if (result < 0)
				{
					if (errno == EAGAIN)
						return true;
					else
						throw std::runtime_error("Failed to write to file");
				}
				else
				{
					m_writeout_position += result;
					m_bytes_to_write -= result;
				}
			}
			m_writeout_position = (char*)m_buff;
			return false;
		}

		/* Return pointer to memory of "count" elements. Will block
		* if no room in buffer for count_min items */
		unsigned int begin_write(T* &buffer, unsigned int count_min)
		{
			if (count_min == 0)
			{
				/* Don't block, just poll and return */
				if (write_buffer())
					return 0;
			}
			/* Call the scheduler first, to make sure there's plenty
			 * room in the buffer before attempting to write. */
			do
			{
				m_scheduler.wait_writeable(m_file_handle);
			}
			while (write_buffer());
			buffer = m_buff;
			return m_capacity;
		}

		/* Informs queue that data as issued by begin_write is
		* now valid and can be processed by the next node. count
		* may be less than previously requested. */
		void end_write(unsigned int count)
		{
			m_bytes_to_write = count * sizeof(T);
			m_writeout_position = (char*)m_buff;
			write_buffer();
		}

		void push_one(const T data)
		{
			T* buffer;
			begin_write(buffer, 1);
			*buffer = data;
			end_write(1);
		}

		void interrupt_write()
		{
			m_scheduler.interrupt();
		}
		FilePollScheduler& get_scheduler() { return m_scheduler; }
	protected:
		T* m_buff;
		unsigned int m_capacity;
		int m_file_handle;
		char* m_writeout_position;
		int m_bytes_to_write;
		FilePollScheduler& m_scheduler;
	};

	template <class T> class FileInputQueue
	{
	public:
		typedef T Element;

		FileInputQueue(FilePollScheduler& scheduler, int file_handle, unsigned int capacity):
			m_buff(new T[capacity]),
			m_capacity(capacity),
			m_carry(0),
			m_size(0),
			m_file_handle(file_handle),
			m_scheduler(scheduler)
		{
			if (set_non_blocking(file_handle) != 0)
				throw std::runtime_error("Failed to set non-blocking mode");
		}
		~FileInputQueue()
		{
			delete [] m_buff;
		}

		unsigned int begin_read(T* &buffer, unsigned int count_min)
		{
			char* buffer_position;
			buffer = m_buff;
			if (m_carry)
			{
				if (m_carry >= count_min)
					return m_carry;
				buffer_position = (char*)(m_buff + m_carry);
				count_min -= m_carry;
			}
			else
			{
				 buffer_position = (char*)buffer;
			}
			int bytes_to_read = count_min * sizeof(T);
			if (bytes_to_read) /* Blocking? poll first */
				m_scheduler.wait_readable(m_file_handle);
			int bytes_read = 0;
			do
			{
				int buffer_space_available = (m_capacity * sizeof(T)) - (buffer_position - (char*)m_buff);
				int result = ::read(m_file_handle, buffer_position, buffer_space_available);
				if (result < 0)
				{
					if (errno == EAGAIN)
					{
						m_scheduler.wait_readable(m_file_handle);
					}
					else
						throw std::runtime_error("Failed to read file");
				}
				else if (result == 0)
					throw EndOfInputException();
				else
				{
					bytes_to_read -= result;
					buffer_position += result;
					bytes_read += result;
				}
			}
			while (bytes_to_read > 0);
			m_size = m_carry + (bytes_read / sizeof(T));
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
		
		void interrupt_read()
		{
			m_scheduler.interrupt();
		}
		FilePollScheduler& get_scheduler() { return m_scheduler; }

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
		FilePollScheduler& m_scheduler;
	};
}
