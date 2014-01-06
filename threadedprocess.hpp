/*
 * threadedprocess.hpp
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

#include "pthreadscheduler.hpp"
#include "queue.hpp"
#include "thread.hpp"

namespace dyplo
{
	template <class InputQueueClass, class OutputQueueClass,	class Processor>
	class ThreadedProcessBase
	{
	protected:
		InputQueueClass *input;
		OutputQueueClass *output;
		Processor m_processor;
		Thread m_thread;
	public:
		typedef typename InputQueueClass::Element InputElement;
		typedef typename OutputQueueClass::Element OutputElement;

		ThreadedProcessBase():
			input(NULL),
			output(NULL),
			m_thread()
		{
		}

		/* Create using copy-constructor */
		ThreadedProcessBase(const Processor& processor):
			input(NULL),
			output(NULL),
			m_thread(),
			m_processor(processor)
		{
		}

		virtual ~ThreadedProcessBase()
		{
			terminate();
		}

		/* May be called from destructor in derived classes */
		void terminate()
		{
			/* Thread is only active when both input and output are set.
			 * No need to interrupt when either is NULL */
			if ((input != NULL) && (output != NULL))
			{
				input->interrupt_read();
				output->interrupt_write();
				m_thread.join();
			}
			input = NULL;
			output = NULL;
		}

		void set_input(InputQueueClass *value)
		{
			input = value;
			if (input && output)
				start();
		}

		void set_output(OutputQueueClass *value)
		{
			output = value;
			if (input && output)
				start();
		}
		
		void process()
		{
			try
			{
				m_processor.process(input, output);
			}
			catch (const dyplo::InterruptedException&)
			{
				// no action
			}
		}

	private:
		void start()
		{
			m_thread.start(&run, this);
		}

		static void* run(void* arg)
		{
			((ThreadedProcessBase*)arg)->process();
			return 0;
		}
	};
	
	template <class InputQueueClass, class OutputQueueClass,
		void(*ProcessBlockFunction)(typename OutputQueueClass::Element*, typename InputQueueClass::Element*),
		int blocksize = 1
	> class StaticProcessor
	{
	public:
		typedef typename InputQueueClass::Element InputElement;
		typedef typename OutputQueueClass::Element OutputElement;

		void process(InputQueueClass* input, OutputQueueClass* output)
		{
			InputElement *src;
			OutputElement *dest;
			for(;;)
			{
				unsigned int count = input->begin_read(src, blocksize);
				DEBUG_ASSERT(count >= blocksize, "invalid value from begin_read");
				count = output->begin_write(dest, blocksize);
				DEBUG_ASSERT(count >= blocksize, "invalid value from begin_write");
				ProcessBlockFunction(dest, src);
				output->end_write(blocksize);
				input->end_read(blocksize);
			}
		}
	};
	
	
	template <class InputQueueClass, class OutputQueueClass,
		void(*ProcessBlockFunction)(typename OutputQueueClass::Element*, typename InputQueueClass::Element*),
		int blocksize = 1>
	class ThreadedProcess:
		public ThreadedProcessBase<InputQueueClass, OutputQueueClass,
			StaticProcessor<InputQueueClass, OutputQueueClass, ProcessBlockFunction, blocksize>
		>
	{
	};

}
