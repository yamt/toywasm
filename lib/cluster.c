#include <assert.h>

#include "cluster.h"

void
cluster_init(struct cluster *c)
{
        toywasm_mutex_init(&c->lock);
        toywasm_cv_init(&c->cv);
        c->nrunners = 0;
        atomic_init(&c->interrupt, 0);

        c->suspend_state = SUSPEND_STATE_NONE;
        c->nparked = 0;
        toywasm_cv_init(&c->stop_cv);
}

void
cluster_destroy(struct cluster *c)
{
        toywasm_cv_destroy(&c->cv);
        toywasm_mutex_destroy(&c->lock);
}

void
cluster_join(struct cluster *c)
{
        toywasm_mutex_lock(&c->lock);
        while (c->nrunners > 0) {
                toywasm_cv_wait(&c->cv, &c->lock);
        }
        toywasm_mutex_unlock(&c->lock);
}

void
cluster_add_thread(struct cluster *c)
{
        /* XXX should park on SUSPEND_STATE_STOPPING? */
        assert(c->nrunners < UINT32_MAX);
        c->nrunners++;
}

void
cluster_remove_thread(struct cluster *c)
{
        assert(c->nrunners > 0);
        c->nrunners--;
        if (c->nrunners == 0) {
                toywasm_cv_signal(&c->cv, &c->lock);
        }
        if (c->suspend_state == SUSPEND_STATE_STOPPING) {
                toywasm_cv_signal(&c->stop_cv, &c->lock);
        }
}
