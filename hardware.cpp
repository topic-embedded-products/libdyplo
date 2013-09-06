#include "config.h"
#include "hardware.hpp"
#include <sys/ioctl.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include <errno.h>

#define DYPLO_DRIVER_PREFIX "/dev/dyplo"
#define XILINX_IS_PARTIAL_BITSTREAM_FILE "/sys/class/xdevcfg/xdevcfg/device/is_partial_bitstream"
#define XILINX_XDEVCFG "/dev/xdevcfg"

/* TODO: Put in header */
extern "C"
{
/* ioctl values for dyploctl device, set and get routing tables */
struct dyplo_route_item_t {
	unsigned char dstFifo; /* LSB */
	unsigned char dstNode;
	unsigned char srcFifo;
	unsigned char srcNode; /* MSB */
};

struct dyplo_route_t  {
	unsigned int n_routes;
	struct dyplo_route_item_t* proutes;
};
#define DYPLO_IOC_MAGIC	'd'
#define DYPLO_IOC_ROUTE_CLEAR	0x00
#define DYPLO_IOC_ROUTE_SET	0x01
#define DYPLO_IOC_ROUTE_GET	0x02
#define DYPLO_IOC_ROUTE_TELL	0x03
/* S means "Set" through a ptr,
 * T means "Tell", sets directly
 * G means "Get" through a ptr
 * Q means "Query", return value */
#define DYPLO_IOCROUTE_CLEAR	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_CLEAR)
#define DYPLO_IOCSROUTE   _IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_SET, struct dyplo_route_t)
#define DYPLO_IOCGROUTE   _IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_GET, struct dyplo_route_t)
#define DYPLO_IOCTROUTE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_TELL)
}

namespace dyplo
{
	static const char DRIVER_CONTROL[] = DYPLO_DRIVER_PREFIX "ctl";

	const char* TruncatedFileException::what() const throw()
	{
		return "Unexpected end of file";
	}
	
	HardwareContext::HardwareContext():
		control_device(DRIVER_CONTROL, O_RDWR),
		prefix(DYPLO_DRIVER_PREFIX)
	{}
	
	HardwareContext::HardwareContext(const std::string& driver_prefix):
		control_device((std::string(driver_prefix) + "ctl").c_str()),
		prefix(driver_prefix)
	{
	}
	
	void HardwareContext::routeDeleteAll()
	{
		if (::ioctl(control_device, DYPLO_IOCROUTE_CLEAR) != 0)
			throw IOException();
	}

	void HardwareContext::routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo)
	{
		struct dyplo_route_item_t route =
			{dstFifo, dstNode, srcFifo, srcNode};
		if (::ioctl(control_device, DYPLO_IOCTROUTE, route) != 0)
			throw IOException();
	}

	int HardwareContext::routeGetAll(Route* items, int n_items)
	{
		struct dyplo_route_t routes;
		routes.n_routes = n_items;
		routes.proutes = (struct dyplo_route_item_t*)items;
		return ::ioctl(control_device, DYPLO_IOCGROUTE, &routes);
	}

	void HardwareContext::routeAdd(const Route* items, int n_items)
	{
		struct dyplo_route_t routes;
		routes.n_routes = n_items;
		routes.proutes = (struct dyplo_route_item_t*)items;
		if (::ioctl(control_device, DYPLO_IOCSROUTE, &routes) != 0)
			throw IOException();
	}
	
	int HardwareContext::openFifo(int fifo, int access)
	{
		std::ostringstream name;
		name << prefix;
		if (access == O_RDONLY)
			name << "r";
		else if (access == O_WRONLY)
			name << "w";
		else
			throw IOException(EACCES);
		name << fifo;
		return ::open(name.str().c_str(), access);
	}
	
	int HardwareContext::openConfig(int index, int access)
	{
		std::ostringstream name;
		name << prefix << "cfg" << index;
		return ::open(name.str().c_str(), access);
	}

	void HardwareContext::setProgramMode(bool is_partial_bitstream)
	{
		File fd(XILINX_IS_PARTIAL_BITSTREAM_FILE, O_WRONLY);
		char value = is_partial_bitstream ? '1' : '0';
		if (fd.write(&value, sizeof(value)) != sizeof(value))
			throw TruncatedFileException();
	}
	
	bool HardwareContext::getProgramMode()
	{
		File fd(XILINX_IS_PARTIAL_BITSTREAM_FILE, O_RDONLY);
		char value;
		ssize_t r = fd.read(&value, sizeof(value));
		if (r  != sizeof(value))
			throw TruncatedFileException();
		return (value != '0');
	}

	unsigned short parse_u16(unsigned char* data)
	{
		return (data[0] << 8) | data[1];
	}

	static const size_t BUFFER_SIZE = 16*1024;
	unsigned int HardwareContext::program(File &output, const char* filename)
	{
		File input(filename, O_RDONLY);
		std::vector<unsigned char> buffer(BUFFER_SIZE);
		ssize_t bytes = input.read(&buffer[0], BUFFER_SIZE);
		if (bytes < 64)
			throw TruncatedFileException();
		if ((parse_u16(&buffer[0]) == 9) && /* Magic marker for .bit file */
			(parse_u16(&buffer[11]) == 1) &&
			(buffer[13] == 'a'))
		{
			/* It's a bitstream, convert and flash */
			unsigned char* end = &buffer[bytes];
			unsigned char* data = &buffer[13];
			/* Browse through tag/value pairs looking for the "e" */
			while (*data != 'e')
			{
				++data;
				if (data >= end)
					throw TruncatedFileException();
				unsigned short sz = parse_u16(data);
				data += 2 + sz;
				if (data >= end)
					throw TruncatedFileException();
			}
			++data;
			if (data+4 >= end)
				throw TruncatedFileException();
			unsigned int size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
			input.seek(data + 4 - &buffer[0]);
			unsigned int total_bytes;
			while (total_bytes < size)
			{
				unsigned int to_read = size - total_bytes < BUFFER_SIZE ? size - total_bytes : BUFFER_SIZE;
				bytes = input.read(&buffer[0], to_read);
				if (bytes < to_read)
					throw TruncatedFileException();
				for (unsigned int index = 0; index < to_read; index += 4)
				{
					/* Flip the bytes */
					unsigned char* data = &buffer[index];
					unsigned char t1, t2;
					t1 = data[0];
					t2 = data[1];
					data[0] = data[3];
					data[1] = data[2];
					data[3] = t1;
					data[2] = t2;
				}
				bytes = output.write(&buffer[0], to_read);
				if (bytes < to_read)
					throw TruncatedFileException();
				total_bytes += bytes;
			}
			return size;
		}
		else
		{
			/* Probably a bin file, they tend to start with all FF...
			 * so that seems a reasonable sanity check. */
			if (*(unsigned int*)&buffer[0] != 0xFFFFFFFF)
				throw std::runtime_error("Unrecognized bitstream format");
			unsigned int total_written = 0;
			do
			{
				ssize_t bytes_written = output.write(&buffer[0], bytes);
				if (bytes_written < bytes)
					throw TruncatedFileException();
				total_written += bytes;
				bytes = input.read(&buffer[0], BUFFER_SIZE);
			}
			while (bytes);
			return total_written;
		}
	}
	
	unsigned int HardwareContext::program(const char* filename)
	{
		File output(XILINX_XDEVCFG, O_WRONLY);
		return program(output, filename);
	}

}
