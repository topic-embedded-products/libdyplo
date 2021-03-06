/*
 * fileio.cpp
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
#include "fileio.hpp"
#include "exceptions.hpp"
#include <errno.h>
#include <string.h>

namespace dyplo
{
	const char* EndOfFileException::what() const throw()
	{
		return "End of file";
	}
	const char* EndOfInputException::what() const throw()
	{
		return "End of input file";
	}
	const char* EndOfOutputException::what() const throw()
	{
		return "End of output file";
	}

	IOException::IOException():
		m_errno(errno)
	{
	}
	IOException::IOException(const char* extra_context):
		m_errno(errno),
		context(extra_context)
	{
	}
	IOException::IOException(int code):
		m_errno(code)
	{
	}
	IOException::IOException(const char* extra_context, int code):
		m_errno(code),
		context(extra_context)
	{
	}

	const char* IOException::what() const throw()
	{
		if (context.empty())
			return strerror(m_errno);
		if (buffer.empty())
		{
			buffer = strerror(m_errno);
			buffer += " context: ";
			buffer += context;
		}
		return buffer.c_str();
	}

	int set_non_blocking(int file_handle)
	{
		int flags = fcntl(file_handle, F_GETFL, 0);
		return fcntl(file_handle, F_SETFL, flags | O_NONBLOCK);
	}

	File::File(const char* filename, int access):
		handle(::open(filename, access))
	{
		if (handle == -1)
			throw dyplo::IOException(filename);
	}

	File::File(const File& other):
		handle(::dup(other.handle))
	{
		if (handle == -1)
			throw dyplo::IOException();
	}

	/* Read all file contents until count bytes or end-of-file. Unlike
	 * regular read, never returns a short read until EOF. */
	ssize_t File::read_all(void *buf, size_t count)
	{
		size_t bytes = 0;
		for (;;)
		{
			ssize_t result = ::read(handle, buf, count);
			if (result <= 0) {
				if ((result == 0) || (bytes != 0))
					return bytes;

				throw dyplo::IOException();
			}
			bytes += result;
			count -= result;
			if (count == 0)
				return bytes;
			buf = ((char*)buf) + result;
		}
	}

	void File::flush()
	{
		if (fsync(handle) == -1)
		{
			if (errno == EROFS || errno == EINVAL)
			{
				// no need to throw exception:
				// handle is bound to a special file which does not support synchronization
			}
			else
			{
				throw dyplo::IOException();
			}
		}
	}

	void File::fcntl_set_flag(long flag)
	{
		int flags = ::fcntl(handle, F_GETFL, 0);
		if (flags < 0)
			throw dyplo::IOException();
		if (::fcntl(handle, F_SETFL, flags | flag) < 0)
			throw dyplo::IOException();
	}

	bool File::poll_for_incoming_data(struct timeval *timeout)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(handle, &fds);
		int status = select(handle+1, &fds, NULL, NULL, timeout);
		if (status < 0)
			throw dyplo::IOException(__func__);
		return status && FD_ISSET(handle, &fds);
	}

	bool File::poll_for_outgoing_data(struct timeval *timeout)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(handle, &fds);
		int status = select(handle+1, NULL, &fds, NULL, timeout);
		if (status < 0)
			throw dyplo::IOException(__func__);
		return status && FD_ISSET(handle, &fds);
	}

	bool File::poll_for_incoming_data(int timeout_in_seconds)
	{
		struct timeval timeout;
		timeout.tv_sec = timeout_in_seconds;
		timeout.tv_usec = 0;
		return poll_for_incoming_data(&timeout);
	}

	bool File::poll_for_outgoing_data(int timeout_in_seconds)
	{
		struct timeval timeout;
		timeout.tv_sec = timeout_in_seconds;
		timeout.tv_usec = 0;
		return poll_for_outgoing_data(&timeout);
	}

	off_t File::get_size(const char* path)
	{
		struct stat info;
		int status = ::stat(path, &info);
		if (status < 0)
			throw dyplo::IOException(path);
		return info.st_size;
	}
}
