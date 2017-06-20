/*
 * testdyplostress.cpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2015 Topic Embedded Products B.V. (http://www.topic.nl).
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

#ifndef __rtems__
#define YAFFUT_MAIN
#endif
#include "yaffut.h"

#include "hardware.hpp"
#include <errno.h>
#include <vector>

using dyplo::IOException;
using dyplo::File;

static const char* function_name = "testNode";

struct StressNodeLane /* At offset (lane * 0x40) + 0x4 */
{
	unsigned int burst_interval;
	unsigned int burst_length;
	unsigned int output_stepsize;
	unsigned int input_stepsize;

	StressNodeLane(unsigned int a_burst_interval, unsigned int a_burst_length = 1, unsigned int stepsize = 1):
		burst_interval(a_burst_interval),
		burst_length(a_burst_length),
		output_stepsize(stepsize),
		input_stepsize(stepsize)
	{}

};

class StressNode: public dyplo::File
{
public:
	int id;
	StressNode(int node_id, int handle):
		dyplo::File(handle),
		id(node_id)
	{}
	void reset()
	{
		unsigned int command = 1;
		seek(0x04);
		write(&command, sizeof(command));
	}
	void reset_lane(int lane)
	{
		unsigned int command = 1;
		seek((0x40 * lane) + 0x40);
		write(&command, sizeof(command));
	}
	void set_lane_params(unsigned int lane, const StressNodeLane& settings)
	{
		seek((0x40 * lane) + 0x44);
		write(&settings, sizeof(settings));
	}
	void enable(unsigned int enabled = 1)
	{
		seek(0x00);
		write(&enabled, sizeof(enabled));
	}
	void start(unsigned int lane_mask)
	{
		seek(0x08);
		write(&lane_mask, sizeof(lane_mask));
	}
	void stop(unsigned int lane_mask)
	{
		seek(0x0C);
		write(&lane_mask, sizeof(lane_mask));
	}
	unsigned int error()
	{
		unsigned int result;
		seek(0x14);
		EQUAL((ssize_t)sizeof(result), read(&result, sizeof(result)));
		seek(0x14);
		EQUAL((ssize_t)sizeof(result), read(&result, sizeof(result)));
		return result;
	}
};

typedef std::vector<StressNode*> StressNodeList;

struct Stress
{
	dyplo::HardwareContext context;
	dyplo::HardwareControl control;
	StressNodeList nodes;
	Stress():
		context(),
		control(context)
	{
		unsigned int candidates = context.getAvailablePartitions(function_name);
		if (candidates == 0)
			FAIL("No testNode available");
		control.disableNodes(candidates);
		{
			dyplo::HardwareProgrammer programmer(context, control);
			unsigned int mask = 1;
			for (int id = 1; id < 32; ++id)
			{
				mask <<= 1;
				if ((mask & candidates) != 0)
				{
					int handle = context.openConfig(id, O_RDWR);
					if (handle == -1)
					{
						if (errno != EBUSY) /* Non existent? Bail out, last node */
							break;
					}
					else
					{
						std::string filename =
								context.findPartition(function_name, id);
						programmer.fromFile(filename.c_str());
						//std::cerr << __func__ << " handle=" << handle << " id=" << id << std::endl;
						nodes.push_back(new StressNode(id, handle));
					}
				}
			}
		}
		/* Enabling nodes must be postponed until AFTER closing the
		 * programmer object. */
		control.enableNodes(candidates);
		std::cout << " (" << nodes.size() << " nodes)";
	}

	int openAvailableFifo(int access)
	{
		for (int index = 0; index < 32; ++index)
		{
			int result = context.openFifo(index, access);
			if (result != -1)
				return result;
			if (errno != EBUSY)
				throw IOException();
		}
		throw IOException(ENODEV);
	}

	int openAvailableDMA(int access)
	{
		return context.openAvailableDMA(access);
	}

	~Stress()
	{
		for (StressNodeList::iterator it = nodes.begin();
			it != nodes.end(); ++it)
		{
			delete *it;
		}
	}
};

static const unsigned int nr_of_lanes = 4;

TEST(Stress, a_to_cpu)
{
	CHECK(nodes.size() != 0);
	dyplo::HardwareFifo to_cpu(openAvailableFifo(O_RDONLY));
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		control.routeDelete(node->id);
		node->reset();
		to_cpu.reset(); /* Clear FIFO */
		to_cpu.addRouteFrom(node->id);
		CHECK(! to_cpu.poll_for_incoming_data(0) );
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(0));
		node->enable(1);
		node->start(1);
		unsigned int data[256];
		unsigned int value = 0;
		for (unsigned int repeat = 4096; repeat != 0; --repeat)
		{
			EQUAL((ssize_t)sizeof(data), to_cpu.read(data, sizeof(data)));
			for (unsigned int i = 0; i < (sizeof(data)/sizeof(data[0])); ++i)
			{
				if (data[i] != value)
				{
					std::ostringstream msg;
					msg << "Node " << (it - nodes.begin())
						<< " expected=" << value << " data=" << data[i] << " " << data[i+1];
					FAIL(msg.str());
				}
				++value;
			}
		}
		node->enable(0);
		to_cpu.reset(); /* Clear FIFO for future tests */
	}
}

