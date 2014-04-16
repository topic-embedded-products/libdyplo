/*
 * cooperativescheduler.hpp
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

#include <stdlib.h>
#include <stdexcept>

namespace dyplo
{

	class Process
	{
	public:
		virtual void process_one() = 0;
		virtual void interrupt() = 0;
	};

	class CooperativeScheduler
	{
	public:
		Process* downstream;

		CooperativeScheduler():
			downstream(NULL)
		{
		}

		void process_one()
		{
			unlock();
			downstream->process_one();
			lock();
		}

		void wait_until_not_full()
		{
			process_one();
		}
		void wait_until_not_empty()
		{
			throw std::runtime_error("Not implemented: wait_until_not_empty");
		}
		void trigger_not_full() const
		{
		}
		void trigger_not_empty()
		{
			process_one();
		}

		void lock() const
		{
		}

		void unlock() const
		{
		}

		void interrupt_not_full()
		{
			if (downstream)
				downstream->interrupt();
		}

		void interrupt_not_empty()
		{
		}

		void resume_not_full()
		{
			if (downstream)
				process_one();
		}

		void resume_not_empty()
		{
		}
	};

}
