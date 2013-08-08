#include <iostream>
#include <sstream>

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
	try
	{
		Crypto crypto(0, 1);
		crypto.process();
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error while processing data: " << ex.what() << std::endl;
		return 1;
	}
	return 0;
}
