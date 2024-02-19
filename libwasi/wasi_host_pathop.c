#define _DARWIN_C_SOURCE /* symlink etc */
#define _GNU_SOURCE      /* symlink etc */

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wasi_host_pathop.h"
#include "wasi_host_subr.h"
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
wasi_host_path_open(struct path_info *pi,
                    const struct path_open_params *params,
                    struct wasi_fdinfo *fdinfo)
{
        int hostfd = -1;
        int ret;
        int oflags;
        ret = wasi_build_oflags(params->lookupflags, params->wasmoflags,
                                params->rights_base, params->fdflags, &oflags);
        if (ret != 0) {
                goto fail;
        }
        /*
         * Note: mode 0666 is what wasm-micro-runtime and wasmtime use.
         *
         * wasm-micro-runtime has it hardcoded:
         * https://github.com/bytecodealliance/wasm-micro-runtime/blob/cadf9d0ad36ec12e2a1cab4edf5f0dfb9bf84de0/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/src/posix.c#L1971
         *
         * wasmtime uses the default of the underlying library:
         * https://doc.rust-lang.org/nightly/std/os/unix/fs/trait.OpenOptionsExt.html#tymethod.mode
         */
        ret = open(pi->hostpath, oflags | O_NONBLOCK, 0666);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        hostfd = ret;
        struct stat stat;
        ret = fstat(hostfd, &stat);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (!S_ISDIR(stat.st_mode)) {
                free(pi->hostpath);
                pi->hostpath = NULL;
        }
        fdinfo->type = WASI_FDINFO_USER;
        fdinfo->u.u_user.hostfd = hostfd;
        fdinfo->u.u_user.dir = NULL;
        fdinfo->u.u_user.path = pi->hostpath;
        fdinfo->blocking = (params->fdflags & WASI_FDFLAG_NONBLOCK) == 0;
        pi->hostpath = NULL;
        hostfd = -1;
fail:
        if (hostfd != -1) {
                close(hostfd);
        }
        return ret;
}

int
wasi_host_path_unlink(const struct path_info *pi)
{
        int ret = unlink(pi->hostpath);
        return handle_errno(ret);
}

int
wasi_host_path_mkdir(const struct path_info *pi)
{
        int ret = mkdir(pi->hostpath, 0777);
        return handle_errno(ret);
}

int
wasi_host_path_rmdir(const struct path_info *pi)
{
        int ret = rmdir(pi->hostpath);
        return handle_errno(ret);
}

int
wasi_host_path_symlink(const char *target_buf, const struct path_info *pi)
{
        int ret = symlink(target_buf, pi->hostpath);
        return handle_errno(ret);
}

int
wasi_host_path_readlink(const struct path_info *pi, char *buf, size_t buflen,
                        size_t *resultp)
{
        ssize_t ret = readlink(pi->hostpath, buf, buflen);
        if (ret == -1) {
                return handle_errno(ret);
        }
        *resultp = ret;
        return 0;
}

int
wasi_host_path_link(const struct path_info *pi1, const struct path_info *pi2)
{
        int ret = link(pi1->hostpath, pi2->hostpath);
        return handle_errno(ret);
}

int
wasi_host_path_rename(const struct path_info *pi1, const struct path_info *pi2)
{
        int ret = rename(pi1->hostpath, pi2->hostpath);
        return handle_errno(ret);
}

int
wasi_host_path_stat(const struct path_info *pi, struct wasi_filestat *wstp)
{
        struct stat st;
        int ret = stat(pi->hostpath, &st);
        if (ret != 0) {
                return handle_errno(ret);
        }
        wasi_convert_filestat(&st, wstp);
        return 0;
}

int
wasi_host_path_lstat(const struct path_info *pi, struct wasi_filestat *wstp)
{
        struct stat st;
        int ret = lstat(pi->hostpath, &st);
        if (ret != 0) {
                return handle_errno(ret);
        }
        wasi_convert_filestat(&st, wstp);
        return 0;
}

int
wasi_host_path_utimes(const struct path_info *pi,
                      const struct utimes_args *args)
{
#if defined(TOYWASM_OLD_WASI_LIBC)
        return ENOSYS;
#else
        struct timeval tv[2];
        const struct timeval *tvp;
        int ret;
        ret = prepare_utimes_tv(args, tv, &tvp);
        if (ret != 0) {
                return ret;
        }
        ret = utimes(pi->hostpath, tvp);
        return handle_errno(ret);
#endif
}

int
wasi_host_path_lutimes(const struct path_info *pi,
                       const struct utimes_args *args)
{
        struct timeval tv[2];
        const struct timeval *tvp;
        int ret;
        ret = prepare_utimes_tv(args, tv, &tvp);
        if (ret != 0) {
                return ret;
        }
        ret = lutimes(pi->hostpath, tvp);
        return handle_errno(ret);
}
