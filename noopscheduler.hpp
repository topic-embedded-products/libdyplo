#pragma once

namespace dyplo
{
	class NoopScheduler
	{
	public:
		/* wait_ and trigger_ methods are to be called with the lock held */
		void wait_until_not_full();
		void wait_until_not_empty();
		void trigger_not_full()
		{
		}
		void trigger_not_empty()
		{
		}

		/* Having empty lock/unlock methods will cause the whole locking to be optimized away */
		void lock() const
		{
		}
		void unlock() const
		{
		}

		void interrupt_not_empty() const
		{
		}

		void interrupt_not_full() const
		{
		}

		void reset() const
		{
		}
	};

}
