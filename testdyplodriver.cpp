/*
 * testdyplodriver.cpp
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
#include <unistd.h>
#include <errno.h>
#include "fileio.hpp"
#include "filequeue.hpp"
#include "thread.hpp"
#include "condition.hpp"
#include "mutex.hpp"
#include "hardware.hpp"

#define YAFFUT_MAIN
#include "yaffut.h"

/* In future, retrieve number of fifo's and nodes from driver. */
#define DYPLO_CPU_FIFO_COUNT	12

/* Define "eternity" as a 5 second wait */
#define VERY_LONG_TIMEOUT_US	5000000

using dyplo::File;

static int openFifo(int fifo, int access)
{		
	std::ostringstream name;
	if (access == O_RDONLY)
		name << "/dev/dyplor";
	else if (access == O_WRONLY)
		name << "/dev/dyplow";
	else
		FAIL("bad access");
	name << fifo;
	return ::open(name.str().c_str(), access);
}

static void hardware_driver_clean_all_routes()
{
	try
	{
		/* Clean all routes */
		dyplo::HardwareContext ctx;
		dyplo::HardwareControl ctrl(ctx);
		ctrl.routeDeleteAll();
	}
	catch (const std::exception& ex)
	{
		std::cout << "ERROR in cleanup: " << ex.what() << std::endl;
	}
}

struct hardware_driver 
{
};

