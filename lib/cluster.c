#include <assert.h>

#include "cluster.h"

void
cluster_init(struct cluster *c)
{
        toywasm_mutex_init(&c->lock);
        int ret = pthread_cond_init(&c->cv, NULL);
        assert(ret == 0);
        c->nrunners = 0;
}

void
cluster_destroy(struct cluster *c)
{
        int ret = pthread_cond_destroy(&c->cv);
        assert(ret == 0);
        toywasm_mutex_destroy(&c->lock);
}

void
cluster_join(struct cluster *c)
{
        toywasm_mutex_lock(&c->lock);
        while (c->nrunners > 0) {
                int ret = pthread_cond_wait(&c->cv, &c->lock.lock);
                assert(ret == 0);
        }
        toywasm_mutex_unlock(&c->lock);
}

void
cluster_add_thread(struct cluster *c)
{
        assert(c->nrunners < UINT32_MAX);
        c->nrunners++;
}

void
cluster_remove_thread(struct cluster *c)
{
        assert(c->nrunners > 0);
        c->nrunners--;
        if (c->nrunners == 0) {
                int ret = pthread_cond_signal(&c->cv);
                assert(ret == 0);
        }
}
