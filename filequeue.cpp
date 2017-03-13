/*
 * filequeue.cpp
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
#include <fcntl.h>
#include <sys/poll.h>
#include "filequeue.hpp"

#ifdef __rtems__
// TODO
#define POLLRDHUP 0
#endif

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

	const char* IOException::what() const _GLIBCXX_USE_NOEXCEPT
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

	FilePollScheduler::FilePollScheduler():
		m_interrupted(false)
	{
		set_non_blocking(m_internal_pipe.read_handle());
	}

	void FilePollScheduler::wait_readable(int filehandle)
	{
		if (m_interrupted)
			throw InterruptedException();
		struct pollfd fds[2];
		fds[0].fd = filehandle;
		fds[0].events = POLLIN | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
		fds[1].fd = m_internal_pipe.read_handle();
		fds[1].events = POLLIN | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
		int result = poll(fds, 2, -1);
		if (result == -1)
			throw std::runtime_error("poll() failed");
		if (fds[1].revents)
			throw InterruptedException();
	}

	void FilePollScheduler::wait_writeable(int filehandle)
	{
		if (m_interrupted)
			throw InterruptedException();
		struct pollfd fds[2];
		fds[0].fd = filehandle;
		fds[0].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
		fds[1].fd = m_internal_pipe.read_handle();
		fds[1].events = POLLIN | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
		int result = poll(fds, 2, -1);
		if (result == -1)
			throw std::runtime_error("poll() failed");
		if (fds[1].revents)
			throw InterruptedException();
	}

	void FilePollScheduler::interrupt()
	{
		char dummy = 'q';
		m_interrupted = true;
		if (::write(m_internal_pipe.write_handle(), &dummy, 1) < 0)
			throw IOException();
	}

	void FilePollScheduler::reset()
	{
		char dummy[16];
		while (::read(m_internal_pipe.read_handle(), dummy, sizeof(dummy)) == sizeof(dummy))
		{}
		m_interrupted = false;
	}
}
