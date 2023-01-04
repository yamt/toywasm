#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "list.h"
#include "waitlist.h"
#include "xlog.h"

struct atomics_mutex {
        pthread_mutex_t lock;
};

static struct atomics_mutex g_atomics_lock = {
        PTHREAD_MUTEX_INITIALIZER,
};

struct waiter {
        LIST_ENTRY(struct waiter) e;
        pthread_cond_t cv;
        bool woken;
};

struct waiter_list {
        struct waiter_list *next;
        LIST_HEAD(struct waiter) waiters;
        uint32_t ident;
        uint32_t nwaiters;
};

struct atomics_mutex *
atomics_mutex_getptr(struct waiter_list_table *tab, uint32_t ident)
{
        /* REVISIT: is it worth to use finer grained lock? */
        return &g_atomics_lock;
}

void
atomics_mutex_lock(struct atomics_mutex *lock)
{
        assert(lock != NULL);
        int ret = pthread_mutex_lock(&lock->lock);
        assert(ret == 0);
}

void
atomics_mutex_unlock(struct atomics_mutex *lock)
{
        assert(lock != NULL);
        int ret = pthread_mutex_unlock(&lock->lock);
        assert(ret == 0);
}

static struct waiter_list *
waiter_list_lookup(struct waiter_list_table *tab, uint32_t ident,
                   struct atomics_mutex **lockp, bool allocate)
{
        struct waiter_list *l;
        struct waiter_list **headp = &tab->lists[0];
        struct atomics_mutex *lock = atomics_mutex_getptr(tab, ident);
        *lockp = lock;
        for (l = *headp; l != NULL; l = l->next) {
                if (l->ident == ident) {
                        return l;
                }
        }
        if (allocate) {
                l = malloc(sizeof(*l));
                if (l != NULL) {
                        LIST_HEAD_INIT(&l->waiters);
                        l->ident = ident;
                        l->nwaiters = 0;
                        l->next = *headp;
                        *headp = l;
                        return l;
                }
        }
        return NULL;
}

static void
waiter_list_free(struct waiter_list_table *tab, struct waiter_list *l1)
{
        struct waiter_list **pp = &tab->lists[0];
        struct waiter_list *l;
        for (l = *pp; l != NULL; l = *pp) {
                if (l == l1) {
                        break;
                }
                assert(l->ident != l1->ident);
                pp = &l->next;
        }
        assert(l != NULL);
        assert(l->nwaiters == 0);
        assert(LIST_EMPTY(&l->waiters));
        *pp = l->next;
        free(l);
}

void
waiter_list_table_init(struct waiter_list_table *tab)
{
        tab->lists[0] = 0;
}

static void
waiter_init(struct waiter *w)
{
        int ret = pthread_cond_init(&w->cv, NULL);
        assert(ret == 0);
        w->woken = false;
}

static void
waiter_destroy(struct waiter *w)
{
        int ret = pthread_cond_destroy(&w->cv);
        assert(ret == 0);
}

static void
waiter_remove(struct waiter_list *l, struct waiter *w)
{
        assert(l->nwaiters > 0);
        LIST_REMOVE(&l->waiters, w, e);
        l->nwaiters--;
}

static void
waiter_insert_tail(struct waiter_list *l, struct waiter *w)
{
        LIST_INSERT_TAIL(&l->waiters, w, e);
        l->nwaiters++;
}

static int
waiter_block(struct waiter_list *l, struct atomics_mutex *lock,
             struct waiter *w, const struct timespec *abstimeout)
{
        int ret;
        while (!w->woken) {
                ret = pthread_cond_timedwait(&w->cv, &lock->lock, abstimeout);
                if (ret == ETIMEDOUT) {
                        if (w->woken) {
                                xlog_trace("%s: ignoring timeout", __func__);
                                ret = 0;
                        }
                        break;
                }
                assert(ret == 0);
        }
        return ret;
}

static void
waiter_wakeup(struct waiter_list *l, struct atomics_mutex *lock,
              struct waiter *w)
{
        w->woken = true;
        int ret = pthread_cond_signal(&w->cv);
        assert(ret == 0);
}

/*
 * modelled after https://tc39.es/ecma262/#sec-atomics.notify
 *
 * returns the number of waiters woken.
 */
uint32_t
atomics_notify(struct waiter_list_table *tab, uint32_t ident, uint32_t count)
{
        xlog_trace("%s: ident=%" PRIx32, __func__, ident);
        struct atomics_mutex *lock;
        struct waiter_list *l = waiter_list_lookup(tab, ident, &lock, false);
        if (l == NULL) {
                return 0;
        }
        assert(!LIST_EMPTY(&l->waiters));
        struct waiter *w;
        uint32_t left = count;
        while (left > 0 && (w = LIST_FIRST(&l->waiters)) != NULL) {
                left--;
                LIST_REMOVE(&l->waiters, w, e);
                waiter_wakeup(l, lock, w);
        }
        assert(left <= count);
        uint32_t nwoken = count - left;
        assert(nwoken <= count);
        assert(nwoken <= l->nwaiters);
        l->nwaiters -= nwoken;
        if (l->nwaiters == 0) {
                waiter_list_free(tab, l);
        }
        return nwoken;
}

/*
 * modelled after https://tc39.es/ecma262/#sec-atomics.wait
 *
 * typical return values are: 0, ETIMEDOUT, and EOVERFLOW.
 */
int
atomics_wait(struct waiter_list_table *tab, uint32_t ident,
             const struct timespec *abstimeout)
{
        xlog_trace("%s: ident=%" PRIx32, __func__, ident);
        int ret;
        struct atomics_mutex *lock;
        struct waiter_list *l = waiter_list_lookup(tab, ident, &lock, true);
        if (l->nwaiters == UINT32_MAX) {
                atomics_mutex_unlock(lock);
                return EOVERFLOW;
        }
        struct waiter w0;
        struct waiter *w = &w0;
        waiter_init(w);
        waiter_insert_tail(l, w);
        ret = waiter_block(l, lock, w, abstimeout);
        if (ret != 0) {
                waiter_remove(l, w);
                if (l->nwaiters == 0) {
                        waiter_list_free(tab, l);
                }
        }
        waiter_destroy(w);
        return ret;
}
