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
#include "wasi_impl.h"

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
wasi_userfd_ftruncate(struct wasi_fdinfo *fdinfo, off_t size)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return ftruncate(hostfd, size);
}

ssize_t
wasi_userfd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return writev(hostfd, iov, iovcnt);
}

ssize_t
wasi_userfd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, off_t off)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return pwritev(hostfd, iov, iovcnt, off);
}

ssize_t
wasi_userfd_fcntl(struct wasi_fdinfo *fdinfo, int cmd, int data)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return fcntl(hostfd, cmd, data);
}

ssize_t
wasi_userfd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                  int iovcnt)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return readv(hostfd, iov, iovcnt);
}

ssize_t
wasi_userfd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, off_t off)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return preadv(hostfd, iov, iovcnt, off);
}

DIR *
wasi_userfd_fdopendir(struct wasi_fdinfo *fdinfo)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return fdopendir(hostfd);
}

int
wasi_userfd_fstat(struct wasi_fdinfo *fdinfo, struct stat *stp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return fstat(hostfd, stp);
}

off_t
wasi_userfd_lseek(struct wasi_fdinfo *fdinfo, off_t offset, int whence)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return lseek(hostfd, offset, whence);
}

int
wasi_userfd_fsync(struct wasi_fdinfo *fdinfo)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return fsync(hostfd);
}

int
wasi_userfd_fdatasync(struct wasi_fdinfo *fdinfo)
{
        int hostfd = fdinfo_hostfd(fdinfo);
#if defined(__APPLE__)
        /* macOS doesn't have fdatasync */
        return fsync(hostfd);
#else
        return fdatasync(hostfd);
#endif
}

int
wasi_userfd_futimes(struct wasi_fdinfo *fdinfo, const struct timeval *tvp)
{
        int hostfd = fdinfo_hostfd(fdinfo);
        return futimes(hostfd, tvp);
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
                if (ret != 0) {
                        ret = errno;
                        assert(ret > 0);
                        xlog_trace("failed to close: host fd %" PRIu32
                                   " with errno %d",
                                   hostfd, ret);
                }
        }
        free(fdinfo->u.u_user.path);
        if (fdinfo->u.u_user.dir != NULL) {
                closedir(fdinfo->u.u_user.dir);
        }
        fdinfo->u.u_user.hostfd = -1;
        fdinfo->u.u_user.path = NULL;
        fdinfo->u.u_user.dir = NULL;
        return ret;
}
