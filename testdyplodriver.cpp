#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "exceptions.hpp"
#include "filequeue.hpp"

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

FUNC(hardware_driver_control_multiple_open)
{
	File ctrl1(DRIVER_CONTROL);
	File ctrl2(DRIVER_CONTROL);
}

FUNC(hardware_driver_config_single_open)
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

FUNC(hardware_driver_fifo_single_open_rw_access)
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

FUNC(hardware_driver_io_control)
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

FUNC(hardware_driver_transmit_loop)
{
	for (int fifo=0; fifo<32; ++fifo)
	{
		std::ostringstream name_in;
		name_in << "/dev/dyplor" << fifo;
		File fifo_in(name_in.str().c_str(), O_RDONLY);
		std::ostringstream name_out;
		name_out << "/dev/dyplow" << fifo;
		File fifo_out(name_out.str().c_str(), O_WRONLY);
		/* Future todo: Set up routing... */
		int buffer[16];
		int received[16];
		for (int repeats = 4; repeats !=0; --repeats)
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

FUNC(hardware_driver_poll)
{
	File fifo_in("/dev/dyplor10", O_RDONLY);
	File fifo_out("/dev/dyplow10", O_WRONLY);
	
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
	
	/* Fill it until it is full */
	int max_repeats = 100;
	int* buffer = new int[1024];
	ssize_t total_written = 0;
	
	while (max_repeats--)
	{
		ssize_t written = fifo_out.write(buffer, 1024*sizeof(buffer[0]));
		total_written += written;
		if (written < 1024*sizeof(buffer[0]))
			break;
	}
	CHECK(total_written > 0);
	CHECK(max_repeats != 0);
	
	/* Status must indicate that there is no room */
	CHECK(! fifo_out.poll_for_outgoing_data(0));
	
	CHECK(fifo_in.poll_for_incoming_data(1) ); /*  has data */
	ssize_t total_read = 0;
	while (total_read < total_written)
	{
		ssize_t bytes_read = fifo_in.read(buffer, 1024*sizeof(buffer[0]));
		total_read += bytes_read;
	}
	EQUAL(total_written, total_read);
	CHECK(! fifo_in.poll_for_incoming_data(0) ); /* is empty */
	
	ASSERT_THROW(fifo_in.read(&data, sizeof(data)), dyplo::IOException);
	
	delete [] buffer;
}
