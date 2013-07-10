#pragma once

#include "queue.hpp"
#include "cooperativescheduler.hpp"

namespace dyplo
{
	template <class InputQueueClass, class OutputQueueClass, int blocksize = 1> class CooperativeProcess: public Process
	{
	public:
		InputQueueClass &input;
		OutputQueueClass &output;

		CooperativeProcess(InputQueueClass &input_, OutputQueueClass &output_):
			input(input_),
			output(output_)
		{
			input.get_scheduler().downstream = this;
		}

		virtual void process_one()
		{
			unsigned int count;
			typename InputQueueClass::Element *src;
			typename OutputQueueClass::Element *dest;

			for (;;)
			{
				count = input.begin_read(src, 0);
				if (count < blocksize)
					return;
				output.begin_write(dest, blocksize);
				for (int i = 0; i < blocksize; ++i)
				{
					*dest++ = (*src++) + 1;
				}
				output.end_write(blocksize);
				input.end_read(blocksize);
			}
		}
	};
}