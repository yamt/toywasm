#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "wasi_vfs_impl_host.h"

int
wasi_fd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
               struct wasi_fdinfo **infop)
{
        toywasm_mutex_lock(&wasi->lock);
        int ret = wasi_table_lookup_locked(wasi, WASI_TABLE_FILES, wasifd,
                                           infop);
        toywasm_mutex_unlock(&wasi->lock);
        return ret;
}

int
wasi_hostfd_lookup(struct wasi_instance *wasi, uint32_t wasifd, int *hostfdp,
                   struct wasi_fdinfo **fdinfop)
{
        struct wasi_fdinfo *info;
        int ret = wasi_userfd_lookup(wasi, wasifd, &info);
        if (ret != 0) {
                return ret;
        }
        if (!wasi_fdinfo_is_host(info)) {
                wasi_fdinfo_release(wasi, info);
                return EBADF;
        }
        *hostfdp = wasi_fdinfo_hostfd(info);
        *fdinfop = info;
        return 0;
}

int
wasi_userfd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                   struct wasi_fdinfo **fdinfop)
{
        struct wasi_fdinfo *info;
        int ret = wasi_fd_lookup(wasi, wasifd, &info);
        if (ret != 0) {
                return ret;
        }
        if (info->type != WASI_FDINFO_USER) {
                wasi_fdinfo_release(wasi, info);
                return EBADF;
        }
        *fdinfop = info;
        return 0;
}

int
wasi_hostfd_add(struct wasi_instance *wasi, int hostfd, char *path,
                uint16_t fdflags, uint32_t *wasifdp)
{
        struct wasi_fdinfo *fdinfo;
        uint32_t wasifd;
        int ret;
        assert((fdflags & ~WASI_FDFLAG_NONBLOCK) == 0);
        ret = wasi_vfs_impl_host_fdinfo_alloc(&fdinfo);
        if (ret != 0) {
                free(path);
                return ret;
        }
        struct wasi_fdinfo_host *fdinfo_host = wasi_fdinfo_to_host(fdinfo);
        fdinfo_host->user.path = path;
        fdinfo_host->user.blocking = (fdflags & WASI_FDFLAG_NONBLOCK) == 0;
        fdinfo_host->hostfd = hostfd;
        ret = wasi_table_fdinfo_add(wasi, WASI_TABLE_FILES, fdinfo, &wasifd);
        if (ret != 0) {
                free(path);
                free(fdinfo);
        }
        *wasifdp = wasifd;
        return 0;
}
