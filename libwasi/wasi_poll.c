#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "exec.h"
#include "restart.h"
#include "wasi_impl.h"
#include "wasi_poll_subr.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

int
wasi_poll_oneoff(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t in = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t out = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t nsubscriptions = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct pollfd *pollfds = NULL;
        const struct wasi_subscription *subscriptions;
        struct wasi_fdinfo **fdinfos = NULL;
        uint32_t nfdinfos = 0;
        struct wasi_event *events;
        int host_ret = 0;
        int ret;
        if (nsubscriptions == 0) {
                /*
                 * https://github.com/WebAssembly/WASI/pull/193
                 */
                xlog_trace("poll_oneoff: no subscriptions");
                ret = EINVAL;
                goto fail;
        }
        void *p;
        size_t insize = nsubscriptions * sizeof(struct wasi_subscription);
retry:
        host_ret = host_func_check_align(ctx, in, WASI_SUBSCRIPTION_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        host_ret = memory_getptr(ctx, 0, in, 0, insize, &p);
        if (host_ret != 0) {
                goto fail;
        }
        subscriptions = p;
        bool moved = false;
        size_t outsize = nsubscriptions * sizeof(struct wasi_event);
        host_ret = host_func_check_align(ctx, in, WASI_EVENT_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        host_ret = memory_getptr2(ctx, 0, out, 0, outsize, &p, &moved);
        if (host_ret != 0) {
                goto fail;
        }
        if (moved) {
                goto retry;
        }
        events = p;
        pollfds = calloc(nsubscriptions, sizeof(*pollfds));
        fdinfos = calloc(nsubscriptions, sizeof(*fdinfos));
        if (pollfds == NULL || fdinfos == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        uint32_t i;
        int timeout_ms = -1;
        uint64_t timeout_ns;
        int nevents = 0;
        for (i = 0; i < nsubscriptions; i++) {
                const struct wasi_subscription *s = &subscriptions[i];
                struct pollfd *pfd = &pollfds[i];
                clockid_t host_clock_id;
                bool abstime;
                switch (s->type) {
                case WASI_EVENTTYPE_CLOCK:
                        switch (le32_to_host(s->u.clock.clock_id)) {
                        case WASI_CLOCK_ID_REALTIME:
                                host_clock_id = CLOCK_REALTIME;
                                break;
                        case WASI_CLOCK_ID_MONOTONIC:
                                host_clock_id = CLOCK_MONOTONIC;
                                break;
                        default:
                                xlog_trace("poll_oneoff: unsupported clock id "
                                           "%" PRIu32,
                                           le32_to_host(s->u.clock.clock_id));
                                ret = ENOTSUP;
                                goto fail;
                        }
                        abstime =
                                (s->u.clock.flags &
                                 host_to_le16(WASI_SUBCLOCKFLAG_ABSTIME)) != 0;
                        if (timeout_ms != -1) {
                                xlog_trace("poll_oneoff: multiple clock "
                                           "subscriptions");
                                ret = ENOTSUP;
                                goto fail;
                        }
                        timeout_ns = le64_to_host(s->u.clock.timeout);
                        struct timespec absts;
                        if (abstime) {
                                ret = timespec_from_ns(&absts, timeout_ns);
                        } else {
                                ret = abstime_from_reltime_ns(
                                        host_clock_id, &absts, timeout_ns);
                        }
                        if (ret != 0) {
                                goto fail;
                        }
                        if (host_clock_id != CLOCK_REALTIME) {
                                ret = convert_timespec(host_clock_id,
                                                       CLOCK_REALTIME, &absts,
                                                       &absts);
                                if (ret != 0) {
                                        goto fail;
                                }
                        }
                        ret = abstime_to_reltime_ms_roundup(
                                CLOCK_REALTIME, &absts, &timeout_ms);
                        if (ret != 0) {
                                goto fail;
                        }
                        pfd->fd = -1;
                        xlog_trace("poll_oneoff: pfd[%" PRIu32 "] timer %d ms",
                                   i, timeout_ms);
                        break;
                case WASI_EVENTTYPE_FD_READ:
                case WASI_EVENTTYPE_FD_WRITE:
                        assert(s->u.fd_read.fd == s->u.fd_write.fd);
                        ret = wasi_hostfd_lookup(wasi,
                                                 le32_to_host(s->u.fd_read.fd),
                                                 &pfd->fd, &fdinfos[nfdinfos]);
                        if (ret != 0) {
                                pfd->revents = POLLNVAL;
                                nevents++;
                        }
                        nfdinfos++;
                        if (s->type == WASI_EVENTTYPE_FD_READ) {
                                pfd->events = POLLIN;
                        } else {
                                pfd->events = POLLOUT;
                        }
                        xlog_trace("poll_oneoff: pfd[%" PRIu32 "] hostfd %d",
                                   i, pfd->fd);
                        break;
                default:
                        xlog_trace("poll_oneoff: pfd[%" PRIu32
                                   "] invalid type %u",
                                   i, s->type);
                        ret = EINVAL;
                        goto fail;
                }
        }
        if (nevents == 0) {
                xlog_trace("poll_oneoff: start polling");
                host_ret = wasi_poll(ctx, pollfds, nsubscriptions, timeout_ms,
                                     &ret, &nevents);
                if (host_ret != 0) {
                        goto fail;
                }
                if (ret == ETIMEDOUT) {
                        /* timeout is an event */
                        nevents = 1;
                } else if (ret != 0) {
                        xlog_trace("poll_oneoff: wasi_poll failed with %d",
                                   ret);
                        goto fail;
                }
        }
        struct wasi_event *ev = events;
        for (i = 0; i < nsubscriptions; i++) {
                const struct wasi_subscription *s = &subscriptions[i];
                const struct pollfd *pfd = &pollfds[i];
                ev->userdata = s->userdata;
                ev->error = 0;
                ev->type = s->type;
                ev->availbytes = 0; /* TODO should use FIONREAD? */
                ev->rwflags = 0;
                switch (s->type) {
                case WASI_EVENTTYPE_CLOCK:
                        if (ret == ETIMEDOUT) {
                                ev++;
                        }
                        break;
                case WASI_EVENTTYPE_FD_READ:
                case WASI_EVENTTYPE_FD_WRITE:
                        if (pfd->revents != 0) {
                                /*
                                 * translate per-fd error.
                                 *
                                 * Note: the mapping to EBADF and EPIPE here
                                 * matches wasi-libc.
                                 */
                                if ((pfd->revents & POLLNVAL) != 0) {
                                        ev->error = wasi_convert_errno(EBADF);
                                } else if ((pfd->revents & POLLHUP) != 0) {
                                        ev->error = wasi_convert_errno(EPIPE);
                                } else if ((pfd->revents & POLLERR) != 0) {
                                        ev->error = wasi_convert_errno(EIO);
                                }
                                ev++;
                        }
                        break;
                default:
                        assert(false);
                }
        }
        assert(events + nevents == ev);
        uint32_t result = host_to_le32(nevents);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U32_ALIGN);
        ret = 0;
fail:
        for (i = 0; i < nfdinfos; i++) {
                wasi_fdinfo_release(wasi, fdinfos[i]);
        }
        free(fdinfos);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(pollfds);
        if (!IS_RESTARTABLE(host_ret)) {
                /*
                 * avoid leaving a stale restart state.
                 *
                 * consider:
                 * 1. poll_oneoff returns a restartable error with
                 *    restart_abstimeout saved.
                 * 2. exec_expr_continue restarts the poll_oneoff.
                 * 3. however, for some reasons, poll_oneoff doesn't
                 *    consume the saved timeout. it's entirely possible
                 *    especially when the app is multi-threaded.
                 * 4. the subsequent restartable operation gets confused
                 *    by the saved timeout.
                 */
                restart_info_clear(ctx);
        } else {
                xlog_trace("%s: restarting", __func__);
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}
