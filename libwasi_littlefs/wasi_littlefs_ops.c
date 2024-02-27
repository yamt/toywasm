#include <errno.h>

#include "lfs.h"

#include "endian.h"
#include "wasi_littlefs_impl.h"
#include "wasi_path_subr.h"
#include "wasi_vfs_impl_littlefs.h"
#include "wasi_vfs_types.h"
#include "xlog.h"

static void
fdinfo_to_lfs(struct wasi_fdinfo *fdinfo, struct wasi_vfs_lfs **vfs_lfsp,
              struct wasi_fdinfo_lfs **fdinfo_lfsp)
{
        const struct wasi_vfs *vfs = wasi_fdinfo_vfs(fdinfo);
        *vfs_lfsp = wasi_vfs_to_lfs(vfs);
        if (wasi_fdinfo_is_prestat(fdinfo)) {
                *fdinfo_lfsp = NULL;
        } else {
                *fdinfo_lfsp = wasi_fdinfo_to_lfs(fdinfo);
        }
}

static int
fdinfo_to_lfs_file(struct wasi_fdinfo *fdinfo, struct wasi_vfs_lfs **vfs_lfsp,
                   lfs_file_t **filep)
{
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_to_lfs(fdinfo, vfs_lfsp, &fdinfo_lfs);
        assert(fdinfo_lfs != NULL);
        if (fdinfo_lfs->type != WASI_LFS_TYPE_FILE) {
                return EISDIR;
        }
        *filep = &fdinfo_lfs->u.file;
        return 0;
}

#if 0
static int
fdinfo_to_lfs_dir(struct wasi_fdinfo *fdinfo, struct wasi_vfs_lfs **vfs_lfsp,
                  lfs_dir_t **dirp)
{
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_to_lfs(fdinfo, vfs_lfsp, &fdinfo_lfs);
        assert(fdinfo_lfs != NULL);
        if (fdinfo_lfs->type != WASI_LFS_TYPE_DIR) {
                return ENOTDIR;
        }
        *dirp = &fdinfo_lfs->u.dir;
        return 0;
}
#endif

static int
path_to_lfs_dir(const struct path_info *pi, struct wasi_vfs_lfs **vfs_lfsp,
                lfs_dir_t **dirp)
{
        /* Note: pi can be a prestat */
        struct wasi_fdinfo *fdinfo = pi->dirfdinfo;
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_to_lfs(fdinfo, vfs_lfsp, &fdinfo_lfs);
        if (fdinfo_lfs == NULL) {
                /* prestat */
                *dirp = NULL;
        } else {
                if (fdinfo_lfs->type != WASI_LFS_TYPE_DIR) {
                        return ENOTDIR;
                }
                *dirp = &fdinfo_lfs->u.dir;
        }
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
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        memset(stp, 0, sizeof(*stp));
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        if (ret != EISDIR) {
                if (ret != 0) {
                        return ret;
                }
                lfs_soff_t size = lfs_file_size(&lfs->lfs, file);
                if (size < 0) {
                        return lfs_error_to_errno(size);
                }
                stp->size = host_to_le64(size);
                stp->type = WASI_FILETYPE_REGULAR_FILE;
        } else {
                /* directory */
                stp->type = WASI_FILETYPE_DIRECTORY;
                /* REVISIT: should emulate linkcount for child directories? */
        }
        stp->linkcount = host_to_le64(1);
        return 0;
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
        const struct wasi_vfs *vfs = wasi_fdinfo_vfs(pi->dirfdinfo);
        return wasi_fdinfo_alloc_lfs(fdinfop, vfs);
}

int
wasi_lfs_path_open(struct path_info *pi, const struct path_open_params *params,
                   struct wasi_fdinfo *fdinfo)
{
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = path_to_lfs_dir(pi, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        struct wasi_fdinfo_lfs *fdinfo_lfs = wasi_fdinfo_to_lfs(fdinfo);
        ret = lfs_file_open(&lfs->lfs, &fdinfo_lfs->u.file, pi->hostpath, 0);
        if (ret == 0) {
                fdinfo_lfs->type = WASI_LFS_TYPE_FILE;
        }
        if (ret == LFS_ERR_ISDIR) {
                ret = lfs_dir_open(&lfs->lfs, &fdinfo_lfs->u.dir,
                                   pi->hostpath);
                if (ret == 0) {
                        fdinfo_lfs->type = WASI_LFS_TYPE_DIR;
                        fdinfo_lfs->user.path = pi->hostpath;
                        pi->hostpath = NULL;
                }
        }
        if (ret != 0) {
                xlog_error("%s: lfs_stat on %s failed with %d", __func__,
                           pi->hostpath, ret);
                return lfs_error_to_errno(ret);
        }
        fdinfo_lfs->user.blocking =
                (params->fdflags & WASI_FDFLAG_NONBLOCK) == 0;
        return 0;
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
        xlog_trace("%s: path %s", __func__, pi->hostpath);
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = path_to_lfs_dir(pi, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        struct lfs_info info;
        ret = lfs_stat(&lfs->lfs, pi->hostpath, &info);
        if (ret != 0) {
                xlog_error("%s: lfs_stat on %s failed with %d", __func__,
                           pi->hostpath, ret);
                return lfs_error_to_errno(ret);
        }
        memset(stp, 0, sizeof(*stp));
        if (info.type == LFS_TYPE_REG) {
                stp->type = WASI_FILETYPE_REGULAR_FILE;
        } else {
                assert(info.type == LFS_TYPE_DIR);
                stp->type = WASI_FILETYPE_DIRECTORY;
        }
        stp->linkcount = host_to_le64(1);
        stp->size = host_to_le64(info.size);
        return 0;
}

int
wasi_lfs_path_lstat(const struct path_info *pi, struct wasi_filestat *stp)
{
        /* littlefs doesn't have symlink */
        return wasi_lfs_path_stat(pi, stp);
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
