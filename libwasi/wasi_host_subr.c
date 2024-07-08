#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "endian.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "wasi_utimes.h"
#include "wasi_vfs_impl_host.h"
#include "xlog.h"

uint64_t
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

void
timeval_from_ns(struct timeval *tv, uint64_t ns)
{
        tv->tv_sec = ns / 1000000000;
        tv->tv_usec = (ns % 1000000000) / 1000;
}

int
prepare_utimes_tv(const struct utimes_args *args, struct timeval *tvstore,
                  const struct timeval **resultp)
{
        uint32_t fstflags = args->fstflags;
        const struct timeval *tvp;
        if (fstflags == (WASI_FSTFLAG_ATIM_NOW | WASI_FSTFLAG_MTIM_NOW)) {
                tvp = NULL;
        } else if (fstflags == (WASI_FSTFLAG_ATIM | WASI_FSTFLAG_MTIM)) {
                timeval_from_ns(&tvstore[0], args->atim);
                timeval_from_ns(&tvstore[1], args->mtim);
                tvp = tvstore;
        } else {
                return ENOTSUP;
        }
        *resultp = tvp;
        return 0;
}

uint8_t
wasi_convert_filetype(unsigned int mode)
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

void
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

uint8_t
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

int
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
        case ENOSPC:
                wasmerrno = 51;
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
        case EOVERFLOW:
                wasmerrno = 61;
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

int
wasi_build_oflags(uint32_t lookupflags, uint32_t wasmoflags,
                  uint64_t rights_base, uint32_t fdflags, int *hostoflagsp)
{
        int oflags = 0;
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
                return ENOTSUP;
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
        *hostoflagsp = oflags;
        return 0;
}

int
wasi_fdinfo_hostfd(struct wasi_fdinfo *fdinfo)
{
        assert(wasi_fdinfo_is_host(fdinfo));
        struct wasi_fdinfo_host *fdinfo_host = wasi_fdinfo_to_host(fdinfo);
        assert(fdinfo_host->hostfd != -1);
        return fdinfo_host->hostfd;
}

struct wasi_fdinfo_host *
wasi_fdinfo_to_host(struct wasi_fdinfo *fdinfo)
{
        assert(wasi_fdinfo_is_host(fdinfo));
        struct wasi_fdinfo_host *fdinfo_host = (void *)fdinfo;
        assert(fdinfo == &fdinfo_host->user.fdinfo);
        return fdinfo_host;
}