struct hardware_driver_hdl
{
	dyplo::HardwareContext context;
	hardware_driver_hdl()
	{
		context.setProgramMode(true); /* partial */
		for (int id = 1; id <= 4; ++id)
		{
			std::string filename =
				context.findPartition("adder", id);
			if (filename.empty())
				std::cerr << "No 'adder' logic found for node " << id;
			context.program(filename.c_str());
		}
	}
	~hardware_driver_hdl()
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

TEST(hardware_driver, a_control_multiple_open)
{
	dyplo::HardwareContext ctrl1;
	dyplo::HardwareContext ctrl2;
}

TEST(hardware_driver, b_config_single_open)
{
	dyplo::HardwareContext ctrl;
	File cfg0r(ctrl.openConfig(0, O_RDONLY));
	File cfg1r(ctrl.openConfig(1, O_RDONLY)); /* Other cfg can be opened */
	ASSERT_THROW(File another_cfg0r(ctrl.openConfig(0, O_RDONLY)), dyplo::IOException);
	/* Can open same file with other access rights */
	File cfg0w(ctrl.openConfig(0, O_WRONLY));
	/* But cannot open it twice either */
	ASSERT_THROW(File another_cfg0w(ctrl.openConfig(0, O_WRONLY)), dyplo::IOException);
	/* Open for both R/W */
	File cfg3rw(ctrl.openConfig(2, O_RDWR));
	ASSERT_THROW(File another_cfg3rw(ctrl.openConfig(2, O_RDONLY)), dyplo::IOException);
	ASSERT_THROW(File another_cfg3rw(ctrl.openConfig(2, O_WRONLY)), dyplo::IOException);
}

TEST(hardware_driver, c_fifo_single_open_rw_access)
{
	dyplo::HardwareContext ctrl;
	File r0(ctrl.openFifo(0, O_RDONLY));
	File r1(ctrl.openFifo(1, O_WRONLY)); /* Other fifo can be opened */
	ASSERT_THROW(File another_r0(ctrl.openFifo(0, O_RDONLY)), dyplo::IOException);
	ASSERT_THROW(File another_r1(ctrl.openFifo(1, O_WRONLY)), dyplo::IOException);
	/* Cannot open input fifo for writing */
	ASSERT_THROW(File r2w("/dev/dyplor2", O_WRONLY), dyplo::IOException);
	/* Cannot open output fifo for reading */
	ASSERT_THROW(File w0r("/dev/dyplow0", O_RDONLY), dyplo::IOException);
}

static void connect_all_fifos_in_loop()
{
	dyplo::HardwareContext ctrl;
	dyplo::HardwareControl::Route routes[DYPLO_CPU_FIFO_COUNT];
	for (int i = 0; i < DYPLO_CPU_FIFO_COUNT; ++i) {
		routes[i].srcNode = 0;
		routes[i].srcFifo = i;
		routes[i].dstNode = 0;
		routes[i].dstFifo = i;
	}
	dyplo::HardwareControl(ctrl).routeAdd(routes, DYPLO_CPU_FIFO_COUNT);
}

TEST(hardware_driver_hdl, d_io_control_route)
{
	dyplo::HardwareContext ctx;
	dyplo::HardwareControl::Route routes[64];
	dyplo::HardwareControl ctrl(ctx);
	/* Clean all routes */
	ctrl.routeDeleteAll();
	int n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(n_routes, 0);
	/* Set up single route */
	ctrl.routeAddSingle(0,0,0,0);
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(1, n_routes);
	EQUAL(0, (int)routes[0].srcNode);
	EQUAL(0, (int)routes[0].srcFifo);
	EQUAL(0, (int)routes[0].dstNode);
	EQUAL(0, (int)routes[0].dstFifo);
	ctrl.routeAddSingle(0,1,0,1);
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(2, n_routes);
	EQUAL(0, (int)routes[1].srcNode);
	EQUAL(1, (int)routes[1].srcFifo);
	EQUAL(0, (int)routes[1].dstNode);
	EQUAL(1, (int)routes[1].dstFifo);
	ctrl.routeAddSingle(0,1,0,2); /* Overwrites existing route */
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(2, n_routes);
	EQUAL(0, (int)routes[1].srcNode);
	EQUAL(1, (int)routes[1].srcFifo);
	EQUAL(0, (int)routes[1].dstNode);
	EQUAL(2, (int)routes[1].dstFifo);
	ctrl.routeAddSingle(0,1,0,1);
	/* Start over */
	ctrl.routeDeleteAll();
	ctrl.routeAddSingle(2,1,0,4);
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(1, n_routes);
	EQUAL(2, (int)routes[0].srcNode);
	EQUAL(1, (int)routes[0].srcFifo);
	EQUAL(0, (int)routes[0].dstNode);
	EQUAL(4, (int)routes[0].dstFifo);
	ctrl.routeAddSingle(2,0,1,3);
	ctrl.routeAddSingle(1,0,0,5);
	ctrl.routeAddSingle(1,1,2,2);
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(4, n_routes);
	/* Remove node */
	ctrl.routeDelete(2);
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(1, n_routes); /* Only 1,0,0,5 must remain */
	EQUAL(1, (int)routes[0].srcNode);
	EQUAL(0, (int)routes[0].srcFifo);
	EQUAL(0, (int)routes[0].dstNode);
	EQUAL(5, (int)routes[0].dstFifo);
	ctrl.routeAddSingle(2,0,1,0);
	ctrl.routeAddSingle(0,0,1,1);
	ctrl.routeAddSingle(0,1,2,1);
	ctrl.routeAddSingle(2,1,0,1);
	ctrl.routeAddSingle(1,1,2,0);
	ctrl.routeDelete(1);
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(2, n_routes); /* Only 2,1,0,1 and 0,1,2,1 must remain */
	/* Setup loopback system (1->0, 1->1 etc) */
	connect_all_fifos_in_loop();
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(DYPLO_CPU_FIFO_COUNT, n_routes);
	for (int fifo=0; fifo<DYPLO_CPU_FIFO_COUNT; ++fifo)
	{
		EQUAL(0, routes[fifo].srcNode);
		EQUAL(fifo, routes[fifo].srcFifo);
		EQUAL(0, routes[fifo].dstNode);
		EQUAL(fifo, routes[fifo].dstFifo);
	}
}

TEST(hardware_driver, d_io_control_backplane)
{
	dyplo::HardwareContext ctx;
	dyplo::HardwareControl ctrl(ctx);
	
	ctrl.enableNode(0);
	CHECK(ctrl.isNodeEnabled(0));
	ctrl.disableNode(1);
	CHECK(!ctrl.isNodeEnabled(1));
	CHECK(ctrl.isNodeEnabled(0));
	ctrl.enableNode(1);
	CHECK(ctrl.isNodeEnabled(1));
	ctrl.disableNode(2);
	CHECK(!ctrl.isNodeEnabled(2));
	CHECK(ctrl.isNodeEnabled(1));
	CHECK(ctrl.isNodeEnabled(0));
	ctrl.enableNode(2);
	CHECK(ctrl.isNodeEnabled(2));
	
	dyplo::HardwareConfig cfg1(ctx, 1);
	CHECK(cfg1.isNodeEnabled());
	dyplo::HardwareConfig cfg2(ctx, 2);
	CHECK(cfg2.isNodeEnabled());
	cfg1.disableNode();
	CHECK(!cfg1.isNodeEnabled());
	CHECK(ctrl.isNodeEnabled(0));
	CHECK(!ctrl.isNodeEnabled(1));
	CHECK(ctrl.isNodeEnabled(2));
	cfg1.enableNode();
	cfg2.disableNode();
	CHECK(cfg1.isNodeEnabled());
	CHECK(!cfg2.isNodeEnabled());
	cfg2.enableNode();
	CHECK(cfg2.isNodeEnabled());
}

TEST(hardware_driver, d_io_control_fifo_reset)
{
	dyplo::HardwareContext ctx;
	dyplo::HardwareControl ctrl(ctx);
	
	{
		dyplo::HardwareConfig cfg_cpu(ctx, 0); /* open CPU node */
		ctrl.routeAddSingle(0,0,0,0);
		File fifo_in(openFifo(0, O_RDONLY));
		File fifo_out(openFifo(0, O_WRONLY));
		int data[16];
		for (int i = 0 ; i < 16; ++i) data[i] = 0x11 << i;
		fifo_out.write(data, sizeof(data));
		// Check data availabe
		CHECK(fifo_in.poll_for_incoming_data(1) );
		cfg_cpu.resetReadFifos(0x1);
		cfg_cpu.resetWriteFifos(0x1);
		// Data is gone now
		CHECK(! fifo_in.poll_for_incoming_data(0) );
		data[0] = 0x12345678;
		// Flush any orphans
		fifo_out.write(data, sizeof(int));
		do
		{
			CHECK(fifo_in.poll_for_incoming_data(1));
			fifo_in.read(data, sizeof(int));
		}
		while (data[0] != 0x12345678);
	}

	{
		ctrl.routeAddSingle(0,2,0,1);
		dyplo::HardwareFifo fifo_in(openFifo(1, O_RDONLY));
		dyplo::HardwareFifo fifo_out(openFifo(2, O_WRONLY));
		int data[16];
		for (int i = 0 ; i < 16; ++i) data[i] = 0x101 << i;
		fifo_out.write(data, sizeof(data));
		// Check data availabe
		CHECK(fifo_in.poll_for_incoming_data(1));
		fifo_out.reset();
		fifo_in.reset();
		// Data is gone now
		CHECK(! fifo_in.poll_for_incoming_data(0) );
		// Flush any orphans
		data[0] = 0x12345678;
		fifo_out.write(data, sizeof(int));
		do
		{
			CHECK(fifo_in.poll_for_incoming_data(1));
			fifo_in.read(data, sizeof(int));
		}
		while (data[0] != 0x12345678);
	}

	ctrl.routeDeleteAll();
}


TEST(hardware_driver, e_transmit_loop)
{
	connect_all_fifos_in_loop();
	for (int fifo = 0; fifo < DYPLO_CPU_FIFO_COUNT; ++fifo)
	{
		File fifo_in(openFifo(fifo, O_RDONLY));
		File fifo_out(openFifo(fifo, O_WRONLY));
		/* Future todo: Set up routing... */
		/* Assert that input is empty */
		CHECK(! fifo_in.poll_for_incoming_data(0) );
		int buffer[16];
		int received[16];
		for (int repeats = 4; repeats != 0; --repeats)
		{
			for (unsigned int i=0; i<sizeof(buffer)/sizeof(buffer[0]); ++i)
				buffer[i] = i + (1000 * i * repeats);
			int bytes = write(fifo_out.handle, buffer, sizeof(buffer));
			EQUAL((ssize_t)sizeof(buffer), bytes);
			bytes = read(fifo_in.handle, received, bytes);
			EQUAL((ssize_t)sizeof(buffer), bytes);
			for (unsigned int i=0; i<sizeof(buffer)/sizeof(buffer[0]); ++i)
			{
				if ((int)(i + (repeats * 1000 * i)) != received[i])
				{
					std::ostringstream msg;
					msg << "mismatch, fifo: " << fifo
						<< " repeat: " << (4-repeats)
						<< " sample: " << i
						<< " expected: " << (int)(i + (repeats * 1000 * i))
						<< " actual: " << received[i];
					FAIL(msg.str());
				}
			}
		}
	}
}

ssize_t fill_fifo_to_the_brim(File &fifo_out, int signature = 0, int timeout_us = 0)
{
	const int blocksize = 1024;
	/* Fill it until it is full */
	int max_repeats = 256;
	int* buffer = new int[blocksize];
	ssize_t total_written = 0;
	
	while (--max_repeats)
	{
		for (int i = 0; i < blocksize; ++i)
			buffer[i] = ((total_written >> 2) + i) + signature;
		ssize_t written = fifo_out.write(buffer, blocksize*sizeof(buffer[0]));
		total_written += written;
		
		if (written < (ssize_t)(blocksize*sizeof(buffer[0])))
		{
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = timeout_us;
			if (!fifo_out.poll_for_outgoing_data(&timeout))
				break;
		}
	}
	delete [] buffer;
	
	if (max_repeats <= 0)
	{
		std::ostringstream msg;
		msg << "Still not full after " << total_written << " bytes";
		FAIL(msg.str());
	}
	CHECK(max_repeats > 0);
	
	return total_written;
}

void hardware_driver_poll_single(int fifo)
{
	File fifo_in(openFifo(fifo, O_RDONLY));
	File fifo_out(openFifo(fifo, O_WRONLY));
	const int signature = 10000000 * (fifo+1);
	int data = 42;
	
	/* Nothing to read yet */
	CHECK(! fifo_in.poll_for_incoming_data(0) );
	
	fifo_out.write(&data, sizeof(data));

	/* Eternity takes 5 seconds on our system */
	CHECK(fifo_in.poll_for_incoming_data(5) );
	
	fifo_in.read(&data, sizeof(data));

	CHECK(! fifo_in.poll_for_incoming_data(0) ); /* is empty */
	
	EQUAL(0, dyplo::set_non_blocking(fifo_in.handle));
	/* Read from empty fifo, must not block but return EAGAIN error */
	ASSERT_THROW(fifo_in.read(&data, sizeof(data)), dyplo::IOException);

	/* write queue must report to be ready (nothing in there, plenty room) */
	CHECK(fifo_out.poll_for_outgoing_data(0));
	
	/* Set output fifo in non-blocking mode */
	EQUAL(0, dyplo::set_non_blocking(fifo_out.handle));

	/* Write until we can't write no more for 2ms */
	ssize_t total_written = fill_fifo_to_the_brim(fifo_out, signature, 2000);
	CHECK(total_written > 0);
	/* The write fifo holds 255 words, the read fifo 1023. So in total,
	 * the fifo's cannot contain more than 5k bytes. If the total number
	 * of bytes is more, it means something goofed up, or the fifo's are
	 * bigger than before this test was written */
	CHECK(total_written < 6000);

	/* Status must indicate that there is no room */
	CHECK(! fifo_out.poll_for_outgoing_data(0));
	CHECK(fifo_in.poll_for_incoming_data(1) ); /*  has data */

	int* buffer = new int[1024];
	ssize_t total_read = 0;

	while (total_read < total_written)
	{
		ssize_t bytes_read;
		try {
			bytes_read = fifo_in.read(buffer, 1024*sizeof(buffer[0]));
		}
		catch (const dyplo::IOException& ex)
		{
			/* Handle EAGAIN errors by waiting for more */
			CHECK(fifo_in.poll_for_incoming_data(1));
			continue;
		}
		const ssize_t ints_read = bytes_read >> 2;
		for (int i = 0; i < ints_read; ++i) {
			EQUAL((total_read >> 2) + i + signature, buffer[i]);
		}
		total_read += bytes_read;
	}

	delete [] buffer;

	EQUAL(total_written, total_read);
	CHECK(! fifo_in.poll_for_incoming_data(0) ); /* is empty */
	CHECK(fifo_out.poll_for_outgoing_data(0)); /* Has room */

	ASSERT_THROW(fifo_in.read(&data, sizeof(data)), dyplo::IOException);
	
}

TEST(hardware_driver, f_poll)
{
	connect_all_fifos_in_loop();
	for (int i = 0; i < DYPLO_CPU_FIFO_COUNT; ++i)
	{
		try
		{
			hardware_driver_poll_single(i);
		}
		catch (const dyplo::IOException& ex)
		{
			std::ostringstream msg;
			msg << "IOException while testing fifo " << i << ": " << ex.what();
			FAIL(msg.str());
		}
	}
}

struct FileContext
{
	File* file;
	dyplo::Condition started;
	dyplo::Mutex mutex;
	bool is_started;
	
