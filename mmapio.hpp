/*
 * mmapio.hpp
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
#include "fileio.hpp"
#include <sys/mman.h>

namespace dyplo
{
	/* Wrapper around generic memory mapped IO */
	class MemoryMap
	{
	public:
		void* memory;
		const size_t size;
		
		MemoryMap(int fd, off_t offset, size_t map_size, int prot=PROT_WRITE|PROT_READ, int flags=MAP_SHARED):
			memory(::mmap(NULL, map_size, prot, flags, fd, offset)),
			size(map_size)
		{
			if (memory == MAP_FAILED)
				throw IOException();
		}
		~MemoryMap()
		{
			munmap(memory, size);
		}
		
		operator void*() const
			{ return memory; }
		operator unsigned int*() const
			{ return reinterpret_cast<unsigned int*>(memory); }
	};
}
