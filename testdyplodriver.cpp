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


FUNC(hardware_driver_a_control_multiple_open)
{
	dyplo::HardwareContext ctrl1;
	dyplo::HardwareContext ctrl2;
}

FUNC(hardware_driver_b_config_single_open)
{
	File cfg0r("/dev/dyplocfg0");
	File cfg1r("/dev/dyplocfg1"); /* Other cfg can be opened */
	ASSERT_THROW(File another_cfg0r("/dev/dyplocfg0"), dyplo::IOException);
	/* Can open same file with other access rights */
	File cfg0w("/dev/dyplocfg0", O_WRONLY);
	/* But cannot open it twice either */
	ASSERT_THROW(File another_cfg0w("/dev/dyplocfg0", O_WRONLY), dyplo::IOException);
	/* Open for both R/W */
	File cfg3rw("/dev/dyplocfg2", O_RDWR);
	ASSERT_THROW(File another_cfg3rw("/dev/dyplocfg2", O_RDONLY), dyplo::IOException);
	ASSERT_THROW(File another_cfg3rw("/dev/dyplocfg2", O_WRONLY), dyplo::IOException);
}

FUNC(hardware_driver_c_fifo_single_open_rw_access)
{
	File r0("/dev/dyplor0");
	File r1("/dev/dyplor1"); /* Other fifo can be opened */
	ASSERT_THROW(File another_r0("/dev/dyplor0"), dyplo::IOException);
	ASSERT_THROW(File another_r1("/dev/dyplor1"), dyplo::IOException);
	/* Cannot open input fifo for writing */
	ASSERT_THROW(File r2w("/dev/dyplor2", O_WRONLY), dyplo::IOException);
	/* Cannot open output fifo for reading */
	ASSERT_THROW(File w0r("/dev/dyplow0", O_RDONLY), dyplo::IOException);
}

static void connect_all_fifos_in_loop()
{
	dyplo::HardwareContext ctrl;
	dyplo::HardwareContext::Route routes[32];
	for (int i = 0; i < 32; ++i) {
		routes[i].srcNode = 0;
		routes[i].srcFifo = i;
		routes[i].dstNode = 0;
		routes[i].dstFifo = i;
	}
	ctrl.routeAdd(routes, 32);
}

FUNC(hardware_driver_d_io_control)
{
	dyplo::HardwareContext ctrl;
	dyplo::HardwareContext::Route routes[64];
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
	/* Setup loopback system (1->0, 1->1 etc) */
	connect_all_fifos_in_loop();
	n_routes =
		ctrl.routeGetAll(routes, sizeof(routes)/sizeof(routes[0]));
	EQUAL(32, n_routes);
	for (int fifo=0; fifo<32; ++fifo)
	{
		EQUAL(0, routes[fifo].srcNode);
		EQUAL(fifo, routes[fifo].srcFifo);
		EQUAL(0, routes[fifo].dstNode);
		EQUAL(fifo, routes[fifo].dstFifo);
	}
}

FUNC(hardware_driver_e_transmit_loop)
{
	connect_all_fifos_in_loop();
	for (int fifo=0; fifo<32; ++fifo)
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
			for (int i=0; i<sizeof(buffer)/sizeof(buffer[0]); ++i)
				buffer[i] = i + (1000 * i * repeats);
			int bytes = write(fifo_out.handle, buffer, sizeof(buffer));
			EQUAL(bytes, sizeof(buffer));
			bytes = read(fifo_in.handle, received, bytes);
			EQUAL(bytes, sizeof(buffer));
			for (int i=0; i<sizeof(buffer)/sizeof(buffer[0]); ++i)
				EQUAL(i + (repeats * 1000 * i), received[i]);
		}
	}
}

