#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "exec.h"
#include "nbio.h"
#include "restart.h"
#include "wasi_impl.h"
#include "wasi_poll_subr.h"

int
wasi_poll(struct exec_context *ctx, struct pollfd *fds, nfds_t nfds,
          int timeout_ms, int *retp, int *neventsp)
{
        const int interval_ms = check_interrupt_interval_ms(ctx);
        const struct timespec *abstimeout;
        int host_ret = 0;
        int ret;

        /*
         * Note about timeout
         *
         * In POSIX poll, timeout is the minimum interval:
         *
         * > poll() shall wait at least timeout milliseconds for an event
         * > to occur on any of the selected file descriptors.
         *
         * > If the requested timeout interval requires a finer granularity
         * > than the implementation supports, the actual timeout interval
         * > shall be rounded up to the next supported value.
         *
         * Linux agrees with it:
         * https://www.man7.org/linux/man-pages/man2/poll.2.html
         *
         * > Note that the timeout interval will be rounded up to the
         * > system clock granularity, and kernel scheduling delays mean
         * > that the blocking interval may overrun by a small amount.
         *
         * While the poll(2) of macOS and other BSDs have the text like
         * the below, which can be (mis)interpreted as being the opposite of
         * the POSIX behavior, I guess it isn't the intention of the text.
         * At least the implementation in NetBSD rounds it up to the
         * system granularity. (HZ/tick)
         *
         * > If timeout is greater than zero, it specifies a maximum
         * > interval (in milliseconds) to wait for any file descriptor to
         * > become ready.
         *
         * While I couldn't find any authoritative text about this in
         * the WASI spec, I assume it follows the POSIX semantics.
         * Thus, round up when converting the timespec to ms.
         * (abstime_to_reltime_ms_roundup)
         * It also matches the assumption of wasmtime's poll_oneoff_files
         * test case.
         * https://github.com/bytecodealliance/wasmtime/blob/93ae9078c5a2588b5241bd7221ace459d2b04d54/crates/test-programs/wasi-tests/src/bin/poll_oneoff_files.rs#L86-L89
         */

        ret = restart_info_prealloc(ctx);
        if (ret != 0) {
                return ret;
        }
        struct restart_info *restart = &VEC_NEXTELEM(ctx->restarts);
        assert(restart->restart_type == RESTART_NONE ||
               restart->restart_type == RESTART_TIMER);
        if (restart->restart_type == RESTART_TIMER) {
                abstimeout = &restart->restart_u.timer.abstimeout;
                restart->restart_type = RESTART_NONE;
        } else if (timeout_ms < 0) {
                abstimeout = NULL;
        } else {
                ret = abstime_from_reltime_ms(
                        CLOCK_REALTIME, &restart->restart_u.timer.abstimeout,
                        timeout_ms);
                if (ret != 0) {
                        goto fail;
                }
                abstimeout = &restart->restart_u.timer.abstimeout;
        }
        assert(restart->restart_type == RESTART_NONE);
        while (true) {
                int next_timeout_ms;

                host_ret = check_interrupt(ctx);
                if (host_ret != 0) {
                        if (IS_RESTARTABLE(host_ret)) {
                                if (abstimeout != NULL) {
                                        assert(abstimeout ==
                                               &restart->restart_u.timer
                                                        .abstimeout);
                                        restart->restart_type = RESTART_TIMER;
                                }
                        }
                        goto fail;
                }
                if (abstimeout == NULL) {
                        next_timeout_ms = interval_ms;
                } else {
                        struct timespec next;
                        ret = abstime_from_reltime_ms(CLOCK_REALTIME, &next,
                                                      interval_ms);
                        if (ret != 0) {
                                goto fail;
                        }
                        if (timespec_cmp(abstimeout, &next) > 0) {
                                next_timeout_ms = interval_ms;
                        } else {
                                ret = abstime_to_reltime_ms_roundup(
                                        CLOCK_REALTIME, abstimeout,
                                        &next_timeout_ms);
                                if (ret != 0) {
                                        goto fail;
                                }
                        }
                }
                ret = poll(fds, nfds, next_timeout_ms);
                if (ret < 0) {
                        ret = errno;
                        assert(ret > 0);
                        goto fail;
                }
                if (ret > 0) {
                        *neventsp = ret;
                        ret = 0;
                        break;
                }
                if (ret == 0 && next_timeout_ms != interval_ms) {
                        ret = ETIMEDOUT;
                        break;
                }
        }
fail:
        assert(host_ret != 0 || ret >= 0);
        assert(host_ret != 0 || ret != 0 || *neventsp > 0);
        assert(IS_RESTARTABLE(host_ret) ||
               restart->restart_type == RESTART_NONE);
        if (host_ret == 0) {
                *retp = ret;
        }
        return host_ret;
}

int
wait_fd_ready(struct exec_context *ctx, int hostfd, short event, int *retp)
{
        struct pollfd pfd;
        pfd.fd = hostfd;
        pfd.events = event;
        int nev;
        int ret = wasi_poll(ctx, &pfd, 1, -1, retp, &nev);
        if (IS_RESTARTABLE(ret)) {
                xlog_trace("%s: restarting", __func__);
        }
        return ret;
}

bool
emulate_blocking(struct exec_context *ctx, struct wasi_fdinfo *fdinfo,
                 short poll_event, int orig_ret, int *host_retp, int *retp)
{
        assert(fdinfo->type == WASI_FDINFO_USER);
        int hostfd = fdinfo->u.u_user.hostfd;
        assert(hostfd != -1);
        /* See the comment in wasi_instance_create */
        assert(isatty(hostfd) ||
               (fcntl(hostfd, F_GETFL, 0) & O_NONBLOCK) != 0);

        if (!fdinfo->blocking || !is_again(orig_ret)) {
                *host_retp = 0;
                *retp = orig_ret;
                return false;
        }

        int host_ret;
        int ret;

        host_ret = wait_fd_ready(ctx, hostfd, poll_event, &ret);
        if (host_ret != 0) {
                ret = 0;
                goto fail;
        }
        if (ret != 0) {
                goto fail;
        }
        return true;
fail:
        *host_retp = host_ret;
        *retp = ret;
        return false;
}
