#include <errno.h>
#include <stdlib.h>

#include "wasi_littlefs_impl.h"
#include "wasi_littlefs_ops.h"
#include "wasi_vfs_ops.h"

static const struct wasi_vfs_ops wasi_littlefs_ops = {
        .fd_fallocate = wasi_littlefs_fd_fallocate,
        .fd_ftruncate = wasi_littlefs_fd_ftruncate,
        .fd_writev = wasi_littlefs_fd_writev,
        .fd_pwritev = wasi_littlefs_fd_pwritev,
        .fd_get_flags = wasi_littlefs_fd_get_flags,
        .fd_readv = wasi_littlefs_fd_readv,
        .fd_preadv = wasi_littlefs_fd_preadv,
        .fd_fstat = wasi_littlefs_fd_fstat,
        .fd_lseek = wasi_littlefs_fd_lseek,
        .fd_fsync = wasi_littlefs_fd_fsync,
        .fd_fdatasync = wasi_littlefs_fd_fdatasync,
        .fd_futimes = wasi_littlefs_fd_futimes,
        .fd_close = wasi_littlefs_fd_close,
        .dir_rewind = wasi_littlefs_dir_rewind,
        .dir_seek = wasi_littlefs_dir_seek,
        .dir_read = wasi_littlefs_dir_read,
        .path_fdinfo_alloc = wasi_littlefs_path_fdinfo_alloc,
        .path_open = wasi_littlefs_path_open,
        .path_unlink = wasi_littlefs_path_unlink,
        .path_mkdir = wasi_littlefs_path_mkdir,
        .path_rmdir = wasi_littlefs_path_rmdir,
        .path_symlink = wasi_littlefs_path_symlink,
        .path_readlink = wasi_littlefs_path_readlink,
        .path_link = wasi_littlefs_path_link,
        .path_rename = wasi_littlefs_path_rename,
        .path_stat = wasi_littlefs_path_stat,
        .path_lstat = wasi_littlefs_path_lstat,
        .path_utimes = wasi_littlefs_path_utimes,
        .path_lutimes = wasi_littlefs_path_lutimes,
};

struct wasi_vfs wasi_littlefs_vfs = {
        .ops = &wasi_littlefs_ops,
};

const struct wasi_vfs *
wasi_get_vfs_littlefs()
{
        return &wasi_littlefs_vfs;
}

int
wasi_fdinfo_alloc_littlefs(struct wasi_fdinfo **fdinfop,
                           const struct wasi_vfs *vfs)
{
        struct wasi_fdinfo_littlefs *fdinfo_littlefs;
        fdinfo_littlefs = malloc(sizeof(*fdinfo_littlefs));
        if (fdinfo_littlefs == NULL) {
                return ENOMEM;
        }
        wasi_fdinfo_user_init(&fdinfo_littlefs->user);
        fdinfo_littlefs->user.vfs = vfs;
#if 0
        fdinfo_littlefs->hostfd = -1;
        fdinfo_littlefs->dir = NULL;
#endif
        *fdinfop = &fdinfo_littlefs->user.fdinfo;
        return 0;
}

bool
wasi_fdinfo_is_littlefs(struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_USER &&
               wasi_fdinfo_vfs(fdinfo) == &wasi_littlefs_vfs;
}
