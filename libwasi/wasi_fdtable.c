#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "exec.h"
#include "timeutil.h"
#include "wasi_impl.h"

bool
wasi_fdinfo_is_prestat(const struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_PRESTAT;
}

bool
wasi_fdinfo_unused(struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_UNUSED;
}

const char *
wasi_fdinfo_path(struct wasi_fdinfo *fdinfo)
{
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                return fdinfo->u.u_prestat.prestat_path;
        case WASI_FDINFO_USER:
                return fdinfo->u.u_user.path;
        case WASI_FDINFO_UNUSED:
                return NULL;
        }
        assert(false);
        return NULL;
}

struct wasi_fdinfo *
wasi_fdinfo_alloc(void)
{
        struct wasi_fdinfo *fdinfo = zalloc(sizeof(*fdinfo));
        if (fdinfo == NULL) {
                return NULL;
        }
        fdinfo->type = WASI_FDINFO_UNUSED;
        fdinfo->refcount = 0;
        fdinfo->blocking = 1;
        assert(wasi_fdinfo_unused(fdinfo));
        return fdinfo;
}

void
wasi_fd_affix(struct wasi_instance *wasi, uint32_t wasifd,
              struct wasi_fdinfo *fdinfo) REQUIRES(wasi->lock)
{
        assert(wasifd < wasi->fdtable.lsize);
        struct wasi_fdinfo **slot = &VEC_ELEM(wasi->fdtable, wasifd);
        struct wasi_fdinfo *ofdinfo = *slot;
        assert(ofdinfo == NULL || wasi_fdinfo_unused(ofdinfo));
        assert(fdinfo->refcount < UINT32_MAX);
        fdinfo->refcount++;
        *slot = fdinfo;
        free(ofdinfo);
}

int
wasi_fd_lookup_locked(struct wasi_instance *wasi, uint32_t wasifd,
                      struct wasi_fdinfo **infop) REQUIRES(wasi->lock)
{
        struct wasi_fdinfo *fdinfo;
        if (wasifd >= wasi->fdtable.lsize) {
                return EBADF;
        }
        fdinfo = VEC_ELEM(wasi->fdtable, wasifd);
        if (fdinfo == NULL) {
                fdinfo = wasi_fdinfo_alloc();
                if (fdinfo == NULL) {
                        return ENOMEM;
                }
                wasi_fd_affix(wasi, wasifd, fdinfo);
        }
        assert(fdinfo->refcount < UINT32_MAX); /* XXX */
        fdinfo->refcount++;
        *infop = fdinfo;
        return 0;
}

int
wasi_fd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
               struct wasi_fdinfo **infop)
{
        toywasm_mutex_lock(&wasi->lock);
        int ret = wasi_fd_lookup_locked(wasi, wasifd, infop);
        toywasm_mutex_unlock(&wasi->lock);
        return ret;
}

void
wasi_fdinfo_release(struct wasi_instance *wasi, struct wasi_fdinfo *fdinfo)
{
        if (fdinfo == NULL) {
                return;
        }
        toywasm_mutex_lock(&wasi->lock);
        assert(fdinfo->refcount > 0);
        fdinfo->refcount--;
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        if (fdinfo->refcount == 1) {
                /* wake up drain */
                toywasm_cv_signal(&wasi->cv, &wasi->lock);
        }
#endif
        if (fdinfo->refcount == 0) {
                switch (fdinfo->type) {
                case WASI_FDINFO_PRESTAT:
                        free(fdinfo->u.u_prestat.prestat_path);
                        free(fdinfo->u.u_prestat.wasm_path);
                        break;
                case WASI_FDINFO_USER:
                        assert(fdinfo->u.u_user.hostfd == -1);
                        assert(fdinfo->u.u_user.path == NULL);
                        assert(fdinfo->u.u_user.dir == NULL);
                        break;
                case WASI_FDINFO_UNUSED:
                        break;
                }
                free(fdinfo);
        }
        toywasm_mutex_unlock(&wasi->lock);
}

static int
wasi_fdinfo_wait(struct exec_context *ctx, struct wasi_instance *wasi,
                 struct wasi_fdinfo *fdinfo) REQUIRES(wasi->lock)
{
        int host_ret = 0;
        assert(fdinfo->refcount >= 2);
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct timespec absto;
        const int interval_ms = check_interrupt_interval_ms(ctx);
        int ret = abstime_from_reltime_ms(CLOCK_REALTIME, &absto, interval_ms);
        if (ret != 0) {
                goto fail;
        }
        ret = toywasm_cv_timedwait(&wasi->cv, &wasi->lock, &absto);
        assert(ret == 0 || ret == ETIMEDOUT);
        /*
         * Note: at this point, fdinfo might not be a valid anymore.
         */
#else
        assert(false);
#endif
        toywasm_mutex_unlock(&wasi->lock);
        host_ret = check_interrupt(ctx);
        if (host_ret != 0) {
                xlog_trace("%s: failed with %d", __func__, host_ret);
        }
        toywasm_mutex_lock(&wasi->lock);
#if defined(TOYWASM_ENABLE_WASM_THREADS)
fail:
#endif
        return host_ret;
}

