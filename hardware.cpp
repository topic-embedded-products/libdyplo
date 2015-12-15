/*
 * hardware.cpp
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
#include "config.h"
#include "hardware.hpp"
#include "directoryio.hpp"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include <errno.h>
#include <string.h>
#include <fstream>

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

struct dyplo_buffer_block_alloc_req {
	uint32_t size;	/* Size of each buffer (must be cache aligned) */
	uint32_t count;	/* Number of buffers */
};

/* DMA not used for CPU-logic transfers at all, only for logic
 * storage. Buffer can be mmap'ed for inspection. */
#define DYPLO_DMA_MODE_STANDALONE	0
/* (default) Copies data from userspace into a kernel buffer and
 * vice versa. */
#define DYPLO_DMA_MODE_RINGBUFFER_BOUNCE	1
/* Blockwise data transfers, using coherent memory. This will result in
 * slow non-cached memory being used when hardware coherency is not
 * available, but it is the fastest mode. */
#define DYPLO_DMA_MODE_BLOCK_COHERENT	2
/* Blockwise data transfers, using  streaming DMA into cachable memory.
 * Managing the cache may cost more than actually copying the data. */
#define DYPLO_DMA_MODE_BLOCK_STREAMING	3

struct dyplo_dma_configuration_req {
	uint32_t mode;	/* One of DYPLO_DMA_MODE.. */
	uint32_t size;	/* Size of each buffer (will be page aligned) */
	uint32_t count;	/* Number of buffers */
};

#define DYPLO_IOC_MAGIC	'd'
#define DYPLO_IOC_ROUTE_CLEAR	0x00
#define DYPLO_IOC_ROUTE_SET	0x01
#define DYPLO_IOC_ROUTE_GET	0x02
#define DYPLO_IOC_ROUTE_TELL	0x03
#define DYPLO_IOC_ROUTE_DELETE	0x04
#define DYPLO_IOC_ROUTE_TELL_TO_LOGIC	0x05
#define DYPLO_IOC_ROUTE_TELL_FROM_LOGIC	0x06
#define DYPLO_IOC_ROUTE_QUERY_ID	0x07
#define DYPLO_IOC_BACKPLANE_STATUS	0x08
#define DYPLO_IOC_BACKPLANE_DISABLE	0x09
#define DYPLO_IOC_BACKPLANE_ENABLE	0x0A
#define DYPLO_IOC_RESET_FIFO_WRITE	0x0C
#define DYPLO_IOC_RESET_FIFO_READ	0x0D
#define DYPLO_IOC_TRESHOLD_QUERY	0x10
#define DYPLO_IOC_TRESHOLD_TELL	0x11
#define DYPLO_IOC_USERSIGNAL_QUERY	0x12
#define DYPLO_IOC_USERSIGNAL_TELL	0x13
#define DYPLO_IOC_DMA_RECONFIGURE	0x1F
#define DYPLO_IOC_DMABLOCK_ALLOC	0x20
#define DYPLO_IOC_DMABLOCK_FREE 	0x21
#define DYPLO_IOC_DMABLOCK_QUERY	0x22
#define DYPLO_IOC_DMABLOCK_ENQUEUE	0x23
#define DYPLO_IOC_DMABLOCK_DEQUEUE	0x24
#define DYPLO_IOC_DMASTANDALONE_CONFIGURE_TO_LOGIC	0x28
#define DYPLO_IOC_DMASTANDALONE_CONFIGURE_FROM_LOGIC	0x29
#define DYPLO_IOC_DMASTANDALONE_START_TO_LOGIC	0x2A
#define DYPLO_IOC_DMASTANDALONE_START_FROM_LOGIC	0x2B
#define DYPLO_IOC_DMASTANDALONE_STOP_TO_LOGIC	0x2C
#define DYPLO_IOC_DMASTANDALONE_STOP_FROM_LOGIC	0x2D
#define DYPLO_IOC_LICENSE_KEY	0x30
#define DYPLO_IOC_STATIC_ID	0x31
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
/* Add a route from "this" dma or cpu node to another node. The argument
 * is an integer of destination node | fifo << 8 */
