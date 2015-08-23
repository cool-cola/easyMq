#include "LockGuard.hpp"
//---------------------------------------------------
RLockGuard::RLockGuard(pthread_rwlock_t* pRWLock)
{
	pthread_rwlock_rdlock(pRWLock);
	m_pRWLock = pRWLock;		
}

RLockGuard::~RLockGuard()
{
	pthread_rwlock_unlock(m_pRWLock);
}
//---------------------------------------------------
WLockGuard::WLockGuard(pthread_rwlock_t* pRWLock)
{
	pthread_rwlock_wrlock(pRWLock);
	m_pRWLock = pRWLock;		
}
WLockGuard::~WLockGuard()
{
	pthread_rwlock_unlock(m_pRWLock);
}
//---------------------------------------------------
MutexGuard::MutexGuard(pthread_mutex_t* pMutex)
{
	pthread_mutex_lock(pMutex);
	m_pMutex = pMutex;		
}

MutexGuard::~MutexGuard()
{
	pthread_mutex_unlock(m_pMutex);
}

