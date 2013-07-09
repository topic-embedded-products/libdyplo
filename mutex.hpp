#pragma once

#include <pthread.h>

namespace dyplo
{
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
}