#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "exceptions.hpp"
#include "filequeue.hpp"
#include "thread.hpp"
#include "condition.hpp"
#include "mutex.hpp"

#define YAFFUT_MAIN
#include "yaffut.h"

/* TODO: Put in header */
/* ioctl values for dyploctl device, set and get routing tables */
struct dyplo_route_t  {
	unsigned int n_routes;
	unsigned int* proutes;
};
#define DYPLO_IOC_MAGIC	'd'
#define DYPLO_IOC_ROUTE_SET	0x01
#define DYPLO_IOC_ROUTE_GET	0x02
#define DYPLO_IOC_ROUTE_TELL	0x03
/* S means "Set" through a ptr,
 * T means "Tell", sets directly
 * G means "Get" through a ptr
 * Q means "Query", return value */
#define DYPLO_IOCSROUTE   _IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_SET, struct dyplo_route_t)
#define DYPLO_IOCGROUTE   _IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_GET, struct dyplo_route_t)
#define DYPLO_IOCTROUTE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_TELL)


const char DRIVER_CONTROL[] = "/dev/dyploctl";

class File
{
public:
	int handle;
	File(const char* filename, int access = O_RDONLY):
		handle(::open(filename, access))
	{
		if (handle == -1)
			throw dyplo::IOException();
	}
	File(int fifo, int access)
	{		
		std::ostringstream name;
		if (access == O_RDONLY)
			name << "/dev/dyplor";
		else if (access == O_WRONLY)
			name << "/dev/dyplow";
		else
			FAIL("bad access");
		name << fifo;
		handle = ::open(name.str().c_str(), access);
		if (handle == -1)
			throw dyplo::IOException();
	}
	~File()
	{
		::close(handle);
	}

	ssize_t write(const void *buf, size_t count)
	{
		ssize_t result = ::write(handle, buf, count);
		if (result < 0)
			throw dyplo::IOException();
		return result;
	}

	ssize_t read(void *buf, size_t count)
	{
		ssize_t result = ::read(handle, buf, count);
		if (result < 0)
			throw dyplo::IOException();
		return result;
	}
	
	bool poll_for_incoming_data(int timeout_in_seconds)
	{
		struct timeval timeout;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(handle, &fds);
		timeout.tv_sec = timeout_in_seconds;
		timeout.tv_usec = 0;
		int status = select(handle+1, &fds, NULL, NULL, &timeout);
		if (status < 0)
			throw dyplo::IOException();
		return status && FD_ISSET(handle, &fds);
	}

	bool poll_for_outgoing_data(int timeout_in_seconds)
	{
		struct timeval timeout;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(handle, &fds);
		timeout.tv_sec = timeout_in_seconds;
		timeout.tv_usec = 0;
		int status = select(handle+1, NULL, &fds, NULL, &timeout);
		if (status < 0)
			throw dyplo::IOException();
		return status && FD_ISSET(handle, &fds);
	}
};

