#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "endian.h"
#include "wasi_abi.h"
#include "wasi_host_subr.h"
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

uint8_t
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

void
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
