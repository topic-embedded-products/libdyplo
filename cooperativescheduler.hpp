#pragma once

#include <stdlib.h>
#include <stdexcept>

namespace dyplo
{

	class Process
	{
	public:
		virtual void process_one() = 0;
		virtual void interrupt() = 0;
	};

	class CooperativeScheduler
	{
	public:
		Process* downstream;

		CooperativeScheduler():
			downstream(NULL)
		{
		}

		void process_one()
		{
			unlock();
			downstream->process_one();
			lock();
		}

		void wait_until_not_full()
		{
			process_one();
		}
		void wait_until_not_empty()
		{
			throw std::runtime_error("Not implemented: wait_until_not_empty");
		}
		void trigger_not_full() const
		{
		}
		void trigger_not_empty()
		{
			process_one();
		}

		void lock() const
		{
		}

		void unlock() const
		{
		}

		void interrupt_not_full()
		{
			if (downstream)
				downstream->interrupt();
		}

		void interrupt_not_empty()
		{
		}

		void resume_not_full()
		{
			if (downstream)
				process_one();
		}

		void resume_not_empty()
		{
		}
	};

}
