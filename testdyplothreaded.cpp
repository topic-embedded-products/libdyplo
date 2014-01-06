/*
 * testdyplothreaded.cpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. <Mike Looijmans> (http://www.topic.nl).
 * All rights reserved.
 *
 * This file is part of libdyplo.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA or see <http://www.gnu.org/licenses/>.
 *
 * You can contact Topic by electronic mail via info@topic.nl or via
 * paper mail at the following address: Postbus 440, 5680 AK Best, The Netherlands.
 */
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

struct threading_scheduler {};

TEST(threading_scheduler, terminate_immediately)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
	AddFive<int, 1> proc;
	proc.set_input(&input_to_a);
	proc.set_output(&output_from_a);
	/* Do nothing. This destroys the thread before it runs, which must not hang. */
}

TEST(threading_scheduler, block_on_input)
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

TEST(threading_scheduler, reuse_process)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_a(2);
	AddFive<int, 1> proc;
	proc.set_input(&input_to_a);
	proc.set_output(&output_from_a);

	input_to_a.push_one(60);
	YAFFUT_EQUAL(65, output_from_a.pop_one());
	input_to_a.push_one(61);
	input_to_a.push_one(62);
	int* data;
	output_from_a.begin_read(data, 2); /* Wait until (some) output */

	proc.terminate();
	input_to_a.resume_read();
	output_from_a.resume_write();
	proc.set_input(&input_to_a);
	proc.set_output(&output_from_a);
	input_to_a.push_one(63);
	input_to_a.push_one(64);
	/* No data lost */
	YAFFUT_EQUAL(66, output_from_a.pop_one());
	YAFFUT_EQUAL(67, output_from_a.pop_one());
	YAFFUT_EQUAL(68, output_from_a.pop_one());
	YAFFUT_EQUAL(69, output_from_a.pop_one());
	/* External interrupt does not kill internal process */
	input_to_a.push_one(100);
	input_to_a.push_one(101);
	input_to_a.interrupt_write();
	output_from_a.interrupt_read();
	ASSERT_THROW(input_to_a.begin_write(data, 2), dyplo::InterruptedException);
	ASSERT_THROW(output_from_a.begin_read(data, 3), dyplo::InterruptedException);
	input_to_a.resume_write();
	output_from_a.resume_read();
	/* No data lost */
	YAFFUT_EQUAL(105, output_from_a.pop_one());
	input_to_a.push_one(102);
	YAFFUT_EQUAL(106, output_from_a.pop_one());
	YAFFUT_EQUAL(107, output_from_a.pop_one());
}

TEST(threading_scheduler, block_on_output)
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

TEST(threading_scheduler, multi_stages)
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

struct mixed_scheduler {};

TEST(mixed_scheduler, threading_and_cooperative)
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

TEST(mixed_scheduler, threading_and_cooperative_block_output)
{
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> input_to_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> output_from_a(2);
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> output_from_b(2);
	dyplo::FixedMemoryQueue<int, dyplo::PthreadScheduler> output_from_c(2);

	AddTwoCQCP<int, typeof(output_from_c) > proc_c;
	AddTwoCQCP<int, typeof(output_from_b) > proc_b;
	AddFiveTQTPCQ<int> proc_a;

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

TEST(mixed_scheduler, file_queue_in_chain)
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
