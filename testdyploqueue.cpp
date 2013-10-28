#include <unistd.h>
#include <string>
#include "queue.hpp"
#include "noopscheduler.hpp"
#include "filequeue.hpp"

#include "yaffut.h"

struct a_fixed_memory_queue {};

TEST(a_fixed_memory_queue, without_scheduler)
{
	const unsigned int capacity = 10;
	const unsigned int block = 2;

	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> q(capacity);

	for (unsigned int i = 0; i < 20; ++i)
	{
		int* data;
		YAFFUT_CHECK(q.empty());
		YAFFUT_CHECK(q.begin_write(data, block) >= block);
		YAFFUT_CHECK(data != NULL);
		for (unsigned int b = 0; b < block; ++b)
			data[b] = 10 * i + b;
		q.end_write(block);

		YAFFUT_CHECK(!q.empty());
		unsigned int result;
		result = q.begin_read(data, block);
		YAFFUT_EQUAL(block, result);
		YAFFUT_CHECK(data != NULL);
		for (unsigned int b = 0; b < block; ++b)
			YAFFUT_EQUAL((int)(10 * i + b), data[b]);
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

TEST(a_fixed_memory_queue, border_cases)
{
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> q(5);
	int* data;

	YAFFUT_EQUAL(5u, q.begin_write(data, 1));
	data[0] = 1;
	data[1] = 2;
	data[2] = 3;
	q.end_write(3);
	YAFFUT_EQUAL(3u, q.begin_read(data, 1));
	YAFFUT_EQUAL(2, data[1]);
	q.end_read(2);
	YAFFUT_EQUAL(2u, q.begin_write(data, 1)); /* Leave 1 item */
	data[0] = 4;
	data[1] = 5;
	q.end_write(1);
	q.end_write(1);
	YAFFUT_EQUAL(2u, q.begin_write(data, 1));
	YAFFUT_EQUAL(3u, q.begin_read(data, 1));
	q.end_write(1);
	YAFFUT_EQUAL(1u, q.begin_write(data, 1));
	q.end_read(2);
	YAFFUT_EQUAL(3u, q.begin_write(data, 1));
}

struct a_single_queue {};
TEST(a_single_queue, basic)
{
	dyplo::SingleElementQueue<int, dyplo::NoopScheduler> q;
	int* data = NULL;
	
	YAFFUT_CHECK(q.empty());
	YAFFUT_EQUAL(1u, q.begin_write(data, 1));
	*data = 42;
	q.end_write(1);
	data = NULL;
	YAFFUT_EQUAL(1u, q.begin_read(data, 1));
	YAFFUT_EQUAL(*data, 42);
	YAFFUT_CHECK(q.full());
	q.end_read(1);
	YAFFUT_CHECK(q.empty());
}

TEST(a_fixed_memory_queue, with_strings)
{
	dyplo::FixedMemoryQueue<std::string, dyplo::NoopScheduler> q(5);
	std::string *data;
	
	for (int repeat = 2; repeat !=0; --repeat)
	{
		YAFFUT_CHECK(q.empty());
		YAFFUT_CHECK(q.begin_write(data, 4) >= 4);
		data[0] = "One";
		data[1] = "Two";
		data[2] = "Three";
		data[3] = "Four";
		q.end_write(4);
		q.push_one("Five");
		
		YAFFUT_EQUAL(std::string("One"), q.pop_one());
		YAFFUT_EQUAL(4u, q.begin_read(data, 4));
		YAFFUT_EQUAL("Two", data[0]);
		YAFFUT_EQUAL("Three", data[1]);
		YAFFUT_EQUAL("Four", data[2]);
		q.end_read(4);
	}
}

class Storage
{
public:
	static int instances_alive_count;
	static int assignment_count;
	Storage()
	{
		++instances_alive_count;
	}
	~Storage()
	{
		--instances_alive_count;
	}
	/* An assignment operator is needed, keep count */
	const Storage& operator =(const Storage&)
	{
		++assignment_count;
		return *this;
	}
private:
	/* Prove that a copy constructor is not needed */
	Storage(const Storage&);
};

int Storage::instances_alive_count = 0;
int Storage::assignment_count = 0;

TEST(a_fixed_memory_queue, with_custom_class)
{
	YAFFUT_EQUAL(0, Storage::instances_alive_count);
	YAFFUT_EQUAL(0, Storage::assignment_count);
	{
		dyplo::FixedMemoryQueue<Storage, dyplo::NoopScheduler> q(3);
		YAFFUT_EQUAL(3, Storage::instances_alive_count);
		Storage* data;
		q.begin_write(data, 1);
		*data = Storage();
		YAFFUT_EQUAL(3, Storage::instances_alive_count);
		YAFFUT_EQUAL(1, Storage::assignment_count);
	}
	YAFFUT_EQUAL(1, Storage::assignment_count);
	YAFFUT_EQUAL(0, Storage::instances_alive_count);
}

struct a_file_queue {};
TEST(a_file_queue, blocking)
{
	dyplo::Pipe p;
	dyplo::FilePollScheduler scheduler;
	dyplo::FileOutputQueue<int> output(scheduler, p.write_handle(), 10);
	dyplo::FileInputQueue<int> input(scheduler, p.read_handle(), 5);
	int* data = NULL;
	unsigned int count;

	count = output.begin_write(data, 1);
	YAFFUT_EQUAL(10u, count);
	YAFFUT_CHECK(data != NULL);
	for (int i = 0; i < 10; ++i)
		data[i] = i * 10;
	output.end_write(10);
	
	count = input.begin_read(data, 5);
	YAFFUT_EQUAL(5u, count);
	YAFFUT_CHECK(data != NULL);
	for (int i = 0; i < 5; ++i)
		YAFFUT_EQUAL(i * 10, data[i]);
	input.end_read(3); /* Only take 3 items from the queue */

	count = input.begin_read(data, 5);
	YAFFUT_EQUAL(5u, count);
	YAFFUT_CHECK(data != NULL);
	for (int i = 0; i < 5; ++i)
		YAFFUT_EQUAL(30 + i * 10, data[i]);
	input.end_read(5);

	count = input.begin_read(data, 2);
	YAFFUT_EQUAL(2u, count);
	YAFFUT_CHECK(data != NULL);
	for (int i = 0; i < 2; ++i)
		YAFFUT_EQUAL(80 + i * 10, data[i]);
	input.end_read(2);
}

TEST(a_file_queue, non_blocking)
{
	dyplo::Pipe p;
	dyplo::FilePollScheduler scheduler;
	dyplo::FileOutputQueue<int> output(scheduler, p.write_handle(), 10);
	dyplo::FileInputQueue<int> input(scheduler, p.read_handle(), 10);
	int* data = NULL;
	unsigned int count;

	count = output.begin_write(data, 1);
	YAFFUT_EQUAL(10u, count);
	YAFFUT_CHECK(data != NULL);
	for (int i = 0; i < 10; ++i)
		data[i] = i * 10;
	output.end_write(10);

	count = input.begin_read(data, 5);
	YAFFUT_EQUAL(10u, count); /* Should read more than requested */
	input.end_read(count);
	
	int written = 0;
	while(true)
	{
		/* Wait until write would block on the OS fifo */
		count = output.begin_write(data, 0);
		if (count < 10)
			break;
		YAFFUT_EQUAL(10u, count);
		output.end_write(10);
		written += 10;
	}
	while (written > 10)
	{
		/* Empty the OS fifo */
		count = input.begin_read(data, 10);
		if (count == 0)
			break;
		input.end_read(count);
		written -= count;
	}
	YAFFUT_CHECK(written <= 10);
	/* There must be room in the OS fifo again */
	count = output.begin_write(data, 0);
	YAFFUT_CHECK(count > 0);
	count = input.begin_read(data, 10);
	YAFFUT_CHECK(count > 0);
}

TEST(a_file_queue, interrupt_resume)
{
	dyplo::Pipe p;
	dyplo::FilePollScheduler scheduler;
	dyplo::FileOutputQueue<int> output(scheduler, p.write_handle(), 10);
	dyplo::FileInputQueue<int> input(scheduler, p.read_handle(), 10);
	int* data = NULL;
	unsigned int count;

	count = output.begin_write(data, 1);
	YAFFUT_EQUAL(10u, count);
	YAFFUT_CHECK(data != NULL);
	for (int i = 0; i < 10; ++i)
		data[i] = i * 10;
	output.end_write(10);

	count = input.begin_read(data, 5);
	YAFFUT_EQUAL(10u, count); /* Should read more than requested */
	input.end_read(count);
	
	input.interrupt_read();
	scheduler.reset();
	
	int written = 0;
	while(true)
	{
		/* Wait until write would block on the OS fifo */
		count = output.begin_write(data, 0);
		if (count < 10)
			break;
		YAFFUT_EQUAL(10u, count);
		output.end_write(10);
		written += 10;
	}
	
	output.interrupt_write();
	scheduler.reset();
	
	while (written > 10)
	{
		/* Empty the OS fifo */
		count = input.begin_read(data, 10);
		if (count == 0)
			break;
		input.end_read(count);
		written -= count;
	}
	YAFFUT_CHECK(written <= 10);
	/* There must be room in the OS fifo again */
	count = output.begin_write(data, 0);
	YAFFUT_CHECK(count > 0);
	count = input.begin_read(data, 10);
	YAFFUT_CHECK(count > 0);
}
