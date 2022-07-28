#define _POSIX_C_SOURCE 199309 /* clock_gettime */
#define _DARWIN_C_SOURCE       /* arc4random_buf */

#include <sys/random.h> /* getrandom */
#include <sys/stat.h>
#include <sys/uio.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "context.h"
#include "endian.h"
#include "host_instance.h"
#include "type.h"
#include "util.h"
#include "wasi.h"
#include "xlog.h"

struct wasi_instance {
        struct host_instance hi;
        int *fdtable;
        uint32_t nfds;
        int argc;
        char *const *argv;
};

struct wasi_iov {
        uint32_t iov_base;
        uint32_t iov_len;
};

#define WASI_FILETYPE_UNKNOWN 0
#define WASI_FILETYPE_BLOCK_DEVICE 1
#define WASI_FILETYPE_CHARACTER_DEVICE 2
#define WASI_FILETYPE_DIRECTORY 3
#define WASI_FILETYPE_REGULAR_FILE 4
#define WASI_FILETYPE_SOCKET_DGRAM 5
#define WASI_FILETYPE_SOCKET_STREAM 6
#define WASI_FILETYPE_SYMBOLIC_LINK 7

#define WASI_FDFLAG_APPEND 1
#define WASI_FDFLAG_DSYNC 2
#define WASI_FDFLAG_NONBLOCK 4
#define WASI_FDFLAG_RSYNC 8
#define WASI_FDFLAG_SYNC 16

struct wasi_fdstat {
        uint8_t fs_filetype;
        uint8_t pad1;
        uint16_t fs_flags;
        uint8_t pad2[4];
        uint64_t fs_rights_base;
        uint64_t fs_rights_inheriting;
};
_Static_assert(sizeof(struct wasi_fdstat) == 24, "wasi_fdstat");

static int
wasi_copyout(struct exec_context *ctx, const void *hostaddr, uint32_t wasmaddr,
             size_t len)
{
        void *p;
        int ret;

        ret = memory_getptr(ctx, 0, wasmaddr, 0, len, &p);
        if (ret != 0) {
                return ret;
        }
        memcpy(p, hostaddr, len);
        return 0;
}

static int
wasi_copyin_iovec(struct exec_context *ctx, uint32_t iov_uaddr,
                  uint32_t iov_count, struct iovec **resultp)
{
        struct iovec *hostiov = calloc(iov_count, sizeof(*hostiov));
        void *p;
        int ret;
        if (hostiov == NULL) {
                ret = ENOMEM;
                goto fail;
        }
retry:
        ret = memory_getptr(ctx, 0, iov_uaddr, 0,
                            iov_count * sizeof(struct wasi_iov), &p);
        if (ret != 0) {
                goto fail;
        }
        const struct wasi_iov *iov_in_module = p;
        uint32_t i;
        for (i = 0; i < iov_count; i++) {
                bool moved = false;
                uint32_t iov_base = le32_decode(&iov_in_module[i].iov_base);
                uint32_t iov_len = le32_decode(&iov_in_module[i].iov_len);
                xlog_trace("iov [%" PRIu32 "] base %" PRIx32 " len %" PRIu32,
                           i, iov_base, iov_len);
                ret = memory_getptr2(ctx, 0, iov_base, 0, iov_len, &p, &moved);
                if (ret != 0) {
                        goto fail;
                }
                if (moved) {
                        goto retry;
                }
                hostiov[i].iov_base = p;
                hostiov[i].iov_len = iov_len;
        }
        *resultp = hostiov;
        return 0;
fail:
        free(hostiov);
        return ret;
}

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
        uint32_t wasmerrno;
        switch (host_errno) {
        case 0:
                wasmerrno = 0;
                break;
        default:
                wasmerrno = 29; /* EIO */
        }
        return wasmerrno;
}

static int
wasi_proc_exit(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct val *params,
               struct val *results)
{
        xlog_trace("%s called", __func__);
        uint32_t code = params[0].u.i32;
        ctx->exit_code = code;
        return trap_with_id(ctx, TRAP_VOLUNTARY_EXIT,
                            "proc_exit with %" PRIu32, code);
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
        ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov);
        if (ret != 0) {
                goto fail;
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
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        free(hostiov);
        return 0;
}

