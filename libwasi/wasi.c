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
 * References:
 * https://github.com/WebAssembly/WASI/tree/main/phases/snapshot/witx
 * https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/headers/public/wasi/api.h
 */

#define _DARWIN_C_SOURCE /* arc4random_buf */
#define _GNU_SOURCE      /* asprintf, realpath, O_DIRECTORY */
#define _NETBSD_SOURCE   /* asprintf, DT_REG, etc */

#if defined(__wasi__) && (!defined(__clang_major__) || __clang_major__ < 17)
/*
 * wasi-libc bug workaround.
 * https://github.com/WebAssembly/wasi-libc/pull/375
 *
 * LLVM 17 complains when you undefine builtin macros like __STDC_VERSION__.
 * (-Wbuiltin-macro-redefined)
 * if you are using LLVM 17, you probably are using a recent enough version
 * of wasi-libc which doesn't require the workaround.
 */
#undef __STDC_VERSION__
#define __STDC_VERSION__ 201112L
#endif

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

#include <sys/socket.h>
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

#if defined(__GLIBC__) || defined(__NuttX__)
#include <sys/random.h> /* getrandom */
#endif

#include "endian.h"
#include "exec.h"
#include "lock.h"
#include "nbio.h"
#include "restart.h"
#include "timeutil.h"
#include "type.h"
#include "util.h"
#include "vec.h"
#include "wasi.h"
#include "wasi_impl.h"
#include "xlog.h"

#if defined(__wasi__)
#if !defined(AT_FDCWD)
/* a workaroud for wasi-sdk-8.0 which we use for wapm */
#define TOYWASM_OLD_WASI_LIBC
#endif

/*
 * For some reasons, wasi-libc doesn't have legacy stuff enabled.
 * It includes lutimes and futimes.
 */

