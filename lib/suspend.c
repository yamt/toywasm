#include <assert.h>
#include <inttypes.h>

#include "cluster.h"
#include "context.h"
#include "suspend.h"
#include "timeutil.h"
#include "xlog.h"

#if !defined(TOYWASM_USE_USER_SCHED)
static void
parked(struct cluster *c) REQUIRES(c->lock)
{
        c->nparked++;
        xlog_trace("%s: parked %" PRIu32 " / %" PRIu32, __func__, c->nparked,
                   c->nrunners);
        if (c->nrunners == c->nparked + 1) {
                toywasm_cv_broadcast(&c->stop_cv, &c->lock);
        }
        while (c->suspend_state == SUSPEND_STATE_STOPPING) {
                toywasm_cv_wait(&c->stop_cv, &c->lock);
        }
        xlog_trace("%s: parked %" PRIu32 " / %" PRIu32, __func__, c->nparked,
                   c->nrunners);
        assert(c->nparked > 0);
        c->nparked--;
        assert(c->suspend_state == SUSPEND_STATE_RESUMING);
        if (c->nparked == 0) {
                c->suspend_state = SUSPEND_STATE_NONE;
                toywasm_cv_broadcast(&c->stop_cv, &c->lock);
        }
}
#endif /* !defined(TOYWASM_USE_USER_SCHED) */

int
suspend_check_interrupt(struct cluster *c)
{
#if !defined(TOYWASM_USE_USER_SCHED)
        if (c->suspend_state == SUSPEND_STATE_STOPPING) {
                xlog_trace("%s: restart", __func__);
                return ETOYWASMRESTART;
        }
#endif /* !defined(TOYWASM_USE_USER_SCHED) */
        return 0;
}

void
suspend_parked(struct cluster *c)
{
#if !defined(TOYWASM_USE_USER_SCHED)
        if (c == NULL) {
                return;
        }
        if (c->suspend_state != SUSPEND_STATE_STOPPING) {
                return;
        }
        xlog_trace("%s: parked", __func__);
        toywasm_mutex_lock(&c->lock);
        parked(c);
        toywasm_mutex_unlock(&c->lock);
#endif /* !defined(TOYWASM_USE_USER_SCHED) */
}

void
suspend_threads(struct cluster *c)
{
#if !defined(TOYWASM_USE_USER_SCHED)
        toywasm_mutex_lock(&c->lock);
retry:
        if (c->suspend_state == SUSPEND_STATE_STOPPING) {
                xlog_trace("%s: parking for the previous suspend", __func__);
                parked(c);
                goto retry;
        }
        if (c->suspend_state == SUSPEND_STATE_RESUMING) {
                xlog_trace("%s: waitng for the previous suspend to complete",
                           __func__);
                toywasm_cv_wait(&c->stop_cv, &c->lock);
                goto retry;
        }
        assert(c->nparked == 0);
        struct timespec start;
        struct timespec end;
        timespec_now(CLOCK_REALTIME, &start);
        c->suspend_state = SUSPEND_STATE_STOPPING;
        while (c->nrunners != c->nparked + 1) {
                xlog_trace("%s: waiting %" PRIu32 " / %" PRIu32, __func__,
                           c->nparked, c->nrunners);
                toywasm_cv_wait(&c->stop_cv, &c->lock);
        }
        timespec_now(CLOCK_REALTIME, &end);
        if (timespec_cmp(&end, &start) > 0) {
                struct timespec diff;
                timespec_sub(&end, &start, &diff);
                xlog_trace("%s: suspending %" PRIu32
                           " threads took %ju.%09lu seconds",
                           __func__, c->nrunners - 1, (uintmax_t)diff.tv_sec,
                           diff.tv_nsec);
        }
        toywasm_mutex_unlock(&c->lock);
#endif /* !defined(TOYWASM_USE_USER_SCHED) */
}

void
resume_threads(struct cluster *c)
{
#if !defined(TOYWASM_USE_USER_SCHED)
        xlog_trace("%s: resuming", __func__);
        toywasm_mutex_lock(&c->lock);
        assert(c->suspend_state == SUSPEND_STATE_STOPPING);
        assert(c->nrunners == c->nparked + 1);
        if (c->nparked > 0) {
                c->suspend_state = SUSPEND_STATE_RESUMING;
        } else {
                c->suspend_state = SUSPEND_STATE_NONE;
        }
        toywasm_cv_broadcast(&c->stop_cv, &c->lock);
        toywasm_mutex_unlock(&c->lock);
#endif /* !defined(TOYWASM_USE_USER_SCHED) */
}
