#pragma once

#include "queue.hpp"
#include "cooperativescheduler.hpp"

namespace dyplo
{
	template <
		class InputQueueClass,
		class OutputQueueClass,
		void(*ProcessBlockFunction)(typename OutputQueueClass::Element*, typename InputQueueClass::Element*),
		int blocksize = 1>
	class CooperativeProcess: public Process
	{
	protected:
		InputQueueClass *input;
		OutputQueueClass *output;

	public:
		CooperativeProcess():
			input(NULL),
			output(NULL)
		{}
		
		~CooperativeProcess()
		{
			/* Inform downstream and upstream elements that we
			 * will terminate */
			if (input != NULL)
				input->get_scheduler().interrupt();
			if (output != NULL)
				output->get_scheduler().interrupt();
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

		/* override */ void process_one()
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
				ProcessBlockFunction(dest, src);
				output->end_write(blocksize);
				input->end_read(blocksize);
			}
		}

		/* override */ void interrupt()
		{
			if (output != NULL)
				output->get_scheduler().interrupt();
		}
	};
}
