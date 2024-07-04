#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "exec.h"
#include "timeutil.h"
#include "wasi_impl.h"
#include "xlog.h"

void
wasi_table_affix(struct wasi_instance *wasi, enum wasi_table_idx idx,
                 uint32_t wasifd, struct wasi_fdinfo *fdinfo)
        REQUIRES(wasi->lock)
{
        struct wasi_fdinfo **slot = wasi_table_slot_ptr(wasi, idx, wasifd);
        struct wasi_fdinfo *ofdinfo = *slot;
        assert(ofdinfo == NULL);
        assert(fdinfo->refcount < UINT32_MAX);
        fdinfo->refcount++;
        *slot = fdinfo;
}

int
wasi_table_lookup_locked(struct wasi_instance *wasi, enum wasi_table_idx idx,
                         uint32_t wasifd, struct wasi_fdinfo **infop)
        REQUIRES(wasi->lock)
{
        struct wasi_table *table = &wasi->fdtable[idx];
        struct wasi_fdinfo *fdinfo;
        if (wasifd >= table->table.lsize) {
                return EBADF;
        }
        fdinfo = VEC_ELEM(table->table, wasifd);
        if (fdinfo == NULL) {
                return EBADF;
        }
        assert(fdinfo->refcount < UINT32_MAX); /* XXX */
        fdinfo->refcount++;
        *infop = fdinfo;
        return 0;
}

int
wasi_table_lookup(struct wasi_instance *wasi, enum wasi_table_idx idx,
                  uint32_t wasifd, struct wasi_fdinfo **infop)
{
        toywasm_mutex_lock(&wasi->lock);
        int ret = wasi_table_lookup_locked(wasi, idx, wasifd, infop);
        toywasm_mutex_unlock(&wasi->lock);
        return ret;
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
wasi_table_lookup_locked_for_close(struct exec_context *ctx,
                                   struct wasi_instance *wasi,
                                   enum wasi_table_idx idx, uint32_t wasifd,
                                   struct wasi_fdinfo **fdinfop, int *retp)
        REQUIRES(wasi->lock)
{
        struct wasi_fdinfo *fdinfo;
        int ret;
retry:
        ret = wasi_table_lookup_locked(wasi, idx, wasifd, &fdinfo);
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
wasi_table_expand(struct wasi_instance *wasi, enum wasi_table_idx idx,
                  uint32_t maxfd) REQUIRES(wasi->lock)
{
        struct wasi_table *table = &wasi->fdtable[idx];
        uint32_t osize = table->table.lsize;
        if (maxfd < osize) {
                return 0;
        }
        int ret = VEC_RESIZE(wasi->mctx, table->table, maxfd + 1);
        if (ret != 0) {
                return ret;
        }
        uint32_t i;
        for (i = osize; i <= maxfd; i++) {
                VEC_ELEM(table->table, i) = NULL;
        }
        return 0;
}

void
wasi_table_clear(struct wasi_instance *wasi,
                 enum wasi_table_idx idx) NO_THREAD_SAFETY_ANALYSIS
{
        struct wasi_table *table = &wasi->fdtable[idx];
        struct wasi_fdinfo **it;
        uint32_t i;
        VEC_FOREACH_IDX(i, it, table->table) {
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
        VEC_FREE(wasi->mctx, table->table);
}

int
wasi_table_alloc_slot(struct wasi_instance *wasi, enum wasi_table_idx idx,
                      uint32_t *wasifdp) REQUIRES(wasi->lock)
{
        struct wasi_table *table = &wasi->fdtable[idx];
        struct wasi_fdinfo **fdinfop;
        uint32_t wasifd;
        VEC_FOREACH_IDX(wasifd, fdinfop, table->table) {
                if (wasifd < table->reserved_slots) {
                        continue;
                }
                struct wasi_fdinfo *fdinfo = *fdinfop;
                if (fdinfo == NULL) {
                        *wasifdp = wasifd;
                        return 0;
                }
        }
        wasifd = table->table.lsize;
        if (wasifd < table->reserved_slots) {
                wasifd = table->reserved_slots;
        }
        int ret = wasi_table_expand(wasi, idx, wasifd);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo *fdinfo = VEC_ELEM(table->table, wasifd);
        assert(fdinfo == NULL);
        *wasifdp = wasifd;
        return 0;
}

int
wasi_table_fdinfo_add(struct wasi_instance *wasi, enum wasi_table_idx idx,
                      struct wasi_fdinfo *fdinfo, uint32_t *wasifdp)
{
        uint32_t wasifd;
        toywasm_mutex_lock(&wasi->lock);
        int ret = wasi_table_alloc_slot(wasi, idx, &wasifd);
        if (ret != 0) {
                toywasm_mutex_unlock(&wasi->lock);
                return ret;
        }
        wasi_table_affix(wasi, idx, wasifd, fdinfo);
        toywasm_mutex_unlock(&wasi->lock);
        *wasifdp = wasifd;
        return 0;
}

struct wasi_fdinfo **
wasi_table_slot_ptr(struct wasi_instance *wasi, enum wasi_table_idx idx,
                    uint32_t wasifd) REQUIRES(wasi->lock)
{
        assert(idx < WASI_NTABLES);
        struct wasi_table *table = &wasi->fdtable[idx];
        assert(wasifd < table->table.lsize);
        struct wasi_fdinfo **slot = &VEC_ELEM(table->table, wasifd);
        return slot;
}
