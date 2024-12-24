#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <pthread.h>

/**
 * @file   Mutex.h
 * @brief  RAII style pthread lock
 */

// RAII: YES
class Lock
{
public:
	Lock(pthread_mutex_t& mutex): mutex_(&mutex) { pthread_mutex_lock(mutex_); }
	Lock(pthread_mutex_t *mutex): mutex_(mutex) { pthread_mutex_lock(mutex_); }
	~Lock() { pthread_mutex_unlock(mutex_); }

private:
	pthread_mutex_t *mutex_;
};

// RAII: YES
class ReadLock
{
public:
	ReadLock(pthread_rwlock_t& rwlock): rwlock_(&rwlock) { pthread_rwlock_rdlock(rwlock_); }
	ReadLock(pthread_rwlock_t *rwlock): rwlock_(rwlock) { pthread_rwlock_rdlock(rwlock_); }
	~ReadLock() { pthread_rwlock_unlock(rwlock_); }

private:
	pthread_rwlock_t *rwlock_;
};

// RAII: YES
class WriteLock
{
public:
	WriteLock(pthread_rwlock_t& rwlock): rwlock_(&rwlock) { pthread_rwlock_wrlock(rwlock_); }
	WriteLock(pthread_rwlock_t *rwlock): rwlock_(rwlock) { pthread_rwlock_wrlock(rwlock_); }
	~WriteLock() { pthread_rwlock_unlock(rwlock_); }

private:
	pthread_rwlock_t *rwlock_;
};

#endif