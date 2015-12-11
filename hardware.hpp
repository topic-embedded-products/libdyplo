/*
 * hardware.hpp
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
#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "fileio.hpp"

namespace dyplo
{
	class HardwareContext
	{
	public:
		HardwareContext();
		HardwareContext(const std::string& driver_prefix);
		/* Return a file handle that must be closed. Suggest to use
		 * as "dyplo::File(hwc.openFifo(..)); " */
		int openFifo(int fifo, int access);
		int openConfig(int index, int access);
		/* Use HardwareControl as more convenient access */
		int openControl(int access);
		int openDMA(int index, int access);

		/* Program device using a partial bitstream */
		static void setProgramMode(bool is_partial_bitstream);
		static bool getProgramMode();
		static unsigned int program(File &output, File &input);
		static unsigned int program(File &output, const char* filename);
		static unsigned int program(const char* filename);
		static unsigned int program(File &input);

		/* Find bitfiles in directories */
		unsigned int getAvailablePartitions(const char* function);
		std::string findPartition(const char* function, int partition);
		static unsigned int getAvailablePartitionsIn(const char* path);
		static std::string findPartitionIn(const char* path, int partition);
		void setBitstreamBasepath(const std::string& value) { bitstream_basepath = value; }
		void setBitstreamBasepath(const char* value) { bitstream_basepath = value; }
	protected:
		std::string prefix;
		std::string bitstream_basepath;
	};

	class HardwareControl: public File
	{
	public:
		struct Route {
			unsigned char dstFifo; /* LSB */
			unsigned char dstNode;
			unsigned char srcFifo;
			unsigned char srcNode; /* MSB */
		};

		HardwareControl(HardwareContext& context):
			File(context.openControl(O_RDWR))
		{}

		void routeDeleteAll();
		void routeAddSingle(unsigned char srcNode, unsigned char srcFifo, unsigned char dstNode, unsigned char dstFifo);
		int routeGetAll(Route* items, int n_items);
		void routeAdd(const Route* items, int n_items);
		void routeDelete(char node);

		unsigned int getEnabledNodes();
		void enableNodes(unsigned int mask);
		void disableNodes(unsigned int mask);
		void writeDyploLicense(unsigned long long license_blob);
		void writeDyploLicenseFile(const char* path_to_dyplo_license_file);
		unsigned int readDyploStaticID();

		bool isNodeEnabled(int node) { return (getEnabledNodes() & (1<<node)) != 0; }
		void enableNode(int node) { enableNodes(1<<node); }
		void disableNode(int node) { disableNodes(1<<node); }
	};

	class HardwareConfig: public File
	{
	public:
		HardwareConfig(int file_descriptor): File(file_descriptor) {}
		HardwareConfig(HardwareContext& context, int index, int access = O_RDWR):
			File(context.openConfig(index, access)) {}
		void resetWriteFifos(unsigned int mask);
		void resetReadFifos(unsigned int mask);
		static void resetWriteFifos(int file_descriptor, unsigned int mask);
		static void resetReadFifos(int file_descriptor, unsigned int mask);
		bool isNodeEnabled();
		void enableNode();
		void disableNode();
		int getNodeIndex();
		static int getNodeIndex(int file_descriptor);
		void deleteRoutes();
		static void deleteRoutes(int file_descriptor);
	};

	class HardwareFifo: public File
	{
	public:
		HardwareFifo(int file_descriptor): File(file_descriptor) {}
		void reset(); /* Reset fifo */
		int getNodeAndFifoIndex(); /* Returns node | fifo << 8 */
		static int getNodeAndFifoIndex(int file_descriptor);
		void addRouteTo(int destination);
		void addRouteFrom(int source);
		unsigned int getDataTreshold();
		void setDataTreshold(unsigned int value);
		void setUserSignal(int usersignal);
		int getUserSignal();
	};
	
	class HardwareDMAFifo: public HardwareFifo
	{
	public:
		/* This struct matches what the driver uses in its ioctl */
		struct InternalBlock
		{
			uint32_t id;	/* 0-based index of the buffer */
			uint32_t offset;	/* Location of data in memory map */
			uint32_t size;	/* Size of buffer */
			uint32_t bytes_used; /* How much actually is in use */
			uint16_t user_signal; /* User signals (framing) either way */
			uint16_t state; /* Who's owner of the buffer */
		};
		struct Block: public InternalBlock
		{
			void* data; /* Points to memory-mapped DMA buffer */
		};
		HardwareDMAFifo(int file_descriptor);
		~HardwareDMAFifo();

		enum {
			MODE_STANDALONE = 0,
			MODE_RINGBUFFER = 1,
			MODE_COHERENT = 2,
			MODE_STREAMING= 3,
		};
		/* Allocates "count" buffers of "size" bytes each. Size will be
		 * rounded up to page size by the driver, count will max at 8.
		 * Must be called before dequeue.
		 * The readonly flag determines the memory map access. */
		void reconfigure(unsigned int mode, unsigned int size, unsigned int count, bool readonly);
		/* Explicitly dispose of allocated DMA buffers. Also called from
		 * destructor */
		void dispose();
		
		/* Get a block from the queue. In non-blocking mode, returns
		 * NULL when it would block. When writing, this is the first
		 * thing to do. Only valid for MODE_COHERENT and MODE_STREAMING
		 * configurations. */
		Block* dequeue();
		/* Send block to device. The block should have been obtained
		 * using dequeue. Does not block. */
		void enqueue(Block* block);

		void standaloneStart(bool direction_from_logic);
		void standaloneStop(bool direction_from_logic);
		/* This struct matches what the driver uses in its ioctl */
		struct StandaloneConfiguration
		{
			uint32_t offset;
			uint32_t burst_size;
			uint32_t incr_a;
			uint32_t iterations_a;
			uint32_t incr_b;
			uint32_t iterations_b;
			uint32_t incr_c;
			uint32_t iterations_c;

		};
		void setStandaloneConfig(const StandaloneConfiguration *config, bool direction_from_logic);
		void getStandaloneConfig(StandaloneConfiguration *config, bool direction_from_logic);
		/* For test and diagnostic */
		unsigned int count() const { return blocks.size(); }
		const Block* at(uint32_t id) const { return &blocks[id]; }

	protected:
		void resize(unsigned int number_of_blocks, unsigned int blocksize);
		void unmap();
		std::vector<Block> blocks;
		std::vector<Block>::iterator blocks_head;
	};

	/* Define equality operators */
	bool operator==(const dyplo::HardwareDMAFifo::StandaloneConfiguration& lhs, const dyplo::HardwareDMAFifo::StandaloneConfiguration& rhs);
	inline bool operator!=(const dyplo::HardwareDMAFifo::StandaloneConfiguration& lhs, const dyplo::HardwareDMAFifo::StandaloneConfiguration& rhs) {return !(lhs == rhs);}

}