TEST(Stress, a_from_cpu)
{
	CHECK(nodes.size() != 0);
	dyplo::HardwareFifo from_cpu(openAvailableFifo(O_WRONLY|O_APPEND));
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		control.routeDelete(node->id);
		node->reset();
		from_cpu.addRouteTo(node->id);
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(0));
		node->enable(1);
		node->start(1);
		unsigned int data[256];
		unsigned int value = 0;
		for (unsigned int repeat = 4096; repeat != 0; --repeat)
		{
			for (unsigned int i = 0; i < (sizeof(data)/sizeof(data[0])); ++i)
			{
				data[i] = value;
				++value;
			}
			EQUAL((ssize_t)sizeof(data), from_cpu.write(data, sizeof(data)));
			EQUAL(0u, node->error());
		}
		node->enable(0);
		EQUAL(0u, node->error());
	}
}

TEST(Stress, d_dma_to_logic)
{
	CHECK(nodes.size() != 0);
	dyplo::HardwareFifo from_cpu(openAvailableDMA(O_WRONLY|O_APPEND));
	for (StressNodeList::iterator it = nodes.begin(); it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		control.routeDelete(node->id);
		node->reset();
		from_cpu.addRouteTo(node->id);
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(0));
		node->enable(1);
		node->start(1);

		unsigned int dmaBufferSize = from_cpu.getDataTreshold() >> 1;
		/* Expect a sensible size, even 1k is rediculously small, but still
		 * bigger than what the (non-DMA) CPU node would support */
		CHECK(dmaBufferSize > 1024);
		unsigned int dmaBufferSizeWords = dmaBufferSize >> 2;
		std::vector<unsigned int> data(dmaBufferSizeWords);
		unsigned int value = 0;
		for (unsigned int repeat = 16; repeat != 0; --repeat)
		{
			for (unsigned int i = 0; i < dmaBufferSizeWords; ++i)
			{
				data[i] = value;
				++value;
			}
			EQUAL(dmaBufferSize, from_cpu.write(&data[0], dmaBufferSize));
			EQUAL(0u, node->error());
		}
		// Clumsy way to flush the data, just sleep a second...
		sleep(1);
		node->enable(0);
		EQUAL(0u, node->error());
	}
}

TEST(Stress, d_dma_from_logic)
{
	CHECK(nodes.size() != 0);
	dyplo::HardwareFifo to_cpu(openAvailableDMA(O_RDONLY));
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		control.routeDelete(node->id);
		node->reset();
		to_cpu.reset(); /* Clear FIFO */
		to_cpu.addRouteFrom(node->id);
		CHECK(! to_cpu.poll_for_incoming_data(0) );
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(0));
		node->enable(1);
		node->start(1);
		unsigned int dmaBufferSize = to_cpu.getDataTreshold() >> 1;
		/* Expect a sensible size, even 1k is rediculously small, but still
		 * bigger than what the (non-DMA) CPU node would support */
		CHECK(dmaBufferSize > 1024);
		unsigned int dmaBufferSizeWords = dmaBufferSize >> 2;
		std::vector<unsigned int> data(dmaBufferSizeWords);
		unsigned int value = 0;
		for (unsigned int repeat = 16; repeat != 0; --repeat)
		{
			EQUAL(dmaBufferSize, to_cpu.read(&data[0], dmaBufferSize));
			for (unsigned int i = 0; i < dmaBufferSizeWords; ++i)
			{
				if (data[i] != value)
				{
					std::ostringstream msg;
					msg << "Node " << (it - nodes.begin())
						<< " expected=" << value << " data=" << data[i] << " " << data[i+1];
					FAIL(msg.str());
				}
				++value;
			}
		}
		node->enable(0);
		to_cpu.reset(); /* Clear FIFO for future tests */
	}
}


TEST(Stress, b_loop_to_self_single)
{
	CHECK(nodes.size() != 0);
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		unsigned int seed = it - nodes.begin();
		node->reset();
		EQUAL(0u, node->error()); /* Reset must clear error flag */
		control.routeDelete(node->id);
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(1 + (2*seed), 1 + (3*seed), 1));
		control.routeAddSingle(node->id, 0, node->id, 0);
		node->enable();
		node->start(1);
	}
	sleep(1); /* Run for 1 second */
	bool all_nodes_okay = true;
	std::ostringstream msg;
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		node->enable(0);
		unsigned int err = node->error();
		if (err)
		{
			all_nodes_okay = false;
			msg << "Node " << (it - nodes.begin()) << " error flag: " << err << '\n';
		}
		//node->reset();
	}
	if(!all_nodes_okay)
		FAIL(msg.str());
}
