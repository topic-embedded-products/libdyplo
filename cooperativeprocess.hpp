/*
 * cooperativeprocess.hpp
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

#include "queue.hpp"
#include "cooperativescheduler.hpp"

namespace dyplo
{
	template <
		class InputQueueClass,
		class OutputQueueClass>
	class CooperativeProcessBase: public Process
	{
	protected:
		InputQueueClass *input;
		OutputQueueClass *output;

	public:
		CooperativeProcessBase():
			input(NULL),
			output(NULL)
		{}

		~CooperativeProcessBase()
		{
			/* Inform downstream and upstream elements that we
			 * will terminate */
			if (input != NULL)
				input->interrupt_read();
			if (output != NULL)
				output->interrupt_write();
		}

		void set_input(InputQueueClass *value)
		{
			input = value;
			input->get_scheduler().downstream = this;
		}

		void set_output(OutputQueueClass *value)
		{
			output = value;
		}

		/* override */ void interrupt()
		{
			if (output != NULL)
				output->interrupt_write();
		}
	};


	template <
		class InputQueueClass,
		class OutputQueueClass,
		void(*ProcessBlockFunction)(typename OutputQueueClass::Element*, typename InputQueueClass::Element*),
		int blocksize = 1>
	class CooperativeProcess: public CooperativeProcessBase<InputQueueClass, OutputQueueClass>
	{
	public:
		typedef CooperativeProcessBase<InputQueueClass, OutputQueueClass> Base;
		/* override */ void process_one()
		{
			typename InputQueueClass::Element *src;
			typename OutputQueueClass::Element *dest;
			for (;;)
			{
				unsigned int count = Base::input->begin_read(src, 0);
				if (count < blocksize)
					return;
				Base::output->begin_write(dest, blocksize);
				ProcessBlockFunction(dest, src);
				Base::output->end_write(blocksize);
				Base::input->end_read(blocksize);
			}
		}

		~CooperativeProcess()
		{
			Base::interrupt();
		}
	};
}
