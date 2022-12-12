#include <assert.h>
#include <pthread.h>

#include "lock.h"

void
toywasm_mutex_init(struct toywasm_mutex *lock)
{
        int ret = pthread_mutex_init(&lock->lock, NULL);
        assert(ret == 0);
}

void
toywasm_mutex_destroy(struct toywasm_mutex *lock)
{
        int ret = pthread_mutex_destroy(&lock->lock);
        assert(ret == 0);
}

void
toywasm_mutex_lock(struct toywasm_mutex *lock) NO_THREAD_SAFETY_ANALYSIS
{
        int ret = pthread_mutex_lock(&lock->lock);
        assert(ret == 0);
}

void
toywasm_mutex_unlock(struct toywasm_mutex *lock) NO_THREAD_SAFETY_ANALYSIS
{
        int ret = pthread_mutex_unlock(&lock->lock);
        assert(ret == 0);
}
