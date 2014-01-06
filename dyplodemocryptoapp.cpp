/*
 * dyplodemocryptoapp.cpp
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
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cooperativescheduler.hpp"
#include "cooperativeprocess.hpp"
#include "filequeue.hpp"

/*
 * Setup the following queue:
 * stdin -> crypto -> stdout
 */

#define BLOCKSIZE (32 * 1024)

class Crypto
{
public:
	Crypto(int input_fd, int output_fd):
		input(file_scheduler, input_fd, BLOCKSIZE),
		output(file_scheduler, output_fd, BLOCKSIZE)
	{
	}

	void process()
	{
		unsigned int count;
		char *src;
		char *dest;

		try
		{
			for (;;)
			{
				count = input.begin_read(src, 1);
				output.begin_write(dest, count);
				if (count <= 0)
					return;
				for (unsigned int i = count; i != 0; --i)
					*dest++ = *src++ ^ 1;
				output.end_write(count);
				input.end_read(count);
			}
		}
		catch (const dyplo::EndOfFileException&)
		{
		}
	}

protected:
	dyplo::FilePollScheduler file_scheduler;
	dyplo::FileInputQueue<char> input;
	dyplo::FileOutputQueue<char> output;
};

int main(int argc, char** argv)
{
	int fd_in = 0;
	int fd_out = 1;
	if (argc > 1)
	{
		fd_in = open(argv[1], O_RDONLY);
		if (fd_in == -1)
		{
			perror("Failed to open input file");
			return 1;
		}
		if (argc > 2)
		{
			fd_out = open(argv[2], O_WRONLY);
			if (fd_out == -1)
			{
				perror("Failed to open output file");
				return 1;
			}
		}
	}
	try
	{
		Crypto crypto(fd_in, fd_out);
		crypto.process();
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error while processing data: " << ex.what() << std::endl;
		return 1;
	}
	return 0;
}
