#pragma once

#include <stdexcept>

#define DEBUG_ASSERT(x, e) if (!(x)) throw std::runtime_error(e);

namespace hmu
{

class Process
{
public:
	virtual void process_one() = 0;
};

class NoopScheduler
{
public:
	/* wait_ and trigger_ methods are to be called with the lock held */
	void wait_until_not_full()
	{
		throw std::runtime_error("Not implemented: wait_until_not_full");
	}
	void wait_until_not_empty()
	{
		throw std::runtime_error("Not implemented: wait_until_not_empty");
	}
	void trigger_not_full()
	{
	}
	void trigger_not_empty()
	{
	}

	/* Having empty lock/unlock methods will cause the whole locking to be optimized away */
	void lock()
	{
	}
	void unlock()
	{
	}
};

template <class Lockable> class ScopedLock
{
	Lockable& m_lock;
public:
	ScopedLock(Lockable& lock):
		m_lock(lock)
	{
		lock.lock();
	}

	~ScopedLock()
	{
		m_lock.unlock();
	}
};


template <class T, class Scheduler> class FixedMemoryQueue //: public IQueue<T>
{
public:
	typedef T Element;

	FixedMemoryQueue(unsigned int capacity, const Scheduler& scheduler = Scheduler()):
		m_scheduler(scheduler),
		m_buff((T*)calloc(capacity, sizeof(T))),
		m_end(m_buff + capacity),
		m_first(m_buff),
		m_last(m_buff),
		m_size(0)
	{}
	~FixedMemoryQueue()
	{
		free(m_buff);
	}

	/* Return pointer to memory of "count" bytes. Will block
	 * if no room in buffer */
	T* begin_write(unsigned int count)
	{
		ScopedLock<Scheduler> lock(m_scheduler);
		wait_until_not_full();
		DEBUG_ASSERT(!full(), "write when full");
		return m_first;
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
	const T* begin_read(unsigned int count)
	{
		ScopedLock<Scheduler> lock(m_scheduler);
		wait_until_not_empty();
		DEBUG_ASSERT(!empty(), "read when empty");
		return m_last;
	}

	/* Notify queue that count bytes have been consumed and
	 * that the buffer can be re-used for incoming data */
	void end_read(unsigned int count)
	{
		ScopedLock<Scheduler> lock(m_scheduler);
		m_last = increment(m_last, count);
		m_size -= count;
		m_scheduler.trigger_not_full();
	}

	unsigned int capacity() const { return m_end - m_buff; }
	unsigned int size() const { return m_size; }
	bool empty() const { return size() == 0; }
	bool full() const { return size() == capacity(); }

	Scheduler& get_scheduler() { return m_scheduler; }
	const Scheduler& get_scheduler() const { return m_scheduler; }
protected:
	void wait_until_not_full()
	{
		while (full())
			m_scheduler.wait_until_not_full();
	}

	void wait_until_not_empty()
	{
		while (empty())
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

}