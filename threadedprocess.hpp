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