FUNC(hardware_driver_a_control_multiple_open)
{
	File ctrl1(DRIVER_CONTROL);
	File ctrl2(DRIVER_CONTROL);
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
	File cfg3rw("/dev/dyplocfg3", O_RDWR);
	ASSERT_THROW(File another_cfg3rw("/dev/dyplocfg3", O_RDONLY), dyplo::IOException);
	ASSERT_THROW(File another_cfg3rw("/dev/dyplocfg3", O_WRONLY), dyplo::IOException);
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

FUNC(hardware_driver_d_io_control)
{
	File ctrl(DRIVER_CONTROL);
	/* Invalid ioctl, must fail */
	EQUAL(-1, ioctl(ctrl.handle, 0x12345678));
	EQUAL(ENOTTY, errno);
	/* Set up route */
	EQUAL(0, ioctl(ctrl.handle, DYPLO_IOCTROUTE, 0x00200020)); /* Should be: fifo 1[0] -> fifo 1[0] */
	struct dyplo_route_t routes;
	routes.n_routes = 10;
	routes.proutes = new unsigned int [10];
	int n_routes = ioctl(ctrl.handle, DYPLO_IOCGROUTE, &routes);
	CHECK(n_routes > 0);
	/* TODO: Driver/hardware doesn't really work yet
	EQUAL(1, n_routes);
	EQUAL(0x00200020, routes.proutes[0]);
	*/
}

static void connect_all_fifos_in_loop()
{
	File ctrl(DRIVER_CONTROL);
	unsigned int routes[32];
	struct dyplo_route_t route_table = {32, routes};
	for (int i = 0; i < 32; ++i)
		routes[i] = (i << 21) | (i << 5);
	/* TODO: Send to driver (and verify?)
	EQUAL(0, ioctl(ctrl.handle, DYPLO_IOCSROUTE, &route_table));
	*/
}

FUNC(hardware_driver_e_transmit_loop)
{
	connect_all_fifos_in_loop();
	for (int fifo=0; fifo<32; ++fifo)
	{
		File fifo_in(fifo, O_RDONLY);
		File fifo_out(fifo, O_WRONLY);
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

ssize_t fill_fifo_to_the_brim(File &fifo_out)
{
	/* Fill it until it is full */
	int max_repeats = 100;
	int* buffer = new int[1024];
	ssize_t total_written = 0;
	
	while (--max_repeats)
	{
		for (int i = 0; i < 1024; ++i)
			buffer[i] = (total_written >> 2) + i;
		ssize_t written = fifo_out.write(buffer, 1024*sizeof(buffer[0]));
		total_written += written;
		if (written < 1024*sizeof(buffer[0]))
			break;
	}
	delete [] buffer;
	
	CHECK(max_repeats > 0);
	
	return total_written;
}

void hardware_driver_poll_single(int fifo)
{
	File fifo_in(fifo, O_RDONLY);
	File fifo_out(fifo, O_WRONLY);
	
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

	ssize_t total_written = fill_fifo_to_the_brim(fifo_out);
	CHECK(total_written > 0);

	/* Status must indicate that there is no room */
	CHECK(! fifo_out.poll_for_outgoing_data(0));
	CHECK(fifo_in.poll_for_incoming_data(1) ); /*  has data */

	int* buffer = new int[1024];
	ssize_t total_read = 0;

	while (total_read < total_written)
	{
		ssize_t bytes_read = fifo_in.read(buffer, 1024*sizeof(buffer[0]));
		const ssize_t ints_read = bytes_read >> 2;
		for (int i = 0; i < ints_read; ++i) {
			EQUAL((total_read >> 2) + i, buffer[i]);
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
		hardware_driver_poll_single(i);
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
		File fifo_in(fifo, O_RDONLY);
		
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
	
	File fifo_in(fifo, O_RDONLY);
	File fifo_out(fifo, O_WRONLY);
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
	hardware_driver_irq_driven_read_single(1);
	hardware_driver_irq_driven_read_single(5);
	hardware_driver_irq_driven_read_single(19);
	hardware_driver_irq_driven_read_single(25);
	hardware_driver_irq_driven_read_single(31);
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
		File fifo_out(fifo, O_WRONLY);
		EQUAL(0, dyplo::set_non_blocking(fifo_out.handle));
		CHECK(fifo_out.poll_for_outgoing_data(0)); /* Must have room */
		/* Fill it until it is full */
		total_written = fill_fifo_to_the_brim(fifo_out);
		CHECK(total_written > 0);
		/* Status must indicate that there is no room */
		CHECK(!fifo_out.poll_for_outgoing_data(0));
		/* Close and re-open the output to clear the "non-blocking" flag */
	}

	File fifo_in(fifo, O_RDONLY);
	File fifo_out(fifo, O_WRONLY);
	FileContext ctx(&fifo_out);
	dyplo::Thread writer;
	
	writer.start(thread_write_data, &ctx);
	ctx.wait(); /* Block until we're calling "write" on the other thread*/
	ssize_t total_read = 0;
	int buffer[64];
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
		bytes = fifo_in.read(buffer, sizeof(buffer));
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
	hardware_driver_irq_driven_write_single(9);
	hardware_driver_irq_driven_write_single(11);
	hardware_driver_irq_driven_write_single(16);
	hardware_driver_irq_driven_write_single(17);
	hardware_driver_irq_driven_write_single(18);
	check_all_input_fifos_are_empty();
}
