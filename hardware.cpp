#include "config.h"
#include "hardware.hpp"
#include "directoryio.hpp"
#include <sys/ioctl.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include <errno.h>
#include <string.h>

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
#define DYPLO_IOC_ROUTE_DELETE	0x04
#define DYPLO_IOC_BACKPLANE_STATUS	0x08
#define DYPLO_IOC_BACKPLANE_DISABLE	0x09
#define DYPLO_IOC_BACKPLANE_ENABLE	0x0A
/* S means "Set" through a ptr,
 * T means "Tell", sets directly
 * G means "Get" through a ptr
 * Q means "Query", return value */
#define DYPLO_IOCROUTE_CLEAR	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_CLEAR)
#define DYPLO_IOCSROUTE   _IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_SET, struct dyplo_route_t)
#define DYPLO_IOCGROUTE   _IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_GET, struct dyplo_route_t)
#define DYPLO_IOCTROUTE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_TELL)
/* Remove routes to a node. Argument is a integer node number. */
#define DYPLO_IOCTROUTE_DELETE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_DELETE)
/* Get backplane status. When called on control node, returns a bit mask where 0=CPU and
 * 1=first HDL node and so on. When called on config node, returns the status for only
 * that node, 0=disabled, non-zero is enabled */
#define DYPLO_IOCQBACKPLANE_STATUS   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_BACKPLANE_STATUS)
/* Enable or disable backplane status. Disable is required when the logic is active and
 * you want to replace a node using partial configuration. Operations are atomic. */
#define DYPLO_IOCTBACKPLANE_ENABLE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_BACKPLANE_ENABLE)
#define DYPLO_IOCTBACKPLANE_DISABLE  _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_BACKPLANE_DISABLE)
}

namespace dyplo
{
	const char* TruncatedFileException::what() const throw()
	{
		return "Unexpected end of file";
	}
	
	HardwareContext::HardwareContext():
		prefix(DYPLO_DRIVER_PREFIX),
		bitstream_basepath(BITSTREAM_DATA_PATH)
	{}
	
	HardwareContext::HardwareContext(const std::string& driver_prefix):
		prefix(driver_prefix),
		bitstream_basepath(BITSTREAM_DATA_PATH)
	{
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

	int HardwareContext::openControl(int access)
	{
		std::ostringstream name;
		name << prefix << "ctl";
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
			unsigned int total_bytes = 0;
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
	
	static bool is_digit(const char c)
	{
		return (c >= '0') && (c <= '9');
	}

	static int parse_number_from_name(const char* name)
	{
		for (const char* pos = name + strlen(name) - 1; pos >= name; --pos)
		{
			if (is_digit(*pos))
			{
				int result = *pos - '0';
				int weight = 10;
				while (pos != name)
				{
					--pos;
					if (is_digit(*pos))
					{
						result += weight * (*pos - '0');
						weight *= 10;
					}
					else
					{
						break;
					}
				}
				return result;
			}
		}
		return -1;
	}
	
	unsigned int HardwareContext::getAvailablePartitions(const char* function)
	{
		unsigned int result = 0;
		std::string path(bitstream_basepath);
		path += '/';
		path += function;
		DirectoryListing dir(path.c_str());
		struct dirent *entry;
		while ((entry = dir.next()) != NULL)
		{
			switch (entry->d_type)
			{
				case DT_REG:
				case DT_LNK:
				case DT_UNKNOWN:
					int index = parse_number_from_name(entry->d_name);
					if (index >= 0)
						result |= (1 << index);
					break;
			}
		}
		return result;
	}
	
	std::string HardwareContext::findPartition(const char* function, int partition)
	{
		std::string path(bitstream_basepath);
		path += '/';
		path += function;
		DirectoryListing dir(path.c_str());
		struct dirent *entry;
		while ((entry = dir.next()) != NULL)
		{
			switch (entry->d_type)
			{
				case DT_REG:
				case DT_LNK:
				case DT_UNKNOWN:
					int index = parse_number_from_name(entry->d_name);
					if (index == partition)
						return path + '/' + entry->d_name;
			}
		}
		return "";
	}

	void HardwareControl::routeDeleteAll()
	{
		if (::ioctl(handle, DYPLO_IOCROUTE_CLEAR) != 0)
			throw IOException(__func__);
	}

	void HardwareControl::routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo)
	{
		struct dyplo_route_item_t route =
			{dstFifo, dstNode, srcFifo, srcNode};
		if (::ioctl(handle, DYPLO_IOCTROUTE, route) != 0)
			throw IOException(__func__);
	}

	int HardwareControl::routeGetAll(Route* items, int n_items)
	{
		struct dyplo_route_t routes;
		routes.n_routes = n_items;
		routes.proutes = (struct dyplo_route_item_t*)items;
		return ::ioctl(handle, DYPLO_IOCGROUTE, &routes);
	}

	void HardwareControl::routeAdd(const Route* items, int n_items)
	{
		struct dyplo_route_t routes;
		routes.n_routes = n_items;
		routes.proutes = (struct dyplo_route_item_t*)items;
		if (::ioctl(handle, DYPLO_IOCSROUTE, &routes) != 0)
			throw IOException(__func__);
	}
	
	void HardwareControl::routeDelete(char node)
	{
		int arg = node;
		if (::ioctl(handle, DYPLO_IOCTROUTE_DELETE, arg) != 0)
			throw IOException(__func__);
	}
	
	unsigned int HardwareControl::getEnabledNodes()
	{
		int result = ::ioctl(handle, DYPLO_IOCQBACKPLANE_STATUS);
		if (result < 0)
			throw IOException();
		return (unsigned int)result;
	}

	void HardwareControl::enableNodes(unsigned int mask)
	{
		int result = ::ioctl(handle, DYPLO_IOCTBACKPLANE_ENABLE, mask);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareControl::disableNodes(unsigned int mask)
	{
		int result = ::ioctl(handle, DYPLO_IOCTBACKPLANE_DISABLE, mask);
		if (result < 0)
			throw IOException(__func__);
	}
}
