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


// template <class T> class IQueue
// {
// public:
// 	/* Return pointer to memory of "count" bytes. Will block
// 	 * if no room in buffer */
// 	virtual T* begin_write(unsigned int count) = 0;
// 	/* Informs queue that data as issued by begin_write is
// 	 * now valid and can be processed by the next node. count
// 	 * may be less than previously requested. */
// 	virtual void end_write(unsigned int count) = 0;
// 	/* Return pointer to memory of count bytes. Blocks if
// 	 * no data available (someone must call end_write) */
// 	virtual const T* begin_read(unsigned int count) = 0;
// 	/* Notify queue that count bytes have been consumed and
// 	 * that the buffer can be re-used for incoming data */
// 	virtual void end_read(unsigned int count) = 0;
// };

class NoopScheduler
{
public:
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

	T* begin_write(unsigned int count)
	{
		wait_until_not_full();
		DEBUG_ASSERT(!full(), "write when full");
		return m_first;
	}

	void end_write(unsigned int count)
	{
		m_first = increment(m_first, count);
		m_size += count;
		m_scheduler.trigger_not_empty();
	}

	const T* begin_read(unsigned int count)
	{
		wait_until_not_empty();
		DEBUG_ASSERT(!empty(), "read when empty");
		return m_last;
	}

	void end_read(unsigned int count)
	{
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