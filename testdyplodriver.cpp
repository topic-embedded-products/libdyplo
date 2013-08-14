#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "exceptions.hpp"

#define YAFFUT_MAIN
#include "yaffut.h"

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
