#pragma once

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
		void routeDeleteAll();
		void routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo);
		int routeGetAll(Route* items, int n_items);
		void routeAdd(const Route* items, int n_items);

		int openFifo(int fifo, int access);
		
	protected:
		File control_device;
	};
}
