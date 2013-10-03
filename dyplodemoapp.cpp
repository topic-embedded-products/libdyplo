#include <iostream>
#include <sstream>

#include "threadedprocess.hpp"
#include "cooperativescheduler.hpp"
#include "cooperativeprocess.hpp"
#include "thread.hpp"

/*
 * Setup the following queue:
 * stdin -> (T) -> string_to_int -> (T) -> Add_5 -> (C) -> int_to_string -> (T) -> stdout
 */

template <class T, int raise, int blocksize> void process_block_add_constant(T* dest, T* src)
{
	for (int i = 0; i < blocksize; ++i)
		*dest++ = (*src++) + raise;
}


void process_string_to_int(int* dest, std::string *src)
{
	*dest = atoi(src->c_str());
}

void process_int_to_string(std::string *dest, int* src)
{
	std::stringstream ss;
	ss << *src;
	*dest = ss.str();
}

void display_string(std::string* src)
{
	std::cout << *src << std::endl;
}


template <class InputQueueClass,
		void(*ProcessItemFunction)(typename InputQueueClass::Element*),
		int blocksize = 1>
class ThreadedProcessSink
{
	protected:
		InputQueueClass *input;
		dyplo::Thread m_thread;
	public:
		typedef typename InputQueueClass::Element InputElement;

		ThreadedProcessSink():
			input(NULL),
			m_thread()
		{
		}

		~ThreadedProcessSink()
		{
			if (input != NULL)
				input->interrupt_read();
			m_thread.join();
		}

		void set_input(InputQueueClass *value)
		{
			input = value;
			if (input)
				start();
		}

		void* process()
		{
			unsigned int count;
			InputElement *src;

			try
			{
				for(;;)
				{
					count = input->begin_read(src, blocksize);
					DEBUG_ASSERT(count >= blocksize, "invalid value from begin_read");
					ProcessItemFunction(src);
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
			return ((ThreadedProcessSink*)arg)->process();
		}
};


int main(int argc, char** argv)
{
	dyplo::FixedMemoryQueue<std::string, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
	dyplo::SingleElementQueue<int, dyplo::CooperativeScheduler> output_from_b;
	dyplo::FixedMemoryQueue<std::string, dyplo::PthreadScheduler> output_from_c(2);


	dyplo::ThreadedProcess<
		typeof(input_to_a), typeof(output_from_a),
		process_string_to_int,
		1 /*blocksize*/ > proc_a;

	dyplo::ThreadedProcess<
		typeof(output_from_a), typeof(output_from_b),
		process_block_add_constant<int, 5, 1>
		> proc_b;

	dyplo::CooperativeProcess<
		typeof(output_from_b), typeof(output_from_c),
		process_int_to_string,
		1 /*blocksize*/ > proc_c;

	ThreadedProcessSink<
		typeof(output_from_c),
		display_string,
		1 /*blocksize*/ > proc_d;

	proc_a.set_input(&input_to_a);
	proc_a.set_output(&output_from_a);
	proc_c.set_input(&output_from_b);
	proc_c.set_output(&output_from_c);
	proc_b.set_input(&output_from_a);
	proc_b.set_output(&output_from_b);
	proc_d.set_input(&output_from_c);
	
	for(;;)
	{
		std::string line;
		std::cin >> line;
		if (std::cin.eof())
			break;
		input_to_a.push_one(line);
	}
	input_to_a.wait_empty();
	output_from_a.wait_empty();
	output_from_b.wait_empty();
	output_from_c.wait_empty();
	return 0;
}
