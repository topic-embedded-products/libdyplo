#include "pthreadscheduler.hpp"
#include "queue.hpp"
#include "noopscheduler.hpp"
#include "cooperativescheduler.hpp"
#include "thread.hpp"

#define YAFFUT_MAIN
#include "yaffut.h"

template <class T, int blocksize = 1> class CooperativeProcess: public dyplo::Process
{
public:
	dyplo::FixedMemoryQueue<T, dyplo::CooperativeScheduler> input;
	dyplo::FixedMemoryQueue<T, dyplo::NoopScheduler> output;

	CooperativeProcess():
		input(blocksize),
		output(5 * blocksize)
	{
		input.get_scheduler().downstream = this;
	}

	virtual void process_one()
	{
		unsigned int count;
		T *src;
		T *dest;

		for (;;)
		{
			count = input.begin_read(src, 0);
			if (count < blocksize)
				return;
			YAFFUT_CHECK(output.begin_write(dest, 0) >= blocksize);
			for (int i = 0; i < blocksize; ++i)
			{
				*dest++ = *src++;
			}
			output.end_write(blocksize);
			input.end_read(blocksize);
		}
	}
};

FUNC(fixed_memory_queue_cooperative_scheduler)
{
	CooperativeProcess<int> proc;

	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_CHECK(proc.output.empty());

	proc.input.push_one(42);
	YAFFUT_EQUAL(1, proc.output.size());
	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_EQUAL(42, proc.output.pop_one());
	YAFFUT_CHECK(proc.output.empty());
}

FUNC(fixed_memory_queue_cooperative_scheduler_multi)
{
	CooperativeProcess<int, 2> proc;

	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_CHECK(proc.output.empty());

	proc.input.push_one(42);
	YAFFUT_CHECK(proc.output.empty());
	proc.input.push_one(43);
	YAFFUT_EQUAL(2, proc.output.size());
	YAFFUT_CHECK(proc.input.empty());

	YAFFUT_EQUAL(42, proc.output.pop_one());
	YAFFUT_EQUAL(43, proc.output.pop_one());
	YAFFUT_CHECK(proc.output.empty());
}

template <class InputQueueClass, class OutputQueueClass, int blocksize = 1> class CooperativeLinkableProcess: public dyplo::Process
{
public:
	InputQueueClass &input;
	OutputQueueClass &output;

	CooperativeLinkableProcess(InputQueueClass &input_, OutputQueueClass &output_):
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
			YAFFUT_CHECK(output.begin_write(dest, 0) >= blocksize);
			for (int i = 0; i < blocksize; ++i)
			{
				*dest++ = (*src++) + 1;
			}
			output.end_write(blocksize);
			input.end_read(blocksize);
		}
	}
};



FUNC(multiple_processes_and_queues)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input_to_a(1);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> between_a_and_b(1);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output_from_b(2);
	
	CooperativeLinkableProcess <
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> >
			a(input_to_a, between_a_and_b);
	CooperativeLinkableProcess<
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> >
			b(between_a_and_b, output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(52, output_from_b.pop_one());
}

template <class InputQueueClass, class OutputQueueClass, int blocksize = 1> class ThreadedProcess
{
public:
	InputQueueClass &input;
	OutputQueueClass &output;
	dyplo::Thread m_thread;

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
				YAFFUT_CHECK(count >= blocksize);
				YAFFUT_CHECK(output.begin_write(dest, blocksize) >= blocksize);
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

FUNC(threading_scheduler_terminate_immediately)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
 	ThreadedProcess <
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> >
			proc(input_to_a, output_from_a);
	/* Do nothing. This destroys the thread before it runs, which must not hang. */
}

FUNC(threading_scheduler_block_on_input)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
 	ThreadedProcess <
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> >
			proc(input_to_a, output_from_a);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(55, output_from_a.pop_one());
	input_to_a.push_one(51);
	input_to_a.push_one(52);
	YAFFUT_EQUAL(56, output_from_a.pop_one());
	YAFFUT_EQUAL(57, output_from_a.pop_one());
}

FUNC(threading_scheduler_block_on_output)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(1);
 	ThreadedProcess <
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> >
			proc(input_to_a, output_from_a);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(55, output_from_a.pop_one());
	input_to_a.push_one(51);
	input_to_a.push_one(52);
	YAFFUT_EQUAL(56, output_from_a.pop_one());
	input_to_a.push_one(53);
}