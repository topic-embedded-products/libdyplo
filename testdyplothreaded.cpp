#include "threadedprocess.hpp"

#include "yaffut.h"

FUNC(threading_scheduler_terminate_immediately)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
 	dyplo::ThreadedProcess <
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler>,
		dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> >
			proc(input_to_a, output_from_a);
	/* Do nothing. This destroys the thread before it runs, which must not hang. */
}

FUNC(threading_scheduler_block_on_input)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
 	dyplo::ThreadedProcess <
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
 	dyplo::ThreadedProcess <
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