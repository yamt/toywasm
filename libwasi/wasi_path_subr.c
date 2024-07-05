#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasi_impl.h"
#include "wasi_path_subr.h"

void
path_clear(struct wasi_instance *wasi, struct path_info *pi)
{
        wasi_fdinfo_release(wasi, pi->dirfdinfo);
        free(pi->hostpath);
}

int
wasi_copyin_and_convert_path(struct exec_context *ctx,
                             struct wasi_instance *wasi, uint32_t dirwasifd,
                             uint32_t path, uint32_t pathlen,
                             struct path_info *pi, int *usererrorp)
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
        const char *dirpath = wasi_fdinfo_path(dirfdinfo);
        if (dirpath == NULL) {
                ret = ENOTDIR;
                goto fail;
        }
        ret = asprintf(&hostpath, "%s/%s", dirpath, wasmpath);
        if (ret < 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        xlog_trace("%s: wasifd %d wasmpath %s hostpath %s", __func__,
                   dirwasifd, wasmpath, hostpath);
        pi->hostpath = hostpath;
        pi->dirfdinfo = dirfdinfo;
        free(wasmpath);
        *usererrorp = 0;
        return 0;
fail:
        wasi_fdinfo_release(wasi, dirfdinfo);
        free(hostpath);
        free(wasmpath);
        *usererrorp = ret;
        return host_ret;
}
