/*
 * testdyplocooperative.cpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. (http://www.topic.nl).
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
#include "queue.hpp"
#include "noopscheduler.hpp"
#include "cooperativescheduler.hpp"
#include "cooperativeprocess.hpp"

#include "yaffut.h"

template <class TD, class TS, int blocksize> void process_block_add_one(TD* dest, TS* src)
{
	for (int i = 0; i < blocksize; ++i)
		*dest++ = (*src++) + 1;
}

template <class InputQueue, class OutputQueue, int blocksize = 1>
class AddOne: public dyplo::CooperativeProcess<
	InputQueue,
	OutputQueue,
	process_block_add_one<typename OutputQueue::Element, typename InputQueue::Element, blocksize>,
	blocksize>
{
};

struct cooperative_scheduler {};

TEST(cooperative_scheduler, single_element_queue)
{
	dyplo::SingleElementQueue<int, dyplo::CooperativeScheduler> input;
	dyplo::SingleElementQueue<int, dyplo::NoopScheduler> output;

	AddOne<typeof(input), typeof(output), 1> proc;
	proc.set_input(&input);
	proc.set_output(&output);

	YAFFUT_CHECK(input.empty());
	YAFFUT_CHECK(output.empty());

	input.push_one(42);
	YAFFUT_EQUAL(1u, output.size());
	YAFFUT_CHECK(input.empty());
	YAFFUT_EQUAL(43, output.pop_one());
	YAFFUT_CHECK(output.empty());
}

TEST(cooperative_scheduler, fixed_memory_queue_multi)
{
	dyplo::FixedMemoryQueue<int, dyplo::CooperativeScheduler> input(2);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output(2);

	AddOne<typeof(input), typeof(output), 2> proc;
	proc.set_input(&input);
	proc.set_output(&output);

	YAFFUT_CHECK(input.empty());
	YAFFUT_CHECK(output.empty());

	input.push_one(42);
	YAFFUT_CHECK(output.empty());
	input.push_one(43);
	YAFFUT_EQUAL(2u, output.size());
	YAFFUT_CHECK(input.empty());

	YAFFUT_EQUAL(43, output.pop_one());
	YAFFUT_EQUAL(44, output.pop_one());
	YAFFUT_CHECK(output.empty());
}

TEST(cooperative_scheduler, multiple_processes_and_queues)
{
	dyplo::SingleElementQueue<int, dyplo::CooperativeScheduler> input_to_a;
	dyplo::FixedMemoryQueue<short, dyplo::CooperativeScheduler> between_a_and_b(1);
	dyplo::FixedMemoryQueue<int, dyplo::NoopScheduler> output_from_b(2);

	AddOne<typeof(input_to_a), typeof(between_a_and_b)> a;
	AddOne<typeof(between_a_and_b), typeof(output_from_b)> b;

	a.set_input(&input_to_a);
	a.set_output(&between_a_and_b);
	b.set_input(&between_a_and_b);
	b.set_output(&output_from_b);

	input_to_a.push_one(50);
	YAFFUT_EQUAL(52, output_from_b.pop_one());
	input_to_a.push_one(51);
	YAFFUT_EQUAL(53, output_from_b.pop_one());
	input_to_a.push_one(32767); /* Cause overflow in short */
	YAFFUT_EQUAL(-32767, output_from_b.pop_one());

	YAFFUT_EQUAL(0u, input_to_a.size());
	YAFFUT_EQUAL(0u, between_a_and_b.size());
	YAFFUT_EQUAL(0u, output_from_b.size());

	input_to_a.push_one(60);
	YAFFUT_EQUAL(0u, input_to_a.size());
	YAFFUT_EQUAL(0u, between_a_and_b.size());
	YAFFUT_EQUAL(1u, output_from_b.size());
	input_to_a.push_one(61);
	YAFFUT_EQUAL(2u, output_from_b.size());
}