#define DYPLO_IOCTROUTE_TELL_TO_LOGIC	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_TELL_TO_LOGIC)
/* Add a route from another node into "this" dma or cpu node. Argument
 * is an integer of source node | fifo << 8 */
#define DYPLO_IOCTROUTE_TELL_FROM_LOGIC	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_TELL_FROM_LOGIC)
/* Get the node number and fifo (if applicable) for this cpu or dma
 * node. Returns an integer of node | fifo << 8 */
#define DYPLO_IOCQROUTE_QUERY_ID	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_QUERY_ID)
/* Get backplane status. When called on control node, returns a bit mask where 0=CPU and
 * 1=first HDL node and so on. When called on config node, returns the status for only
 * that node, 0=disabled, non-zero is enabled */
#define DYPLO_IOCQBACKPLANE_STATUS   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_BACKPLANE_STATUS)
/* Enable or disable backplane status. Disable is required when the logic is active and
 * you want to replace a node using partial configuration. Operations are atomic. */
#define DYPLO_IOCTBACKPLANE_ENABLE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_BACKPLANE_ENABLE)
#define DYPLO_IOCTBACKPLANE_DISABLE  _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_BACKPLANE_DISABLE)
/* Reset FIFO data (i.e. throw it away). Can be applied to config
 * nodes to reset its incoming fifos (argument is bitmask for queues to
 * reset), or to a CPU read/write fifo (argument ignored). */
#define DYPLO_IOCRESET_FIFO_WRITE	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_RESET_FIFO_WRITE)
#define DYPLO_IOCRESET_FIFO_READ	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_RESET_FIFO_READ)
/* Set the thresholds for "writeable" or "readable" on a CPU node fifo. Allows
 * tuning for low latency or reduced interrupt rate. On the DMA node, these
 * will get the DMA buffer size */
#define DYPLO_IOCQTRESHOLD   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_TRESHOLD_QUERY)
#define DYPLO_IOCTTRESHOLD   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_TRESHOLD_TELL)
/* Set or get user signal bits. These are the upper 4 bits of Dyplo data
 * that aren't part of the actual data, but control the flow. */
