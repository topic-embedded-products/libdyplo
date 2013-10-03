#include "pthreadscheduler.hpp"

namespace dyplo
{
	const char* InterruptedException::what() const throw()
	{
		return "Interrupted";
	}

	PthreadScheduler::PthreadScheduler():
		m_interrupted_not_full(false),
		m_interrupted_not_empty(false)
	{
	}

	void PthreadScheduler::wait_until_not_full()
	{
		if (m_interrupted_not_full)
			throw InterruptedException();
		m_condition_not_full.wait(m_mutex);
	}
	void PthreadScheduler::wait_until_not_empty()
	{
		if (m_interrupted_not_empty)
			throw InterruptedException();
		m_condition_not_empty.wait(m_mutex);
	}

	void PthreadScheduler::trigger_not_full()
	{
		m_condition_not_full.signal();
	}
	void PthreadScheduler::trigger_not_empty()
	{
		m_condition_not_empty.signal();
	}

	void PthreadScheduler::lock()
	{
		m_mutex.lock();
	}
	void PthreadScheduler::unlock()
	{
		m_mutex.unlock();
	}

	void PthreadScheduler::interrupt_not_full()
	{
		m_interrupted_not_full = true;
		m_condition_not_full.signal();
	}

	void PthreadScheduler::interrupt_not_empty()
	{
		m_interrupted_not_empty = true;
		m_condition_not_empty.signal();
	}

	void PthreadScheduler::reset()
	{
		m_interrupted_not_full = false;
		m_interrupted_not_empty = false;
	}
}
