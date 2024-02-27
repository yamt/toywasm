#include <errno.h>

#include "wasi_littlefs_impl.h"

int
lfs_error_to_errno(enum lfs_error lfs_error)
{
        assert(lfs_error <= 0);
        int error = 0;
        switch (lfs_error) {
        case LFS_ERR_OK:
                error = 0;
        case LFS_ERR_IO:
                error = EIO;
        case LFS_ERR_CORRUPT:
                error = EIO; /* XXX */
        case LFS_ERR_NOENT:
                error = ENOENT;
        case LFS_ERR_EXIST:
                error = EEXIST;
        case LFS_ERR_NOTDIR:
                error = ENOTDIR;
        case LFS_ERR_ISDIR:
                error = EISDIR;
        case LFS_ERR_NOTEMPTY:
                error = ENOTEMPTY;
        case LFS_ERR_BADF:
                error = EBADF;
        case LFS_ERR_FBIG:
                error = EFBIG;
        case LFS_ERR_INVAL:
                error = EINVAL;
        case LFS_ERR_NOSPC:
                error = ENOSPC;
        case LFS_ERR_NOMEM:
                error = ENOMEM;
        case LFS_ERR_NOATTR:
                error = ENOENT; /* XXX */
        case LFS_ERR_NAMETOOLONG:
                error = ENAMETOOLONG;
        }
        return error;
}

struct wasi_fdinfo_littlefs *
wasi_fdinfo_to_littlefs(struct wasi_fdinfo *fdinfo)
{
        return (void *)fdinfo;
}

struct wasi_vfs_littlefs *
wasi_vfs_to_littlefs(const struct wasi_vfs *vfs)
{
        return (void *)vfs;
}
