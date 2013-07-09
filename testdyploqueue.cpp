#include "queue.hpp"
#include "noopscheduler.hpp"

#define YAFFUT_MAIN
#include "yaffut.h"

using namespace dyplo; /* I'm lazy */

FUNC(a_fixed_memory_queue_without_scheduler)
{
	const unsigned int capacity = 10;
	const unsigned int block = 2;

	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> q(capacity);

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
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> q(5);
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
