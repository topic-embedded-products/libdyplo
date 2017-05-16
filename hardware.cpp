/*
 * hardware.cpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013-2016 Topic Embedded Products B.V. (http://www.topic.nl).
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
#ifdef __linux__
#include "config.h"
#endif
#include "hardware.hpp"
#include "directoryio.hpp"
#include "dyplo-ioctl.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include <list>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <assert.h>

#include <iostream>

#define DYPLO_DRIVER_PREFIX "/dev/dyplo"

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

	int HardwareContext::openAvailableDMA(int access)
	{
		for (int index = 0; index < 31; ++index)
		{
			int result = openDMA(index, access);
			if (result != -1)
				return result;
			if (errno != EBUSY)
				throw IOException();
		}
		throw IOException(ENODEV);
	}

	static int count_numbered_files(const char* pattern)
	{
		int result = 0;
		char filename[64];
		for (;;)
		{
			int char_amount_requested = snprintf(filename, sizeof(filename), pattern, result);
			assert(char_amount_requested < static_cast<int>(sizeof(filename)));
			if (::access(filename, F_OK) != 0)
				return result;
			++result;
		}
	}

	static int get_dyplo_cpu_fifo_count_r()
	{
		return count_numbered_files("/dev/dyplor%d");
	}

	static int get_dyplo_cpu_fifo_count_w()
	{
		return count_numbered_files("/dev/dyplow%d");
	}

	int HardwareContext::openAvailableWriteFifo()
	{
		int numberOfCpuFifos = get_dyplo_cpu_fifo_count_w();

		for (int fifoIndex = 0; fifoIndex < numberOfCpuFifos; ++fifoIndex)
		{
			int fileDesc = openFifo(fifoIndex, O_WRONLY);

			if (fileDesc != -1)
				return fileDesc;
			if (errno != EBUSY)
				throw IOException();
		}

		throw IOException(ENODEV);
	}

	int HardwareContext::openAvailableReadFifo()
	{
		int numberOfCpuFifos = get_dyplo_cpu_fifo_count_r();

		for (int fifoIndex = 0; fifoIndex < numberOfCpuFifos; ++fifoIndex)
		{
			int fileDesc = openFifo(fifoIndex, O_RDONLY);
			if (fileDesc != -1)
				return fileDesc;
			if (errno != EBUSY)
				throw IOException();
		}

		throw IOException(ENODEV);
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

	static unsigned short parse_u16(const unsigned char* data)
	{
		return (data[0] << 8) | data[1];
	}

	static void swap_buffer_aligned(unsigned int* buffer, unsigned int size)
	{
		while (size)
		{
			/* Flip the bytes (gcc will recognize this as a "bswap") */
			unsigned int x = *buffer;
			*buffer = (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | ((x & 0xFF000000) >> 24);
			++buffer;
			--size;
		}
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
			bool isLinkOrRegularFile = false;
#if defined (__linux__)
			switch (entry->d_type)
			{
				case DT_REG:
				case DT_LNK:
				case DT_UNKNOWN:
					isLinkOrRegularFile = true;
					break;
			}
#elif defined(__rtems__)
			struct stat statBuf;
			std::string fullFileName = std::string(path) + "/" + std::string(entry->d_name);
			int res = stat(fullFileName.c_str(), &statBuf);

			if ((res == 0) && (S_ISREG(statBuf.st_mode) || S_ISLNK(statBuf.st_mode)))
			{
				isLinkOrRegularFile = true;
			}