static int
wasi_fd_read(struct exec_context *ctx, struct host_instance *hi,
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
        ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov);
        if (ret != 0) {
                goto fail;
        }
        ssize_t n = readv(hostfd, hostiov, iov_count);
        if (n == -1) {
                ret = errno;
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
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
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
        uint32_t stat_addr = params[1].u.i32;
        int hostfd;
        int ret;
        ret = wasi_fdlookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        struct stat stat;
        ret = fstat(hostfd, &stat);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_fdstat st;
        memset(&st, 0, sizeof(st));
        if (S_ISREG(stat.st_mode)) {
                st.fs_filetype = WASI_FILETYPE_REGULAR_FILE;
        } else if (S_ISDIR(stat.st_mode)) {
                st.fs_filetype = WASI_FILETYPE_DIRECTORY;
        } else if (S_ISCHR(stat.st_mode)) {
                st.fs_filetype = WASI_FILETYPE_CHARACTER_DEVICE;
        } else if (S_ISBLK(stat.st_mode)) {
                st.fs_filetype = WASI_FILETYPE_BLOCK_DEVICE;
        } else if (S_ISLNK(stat.st_mode)) {
                st.fs_filetype = WASI_FILETYPE_SYMBOLIC_LINK;
        } else {
                st.fs_filetype = WASI_FILETYPE_UNKNOWN;
        }
        /* TODO fs_flags */
        /* TODO fs_rights_base */
        /* TODO fs_rights_inheriting */
        ret = wasi_copyout(ctx, &st, stat_addr, sizeof(st));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_fd_seek(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct val *params,
             struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
        int64_t offset = params[1].u.i64;
        uint32_t whence = params[2].u.i32;
        uint32_t retp = params[3].u.i32;
        int hostfd;
        int ret;
        ret = wasi_fdlookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        int hostwhence;
        switch (whence) {
        case 0:
                hostwhence = SEEK_SET;
                break;
        case 1:
                hostwhence = SEEK_CUR;
                break;
        case 2:
                hostwhence = SEEK_END;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        off_t ret1 = lseek(hostfd, offset, hostwhence);
        if (ret1 == -1) {
                ret = errno;
                goto fail;
        }
        uint64_t result = host_to_le64(ret1);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_clock_time_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct val *params,
                    struct val *results)
{
        xlog_trace("%s called", __func__);
        uint32_t clockid = params[0].u.i32;
#if 0 /* REVISIT what to do with the precision? */
        uint64_t precision = params[1].u.i64;
#endif
        uint32_t retp = params[2].u.i32;
        clockid_t hostclockid;
        int ret;
        switch (clockid) {
        case 0:
                hostclockid = CLOCK_REALTIME;
                break;
        case 1:
                hostclockid = CLOCK_MONOTONIC;
                break;
        case 2:
                /* REVISIT what does this really mean for wasm? */
                hostclockid = CLOCK_PROCESS_CPUTIME_ID;
                break;
        case 3:
                /* REVISIT what does this really mean for wasm? */
                hostclockid = CLOCK_THREAD_CPUTIME_ID;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        struct timespec ts;
        ret = clock_gettime(hostclockid, &ts);
        if (ret == -1) {
                ret = errno;
                goto fail;
        }
        uint64_t result =
                host_to_le64((uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_args_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct val *params,
                    struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t argcp = params[0].u.i32;
        uint32_t argv_buf_sizep = params[1].u.i32;
        int argc = wasi->argc;
        char *const *argv = wasi->argv;
        xlog_trace("%s called argc=%u", __func__, argc);
        int ret;
        uint32_t argc_le32 = host_to_le32(argc);
        ret = wasi_copyout(ctx, &argc_le32, argcp, sizeof(argc_le32));
        if (ret != 0) {
                goto fail;
        }
        int i;
        uint32_t argv_buf_size = 0;
        for (i = 0; i < argc; i++) {
                argv_buf_size += strlen(argv[i]) + 1;
        }
        argv_buf_size = host_to_le32(argv_buf_size);
        ret = wasi_copyout(ctx, &argv_buf_size, argv_buf_sizep,
                           sizeof(argv_buf_size));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_args_get(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct val *params,
              struct val *results)
{
        xlog_trace("%s called", __func__);
        struct wasi_instance *wasi = (void *)hi;
        uint32_t argvp = params[0].u.i32;
        uint32_t argv_buf = params[1].u.i32;
        int argc = wasi->argc;
        char *const *argv = wasi->argv;
        xlog_trace("%s called argc=%u", __func__, argc);
        int ret;
        uint32_t i;
        uint32_t *wasm_argv = malloc(argc * sizeof(*wasm_argv));
        if (wasm_argv == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        uint32_t wasmp = argv_buf;
        for (i = 0; i < argc; i++) {
                le32_encode(&wasm_argv[i], wasmp);
                xlog_trace("wasm_argv[%" PRIu32 "] %" PRIx32, i, wasmp);
                wasmp += strlen(argv[i]) + 1;
        }
        for (i = 0; i < argc; i++) {
                size_t sz = strlen(argv[i]) + 1;
                ret = wasi_copyout(ctx, argv[i], le32_to_host(wasm_argv[i]),
                                   sz);
                if (ret != 0) {
                        goto fail;
                }
        }
        ret = wasi_copyout(ctx, wasm_argv, argvp, argc * sizeof(*wasm_argv));
fail:
        free(wasm_argv);
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_environ_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct val *params,
                       struct val *results)
{
        xlog_trace("%s called", __func__);
        uint32_t environ_count_p = params[0].u.i32;
        uint32_t environ_buf_size_p = params[1].u.i32;
        uint32_t zero = 0; /* REVISIT */
        int ret;
        ret = wasi_copyout(ctx, &zero, environ_count_p, sizeof(zero));
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_copyout(ctx, &zero, environ_buf_size_p, sizeof(zero));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_environ_get(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct val *params,
                 struct val *results)
{
        xlog_trace("%s called", __func__);
        /* REVISIT */
        results[0].u.i32 = wasi_convert_errno(0);
        return 0;
}

static int
wasi_random_get(struct exec_context *ctx, struct host_instance *hi,
                const struct functype *ft, const struct val *params,
                struct val *results)
{
        xlog_trace("%s called", __func__);
        uint32_t buf = params[0].u.i32;
        uint32_t buflen = params[1].u.i32;
        int ret;
        void *p;
        ret = memory_getptr(ctx, 0, buf, 0, buflen, &p);
        if (ret != 0) {
                goto fail;
        }
#if defined(__GLIBC__)
        /*
         * glibc doesn't have arc4random
         * https://sourceware.org/bugzilla/show_bug.cgi?id=4417
         */
        while (buflen > 0) {
                ssize_t ssz = getrandom(p, buflen, 0);
                if (ssz == -1) {
                        ret = errno;
                        if (ret == EINTR) {
                                continue;
                        }
                        break;
                }
                p += ssz;
                buflen -= ssz;
        }
#else
        arc4random_buf(p, buflen);
        ret = 0;
#endif
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
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
        WASI_HOST_FUNC(fd_read, "(iiii)i"),
        WASI_HOST_FUNC(fd_fdstat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_seek, "(iIii)i"),
        WASI_HOST_FUNC(clock_time_get, "(iIi)i"),
        WASI_HOST_FUNC(args_sizes_get, "(ii)i"),
        WASI_HOST_FUNC(args_get, "(ii)i"),
        WASI_HOST_FUNC(environ_sizes_get, "(ii)i"),
        WASI_HOST_FUNC(environ_get, "(ii)i"),
        WASI_HOST_FUNC(random_get, "(ii)i"),
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
wasi_instance_set_args(struct wasi_instance *inst, int argc, char *const *argv)
{
        inst->argc = argc;
        inst->argv = argv;
#if defined(ENABLE_TRACING)
        xlog_trace("%s argc = %u", __func__, argc);
        int i;
        for (i = 0; i < argc; i++) {
                xlog_trace("%s arg[%u] = \"%s\"", __func__, i, argv[i]);
        }
#endif
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
