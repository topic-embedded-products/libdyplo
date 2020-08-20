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
#include <poll.h>
#include "filequeue.hpp"

namespace dyplo
{
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
