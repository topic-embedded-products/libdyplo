#pragma once

#include <string>
#include "fileio.hpp"

namespace dyplo
{
	class HardwareContext
	{
	public:
		struct Route {
			unsigned char dstFifo; /* LSB */
			unsigned char dstNode;
			unsigned char srcFifo;
			unsigned char srcNode; /* MSB */
		};

		HardwareContext();
		HardwareContext(const std::string& driver_prefix);
		void routeDeleteAll();
		void routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo);
		int routeGetAll(Route* items, int n_items);
		void routeAdd(const Route* items, int n_items);

		/* Returns a file handle that must be closed. Suggest to use
		 * as "dyplo::File(hwc.openFifo(..)); " */
		int openFifo(int fifo, int access);
		int openConfig(int index, int access);
		
		/* Program device using a partial bitstream */
		static void setProgramMode(bool is_partial_bitstream);
		static bool getProgramMode();
		static unsigned int program(File &output, const char* filename);
		static unsigned int program(const char* filename);
	protected:
		File control_device;
		std::string prefix;
	};
}
