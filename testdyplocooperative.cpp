#include "queue.hpp"
#include "noopscheduler.hpp"
#include "cooperativescheduler.hpp"
#include "cooperativeprocess.hpp"

#include "yaffut.h"

FUNC(fixed_memory_queue_cooperative_scheduler)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input(1);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output(2);

	dyplo::CooperativeProcess<
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> >
			proc(input, output);

	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_CHECK(proc.output.empty());

	proc.input.push_one(42);
	YAFFUT_EQUAL(1, proc.output.size());
	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_EQUAL(43, proc.output.pop_one());
	YAFFUT_CHECK(proc.output.empty());
}

FUNC(fixed_memory_queue_cooperative_scheduler_multi)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input(2);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output(2);

	dyplo::CooperativeProcess<
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler>,
		2>
			proc(input, output);

	YAFFUT_CHECK(proc.input.empty());
	YAFFUT_CHECK(proc.output.empty());

	proc.input.push_one(42);
	YAFFUT_CHECK(proc.output.empty());
	proc.input.push_one(43);
	YAFFUT_EQUAL(2, proc.output.size());
	YAFFUT_CHECK(proc.input.empty());

	YAFFUT_EQUAL(43, proc.output.pop_one());
	YAFFUT_EQUAL(44, proc.output.pop_one());
	YAFFUT_CHECK(proc.output.empty());
}

FUNC(multiple_processes_and_queues)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input_to_a(1);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> between_a_and_b(1);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output_from_b(2);
	
	dyplo::CooperativeProcess <
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> >
			a(input_to_a, between_a_and_b);
	dyplo::CooperativeProcess<
		dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> >
			b(between_a_and_b, output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(52, output_from_b.pop_one());
}
