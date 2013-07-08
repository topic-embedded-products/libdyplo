#pragma once

namespace hmu
{
	/* Lock an object when constructing and unlock it on destruction */
	template <class Lockable> class ScopedLock
	{
		Lockable& m_lock;
	public:
		ScopedLock(Lockable& lock):
			m_lock(lock)
		{
			lock.lock();
		}

		~ScopedLock()
		{
			m_lock.unlock();
		}
	};

}

