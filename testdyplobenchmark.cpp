/*
 * testdyplodriver.cpp
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
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <stdio.h>
#include "hardware.hpp"

#define YAFFUT_MAIN
#include "yaffut.h"

#include <time.h>

class Stopwatch
{
public:
	struct timespec m_start;
	struct timespec m_stop;

	Stopwatch()
	{
		clock_gettime(CLOCK_MONOTONIC, &m_start);
	}

	void start()
	{
		clock_gettime(CLOCK_MONOTONIC, &m_start);
	}

	void stop()
	{
		clock_gettime(CLOCK_MONOTONIC, &m_stop);
	}

	unsigned int elapsed_us()
	{
		return
			((m_stop.tv_sec - m_start.tv_sec) * 1000000) +
				(m_stop.tv_nsec - m_start.tv_nsec) / 1000;
	}
};

struct hardware_driver_ctx
{
	dyplo::HardwareContext context;
	~hardware_driver_ctx()
	{
		try
		{
			dyplo::HardwareControl ctrl(context);
			ctrl.routeDeleteAll();
		}
		catch (const std::exception& ex)
		{
			std::cout << "ERROR in cleanup: " << ex.what() << std::endl;
		}
	}
};

static const unsigned int benchmark_block_sizes[] = {
	4096, 8192, 16384, 32*1024, 64*1024, 128*1024, 256*1024, 1024*1024, 4096*1024,
};

static const char *MODE_NAME[] = {
	"",
	"RINGBUFFER",
	"  COHERENT",
	" STREAMING",
};
TEST(hardware_driver_ctx, dma_zerocopy_benchmark)
{
	static const int dma_index = 0;
	for (unsigned int mode = dyplo::HardwareDMAFifo::MODE_COHERENT;
		mode <= dyplo::HardwareDMAFifo::MODE_STREAMING; ++mode)
	{
		std::cout << "\n" << MODE_NAME[mode];
	for (unsigned int blocksize_index = 0;
		blocksize_index < sizeof(benchmark_block_sizes)/sizeof(benchmark_block_sizes[0]);
		++blocksize_index)
	{
		unsigned int blocksize = benchmark_block_sizes[blocksize_index];
		/* Don't use streaming mode over 256k */
		if (mode == dyplo::HardwareDMAFifo::MODE_STREAMING &&
		    blocksize > 256*1024)
			break;
		static const unsigned int num_blocks = 2;
		std::cout << ' ' << (blocksize>>10) << "k:";
		dyplo::HardwareDMAFifo::Block *block;
		dyplo::HardwareDMAFifo dma0r(context.openDMA(dma_index, O_RDONLY));
		dma0r.reconfigure(mode, blocksize, num_blocks, true);
		EQUAL(num_blocks, dma0r.count());
		/* For memory mapping to work on a writeable device, you have to open it in R+W mode */
		dyplo::HardwareDMAFifo dma0w(context.openDMA(dma_index, O_RDWR));
		dma0w.reconfigure(mode, blocksize, num_blocks, false);
		EQUAL(num_blocks, dma0w.count());
		dma0r.addRouteFrom(dma0w.getNodeAndFifoIndex());
		/* Prime the reader */
		unsigned int nonzero = 0;
		for (unsigned int i = 0; i < num_blocks; ++i)
		{
			block = dma0r.dequeue();
			EQUAL(blocksize, block->size);
			block->bytes_used = block->size;
			/* Touch all memory to bring it into cache */
			unsigned int* data = (unsigned int*)block->data;
			for (unsigned int j = 0; j < block->size/sizeof(unsigned int); ++j)
				if (data[j] != 0)
					++nonzero;
			dma0r.enqueue(block);
		}
		/* Bogus check to make sure that nonzero does not get optimized away */
		CHECK(nonzero <= num_blocks*blocksize/sizeof(unsigned int));
		/* Touch all memory to bring it into cache */
		for (unsigned int i = 0; i < num_blocks; ++i)
		{
			unsigned int* data = (unsigned int*)dma0w.at(i)->data;
			for (unsigned int j = 0; j < blocksize/sizeof(unsigned int); ++j)
				data[j] = (i << 24) | j;
		}
		Stopwatch timer;
		timer.start();
		/* Send data */
		for (unsigned int i = 0 ; i < num_blocks; ++i)
		{
			block = dma0w.dequeue();
			block->bytes_used = block->size;
			dma0w.enqueue(block);
		}
		unsigned int total_received = 0;
		for (unsigned int i = (48*1024*1024)/blocksize; i != 0; --i)
		{
			block = dma0w.dequeue();
			block->bytes_used = block->size;
			dma0w.enqueue(block);
			block = dma0r.dequeue();
			total_received += block->bytes_used;
			block->bytes_used = block->size;
			dma0r.enqueue(block);
		}
		for (unsigned int i = 0 ; i < num_blocks; ++i)
		{
			block = dma0r.dequeue();
			total_received += block->bytes_used;
		}
		timer.stop();
		std::cout << (total_received/timer.elapsed_us()) << std::flush;
	}
	}
	std::cout << " (MB/s) ";
}

TEST(hardware_driver_ctx, dma_bounce_benchmark)
{
	static const unsigned int max_blocksize_index = 5; /* don't go over 64k */
	static const unsigned int num_blocks = 4;
	static const int dma_index = 0;
	std::vector<char> write_buffer(
				benchmark_block_sizes[max_blocksize_index - 1]);
	std::vector<char> read_buffer(
				benchmark_block_sizes[max_blocksize_index - 1]);
	std::cout << "\n" << MODE_NAME[1]; /* ringbuffer */
	for (unsigned int blocksize_index = 0;
		blocksize_index < max_blocksize_index;
		++blocksize_index)
	{
		unsigned int blocksize = benchmark_block_sizes[blocksize_index];
		std::cout << ' ' << (blocksize>>10) << "k:";
		dyplo::HardwareFifo dma0r(context.openDMA(dma_index, O_RDONLY));
		dma0r.reset();
		dma0r.setDataTreshold(blocksize);
		dyplo::HardwareFifo dma0w(context.openDMA(dma_index, O_WRONLY));
		dma0w.reset();
		dma0r.addRouteFrom(dma0w.getNodeAndFifoIndex());

		/* Touch all memory to bring it into cache */
		memset(&read_buffer[0], 0xDEADBEEF, blocksize);
		unsigned int* data = (unsigned int*)&write_buffer[0];
		for (unsigned int j = 0; j < blocksize/sizeof(unsigned int); ++j)
			data[j] = j;
		unsigned int total_received = 0;
		Stopwatch timer;
		timer.start();
		/* Send data */
		for (unsigned int i = 0 ; i < num_blocks; ++i) {
			size_t bytes = dma0w.write(&write_buffer[0], blocksize);
			EQUAL(blocksize, bytes);
		}
		for (unsigned int i = (16*1024*1024)/blocksize; i != 0; --i)
		{
			ssize_t bytes = dma0r.read(&read_buffer[0], blocksize);
			EQUAL(blocksize, bytes);
			total_received += bytes;
			bytes = dma0w.write(&write_buffer[0], blocksize);
			EQUAL(blocksize, bytes);
		}
		for (unsigned int i = 0 ; i < num_blocks; ++i)
		{
			ssize_t bytes = dma0r.read(&read_buffer[0], blocksize);
			EQUAL(blocksize, bytes);
			total_received += bytes;
		}
		timer.stop();
		std::cout << (total_received/timer.elapsed_us()) << std::flush;
	}
	std::cout << " (MB/s) ";
}
