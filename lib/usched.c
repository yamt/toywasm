/*
 * a simple userland thread implementation.
 *
 * the purpose of this scheduler is to make it possible to experiment
 * our wasi-threads implementation on platforms w/o pthread. namely,
 * wasi itself.
 *
 * also, it might or might not be useful for small use cases, where
 * real host thread is too expensive. in that case, it's probably better
 * to implement a bit more serious scheduler though.
 *
 * this is not efficient because of a few reasons:
 *
 * - sleeping threads do not yield the scheduler. there is no concept
 *   like runnable threads. all threads are equally scheduled in a
 *   round-robin manner.
 *
 * - reschedule requests are based on periodic polling. (check_interrupt)
 *   because of that, long sleep request is divided into small intervals
 *   so that the thread have a chances to check reschedule requests
 *   frequently enough.
 *
 * - no real i/o wait. when a thread want to block on an i/o event, it just
 *   yields the cpu to other threads. when the thread is scheduled next time,
 *   it simply polls the event again. an ideal scheduler implementation
 *   would have a list of event sources to poll on instead.
 */

#include <assert.h>

#include "context.h"
#include "instance.h"
#include "timeutil.h"
#include "usched.h"
#include "xlog.h"

void
sched_enqueue(struct sched *sched, struct exec_context *ctx)
{
        xlog_trace("%s: enqueueing ctx %p", __func__, (void *)ctx);
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
                xlog_trace("%s: running ctx %p", __func__, (void *)ctx);
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
                if (IS_RESTARTABLE(ret)) {
                        xlog_trace("%s: re-enqueueing ctx %p", __func__,
                                   (void *)ctx);
                        LIST_INSERT_TAIL(q, ctx, rq);
                        continue;
                }
                xlog_trace("%s: finishing ctx %p", __func__, (void *)ctx);
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

        /* if we are the only thread, no point to resched. */
        if (LIST_FIRST(&sched->runq) == NULL) {
                return false;
        }

        /*
         * REVISIT:
         * On a very slow environment, this can be always true.
         * We should somehow ensure threads making a progress.
         */

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
