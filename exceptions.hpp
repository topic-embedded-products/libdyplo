#pragma once

#include <stdexcept>

namespace dyplo
{
	class InterruptedException: public std::exception
	{
	public:
		virtual const char* what() const throw();
	};
	
	class EndOfFileException: public std::exception
	{
	public:
		virtual const char* what() const throw();
	};
	
	class EndOfInputException: public EndOfFileException
	{
	public:
		virtual const char* what() const throw();
	};

	class EndOfOutputException: public EndOfFileException
	{
	public:
		virtual const char* what() const throw();
	};
	
	class IOException: public std::exception
	{
	public:
		IOException();
		virtual const char* what() const throw();
		int m_errno;
	};
}
