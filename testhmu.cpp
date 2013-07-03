#define YAFFUT_MAIN
#include "yaffut.h"

#include "hmu.hpp"

using namespace hmu; /* I'm lazy */

class CooperativeScheduler
{
public:
	Process* downstream;

	void wait_until_not_full()
	{
		downstream->process_one();
	}
	void wait_until_not_empty()
	{
		throw std::runtime_error("Not implemented: wait_until_not_empty");
	}
	void trigger_not_full()
	{
	}
	void trigger_not_empty()
	{
		downstream->process_one();
	}
};

template <class T> class CooperativeProcess: public Process
{
public:
	hmu::FixedMemoryQueue<T, CooperativeScheduler> input;
	hmu::FixedMemoryQueue<T, hmu::NoopScheduler> output;

	CooperativeProcess():
		input(2),
		output(5)
	{
		input.get_scheduler().downstream = this;
	}

	virtual void process_one()
	{
		const T *item;
		item = input.begin_read(1);
		*output.begin_write(1) = *item;
		output.end_write(1);
		input.end_read(1);
	}
};

FUNC(fixed_memory_queue_without_scheduler)
{
	const unsigned int capacity = 10;
	const unsigned int block = 2;

	hmu::FixedMemoryQueue<int, hmu::NoopScheduler> q(capacity);

	for (int i = 0; i < 20; ++i)
	{
		YAFFUT_CHECK(q.empty());
		int* data = q.begin_write(block);
		YAFFUT_CHECK(data != NULL);
		for (unsigned int b = 0; b < block; ++b)
			data[b] = 10 * i + b;
		q.end_write(block);

		YAFFUT_CHECK(!q.empty());
		const int* result = q.begin_read(block);
		YAFFUT_CHECK(result != NULL);
		for (unsigned int b = 0; b < block; ++b)
			YAFFUT_EQUAL(10 * i + b, result[b]);
		q.end_read(block);
	}
	YAFFUT_CHECK(q.empty());
	YAFFUT_CHECK(!q.full());

	for (unsigned int b = 0; b < capacity; ++b)
	{
		q.begin_write(1);
		q.end_write(1);
	}
	YAFFUT_CHECK(q.full());
	for (unsigned int b = 0; b < capacity; ++b)
	{
		q.begin_read(1);
		q.end_read(1);
	}
	YAFFUT_CHECK(q.empty());
}

FUNC(fixed_memory_queue_cooperative_scheduler)
{
	CooperativeProcess<int> proc;

	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_CHECK(proc.output.empty());

	*proc.input.begin_write(1) = 42;
	proc.input.end_write(1);

	YAFFUT_EQUAL(1, proc.output.size());
	YAFFUT_CHECK(proc.input.empty());

	YAFFUT_EQUAL(42, *proc.output.begin_read(1));
	proc.output.end_read(1);
}
