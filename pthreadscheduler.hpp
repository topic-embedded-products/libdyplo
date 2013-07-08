#pragma once

#include <pthread.h>
#include <stdexcept>

namespace hmu
{
	class InterruptedException: public std::exception
	{
		virtual const char* what() const throw()
		{
			return "Interrupted";
		}
	};

	class Mutex
	{
		pthread_mutex_t m_handle;
	public:
		Mutex()
		{
			int result = pthread_mutex_init(&m_handle, NULL);
			if (result != 0) throw std::runtime_error("Failed to create mutex");
		}
		~Mutex()
		{
			pthread_mutex_destroy(&m_handle);
		}
		void lock()
		{
			pthread_mutex_lock(&m_handle);
		}
		void unlock()
		{
			pthread_mutex_unlock(&m_handle);
		}
		operator pthread_mutex_t*()
		{
			return &m_handle;
		}
	};

	class Condition
	{
		pthread_cond_t m_handle;
	public:
		Condition()
		{
			int result = pthread_cond_init(&m_handle, NULL);
			if (result != 0) throw std::runtime_error("Failed to create condition");
		}
		~Condition()
		{
			pthread_cond_destroy(&m_handle);
		}
		void signal()
		{
			pthread_cond_signal(&m_handle);
		}
		void wait(pthread_mutex_t* mutex)
		{
			pthread_cond_wait(&m_handle, mutex);
		}
	};

	class PthreadScheduler
	{
	protected:
		Mutex m_mutex;
		Condition m_condition_not_full;
		Condition m_condition_not_empty;
		bool m_interrupted;
	public:
		PthreadScheduler():
			m_interrupted(false)
		{
		}

		/* wait_ and trigger_ methods are to be called with the lock held */
		void wait_until_not_full()
		{
			if (m_interrupted)
				throw InterruptedException();
			m_condition_not_full.wait(m_mutex);
		}
		void wait_until_not_empty()
		{
			if (m_interrupted)
				throw InterruptedException();
			m_condition_not_empty.wait(m_mutex);
		}
		void trigger_not_full()
		{
			m_condition_not_full.signal();
		}
		void trigger_not_empty()
		{
			m_condition_not_empty.signal();
		}

		/* Having empty lock/unlock methods will cause the whole locking to be optimized away */
		void lock()
		{
			m_mutex.lock();
		}
		void unlock()
		{
			m_mutex.unlock();
		}

		void interrupt()
		{
			lock();
			m_interrupted = true;
			m_condition_not_full.signal();
			m_condition_not_empty.signal();
			unlock();
		}
	};

}