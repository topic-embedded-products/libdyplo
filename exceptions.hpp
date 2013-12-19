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
	
	class TruncatedFileException: public EndOfFileException
	{
	public:
		virtual const char* what() const throw();
	};

	class IOException: public std::exception
	{
	public:
		IOException();
		IOException(const char* extra_context);
		IOException(int error_code);
		IOException(const char* extra_context, int error_code);
		~IOException() throw () {} /* Fails compiling without this */
		virtual const char* what() const throw();
		int m_errno;
		std::string context;
	protected:
		mutable std::string buffer; /* mutable because of "const" what */
	};
}
