#include <assert.h>

#include "cluster.h"
#include "exec.h"
#include "suspend.h"

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
        assert(c->nrunners > c->nparked);
        assert(c->suspend_state != SUSPEND_STATE_STOPPING ||
               c->nrunners > c->nparked + 1);
}

void
cluster_remove_thread(struct cluster *c)
{
        assert(c->nrunners > 0);
        assert(c->nrunners > c->nparked);
        c->nrunners--;
        if (c->nrunners == 0) {
                toywasm_cv_signal(&c->cv, &c->lock);
        }
        if (c->suspend_state == SUSPEND_STATE_STOPPING) {
                toywasm_cv_broadcast(&c->stop_cv, &c->lock);
        }
}

int
cluster_check_interrupt(struct exec_context *ctx, const struct cluster *c)
{
        if (c->interrupt) {
                STAT_INC(ctx, interrupt_exit);
                return trap_with_id(ctx, TRAP_VOLUNTARY_THREAD_EXIT,
                                    "interrupt");
        }
        return suspend_check_interrupt(ctx, c);
}

bool
cluster_set_interrupt(struct cluster *c)
{
        if (c->interrupt) {
                return false;
        }
        c->interrupt = 1;
        return true;
}
