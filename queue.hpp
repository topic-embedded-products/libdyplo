/*
 * queue.hpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. (http://www.topic.nl).
 * All rights reserved.
 *
 * This file is part of libdyplo.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA or see <http://www.gnu.org/licenses/>.
 *
 * You can contact Topic by electronic mail via info@topic.nl or via
 * paper mail at the following address: Postbus 440, 5680 AK Best, The Netherlands.
 */
#pragma once

#include <stdexcept>
#include "generics.hpp"
#include "scopedlock.hpp"

namespace dyplo
{
	template <class T, class Scheduler> class FixedMemoryQueueImpl
	{
	public:
		typedef T Element;

		FixedMemoryQueueImpl(T* buffer, unsigned int capacity, const Scheduler& scheduler = Scheduler()):
			m_scheduler(scheduler),
			m_buff(buffer),
			m_end(m_buff + capacity),
			m_first(m_buff),
			m_last(m_buff),
			m_size(0)
		{}

		void clear()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_first = m_buff;
			m_last = m_buff;
			m_size = 0;
		}

		/* Return pointer to memory of "count" elements. Will block
		* if no room in buffer for count_min items */
		unsigned int begin_write(T* &buffer, unsigned int count_min)
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			wait_until_not_full(count_min);
			buffer = m_first;
			unsigned int result;
			if (m_last > m_first)
				result = m_last - m_first;
			else if (m_last == m_first) /* Can mean "full" or "empty", size determines which it is */
				result = (m_size == 0) ? (m_end - m_first) : 0;
			else
				result = m_end - m_first;
			return result;
		}

		/* Informs queue that data as issued by begin_write is
		* now valid and can be processed by the next node. count
		* may be less than previously requested. */
		void end_write(unsigned int count)
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_first = increment(m_first, count);
			m_size += count;
			m_scheduler.trigger_not_empty();
		}

		/* Return pointer to memory of count bytes. Blocks if
		* no data available (someone must call end_write) */
		unsigned int begin_read(T* &buffer, unsigned int count_min)
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			wait_until_not_empty(count_min);
			buffer = m_last;
			unsigned int result;
			if (m_first > m_last)
				result =  m_first - m_last;
			else if (m_size)
				result = m_end - m_last;
			else
				result = 0;
			return result;
		}

		/* Notify queue that count bytes have been consumed and
		* that the buffer can be re-used for incoming data */
		void end_read(unsigned int count)
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_last = increment(m_last, count);
			DEBUG_ASSERT(m_size >= count, "invalid end_read");
			m_size -= count;
			m_scheduler.trigger_not_full();
		}

		void wait_empty()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			for(;;)
			{
				if (empty())
					return;
				m_scheduler.wait_until_not_full();
			}
		}

		unsigned int capacity() const { return m_end - m_buff; }
		unsigned int size() const { return m_size; }
		unsigned int available() const { return capacity() - size(); }
		bool empty() const { return size() == 0; }
		bool full() const { return size() == capacity(); }

		void push_one(const T data)
		{
			T* buffer;
			begin_write(buffer, 1);
			*buffer = data;
			end_write(1);
		}

		T pop_one()
		{
			T* buffer;
			begin_read(buffer, 1);
			T result = *buffer;
			end_read(1);
			return result;
		}

		void interrupt_read()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_scheduler.interrupt_not_empty();
		}

		void interrupt_write()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_scheduler.interrupt_not_full();
		}

		void resume_read()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_scheduler.resume_not_empty();
		}

		void resume_write()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_scheduler.resume_not_full();
		}

		Scheduler& get_scheduler() { return m_scheduler; }
		const Scheduler& get_scheduler() const { return m_scheduler; }
	protected:
		void wait_until_not_full(unsigned int count)
		{
			while (available() < count)
				m_scheduler.wait_until_not_full();
		}

		void wait_until_not_empty(unsigned int count)
		{
			while (size() < count)
				m_scheduler.wait_until_not_empty();
		}


		T* increment(T* what, unsigned int count)
		{
			T* result = what + count;
			DEBUG_ASSERT(what >= m_buff, "bad pointer");
			DEBUG_ASSERT(result <= m_end, "bad increment");
			if (result  == m_end)
				return m_buff;
			return result;
		}

		Scheduler m_scheduler;
		T* m_buff;
		T* m_end;
		T* m_first;
		T* m_last;
		unsigned int m_size;
	};

	/* Generic case where "new" and "delete" are being used to
	 * create the buffer */
	template <class T, class Scheduler> class FixedMemoryQueue:
		public FixedMemoryQueueImpl<T, Scheduler>
	{
	public:
		FixedMemoryQueue(unsigned int capacity, const Scheduler& scheduler = Scheduler()):
			FixedMemoryQueueImpl<T, Scheduler>(new T[capacity], capacity, scheduler)
		{
		}
		~FixedMemoryQueue()
		{
			delete [] FixedMemoryQueueImpl<T, Scheduler>::m_buff;
		}
	};

	/* A specialized queue that can only hold one element. */
	template <class T, class Scheduler> class SingleElementQueue
	{
	public:
		typedef T Element;

		SingleElementQueue(const Scheduler& scheduler = Scheduler()):
			m_scheduler(scheduler),
			m_full(false)
		{}

		void clear()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_full = false;
		}

		/* Return pointer to memory of "count" elements. Will block
		* if no room in buffer for count_min items */
		unsigned int begin_write(T* &buffer, unsigned int count_min)
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			if (count_min)
				wait_until_not_full();
			else if (m_full)
				return 0; /* no room */
			buffer = &m_buffer;
			return 1;
		}

		/* Informs queue that data as issued by begin_write is
		* now valid and can be processed by the next node. count
		* may be less than previously requested. */
		void end_write(unsigned int count)
		{
			DEBUG_ASSERT(count <= 1, "count must be <= 1");
			if (count)
			{
				ScopedLock<Scheduler> lock(m_scheduler);
				m_full = true;
				m_scheduler.trigger_not_empty();
			}
		}

		/* Return pointer to memory of count bytes. Blocks if
		* no data available (someone must call end_write) */
		unsigned int begin_read(T* &buffer, unsigned int count_min)
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			if (count_min)
				wait_until_not_empty();
			else if (!m_full)
				return 0;
			buffer = &m_buffer;
			return 1;
		}

		/* Notify queue that count bytes have been consumed and
		* that the buffer can be re-used for incoming data */
		void end_read(unsigned int count)
		{
			DEBUG_ASSERT(count <= 1, "count must be <= 1");
			if (count)
			{
				ScopedLock<Scheduler> lock(m_scheduler);
				m_full = false;
				m_scheduler.trigger_not_full();
			}
		}

		void wait_empty()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			wait_until_not_full();
		}

		unsigned int capacity() const { return 1; }
		unsigned int size() const { return m_full ? 1 : 0; }
		unsigned int available() const { return 1 - size(); }
		bool empty() const { return !m_full; }
		bool full() const { return m_full; }

		void push_one(const T data)
		{
			T* buffer;
			begin_write(buffer, 1);
			*buffer = data;
			end_write(1);
		}

		T pop_one()
		{
			T* buffer;
			begin_read(buffer, 1);
			T result = *buffer;
			end_read(1);
			return result;
		}

		void interrupt_read()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_scheduler.interrupt_not_empty();
		}

		void interrupt_write()
		{
			ScopedLock<Scheduler> lock(m_scheduler);
			m_scheduler.interrupt_not_full();
		}

		Scheduler& get_scheduler() { return m_scheduler; }
		const Scheduler& get_scheduler() const { return m_scheduler; }
	protected:
		void wait_until_not_full()
		{
			while (m_full)
				m_scheduler.wait_until_not_full();
		}

		void wait_until_not_empty()
		{
			while (!m_full)
				m_scheduler.wait_until_not_empty();
		}

		Scheduler m_scheduler;
		T m_buffer;
		bool m_full;
	};
}
