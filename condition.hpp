/*
 * condition.hpp
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
		int timedwait(pthread_mutex_t* mutex, const struct timespec* abstime)
		{
			return pthread_cond_timedwait(&m_handle, mutex, abstime);
		}
	};
}