static int
lutimes(const char *path, const struct timeval *tvp)
{
#if defined(TOYWASM_OLD_WASI_LIBC)
        errno = ENOSYS;
        return -1;
#else
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
#endif
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

#if defined(__NuttX__) && !defined(CONFIG_PSEUDOFS_SOFTLINKS)
int
symlink(const char *path1, const char *path2)
{
        errno = ENOSYS;
        return -1;
}

ssize_t
readlink(const char *path, char *buf, size_t buflen)
{
        errno = ENOSYS;
        return -1;
}
#endif

static int
wasi_copyin_iovec(struct exec_context *ctx, uint32_t iov_uaddr,
                  uint32_t iov_count, struct iovec **resultp, int *usererrorp)
{
        struct iovec *hostiov = NULL;
        void *p;
        int host_ret = 0;
        int ret = 0;
        if (iov_count == 0) {
                ret = EINVAL;
                goto fail;
        }
        hostiov = calloc(iov_count, sizeof(*hostiov));
        if (hostiov == NULL) {
                ret = ENOMEM;
                goto fail;
        }
retry:
        host_ret = host_func_check_align(ctx, iov_uaddr, WASI_IOV_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        host_ret = memory_getptr(ctx, 0, iov_uaddr, 0,
                                 iov_count * sizeof(struct wasi_iov), &p);
        if (host_ret != 0) {
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
                host_ret = memory_getptr2(ctx, 0, iov_base, 0, iov_len, &p,
                                          &moved);
                if (host_ret != 0) {
                        goto fail;
                }
                if (moved) {
                        goto retry;
                }
                hostiov[i].iov_base = p;
                hostiov[i].iov_len = iov_len;
        }
        *resultp = hostiov;
        *usererrorp = 0;
        return 0;
fail:
        free(hostiov);
        *usererrorp = ret;
        return host_ret;
}

int
wasi_fdinfo_close(struct wasi_fdinfo *fdinfo)
{
        int ret = 0;
        int hostfd = fdinfo->hostfd;
#if defined(__wasi__) /* wasi has no dup */
        if (hostfd != -1 && hostfd >= 3) {
#else
        if (hostfd != -1) {
#endif
                ret = close(fdinfo->hostfd);
                if (ret != 0) {
                        ret = errno;
                        assert(ret > 0);
                }
        }
        fdinfo->hostfd = -1;
        if (fdinfo->dir != NULL) {
                closedir(fdinfo->dir);
        }
        fdinfo->dir = NULL;
        free(fdinfo->prestat_path);
        free(fdinfo->wasm_path);
        fdinfo->prestat_path = NULL;
        fdinfo->wasm_path = NULL;
        return ret;
}

static uint64_t
timespec_to_ns(const struct timespec *ts)
{
        /*
         * While this might overflow, we don't care much because
         * it's the limitation of WASI itself.
         *
         * Note: WASI timestamps are uint64_t, which wraps at
         * the year 2554.
         */
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

static void
wasi_convert_filestat(const struct stat *hst, struct wasi_filestat *wst)
{
        memset(wst, 0, sizeof(*wst));
        wst->dev = host_to_le64(hst->st_dev);
        wst->ino = host_to_le64(hst->st_ino);
        wst->type = wasi_convert_filetype(hst->st_mode);
        wst->linkcount = host_to_le64(hst->st_nlink);
        wst->size = host_to_le64(hst->st_size);
#if defined(__APPLE__)
        wst->atim = host_to_le64(timespec_to_ns(&hst->st_atimespec));
        wst->mtim = host_to_le64(timespec_to_ns(&hst->st_mtimespec));
        wst->ctim = host_to_le64(timespec_to_ns(&hst->st_ctimespec));
#else
        wst->atim = host_to_le64(timespec_to_ns(&hst->st_atim));
        wst->mtim = host_to_le64(timespec_to_ns(&hst->st_mtim));
        wst->ctim = host_to_le64(timespec_to_ns(&hst->st_ctim));
#endif
}

static void
wasi_unstable_convert_filestat(const struct stat *hst,
                               struct wasi_unstable_filestat *wst)
{
        memset(wst, 0, sizeof(*wst));
        wst->dev = host_to_le64(hst->st_dev);
        wst->ino = host_to_le64(hst->st_ino);
        wst->type = wasi_convert_filetype(hst->st_mode);
        wst->linkcount = host_to_le32(hst->st_nlink);
        wst->size = host_to_le64(hst->st_size);
#if defined(__APPLE__)
        wst->atim = host_to_le64(timespec_to_ns(&hst->st_atimespec));
        wst->mtim = host_to_le64(timespec_to_ns(&hst->st_mtimespec));
        wst->ctim = host_to_le64(timespec_to_ns(&hst->st_ctimespec));
#else
        wst->atim = host_to_le64(timespec_to_ns(&hst->st_atim));
        wst->mtim = host_to_le64(timespec_to_ns(&hst->st_mtim));
        wst->ctim = host_to_le64(timespec_to_ns(&hst->st_ctim));
#endif
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
                             char **hostpathp, int *usererrorp)
{
        /*
         * TODO: somehow prevent it from escaping the dirwasifd directory.
         *
         * eg. reject too many ".."s, check symlinks, etc
         *
         * probably non-racy implementation is impossible w/o modern
         * interfaces like openat, O_DIRECTORY, O_NOFOLLOW.
         */
        char *hostpath = NULL;
        char *wasmpath = NULL;
        struct wasi_fdinfo *dirfdinfo = NULL;
        int host_ret = 0;
        int ret = 0;
        wasmpath = malloc(pathlen + 1);
        if (wasmpath == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        host_ret = wasi_copyin(ctx, wasmpath, path, pathlen, 1);
        if (host_ret != 0) {
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
        ret = wasi_fd_lookup(wasi, dirwasifd, &dirfdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (dirfdinfo->prestat_path == NULL) {
                ret = ENOTDIR;
                goto fail;
        }
        ret = asprintf(&hostpath, "%s/%s", dirfdinfo->prestat_path, wasmpath);
        if (ret < 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        wasi_fdinfo_release(wasi, dirfdinfo);
        xlog_trace("%s: wasifd %d wasmpath %s hostpath %s", __func__,
                   dirwasifd, wasmpath, hostpath);
        *hostpathp = hostpath;
        *wasmpathp = wasmpath;
        *usererrorp = 0;
        return 0;
fail:
        wasi_fdinfo_release(wasi, dirfdinfo);
        free(hostpath);
        free(wasmpath);
        *usererrorp = ret;
        return host_ret;
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
#if defined(CLOCK_PROCESS_CPUTIME_ID)
        case WASI_CLOCK_ID_PROCESS_CPUTIME_ID:
                /* REVISIT what does this really mean for wasm? */
                hostclockid = CLOCK_PROCESS_CPUTIME_ID;
                break;
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
        case WASI_CLOCK_ID_THREAD_CPUTIME_ID:
                /* REVISIT what does this really mean for wasm? */
                hostclockid = CLOCK_THREAD_CPUTIME_ID;
                break;
#endif
        default:
                ret = EINVAL;
                goto fail;
        }
        *hostidp = hostclockid;
        return 0;
fail:
        return ret;
}

uint32_t
wasi_convert_errno(int host_errno)
{
        /* TODO implement */
        uint32_t wasmerrno;
        assert(host_errno >= 0); /* ETOYWASMxxx shouldn't be here */
        switch (host_errno) {
        case 0:
                wasmerrno = 0;
                break;
        case EACCES:
                wasmerrno = 2;
                break;
        /*
         * Note: in WASI EWOULDBLOCK == EAGAIN.
         * We don't assume EWOULDBLOCK == EAGAIN for the host.
         */
        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
                wasmerrno = 6;
                break;
        case EBADF:
                wasmerrno = 8;
                break;
        case ECONNRESET:
                wasmerrno = 15;
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
        case EIO:
                wasmerrno = 29;
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
        case ENOMEM:
                wasmerrno = 48;
                break;
        case ENOTDIR:
                wasmerrno = 54;
                break;
        case ENOTEMPTY:
                wasmerrno = 55;
                break;
        case ENOTSOCK:
                wasmerrno = 57;
                break;
        case ENOTSUP:
                wasmerrno = 58;
                break;
        case EPERM:
                wasmerrno = 63;
                break;
        case EPIPE:
                wasmerrno = 64;
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
                xlog_error("Converting unimplemented errno: %u", host_errno);
                wasmerrno = 29; /* EIO */
        }
        xlog_trace("error converted from %u to %" PRIu32, host_errno,
                   wasmerrno);
        return wasmerrno;
}

static int
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

static int
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

static bool
emulate_blocking(struct exec_context *ctx, struct wasi_fdinfo *fdinfo,
                 short poll_event, int orig_ret, int *host_retp, int *retp)
{
        int hostfd = fdinfo->hostfd;
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

static int
wasi_proc_exit(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t code = HOST_FUNC_PARAM(ft, params, 0, i32);
        /*
         * Note: While our embedder API (wasi_instance_exit_code) has
         * no problem to propagate a full 32-bit value, toywasm cli
         * can't represent full range with its exit code, especially
         * when it's running on a wasm runtime with restricted wasi
         * exit code.
         *
         * Note: wasmtime traps for exit code >= 126.
         *
         * Note: in preview1, only 0 has a defined meaning.
         * https://github.com/WebAssembly/WASI/blob/ddfe3d1dda5d1473f37ecebc552ae20ce5fd319a/legacy/preview1/witx/wasi_snapshot_preview1.witx#L460-L462
         * > An exit code of 0 indicates successful termination of
         * > the program. The meanings of other values is dependent on
         * > the environment.
         *
         * Note: A change to restrict wasi exit code to [0,126) has been
         * made to ephemeral. But it has never been a part of preview1.
         * https://github.com/WebAssembly/WASI/pull/235
         * https://github.com/WebAssembly/WASI/pull/510
         */
        toywasm_mutex_lock(&wasi->lock);
        wasi->exit_code = code;
        toywasm_mutex_unlock(&wasi->lock);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        struct wasi_fdinfo *fdinfo;
        int hostfd;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        fdinfo = NULL;
#endif
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = 0;
        /* no-op */
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        struct wasi_fdinfo *fdinfo;
        int hostfd;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        fdinfo = NULL;
#endif
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        struct wasi_fdinfo *fdinfo;
        int hostfd;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        fdinfo = NULL;
#endif
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;

        /*
         * we simply make fd_close block until other threads finish working
         * on the descriptor.
         *
         * note that we can't assume that the other threads will finish on
         * the descriptor soon. actually they might keep using the
         * descriptor very long or even forever. (poll, socket read, ...)
         *
         * when i have done some experiments with native pthreads
         * decades ago, the behavior on concurrent close() was not
         * very consistent among platforms. (Solaris/BSDs/Linux...)
         * some of them block close() as we do here.
         * others "interrupt" and make other non-close users fail.
         *
         * i guess portable applications should not rely on either
         * behaviors.
         */

        toywasm_mutex_lock(&wasi->lock);
        host_ret = wasi_fd_lookup_locked_for_close(ctx, wasi, wasifd, &fdinfo,
                                                   &ret);
        if (host_ret != 0 || ret != 0) {
                toywasm_mutex_unlock(&wasi->lock);
                goto fail;
        }
        if (wasi_fdinfo_unused(fdinfo)) {
                toywasm_mutex_unlock(&wasi->lock);
                ret = EBADF;
                goto fail;
        }

        assert(fdinfo->refcount == 2);
        fdinfo->refcount--;
        assert(VEC_ELEM(wasi->fdtable, wasifd) == fdinfo);
        VEC_ELEM(wasi->fdtable, wasifd) = NULL;
        toywasm_mutex_unlock(&wasi->lock);

        ret = wasi_fdinfo_close(fdinfo);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
retry:
        n = writev(hostfd, hostiov, iov_count);
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLOUT, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
retry:
        n = pwritev(hostfd, hostiov, iov_count, offset);
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLOUT, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;

        /* hack for tty. see the comment in wasi_instance_create. */
        if ((fcntl(hostfd, F_GETFL, 0) & O_NONBLOCK) == 0) {
                ret = EAGAIN;
                goto tty_hack;
        }
retry:
        n = readv(hostfd, hostiov, iov_count);
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
tty_hack:
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
retry:
        n = preadv(hostfd, hostiov, iov_count, offset);
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        DIR *dir = fdinfo->dir;
        if (dir == NULL) {
                xlog_trace("fd_readdir: fdopendir");
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
                xlog_trace("fd_readdir: rewinddir");
                rewinddir(dir);
        } else if (cookie > LONG_MAX) {
                ret = EINVAL;
                goto fail;
        } else {
                xlog_trace("fd_readdir: seekdir %" PRIu64, cookie);
                seekdir(dir, cookie);
        }
        uint32_t n = 0;
        while (true) {
                struct dirent *d;
                errno = 0;
                d = readdir(dir);
                if (d == NULL) {
                        if (errno != 0) {
                                ret = errno;
                                xlog_trace(
                                        "fd_readdir: readdir failed with %d",
                                        ret);
                                goto fail;
                        }
                        xlog_trace("fd_readdir: EOD");
                        break;
                }
                long nextloc = telldir(dir);
                struct wasi_dirent wde;
                memset(&wde, 0, sizeof(wde));
                le64_encode(&wde.d_next, nextloc);
#if defined(__NuttX__)
                /* NuttX doesn't have d_ino */
                wde.d_ino = 0;
#else
                le64_encode(&wde.d_ino, d->d_ino);
#endif
                uint32_t namlen = strlen(d->d_name);
                le32_encode(&wde.d_namlen, namlen);
                wde.d_type = wasi_convert_dirent_filetype(d->d_type);
                xlog_trace("fd_readdir: ino %" PRIu64 " nam %.*s",
                           (uint64_t)d->d_ino, (int)namlen, d->d_name);
                if (buflen - n < sizeof(wde)) {
                        xlog_trace("fd_readdir: buffer full");
                        n = buflen; /* signal buffer full */
                        break;
                }
                /* XXX is it ok to return unaligned structure? */
                host_ret = wasi_copyout(ctx, &wde, buf, sizeof(wde), 1);
                if (host_ret != 0) {
                        goto fail;
                }
                buf += sizeof(wde);
                n += sizeof(wde);
                if (buflen - n < namlen) {
                        xlog_trace("fd_readdir: buffer full");
                        n = buflen; /* signal buffer full */
                        break;
                }
                host_ret = wasi_copyout(ctx, d->d_name, buf, namlen, 1);
                if (host_ret != 0) {
                        goto fail;
                }
                buf += namlen;
                n += namlen;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_fdstat st;
        memset(&st, 0, sizeof(st));
        if (wasi_fdinfo_is_prestat(fdinfo)) {
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
                int flags = fcntl(hostfd, F_GETFL, 0);
                if (!fdinfo->blocking) {
                        st.fs_flags |= WASI_FDFLAG_NONBLOCK;
                }
                if ((flags & O_APPEND) != 0) {
                        st.fs_flags |= WASI_FDFLAG_APPEND;
                }
        }

        /*
         * for some reasons, old libc (eg. the one from wasi-sdk 8)
         * seems to perform ENOTCAPABLE checks for preopens by itself,
         * looking at fs_rights_base.
         *
         * Note: some code (eg. wasmtime wasi testsuite)
         * passes fs_rights_base from fd_fdstat_get to path_open.
         * our path_open uses WASI_RIGHT_FD_READ and WASI_RIGHT_FD_WRITE
         * to decide O_RDONLY/O_WRITEONLY/O_RDWR. the underlying os
         * and/or filesystem might reject them with EISDIR if it's
         * a directory.
         *
         * Note: In WASI, directories are not seekable.
         */
        const uint64_t seek_rights = WASI_RIGHT_FD_SEEK | WASI_RIGHT_FD_TELL;
        const uint64_t path_rights =
                WASI_RIGHT_PATH_CREATE_DIRECTORY |
                WASI_RIGHT_PATH_CREATE_FILE | WASI_RIGHT_PATH_LINK_SOURCE |
                WASI_RIGHT_PATH_LINK_TARGET | WASI_RIGHT_PATH_OPEN |
                WASI_RIGHT_PATH_READLINK | WASI_RIGHT_PATH_RENAME_SOURCE |
                WASI_RIGHT_PATH_RENAME_TARGET | WASI_RIGHT_PATH_FILESTAT_GET |
                WASI_RIGHT_PATH_FILESTAT_SET_SIZE |
                WASI_RIGHT_PATH_FILESTAT_SET_TIMES | WASI_RIGHT_PATH_SYMLINK |
                WASI_RIGHT_PATH_REMOVE_DIRECTORY | WASI_RIGHT_PATH_UNLINK_FILE;
        const uint64_t sock_rights =
                WASI_RIGHT_SOCK_SHUTDOWN | WASI_RIGHT_SOCK_ACCEPT;
        const uint64_t regfile_rights = WASI_RIGHT_FD_READ |
                                        WASI_RIGHT_FD_WRITE |
                                        WASI_RIGHT_FD_FILESTAT_SET_SIZE;
        uint64_t rights = ~UINT64_C(0);
        switch (st.fs_filetype) {
        case WASI_FILETYPE_DIRECTORY:
                rights = ~(regfile_rights | seek_rights | sock_rights);
                break;
        case WASI_FILETYPE_CHARACTER_DEVICE:
                /*
                 * Note: SEEK/TELL bits are important because
                 * wasi-libc isatty() checks them.
                 */
                rights = ~(path_rights | seek_rights | sock_rights);
                break;
        case WASI_FILETYPE_REGULAR_FILE:
                rights = ~(path_rights | sock_rights);
                break;
        }
        st.fs_rights_base = host_to_le64(rights);

        /*
         * A hack to make wasm-on-wasm happier.
         *
         * wasi-libc clamps the request by itself using rights_inheriting.
         * https://github.com/WebAssembly/wasi-libc/blob/9d2f5a8242667ac659793b19163cbeec1e077e01/libc-bottom-half/cloudlibc/src/libc/fcntl/openat.c#L53-L69
         */
        st.fs_rights_inheriting = ~UINT64_C(0);

        host_ret = wasi_copyout(ctx, &st, stat_addr, sizeof(st),
                                WASI_FDSTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        if ((fdflags & ~WASI_FDFLAG_NONBLOCK) != 0) {
                ret = ENOTSUP;
                goto fail;
        }
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        fdinfo->blocking = ((fdflags & WASI_FDFLAG_NONBLOCK) == 0);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        /* TODO implement */
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        int64_t offset = (int64_t)HOST_FUNC_PARAM(ft, params, 1, i64);
        uint32_t whence = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        struct stat st;
        ret = fstat(hostfd, &st);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (S_ISDIR(st.st_mode)) {
                /*
                 * Note: wasmtime directory_seek.rs test expects EBADF.
                 * Why not EISDIR?
                 */
                ret = EBADF;
                goto fail;
        }
        off_t ret1 = lseek(hostfd, offset, hostwhence);
        if (ret1 == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(ret1);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_unstable_fd_seek(struct exec_context *ctx, struct host_instance *hi,
                      const struct functype *ft, const struct cell *params,
                      struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        int64_t offset = (int64_t)HOST_FUNC_PARAM(ft, params, 1, i64);
        uint32_t whence = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        int hostwhence;
        switch (whence) {
        case 0:
                hostwhence = SEEK_CUR;
                break;
        case 1:
                hostwhence = SEEK_END;
                break;
        case 2:
                hostwhence = SEEK_SET;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        struct stat st;
        ret = fstat(hostfd, &st);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (S_ISDIR(st.st_mode)) {
                /*
                 * Note: wasmtime directory_seek.rs test expects EBADF.
                 * Why not EISDIR?
                 */
                ret = EBADF;
                goto fail;
        }
        off_t ret1 = lseek(hostfd, offset, hostwhence);
        if (ret1 == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(ret1);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_fd_sync(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = fsync(hostfd);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

#if defined(__APPLE__)
/* macOS doesn't have fdatasync */
#define wasi_fd_datasync wasi_fd_sync
#else
static int
wasi_fd_datasync(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = fdatasync(hostfd);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}
#endif

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
        struct wasi_fdinfo *fdinfo_from = NULL;
        struct wasi_fdinfo *fdinfo_to = NULL;
        int host_ret = 0;
        int ret;

        toywasm_mutex_lock(&wasi->lock);

        /* ensure the table size is big enough */
        ret = wasi_fdtable_expand(wasi, wasifd_to);
        if (ret != 0) {
                goto fail_locked;
        }

        /* Note: we check "to" first because it can involve a restart */

        /* check "to" */
        host_ret = wasi_fd_lookup_locked_for_close(ctx, wasi, wasifd_to,
                                                   &fdinfo_to, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail_locked;
        }
        /*
         * Note: unlike dup2, for some thread-safety reasons, fd_renumber
         * requires the "to" be an open descriptor.
         */
        if (wasi_fdinfo_unused(fdinfo_to)) {
                ret = EBADF;
                goto fail_locked;
        }

        /* check "from" */
        ret = wasi_fd_lookup_locked(wasi, wasifd_from, &fdinfo_from);
        if (ret != 0) {
                goto fail_locked;
        }
        if (!wasi_fdinfo_is_prestat(fdinfo_from) &&
            fdinfo_from->hostfd == -1) {
                ret = EBADF;
                goto fail_locked;
        }

        /* renumber */
        assert(fdinfo_to->refcount == 2);
        fdinfo_to->refcount--;
        VEC_ELEM(wasi->fdtable, wasifd_to) = fdinfo_from;
        VEC_ELEM(wasi->fdtable, wasifd_from) = NULL;

        toywasm_mutex_unlock(&wasi->lock);

        wasi_fdinfo_release(wasi, fdinfo_from);
        fdinfo_from = NULL;

        /* close the old "to" file */
        ret = wasi_fdinfo_close(fdinfo_to);
        if (ret != 0) {
                /* log and ignore */
                xlog_error("%s: closing to-fd failed with %d", __func__, ret);
                ret = 0;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo_from);
        wasi_fdinfo_release(wasi, fdinfo_to);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
fail_locked:
        toywasm_mutex_unlock(&wasi->lock);
        goto fail;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        wasi_convert_filestat(&hst, &wst);
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_FILESTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_unstable_fd_filestat_get(struct exec_context *ctx,
                              struct host_instance *hi,
                              const struct functype *ft,
                              const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        struct wasi_unstable_filestat wst;
        wasi_unstable_convert_filestat(&hst, &wst);
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_UNSTABLE_FILESTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int ret;
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
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
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
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
        int host_ret = 0;
        int ret;
        struct wasi_fdinfo *fdinfo = NULL;
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
        const char *prestat_path = fdinfo->prestat_path;
        if (fdinfo->wasm_path != NULL) {
                prestat_path = fdinfo->wasm_path;
        }
        st.dir_name_len = host_to_le32(strlen(prestat_path));
        host_ret =
                wasi_copyout(ctx, &st, retp, sizeof(st), WASI_PRESTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
        int ret;
        struct wasi_fdinfo *fdinfo = NULL;
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

        const char *prestat_path = fdinfo->prestat_path;
        if (fdinfo->wasm_path != NULL) {
                prestat_path = fdinfo->wasm_path;
        }
        size_t len = strlen(prestat_path);
        if (len > pathlen) {
                xlog_trace("path buffer too small %zu > %" PRIu32, len,
                           pathlen);
                ret = EINVAL;
                goto fail;
        }
        host_ret = wasi_copyout(ctx, prestat_path, path, len, 1);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
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
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
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
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
args_environ_sizes_get(struct exec_context *ctx, struct wasi_instance *wasi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results, int argc, const char *const *argv)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t argcp = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t argv_buf_sizep = HOST_FUNC_PARAM(ft, params, 1, i32);
        int host_ret;
        uint32_t argc_le32 = host_to_le32(argc);
        host_ret = wasi_copyout(ctx, &argc_le32, argcp, sizeof(argc_le32),
                                WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        int i;
        uint32_t argv_buf_size = 0;
        for (i = 0; i < argc; i++) {
                argv_buf_size += strlen(argv[i]) + 1;
        }
        argv_buf_size = host_to_le32(argv_buf_size);
        host_ret = wasi_copyout(ctx, &argv_buf_size, argv_buf_sizep,
                                sizeof(argv_buf_size), 1);
fail:
        if (host_ret == 0) {
                int ret = 0; /* never fail */
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
args_environ_get(struct exec_context *ctx, struct wasi_instance *wasi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results, int argc, const char *const *argv)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t argvp = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t argv_buf = HOST_FUNC_PARAM(ft, params, 1, i32);
        int host_ret = 0;
        int ret = 0;
        uint32_t i;
        uint32_t *wasm_argv = NULL;
        if (argc > 0) {
                wasm_argv = malloc(argc * sizeof(*wasm_argv));
                if (wasm_argv == NULL) {
                        ret = ENOMEM;
                        goto fail;
                }
        }
        uint32_t wasmp = argv_buf;
        for (i = 0; i < argc; i++) {
                le32_encode(&wasm_argv[i], wasmp);
                xlog_trace("wasm_argv[%" PRIu32 "] %" PRIx32, i, wasmp);
                wasmp += strlen(argv[i]) + 1;
        }
        for (i = 0; i < argc; i++) {
                size_t sz = strlen(argv[i]) + 1;
                host_ret = wasi_copyout(ctx, argv[i],
                                        le32_to_host(wasm_argv[i]), sz, 1);
                if (host_ret != 0) {
                        goto fail;
                }
        }
        host_ret = wasi_copyout(ctx, wasm_argv, argvp,
                                argc * sizeof(*wasm_argv), WASI_U32_ALIGN);
fail:
        free(wasm_argv);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_args_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_sizes_get(ctx, wasi, ft, params, results,
                                      wasi->argc, wasi->argv);
}

static int
wasi_args_get(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_get(ctx, wasi, ft, params, results, wasi->argc,
                                wasi->argv);
}

static int
wasi_environ_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_sizes_get(ctx, wasi, ft, params, results,
                                      wasi->nenvs, wasi->envs);
}

static int
wasi_environ_get(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_get(ctx, wasi, ft, params, results, wasi->nenvs,
                                wasi->envs);
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
        int ret = 0;
        void *p;
        int host_ret = memory_getptr(ctx, 0, buf, 0, buflen, &p);
        if (host_ret != 0) {
                goto fail;
        }
#if defined(__GLIBC__) || defined(__NuttX__)
        /*
         * glibc doesn't have arc4random
         * https://sourceware.org/bugzilla/show_bug.cgi?id=4417
         *
         * NuttX has both of getrandom and arc4random_buf.
         * The latter is available only if CONFIG_CRYPTO_RANDOM_POOL=y.
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
                p = (uint8_t *)p + ssz;
                buflen -= ssz;
        }
#else
        arc4random_buf(p, buflen);
        ret = 0;
#endif
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
        int ret = 0;
        int oflags = 0;
        xlog_trace("wasm oflags %" PRIx32 " rights_base %" PRIx64, wasmoflags,
                   rights_base);
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) == 0) {
#if defined(__NuttX__) && !defined(CONFIG_PSEUDOFS_SOFTLINKS)
                /*
                 * Ignore O_NOFOLLOW where the system doesn't
                 * support symlink at all.
                 */
                xlog_trace("Ignoring O_NOFOLLOW");
#elif defined(O_NOFOLLOW)
                oflags |= O_NOFOLLOW;
                xlog_trace("oflag O_NOFOLLOW");
#else
                ret = ENOTSUP;
                goto fail;
#endif
        }
        if ((wasmoflags & WASI_OFLAG_CREAT) != 0) {
                oflags |= O_CREAT;
                xlog_trace("oflag O_CREAT");
        }
        if ((wasmoflags & WASI_OFLAG_DIRECTORY) != 0) {
                oflags |= O_DIRECTORY;
                xlog_trace("oflag O_DIRECTORY");
        }
        if ((wasmoflags & WASI_OFLAG_EXCL) != 0) {
                oflags |= O_EXCL;
                xlog_trace("oflag O_EXCL");
        }
        if ((wasmoflags & WASI_OFLAG_TRUNC) != 0) {
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
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        xlog_trace("open %s oflags %x", hostpath, oflags);
        /*
         * TODO: avoid blocking on fifos for wasi-threads.
         */
        /*
         * Note: mode 0666 is what wasm-micro-runtime and wasmtime use.
         *
         * wasm-micro-runtime has it hardcoded:
         * https://github.com/bytecodealliance/wasm-micro-runtime/blob/cadf9d0ad36ec12e2a1cab4edf5f0dfb9bf84de0/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/src/posix.c#L1971
         *
         * wasmtime uses the default of the underlying library:
         * https://doc.rust-lang.org/nightly/std/os/unix/fs/trait.OpenOptionsExt.html#tymethod.mode
         */
        hostfd = open(hostpath, oflags | O_NONBLOCK, 0666);
        if (hostfd == -1) {
                ret = errno;
                assert(ret > 0);
                xlog_trace("open %s oflags %x failed with %d", hostpath,
                           oflags, errno);
                goto fail;
        }
        struct stat stat;
        ret = fstat(hostfd, &stat);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (!S_ISDIR(stat.st_mode)) {
                free(hostpath);
                hostpath = NULL;
        }
        uint32_t wasifd;
        ret = wasi_fd_add(wasi, hostfd, hostpath,
                          fdflags & WASI_FDFLAG_NONBLOCK, &wasifd);
        if (ret != 0) {
                goto fail;
        }
        hostpath = NULL; /* consumed by wasi_fd_add */
        hostfd = -1;
        xlog_trace("-> new wasi fd %" PRIu32, wasifd);
        uint32_t r = host_to_le32(wasifd);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        if (host_ret != 0) {
                /* XXX close wasifd? */
                goto fail;
        }
fail:
        if (hostfd != -1) {
                close(hostfd);
        }
        free(hostpath);
        free(wasmpath);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
        int ret = 0;
        target_buf = malloc(targetlen + 1);
        if (target_buf == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        host_ret = wasi_copyin(ctx, target_buf, target, targetlen, 1);
        if (host_ret != 0) {
                goto fail;
        }
        target_buf[targetlen] = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int host_ret = 0;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
         * For some reasons, wasmtime used to return ERANGE.
         * https://github.com/bytecodealliance/wasmtime/commit/222a57868e9c01baa838aa81e92a80451e2d920a
         * However, it has been fixed.
         * https://github.com/bytecodealliance/wasmtime/commit/24b607cf751930c51f2b6449cdfbf2e81dce1c31
         */
        void *p;
        host_ret = memory_getptr(ctx, 0, buf, 0, buflen, &p);
        if (host_ret != 0) {
                goto fail;
        }
        ssize_t ret1 = readlink(hostpath, p, buflen);
        if (ret1 == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint32_t result = le32_to_host(ret1);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
fail:
        free(hostpath);
        free(wasmpath);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
        int ret = 0;
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) == 0) {
#if 1
                ret = ENOTSUP;
                goto fail;
#else
                xlog_trace(
                        "path_link: Ignoring !WASI_LOOKUPFLAG_SYMLINK_FOLLOW");
#endif
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd1, path1,
                                                pathlen1, &wasmpath1,
                                                &hostpath1, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd2, path2,
                                                pathlen2, &wasmpath2,
                                                &hostpath2, &ret);
        if (host_ret != 0 || ret != 0) {
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
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd1, path1,
                                                pathlen1, &wasmpath1,
                                                &hostpath1, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd2, path2,
                                                pathlen2, &wasmpath2,
                                                &hostpath2, &ret);
        if (host_ret != 0 || ret != 0) {
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
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        int host_ret = 0;
        int ret;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
        wasi_convert_filestat(&hst, &wst);
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_FILESTAT_ALIGN);
fail:
        free(hostpath);
        free(wasmpath);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_unstable_path_filestat_get(struct exec_context *ctx,
                                struct host_instance *hi,
                                const struct functype *ft,
                                const struct cell *params,
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
        int host_ret = 0;
        int ret;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
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
        struct wasi_unstable_filestat wst;
        wasi_unstable_convert_filestat(&hst, &wst);
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_UNSTABLE_FILESTAT_ALIGN);
fail:
        free(hostpath);
        free(wasmpath);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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
        uint32_t fstflags = HOST_FUNC_PARAM(ft, params, 6, i32);
        char *hostpath = NULL;
        char *wasmpath = NULL;
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &wasmpath, &hostpath,
                                                &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        struct timeval tv[2];
        const struct timeval *tvp;
        ret = prepare_utimes_tv(fstflags, atim, mtim, tv, &tvp);
        if (ret != 0) {
                goto fail;
        }
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) != 0) {
#if defined(TOYWASM_OLD_WASI_LIBC)
                errno = ENOSYS;
                ret = -1;
#else
                ret = utimes(hostpath, tvp);
#endif
        } else {
                ret = lutimes(hostpath, tvp);
        }
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostpath);
        free(wasmpath);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
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

static int
wasi_sock_accept(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 2, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int hostchildfd = -1;
        int host_ret = 0;
        int ret;

        /*
         * non-zero fdflags is used by accept4.
         *
         * only WASI_FDFLAG_NONBLOCK makes sense for a socket.
         *
         * as wasi doesn't have close-on-exec, accept4 itself doesn't
         * make much sense. it merely saves an fcntl.
         * (for a threaded environment, atomicity of close-on-exec is
         * important to avoid descriptor leaks when other threads
         * performs exec. it's why accept4 has been invented in
         * the first place. other flags are not that important.
         * cf. https://www.austingroupbugs.net/view.php?id=411)
         */
        if ((fdflags & ~WASI_FDFLAG_NONBLOCK) != 0) {
                xlog_error("%s: unsupported fdflags %x", __func__, fdflags);
                ret = ENOTSUP;
                goto fail;
        }

        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct sockaddr_storage ss;
        struct sockaddr *sa = (void *)&ss;
        socklen_t salen;
retry:
#if defined(TOYWASM_OLD_WASI_LIBC)
        errno = ENOSYS;
        hostchildfd = -1;
#else
        hostchildfd = accept(hostfd, sa, &salen);
#endif
        if (hostchildfd < 0) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        /*
         * Ensure O_NONBLOCK of the new socket.
         * Note: O_NONBLOCK behavior is not consistent among platforms.
         * eg. BSD inherits O_NONBLOCK. Linux doesn't.
         */
        ret = set_nonblocking(hostchildfd, true, NULL);
        if (ret != 0) {
                goto fail;
        }
        uint32_t wasichildfd;
        ret = wasi_fd_add(wasi, hostchildfd, NULL,
                          fdflags & WASI_FDFLAG_NONBLOCK, &wasichildfd);
        if (ret != 0) {
                goto fail;
        }
        hostchildfd = -1;
        uint32_t r = host_to_le32(wasichildfd);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        if (host_ret != 0) {
                /* XXX close wasichildfd? */
                goto fail;
        }
fail:
        if (hostchildfd == -1) {
                close(hostchildfd);
        }
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_sock_recv(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t riflags = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t roflagsp = HOST_FUNC_PARAM(ft, params, 5, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        int flags = 0;
        if ((riflags & ~(WASI_RIFLAG_RECV_PEEK | WASI_RIFLAG_RECV_WAITALL)) !=
            0) {
                ret = EINVAL;
                goto fail;
        }
        if ((riflags & WASI_RIFLAG_RECV_PEEK) != 0) {
                flags |= MSG_PEEK;
        }
        if ((riflags & WASI_RIFLAG_RECV_WAITALL) != 0) {
                /*
                 * Note: it seems difficult (or impossible) to emulate
                 * blocking MSG_WAITALL behavior with a non-blocking
                 * underlying socket.
                 *
                 * Note: MSG_WAITALL behavior on a non-blocking socket
                 * varies among platforms.
                 * eg. BSD honors MSG_WAITALL. Linux ignores it.
                 */
                ret = ENOTSUP;
                goto fail;
        }
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
        struct msghdr msg;
retry:
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = hostiov;
        msg.msg_iovlen = iov_count;
#if defined(__wasi__)
        n = -1;
        errno = ENOSYS;
#else
        n = recvmsg(hostfd, &msg, flags);
#endif
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint16_t roflags = 0;
        /*
         * wasi-sdk <=19 had a broken MSG_TRUNC definition.
         * https://github.com/WebAssembly/wasi-libc/pull/391
         *
         * Note: older versions of wasi-sdk doesn't even have
         * __WASI_ROFLAGS_RECV_DATA_TRUNCATED.
         */
#if defined(__wasi__) && defined(MSG_TRUNC) &&                                \
        defined(__WASI_ROFLAGS_RECV_DATA_TRUNCATED)
#undef MSG_TRUNC
#define MSG_TRUNC __WASI_ROFLAGS_RECV_DATA_TRUNCATED
#endif /* defined(__wasi__) */
        if ((msg.msg_flags & MSG_TRUNC) != 0) {
                roflags = WASI_ROFLAG_RECV_DATA_TRUNCATED;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        uint16_t roflags_wasm = host_to_le16(roflags);
        host_ret = wasi_copyout(ctx, &roflags_wasm, roflagsp,
                                sizeof(roflags_wasm), WASI_U16_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_sock_send(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t siflags = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        if (siflags != 0) {
                ret = EINVAL;
                goto fail;
        }
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
        struct msghdr msg;
retry:
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = hostiov;
        msg.msg_iovlen = iov_count;
#if defined(__wasi__)
        n = -1;
        errno = ENOSYS;
#else
        n = sendmsg(hostfd, &msg, 0);
#endif
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLOUT, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                if (host_ret != 0 || ret != 0) {
                        goto fail;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
wasi_sock_shutdown(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t sdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int ret;
        int how = 0;
        switch (sdflags) {
        case WASI_SDFLAG_RD | WASI_SDFLAG_WR:
                how = SHUT_RDWR;
                break;
        case WASI_SDFLAG_RD:
                how = SHUT_RD;
                break;
        case WASI_SDFLAG_WR:
                how = SHUT_WR;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = shutdown(hostfd, how);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

const struct host_func wasi_funcs[] = {
        /* args */
        WASI_HOST_FUNC(args_get, "(ii)i"),
        WASI_HOST_FUNC(args_sizes_get, "(ii)i"),

        /* clock */
        WASI_HOST_FUNC(clock_res_get, "(ii)i"),
        WASI_HOST_FUNC(clock_time_get, "(iIi)i"),

        /* environ */
        WASI_HOST_FUNC(environ_get, "(ii)i"),
        WASI_HOST_FUNC(environ_sizes_get, "(ii)i"),

        /* fd */
        WASI_HOST_FUNC(fd_advise, "(iIIi)i"),
        WASI_HOST_FUNC(fd_allocate, "(iII)i"),
        WASI_HOST_FUNC(fd_close, "(i)i"),
        WASI_HOST_FUNC(fd_datasync, "(i)i"),
        WASI_HOST_FUNC(fd_fdstat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_fdstat_set_flags, "(ii)i"),
        WASI_HOST_FUNC(fd_fdstat_set_rights, "(iII)i"),
        WASI_HOST_FUNC(fd_filestat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_filestat_set_size, "(iI)i"),
        WASI_HOST_FUNC(fd_filestat_set_times, "(iIIi)i"),
        WASI_HOST_FUNC(fd_pread, "(iiiIi)i"),
        WASI_HOST_FUNC(fd_prestat_dir_name, "(iii)i"),
        WASI_HOST_FUNC(fd_prestat_get, "(ii)i"),
        WASI_HOST_FUNC(fd_pwrite, "(iiiIi)i"),
        WASI_HOST_FUNC(fd_read, "(iiii)i"),
        WASI_HOST_FUNC(fd_readdir, "(iiiIi)i"),
        WASI_HOST_FUNC(fd_renumber, "(ii)i"),
        WASI_HOST_FUNC(fd_seek, "(iIii)i"),
        WASI_HOST_FUNC(fd_sync, "(i)i"),
        WASI_HOST_FUNC(fd_tell, "(ii)i"),
        WASI_HOST_FUNC(fd_write, "(iiii)i"),

        /* path */
        WASI_HOST_FUNC(path_create_directory, "(iii)i"),
        WASI_HOST_FUNC(path_filestat_get, "(iiiii)i"),
        WASI_HOST_FUNC(path_filestat_set_times, "(iiiiIIi)i"),
        WASI_HOST_FUNC(path_link, "(iiiiiii)i"),
        WASI_HOST_FUNC(path_open, "(iiiiiIIii)i"),
        WASI_HOST_FUNC(path_readlink, "(iiiiii)i"),
        WASI_HOST_FUNC(path_remove_directory, "(iii)i"),
        WASI_HOST_FUNC(path_rename, "(iiiiii)i"),
        WASI_HOST_FUNC(path_symlink, "(iiiii)i"),
        WASI_HOST_FUNC(path_unlink_file, "(iii)i"),

        /* poll */
        WASI_HOST_FUNC(poll_oneoff, "(iiii)i"),

        /* proc */
        WASI_HOST_FUNC(proc_exit, "(i)"),

        /* random */
        WASI_HOST_FUNC(random_get, "(ii)i"),

        /* sched */
        WASI_HOST_FUNC(sched_yield, "()i"),

        /* sock */
        WASI_HOST_FUNC(sock_accept, "(iii)i"),
        WASI_HOST_FUNC(sock_recv, "(iiiiii)i"),
        WASI_HOST_FUNC(sock_send, "(iiiii)i"),
        WASI_HOST_FUNC(sock_shutdown, "(ii)i"),
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
        WASI_HOST_FUNC2("fd_filestat_get", wasi_unstable_fd_filestat_get,
                        "(ii)i"),
        WASI_HOST_FUNC2("path_filestat_get", wasi_unstable_path_filestat_get,
                        "(iiiii)i"),
        WASI_HOST_FUNC2("fd_seek", wasi_unstable_fd_seek, "(iIii)i"),
};

int
wasi_instance_add_hostfd(struct wasi_instance *inst, uint32_t wasmfd,
                         int hostfd)
{
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        toywasm_mutex_lock(&inst->lock);
        ret = wasi_fdtable_expand(inst, wasmfd);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_fd_lookup_locked(inst, wasmfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (!wasi_fdinfo_unused(fdinfo)) {
                ret = EBUSY;
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
        assert(fdinfo->hostfd == -1);
        fdinfo->hostfd = dupfd;
        if (dupfd == -1) {
                xlog_trace("failed to dup: wasm fd %" PRIu32
                           " host fd %u with errno %d",
                           wasmfd, hostfd, errno);
        }
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
                        xlog_error("wasi_instance_add_hostfd failed on fd %d "
                                   "with %d",
                                   i, ret);
                        goto fail;
                }
        }
        ret = 0;
fail:
        return ret;
}

int
wasi_instance_create(struct wasi_instance **instp) NO_THREAD_SAFETY_ANALYSIS
{
        struct wasi_instance *inst;

        inst = zalloc(sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        toywasm_mutex_init(&inst->lock);
        toywasm_cv_init(&inst->cv);
        *instp = inst;
        return 0;
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

static int
wasi_instance_prestat_add_common(struct wasi_instance *wasi, const char *path,
                                 bool is_mapdir)
{
        struct wasi_fdinfo *fdinfo = NULL;
        char *host_path = NULL;
        char *wasm_path = NULL;
        uint32_t wasifd;
        int ret;
        xlog_trace("prestat adding mapdir %s", path);

        if (is_mapdir) {
                /*
                 * <wasm dir>::<host dir>
                 *
                 * intended to be compatible with wasmtime's --mapdir
                 */

                const char *colon = strchr(path, ':');
                if (colon == NULL || colon[1] != ':') {
                        ret = EINVAL;
                        goto fail;
                }
                wasm_path = strndup(path, colon - path);
                host_path = strdup(colon + 2);
        } else {
                host_path = strdup(path);
        }
        fdinfo = wasi_fdinfo_alloc();
        if (host_path == NULL || (is_mapdir && wasm_path == NULL) ||
            fdinfo == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        fdinfo->prestat_path = host_path;
        fdinfo->wasm_path = wasm_path;
        toywasm_mutex_lock(&wasi->lock);
        ret = wasi_fd_alloc(wasi, &wasifd);
        if (ret != 0) {
                toywasm_mutex_unlock(&wasi->lock);
                goto fail;
        }
        wasi_fd_affix(wasi, wasifd, fdinfo);
        toywasm_mutex_unlock(&wasi->lock);
        xlog_trace("prestat added %s (%s)", path, fdinfo->prestat_path);
        return 0;
fail:
        free(host_path);
        free(wasm_path);
        free(fdinfo);
        return ret;
}

int
wasi_instance_prestat_add(struct wasi_instance *wasi, const char *path)
{
        return wasi_instance_prestat_add_common(wasi, path, false);
}

int
wasi_instance_prestat_add_mapdir(struct wasi_instance *wasi, const char *path)
{
        return wasi_instance_prestat_add_common(wasi, path, true);
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
        wasi_fdtable_free(inst);
        toywasm_cv_destroy(&inst->cv);
        toywasm_mutex_destroy(&inst->lock);
        free(inst);
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
import_object_create_for_wasi(struct wasi_instance *wasi,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                module_wasi, ARRAYCOUNT(module_wasi), &wasi->hi, impp);
}
