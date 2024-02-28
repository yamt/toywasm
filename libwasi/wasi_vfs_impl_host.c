#include <errno.h>
#include <stdlib.h>

#include "wasi_host_dirent.h"
#include "wasi_host_fdop.h"
#include "wasi_host_pathop.h"
#include "wasi_impl.h"
#include "wasi_vfs_ops.h"

static const struct wasi_vfs_ops wasi_host_ops = {
        .fd_fallocate = wasi_host_fd_fallocate,
        .fd_ftruncate = wasi_host_fd_ftruncate,
        .fd_writev = wasi_host_fd_writev,
        .fd_pwritev = wasi_host_fd_pwritev,
        .fd_get_flags = wasi_host_fd_get_flags,
        .fd_readv = wasi_host_fd_readv,
        .fd_preadv = wasi_host_fd_preadv,
        .fd_fstat = wasi_host_fd_fstat,
        .fd_lseek = wasi_host_fd_lseek,
        .fd_fsync = wasi_host_fd_fsync,
        .fd_fdatasync = wasi_host_fd_fdatasync,
        .fd_futimes = wasi_host_fd_futimes,
        .fd_close = wasi_host_fd_close,
        .dir_rewind = wasi_host_dir_rewind,
        .dir_seek = wasi_host_dir_seek,
        .dir_read = wasi_host_dir_read,
        .path_fdinfo_alloc = wasi_host_path_fdinfo_alloc,
        .path_open = wasi_host_path_open,
        .path_unlink = wasi_host_path_unlink,
        .path_mkdir = wasi_host_path_mkdir,
        .path_rmdir = wasi_host_path_rmdir,
        .path_symlink = wasi_host_path_symlink,
        .path_readlink = wasi_host_path_readlink,
        .path_link = wasi_host_path_link,
        .path_rename = wasi_host_path_rename,
        .path_stat = wasi_host_path_stat,
        .path_lstat = wasi_host_path_lstat,
        .path_utimes = wasi_host_path_utimes,
        .path_lutimes = wasi_host_path_lutimes,
};

struct wasi_vfs wasi_host_vfs = {
        .ops = &wasi_host_ops,
};

const struct wasi_vfs *
wasi_get_vfs_host()
{
        return &wasi_host_vfs;
}

int
wasi_fdinfo_alloc_host(struct wasi_fdinfo **fdinfop)
{
        struct wasi_fdinfo_host *fdinfo_host;
        fdinfo_host = malloc(sizeof(*fdinfo_host));
        if (fdinfo_host == NULL) {
                return ENOMEM;
        }
        wasi_fdinfo_user_init(&fdinfo_host->user);
        /*
         * REVISIT: should we use separate vfs for files came from
         * separate prestat to reject "cross-mount" operations?
         */
        fdinfo_host->user.vfs = &wasi_host_vfs;
        fdinfo_host->hostfd = -1;
        fdinfo_host->dir = NULL;
        *fdinfop = &fdinfo_host->user.fdinfo;
        return 0;
}

bool
wasi_fdinfo_is_host(struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_USER &&
               wasi_fdinfo_vfs(fdinfo) == &wasi_host_vfs;
}