#define DYPLO_IOCQUSERSIGNAL   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_USERSIGNAL_QUERY)
#define DYPLO_IOCTUSERSIGNAL   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_USERSIGNAL_TELL)
/* DMA configuration */
#define DYPLO_IOCDMA_RECONFIGURE _IOWR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMA_RECONFIGURE, struct dyplo_dma_configuration_req)
/* Dyplo's IIO-alike DMA block interface */
#define DYPLO_IOCDMABLOCK_ALLOC	_IOWR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMABLOCK_ALLOC, struct dyplo_buffer_block_alloc_req)
#define DYPLO_IOCDMABLOCK_FREE 	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_DMABLOCK_FREE)
#define DYPLO_IOCDMABLOCK_QUERY	_IOWR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMABLOCK_QUERY, HardwareDMAFifo::InternalBlock)
#define DYPLO_IOCDMABLOCK_ENQUEUE	_IOWR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMABLOCK_ENQUEUE, HardwareDMAFifo::InternalBlock)
#define DYPLO_IOCDMABLOCK_DEQUEUE	_IOWR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMABLOCK_DEQUEUE, HardwareDMAFifo::InternalBlock)
/* Standalone DMA configuration and control */
#define DYPLO_IOCSDMASTANDALONE_CONFIGURE_TO_LOGIC	_IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_CONFIGURE_TO_LOGIC, HardwareDMAFifo::StandaloneConfiguration)
#define DYPLO_IOCGDMASTANDALONE_CONFIGURE_TO_LOGIC	_IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_CONFIGURE_TO_LOGIC, HardwareDMAFifo::StandaloneConfiguration)
#define DYPLO_IOCSDMASTANDALONE_CONFIGURE_FROM_LOGIC	_IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_CONFIGURE_FROM_LOGIC, HardwareDMAFifo::StandaloneConfiguration)
#define DYPLO_IOCGDMASTANDALONE_CONFIGURE_FROM_LOGIC	_IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_CONFIGURE_FROM_LOGIC, HardwareDMAFifo::StandaloneConfiguration)
#define DYPLO_IOCDMASTANDALONE_START_TO_LOGIC   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_START_TO_LOGIC)
#define DYPLO_IOCDMASTANDALONE_START_FROM_LOGIC   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_START_FROM_LOGIC)
#define DYPLO_IOCDMASTANDALONE_STOP_TO_LOGIC  _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_STOP_TO_LOGIC)
#define DYPLO_IOCDMASTANDALONE_STOP_FROM_LOGIC  _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_DMASTANDALONE_STOP_FROM_LOGIC)
/* Read or write a 64-bit license key */
#define DYPLO_IOCSLICENSE_KEY   _IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_LICENSE_KEY, unsigned long long)
#define DYPLO_IOCGLICENSE_KEY   _IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_LICENSE_KEY, unsigned long long)
/* Retrieve static ID (to match against partials) */
#define DYPLO_IOCGSTATIC_ID   _IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_STATIC_ID, unsigned int)
} /* extern "C" */

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
		switch (access & O_ACCMODE)
		{
			case O_RDONLY:
				name << "r";
				break;
			case O_WRONLY:
				name << "w";
				break;
			default:
				throw IOException(EACCES);
		}
		name << fifo;
		return ::open(name.str().c_str(), access);
	}

	int HardwareContext::openDMA(int index, int access)
	{
		std::ostringstream name;
		name << prefix << "d" << index;
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

	static unsigned short parse_u16(const unsigned char* data)
	{
		return (data[0] << 8) | data[1];
	}

	static void swap_buffer(unsigned char* buffer, unsigned int size)
	{
		for (unsigned int index = 0; index < size; index += 4)
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
	}

	static const size_t ALIGN_SIZE = 16 * 1024;
	static const size_t BUFFER_SIZE = 2 * ALIGN_SIZE;

	unsigned int HardwareContext::program(File &output, File &input)
	{
		std::vector<unsigned char> buffer(BUFFER_SIZE);
		ssize_t bytes = input.read_all(&buffer[0], ALIGN_SIZE);
		if (bytes < 64)
			throw TruncatedFileException();
		if ((parse_u16(&buffer[0]) == 9) && /* Magic marker for .bit file */
			(parse_u16(&buffer[11]) == 1) &&
			(buffer[13] == 'a'))
		{
			/* It's a bitstream, convert and flash */
			const unsigned char* end = &buffer[bytes];
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
			data += 4;
			unsigned int total_bytes = bytes - (data - &buffer[0]);
			if (total_bytes >= size)
			{
				total_bytes = size;
			}
			else
			{
				/* make sure that we always transfer a multiple of
				 * ALIGN_SIZE bytes to logic */
				unsigned int align = total_bytes % ALIGN_SIZE;
				if (align != 0)
				{
					align = ALIGN_SIZE - align; /* number of bytes to read */
					bytes = input.read_all(data + total_bytes, align);
					if (bytes < align)
						throw TruncatedFileException();
					total_bytes += align;
				}
			}
			swap_buffer(data, total_bytes);
			bytes = output.write(data, total_bytes);
			if (bytes < total_bytes)
				throw TruncatedFileException();
			while (total_bytes < size)
			{
				unsigned int to_read = size - total_bytes < BUFFER_SIZE ? size - total_bytes : BUFFER_SIZE;
				bytes = input.read_all(&buffer[0], to_read);
				if (bytes < to_read)
					throw TruncatedFileException();
				swap_buffer(&buffer[0], bytes);
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

	unsigned int HardwareContext::program(File &output, const char* filename)
	{
		File input(filename, O_RDONLY);
		return program(output, input);
	}

	unsigned int HardwareContext::program(const char* filename)
	{
		File output(XILINX_XDEVCFG, O_WRONLY);
		return program(output, filename);
	}

	unsigned int HardwareContext::program(File &input)
	{
		File output(XILINX_XDEVCFG, O_WRONLY);
		return program(output, input);
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

	unsigned int HardwareContext::getAvailablePartitionsIn(const char* path)
	{
		int result = 0;
		DirectoryListing dir(path);
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

	unsigned int HardwareContext::getAvailablePartitions(const char* function)
	{
		std::string path(bitstream_basepath);
		path += '/';
		path += function;
		return getAvailablePartitionsIn(path.c_str());
	}

	std::string HardwareContext::findPartitionIn(const char* path, int partition)
	{
		DirectoryListing dir(path);
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
						return std::string(path) + '/' + entry->d_name;
			}
		}
		return "";
	}

	std::string HardwareContext::findPartition(const char* function, int partition)
	{
		std::string path(bitstream_basepath);
		path += '/';
		path += function;
		return findPartitionIn(path.c_str(), partition);
	}

	void HardwareControl::routeDeleteAll()
	{
		if (::ioctl(handle, DYPLO_IOCROUTE_CLEAR) != 0)
			throw IOException(__func__);
	}

	void HardwareControl::routeAddSingle(unsigned char srcNode, unsigned char srcFifo, unsigned char dstNode, unsigned char dstFifo)
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

	void HardwareControl::writeDyploLicense(unsigned long long license_blob)
	{
		int result = ::ioctl(handle, DYPLO_IOCSLICENSE_KEY, &license_blob);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareControl::writeDyploLicenseFile(const char* path_to_dyplo_license_file)
	{
		unsigned long long hash;
		std::string line;
		std::ifstream license_file(path_to_dyplo_license_file);

		if (license_file.is_open())
		{
			while ( getline (license_file,line) )
			{
				if (line.find("DYPLO_DNA") != std::string::npos )
				{
					int equal_pos = line.find("=");
					std::string value = line.substr(equal_pos + 1);
					std::istringstream iss(value.c_str());
					iss >> std::hex >> hash;
				}
			}
			license_file.close();
		}
		writeDyploLicense(hash);
	}

	unsigned int HardwareControl::readDyploStaticID()
	{
		unsigned int data;
		if (::ioctl(handle, DYPLO_IOCGSTATIC_ID, &data) < 0)
			throw IOException(__func__);
		return data;
	}

	void HardwareConfig::resetWriteFifos(int file_descriptor, unsigned int mask)
	{
		int result = ::ioctl(file_descriptor, DYPLO_IOCRESET_FIFO_WRITE, mask);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareConfig::resetWriteFifos(unsigned int mask)
	{
		int result = ::ioctl(handle, DYPLO_IOCRESET_FIFO_WRITE, mask);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareConfig::resetReadFifos(int file_descriptor, unsigned int mask)
	{
		int result = ::ioctl(file_descriptor, DYPLO_IOCRESET_FIFO_READ, mask);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareConfig::resetReadFifos(unsigned int mask)
	{
		int result = ::ioctl(handle, DYPLO_IOCRESET_FIFO_READ, mask);
		if (result < 0)
			throw IOException(__func__);
	}

	bool HardwareConfig::isNodeEnabled()
	{
		int result = ::ioctl(handle, DYPLO_IOCQBACKPLANE_STATUS);
		if (result < 0)
			throw IOException();
		return (result != 0);
	}

	void HardwareConfig::enableNode()
	{
		int result = ::ioctl(handle, DYPLO_IOCTBACKPLANE_ENABLE);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareConfig::disableNode()
	{
		int result = ::ioctl(handle, DYPLO_IOCTBACKPLANE_DISABLE);
		if (result < 0)
			throw IOException(__func__);
	}

	int HardwareConfig::getNodeIndex()
	{
		return getNodeIndex(handle);
	}

	int HardwareConfig::getNodeIndex(int file_descriptor)
	{
		int result = ::ioctl(file_descriptor, DYPLO_IOCQROUTE_QUERY_ID);
		if (result < 0)
			throw IOException(__func__);
		return result;
	}

	void HardwareConfig::deleteRoutes()
	{
		deleteRoutes(handle);
	}

	void HardwareConfig::deleteRoutes(int file_descriptor)
	{
		int result = ::ioctl(file_descriptor, DYPLO_IOCTROUTE_DELETE);
		if (result < 0)
			throw IOException(__func__);
	}


	void HardwareFifo::reset()
	{
		int result = ::ioctl(handle, DYPLO_IOCRESET_FIFO_WRITE);
		if (result < 0)
			throw IOException(__func__);
	}

	int HardwareFifo::getNodeAndFifoIndex()
	{
		return getNodeAndFifoIndex(handle);
	}

	int HardwareFifo::getNodeAndFifoIndex(int file_descriptor)
	{
		int result = ::ioctl(file_descriptor, DYPLO_IOCQROUTE_QUERY_ID);
		if (result < 0)
			throw IOException(__func__);
		return result;
	}

	void HardwareFifo::addRouteTo(int destination)
	{
		int result = ::ioctl(handle, DYPLO_IOCTROUTE_TELL_TO_LOGIC, destination);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareFifo::addRouteFrom(int source)
	{
		int result = ::ioctl(handle, DYPLO_IOCTROUTE_TELL_FROM_LOGIC, source);
		if (result < 0)
			throw IOException(__func__);
	}

	unsigned int HardwareFifo::getDataTreshold()
	{
		int result = ::ioctl(handle, DYPLO_IOCQTRESHOLD);
		if (result < 0)
			throw IOException(__func__);
		return (unsigned int)result;
	}

	void HardwareFifo::setDataTreshold(unsigned int value)
	{
		int result = ::ioctl(handle, DYPLO_IOCTTRESHOLD, value);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareFifo::setUserSignal(int usersignal)
	{
		int result = ::ioctl(handle, DYPLO_IOCTUSERSIGNAL, usersignal);
		if (result < 0)
			throw IOException(__func__);
	}

	int HardwareFifo::getUserSignal()
	{
		int result = ::ioctl(handle, DYPLO_IOCQUSERSIGNAL);
		if (result < 0)
			throw IOException(__func__);
		return (unsigned int)result;
	}

	HardwareDMAFifo::HardwareDMAFifo(int file_descriptor):
		HardwareFifo(file_descriptor)
	{
	}

	static void* dma_map_single(int handle, int prot, off_t offset, size_t size)
	{
		void* map = ::mmap(NULL, size, prot, MAP_SHARED, handle, offset);
		if (map == MAP_FAILED)
			throw IOException("mmap");
		return map;
	}

	void HardwareDMAFifo::reconfigure(unsigned int mode, unsigned int size, unsigned int count, bool readonly)
	{
		struct dyplo_dma_configuration_req req;
		const int prot = readonly ? PROT_READ : (PROT_READ | PROT_WRITE);
		unmap();
		req.mode = mode;
		req.size = size;
		req.count = count;
		if (::ioctl(handle, DYPLO_IOCDMA_RECONFIGURE, &req) < 0)
			throw IOException(__func__);
		resize(req.count, req.size);
		try
		{
			switch (mode)
			{
				case MODE_STANDALONE:
					/* In standalone mode, it creates one big block of size*count bytes. */
					/* We don't need to mmap it because it's for the FPGA only. */
					break;
				case MODE_RINGBUFFER:
					/* This mode does not support memory mapping yet */
					break;
				case MODE_COHERENT:
				case MODE_STREAMING:
					for (std::vector<Block>::iterator it = blocks.begin(); it != blocks.end(); ++it)
					{
						if (::ioctl(handle, DYPLO_IOCDMABLOCK_QUERY, &(*it)) < 0)
							throw IOException("DYPLO_IOCDMABLOCK_QUERY");
						it->data = dma_map_single(handle, prot, it->offset, it->size);
					}
					break;
			}
		}
		catch (const std::exception&)
		{
			dispose();
			throw;
		}
	}

	HardwareDMAFifo::~HardwareDMAFifo()
	{
		dispose();
	}
	
	void HardwareDMAFifo::dispose()
	{
		unmap();
		::ioctl(handle, DYPLO_IOCDMABLOCK_FREE);
	}

	HardwareDMAFifo::Block* HardwareDMAFifo::dequeue()
	{
		Block* result = &(*blocks_head);
		if (result->state) /* Non-zero state indicates "driver" owns it */
		{
			int status = ::ioctl(handle, DYPLO_IOCDMABLOCK_DEQUEUE, result);
			if (status < 0) {
				if (errno == EAGAIN)
					return NULL;
				throw IOException("DYPLO_IOCDMABLOCK_DEQUEUE");
			}
		}
		++blocks_head;
		if (blocks_head == blocks.end())
			blocks_head = blocks.begin();
		return result;
	}

	void HardwareDMAFifo::enqueue(HardwareDMAFifo::Block* block)
	{
		int status = ::ioctl(handle, DYPLO_IOCDMABLOCK_ENQUEUE, block);
		if (status < 0) {
			throw IOException("DYPLO_IOCDMABLOCK_ENQUEUE");
		}
	}
	
	void HardwareDMAFifo::resize(unsigned int number_of_blocks, unsigned int blocksize)
	{
		blocks.resize(number_of_blocks);
		blocks_head = blocks.begin();
		unsigned int offset = 0;
		for (unsigned int i = 0; i < number_of_blocks; ++i)
		{
			blocks[i].id = i;
			blocks[i].data = NULL;
			blocks[i].size = blocksize;
			blocks[i].offset = offset;
			offset += blocksize;
		}
	}

	void HardwareDMAFifo::unmap()
	{
		for (std::vector<Block>::iterator it = blocks.begin(); it != blocks.end(); ++it)
		{
			if (it->data)
			{
				::munmap(it->data, it->size);
				it->data = NULL;
			}
		}
	}

	void HardwareDMAFifo::standaloneStart(bool direction_from_logic)
	{
		if(::ioctl(handle, direction_from_logic ? DYPLO_IOCDMASTANDALONE_START_FROM_LOGIC : DYPLO_IOCDMASTANDALONE_START_TO_LOGIC))
			throw IOException(__func__);
	}
	void HardwareDMAFifo::standaloneStop(bool direction_from_logic)
	{
		if(::ioctl(handle, direction_from_logic ? DYPLO_IOCDMASTANDALONE_STOP_FROM_LOGIC : DYPLO_IOCDMASTANDALONE_STOP_TO_LOGIC))
			throw IOException(__func__);
	}
	void HardwareDMAFifo::setStandaloneConfig(
		const  HardwareDMAFifo::StandaloneConfiguration *config, bool direction_from_logic)
	{
		if(::ioctl(handle, direction_from_logic ? DYPLO_IOCSDMASTANDALONE_CONFIGURE_FROM_LOGIC : DYPLO_IOCSDMASTANDALONE_CONFIGURE_TO_LOGIC, config))
			throw IOException(__func__);
	}

	void HardwareDMAFifo::getStandaloneConfig(
		HardwareDMAFifo::StandaloneConfiguration *config, bool direction_from_logic)
	{
		if(::ioctl(handle, direction_from_logic ? DYPLO_IOCGDMASTANDALONE_CONFIGURE_FROM_LOGIC : DYPLO_IOCGDMASTANDALONE_CONFIGURE_TO_LOGIC, config))
			throw IOException(__func__);
	}

	bool operator==(const dyplo::HardwareDMAFifo::StandaloneConfiguration& lhs, const dyplo::HardwareDMAFifo::StandaloneConfiguration& rhs)
	{
		return !memcmp(&lhs, &rhs, sizeof(lhs));
	}
}
