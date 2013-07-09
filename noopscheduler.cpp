#pragma once

#include "noopscheduler.hpp"
#include <stdexcept>

namespace dyplo
{
	void NoopScheduler::wait_until_not_full()
	{
		throw std::runtime_error("Not implemented: wait_until_not_full");
	}
	void NoopScheduler::wait_until_not_empty()
	{
		throw std::runtime_error("Not implemented: wait_until_not_empty");
	}
}
