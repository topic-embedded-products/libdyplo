#include <fcntl.h>
#include <poll.h>
#include "filequeue.hpp"

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
	IOException::IOException(int code):
		m_errno(code)
	{
	}

	const char* IOException::what() const throw()
	{
		return strerror(m_errno);
	}
	
	int set_non_blocking(int file_handle)
	{
		int flags = fcntl(file_handle, F_GETFL, 0);
		return fcntl(file_handle, F_SETFL, flags | O_NONBLOCK);
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
		int result = poll(fds, 2, 1000);
		if (result == -1)
			throw std::runtime_error("poll() failed");
		if (fds[1].revents)
			throw InterruptedException();
	}
	
	void FilePollScheduler::wait_writeable(int filehandle)
	{
		if (m_interrupted)
			throw InterruptedException();
	}
	
	void FilePollScheduler::interrupt()
	{
		char dummy = 'q';
		m_interrupted = true;
		if (::write(m_internal_pipe.write_handle(), &dummy, 1) < 0)
			throw IOException();
	}
}
