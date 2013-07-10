#pragma once

#include "pthreadscheduler.hpp"
#include "queue.hpp"
#include "thread.hpp"

namespace dyplo
{
	template <class InputQueueClass, class OutputQueueClass,
		void(*ProcessBlockFunction)(typename InputQueueClass::Element*, typename OutputQueueClass::Element*),
		int blocksize = 1>
	class ThreadedProcess
	{
	protected:
		InputQueueClass *input;
		OutputQueueClass *output;
		Thread m_thread;
	public:
		typedef typename InputQueueClass::Element InputElement;
		typedef typename OutputQueueClass::Element OutputElement;

		ThreadedProcess():
			input(NULL),
			output(NULL),
			m_thread()
		{
		}

		~ThreadedProcess()
		{
			if (input != NULL)
				input->get_scheduler().interrupt();
			if (output != NULL)
				output->get_scheduler().interrupt();
			m_thread.join();
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

		void set_process_block_function(void (*routine) (OutputElement *dest, InputElement *src))
		{
		}

		void* process()
		{
			unsigned int count;
			InputElement *src;
			OutputElement *dest;

			try
			{
				for(;;)
				{
					count = input->begin_read(src, blocksize);
					DEBUG_ASSERT(count >= blocksize, "invalid value from begin_read");
					count = output->begin_write(dest, blocksize);
					DEBUG_ASSERT(count >= blocksize, "invalid value from begin_write");
					ProcessBlockFunction(dest, src);
					output->end_write(blocksize);
					input->end_read(blocksize);
				}
			}
			catch (const dyplo::InterruptedException&)
			{
				//
			}
			return 0;
		}
	private:
		void start()
		{
			m_thread.start(&run, this);
		}

		static void* run(void* arg)
		{
			return ((ThreadedProcess*)arg)->process();
		}
	};
}