int
wasi_fd_lookup_locked_for_close(struct exec_context *ctx,
                                struct wasi_instance *wasi, uint32_t wasifd,
                                struct wasi_fdinfo **fdinfop, int *retp)
        REQUIRES(wasi->lock)
{
        struct wasi_fdinfo *fdinfo;
        int ret;
retry:
        ret = wasi_fd_lookup_locked(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }

        /*
         * it should have at least two references for
         * fdtable and wasi_fd_lookup_locked above.
         */
        assert(fdinfo->refcount >= 2);
        if (fdinfo->refcount > 2) {
                /*
                 * it's important to drop our own reference before waiting.
                 * otherwise, it can deadlock if two or more threads attempt
                 * to close the same fd.
                 */
                fdinfo->refcount--;
                int host_ret = wasi_fdinfo_wait(ctx, wasi, fdinfo);
                if (host_ret != 0) {
                        return host_ret;
                }
                /*
                 * as wasi_fdinfo_wait might have dropped the lock,
                 * restart from fd lookup.
                 */
                goto retry;
        }
        *fdinfop = fdinfo;
        *retp = 0;
        return 0;
fail:
        *retp = ret;
        return 0;
}

int
wasi_hostfd_lookup(struct wasi_instance *wasi, uint32_t wasifd, int *hostfdp,
                   struct wasi_fdinfo **fdinfop)
{
        struct wasi_fdinfo *info;
        int ret = wasi_fd_lookup(wasi, wasifd, &info);
        if (ret != 0) {
                return ret;
        }
        if (info->type != WASI_FDINFO_USER) {
                wasi_fdinfo_release(wasi, info);
                return EBADF;
        }
        assert(info->u.u_user.hostfd != -1);
        *hostfdp = info->u.u_user.hostfd;
        *fdinfop = info;
        return 0;
}

int
wasi_fdtable_expand(struct wasi_instance *wasi, uint32_t maxfd)
        REQUIRES(wasi->lock)
{
        uint32_t osize = wasi->fdtable.lsize;
        if (maxfd < osize) {
                return 0;
        }
        int ret = VEC_RESIZE(wasi->fdtable, maxfd + 1);
        if (ret != 0) {
                return ret;
        }
        uint32_t i;
        for (i = osize; i <= maxfd; i++) {
                VEC_ELEM(wasi->fdtable, i) = NULL;
        }
        return 0;
}

void
wasi_fdtable_free(struct wasi_instance *wasi) NO_THREAD_SAFETY_ANALYSIS
{
        struct wasi_fdinfo **it;
        uint32_t i;
        VEC_FOREACH_IDX(i, it, wasi->fdtable) {
                struct wasi_fdinfo *fdinfo = *it;
                if (fdinfo == NULL) {
                        continue;
                }
                assert(fdinfo->refcount == 1);
                int ret = wasi_fdinfo_close(fdinfo);
                if (ret != 0) {
                        xlog_trace("failed to close: wasm fd %" PRIu32
                                   " with errno %d",
                                   i, ret);
                }
                free(fdinfo);
        }
        VEC_FREE(wasi->fdtable);
}

int
wasi_fd_alloc(struct wasi_instance *wasi, uint32_t *wasifdp)
        REQUIRES(wasi->lock)
{
        struct wasi_fdinfo **fdinfop;
        uint32_t wasifd;
        VEC_FOREACH_IDX(wasifd, fdinfop, wasi->fdtable) {
                struct wasi_fdinfo *fdinfo = *fdinfop;
                if (fdinfo == NULL || wasi_fdinfo_unused(fdinfo)) {
                        *wasifdp = wasifd;
                        return 0;
                }
        }
        wasifd = wasi->fdtable.lsize;
        int ret = wasi_fdtable_expand(wasi, wasifd);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo *fdinfo = VEC_ELEM(wasi->fdtable, wasifd);
        assert(fdinfo == NULL);
        *wasifdp = wasifd;
        return 0;
}

int
wasi_fd_add(struct wasi_instance *wasi, int hostfd, char *path,
            uint16_t fdflags, uint32_t *wasifdp)
{
        struct wasi_fdinfo *fdinfo;
        uint32_t wasifd;
        int ret;
        assert((fdflags & ~WASI_FDFLAG_NONBLOCK) == 0);
        fdinfo = wasi_fdinfo_alloc();
        if (fdinfo == NULL) {
                free(path);
                return ENOMEM;
        }
        fdinfo->type = WASI_FDINFO_USER;
        fdinfo->u.u_user.hostfd = hostfd;
        fdinfo->u.u_user.dir = NULL;
        fdinfo->u.u_user.path = path;
        fdinfo->blocking = (fdflags & WASI_FDFLAG_NONBLOCK) == 0;
        toywasm_mutex_lock(&wasi->lock);
        ret = wasi_fd_alloc(wasi, &wasifd);
        if (ret != 0) {
                toywasm_mutex_unlock(&wasi->lock);
                free(path);
                free(fdinfo);
                return ret;
        }
        wasi_fd_affix(wasi, wasifd, fdinfo);
        toywasm_mutex_unlock(&wasi->lock);
        *wasifdp = wasifd;
        return 0;
}
