/*
 * thread.hpp
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
