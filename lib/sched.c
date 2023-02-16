/*
 * a simple userland thread implementation.
 *
 * this is slow because of a few reasons:
 *
 * - sleeping threads do not yield the scheduler.
 * - reschedule requests are based on periodic polling. (check_interrupt)
 * - no real i/o wait.
 */

#include <assert.h>

#include "context.h"
#include "instance.h"
#include "sched.h"
#include "timeutil.h"
#include "xlog.h"

void
sched_enqueue(struct sched *sched, struct exec_context *ctx)
{
        xlog_trace("%s: enqueueing ctx %p", __func__, ctx);
        assert(sched == ctx->sched);
        LIST_INSERT_TAIL(&sched->runq, ctx, rq);
}

#define RR_INTERVAL_MS (CHECK_INTERRUPT_INTERVAL_MS * 2)

void
sched_run(struct sched *sched, struct exec_context *caller)
{
        struct runq *q = &sched->runq;
        struct exec_context *ctx;

        while ((ctx = LIST_FIRST(q)) != NULL) {
                int ret;
                LIST_REMOVE(q, ctx, rq);
                xlog_trace("%s: running ctx %p", __func__, ctx);
                ret = abstime_from_reltime_ms(
                        CLOCK_MONOTONIC, &sched->next_resched, RR_INTERVAL_MS);
                if (ret != 0) {
                        /* XXX what to do? */
                        xlog_error(
                                "%s: abstime_from_reltime_ms failed with %d",
                                __func__, ret);
                }
                /*
                 * Note: when spawning a thread, this
                 * instance_execute_continue() ends up with
                 * calling sched_enqueue.
                 */
                ret = instance_execute_continue(ctx);
                if (ret == ETOYWASMRESTART) {
                        xlog_trace("%s: re-enqueueing ctx %p", __func__, ctx);
                        LIST_INSERT_TAIL(q, ctx, rq);
                        continue;
                }
                xlog_trace("%s: finishing ctx %p", __func__, ctx);
                ctx->exec_ret = ret;
                if (ctx == caller) {
                        break;
                }
                ctx->exec_done(ctx);
        }
}

void
sched_init(struct sched *sched)
{
        LIST_HEAD_INIT(&sched->runq);
}

void
sched_clear(struct sched *sched)
{
}

bool
sched_need_resched(struct sched *sched)
{
        struct timespec now;
        int ret;

        ret = timespec_now(CLOCK_MONOTONIC, &now);
        if (ret != 0) {
                /* XXX what to do? */
                xlog_error("%s: timespec_now failed with %d", __func__, ret);
                return true;
        }
        if (timespec_cmp(&sched->next_resched, &now) <= 0) {
                return true;
        }
        return false;
}