	void set()
	{
		dyplo::ScopedLock<dyplo::Mutex> lock(mutex);
		is_started = true;
		started.signal();
	}
	
	void wait()
	{
		dyplo::ScopedLock<dyplo::Mutex> lock(mutex);
		while (!is_started)
			started.wait(mutex);
	}
	
	FileContext(File* f):
		file(f),
		is_started(false)
	{}
};

void* thread_read_data(void* arg)
{
	FileContext* ctx = (FileContext*)arg;
	try
	{
		char buffer[16];
		ctx->set();
		return (void*) ctx->file->read(buffer, sizeof(buffer));
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error in " << __func__ << ": " << ex.what();
		return (void*)-1;
	}
}

void check_all_input_fifos_are_empty()
{
	std::ostringstream msg;
	for (int fifo = 0; fifo < DYPLO_CPU_FIFO_COUNT; ++fifo)
	{
		File fifo_in(openFifo(fifo, O_RDONLY));
		
		if (fifo_in.poll_for_incoming_data(0))
		{
			msg << "Input fifo " << fifo << " is not empty:";
			msg << std::hex;
			dyplo::set_non_blocking(fifo_in.handle);
			int count = 0;
			while (fifo_in.poll_for_incoming_data(0))
			{
				int data;
				fifo_in.read(&data, sizeof(data));
				msg << " 0x" << data;
				if (++count > 10)
				{
					msg << "...";
					break;
				}
			}
			msg << std::dec << " (" << count << ")\n";
		}
	}
	if (msg.tellp() != 0)
		FAIL(msg.str());
}

static void hardware_driver_irq_driven_read_single(int fifo)
{
	dyplo::Thread reader;
	
	File fifo_in(openFifo(fifo, O_RDONLY));
	File fifo_out(openFifo(fifo, O_WRONLY));
	FileContext ctx(&fifo_in);
	reader.start(thread_read_data, &ctx);
	ctx.wait(); /* Block until we're calling "read" on the other thread*/
	char buffer[16];
	EQUAL((ssize_t)sizeof(buffer), fifo_out.write(buffer, sizeof(buffer)));
	void* result;
	EQUAL(reader.join(&result), 0);
	EQUAL((ssize_t)sizeof(buffer), (ssize_t)result);
}

TEST(hardware_driver, g_irq_driven_read)
{
	connect_all_fifos_in_loop();
	for (int fifo = DYPLO_CPU_FIFO_COUNT-1; fifo >= 0; --fifo) /* go back, variation */
		hardware_driver_irq_driven_read_single(fifo);
	check_all_input_fifos_are_empty();
}

static int extra_data_to_write_in_thread[] =
	{0xD4741337, 0x12345678, 0xFEEDB4B3, 0x54FEF00D};

void* thread_write_data(void* arg)
{
	FileContext* ctx = (FileContext*)arg;
	try
	{
		ctx->set();
		return (void*) ctx->file->write(extra_data_to_write_in_thread,
					sizeof(extra_data_to_write_in_thread));
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error in " << __func__ << ": " << ex.what();
		return (void*)-1;
	}
}

void hardware_driver_irq_driven_write_single(int fifo)
{
	ssize_t total_written;
	{
		/* Make it so that the pipe is filled to the brim */
		File fifo_out(openFifo(fifo, O_WRONLY));
		EQUAL(0, dyplo::set_non_blocking(fifo_out.handle));
		CHECK(fifo_out.poll_for_outgoing_data(0)); /* Must have room */
		/* Fill it until it is full */
		total_written = fill_fifo_to_the_brim(fifo_out);
		CHECK(total_written > 0);
		/* Status must indicate that there is no room */
		CHECK(!fifo_out.poll_for_outgoing_data(0));
		/* Close and re-open the output to clear the "non-blocking" flag */
	}

	File fifo_in(openFifo(fifo, O_RDONLY));
	File fifo_out(openFifo(fifo, O_WRONLY));
	FileContext ctx(&fifo_out);
	dyplo::Thread writer;
	
	writer.start(thread_write_data, &ctx);
	ctx.wait(); /* Block until we're calling "write" on the other thread*/
	ssize_t total_read = 0;
	int buffer[256];
	ssize_t bytes = fifo_in.read(buffer, sizeof(buffer));
	for (int i = 0; i < (bytes>>2); ++i)
		EQUAL(buffer[i], (total_read>>2) + i);
	total_read += bytes;
	EQUAL((ssize_t)sizeof(buffer), bytes);
	void* result;
	EQUAL(writer.join(&result), 0);
	EQUAL((ssize_t)sizeof(extra_data_to_write_in_thread), (ssize_t)result);
	/* Drain the buffer */
	while (total_read < total_written)
	{
		int bytes_to_read;
		if (total_written - total_read > (ssize_t)sizeof(buffer))
			bytes_to_read = sizeof(buffer);
		else
			bytes_to_read = total_written - total_read;
		bytes = fifo_in.read(buffer, bytes_to_read);
		CHECK(bytes > 0);
		for (int i = 0; i < (bytes>>2); ++i)
			EQUAL(buffer[i], (total_read>>2) + i);
		total_read += bytes;
	}
	EQUAL(total_written, total_read);
	/* check that the "tail" is also available */
	bytes = fifo_in.read(buffer, sizeof(extra_data_to_write_in_thread));
	EQUAL((ssize_t)sizeof(extra_data_to_write_in_thread), bytes);
	for (unsigned int i = 0; i < sizeof(extra_data_to_write_in_thread)/sizeof(extra_data_to_write_in_thread[0]); ++i)
		EQUAL(extra_data_to_write_in_thread[i], buffer[i]);
}

TEST(hardware_driver, h_irq_driven_write)
{
	connect_all_fifos_in_loop();
	for (int fifo = 0; fifo < DYPLO_CPU_FIFO_COUNT; ++fifo)
		hardware_driver_irq_driven_write_single(fifo);
	check_all_input_fifos_are_empty();
}

TEST(hardware_driver, i_cpu_block_crossbar)
{
	/* Set up weird routing */
	static dyplo::HardwareControl::Route routes[] = {
		{0, 0, 5, 0}, 
		{4, 0, 8, 0},
		{1, 0, DYPLO_CPU_FIFO_COUNT-1, 0},
		{DYPLO_CPU_FIFO_COUNT-1,0, 4, 0},
	};
	dyplo::HardwareContext ctrl;
	dyplo::HardwareControl(ctrl).routeAdd(routes, sizeof(routes)/sizeof(routes[0]));
	int data[] = {0x12345678, 0xDEADBEEF};
	const size_t data_size = sizeof(data)/sizeof(data[0]);
	for (unsigned int i = 0; i < sizeof(routes)/sizeof(routes[0]); ++i)
	{
		int src = routes[i].srcFifo;
		int dst = routes[i].dstFifo;
		File f_src(ctrl.openFifo(src, O_WRONLY));
		File f_dst(ctrl.openFifo(dst, O_RDONLY));
		ssize_t bytes_written = f_src.write(data, sizeof(data));
		EQUAL((ssize_t)sizeof(data), bytes_written);
		if (!f_dst.poll_for_incoming_data(2)) {
			std::ostringstream msg;
			msg << "Routing " << src << " -> " << dst << " didn't work, no data received";
			FAIL(msg.str());
		}
		int buffer[data_size];
		ssize_t bytes_read = f_dst.read(buffer, sizeof(buffer));
		EQUAL((ssize_t)sizeof(buffer), bytes_read);
		for (unsigned int j = 0; j < data_size; ++j)
			EQUAL(data[j], buffer[j]);
	}
}

TEST(hardware_driver_hdl, j_hdl_block_processing)
{
	static const int hdl_configuration_blob[] = {
		1, 10001, -1000, 100
	};
	static dyplo::HardwareControl::Route routes[] = {
		{0, 1, 0, 0}, 
		{0, 0, 0, 1}, /* Fifo 0 to HDL #1 port 0 */
		{1, 2, 1, 0},
		{1, 0, 1, 2}, /* HDL #2 connected to fifo 1 */
	};
	dyplo::HardwareControl(context).routeAdd(routes, sizeof(routes)/sizeof(routes[0]));
	int data[] = {1, 2, 42000, -42000};
	const size_t data_size = sizeof(data)/sizeof(data[0]);
	for (int fifo = 0; fifo < 2; ++fifo)
	{
		/* configure HDL block with coefficients */
		{
			File hdl_config(context.openConfig(fifo+1, O_WRONLY));
			EQUAL((ssize_t)sizeof(hdl_configuration_blob),
				hdl_config.write(hdl_configuration_blob, sizeof(hdl_configuration_blob)));
		}
		/* Write some test data */
		File hdl_in(context.openFifo(fifo, O_WRONLY));
		ssize_t bytes_written = hdl_in.write(data, sizeof(data));
		EQUAL((ssize_t)sizeof(data), bytes_written);
		/* Read results and verify */
		File hdl_out(context.openFifo(fifo, O_RDONLY));
		CHECK(hdl_out.poll_for_incoming_data(2)); /* Must have data */
		int buffer[data_size];
		ssize_t bytes_read = hdl_out.read(buffer, sizeof(buffer));
		EQUAL((ssize_t)sizeof(buffer), bytes_read);
		/* Check results: End result should be "ADD 1" operation */
		for (unsigned int i=0; i < data_size; ++i)
			EQUAL(data[i] + hdl_configuration_blob[fifo], buffer[i]);
	}
	check_all_input_fifos_are_empty();
}

static void run_hdl_test(dyplo::HardwareContext &ctrl, int from_cpu_fifo, int to_cpu_fifo, int total_effect)
{
	{
		/* Write some test data */
		const int data[] = {1, 2, 42000, -42000, 0x12345678, 0x01234567};
		const size_t data_size = sizeof(data)/sizeof(data[0]);
		File hdl_in(ctrl.openFifo(from_cpu_fifo, O_WRONLY));
		ssize_t bytes_written = hdl_in.write(data, sizeof(data));
		EQUAL((ssize_t)sizeof(data), bytes_written);
		/* Read results and verify */
		File hdl_out(ctrl.openFifo(to_cpu_fifo, O_RDONLY));
		CHECK(hdl_out.poll_for_incoming_data(2)); /* Must have data */
		int buffer[data_size];
		ssize_t bytes_read = hdl_out.read(buffer, sizeof(buffer));
		EQUAL((ssize_t)sizeof(buffer), bytes_read);
		/* Check results: End result should be "ADD 1" operation */
		for (unsigned int i = 0; i < data_size; ++i)
			EQUAL(data[i] + total_effect, buffer[i]);
	}
	{
		File fifo_out(ctrl.openFifo(from_cpu_fifo, O_WRONLY));
		const int signature = 0x1000000;
		/* Set output fifo in non-blocking mode */
		EQUAL(0, dyplo::set_non_blocking(fifo_out.handle));
		ssize_t total_written = fill_fifo_to_the_brim(fifo_out, signature, 2000);
		CHECK(total_written > 0);
		CHECK(total_written > 1600); /* At least some queues must fill up */
		/* Status must indicate that there is no room */
		if (fifo_out.poll_for_outgoing_data(0))
		{
			std::ostringstream msg;
			msg << "Logic reports still room available after writing "
				<< total_written << " bytes and having reported that "
				" there was no more room.";
			FAIL(msg.str());
		}
		File fifo_in(ctrl.openFifo(to_cpu_fifo, O_RDONLY));
		CHECK(fifo_in.poll_for_incoming_data(1) ); /*  has data */
		EQUAL(0, dyplo::set_non_blocking(fifo_in.handle));
		const size_t blocksize = 1024*sizeof(int);
		int* buffer = new int[blocksize/sizeof(int)];
		ssize_t total_read = 0;
		while (total_read < total_written)
		{
			int bytes_to_read;
			if (total_written - total_read > (ssize_t)blocksize)
				bytes_to_read = blocksize;
			else
				bytes_to_read = total_written - total_read;
			ssize_t bytes_read;
			try {
				bytes_read = fifo_in.read(buffer, bytes_to_read);
			}
			catch (const dyplo::IOException& ex)
			{
				/* Handle EAGAIN errors by waiting for more */
				if (!fifo_in.poll_for_incoming_data(1)) {
					std::ostringstream msg;
					msg << "No more data: " << ex.what() <<
						" after " << total_read << " of "<<
						total_written << " bytes.";
					FAIL(msg.str());
				}
				continue;
			}
			const ssize_t ints_read = bytes_read >> 2;
			for (int i = 0; i < ints_read; ++i) {
				EQUAL((total_read >> 2) + i + signature + total_effect, buffer[i]);
			}
			total_read += bytes_read;
		}
		delete [] buffer;
		EQUAL(total_written, total_read);
		CHECK(! fifo_in.poll_for_incoming_data(0) ); /* is empty */
		CHECK(fifo_out.poll_for_outgoing_data(0)); /* Has room */
	}
	check_all_input_fifos_are_empty();
}

TEST(hardware_driver_hdl, k_hdl_block_ping_pong)
{
	static const int hdl_configuration_blob[] = {
		1, 101, -1001, 10001
	};
	int total_effect = 0;
	for (unsigned int i = 0; i < sizeof(hdl_configuration_blob)/sizeof(hdl_configuration_blob[0]); ++i)
		total_effect += 2 * hdl_configuration_blob[i];
	/* Set up route: Loop through the HDL blocks four times */
	static dyplo::HardwareControl::Route routes[] = {
		{0, 1, 6, 0}, /* 0.6 -> 1.0 */
		{0, 2, 0, 1}, /* 1.0 -> 2.0 */
		{1, 1, 0, 2}, /* 2.0 -> 1.1 */
		{1, 2, 1, 1}, /* 1.1 -> 2.1 */
		{2, 1, 1, 2}, /* 2.1 -> 1.2 */
		{2, 2, 2, 1}, /* 1.2 -> 2.2 */
		{3, 1, 2, 2}, /* 2.2 -> 1.3 */
		{3, 2, 3, 1}, /* 1.3 -> 2.3 */
		{7, 0, 3, 2}, /* 2.3 -> 0.7 */
	};
	dyplo::HardwareControl(context)
		.routeAdd(routes, sizeof(routes)/sizeof(routes[0]));
	/* configure HDL block with coefficients */
	for (int block = 1; block < 3; ++block)
	{
		try
		{
			File hdl_config(context.openConfig(block, O_WRONLY));
			EQUAL((ssize_t)sizeof(hdl_configuration_blob),
				hdl_config.write(hdl_configuration_blob, sizeof(hdl_configuration_blob)));
		}
		catch (const dyplo::IOException& ex)
		{
			FAIL(ex.what());
		}
	}
	run_hdl_test(context, 6, 7, total_effect);
	check_all_input_fifos_are_empty();
}

TEST(hardware_driver_hdl, l_hdl_block_zig_zag)
{
	static const int hdl_configuration_blob[] = {
		7, -17, 1000001, 10001
	};
	int total_effect = 0;
	for (unsigned int i = 0; i < sizeof(hdl_configuration_blob)/sizeof(hdl_configuration_blob[0]); ++i)
		total_effect += 2 * hdl_configuration_blob[i];
	/* Set up route: Loop through the HDL blocks four times */
	static dyplo::HardwareControl::Route routes[] = {
		{0, 1, 4, 0}, /* 0.4 -> 1.0 */
		{1, 1, 0, 1}, /* -> 1.1 */
		{2, 1, 1, 1}, /* -> 1.2 */
		{3, 1, 2, 1}, /* -> 1.3 */
		{0, 2, 3, 1}, /* -> 2.0 */
		{1, 2, 0, 2}, /* -> 2.1 */
		{2, 2, 1, 2}, /* -> 2.2 */
		{3, 2, 2, 2}, /* -> 2.3 */
		{5, 0, 3, 2}, /* 2.3 -> 0.5 */
	};
	dyplo::HardwareControl(context).routeAdd(routes, sizeof(routes)/sizeof(routes[0]));
	/* configure HDL block with coefficients */
	for (int block = 1; block < 3; ++block)
	{
		try
		{
			File hdl_config(context.openConfig(block, O_WRONLY));
			EQUAL((ssize_t)sizeof(hdl_configuration_blob),
				hdl_config.write(hdl_configuration_blob, sizeof(hdl_configuration_blob)));
		}
		catch (const dyplo::IOException& ex)
		{
			FAIL(ex.what());
		}
	}
	run_hdl_test(context, 4, 5, total_effect);
	check_all_input_fifos_are_empty();
}

static const int how_many_blocks = 1024;

static void* thread_send_many_blocks(void* arg)
{
	dyplo::FileOutputQueue<int> *q = (dyplo::FileOutputQueue<int>*)arg;
	int counter = 1;
	try
	{
		int* data;
		for (int block = 0; block < how_many_blocks; ++block)
		{
			unsigned int count = q->begin_write(data, 2048);
			EQUAL(2048, count);
			for (unsigned int i=0; i<count; ++i)
			{
				data[i] = counter;
				++counter;
			}
			q->end_write(count);
		}
		/* Flush the buffer */
		q->begin_write(data, 2048);
	}
	catch (const std::exception& ex)
	{
		return (void*)1;
	}
	return NULL;
}


TEST(hardware_driver_hdl, m_hdl_audio_style)
{
	static const int hdl_configuration_blob[] = {
		0, 0, 0, 0,
	};
	/* Set up route: Loop through the HDL blocks four times */
	static dyplo::HardwareControl::Route routes[] = {
		{0, 1, 0, 0}, /* 0.0 -> 1.0 */
		{0, 0, 0, 1}, /* 1.0 -> 0.0 */
	};
	dyplo::HardwareControl(context).routeAdd(routes, sizeof(routes)/sizeof(routes[0]));
	/* configure HDL block with coefficients */
	for (int block = 1; block < 3; ++block)
	{
		try
		{
			File hdl_config(context.openConfig(block, O_WRONLY));
			EQUAL((ssize_t)sizeof(hdl_configuration_blob),
				hdl_config.write(hdl_configuration_blob, sizeof(hdl_configuration_blob)));
		}
		catch (const dyplo::IOException& ex)
		{
			FAIL(ex.what());
		}
	}

	{
		dyplo::FilePollScheduler scheduler;
		dyplo::File output_file(context.openFifo(0, O_WRONLY));
		dyplo::File input_file(context.openFifo(0, O_RDONLY));
		dyplo::FileOutputQueue<int> output(scheduler, output_file, 2048);
		dyplo::FileInputQueue<int> input(scheduler, input_file, 2048);
		dyplo::Thread sender;
		sender.start(thread_send_many_blocks, &output);
		try
		{
			int* data;
			unsigned int count;
			unsigned int prev_count = 0;
			unsigned int prev_raw_count = 0;
			int counter = 1;
			unsigned int words_read = 0;
			while(words_read < how_many_blocks * 2048)
			{
				unsigned int raw_count = input.begin_read(data, 2);
				CHECK(raw_count != 0);
				CHECK(raw_count <= 2048);
				if (raw_count > 142)
					count = 142;
				else
					count = raw_count;
				for (unsigned int i = 0; i < count; ++i)
				{
					if (data[i] != counter)
					{
						std::ostringstream msg; \
						msg << "Mismatch at " << i << "(+" << words_read << ") expected: " 
							<< (counter) << " actual: " << data[i]
							<< " diff:" << (data[i] - counter);
						if (i < count - 2)
							msg << "\nNext data: " << data[i+1] << " "<< data[i+2]
								<< " .. " << data[count-1];
						msg << "\nRemaining: " << (count - i) << " words\n";
						msg << "count=" << count << " raw_count=" << raw_count
						    << " data=" << data << " prev_count=" << prev_count
						    << " prev_raw_count=" << prev_raw_count;
						FAIL(msg.str());
					}
					++counter;
				}
				input.end_read(count);
				words_read += count;
				prev_count = count;
				prev_raw_count = raw_count;
			}
		}
		catch (const std::exception& ex)
		{
			output.interrupt_write();
			input.interrupt_read();
			throw;
		}
	}
	check_all_input_fifos_are_empty();
}
