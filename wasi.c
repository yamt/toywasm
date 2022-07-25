#include <sys/uio.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "context.h"
#include "host_instance.h"
#include "type.h"
#include "util.h"
#include "wasi.h"
#include "xlog.h"

struct wasi_instance {
        struct host_instance hi;
        int *fdtable;
        uint32_t nfds;
};

struct wasi_iov {
        uint32_t iov_base;
        uint32_t iov_len;
};

static int
wasi_fdlookup(struct wasi_instance *wasi, uint32_t wasifd, int *hostfdp)
{
        if (wasifd >= wasi->nfds) {
                return EBADF;
        }
        int hostfd = wasi->fdtable[wasifd];
        if (hostfd == -1) {
                return EBADF;
        }
        *hostfdp = hostfd;
        xlog_trace("hostfd %d found for wasifd %" PRIu32, hostfd, wasifd);
        return 0;
}

static uint32_t
wasi_convert_errno(int host_errno)
{
        /* TODO implement */
        return 29; /* EIO */
}

static int
wasi_proc_exit(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct val *params,
               struct val *results)
{
        xlog_trace("%s called", __func__);
        /* TODO implement */
        results[0].u.i32 = 0;
        return 0;
}

static int
wasi_fd_close(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct val *params,
              struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
        int hostfd;
        int ret;
        ret = wasi_fdlookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        ret = close(hostfd);
        if (ret != 0) {
                ret = errno;
                goto fail;
        }
        ret = 0;
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_fd_write(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct val *params,
              struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
        uint32_t iov_addr = params[1].u.i32;
        uint32_t iov_count = params[2].u.i32;
        uint32_t retp = params[3].u.i32;
        struct iovec *hostiov = NULL;
        void *p;
        int ret;
        int hostfd;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_fdlookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        hostiov = calloc(iov_count, sizeof(*hostiov));
        if (hostiov == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ret = memory_getptr(ctx, 0, iov_addr, 0,
                            iov_count * sizeof(struct wasi_iov), &p);
        if (ret != 0) {
                goto fail;
        }
        const struct wasi_iov *iov_in_module = p;
        uint32_t i;
        for (i = 0; i < iov_count; i++) {
                struct wasi_iov iov;
                memcpy(&iov, &iov_in_module[i], sizeof(iov));
                xlog_trace("iov [%" PRIu32 "] base %" PRIx32 " len %" PRIu32,
                           i, iov.iov_base, iov.iov_len);
                ret = memory_getptr(ctx, 0, iov.iov_base, 0, iov.iov_len, &p);
                if (ret != 0) {
                        goto fail;
                }
                hostiov[i].iov_base = p;
                hostiov[i].iov_len = iov.iov_len;
        }
        ssize_t n = writev(hostfd, hostiov, iov_count);
        if (n == -1) {
                ret = errno;
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = n;
        xlog_trace("nwritten %" PRIu32, r);
        ret = memory_getptr(ctx, 0, retp, 0, sizeof(r), &p);
        if (ret != 0) {
                goto fail;
        }
        memcpy(p, &r, sizeof(r));
        ret = 0;
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        free(hostiov);
        return 0;
}

static int
wasi_fd_fdstat_get(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct val *params,
                   struct val *results)
{
#if defined(ENABLE_TRACING)
        uint32_t fd = params[0].u.i32;
#endif
        xlog_trace("%s called for fd %" PRIu32, __func__, fd);
        // uint32_t stat_addr = params[1].u.i32;

        /* TODO implement */
        results[0].u.i32 = 0;
        return 0;
}

static int
wasi_fd_seek(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct val *params,
             struct val *results)
{
        xlog_trace("%s called", __func__);
        return 0;
}

#define WASI_HOST_FUNC(NAME, TYPE)                                            \
        {                                                                     \
                .name = #NAME, .type = TYPE, .func = wasi_##NAME,             \
        }

const struct host_func wasi_funcs[] = {
        WASI_HOST_FUNC(proc_exit, "(i)"),
        WASI_HOST_FUNC(fd_close, "(i)i"),
        WASI_HOST_FUNC(fd_write, "(iiii)i"),
        WASI_HOST_FUNC(fd_fdstat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_seek, "(iIii)i"),
        /* TODO implement the rest of the api */
};

int
wasi_instance_create(struct wasi_instance **instp)
{
        struct wasi_instance *inst;

        inst = zalloc(sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        /* TODO configurable */
        inst->nfds = 3;
        int ret = ARRAY_RESIZE(inst->fdtable, inst->nfds);
        if (ret != 0) {
                free(inst);
                return ret;
        }
        uint32_t i;
        for (i = 0; i < inst->nfds; i++) {
                int hostfd;
                inst->fdtable[i] = hostfd = dup(i);
                if (hostfd == -1) {
                        xlog_trace("failed to dup: wasm fd %" PRIu32
                                   " host fd %u with errno %d",
                                   i, (int)i, errno);
                }
        }
        *instp = inst;
        return 0;
}

void
wasi_instance_destroy(struct wasi_instance *inst)
{
        uint32_t i;
        for (i = 0; i < inst->nfds; i++) {
                int hostfd = inst->fdtable[i];
                if (hostfd != -1) {
                        int ret = close(hostfd);
                        if (ret != 0) {
                                xlog_trace("failed to close: wasm fd %" PRIu32
                                           " host fd %u with errno %d",
                                           i, hostfd, errno);
                        }
                }
        }
        free(inst->fdtable);
        free(inst);
}

int
import_object_create_for_wasi(struct wasi_instance *wasi,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                "wasi_snapshot_preview1", wasi_funcs, ARRAYCOUNT(wasi_funcs),
                &wasi->hi, impp);
}
