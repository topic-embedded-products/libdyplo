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
