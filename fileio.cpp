#include "fileio.hpp"
#include "exceptions.hpp"

namespace dyplo
{
	File::File(const char* filename, int access):
		handle(::open(filename, access))
	{
		if (handle == -1)
			throw dyplo::IOException();
	}

	bool File::poll_for_incoming_data(int timeout_in_seconds)
	{
		struct timeval timeout;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(handle, &fds);
		timeout.tv_sec = timeout_in_seconds;
		timeout.tv_usec = 0;
		int status = select(handle+1, &fds, NULL, NULL, &timeout);
		if (status < 0)
			throw dyplo::IOException();
		return status && FD_ISSET(handle, &fds);
	}

	bool File::poll_for_outgoing_data(int timeout_in_seconds)
	{
		struct timeval timeout;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(handle, &fds);
		timeout.tv_sec = timeout_in_seconds;
		timeout.tv_usec = 0;
		int status = select(handle+1, NULL, &fds, NULL, &timeout);
		if (status < 0)
			throw dyplo::IOException();
		return status && FD_ISSET(handle, &fds);
	}
}
