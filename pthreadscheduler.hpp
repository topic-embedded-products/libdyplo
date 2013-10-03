#pragma once

#include "mutex.hpp"
#include "condition.hpp"
#include "exceptions.hpp"

namespace dyplo
{
	class PthreadScheduler
	{
	protected:
		Mutex m_mutex;
		Condition m_condition_not_full;
		Condition m_condition_not_empty;
		bool m_interrupted_not_full;
		bool m_interrupted_not_empty;
	public:
		PthreadScheduler();

		/* wait_ and trigger_ methods are to be called with the lock held */
		void wait_until_not_full();
		void wait_until_not_empty();
		void trigger_not_full();
		void trigger_not_empty();

		void lock();
		void unlock();

		/* interrupt and reset are to be called with the lock held */
		void interrupt_not_full();
		void interrupt_not_empty();
		void reset();
	};
}
