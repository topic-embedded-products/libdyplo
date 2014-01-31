#define YAFFUT_MAIN
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
		if (candidates == 0) FAIL("No testNode available");
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
					control.disableNode(id);
					std::string filename =
							context.findPartition(function_name, id);
					context.setProgramMode(true); /* partial */
					context.program(filename.c_str());
					control.enableNode(id);
					//std::cerr << __func__ << " handle=" << handle << " id=" << id << std::endl;
					nodes.push_back(new StressNode(id, handle));
				}
			}
		}
		std::cout << " (" << nodes.size() << " nodes)";
	}

	int openAvailableFifo(int* id, int access)
	{
		for (int index = 0; index < 32; ++index)
		{
			int result = context.openFifo(index, access);
			if (result != -1)
			{
				*id = index;
				return result;
			}
			if (errno != EBUSY)
				throw IOException();
		}
		throw IOException(ENODEV);
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
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		control.routeDelete(node->id);
		int to_cpu_id;
		node->reset();
		dyplo::File to_cpu(openAvailableFifo(&to_cpu_id, O_RDONLY));
		control.routeAddSingle(node->id, 0, 0, to_cpu_id);
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(1, 1024*1024, 1));
		node->enable(1);
		node->start(1);
		unsigned int data[256];
		EQUAL((ssize_t)sizeof(data), to_cpu.read(data, sizeof(data)));
		node->enable(0);
		for (unsigned int i = 0; i < (sizeof(data)/sizeof(data[0])); ++i)
		{
			if (data[i] != i)
			{
				std::ostringstream msg;
				msg << "Node " << (it - nodes.begin())
					<< " i=" << i << " data=" << data[i] << " " << data[i+1]; 
				FAIL(msg.str());
			}
		}
	}
}

TEST(Stress, a_from_cpu)
{
	CHECK(nodes.size() != 0);
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		control.routeDelete(node->id);
		int from_cpu_id;
		node->reset();
		dyplo::File from_cpu(openAvailableFifo(&from_cpu_id, O_WRONLY));
		control.routeAddSingle(0, from_cpu_id, node->id, 0);
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(1, 1024*1024, 1));
		node->enable(1);
		node->start(1);
		unsigned int data[256];
		for (unsigned int i = 0; i < (sizeof(data)/sizeof(data[0])); ++i)
			data[i] = i;
		EQUAL((ssize_t)sizeof(data), from_cpu.write(data, sizeof(data)));
		node->enable(0);
		EQUAL(0u, node->error());
	}
}

TEST(Stress, b_loop_to_self_single)
{
	CHECK(nodes.size() != 0);
	for (StressNodeList::iterator it = nodes.begin();	it != nodes.end(); ++it)
	{
		StressNode* node = *it;
		node->reset();
		EQUAL(0u, node->error()); /* Reset must clear error flag */
		control.routeDelete(node->id);
		node->reset_lane(0);
		node->set_lane_params(0, StressNodeLane(3, 2, 1));
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