#endif
			if (isLinkOrRegularFile)
			{
				int index = parse_number_from_name(entry->d_name);
				if (index >= 0)
					result |= (1 << index);
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

	std::string HardwareContext::findPartitionIn(const char* path, int node_id)
	{
		DirectoryListing dir(path);
		struct dirent *entry;
		std::list<std::string> possible_partitions;
		while ((entry = dir.next()) != NULL)
		{
			bool isLinkOrRegularFile = false;
#if defined (__linux__)
			switch (entry->d_type)
			{
				case DT_REG:
				case DT_LNK:
				case DT_UNKNOWN:
					isLinkOrRegularFile = true;
					break;
			}
#elif defined(__rtems__)
			struct stat statBuf;
			std::string fullFileName = std::string(path) + "/" + std::string(entry->d_name);
			int res = stat(fullFileName.c_str(), &statBuf);

			if ((res == 0) && (S_ISREG(statBuf.st_mode) || S_ISLNK(statBuf.st_mode)))
			{
				isLinkOrRegularFile = true;
			}
#endif

			if (isLinkOrRegularFile)
			{
				int index = parse_number_from_name(entry->d_name);
				if (index == node_id)
				{
					possible_partitions.push_back(std::string(path) + '/' + entry->d_name);
				}
			}
		}

		if (possible_partitions.size() == 1)
		{
			// immediately return the first partition in case of only one result
			return possible_partitions.front();
		}
		else if (!possible_partitions.empty())
		{
			// multiple possible partitions found.. use the '.partial' file if it is available
			const std::string PARTIAL_POSTFIX(".partial");
			for (std::list<std::string>::const_iterator it = possible_partitions.begin(); it != possible_partitions.end(); ++it)
			{
				const std::string& partition_name = *it;

				// check if the partition_name ends with .partial
				if (partition_name.length() > PARTIAL_POSTFIX.length() &&
					partition_name.rfind(PARTIAL_POSTFIX) == (partition_name.length() - PARTIAL_POSTFIX.length()))
				{
					return partition_name;
				}
			}

			// no '.partial' partitions found, return first possibility
			return possible_partitions.front();
		}

		return "";
	}

	std::string HardwareContext::findPartition(const char* function, int node_id)
	{
		std::string path(bitstream_basepath);
		path += '/';
		path += function;
		return findPartitionIn(path.c_str(), node_id);
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

	void HardwareControl::routeDelete(char dstNode)
	{
		int arg = dstNode;
		if (::ioctl(handle, DYPLO_IOCTROUTE_DELETE, arg) != 0)
			throw IOException(__func__);
	}

	unsigned int HardwareControl::program(const char* filename)
	{
		File input(filename, O_RDONLY);
		return program(input);
	}

	unsigned int HardwareControl::program(File &input)
	{
		HardwareProgrammer programmer(ctx, *this);
		return programmer.fromFile(input);
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

	int HardwareControl::getIcapNodeIndex()
	{
		int result = ::ioctl(handle, DYPLO_IOCQICAP_INDEX);
		if ((result < 0) && (result != -ENODEV))
			throw IOException(__func__);
		return result;
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

	void HardwareFifo::addRouteTo(int dstNodeAndFifoIndex)
	{
		int result = ::ioctl(handle, DYPLO_IOCTROUTE_TELL_TO_LOGIC, dstNodeAndFifoIndex);
		if (result < 0)
			throw IOException(__func__);
	}

	void HardwareFifo::addRouteFrom(int srcNodeAndFifoIndex)
	{
		int result = ::ioctl(handle, DYPLO_IOCTROUTE_TELL_FROM_LOGIC, srcNodeAndFifoIndex);
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
		else if (((unsigned int)result) != value)
			throw ArgumentOutOfRangeHasBeenTruncatedException(result);
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
#if defined(__linux__)
		unmap();
		::ioctl(handle, DYPLO_IOCDMABLOCK_FREE);
#endif
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

	void HardwareDMAFifo::flush()
	{
		std::vector<Block>::iterator current = blocks_head;
		do
		{
			Block* block = &(*blocks_head);
			if (block->state)
			{
				int status = ::ioctl(handle, DYPLO_IOCDMABLOCK_DEQUEUE, block);
				if (status < 0)
					throw IOException("DYPLO_IOCDMABLOCK_DEQUEUE");
			}
			++blocks_head;
			if (blocks_head == blocks.end())
				blocks_head = blocks.begin();
		} while (blocks_head != current);
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

	ssize_t FpgaImageFileWriter::processData(const void* data, const size_t length_bytes)
	{
		return output_file.write(data, length_bytes);
	}

	void FpgaImageFileWriter::verifyStaticID(const unsigned int user_id)
	{
		/* No need to check user ID here. */
		(void)user_id;
	}

	bool FpgaImageReader::parseDescriptionTag(const char* data, unsigned short size, bool *is_partial, unsigned int *user_id)
	{
		const char* end;
		bool result = false;
		while (size)
		{
			end = (const char*)memchr(data, ';', size);
			if (!end)
				end = data + size;
			if (!memcmp(data, "UserID=", 7))
			{
				if (user_id)
				{
					*user_id = strtoul(data + 7, NULL, 16);
				}
				result = true;
			}
			else if (is_partial && !memcmp(data, "PARTIAL=", 8))
			{
				*is_partial = data[8] == 'T';
			}
			size -= end - data;
			if (!size)
				return result;
			data = end + 1;
			--size;
		}
		return result;
	}
   
	void FpgaImageReader::processTag(char tag, unsigned short size, const void* data)
	{
		if (tag == 'a')
		{
			unsigned int user_id;
			bool is_partial = true; // for now, assumption is that the user only programs partials via the available ICAP interface
			bool has_user_id = parseDescriptionTag((const char*)data, size, &is_partial, &user_id);
			if (has_user_id && is_partial)
			{
				callback.verifyStaticID(user_id);
			}
		}
	}

	size_t FpgaImageReader::processFile(File& fpgaImageFile)
	{
		size_t total_data_bytes_processed = 0;

		std::vector<unsigned char> buffer(BUFFER_SIZE);
		unsigned char* buffer_start = &buffer[0];
		
		ssize_t bytes = fpgaImageFile.read_all(buffer_start, ALIGN_SIZE);
		if (bytes < 64)
		{
			throw TruncatedFileException();
		}
        
		if ((parse_u16(buffer_start) == 9) && /* Magic marker for .bit file */
			(parse_u16(buffer_start + 11) == 1) &&
			(buffer[13] == 'a'))
		{
			/* It's a bitstream, convert and flash */
			const unsigned char* end = buffer_start + bytes;
			unsigned char* data = buffer_start + 13;
            
			/* Browse through tag/value pairs looking for the "e" */
			for(;;)
			{
				unsigned char tag = *data;
				if (tag == 'e') /* Data tag, stop here */
				{
					break;
				}
				
				++data;
				if (data >= end)
				{
					throw TruncatedFileException();
				}
                
				unsigned short length = parse_u16(data);
				unsigned char* value = data + 2;
				data = value + length;
				if (data >= end)
				{
					throw TruncatedFileException();
				}

				processTag(tag, length, value);
			}
            
			++data;
			if (data+4 >= end)
			{
				throw TruncatedFileException();
			}
            
			unsigned int size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
			total_data_bytes_processed = size;
			data += 4;
			unsigned int total_bytes = bytes - (data - buffer_start);
			
			/* Move the remaining data to the buffer start to align
			 * it on word boundary. */
			memmove(buffer_start, data, total_bytes);
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
					bytes = fpgaImageFile.read_all(buffer_start + total_bytes, align);
					if (bytes < 0)
					{
						throw IOException();
					}
					else if (static_cast<unsigned int>(bytes) < align)
					{
						throw TruncatedFileException();
					}
					total_bytes += align;
				}
			}
            
			swap_buffer_aligned((unsigned int*)buffer_start, total_bytes >> 2);
			bytes = callback.processData(buffer_start, total_bytes);
			if (bytes < 0)
			{
				throw IOException();
			}
			else if (static_cast<unsigned int>(bytes) < total_bytes)
			{
				throw TruncatedFileException();
			}
            
			while (total_bytes < size)
			{
				unsigned int to_read = size - total_bytes < BUFFER_SIZE ? size - total_bytes : BUFFER_SIZE;
				bytes = fpgaImageFile.read_all(buffer_start, to_read);
				if (bytes < 0)
				{
					throw IOException();
				}
				else if (static_cast<unsigned int>(bytes) < to_read)
				{
					throw TruncatedFileException();
				}
                
				swap_buffer_aligned((unsigned int*)buffer_start, bytes >> 2);
				bytes = callback.processData(buffer_start, to_read);
				if (bytes < 0)
				{
					throw IOException();
				}
				else if (static_cast<unsigned int>(bytes) < to_read)
				{
					throw TruncatedFileException();
				}
                    
				total_bytes += bytes;
			}
		}
		else
		{
			if (*(unsigned int*)buffer_start == 0xFFFFFFFF)
			{
				/* Probably a bin file, they tend to start with all FF...
				 * so that seems a reasonable sanity check. */
			}
			else if (buffer_start[0] == 0x00 &&
					 *(unsigned int*)(&buffer_start[4]) == 0xFFFFFFFF)
			{
				/* Probably a .partial file, starts with 32-bit header:
				   Header format: (Hex) 00 {node_index} {static_image_id MSB} {static_image_id LSB}.
				   After the header, the .partial file should be like a .bin file, so it should start with FFFFFFFF. */

				// verify with static ID
				unsigned int partial_id = static_cast<unsigned int>(parse_u16(&buffer_start[2]));
				callback.verifyStaticID(partial_id);
			}
			else
			{
				throw std::runtime_error("Unrecognized bitstream format");
			}

			do
			{
				ssize_t bytes_written = callback.processData(buffer_start, bytes);
				if (bytes_written < bytes)
				{
					throw TruncatedFileException();
				}
                
				total_data_bytes_processed += bytes;
				bytes = fpgaImageFile.read_all(buffer_start, BUFFER_SIZE);
			}
			while (bytes);
		}

		return total_data_bytes_processed;
	}

	static const unsigned int programmer_blocksize = 65536;
	static const unsigned int programmer_numblocks = 2;
	static const unsigned int icap_nop_instruction = 0x20000000U;

	HardwareProgrammer::HardwareProgrammer(HardwareContext& context, HardwareControl& control) :
		dma_writer(NULL),
		cpu_fifo(NULL),
		context(context),
		control(control),
		reader(*this),
		dyplo_user_id_valid(false)
	{
		// check if there is an ICAP node
		int icap = control.getIcapNodeIndex();
		if (icap >= 0)
		{
			// try using the DMA node to set up the route
			try
			{
				dma_writer = new HardwareDMAFifo(context.openAvailableDMA(O_RDWR));

			}
			catch (const std::exception&)
			{
				// DMA failed, continue by trying the CPU node
				try
				{
					cpu_fifo = new HardwareFifo(context.openAvailableWriteFifo());
				}
				catch (const std::exception&)
				{
					throw IOException("No available CPU or DMA nodes to program ICAP");
				}
			}

			// set routes
			if (dma_writer != NULL)
			{
				dma_writer->reconfigure(dyplo::HardwareDMAFifo::MODE_COHERENT,
					programmer_blocksize, programmer_numblocks, false);
				dma_writer->addRouteTo(icap);
			} 
			else if (cpu_fifo != NULL)
			{
				cpu_fifo->addRouteTo(icap);
			}
		}
		else
		{
			throw IOException("No ICAP");
		}
	}
    
	unsigned int HardwareProgrammer::getDyploStaticID()
	{
		if (!dyplo_user_id_valid)
		{
			dyplo_user_id = control.readDyploStaticID();
			dyplo_user_id_valid = true;
		}
        
		return dyplo_user_id;
	}
    
	HardwareProgrammer::~HardwareProgrammer()
	{
		sendNOP(ESTIMATED_FIFO_SIZE);

		/* Wait for all DMA transactions to finish */
		if (dma_writer != NULL)
		{
			dma_writer->flush();
		}

		/* Flush data written to CPU fifo */
		if (cpu_fifo != NULL)
		{
			cpu_fifo->flush();
		}

		delete dma_writer;
		delete cpu_fifo;
	}

	unsigned int HardwareProgrammer::fromFile(const char *filename)
	{
		File input(filename, O_RDONLY);
		return fromFile(input);
	}

	unsigned int HardwareProgrammer::fromFile(File& file)
	{
		return reader.processFile(file);
	}

	unsigned int HardwareProgrammer::programNodeFromFile(unsigned int node_id, const char* filename)
	{
		HardwareConfig cfg(context.openConfig(node_id, O_RDONLY));
		// disconnect from backplane
		cfg.disableNode();
		unsigned int res = fromFile(filename);
		// reconnect to backplane
		cfg.enableNode();

		return res;
	}

	unsigned int HardwareProgrammer::programNodeFromFile(unsigned int node_id, File& file)
	{
		HardwareConfig cfg(context.openConfig(node_id, O_RDONLY));
		// disconnect from backplane
		cfg.disableNode();
		unsigned int res = fromFile(file);
		// reconnect to backplane
		cfg.enableNode();

		return res;
	}

	unsigned int HardwareProgrammer::sendNOP(unsigned int count)
	{
		/* Round up to multiple of 2 */
		count = (count + 1) & ~1;
		unsigned int bytes = count * sizeof(unsigned int);

		if (dma_writer != NULL)
		{
			/* Send a block of NOP instructions. This is to flush all
			 * internal queues, so that any data still in those queues
			 * has been written to ICAP, and only the NOP commands remain
			 * in the Dyplo queues. */
			HardwareDMAFifo::Block *block = dma_writer->dequeue();

			if (bytes > block->size) {
				bytes = block->size;
				count = bytes / sizeof(unsigned int);
			}
			block->bytes_used = bytes;
			unsigned int *data = (unsigned int*)block->data;
			for (unsigned int i = 0; i < count; ++i)
				data[i] = icap_nop_instruction;
			dma_writer->enqueue(block);

			return count;
		}
		else if (cpu_fifo != NULL)
		{
			unsigned int data[count];
			for (unsigned int i = 0; i < count; ++i)
				data[i] = icap_nop_instruction;

			cpu_fifo->write(data, bytes);

			return count;
		}

		return 0;
	}

	ssize_t HardwareProgrammer::processData(const void* data, const size_t length_bytes)
	{
		if (dma_writer != NULL)
		{
			HardwareDMAFifo::Block *block = dma_writer->dequeue();
			memcpy(block->data, data, length_bytes);
            
			/* Workaround for DMA node only supporting 64-bit writes */
			size_t length_dma_block = length_bytes;
			if (length_dma_block % 8)
			{
				if (length_dma_block % 8 <= 4) /* Add a NOP instruction */
					((unsigned int*)((char*)block->data + (length_dma_block & 0xFFFFFFF8)))[1] = icap_nop_instruction;
				length_dma_block = (length_dma_block + 7) & 0xFFFFFFF8U;
			}
			block->bytes_used = length_dma_block;
			dma_writer->enqueue(block);

			return length_bytes;
		}
		else if (cpu_fifo != NULL)
		{
			return cpu_fifo->write(data, length_bytes);
		}

		return 0;
	}

    void HardwareProgrammer::verifyStaticID(const unsigned int user_id)
	{
		if (getDyploStaticID() != user_id)
		{
			throw StaticPartialIDMismatchException();
		}
	}
}
