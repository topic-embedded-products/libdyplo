#define YAFFUT_MAIN
#include "yaffut.h"

#include "hmu.hpp"

using namespace hmu; /* I'm lazy */

class CooperativeScheduler
{
public:
	Process* downstream;
	bool is_locked;

	CooperativeScheduler():
		downstream(NULL),
		is_locked(false)
	{
	}

	void process_one()
	{
		unlock();
		downstream->process_one();
		lock();
	}
	
	void wait_until_not_full()
	{
		process_one();
	}
	void wait_until_not_empty()
	{
		YAFFUT_FAIL("wait_until_not_empty called");
	}
	void trigger_not_full()
	{
	}
	void trigger_not_empty()
	{
		process_one();
	}

	void lock()
	{
		YAFFUT_CHECK(!is_locked);
		is_locked = true;
	}

	void unlock()
	{
		//YAFFUT_CHECK(is_locked);
		is_locked = false;
	}
};

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
		q.begin_write(data, 1);
		q.end_write(1);
	}
	YAFFUT_CHECK(q.full());
	for (unsigned int b = 0; b < capacity; ++b)
	{
		int* data;
		q.begin_read(data, 1);
		q.end_read(1);
	}
	YAFFUT_CHECK(q.empty());
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

