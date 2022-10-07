/*
 * WASI implementation for toywasm
 *
 * This is a bit relaxed implementation of WASI preview1.
 *
 * - The "rights" stuff is not implemented. mendokusai.
 *
 * - The "openat" family API is intentionally not used in favor
 *   of portability.
 *   Note: It makes this implementation considerably complex/incomplete
 *   in some places because WASI is basically a copy of the openat API
 *   family.
 *
 * References:
 * https://github.com/WebAssembly/WASI/tree/main/phases/snapshot/witx
 * https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/headers/public/wasi/api.h
 */

#define _POSIX_C_SOURCE 199309 /* clock_gettime */
#define _DARWIN_C_SOURCE       /* arc4random_buf */
#define _GNU_SOURCE            /* asprintf, realpath, O_DIRECTORY */

#include <sys/random.h> /* getrandom */
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "context.h"
#include "endian.h"
#include "host_instance.h"
#include "type.h"
#include "util.h"
#include "vec.h"
#include "wasi.h"
#include "wasi_abi.h"
#include "xlog.h"

struct wasi_fdinfo {
        /*
         * - directories added by wasi_instance_prestat_add
         *   prestat_path != NULL
         *   hostfd == -1 (for now)
         *
         * - files opened by user
         *   prestat_path == NULL
         *   hostfd != -1
         *
         * - closed descriptors (EBADF)
         *   prestat_path == NULL
         *   hostfd == -1
         */
        char *prestat_path;
        int hostfd;
        DIR *dir;
};

struct wasi_instance {
        struct host_instance hi;
        VEC(, struct wasi_fdinfo) fdtable; /* indexed by wasm fd */
        int argc;
        char *const *argv;
};

#if defined(ENABLE_TRACING)
#define WASI_TRACE                                                            \
        do {                                                                  \
                xlog_trace("WASI: %s called", __func__);                      \
        } while (0)
#else
#define WASI_TRACE                                                            \
        do {                                                                  \
        } while (0)
#endif

#if defined(__wasi__)
/*
 * For some reasons, wasi-libc doesn't have legacy stuff enabled.
 * It includes lutimes and futimes.
 */

static int
lutimes(const char *path, const struct timeval *tvp)
{
        struct timespec ts[2];
        const struct timespec *tsp;
        if (tvp != NULL) {
                ts[0].tv_sec = tvp[0].tv_sec;
                ts[0].tv_nsec = tvp[0].tv_usec * 1000;
                ts[1].tv_sec = tvp[1].tv_sec;
                ts[1].tv_nsec = tvp[1].tv_usec * 1000;
                tsp = ts;
        } else {
                tsp = NULL;
        }
        return utimensat(AT_FDCWD, path, tsp, AT_SYMLINK_NOFOLLOW);
}

static int
futimes(int fd, const struct timeval *tvp)
{
        struct timespec ts[2];
        const struct timespec *tsp;
        if (tvp != NULL) {
                ts[0].tv_sec = tvp[0].tv_sec;
                ts[0].tv_nsec = tvp[0].tv_usec * 1000;
                ts[1].tv_sec = tvp[1].tv_sec;
                ts[1].tv_nsec = tvp[1].tv_usec * 1000;
                tsp = ts;
        } else {
                tsp = NULL;
        }
        return futimens(fd, tsp);
}
#endif

#if defined(__APPLE__)
static int
racy_fallocate(int fd, off_t offset, off_t size)
{
        struct stat sb;
        int ret;

        off_t newsize = offset + size;
        if (newsize < offset) {
                return EOVERFLOW;
        }
        ret = fstat(fd, &sb);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                return ret;
        }
        if (sb.st_size >= newsize) {
                return 0;
        }
        ret = ftruncate(fd, newsize);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
        }
        return ret;
}
#endif

static int
wasi_copyin(struct exec_context *ctx, void *hostaddr, uint32_t wasmaddr,
            size_t len)
{
        void *p;
        int ret;
        ret = memory_getptr(ctx, 0, wasmaddr, 0, len, &p);
        if (ret != 0) {
                return ret;
        }
        memcpy(hostaddr, p, len);
        return 0;
}

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
wasi_fd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
               struct wasi_fdinfo **infop)
{
        if (wasifd >= wasi->fdtable.lsize) {
                return EBADF;
        }
        *infop = &VEC_ELEM(wasi->fdtable, wasifd);
        return 0;
}

static int
wasi_hostfd_lookup(struct wasi_instance *wasi, uint32_t wasifd, int *hostfdp)
{
        struct wasi_fdinfo *info;
        int ret = wasi_fd_lookup(wasi, wasifd, &info);
        if (ret != 0) {
                return ret;
        }
        if (info->hostfd == -1) {
                return EBADF;
        }
        *hostfdp = info->hostfd;
        return 0;
}

static int
wasi_fd_expand_table(struct wasi_instance *wasi, uint32_t maxfd)
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
                struct wasi_fdinfo *fdinfo = &VEC_ELEM(wasi->fdtable, i);
                fdinfo->hostfd = -1;
                fdinfo->dir = NULL;
                fdinfo->prestat_path = NULL;
        }
        return 0;
}

