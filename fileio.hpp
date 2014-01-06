/*
 * fileio.hpp
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
		
		off_t seek(off_t offset, int whence = SEEK_SET)
		{
			off_t result = ::lseek(handle, offset, whence);
			if (result == (off_t)-1)
				throw dyplo::IOException();
			return result;
		}
		
		bool poll_for_incoming_data(int timeout_in_seconds);
		bool poll_for_outgoing_data(int timeout_in_seconds);
		bool poll_for_incoming_data(struct timeval *timeout);
		bool poll_for_outgoing_data(struct timeval *timeout);
		
		static off_t get_size(const char* path);
	private:
		File(const File& other); /* Prevent copy constructor */
	};
}
