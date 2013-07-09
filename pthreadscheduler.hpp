#pragma once

#include <stdexcept>
#include "mutex.hpp"
#include "condition.hpp"

namespace dyplo
{
	class InterruptedException: public std::exception
	{
		virtual const char* what() const throw();
	};

	class PthreadScheduler
	{
	protected:
		Mutex m_mutex;
		Condition m_condition_not_full;
		Condition m_condition_not_empty;
		bool m_interrupted;
	public:
		PthreadScheduler();

		/* wait_ and trigger_ methods are to be called with the lock held */
		void wait_until_not_full();
		void wait_until_not_empty();
		void trigger_not_full();
		void trigger_not_empty();

		void lock();
		void unlock();

		void interrupt();
	};
}