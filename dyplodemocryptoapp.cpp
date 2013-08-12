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
