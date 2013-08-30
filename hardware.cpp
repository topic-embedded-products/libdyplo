#include "hardware.hpp"
#include <sys/ioctl.h>

#define DYPLO_DRIVER_PREFIX "/dev/dyplo"

/* TODO: Put in header */
extern "C"
{
/* ioctl values for dyploctl device, set and get routing tables */
struct dyplo_route_item_t {
	unsigned char dstFifo; /* LSB */
	unsigned char dstNode;
	unsigned char srcFifo;
	unsigned char srcNode; /* MSB */
};

struct dyplo_route_t  {
	unsigned int n_routes;
	struct dyplo_route_item_t* proutes;
};
#define DYPLO_IOC_MAGIC	'd'
#define DYPLO_IOC_ROUTE_CLEAR	0x00
#define DYPLO_IOC_ROUTE_SET	0x01
#define DYPLO_IOC_ROUTE_GET	0x02
#define DYPLO_IOC_ROUTE_TELL	0x03
/* S means "Set" through a ptr,
 * T means "Tell", sets directly
 * G means "Get" through a ptr
 * Q means "Query", return value */
#define DYPLO_IOCROUTE_CLEAR	_IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_CLEAR)
#define DYPLO_IOCSROUTE   _IOW(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_SET, struct dyplo_route_t)
#define DYPLO_IOCGROUTE   _IOR(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_GET, struct dyplo_route_t)
#define DYPLO_IOCTROUTE   _IO(DYPLO_IOC_MAGIC, DYPLO_IOC_ROUTE_TELL)
}

namespace dyplo
{
	static const char DRIVER_CONTROL[] = DYPLO_DRIVER_PREFIX "ctl";
	
	HardwareContext::HardwareContext():
		control_device(DRIVER_CONTROL, O_RDWR)
	{}
	
	void HardwareContext::routeDeleteAll()
	{
		if (::ioctl(control_device, DYPLO_IOCROUTE_CLEAR) != 0)
			throw IOException();
	}

	void HardwareContext::routeAddSingle(char srcNode, char srcFifo, char dstNode, char dstFifo)
	{
		struct dyplo_route_item_t route =
			{dstFifo, dstNode, srcFifo, srcNode};
		if (::ioctl(control_device, DYPLO_IOCTROUTE, route) != 0)
			throw IOException();
	}

	int HardwareContext::routeGetAll(Route* items, int n_items)
	{
		struct dyplo_route_t routes;
		routes.n_routes = n_items;
		routes.proutes = (struct dyplo_route_item_t*)items;
		return ::ioctl(control_device, DYPLO_IOCGROUTE, &routes);
	}

	void HardwareContext::routeAdd(const Route* items, int n_items)
	{
		struct dyplo_route_t routes;
		routes.n_routes = n_items;
		routes.proutes = (struct dyplo_route_item_t*)items;
		if (::ioctl(control_device, DYPLO_IOCSROUTE, &routes) != 0)
			throw IOException();
	}
}
