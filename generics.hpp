#pragma once

#ifdef _DEBUG
#	define DEBUG_ASSERT(x, e) if (!(x)) throw std::runtime_error(e)
#else
#	define DEBUG_ASSERT(x, e) while (false) {}
#endif

