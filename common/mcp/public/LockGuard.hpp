/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 封转的自我析构的互斥锁，读写锁

***********************************************************/

#ifndef _LOCKGUARD_HPP
#define _LOCKGUARD_HPP
#include <pthread.h>

#define INIT_RWLOCK(stRWLock) { \
	pthread_rwlockattr_t  attr;\
	pthread_rwlockattr_init(&attr);\
	pthread_rwlockattr_setpshared(&attr,PTHREAD_PROCESS_PRIVATE);\
	pthread_rwlock_init(&stRWLock,&attr);  \
	pthread_rwlockattr_destroy(&attr);\
}

#define INIT_MUTEXLOCK(stMutexLock) { \
	pthread_mutex_init(&stMutexLock,NULL);\
}

// CC -g -lpthread  -lmtmalloc   *.cpp  
class RLockGuard
{
public:
	RLockGuard(pthread_rwlock_t* pRWLock);
	~RLockGuard();
private:
	pthread_rwlock_t* m_pRWLock;
};

class WLockGuard
{
public:
	WLockGuard(pthread_rwlock_t* pRWLock);
	~WLockGuard();
private:
	pthread_rwlock_t* m_pRWLock;
};

//--------------------------------------------
class MutexGuard
{
public:
	MutexGuard(pthread_mutex_t* pMutex);
	~MutexGuard();
private:
	pthread_mutex_t* m_pMutex;
};

#endif

