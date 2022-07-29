#define _POSIX_C_SOURCE 199309 /* clock_gettime */
#define _DARWIN_C_SOURCE       /* arc4random_buf */
#define _GNU_SOURCE            /* asprintf, realpath, O_DIRECTORY */

#include <sys/random.h> /* getrandom */
#include <sys/stat.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
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
#include "xlog.h"

struct wasi_fdinfo {
        char *prestat_path;
        int hostfd;
};

struct wasi_instance {
        struct host_instance hi;
        VEC(, struct wasi_fdinfo) fdtable; /* indexed by wasm fd */
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

#define WASI_OFLAG_CREAT 1
#define WASI_OFLAG_DIRECTORY 2
#define WASI_OFLAG_EXCL 4
#define WASI_OFLAG_TRUNC 8

#define WASI_RIGHT_FD_READ 2
#define WASI_RIGHT_FD_WRITE 4

struct wasi_fdstat {
        uint8_t fs_filetype;
        uint8_t pad1;
        uint16_t fs_flags;
        uint8_t pad2[4];
        uint64_t fs_rights_base;
        uint64_t fs_rights_inheriting;
};
_Static_assert(sizeof(struct wasi_fdstat) == 24, "wasi_fdstat");

struct wasi_fd_prestat {
        uint8_t type;
        uint8_t pad[3];
        uint32_t dir_name_len;
};
_Static_assert(sizeof(struct wasi_fd_prestat) == 8, "wasi_fd_prestat");

#define WASI_PREOPEN_TYPE_DIR 0

struct wasi_filestat {
        uint64_t dev;
        uint64_t ino;
        uint8_t type;
        uint8_t pad[7];
        uint64_t linkcount;
        uint64_t size;
        uint64_t atim;
        uint64_t mtim;
        uint64_t ctim;
};
_Static_assert(sizeof(struct wasi_filestat) == 64, "wasi_filestat");

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
wasi_fd_alloc(struct wasi_instance *wasi, uint32_t *wasifdp)
{
        struct wasi_fdinfo *fdinfo;
        uint32_t wasifd;
        VEC_FOREACH_IDX(wasifd, fdinfo, wasi->fdtable)
        {
                if (fdinfo->hostfd == -1 && fdinfo->prestat_path == NULL) {
                        *wasifdp = wasifd;
                        return 0;
                }
        }
        wasifd = wasi->fdtable.lsize;
        int ret = VEC_RESIZE(wasi->fdtable, wasifd + 1);
        if (ret != 0) {
                return ret;
        }
        fdinfo = &VEC_ELEM(wasi->fdtable, wasifd);
        fdinfo->hostfd = -1;
        fdinfo->prestat_path = NULL;
        *wasifdp = wasifd;
        return 0;
}

static int
wasi_fd_add(struct wasi_instance *wasi, int hostfd, uint32_t *wasifdp)
{
        uint32_t wasifd;
        int ret;
        ret = wasi_fd_alloc(wasi, &wasifd);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo *fdinfo = &VEC_ELEM(wasi->fdtable, wasifd);
        fdinfo->hostfd = hostfd;
        *wasifdp = wasifd;
        return 0;
}

static uint64_t
timespec_to_ns(const struct timespec *ts)
{
        return (uint64_t)ts->tv_sec * 1000000000 + ts->tv_nsec;
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

static uint32_t
wasi_convert_errno(int host_errno)
{
        /* TODO implement */
        uint32_t wasmerrno;
        switch (host_errno) {
        case 0:
                wasmerrno = 0;
                break;
        case EBADF:
                wasmerrno = 8;
                break;
        case EINVAL:
                wasmerrno = 28;
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
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd);
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
        st.fs_filetype = wasi_convert_filetype(stat.st_mode);
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
                goto fail;
        }
        uint64_t result = host_to_le64(ret1);
        ret = wasi_copyout(ctx, &result, retp, sizeof(result));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_fd_filestat_get(struct exec_context *ctx, struct host_instance *hi,
                     const struct functype *ft, const struct val *params,
                     struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
        uint32_t retp = params[1].u.i32;
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
                assert(ret != 0);
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
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_fd_prestat_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct val *params,
                    struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        uint32_t retp = params[1].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
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
        st.dir_name_len = strlen(fdinfo->prestat_path);
        ret = wasi_copyout(ctx, &st, retp, sizeof(st));
fail:
        results[0].u.i32 = wasi_convert_errno(ret);
        return 0;
}

static int
wasi_fd_prestat_dir_name(struct exec_context *ctx, struct host_instance *hi,
                         const struct functype *ft, const struct val *params,
                         struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t wasifd = params[0].u.i32;
        uint32_t path = params[1].u.i32;
        uint32_t pathlen = params[1].u.i32;
        xlog_trace("%s called for fd %" PRIu32, __func__, wasifd);
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
        if (strlen(fdinfo->prestat_path) != pathlen) {
                ret = EINVAL;
                goto fail;
        }
        ret = wasi_copyout(ctx, fdinfo->prestat_path, path, pathlen);
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
        uint64_t result = host_to_le64(timespec_to_ns(&ts));
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

static int
wasi_path_open(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct val *params,
               struct val *results)
{
        struct wasi_instance *wasi = (void *)hi;
        uint32_t dirwasifd = params[0].u.i32;
        xlog_trace("%s called", __func__);
#if 0
        uint32_t dirflags = params[1].u.i32;
#endif
        uint32_t path = params[2].u.i32;
        uint32_t pathlen = params[3].u.i32;
        uint32_t wasmoflags = params[4].u.i32;
        uint64_t rights_base = params[5].u.i64;
#if 0
        uint64_t rights_inherit = params[6].u.i64;
#endif
        uint32_t fdflags = params[7].u.i32;
        uint32_t retp = params[8].u.i32;
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int hostfd = -1;
        int ret;
        int oflags = 0;
        if ((wasmoflags & 1) != 0) {
                oflags |= O_CREAT;
        }
        if ((wasmoflags & 2) != 0) {
                oflags |= O_DIRECTORY;
        }
        if ((wasmoflags & 4) != 0) {
                oflags |= O_EXCL;
        }
        if ((wasmoflags & 8) != 0) {
                oflags |= O_TRUNC;
        }
        if ((fdflags & WASI_FDFLAG_APPEND) != 0) {
                oflags |= O_APPEND;
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
                assert(ret != 0);
                goto fail;
        }
        ret = open(hostpath, oflags);
        if (ret != 0) {
                ret = errno;
                assert(ret != 0);
                goto fail;
        }
        hostfd = ret;
        uint32_t wasifd;
        ret = wasi_fd_add(wasi, hostfd, &wasifd);
        if (ret != 0) {
                goto fail;
        }
        hostfd = -1;
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
        WASI_HOST_FUNC(fd_filestat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_prestat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_prestat_dir_name, "(iii)i"),
        WASI_HOST_FUNC(clock_time_get, "(iIi)i"),
        WASI_HOST_FUNC(args_sizes_get, "(ii)i"),
        WASI_HOST_FUNC(args_get, "(ii)i"),
        WASI_HOST_FUNC(environ_sizes_get, "(ii)i"),
        WASI_HOST_FUNC(environ_get, "(ii)i"),
        WASI_HOST_FUNC(random_get, "(ii)i"),
        WASI_HOST_FUNC(path_open, "(iiiiIIii)i"),
        WASI_HOST_FUNC(path_open, "(iiiiiIIii)i"),
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
#if defined(__wasi__)
        fdinfo->prestat_path = strdup(path);
#else
        fdinfo->prestat_path = realpath(path, NULL);
#endif
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
        VEC_FOREACH_IDX(i, it, inst->fdtable)
        {
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

int
import_object_create_for_wasi(struct wasi_instance *wasi,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                "wasi_snapshot_preview1", wasi_funcs, ARRAYCOUNT(wasi_funcs),
                &wasi->hi, impp);
}
