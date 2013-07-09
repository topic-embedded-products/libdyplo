#pragma once

#include <pthread.h>

namespace dyplo
{
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
}