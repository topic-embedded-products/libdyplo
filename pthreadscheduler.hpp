/*
 * pthreadscheduler.hpp
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
#pragma once

#include "mutex.hpp"
#include "condition.hpp"
#include "exceptions.hpp"

namespace dyplo
{
	class PthreadScheduler
	{
	protected:
		Mutex m_mutex;
		Condition m_condition_not_full;
		Condition m_condition_not_empty;
		bool m_interrupted_not_full;
		bool m_interrupted_not_empty;
	public:
		PthreadScheduler();

		/* wait_ and trigger_ methods are to be called with the lock held */
		void wait_until_not_full();
		void wait_until_not_empty();
		void trigger_not_full();
		void trigger_not_empty();

		void lock();
		void unlock();

		/* interrupt and resume are to be called with the lock held */
		void interrupt_not_full();
		void interrupt_not_empty();
		void resume_not_full();
		void resume_not_empty();
	};
}
