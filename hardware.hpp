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
		int openAvailableDMA(int access);
		int openAvailableDMAne(int access) throw();
		int openAvailableWriteFifo();
		int openAvailableReadFifo();

		// Find and returns nodeIds for which partial bitstreams are available for the given function.
		// returns the nodeIds as a 32-bit bitmask where every bit represents a node ID for which the partial bitstream is available.
		// LSB bit0 == node0, bit1 == node1 etc..
		unsigned int getAvailablePartitions(const char* function);
		static unsigned int getAvailablePartitionsIn(const char* path);

		// returns path of partial bitstream if found. Empty string otherwise.
		std::string findPartition(const char* function, int node_id);
		static std::string findPartitionIn(const char* path, int node_id);

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
			File(context.openControl(O_RDWR)),
			ctx(context)
		{}

		void routeDeleteAll();
		void routeAddSingle(unsigned char srcNode, unsigned char srcFifo, unsigned char dstNode, unsigned char dstFifo);
		int routeGetAll(Route* items, int n_items);
		void routeAdd(const Route* items, int n_items);
		// deletes all routes to the given destination node
		void routeDelete(char dstNode);

		// Will attempt to program via ICAP interface:
		unsigned int program(File &input);
		unsigned int program(const char* filename);

		unsigned int getEnabledNodes();
		void enableNodes(unsigned int mask);
		void disableNodes(unsigned int mask);
		void writeDyploLicense(unsigned long long license_blob);
		void writeDyploLicenseFile(const char* path_to_dyplo_license_file);
		unsigned int readDyploStaticID();

		bool isNodeEnabled(int node) { return (getEnabledNodes() & (1<<node)) != 0; }
		void enableNode(int node) { enableNodes(1<<node); }
		void disableNode(int node) { disableNodes(1<<node); }

		/* Retrieve ICAP node index. Negative means there is none. */
		int getIcapNodeIndex();

	private:
		HardwareContext& ctx;
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
		int getNodeIndex();
		static int getNodeAndFifoIndex(int file_descriptor);
		// format of parameter dstNodeAndFifoIndex: (nodeIndex | fifoIndex << 8)
		void addRouteTo(int dstNodeAndFifoIndex);
		// format of parameter srcNodeAndFifoIndex: (nodeIndex | fifoIndex << 8)
		void addRouteFrom(int srcNodeAndFifoIndex);
		unsigned int getDataTreshold();
		void setDataTreshold(unsigned int value);
		// only the 4 least significant bits are used for the user signal, giving you a value range of (0 - 15).
		void setUserSignal(int usersignal);
		int getUserSignal();
	};


	// class HardwareDMAFifo
	// DMA lets the logic directly transfer data to and from memory, which .
	// is shared with the CPU.
	// when large blocks of data need to be transferred, usage of DMA is
	// typically faster and uses less CPU overhead. Please note that
	// the CPU overhead is fixed and independent from the block data size.
	// NOTE: The size of a DMA transfer need to be 64-bit aligned.
	class HardwareDMAFifo: public HardwareFifo
	{
	public:
		/* This struct matches what the driver uses in its ioctl */
		struct InternalBlock
		{
			uint32_t id;          // 0-based index of the buffer
			uint32_t offset;      // Location of data in memory map
			uint32_t size;	      // Size of buffer
			uint32_t bytes_used;  // How much actually is in use
			uint16_t user_signal; // User signals (framing) either way
			uint16_t state;       // Who's owner of the buffer
		};
		struct Block: public InternalBlock
		{
			void* data; // Points to memory-mapped DMA buffer
		};
		HardwareDMAFifo(int file_descriptor);
		~HardwareDMAFifo();

		//Modes:
		//MODE_STANDALONE: This mode is used to give a Dyplo Process its own memory to read or write data to.
		//                 Typically the software application is only responsible for setting up a route to/from
		//                 a node to a DMA node and configuring this DMA node to stream the data to/from memory
		//                 refer to the "StandaloneConfiguration" struct for more information.
		//
		//MODE_RINGBUFFER: This is the easiest way to use DMA. After creating and configuring the object,
		//                 the application can simply use the 'File' base class' read and write methods to
		//                 'stream' data.
		//                 data will be copied to/from a ringbuffer in memory; the DMA transfers to/from Dyplo
		//                 are taken care for by a ringbuffer.
		//
		//MODE_COHERENT:   see MODE_STREAMING
		//MODE_STREAMING:  Using one of these modes is more complex for an application, but it enables processing
		//                 of data without making a copy first.
		//                 This is done by the use of multiple buffers. A user application must first
		//                 reconfigure the HardwareDMAFifo to allocate a number of buffers of a certain size.
		//                 further information is given at the methods "dequeue" and "enqueue".
		//
		//                 Both modes are functionally used in the same way by the application.
		//                 The difference is in the performance that can be reached and depends also on the
		//                 hardware architecture of the target. E.g. whether the CPU cache controller can
		//                 'see' the DMA transfers on the memory bus.
		//                 Summarized the difference is as follows:
		//                   MODE_COHERENT: calls to enqueue and dequeue are relatively cheap, access to the
		//                                 allocated memory buffer may be slow (typically because it is not cached)
		//                   MODE_STREAMING: calls to enqueue and dequeue are relatively expensive and access
		//                                to the allocated memory is fast (typically because it is cached).
		//                                The enqueue and dequeue are slower because they have to arrange
		//                                cache and memory consistency.
		enum {
			MODE_STANDALONE = 0,
			MODE_RINGBUFFER = 1,
			MODE_COHERENT = 2,
			MODE_STREAMING= 3,
		};
		/* Allocates "count" buffers of "size" bytes each. Size will be
		 * rounded up to page size by the driver, count will max at 8.
		 * Must be called before dequeue.
		 * The readonly flag determines the memory map access (only applicable on Linux, ignored for RTEMS).
		 * Note: Parameter "size" needs to be 64-bit aligned.
		*/
		void reconfigure(unsigned int mode, unsigned int size, unsigned int count, bool readonly);
		/* Explicitly dispose of allocated DMA buffers. Also called from
		 * destructor */
		void dispose();

		/* About dequeue and enqueue
		* 1. dequeue returns the next available buffer and gives ownership of
		*    that buffer to the users application
		*    - The number of buffers N was set with the method 'reconfigure'. An
		*      application can call dequeue N times without returning a buffer
		*      by calling enqueue.
		*    - Calling dequeue will throw an error if called more than N times
		*      without calling enqueue first.
		* 2. enqueue is used to return ownership of the buffer to the Dyplo driver
		*     -the driver will start a DMA transfer to or from the buffer
		*
		* typical usage
		* A. WRITING DATA from memory to Dyplo using 2 buffers
		*
		*    CODE EXAMPLE:
		*    HardwareDMAFifo dmaWriteBufs(File('/dev/dyplod0', O_RW);
		*    dmaWriteBufs.reconfigure (MODE_COHERENT, 128, 2, false);
		*    Block *one = dmaWriteBufs.dequeue();
		*    Block *two = dmaWriteBufs.dequeue();
		*    one->bytes_used = 128;   //fill meta data of block one. NOTE: needs to be 64-bit aligned.
		*    memcpy(one->data, mysource1, 128);  //fill buffer with data to write
		*    dmaWriteBufs.enqueue(one);  //this will start the actual transfer of data from buffer 'one' to Dyplo
		*    //while this transfer is busy you can fill buffer 'two'
		*    two->bytes_used = 128;   //fill meta data of block two. NOTE: needs to be 64-bit aligned.
		*    memcpy(two->data, mysource2, 128);  //fill buffer with data to write
		*    dmaWriteBufs.enqueue(two); //this will start the transfer of data from buffer 'two' to
		*                               //Dyplo once the transfer from the previous buffer is ready
		*    //continue by reusing the first block
		*    one = dmaWriteBufs.dequeue();    // will block until the previous transfer is ready
		*    ....
		*
		* B. READING DATA to memory from Dyplo using 2 buffers
		*
		*    CODE EXAMPLE
		*    HardwareDMAFifo dmaReadBufs(File('/dev/dyplod1', O_RDONLY);
		*    dmaReadBufs.reconfigure (MODE_COHERENT, 128, 2, false);
		*    Block *one = dmaReadBufs.dequeue();
		*    Block *two = dmaReadBufs.dequeue();
		*    one->bytes_used = 128;   //set the number of bytes to read. NOTE: needs to be 64-bit aligned
		*    dmaReadBufs.enqueue(one);    //this will start the actual transfer of data from Dyplo to buffer 'one'
		*    two->bytes_used = 128;   //set the number of bytes to read. NOTE: needs to be 64-bit aligned.
		*    dmaReadBufs.enqueue(two);    //this will start the actual transfer of data from Dyplo to buffer 'two' (when one is ready)
		*    one = dmaReadBufs.dequeue(); // will block until the transfer is ready
		*    memcpy(mydata1, one->data, 128);  //do something with the data
		*    dmaReadBufs.enqueue(one);         //starts (queues) another read of 128 bytes
		*    two = dmaReadBufs.dequeue();       // will block until the transfer is ready
		*    memcpy(mydata1, twp->data, 128);   //do something with the data
		*    dmaReadBufs.enqueue(two);          //starts (queues) another read of 128 bytes
		*     //and so on.
		*/
		Block* dequeue();
		/* Send block to device. The block should have been obtained
		 * using dequeue. Does not block. */
		void enqueue(Block* block);
		/* Wait until all blocks have been processed in hardware. */
		void flush();

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

	class FpgaImageReaderCallback
	{
	public:
		// returns amount of bytes processed
		virtual ssize_t processData(const void* data, const size_t length_bytes) = 0;
		virtual void verifyStaticID(const unsigned int user_id) = 0;
	};

	class FpgaImageFileWriter : public FpgaImageReaderCallback
	{
	public:
		FpgaImageFileWriter(File& output) :
			output_file(output)
		{
		}

		~FpgaImageFileWriter()
		{
			output_file.flush();
		}

		// FpgaImageReaderCallback interface
		virtual ssize_t processData(const void* data, const size_t length_bytes);
		virtual void verifyStaticID(const unsigned int user_id);
	private:
		File& output_file;
	};

	static const size_t ALIGN_SIZE = 4 * 1024;
	static const size_t BUFFER_SIZE = 8 * ALIGN_SIZE;
	static const size_t ESTIMATED_FIFO_SIZE = 256;

	// can read .bit and .bin and .partial files and will output the data to be flashed on the FPGA
	class FpgaImageReader
	{
	public:
		FpgaImageReader(FpgaImageReaderCallback& resultCallback) :
			callback(resultCallback)
		{
		}

		// returns amount of bytes of FPGA data read
		size_t processFile(File& fpgaImageFile);

	protected:
		virtual bool parseDescriptionTag(const char* data, unsigned short size, bool *is_partial, unsigned int *user_id);
		virtual void processTag(char tag, unsigned short size, const void *data);

	private:
		FpgaImageReaderCallback& callback;
	};

	// can program via ICAP
	class HardwareProgrammer : public FpgaImageReaderCallback
	{
	public:
		HardwareProgrammer(HardwareContext& context, HardwareControl& control);
		~HardwareProgrammer();

		// returns amount of bytes read
		unsigned int fromFile(const char *filename);
		unsigned int fromFile(File& file);
		unsigned int programNodeFromFile(unsigned int node_id, const char *filename);
		unsigned int programNodeFromFile(unsigned int node_id, File& file);

		// FpgaImageReaderCallback interface
		virtual ssize_t processData(const void* data, const size_t length_bytes);
		virtual void verifyStaticID(const unsigned int user_id);
	protected:
		// for DMA data stream to ICAP
		HardwareDMAFifo* dma_writer;

		// for CPU data stream to ICAP
		HardwareFifo*    cpu_fifo;
	private:
		/* Flush out queues by sending a string of NOPs */
		unsigned int sendNOP(unsigned int count);
		unsigned int getDyploStaticID();

		HardwareContext& context;
		HardwareControl& control;
		FpgaImageReader reader;

		unsigned int dyplo_user_id;
		bool dyplo_user_id_valid;
	};

}
