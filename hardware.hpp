/*
 * hardware.hpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. <Mike Looijmans> (http://www.topic.nl).
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
		unsigned int getAvailablePartitions(const char* function);
		std::string findPartition(const char* function, int partition);
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
		void routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo);
		int routeGetAll(Route* items, int n_items);
		void routeAdd(const Route* items, int n_items);
		void routeDelete(char node);
		
		unsigned int getEnabledNodes();
		void enableNodes(unsigned int mask);
		void disableNodes(unsigned int mask);
		
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
	};

	class HardwareFifo: public File
	{
	public:
		HardwareFifo(int file_descriptor): File(file_descriptor) {}
		void reset(); /* Reset fifo */
	};
}
