#include "queue.hpp"
#include "noopscheduler.hpp"
#include "cooperativescheduler.hpp"
#include "cooperativeprocess.hpp"

#include "yaffut.h"

template <class T, class OutputQueue, int blocksize = 1> class AddOne: public dyplo::CooperativeProcess<
		dyplo::FixedMemoryQueue<T, dyplo::CooperativeScheduler>,
		OutputQueue,
		blocksize>
{
	void process_block(T* dest, T* src)
	{
		for (int i = 0; i < blocksize; ++i)
			*dest++ = (*src++) + 1;
	}
};


FUNC(fixed_memory_queue_cooperative_scheduler)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input(1);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output(2);

	AddOne<int, dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler>, 1> proc;
	proc.set_input(&input);
	proc.set_output(&output);

	YAFFUT_CHECK(input.empty());
	YAFFUT_CHECK(output.empty());

	input.push_one(42);
	YAFFUT_EQUAL(1, output.size());
	YAFFUT_CHECK(input.empty());
	YAFFUT_EQUAL(43, output.pop_one());
	YAFFUT_CHECK(output.empty());
}

FUNC(fixed_memory_queue_cooperative_scheduler_multi)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input(2);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output(2);

	AddOne<int, dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler>, 2> proc;
	proc.set_input(&input);
	proc.set_output(&output);

	YAFFUT_CHECK(input.empty());
	YAFFUT_CHECK(output.empty());

	input.push_one(42);
	YAFFUT_CHECK(output.empty());
	input.push_one(43);
	YAFFUT_EQUAL(2, output.size());
	YAFFUT_CHECK(input.empty());

	YAFFUT_EQUAL(43, output.pop_one());
	YAFFUT_EQUAL(44, output.pop_one());
	YAFFUT_CHECK(output.empty());
}

FUNC(multiple_processes_and_queues)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input_to_a(1);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> between_a_and_b(1);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output_from_b(2);

	AddOne<int, dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> > a;
	AddOne<int, dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> > b;

	a.set_input(&input_to_a);
	a.set_output(&between_a_and_b);
	b.set_input(&between_a_and_b);
	b.set_output(&output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(52, output_from_b.pop_one());
	input_to_a.push_one(51);
	YAFFUT_EQUAL(53, output_from_b.pop_one());
	input_to_a.push_one(52);
	YAFFUT_EQUAL(54, output_from_b.pop_one());

	YAFFUT_EQUAL(0, input_to_a.size());
	YAFFUT_EQUAL(0, between_a_and_b.size());
	YAFFUT_EQUAL(0, output_from_b.size());

	input_to_a.push_one(60);
	YAFFUT_EQUAL(0, input_to_a.size());
	YAFFUT_EQUAL(0, between_a_and_b.size());
	YAFFUT_EQUAL(1, output_from_b.size());
	input_to_a.push_one(61);
	YAFFUT_EQUAL(2, output_from_b.size());
}
