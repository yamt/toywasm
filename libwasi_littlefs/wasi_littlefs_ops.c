#include <errno.h>

#include "lfs.h"

#include "wasi_littlefs_impl.h"
#include "wasi_path_subr.h"
#include "wasi_vfs_impl_littlefs.h"
#include "wasi_vfs_types.h"

static void
fdinfo_to_lfs(struct wasi_fdinfo *fdinfo, lfs_t **lfsp,
              struct wasi_fdinfo_lfs **fdinfo_lfsp)
{
        const struct wasi_vfs *vfs = wasi_fdinfo_vfs(fdinfo);
        *lfsp = wasi_vfs_to_lfs(vfs)->lfs;
        *fdinfo_lfsp = wasi_fdinfo_to_lfs(fdinfo);
}

#if 0
static int
fdinfo_to_lfs_file(struct wasi_fdinfo *fdinfo, lfs_t **lfsp, lfs_file_t **filep)
{
    struct wasi_fdinfo_lfs *fdinfo_lfs;
    fdinfo_to_lfs(fdinfo, lfsp, &fdinfo_lfs);
    if (fdinfo_lfs->type != WASI_LFS_TYPE_FILE) {
        return EISDIR;
    }
    *filep = &fdinfo_lfs->u.file;
    return 0;
}
#endif

static int
fdinfo_to_lfs_dir(struct wasi_fdinfo *fdinfo, lfs_t **lfsp, lfs_dir_t **dirp)
{
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_to_lfs(fdinfo, lfsp, &fdinfo_lfs);
        if (fdinfo_lfs->type != WASI_LFS_TYPE_DIR) {
                return ENOTDIR;
        }
        *dirp = &fdinfo_lfs->u.dir;
        return 0;
}

int
wasi_lfs_fd_fallocate(struct wasi_fdinfo *fdinfo, uint64_t offset,
                      wasi_off_t len)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_ftruncate(struct wasi_fdinfo *fdinfo, wasi_off_t size)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, size_t *result)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, wasi_off_t off, size_t *result)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_get_flags(struct wasi_fdinfo *fdinfo, uint16_t *result)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                  int iovcnt, size_t *result)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, wasi_off_t off, size_t *result)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_fstat(struct wasi_fdinfo *fdinfo, struct wasi_filestat *stp)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_lseek(struct wasi_fdinfo *fdinfo, wasi_off_t offset, int whence,
                  wasi_off_t *result)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_fsync(struct wasi_fdinfo *fdinfo)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_fdatasync(struct wasi_fdinfo *fdinfo)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_futimes(struct wasi_fdinfo *fdinfo, const struct utimes_args *args)
{
        return ENOTSUP;
}

int
wasi_lfs_fd_close(struct wasi_fdinfo *fdinfo)
{
        return ENOTSUP;
}

int
wasi_lfs_dir_rewind(struct wasi_fdinfo *fdinfo)
{
        return ENOTSUP;
}

int
wasi_lfs_dir_seek(struct wasi_fdinfo *fdinfo, uint64_t offset)
{
        return ENOTSUP;
}

int
wasi_lfs_dir_read(struct wasi_fdinfo *fdinfo, struct wasi_dirent *wde,
                  const uint8_t **namep, bool *eod)
{
        return ENOTSUP;
}

int
wasi_lfs_path_fdinfo_alloc(struct path_info *pi, struct wasi_fdinfo **fdinfop)
{
        return ENOTSUP;
}

int
wasi_lfs_path_open(struct path_info *pi, const struct path_open_params *params,
                   struct wasi_fdinfo *fdinfo)
{
        lfs_t *lfs;
        lfs_dir_t *dir;
        int ret = fdinfo_to_lfs_dir(pi->dirfdinfo, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo_lfs *fdinfo_lfs = wasi_fdinfo_to_lfs(fdinfo);
        ret = lfs_file_open(lfs, &fdinfo_lfs->u.file, pi->hostpath, 0);
        if (ret != 0) {
                return lfs_error_to_errno(ret);
        }
        return ENOTSUP;
}

int
wasi_lfs_path_unlink(const struct path_info *pi)
{
        return ENOTSUP;
}

int
wasi_lfs_path_mkdir(const struct path_info *pi)
{
        return ENOTSUP;
}

int
wasi_lfs_path_rmdir(const struct path_info *pi)
{
        return ENOTSUP;
}

int
wasi_lfs_path_symlink(const char *target_buf, const struct path_info *pi)
{
        return ENOTSUP;
}

int
wasi_lfs_path_readlink(const struct path_info *pi, char *buf, size_t buflen,
                       size_t *resultp)
{
        return ENOTSUP;
}

int
wasi_lfs_path_link(const struct path_info *pi1, const struct path_info *pi2)
{
        return ENOTSUP;
}

int
wasi_lfs_path_rename(const struct path_info *pi1, const struct path_info *pi2)
{
        return ENOTSUP;
}

int
wasi_lfs_path_stat(const struct path_info *pi, struct wasi_filestat *stp)
{
        return ENOTSUP;
}

int
wasi_lfs_path_lstat(const struct path_info *pi, struct wasi_filestat *stp)
{
        return ENOTSUP;
}

int
wasi_lfs_path_utimes(const struct path_info *pi,
                     const struct utimes_args *args)
{
        return ENOTSUP;
}

int
wasi_lfs_path_lutimes(const struct path_info *pi,
                      const struct utimes_args *args)
{
        return ENOTSUP;
}