ssize_t fill_fifo_to_the_brim(File &fifo_out, int signature = 0)
{
	const int blocksize = 1024;
	/* Fill it until it is full */
	int max_repeats = 100;
	int* buffer = new int[blocksize];
	ssize_t total_written = 0;
	
	while (--max_repeats)
	{
		for (int i = 0; i < blocksize; ++i)
			buffer[i] = ((total_written >> 2) + i) + signature;
		ssize_t written = fifo_out.write(buffer, blocksize*sizeof(buffer[0]));
		total_written += written;
		if (written < blocksize*sizeof(buffer[0]))
			break;
	}
	delete [] buffer;
	
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

	ssize_t total_written = fill_fifo_to_the_brim(fifo_out, signature);
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

FUNC(hardware_driver_f_poll)
{
	connect_all_fifos_in_loop();
	for (int i = 0; i < 32; ++i)
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
	for (int fifo = 0; fifo < 32; ++fifo)
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

void hardware_driver_irq_driven_read_single(int fifo)
{
	dyplo::Thread reader;
	
	File fifo_in(openFifo(fifo, O_RDONLY));
	File fifo_out(openFifo(fifo, O_WRONLY));
	FileContext ctx(&fifo_in);
	reader.start(thread_read_data, &ctx);
	ctx.wait(); /* Block until we're calling "read" on the other thread*/
	char buffer[16];
	EQUAL(sizeof(buffer), fifo_out.write(buffer, sizeof(buffer)));
	void* result;
	EQUAL(reader.join(&result), 0);
	EQUAL(sizeof(buffer), (ssize_t)result);
}

FUNC(hardware_driver_g_irq_driven_read)
{
	connect_all_fifos_in_loop();
	for (int fifo = 31; fifo >= 0; --fifo) /* go back, variation */
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
	EQUAL(bytes, sizeof(buffer));
	void* result;
	EQUAL(writer.join(&result), 0);
	EQUAL(sizeof(extra_data_to_write_in_thread), (ssize_t)result);
	/* Drain the buffer */
	while (total_read < total_written)
	{
		int bytes_to_read;
		if (total_written - total_read > sizeof(buffer))
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
	EQUAL(sizeof(extra_data_to_write_in_thread), bytes);
	for (int i = 0; i < sizeof(extra_data_to_write_in_thread)/sizeof(extra_data_to_write_in_thread[0]); ++i)
		EQUAL(extra_data_to_write_in_thread[i], buffer[i]);
}

FUNC(hardware_driver_h_irq_driven_write)
{
	connect_all_fifos_in_loop();
	for (int fifo = 0; fifo < 32; ++fifo)
		hardware_driver_irq_driven_write_single(fifo);
	check_all_input_fifos_are_empty();
}

/* Test disabled for now */
static void hardware_driver_i_hdl_block_processing()
{
	static dyplo::HardwareContext::Route routes[] = {
		{0, 0, 1, 0}, 
		{1, 0, 0, 0}, /* Fifo 0 to HDL #1 port 0 */
		{0, 1, 2, 0},
		{2, 0, 0, 1}, /* HDL #2 connected to fifo 1 */
	};
	dyplo::HardwareContext ctrl;
	ctrl.routeAdd(routes, sizeof(routes)/sizeof(routes[0]));
	int data[] = {1, 2, 42000, -42000};
	const size_t data_size = sizeof(data)/sizeof(data[0]);
	for (int fifo = 0; fifo < 2; ++fifo)
	{
		File hdl_in(openFifo(fifo, O_WRONLY));
		File hdl_out(openFifo(fifo, O_RDONLY));
		ssize_t bytes_written = hdl_in.write(data, sizeof(data));
		EQUAL(sizeof(data), bytes_written);
		CHECK(hdl_out.poll_for_incoming_data(2)); /* Must have data */
		int buffer[data_size];
		ssize_t bytes_read = hdl_out.read(buffer, sizeof(buffer));
		EQUAL(sizeof(buffer), bytes_read);
		for (int i=0; i < data_size; ++i)
			EQUAL(data[i], buffer[i]);
	}
	check_all_input_fifos_are_empty();
}
