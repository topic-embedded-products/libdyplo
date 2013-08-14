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
	File cfg0("/dev/dyplocfg0");
	File cfg1("/dev/dyplocfg1"); /* Other cfg can be opened */
	ASSERT_THROW(File another_cfg0("/dev/dyplocfg0"), dyplo::IOException);
}

FUNC(hardware_driver_fifo_single_open)
{
	File r0("/dev/dyplor0");
	File r1("/dev/dyplor1"); /* Other fifo can be opened */
	ASSERT_THROW(File another_r0("/dev/dyplor0"), dyplo::IOException);
}