static int
wasi_fd_alloc(struct wasi_instance *wasi, uint32_t *wasifdp)
{
        struct wasi_fdinfo *fdinfo;
        uint32_t wasifd;
        VEC_FOREACH_IDX(wasifd, fdinfo, wasi->fdtable) {
                if (fdinfo->hostfd == -1 && fdinfo->prestat_path == NULL) {
                        *wasifdp = wasifd;
                        return 0;
                }
        }
        wasifd = wasi->fdtable.lsize;
        int ret = wasi_fd_expand_table(wasi, wasifd);
        if (ret != 0) {
                return ret;
        }
        *wasifdp = wasifd;
        return 0;
}

static int
wasi_fd_add(struct wasi_instance *wasi, int hostfd, char *path,
            uint32_t *wasifdp)
{
        uint32_t wasifd;
        int ret;
        ret = wasi_fd_alloc(wasi, &wasifd);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo *fdinfo = &VEC_ELEM(wasi->fdtable, wasifd);
        fdinfo->hostfd = hostfd;
        fdinfo->dir = NULL;
        fdinfo->prestat_path = path;
        *wasifdp = wasifd;
        return 0;
}

static uint64_t
timespec_to_ns(const struct timespec *ts)
{
        return (uint64_t)ts->tv_sec * 1000000000 + ts->tv_nsec;
}

static void
timeval_from_ns(struct timeval *tv, uint64_t ns)
{
        tv->tv_sec = ns / 1000000000;
        tv->tv_usec = (ns % 1000000000) / 1000;
}

static int
prepare_utimes_tv(uint32_t fstflags, uint64_t atim, uint64_t mtim,
                  struct timeval tvstore[2], const struct timeval **resultp)
{
        const struct timeval *tvp;
        if (fstflags == (WASI_FSTFLAG_ATIM_NOW | WASI_FSTFLAG_MTIM_NOW)) {
                tvp = NULL;
        } else if (fstflags == (WASI_FSTFLAG_ATIM | WASI_FSTFLAG_MTIM)) {
                timeval_from_ns(&tvstore[0], atim);
                timeval_from_ns(&tvstore[1], mtim);
                tvp = tvstore;
        } else {
                return ENOTSUP;
        }
        *resultp = tvp;
        return 0;
}

static uint8_t
wasi_convert_filetype(mode_t mode)
{
        uint8_t type;
        if (S_ISREG(mode)) {
                type = WASI_FILETYPE_REGULAR_FILE;
        } else if (S_ISDIR(mode)) {
                type = WASI_FILETYPE_DIRECTORY;
        } else if (S_ISCHR(mode)) {
                type = WASI_FILETYPE_CHARACTER_DEVICE;
        } else if (S_ISBLK(mode)) {
                type = WASI_FILETYPE_BLOCK_DEVICE;
        } else if (S_ISLNK(mode)) {
                type = WASI_FILETYPE_SYMBOLIC_LINK;
        } else {
                type = WASI_FILETYPE_UNKNOWN;
        }
        return type;
}

static uint8_t
wasi_convert_dirent_filetype(uint8_t hosttype)
{
        uint8_t t;
        switch (hosttype) {
        case DT_REG:
                t = WASI_FILETYPE_REGULAR_FILE;
                break;
        case DT_DIR:
                t = WASI_FILETYPE_DIRECTORY;
                break;
        case DT_CHR:
                t = WASI_FILETYPE_CHARACTER_DEVICE;
                break;
        case DT_BLK:
                t = WASI_FILETYPE_BLOCK_DEVICE;
                break;
        case DT_LNK:
                t = WASI_FILETYPE_SYMBOLIC_LINK;
                break;
        case DT_UNKNOWN:
        default:
                t = WASI_FILETYPE_UNKNOWN;
                break;
        }
        return t;
}

