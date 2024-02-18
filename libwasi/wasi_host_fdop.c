#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

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

#include "wasi_fdop.h"
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

static int
fdinfo_hostfd(struct wasi_fdinfo *fdinfo)
{
        assert(fdinfo->type == WASI_FDINFO_USER);
        assert(fdinfo->u.u_user.hostfd != -1);
        return fdinfo->u.u_user.hostfd;
}

int
wasi_userfd_reject_directory(struct wasi_fdinfo *fdinfo)
{
        struct stat st;
        int ret;

        ret = wasi_userfd_fstat(fdinfo, &st);
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
fail:
        return ret;
}

int
wasi_userfd_fallocate(struct wasi_fdinfo *fdinfo, off_t offset, off_t len)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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

int
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
wasi_userfd_ftruncate(struct wasi_fdinfo *fdinfo, off_t size)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        int ret = ftruncate(hostfd, size);
        return handle_errno(ret);
}

int
wasi_userfd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, size_t *resultp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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
wasi_userfd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, off_t off, size_t *resultp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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
wasi_userfd_fcntl(struct wasi_fdinfo *fdinfo, int cmd, int data, int *resultp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        errno = 0;
        int ret = fcntl(hostfd, cmd, data);
        if (ret == -1 && errno != 0) {
                return handle_errno(ret);
        }
        *resultp = ret;
        return 0;
}

int
wasi_userfd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                  int iovcnt, size_t *resultp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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
wasi_userfd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, off_t off, size_t *resultp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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
wasi_userfd_fstat(struct wasi_fdinfo *fdinfo, struct stat *stp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        int ret = fstat(hostfd, stp);
        return handle_errno(ret);
}

int
wasi_userfd_lseek(struct wasi_fdinfo *fdinfo, off_t offset, int whence,
                  off_t *resultp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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
wasi_userfd_fsync(struct wasi_fdinfo *fdinfo)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        int ret = fsync(hostfd);
        return handle_errno(ret);
}

int
wasi_userfd_fdatasync(struct wasi_fdinfo *fdinfo)
{
        int hostfd = fdinfo_hostfd(fdinfo);
#if defined(__APPLE__)
        /* macOS doesn't have fdatasync */
        int ret = fsync(hostfd);
#else
        int ret = fdatasync(hostfd);
#endif
        return handle_errno(ret);
}

int
wasi_userfd_futimes(struct wasi_fdinfo *fdinfo, const struct utimes_args *args)
{
        int hostfd = fdinfo_hostfd(fdinfo);
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
wasi_userfd_close(struct wasi_fdinfo *fdinfo)
{
        assert(fdinfo->type == WASI_FDINFO_USER);
        int ret = 0;
        int hostfd = fdinfo->u.u_user.hostfd;
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
        free(fdinfo->u.u_user.path);
        if (fdinfo->u.u_user.dir != NULL) {
                wasi_userdir_close(fdinfo->u.u_user.dir);
        }
        fdinfo->u.u_user.hostfd = -1;
        fdinfo->u.u_user.path = NULL;
        fdinfo->u.u_user.dir = NULL;
        return ret;
}

int
wasi_userfd_fdopendir(struct wasi_fdinfo *fdinfo, void **dirp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        DIR *dir = fdopendir(hostfd);
        int ret;
        if (dir == NULL) {
                ret = errno;
                assert(ret > 0);
                return ret;
        }
        *dirp = (void *)dir;
        return 0;
}
