#pragma once

#include <pthread.h>

namespace dyplo
{
	class Thread
	{
	protected:
		pthread_t m_thread;
	public:
		/* Use a small thread size for Dyplo-managed threads. The system
		 * default is usually 8MB, resulting in a high load on VM */
		static const int default_stack_size = 128 * 1024;
	
		Thread():
			m_thread(pthread_self())
		{
		}

		~Thread()
		{
			pthread_join(m_thread, NULL); /* Fails with EDEADLK if already joined */
		}

		int start(void *(*start_routine) (void *), void *arg)
		{
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setstacksize(&attr, default_stack_size);
			return pthread_create(&m_thread, &attr, start_routine, arg);
		}

		int join(void **retval = NULL)
		{
			int result = pthread_join(m_thread, retval);
			if (result == 0)
				m_thread = pthread_self(); /* Prevents joining/detaching it again */
			return result;
		}

		int detach()
		{
			int result = pthread_detach(m_thread);
			if (result == 0)
				m_thread = pthread_self(); /* Prevents joining/detaching it again */
			return result;
		}

		operator pthread_t() const
		{
			return m_thread;
		}
	};
}
