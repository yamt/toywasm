/*
 * WASI implementation for toywasm
 *
 * This is a bit relaxed implementation of WASI snapshot preview1.
 *
 * - The "rights" stuff is not implemented. mendokusai.
 *   Also, it's being removed in preview2.
 *
 * - The "openat" family API is intentionally not used in favor
 *   of portability.
 *   Note: It makes this implementation considerably complex/incomplete
 *   in some places because WASI is basically a copy of the openat API
 *   family.
 *
 * - O_RESOLVE_BENEATH behavior is not implemented.
 *   For isolation purposes, maybe you can use a separate filesystem instead.
 *   See TOYWASM_ENABLE_WASI_LITTLEFS for an example.
 *
 * References:
 * https://github.com/WebAssembly/WASI/tree/main/phases/snapshot/witx
 * https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/headers/public/wasi/api.h
 */

#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mem.h"
#include "nbio.h"
#include "wasi.h"
#include "wasi_impl.h"
#include "xlog.h"

#include "wasi_host_subr.h"
#include "wasi_hostfuncs.h"
#include "wasi_vfs_impl_host.h"

#define WASI_API(a, b) WASI_HOST_FUNC(a, b),
#define WASI_API2(a, b, c) WASI_HOST_FUNC2(a, b, c),
const struct host_func wasi_funcs[] = {
#include "wasi_preview1.h"
};

/*
 * a few incompatibilities between wasi_unstable and
 * wasi_snapshot_preview1:
 *
 * |                | unstable | preview1 |
 * |----------------|----------|----------|
 * | SEEK_CUR       | 0        | 1        |
 * | SEEK_END       | 1        | 2        |
 * | SEEK_SET       | 2        | 0        |
 * | filestat nlink | 32-bit   | 64-bit   |
 */
const struct host_func wasi_unstable_funcs[] = {
#include "wasi_unstable.h"
};
#undef WASI_API
#undef WASI_API2

int
wasi_instance_add_hostfd(struct wasi_instance *inst, uint32_t wasmfd,
                         int hostfd)
{
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        const enum wasi_table_idx tblidx = WASI_TABLE_FILES;
        toywasm_mutex_lock(&inst->lock);
        ret = wasi_table_expand(inst, tblidx, wasmfd);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_table_lookup_locked(inst, tblidx, wasmfd, &fdinfo);
        if (ret == 0) {
                ret = EBUSY;
                goto fail;
        }
        if (ret != EBADF) {
                goto fail;
        }
        ret = wasi_fdinfo_alloc_host(&fdinfo);
        if (ret != 0) {
                goto fail;
        }

        /*
         * we make hostfds non-blocking.
         *
         * XXX should restore on failure
         * XXX should restore when we are done
         * XXX this affects other programs sharing files.
         *     (eg. shell pipelines)
         *
         * a fragile hack:
         *
         * tty is often shared with other processes.
         * making such files non blocking breaks other
         * processes.
         * eg. when you run a shell command like
         * "toywasm | more", the tty is toywasm's stdin
         * and also more's stdout.
         *
         * IMO, it's a design mistake (or at least a compromise)
         * to control non-blocking with fcntl(). It should be
         * a per-operation flag instead. eg. MSG_DONTWAIT.
         * Unfortunately, not all operations/platforms have
         * flags like that.
         */
        if (!isatty(hostfd)) {
                ret = set_nonblocking(hostfd, true, NULL);
                if (ret != 0) {
                        xlog_error("set_nonblocking failed on fd %d with %d",
                                   hostfd, ret);
                        goto fail;
                }
        }

        int dupfd;
#if defined(__wasi__) /* wasi has no dup */
        dupfd = hostfd;
#else
        dupfd = dup(hostfd);
#endif
        wasi_fdinfo_to_host(fdinfo)->hostfd = dupfd;
        if (dupfd == -1) {
                xlog_trace("failed to dup: wasm fd %" PRIu32
                           " host fd %u with errno %d",
                           wasmfd, hostfd, errno);
        }
        wasi_table_affix(inst, tblidx, wasmfd, fdinfo);
        fdinfo = NULL;
        ret = 0;
fail:
        toywasm_mutex_unlock(&inst->lock);
        wasi_fdinfo_release(inst, fdinfo);
        return ret;
}

int
wasi_instance_populate_stdio_with_hostfd(struct wasi_instance *inst)
{
        uint32_t nfds = 3;
        uint32_t i;
        int ret;
        for (i = 0; i < nfds; i++) {
                ret = wasi_instance_add_hostfd(inst, i, i);
                if (ret != 0) {
                        xlog_error("wasi_instance_add_hostfd failed on fd "
                                   "%" PRIu32 "with %d",
                                   i, ret);
                        goto fail;
                }
        }
        ret = 0;
fail:
        return ret;
}

int
wasi_instance_create(struct mem_context *mctx,
                     struct wasi_instance **instp) NO_THREAD_SAFETY_ANALYSIS
{
        struct wasi_instance *inst;

