#pragma once

#include "pthreadscheduler.hpp"
#include "queue.hpp"
#include "thread.hpp"

namespace dyplo
{
	template <class InputQueueClass, class OutputQueueClass, int blocksize = 1> class ThreadedProcess
	{
	public:
		InputQueueClass &input;
		OutputQueueClass &output;
		Thread m_thread;

		ThreadedProcess(InputQueueClass &input_, OutputQueueClass &output_):
			input(input_),
			output(output_),
			m_thread(&run, this)
		{
		}

		~ThreadedProcess()
		{
			input.get_scheduler().interrupt();
			output.get_scheduler().interrupt();
			m_thread.join();
		}

		void* process()
		{
			unsigned int count;
			typename InputQueueClass::Element *src;
			typename OutputQueueClass::Element *dest;

			try
			{
				for(;;)
				{
					count = input.begin_read(src, blocksize);
					DEBUG_ASSERT(count >= blocksize, "invalid value from begin_read");
					count = output.begin_write(dest, blocksize);
					DEBUG_ASSERT(count >= blocksize, "invalid value from begin_write");
					for (int i = 0; i < blocksize; ++i)
					{
						*dest++ = (*src++) + 5;
					}
					output.end_write(blocksize);
					input.end_read(blocksize);
				}
			}
			catch (const dyplo::InterruptedException&)
			{
				//
			}
			return 0;
		}
	private:
		static void* run(void* arg)
		{
			return ((ThreadedProcess*)arg)->process();
		}
	};
}