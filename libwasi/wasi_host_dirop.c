#define _DARWIN_C_SOURCE /* symlink etc */

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "wasi_dirop.h"

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

int
wasi_host_open(const struct path_info *pi, int oflags, unsigned int mode)
{
        return open(pi->hostpath, oflags, mode);
}

int
wasi_host_unlink(const struct path_info *pi)
{
        return unlink(pi->hostpath);
}

int
wasi_host_mkdir(const struct path_info *pi)
{
        return mkdir(pi->hostpath, 0777);
}

int
wasi_host_rmdir(const struct path_info *pi)
{
        return rmdir(pi->hostpath);
}

int
wasi_host_symlink(const char *target_buf, const struct path_info *pi)
{
        return symlink(target_buf, pi->hostpath);
}

int
wasi_host_readlink(const struct path_info *pi, char *buf, size_t buflen)
{
        return readlink(pi->hostpath, buf, buflen);
}

int
wasi_host_link(const struct path_info *pi1, const struct path_info *pi2)
{
        return link(pi1->hostpath, pi2->hostpath);
}

int
wasi_host_rename(const struct path_info *pi1, const struct path_info *pi2)
{
        return rename(pi1->hostpath, pi2->hostpath);
}

int
wasi_host_stat(const struct path_info *pi, struct stat *stp)
{
        return stat(pi->hostpath, stp);
}

int
wasi_host_lstat(const struct path_info *pi, struct stat *stp)
{
        return lstat(pi->hostpath, stp);
}

int
wasi_host_utimes(const struct path_info *pi, const struct timeval *tvp)
{
#if defined(TOYWASM_OLD_WASI_LIBC)
        errno = ENOSYS;
        return -1;
#else
        return utimes(pi->hostpath, tvp);
#endif
}

int
wasi_host_lutimes(const struct path_info *pi, const struct timeval *tvp)
{
        return lutimes(pi->hostpath, tvp);
}
