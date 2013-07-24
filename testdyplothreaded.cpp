#include "threadedprocess.hpp"

#include "yaffut.h"

template <class T, int raise, int blocksize> void process_block_add_constant(T* dest, T* src)
{
	for (int i = 0; i < blocksize; ++i)
		*dest++ = (*src++) + raise;
}


template <class T, int blocksize = 1>
class AddFive: public dyplo::ThreadedProcess<
		dyplo::FixedMemoryQueue<T, dyplo::PthreadScheduler>,
		dyplo::FixedMemoryQueue<T, dyplo::PthreadScheduler>,
		process_block_add_constant<T, 5, blocksize>,
		blocksize>
{
};

FUNC(threading_scheduler_terminate_immediately)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
	AddFive<int, 1> proc;
	proc.set_input(&input_to_a);
	proc.set_output(&output_from_a);
	/* Do nothing. This destroys the thread before it runs, which must not hang. */
}

FUNC(threading_scheduler_block_on_input)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
 	AddFive<int, 1> proc;
	proc.set_input(&input_to_a);
	proc.set_output(&output_from_a);

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
 	AddFive<int, 1> proc;
	proc.set_input(&input_to_a);
	proc.set_output(&output_from_a);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(55, output_from_a.pop_one());
 	input_to_a.push_one(51);
  	input_to_a.push_one(52);
  	YAFFUT_EQUAL(56, output_from_a.pop_one());
  	input_to_a.push_one(53);
}

FUNC(threading_scheduler_multi_stages)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_b(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_c(2);
 	AddFive<int, 1> proc_a;
	AddFive<int, 1> proc_b;
	AddFive<int, 1> proc_c;
	proc_a.set_input(&input_to_a);
	proc_a.set_output(&output_from_a);
	proc_b.set_input(&output_from_a);
	proc_b.set_output(&output_from_b);
	proc_c.set_input(&output_from_b);
	proc_c.set_output(&output_from_c);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(65, output_from_c.pop_one()); /* Three times five is 15 */
 	input_to_a.push_one(51);
  	input_to_a.push_one(52);
  	YAFFUT_EQUAL(66, output_from_c.pop_one());
  	input_to_a.push_one(53);
  	YAFFUT_EQUAL(67, output_from_c.pop_one());
  	YAFFUT_EQUAL(68, output_from_c.pop_one());
}

#include "cooperativescheduler.hpp"
#include "cooperativeprocess.hpp"

template <class T, int blocksize = 1>
class AddFiveTQTPCQ: public dyplo::ThreadedProcess<
		dyplo::FixedMemoryQueue<T, dyplo::PthreadScheduler>,
		dyplo::FixedMemoryQueue<T, dyplo::CooperativeScheduler>,
		process_block_add_constant<T, 5, blocksize>,
		blocksize>
{
};

template <class T, class OutputQueue, int blocksize = 1>
class AddTwoCQCP: public dyplo::CooperativeProcess<
		dyplo::FixedMemoryQueue<T, dyplo::CooperativeScheduler>,
		OutputQueue,
		process_block_add_constant<T, 2, blocksize>,
		blocksize>
{
};

FUNC(threading_and_cooperative_mix)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> output_from_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> output_from_b(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_c(2);

	AddFiveTQTPCQ<int> proc_a;
	AddTwoCQCP<int, typeof(output_from_b) > proc_b;
	AddTwoCQCP<int, typeof(output_from_c) > proc_c;

	proc_a.set_input(&input_to_a);
	proc_a.set_output(&output_from_a);
	proc_c.set_input(&output_from_b);
	proc_c.set_output(&output_from_c);
	proc_b.set_input(&output_from_a);
	proc_b.set_output(&output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(59, output_from_c.pop_one()); /* 5 + 2 + 2 */
 	input_to_a.push_one(51);
  	input_to_a.push_one(52);
  	YAFFUT_EQUAL(60, output_from_c.pop_one());
  	input_to_a.push_one(53);
  	YAFFUT_EQUAL(61, output_from_c.pop_one());
  	YAFFUT_EQUAL(62, output_from_c.pop_one());
}

FUNC(threading_and_cooperative_mix_block_output)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> output_from_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> output_from_b(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_c(2);

	AddFiveTQTPCQ<int> proc_a;
	AddTwoCQCP<int, typeof(output_from_b) > proc_b;
	AddTwoCQCP<int, typeof(output_from_c) > proc_c;

	proc_a.set_input(&input_to_a);
	proc_a.set_output(&output_from_a);
	proc_c.set_input(&output_from_b);
	proc_c.set_output(&output_from_c);
	proc_b.set_input(&output_from_a);
	proc_b.set_output(&output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(59, output_from_c.pop_one()); /* 5 + 2 + 2 */
 	input_to_a.push_one(51);
  	input_to_a.push_one(52);
  	YAFFUT_EQUAL(60, output_from_c.pop_one());
  	input_to_a.push_one(53);
  	input_to_a.push_one(54); /* blocks on output */
}

#include "filequeue.hpp"

FUNC(file_queue_in_chain)
{
	/* Q -> P -> Q -> "file" -> Q -> P -> ... */
	dyplo::Pipe p;
	dyplo::FilePollScheduler file_scheduler;
	dyplo::SingleElementQueue<int, dyplo::CooperativeScheduler> input_to_p1;
	dyplo::FileOutputQueue<int> output_to_file(file_scheduler, p.write_handle(), 10);
	dyplo::FileInputQueue<int> input_from_file(file_scheduler, p.read_handle(), 10);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_p2(10);

	dyplo::CooperativeProcess<
		typeof(input_to_p1), typeof(output_to_file),
		process_block_add_constant<int, 2, 1>,
		1> p1;
	dyplo::ThreadedProcess<
		typeof(input_from_file), typeof(output_from_p2),
		process_block_add_constant<int, 7, 1> 
		> p2;
	
	p1.set_input(&input_to_p1);
	p1.set_output(&output_to_file);
	p2.set_input(&input_from_file);
	p2.set_output(&output_from_p2);
	
	input_to_p1.push_one(10);
	input_to_p1.push_one(20);
	YAFFUT_EQUAL(19, output_from_p2.pop_one());
	YAFFUT_EQUAL(29, output_from_p2.pop_one());
}
