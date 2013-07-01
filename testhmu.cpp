#define YAFFUT_MAIN
#include "yaffut.h"

#include "hmu.h"

FUNC(pass)
{
	int result = hmu::init();
	EQUAL(0, result);
}
