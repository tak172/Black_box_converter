#pragma once

#include <boost/thread/recursive_mutex.hpp>
#ifdef LINUX
#include <pthread.h>
#endif // LINUX


#ifdef LINUX
class LockBy_CriticalSection
{
	pthread_mutex_t *pcs_mutex;
public:
	LockBy_CriticalSection(pthread_mutex_t* pCS)
	{
		pcs_mutex = pCS;
		pthread_mutex_lock(pcs_mutex);
	}
	LockBy_CriticalSection(const pthread_mutex_t* pCS)
	{
		pcs_mutex = const_cast<pthread_mutex_t*>(pCS);
		pthread_mutex_lock(pcs_mutex);
	}
	~LockBy_CriticalSection()
	{
		pthread_mutex_unlock(pcs_mutex);
	}
	static void LockBy_Init(pthread_mutex_t* pCS)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init( pCS, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	static void LockBy_Del(pthread_mutex_t* pCS) { pthread_mutex_destroy(pCS); }
};
#else
class LockBy_CriticalSection
{
	CRITICAL_SECTION* ptrCS;
public:
	LockBy_CriticalSection(CRITICAL_SECTION* pCS)
	{
		ptrCS = pCS;
		EnterCriticalSection(ptrCS);
	}
	LockBy_CriticalSection(const CRITICAL_SECTION* pCS)
	{
		ptrCS = const_cast<CRITICAL_SECTION*>(pCS);
		EnterCriticalSection(ptrCS);
	}
	~LockBy_CriticalSection()
	{
		LeaveCriticalSection(ptrCS);
	}
	static void LockBy_Init(CRITICAL_SECTION* pCS) { InitializeCriticalSection(pCS); }
	static void LockBy_Del(CRITICAL_SECTION* pCS) { DeleteCriticalSection(pCS); }
};
#endif // !LINUX


typedef boost::shared_lock <boost::shared_mutex> SharableLock;
typedef boost::unique_lock <boost::shared_mutex> UniqueLock;
typedef boost::unique_lock <boost::recursive_mutex> RecursiveLock;