static int
wasi_copyin_and_convert_path(struct exec_context *ctx,
                             struct wasi_instance *wasi, uint32_t dirwasifd,
                             uint32_t path, uint32_t pathlen, char **wasmpathp,
                             char **hostpathp)
{
        /*
         * TODO: somehow prevent it from escaping the dirwasifd directory.
         * eg. reject too many ".."s, check symlinks, etc
         */
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        wasmpath = malloc(pathlen + 1);
        if (wasmpath == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ret = wasi_copyin(ctx, wasmpath, path, pathlen);
        if (ret != 0) {
                goto fail;
        }
        wasmpath[pathlen] = 0;
        if (strlen(wasmpath) != pathlen) {
                /* Note: wasmtime returns EINVAL for embedded NULs */
                ret = EINVAL;
                goto fail;
        }
        if (wasmpath[0] == '/') {
                ret = EPERM;
                goto fail;
        }
        struct wasi_fdinfo *dirfdinfo;
        ret = wasi_fd_lookup(wasi, dirwasifd, &dirfdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (dirfdinfo->prestat_path == NULL) {
                ret = EBADF;
                goto fail;
        }
        ret = asprintf(&hostpath, "%s/%s", dirfdinfo->prestat_path, wasmpath);
        if (ret < 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        *hostpathp = hostpath;
        *wasmpathp = wasmpath;
        return 0;
fail:
        free(hostpath);
        free(wasmpath);
        return ret;
}

static int
wasi_convert_clockid(uint32_t clockid, clockid_t *hostidp)
{
        clockid_t hostclockid;
        int ret = 0;
        switch (clockid) {
        case WASI_CLOCK_ID_REALTIME:
                hostclockid = CLOCK_REALTIME;
                break;
        case WASI_CLOCK_ID_MONOTONIC:
                hostclockid = CLOCK_MONOTONIC;
                break;
        case WASI_CLOCK_ID_PROCESS_CPUTIME_ID:
                /* REVISIT what does this really mean for wasm? */
                hostclockid = CLOCK_PROCESS_CPUTIME_ID;
                break;
        case WASI_CLOCK_ID_THREAD_CPUTIME_ID:
                /* REVISIT what does this really mean for wasm? */
                hostclockid = CLOCK_THREAD_CPUTIME_ID;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        *hostidp = hostclockid;
        return 0;
fail:
        return ret;
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
        case EACCES:
                wasmerrno = 2;
                break;
        case EAGAIN:
                wasmerrno = 6;
                break;
        case EBADF:
                wasmerrno = 8;
                break;
        case EEXIST:
                wasmerrno = 20;
                break;
        case EINTR:
                wasmerrno = 27;
                break;
        case EINVAL:
                wasmerrno = 28;
                break;
        case EISDIR:
                wasmerrno = 31;
                break;
        case ELOOP:
                wasmerrno = 32;
                break;
        case ENOENT:
                wasmerrno = 44;
                break;
        case ENOTDIR:
                wasmerrno = 54;
                break;
        case ENOTEMPTY:
                wasmerrno = 55;
                break;
        case EPERM:
                wasmerrno = 63;
                break;
        case ERANGE:
                wasmerrno = 68;
                break;
#if defined(ENOTCAPABLE)
        case ENOTCAPABLE:
                wasmerrno = 76;
                break;
#endif
        default:
                wasmerrno = 29; /* EIO */
        }
        xlog_trace("error converted from %u to %" PRIu32, host_errno,
                   wasmerrno);
        return wasmerrno;
}

static int
wasi_proc_exit(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t code = HOST_FUNC_PARAM(ft, params, 0, i32);
        ctx->exit_code = code;
        return trap_with_id(ctx, TRAP_VOLUNTARY_EXIT,
                            "proc_exit with %" PRIu32, code);
}

static int
wasi_fd_advise(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t len = HOST_FUNC_PARAM(ft, params, 2, i64);
        uint32_t adv = HOST_FUNC_PARAM(ft, params, 3, i32);
#endif
        int ret;
        int hostfd;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        ret = 0;
        /* no-op */
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_allocate(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t len = HOST_FUNC_PARAM(ft, params, 2, i64);
        int ret;
        int hostfd;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        /*
         * macOS doesn't have posix_fallocate
         * cf. https://github.com/WebAssembly/wasi-filesystem/issues/19
         */
#if defined(__APPLE__)
        ret = racy_fallocate(hostfd, offset, len);
#else
        ret = posix_fallocate(hostfd, offset, len);
#endif
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_filestat_set_size(struct exec_context *ctx, struct host_instance *hi,
                          const struct functype *ft, const struct cell *params,
                          struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint64_t size = HOST_FUNC_PARAM(ft, params, 1, i64);
        int ret;
        int hostfd;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        xlog_trace("ftruncate wasifd %" PRIu32 " hostfd %d size %" PRIu64,
                   wasifd, hostfd, size);
        ret = ftruncate(hostfd, size);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_close(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct wasi_fdinfo *fdinfo;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (fdinfo->hostfd == -1) {
                ret = EBADF;
                goto fail;
        }
        ret = close(fdinfo->hostfd);
        fdinfo->hostfd = -1;
        if (fdinfo->dir != NULL) {
                closedir(fdinfo->dir);
        }
        fdinfo->dir = NULL;
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        ret = 0;
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_write(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct iovec *hostiov = NULL;
        int ret;
        int hostfd;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
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
                assert(ret > 0);
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        free(hostiov);
        return 0;
}

static int
wasi_fd_pwrite(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 3, i64);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct iovec *hostiov = NULL;
        int ret;
        int hostfd;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov);
        if (ret != 0) {
                goto fail;
        }
        ssize_t n = pwritev(hostfd, hostiov, iov_count, offset);
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        free(hostiov);
        return 0;
}

static int
wasi_fd_read(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct iovec *hostiov = NULL;
        int ret;
        int hostfd;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
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
                assert(ret > 0);
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        free(hostiov);
        return 0;
}

static int
wasi_fd_pread(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 3, i64);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct iovec *hostiov = NULL;
        int ret;
        int hostfd;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov);
        if (ret != 0) {
                goto fail;
        }
        ssize_t n = preadv(hostfd, hostiov, iov_count, offset);
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        free(hostiov);
        return 0;
}

static int
wasi_fd_readdir(struct exec_context *ctx, struct host_instance *hi,
                const struct functype *ft, const struct cell *params,
                struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t buf = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t buflen = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint64_t cookie = HOST_FUNC_PARAM(ft, params, 3, i64);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        int ret;
        int hostfd;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_fdinfo *fdinfo = &VEC_ELEM(wasi->fdtable, wasifd);
        DIR *dir = fdinfo->dir;
        if (dir == NULL) {
                dir = fdopendir(hostfd);
                if (dir == NULL) {
                        ret = errno;
                        assert(ret > 0);
                        goto fail;
                }
                fdinfo->dir = dir;
        }
        if (cookie == WASI_DIRCOOKIE_START) {
                /*
                 * Note: rewinddir invalidates cookies.
                 * is it what WASI expects?
                 */
                rewinddir(dir);
        } else if (cookie > LONG_MAX) {
                ret = EINVAL;
                goto fail;
        } else {
                seekdir(dir, cookie);
        }
        uint32_t n = 0;
        while (true) {
                struct dirent *d;
                d = readdir(dir);
                if (d == NULL) {
                        break;
                }
                long nextloc = telldir(dir);
                struct wasi_dirent wde;
                memset(&wde, 0, sizeof(wde));
                le64_encode(&wde.d_next, nextloc);
                le64_encode(&wde.d_ino, d->d_ino);
                uint32_t namlen = strlen(d->d_name);
                le32_encode(&wde.d_namlen, namlen);
                wde.d_type = wasi_convert_dirent_filetype(d->d_type);
                if (buflen - n < sizeof(wde)) {
                        n = buflen; /* signal buffer full */
                        break;
                }
                ret = wasi_copyout(ctx, &wde, buf, sizeof(wde));
                if (ret != 0) {
                        goto fail;
                }
                buf += sizeof(wde);
                n += sizeof(wde);
                if (buflen - n < namlen) {
                        n = buflen; /* signal buffer full */
                        break;
                }
                ret = wasi_copyout(ctx, d->d_name, buf, namlen);
                if (ret != 0) {
                        goto fail;
                }
                buf += namlen;
                n += namlen;
        }
        uint32_t r = host_to_le32(n);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_fdstat_get(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t stat_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_fdstat st;
        memset(&st, 0, sizeof(st));
        if (fdinfo->prestat_path != NULL) {
                st.fs_filetype = WASI_FILETYPE_DIRECTORY;
        } else {
                struct stat stat;
                int hostfd = fdinfo->hostfd;
                ret = fstat(hostfd, &stat);
                if (ret != 0) {
                        ret = errno;
                        assert(ret > 0);
                        goto fail;
                }
                st.fs_filetype = wasi_convert_filetype(stat.st_mode);
        }
        /* TODO fs_flags */
        /* TODO fs_rights_base */
        /* TODO fs_rights_inheriting */
        ret = wasi_copyout(ctx, &st, stat_addr, sizeof(st));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_fdstat_set_flags(struct exec_context *ctx, struct host_instance *hi,
                         const struct functype *ft, const struct cell *params,
                         struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
#endif
        struct wasi_fdinfo *fdinfo;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        /* TODO implement */
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_fdstat_set_rights(struct exec_context *ctx, struct host_instance *hi,
                          const struct functype *ft, const struct cell *params,
                          struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0
        uint64_t base = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t inheriting = HOST_FUNC_PARAM(ft, params, 2, i64);
#endif
        struct wasi_fdinfo *fdinfo;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        /* TODO implement */
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_seek(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        int64_t offset = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint32_t whence = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
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
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(ret1);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_tell(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        off_t ret1 = lseek(hostfd, 0, SEEK_CUR);
        if (ret1 == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(ret1);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_renumber(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd_from = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t wasifd_to = HOST_FUNC_PARAM(ft, params, 1, i32);
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd_from, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_fd_expand_table(wasi, wasifd_to);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_fdinfo *fdinfo_from =
                &VEC_ELEM(wasi->fdtable, wasifd_from);
        struct wasi_fdinfo *fdinfo_to = &VEC_ELEM(wasi->fdtable, wasifd_to);
        assert(fdinfo_from->hostfd == hostfd);
        if (fdinfo_to->hostfd != -1) {
                close(fdinfo_to->hostfd);
        }
        if (fdinfo_to->dir != NULL) {
                closedir(fdinfo_to->dir);
        }
        free(fdinfo_to->prestat_path);
        fdinfo_to->hostfd = fdinfo_from->hostfd;
        fdinfo_to->dir = fdinfo_from->dir;
        fdinfo_to->prestat_path = fdinfo_from->prestat_path;
        fdinfo_from->hostfd = -1;
        fdinfo_from->dir = NULL;
        fdinfo_from->prestat_path = NULL;
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_filestat_get(struct exec_context *ctx, struct host_instance *hi,
                     const struct functype *ft, const struct cell *params,
                     struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        struct stat hst;
        ret = fstat(hostfd, &hst);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        struct wasi_filestat wst;
        memset(&wst, 0, sizeof(wst));
        wst.dev = host_to_le64(hst.st_dev);
        wst.ino = host_to_le64(hst.st_ino);
        wst.type = wasi_convert_filetype(hst.st_mode);
        wst.linkcount = host_to_le64(hst.st_nlink);
        wst.size = host_to_le64(hst.st_size);
#if defined(__APPLE__)
        wst.atim = host_to_le64(timespec_to_ns(&hst.st_atimespec));
        wst.mtim = host_to_le64(timespec_to_ns(&hst.st_mtimespec));
        wst.ctim = host_to_le64(timespec_to_ns(&hst.st_ctimespec));
#else
        wst.atim = host_to_le64(timespec_to_ns(&hst.st_atim));
        wst.mtim = host_to_le64(timespec_to_ns(&hst.st_mtim));
        wst.ctim = host_to_le64(timespec_to_ns(&hst.st_ctim));
#endif
        ret = wasi_copyout(ctx, &wst, retp, sizeof(wst));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_filestat_set_times(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint64_t atim = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t mtim = HOST_FUNC_PARAM(ft, params, 2, i64);
        uint32_t fstflags = HOST_FUNC_PARAM(ft, params, 3, i32);
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
        if (ret != 0) {
                goto fail;
        }
        struct timeval tv[2];
        const struct timeval *tvp;
        ret = prepare_utimes_tv(fstflags, atim, mtim, tv, &tvp);
        if (ret != 0) {
                goto fail;
        }
        ret = futimes(hostfd, tvp);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_prestat_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        int ret;
        struct wasi_fdinfo *fdinfo;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (fdinfo->prestat_path == NULL) {
                ret = EBADF;
                goto fail;
        }
        struct wasi_fd_prestat st;
        memset(&st, 0, sizeof(st));
        st.type = WASI_PREOPEN_TYPE_DIR;
        st.dir_name_len = host_to_le32(strlen(fdinfo->prestat_path));
        ret = wasi_copyout(ctx, &st, retp, sizeof(st));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_fd_prestat_dir_name(struct exec_context *ctx, struct host_instance *hi,
                         const struct functype *ft, const struct cell *params,
                         struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        int ret;
        struct wasi_fdinfo *fdinfo;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (fdinfo->prestat_path == NULL) {
                xlog_trace("wasm fd %" PRIu32 " is not prestat", wasifd);
                ret = EBADF;
                goto fail;
        }
        xlog_trace("wasm fd %" PRIu32 " is prestat %s", wasifd,
                   fdinfo->prestat_path);

        size_t len = strlen(fdinfo->prestat_path);
        if (len != pathlen) {
                xlog_trace("pathlen mismatch %zu != %" PRIu32, len, pathlen);
                ret = EINVAL;
                goto fail;
        }
        ret = wasi_copyout(ctx, fdinfo->prestat_path, path, len);
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
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
        struct wasi_event *events;
        int ret;
        if (nsubscriptions == 0) {
                /*
                 * I couldn't find any authoritive document about this.
                 * EINVAL is what some of wasmtime wasi tests expect.
                 * https://github.com/bytecodealliance/wasmtime/blob/c9ff14e00bb0a90905a4f1cc2968c1e3e0417ce5/crates/test-programs/wasi-tests/src/bin/poll_oneoff_files.rs#L45-L54
                 */
                xlog_trace("poll_oneoff: no subscriptions");
                ret = EINVAL;
                goto fail;
        }
        void *p;
        size_t insize = nsubscriptions * sizeof(struct wasi_subscription);
retry:
        ret = memory_getptr(ctx, 0, in, 0, insize, &p);
        if (ret != 0) {
                goto fail;
        }
        subscriptions = p;
        bool moved = false;
        size_t outsize = nsubscriptions * sizeof(struct wasi_event);
        ret = memory_getptr2(ctx, 0, out, 0, outsize, &p, &moved);
        if (ret != 0) {
                goto fail;
        }
        if (moved) {
                goto retry;
        }
        events = p;
        pollfds = calloc(nsubscriptions, sizeof(*pollfds));
        if (pollfds == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        uint32_t i;
        int timeout_ms = -1;
        uint64_t timeout_ns;
        for (i = 0; i < nsubscriptions; i++) {
                const struct wasi_subscription *s = &subscriptions[i];
                struct pollfd *pfd = &pollfds[i];
                switch (s->type) {
                case WASI_EVENTTYPE_CLOCK:
                        switch (le32_to_host(s->u.clock.clock_id)) {
                        case WASI_CLOCK_ID_REALTIME:
                                break;
                        case WASI_CLOCK_ID_MONOTONIC:
                                /* TODO: this is of course wrong */
                                xlog_trace("poll_oneoff: treating MONOTONIC "
                                           "as REALTIME");
                                break;
                        default:
                                xlog_trace("poll_oneoff: unsupported clock id "
                                           "%" PRIu32,
                                           le32_to_host(s->u.clock.clock_id));
                                ret = ENOTSUP;
                                goto fail;
                        }
                        if ((s->u.clock.flags &
                             host_to_le16(WASI_SUBCLOCKFLAG_ABSTIME)) != 0) {
                                xlog_trace("poll_oneoff: unsupported flag "
                                           "%" PRIx32,
                                           le16_to_host(s->u.clock.flags));
                                ret = ENOTSUP;
                                goto fail;
                        }
                        if (timeout_ms != -1) {
                                xlog_trace("poll_oneoff: multiple clock "
                                           "subscriptions");
                                ret = ENOTSUP;
                                goto fail;
                        }
                        timeout_ns = le64_to_host(s->u.clock.timeout);
                        if (timeout_ns > (uint64_t)INT_MAX * 1000000) {
                                timeout_ms = INT_MAX; /* early timeout is ok */
                        } else {
                                timeout_ms = timeout_ns / 1000000;
                        }
                        pfd->fd = -1;
                        xlog_trace("poll_oneoff: pfd[%" PRIu32 "] timer %d ms",
                                   i, timeout_ms);
                        break;
                case WASI_EVENTTYPE_FD_READ:
                case WASI_EVENTTYPE_FD_WRITE:
                        assert(s->u.fd_read.fd == s->u.fd_write.fd);
                        ret = wasi_hostfd_lookup(
                                wasi, le32_to_host(s->u.fd_read.fd), &pfd->fd);
                        if (ret != 0) {
                                goto fail;
                        }
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
        xlog_trace("poll_oneoff: start polling");
        ret = poll(pollfds, nsubscriptions, timeout_ms);
        if (ret < 0) {
                ret = errno;
                assert(ret > 0);
                xlog_trace("poll_oneoff: poll failed with %d", ret);
                goto fail;
        }
        xlog_trace("poll_oneoff: poll returned %d", ret);
        uint32_t nevents;
        if (ret == 0) {
                nevents = 1; /* timeout is an event */
        } else {
                nevents = ret;
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
                        if (ret == 0) {
                                ev++;
                        }
                        break;
                case WASI_EVENTTYPE_FD_READ:
                case WASI_EVENTTYPE_FD_WRITE:
                        if (pfd->revents != 0) {
                                ev++;
                        }
                        break;
                default:
                        assert(false);
                }
        }
        assert(events + nevents == ev);
        uint32_t result = host_to_le32(nevents);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        free(pollfds);
        return 0;
}

static int
wasi_clock_res_get(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t clockid = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        clockid_t hostclockid;
        int ret;
        ret = wasi_convert_clockid(clockid, &hostclockid);
        if (ret != 0) {
                goto fail;
        }
        struct timespec ts;
        ret = clock_getres(hostclockid, &ts);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(timespec_to_ns(&ts));
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_clock_time_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t clockid = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0 /* REVISIT what to do with the precision? */
        uint64_t precision = HOST_FUNC_PARAM(ft, params, 1, i64);
#endif
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 2, i32);
        clockid_t hostclockid;
        int ret;
        ret = wasi_convert_clockid(clockid, &hostclockid);
        if (ret != 0) {
                goto fail;
        }
        struct timespec ts;
        ret = clock_gettime(hostclockid, &ts);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(timespec_to_ns(&ts));
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_args_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t argcp = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t argv_buf_sizep = HOST_FUNC_PARAM(ft, params, 1, i32);
        int argc = wasi->argc;
        char *const *argv = wasi->argv;
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
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_args_get(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t argvp = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t argv_buf = HOST_FUNC_PARAM(ft, params, 1, i32);
        int argc = wasi->argc;
        char *const *argv = wasi->argv;
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
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_environ_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        WASI_TRACE;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t environ_count_p = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t environ_buf_size_p = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t zero = 0; /* REVISIT */
        int ret;
        ret = wasi_copyout(ctx, &zero, environ_count_p, sizeof(zero));
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_copyout(ctx, &zero, environ_buf_size_p, sizeof(zero));
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_environ_get(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        /* REVISIT */
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, 0);
        return 0;
}

static int
wasi_random_get(struct exec_context *ctx, struct host_instance *hi,
                const struct functype *ft, const struct cell *params,
                struct cell *results)
{
        WASI_TRACE;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t buf = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t buflen = HOST_FUNC_PARAM(ft, params, 1, i32);
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
                        assert(ret > 0);
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
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_open(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t wasmoflags = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint64_t rights_base = HOST_FUNC_PARAM(ft, params, 5, i64);
#if 0
        uint64_t rights_inherit = HOST_FUNC_PARAM(ft, params, 6, i64);
#endif
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 7, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 8, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int hostfd = -1;
        int ret;
        int oflags = 0;
        xlog_trace("wasm oflags %" PRIx32 " rights_base %" PRIx64, wasmoflags,
                   rights_base);
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) == 0) {
                oflags |= O_NOFOLLOW;
                xlog_trace("oflag O_NOFOLLOW");
        }
        if ((wasmoflags & 1) != 0) {
                oflags |= O_CREAT;
                xlog_trace("oflag O_CREAT");
        }
        if ((wasmoflags & 2) != 0) {
                oflags |= O_DIRECTORY;
                xlog_trace("oflag O_DIRECTORY");
        }
        if ((wasmoflags & 4) != 0) {
                oflags |= O_EXCL;
                xlog_trace("oflag O_EXCL");
        }
        if ((wasmoflags & 8) != 0) {
                oflags |= O_TRUNC;
                xlog_trace("oflag O_TRUNC");
        }
        if ((fdflags & WASI_FDFLAG_APPEND) != 0) {
                oflags |= O_APPEND;
                xlog_trace("oflag O_APPEND");
        }
        switch (rights_base & (WASI_RIGHT_FD_READ | WASI_RIGHT_FD_WRITE)) {
        case WASI_RIGHT_FD_READ:
        default:
                oflags |= O_RDONLY;
                break;
        case WASI_RIGHT_FD_WRITE:
                oflags |= O_WRONLY;
                break;
        case WASI_RIGHT_FD_READ | WASI_RIGHT_FD_WRITE:
                oflags |= O_RDWR;
                break;
        }
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        xlog_trace("open %s oflags %x", hostpath, oflags);
        hostfd = open(hostpath, oflags, 0777);
        if (hostfd == -1) {
                ret = errno;
                assert(ret > 0);
                xlog_trace("open %s oflags %x failed with %d", hostpath,
                           oflags, errno);
                goto fail;
        }
        uint32_t wasifd;
        ret = wasi_fd_add(wasi, hostfd, hostpath, &wasifd);
        if (ret != 0) {
                goto fail;
        }
        hostpath = NULL; /* consumed by wasi_fd_add */
        hostfd = -1;
        xlog_trace("-> new wasi fd %" PRIu32, wasifd);
        uint32_t r = host_to_le32(wasifd);
        ret = wasi_copyout(ctx, &r, retp, sizeof(r));
        if (ret != 0) {
                goto fail;
        }
fail:
        if (hostfd != -1) {
                close(hostfd);
        }
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_unlink_file(struct exec_context *ctx, struct host_instance *hi,
                      const struct functype *ft, const struct cell *params,
                      struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        ret = unlink(hostpath);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_create_directory(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        ret = mkdir(hostpath, 0777);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_remove_directory(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        ret = rmdir(hostpath);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_symlink(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t target = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t targetlen = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 4, i32);
        char *target_buf;
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        target_buf = malloc(targetlen + 1);
        if (target_buf == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ret = wasi_copyin(ctx, target_buf, target, targetlen);
        if (ret != 0) {
                goto fail;
        }
        target_buf[targetlen] = 0;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        ret = symlink(target_buf, hostpath);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        free(target_buf);
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_readlink(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t buf = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t buflen = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 5, i32);
        void *tmpbuf = NULL;
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        /*
         * Traditionaly, readlink with an insufficiant buffer size
         * silently truncates the contents.
         * It's also what POSIX requires:
         * > If the buf argument is not large enough to contain
         * > the link content, the first bufsize bytes shall be
         * > placed in buf.
         *
         * However, for some reasons, wasmtime changed it to
         * return ERANGE.
         * https://github.com/bytecodealliance/wasmtime/commit/222a57868e9c01baa838aa81e92a80451e2d920a
         *
         * I couldn't find any authoritative text about this in
         * the WASI spec.
         *
         * Here we tries to detect the case using a bit larger
         * buffer than requested and mimick the wasmtime behavior.
         */
        tmpbuf = malloc(buflen + 1);
        if (tmpbuf == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ssize_t ret1 = readlink(hostpath, tmpbuf, buflen + 1);
        if (ret1 == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (ret1 >= buflen + 1) {
                ret = ERANGE;
                goto fail;
        }
        ret = wasi_copyout(ctx, tmpbuf, buf, ret1);
        if (ret != 0) {
                goto fail;
        }
        uint32_t result = le32_to_host(ret1);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
        if (ret != 0) {
                goto fail;
        }
fail:
        free(tmpbuf);
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_link(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd1 = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path1 = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen1 = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t dirwasifd2 = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t path2 = HOST_FUNC_PARAM(ft, params, 5, i32);
        uint32_t pathlen2 = HOST_FUNC_PARAM(ft, params, 6, i32);
        char *hostpath1 = NULL;
        char *wasmpath1 = NULL;
        char *hostpath2 = NULL;
        char *wasmpath2 = NULL;
        int ret;
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) == 0) {
#if 1
                ret = ENOTSUP;
                goto fail;
#else
                xlog_trace(
                        "path_link: Ignoring !WASI_LOOKUPFLAG_SYMLINK_FOLLOW");
#endif
        }
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd1, path1,
                                           pathlen1, &wasmpath1, &hostpath1);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd2, path2,
                                           pathlen2, &wasmpath2, &hostpath2);
        if (ret != 0) {
                goto fail;
        }
        ret = link(hostpath1, hostpath2);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        free(hostpath1);
        free(wasmpath1);
        free(hostpath2);
        free(wasmpath2);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_rename(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd1 = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path1 = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen1 = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t dirwasifd2 = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t path2 = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t pathlen2 = HOST_FUNC_PARAM(ft, params, 5, i32);
        char *hostpath1 = NULL;
        char *wasmpath1 = NULL;
        char *hostpath2 = NULL;
        char *wasmpath2 = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd1, path1,
                                           pathlen1, &wasmpath1, &hostpath1);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd2, path2,
                                           pathlen2, &wasmpath2, &hostpath2);
        if (ret != 0) {
                goto fail;
        }
        ret = rename(hostpath1, hostpath2);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        free(hostpath1);
        free(wasmpath1);
        free(hostpath2);
        free(wasmpath2);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_filestat_get(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        struct stat hst;
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) != 0) {
                ret = stat(hostpath, &hst);
        } else {
                ret = lstat(hostpath, &hst);
        }
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        struct wasi_filestat wst;
        memset(&wst, 0, sizeof(wst));
        wst.dev = host_to_le64(hst.st_dev);
        wst.ino = host_to_le64(hst.st_ino);
        wst.type = wasi_convert_filetype(hst.st_mode);
        wst.linkcount = host_to_le64(hst.st_nlink);
        wst.size = host_to_le64(hst.st_size);
#if defined(__APPLE__)
        wst.atim = host_to_le64(timespec_to_ns(&hst.st_atimespec));
        wst.mtim = host_to_le64(timespec_to_ns(&hst.st_mtimespec));
        wst.ctim = host_to_le64(timespec_to_ns(&hst.st_ctimespec));
#else
        wst.atim = host_to_le64(timespec_to_ns(&hst.st_atim));
        wst.mtim = host_to_le64(timespec_to_ns(&hst.st_mtim));
        wst.ctim = host_to_le64(timespec_to_ns(&hst.st_ctim));
#endif
        ret = wasi_copyout(ctx, &wst, retp, sizeof(wst));
fail:
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

static int
wasi_path_filestat_set_times(struct exec_context *ctx,
                             struct host_instance *hi,
                             const struct functype *ft,
                             const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint64_t atim = HOST_FUNC_PARAM(ft, params, 4, i64);
        uint64_t mtim = HOST_FUNC_PARAM(ft, params, 5, i64);
        uint32_t fstflags = HOST_FUNC_PARAM(ft, params, 7, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int ret;
        ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path, pathlen,
                                           &wasmpath, &hostpath);
        if (ret != 0) {
                goto fail;
        }
        struct timeval tv[2];
        const struct timeval *tvp;
        ret = prepare_utimes_tv(fstflags, atim, mtim, tv, &tvp);
        if (ret != 0) {
                goto fail;
        }
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) != 0) {
                ret = utimes(hostpath, tvp);
        } else {
                ret = lutimes(hostpath, tvp);
        }
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        free(hostpath);
        free(wasmpath);
        return 0;
}

static int
wasi_sched_yield(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        int ret = 0;
        /* no-op */
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}

#define WASI_HOST_FUNC(NAME, TYPE)                                            \
        {                                                                     \
                .name = NAME_FROM_CSTR_LITERAL(#NAME), .type = TYPE,          \
                .func = wasi_##NAME,                                          \
        }

const struct host_func wasi_funcs[] = {
        WASI_HOST_FUNC(proc_exit, "(i)"),
        WASI_HOST_FUNC(fd_advise, "(iIIi)i"),
        WASI_HOST_FUNC(fd_allocate, "(iII)i"),
        WASI_HOST_FUNC(fd_filestat_set_size, "(iI)i"),
        WASI_HOST_FUNC(fd_close, "(i)i"),
        WASI_HOST_FUNC(fd_write, "(iiii)i"),
        WASI_HOST_FUNC(fd_pwrite, "(iiiIi)i"),
        WASI_HOST_FUNC(fd_read, "(iiii)i"),
        WASI_HOST_FUNC(fd_pread, "(iiiIi)i"),
        WASI_HOST_FUNC(fd_readdir, "(iiiIi)i"),
        WASI_HOST_FUNC(fd_fdstat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_fdstat_set_flags, "(ii)i"),
        WASI_HOST_FUNC(fd_fdstat_set_rights, "(iII)i"),
        WASI_HOST_FUNC(fd_seek, "(iIii)i"),
        WASI_HOST_FUNC(fd_tell, "(ii)i"),
        WASI_HOST_FUNC(fd_renumber, "(ii)i"),
        WASI_HOST_FUNC(fd_filestat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_filestat_set_times, "(iIIi)i"),
        WASI_HOST_FUNC(fd_prestat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_prestat_dir_name, "(iii)i"),
        WASI_HOST_FUNC(poll_oneoff, "(iiii)i"),
        WASI_HOST_FUNC(clock_time_get, "(iIi)i"),
        WASI_HOST_FUNC(clock_res_get, "(ii)i"),
        WASI_HOST_FUNC(args_sizes_get, "(ii)i"),
        WASI_HOST_FUNC(args_get, "(ii)i"),
        WASI_HOST_FUNC(environ_sizes_get, "(ii)i"),
        WASI_HOST_FUNC(environ_get, "(ii)i"),
        WASI_HOST_FUNC(random_get, "(ii)i"),
        WASI_HOST_FUNC(path_open, "(iiiiiIIii)i"),
        WASI_HOST_FUNC(path_unlink_file, "(iii)i"),
        WASI_HOST_FUNC(path_create_directory, "(iii)i"),
        WASI_HOST_FUNC(path_remove_directory, "(iii)i"),
        WASI_HOST_FUNC(path_symlink, "(iiiii)i"),
        WASI_HOST_FUNC(path_readlink, "(iiiiii)i"),
        WASI_HOST_FUNC(path_link, "(iiiiiii)i"),
        WASI_HOST_FUNC(path_rename, "(iiiiii)i"),
        WASI_HOST_FUNC(path_filestat_get, "(iiiii)i"),
        WASI_HOST_FUNC(path_filestat_set_times, "(iiiiIIi)i"),
        WASI_HOST_FUNC(sched_yield, "()i"),
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
        uint32_t nfds = 3;
        int ret = VEC_RESIZE(inst->fdtable, 3);
        if (ret != 0) {
                free(inst);
                return ret;
        }
        uint32_t i;
        for (i = 0; i < nfds; i++) {
                int hostfd;
#if defined(__wasi__)
                hostfd = i;
#else
                hostfd = dup(i);
#endif
                VEC_ELEM(inst->fdtable, i).hostfd = hostfd;
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

int
wasi_instance_prestat_add(struct wasi_instance *wasi, const char *path)
{
        uint32_t wasifd;
        int ret;
        xlog_trace("prestat adding %s", path);
        ret = wasi_fd_alloc(wasi, &wasifd);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo *fdinfo = &VEC_ELEM(wasi->fdtable, wasifd);
        fdinfo->prestat_path = strdup(path);
        if (fdinfo->prestat_path == NULL) {
                ret = errno;
                assert(ret != 0);
                xlog_trace("realpath failed with %d", ret);
                return ret;
        }
        xlog_trace("prestat added %s (%s)", path, fdinfo->prestat_path);
        return 0;
}

void
wasi_instance_destroy(struct wasi_instance *inst)
{
        struct wasi_fdinfo *it;
        uint32_t i;
        VEC_FOREACH_IDX(i, it, inst->fdtable) {
                int hostfd = it->hostfd;
#if defined(__wasi__)
                if (hostfd != -1 && hostfd >= 3) {
#else
                if (hostfd != -1) {
#endif
                        int ret = close(hostfd);
                        if (ret != 0) {
                                xlog_trace("failed to close: wasm fd %" PRIu32
                                           " host fd %u with errno %d",
                                           i, hostfd, errno);
                        }
                }
                free(it->prestat_path);
        }
        VEC_FREE(inst->fdtable);
        free(inst);
}

struct name wasi_snapshot_preview1 =
        NAME_FROM_CSTR_LITERAL("wasi_snapshot_preview1");

int
import_object_create_for_wasi(struct wasi_instance *wasi,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                &wasi_snapshot_preview1, wasi_funcs, ARRAYCOUNT(wasi_funcs),
                &wasi->hi, impp);
}
