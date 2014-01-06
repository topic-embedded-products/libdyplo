/*
 * pthreadscheduler.cpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. <Mike Looijmans> (http://www.topic.nl).
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

	void PthreadScheduler::resume_not_full()
	{
		m_interrupted_not_full = false;
		m_condition_not_full.signal(); /* In case the queue changed state */
	}

	void PthreadScheduler::resume_not_empty()
	{
		m_interrupted_not_empty = false;
		m_condition_not_empty.signal(); /* In case the queue changed state */
	}
}
