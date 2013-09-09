#pragma once
#include <sys/types.h>
#include <dirent.h>
#include "exceptions.hpp"

namespace dyplo
{
	/* Wrapper around dirent */
	class DirectoryListing
	{
		DIR* handle;
	public:
		DirectoryListing(const char* path):
			handle(::opendir(path))
		{
			if (!handle)
				throw dyplo::IOException();
		}
		
		~DirectoryListing()
		{
			::closedir(handle);
		}
	
		struct dirent* next()
		{
			return readdir(handle);
		}
	};
}
