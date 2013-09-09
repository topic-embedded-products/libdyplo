#pragma once

#include <string>
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
		
		/* Program device using a partial bitstream */
		static void setProgramMode(bool is_partial_bitstream);
		static bool getProgramMode();
		static unsigned int program(File &output, const char* filename);
		static unsigned int program(const char* filename);
		
		/* Find bitfiles in directories */
		static unsigned int getAvailablePartitions(const char* basepath, const char* function);
	protected:
		std::string prefix;
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
		void routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo);
		int routeGetAll(Route* items, int n_items);
		void routeAdd(const Route* items, int n_items);
	};
}
