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
