#include "hardware.hpp"
#include <stdlib.h>
#include <iostream>
#include <getopt.h>
#include <vector>
#include <sstream>

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-v] sn,sf,dn,df ...\n"
		" -v    verbose mode.\n"
		" sn,sf,dn,df	Source node, fifo, destination node and fifo.\n";
}

class ParseError: public std::exception
{
	std::string msg;
public:
	ParseError(const char *txt, const char *what):
		msg("Failed to parse: '")
	{
		msg += txt;
		msg += "' at ";
		msg += what;
	}
	const char* what() const throw()
	{
		return msg.c_str();
	}
	~ParseError() throw()
	{
	}
};

static bool is_digit(char c)
{
	return (c >= '0') && (c <= '9');
}

static int read_int(char const **txt)
{
	const char *d = *txt;
	int result = 0;
	while (is_digit(*d))
	{
		result = result * 10 + (*d - '0');
		++d;
	}
	while (*d && !is_digit(*d))
		++d; /* Skip separator */
	*txt = d;
	return result;
}

static dyplo::HardwareContext::Route parse_route(const char *txt)
{
	dyplo::HardwareContext::Route result;
	int v;
	const char* d = txt;
	v = read_int(&d);
	if (! *d)
		throw ParseError(txt, "srcNode");
	result.srcNode = v;
	v = read_int(&d);
	if (! *d)
		throw ParseError(txt, "srcFifo");
	result.srcFifo = v;
	v = read_int(&d);
	if (! *d)
		throw ParseError(txt, "dstNode");
	result.dstNode = v;
	v = read_int(&d);
	result.dstFifo = v;
	return result;
}


int main(int argc, char** argv)
{
	bool verbose = false;
	static struct option long_options[] = {
	   {"verbose",	no_argument, 0, 'v' },
	   {0,          0,           0, 0 }
	};
	try
	{
		int option_index = 0;
		for (;;)
		{
			int c = getopt_long(argc, argv, "v",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
			case 'v':
				verbose = true;
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		std::vector<dyplo::HardwareContext::Route> routes(argc-optind);
		for (; optind < argc; ++optind)
		{
			if (verbose)
				std::cerr << argv[optind] << ": " << std::flush;
			dyplo::HardwareContext::Route route = parse_route(argv[optind]);
			if (verbose)
				std::cerr << " "
					<< (int)route.srcNode << "." << (int)route.srcFifo
					<< "->"
					<< (int)route.dstNode << "." << (int)route.dstFifo
					<< std::endl;
			routes.push_back(route);
		}
		if (!routes.empty())
		{
			dyplo::HardwareContext ctrl;
			ctrl.routeAdd(&routes[0], routes.size());
		}
	}
	catch (const std::exception& ex)
	{
		std::cerr << "ERROR:\n" << ex.what() << std::endl;
		return 1;
	}
	return 0;
}
