#pragma once

#include <stdlib.h>
#include <stdexcept>

namespace dyplo
{

	class Process
	{
	public:
		virtual void process_one() = 0;
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
		void trigger_not_full()
		{
		}
		void trigger_not_empty()
		{
			process_one();
		}

		void lock()
		{
		}

		void unlock()
		{
		}

		void interrupt()
		{
		}
	};

}