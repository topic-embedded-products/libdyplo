#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "exceptions.hpp"

namespace dyplo
{
	/* Wrapper around generic OS files */
	class File
	{
	public:
		int handle;
		/* Same as File(::open(filename, access)) */
		File(const char* filename, int access = O_RDONLY);
		/* Intended to be called as "File(::open(something...));"
		 * takes ownership of handle, and raises exception when
		 * handle is invalid. */
		File(int file_descriptor):
			handle(file_descriptor)
		{
			if (handle == -1)
				throw dyplo::IOException();
		}

		~File()
		{
			::close(handle);
		}
		
		operator int() const { return handle; }

		ssize_t write(const void *buf, size_t count)
		{
			ssize_t result = ::write(handle, buf, count);
			if (result < 0)
				throw dyplo::IOException();
			return result;
		}

		ssize_t read(void *buf, size_t count)
		{
			ssize_t result = ::read(handle, buf, count);
			if (result < 0)
				throw dyplo::IOException();
			return result;
		}
		
		bool poll_for_incoming_data(int timeout_in_seconds);
		bool poll_for_outgoing_data(int timeout_in_seconds);
	private:
		File(const File& other); /* Prevent copy constructor */
	};
}
