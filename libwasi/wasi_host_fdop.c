#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#include "wasi_host_dirent.h"
#include "wasi_host_fdop.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"

#if defined(__wasi__)
/*
 * For some reasons, wasi-libc doesn't have legacy stuff enabled.
 * It includes lutimes and futimes.
 */

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
        if (S_ISDIR(sb.st_mode)) {
                /*
                 * Note: wasmtime tests expects EBADF.
                 * Why not EISDIR?
                 */
                return EBADF;
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

int
wasi_host_fd_fallocate(struct wasi_fdinfo *fdinfo, wasi_off_t offset,
                       wasi_off_t len)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int ret;
        /*
         * macOS doesn't have posix_fallocate
         * cf. https://github.com/WebAssembly/wasi-filesystem/issues/19
         */
#if defined(__APPLE__)
        ret = racy_fallocate(hostfd, offset, len);
#else
        ret = posix_fallocate(hostfd, offset, len);
#endif
        return ret;
}

static int
handle_errno(int orig_ret)
{
        if (orig_ret == -1) {
                int ret = errno;
                assert(ret > 0);
                return ret;
        }
        assert(orig_ret == 0);
        return 0;
}

int
wasi_host_fd_ftruncate(struct wasi_fdinfo *fdinfo, wasi_off_t size)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int ret = ftruncate(hostfd, size);
        return handle_errno(ret);
}

int
wasi_host_fd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, size_t *resultp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        ssize_t ssz = writev(hostfd, iov, iovcnt);
        if (ssz == -1) {
                int ret = errno;
                assert(ret > 0);
                return ret;
        }
        *resultp = ssz;
        return 0;
}

int
wasi_host_fd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                     int iovcnt, wasi_off_t off, size_t *resultp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        ssize_t ssz = pwritev(hostfd, iov, iovcnt, off);
        if (ssz == -1) {
                int ret = errno;
                assert(ret > 0);
                return ret;
        }
        *resultp = ssz;
        return 0;
}

int
wasi_host_fd_get_flags(struct wasi_fdinfo *fdinfo, uint16_t *resultp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        errno = 0;
        int ret = fcntl(hostfd, F_GETFL, 0);
        if (ret == -1 && errno != 0) {
                return handle_errno(ret);
        }
        uint16_t flags = 0;
        if ((ret & O_APPEND) != 0) {
                flags |= WASI_FDFLAG_APPEND;
        }
        if ((ret & O_NONBLOCK) != 0) {
                flags |= WASI_FDFLAG_NONBLOCK;
        }
        *resultp = flags;
        return 0;
}

int
wasi_host_fd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, size_t *resultp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        ssize_t ssz = readv(hostfd, iov, iovcnt);
        if (ssz == -1) {
                int ret = errno;
                assert(ret > 0);
                return ret;
        }
        *resultp = ssz;
        return 0;
}

int
wasi_host_fd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, wasi_off_t off, size_t *resultp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        ssize_t ssz = preadv(hostfd, iov, iovcnt, off);
        if (ssz == -1) {
                int ret = errno;
                assert(ret > 0);
                return ret;
        }
        *resultp = ssz;
        return 0;
}

int
wasi_host_fd_fstat(struct wasi_fdinfo *fdinfo, struct wasi_filestat *wstp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        struct stat st;
        int ret = fstat(hostfd, &st);
        if (ret != 0) {
                return handle_errno(ret);
        }
        wasi_convert_filestat(&st, wstp);
        return 0;
}

int
wasi_host_fd_lseek(struct wasi_fdinfo *fdinfo, wasi_off_t offset, int whence,
                   wasi_off_t *resultp)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        errno = 0;
        off_t off = lseek(hostfd, offset, whence);
        if (off == -1 && errno != 0) {
                int ret = errno;
                assert(errno > 0);
                return ret;
        }
        *resultp = off;
        return 0;
}

int
wasi_host_fd_fsync(struct wasi_fdinfo *fdinfo)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int ret = fsync(hostfd);
        return handle_errno(ret);
}

int
wasi_host_fd_fdatasync(struct wasi_fdinfo *fdinfo)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
#if defined(__APPLE__)
        /* macOS doesn't have fdatasync */
        int ret = fsync(hostfd);
#else
        int ret = fdatasync(hostfd);
#endif
        return handle_errno(ret);
}

int
wasi_host_fd_futimes(struct wasi_fdinfo *fdinfo,
                     const struct utimes_args *args)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        struct timeval tv[2];
        const struct timeval *tvp;
        int ret;
        ret = prepare_utimes_tv(args, tv, &tvp);
        if (ret != 0) {
                return ret;
        }
        ret = futimes(hostfd, tvp);
        return handle_errno(ret);
}

int
wasi_host_fd_close(struct wasi_fdinfo *fdinfo)
{
        assert(fdinfo->type == WASI_FDINFO_USER);
        struct wasi_fdinfo_host *fdinfo_host = wasi_fdinfo_to_host(fdinfo);
        int ret = 0;
        int hostfd = fdinfo_host->hostfd;
#if defined(__wasi__) /* wasi has no dup */
        if (hostfd != -1 && hostfd >= 3) {
#else
        if (hostfd != -1) {
#endif
                ret = close(hostfd);
                ret = handle_errno(ret);
                if (ret != 0) {
                        xlog_trace("failed to close: host fd %" PRIu32
                                   " with errno %d",
                                   hostfd, ret);
                }
        }
        if (fdinfo_host->dir != NULL) {
                wasi_host_dir_close(fdinfo);
        }
        fdinfo_host->hostfd = -1;
        fdinfo_host->dir = NULL;
        return ret;
}
