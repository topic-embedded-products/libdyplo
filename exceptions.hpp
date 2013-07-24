#pragma once

#include <stdexcept>

namespace dyplo
{
	class InterruptedException: public std::exception
	{
		virtual const char* what() const throw();
	};
}
