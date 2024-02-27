#include <errno.h>
#include <stdlib.h>

#include "wasi_littlefs_impl.h"
#include "wasi_littlefs_ops.h"
#include "wasi_vfs_ops.h"

static const struct wasi_vfs_ops wasi_lfs_ops = {
        .fd_fallocate = wasi_lfs_fd_fallocate,
        .fd_ftruncate = wasi_lfs_fd_ftruncate,
        .fd_writev = wasi_lfs_fd_writev,
        .fd_pwritev = wasi_lfs_fd_pwritev,
        .fd_get_flags = wasi_lfs_fd_get_flags,
        .fd_readv = wasi_lfs_fd_readv,
        .fd_preadv = wasi_lfs_fd_preadv,
        .fd_fstat = wasi_lfs_fd_fstat,
        .fd_lseek = wasi_lfs_fd_lseek,
        .fd_fsync = wasi_lfs_fd_fsync,
        .fd_fdatasync = wasi_lfs_fd_fdatasync,
        .fd_futimes = wasi_lfs_fd_futimes,
        .fd_close = wasi_lfs_fd_close,
        .dir_rewind = wasi_lfs_dir_rewind,
        .dir_seek = wasi_lfs_dir_seek,
        .dir_read = wasi_lfs_dir_read,
        .path_fdinfo_alloc = wasi_lfs_path_fdinfo_alloc,
        .path_open = wasi_lfs_path_open,
        .path_unlink = wasi_lfs_path_unlink,
        .path_mkdir = wasi_lfs_path_mkdir,
        .path_rmdir = wasi_lfs_path_rmdir,
        .path_symlink = wasi_lfs_path_symlink,
        .path_readlink = wasi_lfs_path_readlink,
        .path_link = wasi_lfs_path_link,
        .path_rename = wasi_lfs_path_rename,
        .path_stat = wasi_lfs_path_stat,
        .path_lstat = wasi_lfs_path_lstat,
        .path_utimes = wasi_lfs_path_utimes,
        .path_lutimes = wasi_lfs_path_lutimes,
};

struct wasi_vfs wasi_lfs_vfs = {
        .ops = &wasi_lfs_ops,
};

const struct wasi_vfs *
wasi_get_vfs_lfs()
{
        return &wasi_lfs_vfs;
}

int
wasi_fdinfo_alloc_lfs(struct wasi_fdinfo **fdinfop, const struct wasi_vfs *vfs)
{
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_lfs = malloc(sizeof(*fdinfo_lfs));
        if (fdinfo_lfs == NULL) {
                return ENOMEM;
        }
        wasi_fdinfo_user_init(&fdinfo_lfs->user);
        fdinfo_lfs->user.vfs = vfs;
#if 0
        fdinfo_lfs->hostfd = -1;
        fdinfo_lfs->dir = NULL;
#endif
        *fdinfop = &fdinfo_lfs->user.fdinfo;
        return 0;
}

bool
wasi_fdinfo_is_lfs(struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_USER &&
               wasi_fdinfo_vfs(fdinfo) == &wasi_lfs_vfs;
}
