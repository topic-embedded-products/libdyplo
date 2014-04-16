/*
 * dyplodemohdlapp.cpp
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
#include "hardware.hpp"
#include <stdlib.h>
#include <iostream>

static const int n_hdl = 2;
static const int n_fifos = 4;

static void usage()
{
	std::cerr << "usage: .. [-v] value value ..\n"
		" -v     verbose mode.\n"
		" value  up to 8 integer values to write to config block\n";
}

int main(int argc, char** argv)
{
	bool verbose = false;
	dyplo::HardwareControl::Route routes[n_hdl * n_fifos * 2];
	dyplo::HardwareControl::Route *entry = routes;
	for (int hdl = 0; hdl < n_hdl; ++hdl)
	{
		int base = hdl * n_fifos;
		for (int fifo = 0; fifo < n_fifos; ++fifo)
		{
			/* From CPU to HDL */
			entry->srcFifo = base + fifo;
			entry->srcNode = 0;
			entry->dstFifo = fifo;
			entry->dstNode = hdl + 1;
			++entry;
			/* From HDL to CPU */
			entry->srcFifo = fifo;
			entry->srcNode = hdl + 1;
			entry->dstFifo = base + fifo;
			entry->dstNode = 0;
			++entry;
		}
	}
	dyplo::HardwareContext ctrl;
	/* Parse commandline */
	{
		int data[n_fifos * n_hdl];
		int index = 0;
		while (--argc)
		{
			++argv;
			const char* arg = *argv;
			if (arg[0] == '-')
			{
				switch (arg[1])
				{
					case 'v':
						verbose = true;
						continue;
					case 'h':
						usage();
						return 1;
				}
			}
			int v = atoi(arg);
			data[index++] = v;
		}
		int hdl = 1;
		int* current = data;
		while (index > 0)
		{
			int n_ints = index > n_fifos ? n_fifos : index;
			dyplo::File f(ctrl.openConfig(hdl, O_WRONLY));
			if (f.write(current, n_ints * sizeof(int)) < (ssize_t)(n_ints * sizeof(int)))
				throw std::runtime_error("Failed to write config data");
			index -= n_ints;
			current += n_ints;
			++hdl;
		}
	}
	try
	{
		if (verbose)
		{
			std::cout << "Route setup:\n";
			for (int i = 0; i < n_hdl * n_fifos * 2; ++i)
			{
				std::cout << (int)routes[i].srcNode << "," << (int)routes[i].srcFifo <<
					" -> " << (int)routes[i].dstNode << "," << (int)routes[i].dstFifo <<
					"\n";
			}
			std::cout << "Configuration data:\n";
			for (int hdl = 1; hdl <= n_hdl; ++hdl)
			{
				dyplo::File cfg(ctrl.openConfig(hdl, O_RDONLY));
				int data[n_fifos];
				if (cfg.read(data, sizeof(data)) != sizeof(data))
					throw std::runtime_error("Failed to read config data");
				std::cout << "Node #" << hdl << ": ";
				for (int fifo = 0; fifo < n_fifos; ++fifo)
					std::cout << data[fifo] << ", ";
				std::cout << "\n";
			}
		}
		dyplo::HardwareControl(ctrl).routeAdd(routes, n_hdl * n_fifos * 2);
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Failed to setup route: " << ex.what() << std::endl;
	}
}
