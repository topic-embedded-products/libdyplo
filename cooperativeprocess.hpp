#pragma once

#include "queue.hpp"
#include "cooperativescheduler.hpp"

namespace dyplo
{
	template <class InputQueueClass, class OutputQueueClass, int blocksize = 1> class CooperativeProcess: public Process
	{
	protected:
		InputQueueClass *input;
		OutputQueueClass *output;

	public:
		void set_input(InputQueueClass *value)
		{
			input = value;
			input->get_scheduler().downstream = this;
		}

		void set_output(OutputQueueClass *value)
		{
			output = value;
		}

		virtual void process_block(
			typename OutputQueueClass::Element *dest,
			typename InputQueueClass::Element *src) = 0;

		virtual void process_one()
		{
			unsigned int count;
			typename InputQueueClass::Element *src;
			typename OutputQueueClass::Element *dest;

			for (;;)
			{
				count = input->begin_read(src, 0);
				if (count < blocksize)
					return;
				output->begin_write(dest, blocksize);
				process_block(dest, src);
				output->end_write(blocksize);
				input->end_read(blocksize);
			}
		}
	};
}