        inst = mem_zalloc(mctx, sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        inst->mctx = mctx;
        toywasm_mutex_init(&inst->lock);
        toywasm_cv_init(&inst->cv);
        /* the first three slots are reserved for stdin, stdout, stderr */
        inst->fdtable[WASI_TABLE_FILES].reserved_slots = 3;
        *instp = inst;
        return 0;
}

void
wasi_instance_set_memory(struct wasi_instance *inst, struct meminst *mem)
{
        wasi_memory(inst) = mem;
}

void
wasi_instance_set_args(struct wasi_instance *inst, int argc,
                       const char *const *argv)
{
        inst->argc = argc;
        inst->argv = argv;
#if defined(TOYWASM_ENABLE_TRACING)
        xlog_trace("%s argc = %u", __func__, argc);
        int i;
        for (i = 0; i < argc; i++) {
                xlog_trace("%s arg[%u] = \"%s\"", __func__, i, argv[i]);
        }
#endif
}

void
wasi_instance_set_environ(struct wasi_instance *inst, int nenvs,
                          const char *const *envs)
{
        inst->nenvs = nenvs;
        inst->envs = envs;
#if defined(TOYWASM_ENABLE_TRACING)
        xlog_trace("%s nenvs = %u", __func__, nenvs);
        int i;
        for (i = 0; i < nenvs; i++) {
                xlog_trace("%s env[%u] = \"%s\"", __func__, i, envs[i]);
        }
#endif
}

int
wasi_instance_prestat_add_vfs(struct wasi_instance *wasi, const char *path,
                              struct wasi_vfs *vfs)
{
        struct wasi_fdinfo *fdinfo = NULL;
        char *host_path = NULL;
        char *wasm_path = NULL;
        uint32_t wasifd;
        int ret;
        xlog_trace("prestat adding mapdir %s", path);

        const char *coloncolon = strstr(path, "::");
        if (coloncolon != NULL) {
                /*
                 * <host dir>::<wasm dir>
                 *
                 * intended to be compatible with wasmtime's --dir
                 *
                 * cf.
                 * https://github.com/bytecodealliance/wasmtime/pull/7301
                 */

                host_path = strndup(path, coloncolon - path);
                wasm_path = strdup(coloncolon + 2);
                if (wasm_path == NULL) {
                        ret = ENOMEM;
                        goto fail;
                }
        } else {
                host_path = strdup(path);
        }
        if (host_path == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ret = wasi_fdinfo_alloc_prestat(&fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_fdinfo_prestat *fdinfo_prestat =
                wasi_fdinfo_to_prestat(fdinfo);
        fdinfo_prestat->vfs = vfs;
        fdinfo_prestat->prestat_path = host_path;
        fdinfo_prestat->wasm_path = wasm_path;
        host_path = NULL;
        wasm_path = NULL;
        ret = wasi_table_fdinfo_add(wasi, WASI_TABLE_FILES, fdinfo, &wasifd);
        if (ret != 0) {
                goto fail;
        }
        xlog_trace("prestat added %s (%s)", path,
                   fdinfo_prestat->prestat_path);
        return 0;
fail:
        free(host_path);
        free(wasm_path);
        wasi_fdinfo_free(fdinfo);
        return ret;
}

int
wasi_instance_prestat_add(struct wasi_instance *wasi, const char *path)
{
        struct wasi_vfs *vfs = wasi_get_vfs_host();
        return wasi_instance_prestat_add_vfs(wasi, path, vfs);
}

uint32_t
wasi_instance_exit_code(struct wasi_instance *wasi)
{
        uint32_t exit_code;
        toywasm_mutex_lock(&wasi->lock);
        exit_code = wasi->exit_code;
        toywasm_mutex_unlock(&wasi->lock);
        return exit_code;
}

void
wasi_instance_destroy(struct wasi_instance *inst)
{
        unsigned int i;
        for (i = 0; i < WASI_NTABLES; i++) {
                wasi_table_clear(inst, i);
        }
        toywasm_cv_destroy(&inst->cv);
        toywasm_mutex_destroy(&inst->lock);
        mem_free(inst->mctx, inst, sizeof(*inst));
}

static const struct name wasi_snapshot_preview1 =
        NAME_FROM_CSTR_LITERAL("wasi_snapshot_preview1");

static const struct name wasi_unstable =
        NAME_FROM_CSTR_LITERAL("wasi_unstable");

static const struct host_module module_wasi[] = {
        {
                .module_name = &wasi_snapshot_preview1,
                .funcs = wasi_funcs,
                .nfuncs = ARRAYCOUNT(wasi_funcs),
        },
        {
                .module_name = &wasi_unstable,
                .funcs = wasi_unstable_funcs,
                .nfuncs = ARRAYCOUNT(wasi_unstable_funcs),
        },
        {
                .module_name = &wasi_unstable,
                .funcs = wasi_funcs,
                .nfuncs = ARRAYCOUNT(wasi_funcs),
        },
};

int
import_object_create_for_wasi(struct mem_context *mctx,
                              struct wasi_instance *wasi,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                mctx, module_wasi, ARRAYCOUNT(module_wasi), &wasi->hi, impp);
}
