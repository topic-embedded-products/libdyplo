#include "pthreadscheduler.hpp"
#include "queue.hpp"
#include "noopscheduler.hpp"
#include "cooperativescheduler.hpp"


#define YAFFUT_MAIN
#include "yaffut.h"

using namespace hmu; /* I'm lazy */


template <class T, int blocksize = 1> class CooperativeProcess: public Process
{
public:
	hmu::FixedMemoryQueue<T, CooperativeScheduler> input;
	hmu::FixedMemoryQueue<T, hmu::NoopScheduler> output;

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


FUNC(a_fixed_memory_queue_without_scheduler)
{
	const unsigned int capacity = 10;
	const unsigned int block = 2;

	hmu::FixedMemoryQueue<int, hmu::NoopScheduler> q(capacity);

	for (int i = 0; i < 20; ++i)
	{
		int* data;
		YAFFUT_CHECK(q.empty());
		YAFFUT_CHECK(q.begin_write(data, block) >= block);
		YAFFUT_CHECK(data != NULL);
		for (unsigned int b = 0; b < block; ++b)
			data[b] = 10 * i + b;
		q.end_write(block);

		YAFFUT_CHECK(!q.empty());
		int result;
		result = q.begin_read(data, block);
		YAFFUT_EQUAL(block, result);
		YAFFUT_CHECK(data != NULL);
		for (unsigned int b = 0; b < block; ++b)
			YAFFUT_EQUAL(10 * i + b, data[b]);
		q.end_read(block);
	}
	YAFFUT_CHECK(q.empty());
	YAFFUT_CHECK(!q.full());

	for (unsigned int b = 0; b < capacity; ++b)
	{
		int* data;
		YAFFUT_EQUAL(capacity - b, q.begin_write(data, 1));
		q.end_write(1);
	}
	YAFFUT_CHECK(q.full());
	for (unsigned int b = 0; b < capacity; ++b)
	{
		int* data;
		YAFFUT_EQUAL(capacity - b, q.begin_read(data, 1));
		q.end_read(1);
	}
	YAFFUT_CHECK(q.empty());
}

FUNC(a_fixed_memory_queue_border_cases)
{
	hmu::FixedMemoryQueue<int, hmu::NoopScheduler> q(5);
	int* data;

	YAFFUT_EQUAL(5, q.begin_write(data, 1));
	data[0] = 1;
	data[1] = 2;
	data[2] = 3;
	q.end_write(3);
	YAFFUT_EQUAL(3, q.begin_read(data, 1));
	YAFFUT_EQUAL(2, data[1]);
	q.end_read(2);
	YAFFUT_EQUAL(2, q.begin_write(data, 1)); /* Leave 1 item */
	data[0] = 4;
	data[1] = 5;
	q.end_write(1);
	q.end_write(1);
	YAFFUT_EQUAL(2, q.begin_write(data, 1));
	YAFFUT_EQUAL(3, q.begin_read(data, 1));
	q.end_write(1);
	YAFFUT_EQUAL(1, q.begin_write(data, 1));
	q.end_read(2);
	YAFFUT_EQUAL(3, q.begin_write(data, 1));
}


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

template <class InputQueueClass, class OutputQueueClass, int blocksize = 1> class CooperativeLinkableProcess: public Process
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
	hmu::FixedMemoryQueue<int, CooperativeScheduler> input_to_a(1);
	hmu::FixedMemoryQueue<int, CooperativeScheduler> between_a_and_b(1);
	hmu::FixedMemoryQueue<int, hmu::NoopScheduler> output_from_b(2);
	
	CooperativeLinkableProcess < hmu::FixedMemoryQueue<int, CooperativeScheduler>, hmu::FixedMemoryQueue<int, CooperativeScheduler> >
		a(input_to_a, between_a_and_b);
	CooperativeLinkableProcess< hmu::FixedMemoryQueue<int, CooperativeScheduler>, hmu::FixedMemoryQueue<int, NoopScheduler> >
		b(between_a_and_b, output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(52, output_from_b.pop_one());
}


class Thread
{
protected:
	pthread_t m_thread;
public:
	Thread(void *(*start_routine) (void *), void *arg)
	{
		int result = pthread_create(&m_thread, NULL, start_routine, arg);
		if (result != 0)
			throw std::runtime_error("Failed to start thread");
	}
	~Thread()
	{
		pthread_join(m_thread, NULL);
	}
};

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
		catch (const InterruptedException&)
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
	hmu::FixedMemoryQueue<int, PthreadScheduler> input_to_a(2);
	hmu::FixedMemoryQueue<int, PthreadScheduler> output_from_a(2);
 	ThreadedProcess < hmu::FixedMemoryQueue<int, PthreadScheduler>, hmu::FixedMemoryQueue<int, PthreadScheduler> >
 		proc(input_to_a, output_from_a);
	/* Do nothing. This destroys the thread before it runs, which must not hang. */
}

FUNC(threading_scheduler_block_on_input)
{
	hmu::FixedMemoryQueue<int, PthreadScheduler> input_to_a(2);
	hmu::FixedMemoryQueue<int, PthreadScheduler> output_from_a(2);
	ThreadedProcess < hmu::FixedMemoryQueue<int, PthreadScheduler>, hmu::FixedMemoryQueue<int, PthreadScheduler> >
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
	hmu::FixedMemoryQueue<int, PthreadScheduler> input_to_a(2);
	hmu::FixedMemoryQueue<int, PthreadScheduler> output_from_a(1);
 	ThreadedProcess < hmu::FixedMemoryQueue<int, PthreadScheduler>, hmu::FixedMemoryQueue<int, PthreadScheduler> >
 		proc(input_to_a, output_from_a);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(55, output_from_a.pop_one());
	input_to_a.push_one(51);
	input_to_a.push_one(52);
	YAFFUT_EQUAL(56, output_from_a.pop_one());
	input_to_a.push_one(53);
}