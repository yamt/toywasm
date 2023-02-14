#include <assert.h>
#include <errno.h>
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

void
toywasm_cv_init(pthread_cond_t *cv)
{
        int ret;
        ret = pthread_cond_init(cv, NULL);
        assert(ret == 0);
}

void
toywasm_cv_destroy(pthread_cond_t *cv)
{
        int ret;
        ret = pthread_cond_destroy(cv);
        assert(ret == 0);
}

void
toywasm_cv_wait(pthread_cond_t *cv, struct toywasm_mutex *lock)
{
        int ret;
        ret = pthread_cond_wait(cv, &lock->lock);
        assert(ret == 0);
}

int
toywasm_cv_timedwait(pthread_cond_t *cv, struct toywasm_mutex *lock,
                     const struct timespec *abs)
{
        int ret;
        ret = pthread_cond_timedwait(cv, &lock->lock, abs);
        assert(ret == 0 || ret == ETIMEDOUT);
        return ret;
}

void
toywasm_cv_signal(pthread_cond_t *cv, struct toywasm_mutex *lock)
{
        int ret;
        ret = pthread_cond_signal(cv);
        assert(ret == 0);
}

void
toywasm_cv_broadcast(pthread_cond_t *cv, struct toywasm_mutex *lock)
{
        int ret;
        ret = pthread_cond_broadcast(cv);
        assert(ret == 0);
}
