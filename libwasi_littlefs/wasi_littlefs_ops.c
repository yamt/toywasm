#include <assert.h>
#include <errno.h>

#include "lfs.h"

#include "endian.h"
#include "wasi_littlefs_impl.h"
#include "wasi_littlefs_mount.h"
#include "wasi_path_subr.h"
#include "wasi_uio.h"
#include "wasi_vfs_impl_littlefs.h"
#include "wasi_vfs_types.h"
#include "xlog.h"

__attribute__((unused)) static char *
fdinfo_path(struct wasi_fdinfo *fdinfo)
{
        return wasi_fdinfo_to_lfs(fdinfo)->user.path;
}

static void
fdinfo_to_lfs(struct wasi_fdinfo *fdinfo, struct wasi_vfs_lfs **vfs_lfsp,
              struct wasi_fdinfo_lfs **fdinfo_lfsp)
{
        struct wasi_vfs *vfs = wasi_fdinfo_vfs(fdinfo);
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
        *dirp = &fdinfo_lfs->u.dir.dir;
        return 0;
}

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
                *dirp = &fdinfo_lfs->u.dir.dir;
        }
        return 0;
}

int
wasi_lfs_fd_fallocate(struct wasi_fdinfo *fdinfo, uint64_t offset,
                      wasi_off_t len)
{
        /*
         * littlefs doesn't have fallocate.
         * REVISIT: racy emulation is probably possible
         */
        xlog_error("%s: unimplemented", __func__);
        return ENOTSUP;
}

int
wasi_lfs_fd_ftruncate(struct wasi_fdinfo *fdinfo, wasi_off_t size)
{
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        if (ret != 0) {
                return ret;
        }
        ret = lfs_file_truncate(&lfs->lfs, file, size);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_fd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, size_t *result)
{
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        if (ret != 0) {
                return ret;
        }
        void *buf = NULL;
        size_t buflen;
        ret = wasi_iovec_flatten(iov, iovcnt, &buf, &buflen);
        if (ret != 0) {
                goto fail;
        }
        lfs_ssize_t ssz = lfs_file_write(&lfs->lfs, file, buf, buflen);
        if (ssz < 0) {
                ret = lfs_error_to_errno(ssz);
                goto fail;
        }
        *result = ssz;
        ret = 0;
fail:
        wasi_iovec_free_flattened_buffer(buf);
        return ret;
}

int
wasi_lfs_fd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, wasi_off_t off, size_t *result)
{
        /*
         * littlefs doesn't have pwrite.
         * REVISIT: racy emulation is probably possible
         */
        xlog_error("%s: unimplemented", __func__);
        return ENOTSUP;
}

int
wasi_lfs_fd_get_flags(struct wasi_fdinfo *fdinfo, uint16_t *result)
{
        /* XXX APPEND etc */
        *result = 0;
        return 0;
}

int
wasi_lfs_fd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                  int iovcnt, size_t *result)
{
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        if (ret != 0) {
                return ret;
        }
        void *buf = NULL;
        size_t buflen;
        ret = wasi_iovec_flatten_uninitialized(iov, iovcnt, &buf, &buflen);
        if (ret != 0) {
                goto fail;
        }
        lfs_ssize_t ssz = lfs_file_read(&lfs->lfs, file, buf, buflen);
        if (ssz < 0) {
                ret = lfs_error_to_errno(ssz);
                goto fail;
        }
        wasi_iovec_commit_flattened_data(iov, iovcnt, buf, ssz);
        *result = ssz;
        ret = 0;
fail:
        wasi_iovec_free_flattened_buffer(buf);
        return ret;
}

int
wasi_lfs_fd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, wasi_off_t off, size_t *result)
{
        /*
         * littlefs doesn't have pread.
         * REVISIT: racy emulation is probably possible
         */
        xlog_error("%s: unimplemented", __func__);
        return ENOTSUP;
}

int
wasi_lfs_fd_fstat(struct wasi_fdinfo *fdinfo, struct wasi_filestat *stp)
{
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        memset(stp, 0, sizeof(*stp));
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
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        if (ret == EISDIR) {
                return 0;
        }
        int lfs_whence;
        switch (whence) {
        case WASI_SEEK_SET:
                lfs_whence = LFS_SEEK_SET;
                break;
        case WASI_SEEK_CUR:
                lfs_whence = LFS_SEEK_CUR;
                break;
        case WASI_SEEK_END:
                lfs_whence = LFS_SEEK_END;
                break;
        default:
                return EINVAL;
        }
        ret = lfs_file_seek(&lfs->lfs, file, offset, lfs_whence);
        if (ret < 0) {
                return lfs_error_to_errno(ret);
        }
        *result = ret;
        return 0;
}

int
wasi_lfs_fd_fsync(struct wasi_fdinfo *fdinfo)
{
        struct wasi_vfs_lfs *lfs;
        lfs_file_t *file;
        int ret = fdinfo_to_lfs_file(fdinfo, &lfs, &file);
        if (ret == EISDIR) {
                return 0;
        }
        ret = lfs_file_sync(&lfs->lfs, file);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_fd_fdatasync(struct wasi_fdinfo *fdinfo)
{
        return wasi_lfs_fd_fsync(fdinfo);
}

int
wasi_lfs_fd_futimes(struct wasi_fdinfo *fdinfo, const struct utimes_args *args)
{
        /* littlefs doesn't have timestamp */
        xlog_trace("%s: ignore unimplemented op", __func__);
        return 0;
}

int
wasi_lfs_fd_close(struct wasi_fdinfo *fdinfo)
{
        struct wasi_vfs_lfs *vfs_lfs;
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_to_lfs(fdinfo, &vfs_lfs, &fdinfo_lfs);
        int ret;
        if (fdinfo_lfs->type == WASI_LFS_TYPE_FILE) {
                ret = lfs_file_close(&vfs_lfs->lfs, &fdinfo_lfs->u.file);
        } else {
                assert(fdinfo_lfs->type == WASI_LFS_TYPE_DIR);
                ret = lfs_dir_close(&vfs_lfs->lfs, &fdinfo_lfs->u.dir.dir);
        }
        fdinfo_lfs->type = WASI_LFS_TYPE_NONE;
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_dir_rewind(struct wasi_fdinfo *fdinfo)
{
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = fdinfo_to_lfs_dir(fdinfo, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        ret = lfs_dir_rewind(&lfs->lfs, dir);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_dir_seek(struct wasi_fdinfo *fdinfo, uint64_t offset)
{
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = fdinfo_to_lfs_dir(fdinfo, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        xlog_trace("%s: path %s offset %" PRIu64, __func__,
                   fdinfo_path(fdinfo), offset);
        ret = lfs_dir_seek(&lfs->lfs, dir, offset);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_dir_read(struct wasi_fdinfo *fdinfo, struct wasi_dirent *wde,
                  const uint8_t **namep, bool *eod)
{
        struct wasi_vfs_lfs *lfs;
        struct wasi_fdinfo_lfs *fdinfo_lfs;
        fdinfo_to_lfs(fdinfo, &lfs, &fdinfo_lfs);
        assert(fdinfo_lfs != NULL);
        if (fdinfo_lfs->type != WASI_LFS_TYPE_DIR) {
                return ENOTDIR;
        }
        xlog_trace("%s: path %s", __func__, fdinfo_path(fdinfo));
        lfs_dir_t *dir = &fdinfo_lfs->u.dir.dir;
        struct lfs_info *info = &fdinfo_lfs->u.dir.info;
        int ret = lfs_dir_read(&lfs->lfs, dir, info);
        if (ret < 0) {
                return lfs_error_to_errno(ret);
        }
        if (ret == 0) {
                *eod = true;
                return 0;
        }
        lfs_soff_t off = lfs_dir_tell(&lfs->lfs, dir);
        if (off < 0) {
                return lfs_error_to_errno(off);
        }
        memset(wde, 0, sizeof(*wde));
        wde->d_next = host_to_le64(off);
        if (info->type == LFS_TYPE_REG) {
                wde->d_type = WASI_FILETYPE_REGULAR_FILE;
        } else {
                assert(info->type == LFS_TYPE_DIR);
                wde->d_type = WASI_FILETYPE_DIRECTORY;
        }
        wde->d_namlen = host_to_le32(strlen(info->name));
        *namep = (uint8_t *)info->name;
        *eod = false;
        xlog_trace("%s: path %s -> name %s next %" PRIu64, __func__,
                   fdinfo_path(fdinfo), info->name, (uint64_t)off);
        return 0;
}

int
wasi_lfs_path_fdinfo_alloc(struct path_info *pi, struct wasi_fdinfo **fdinfop)
{
        struct wasi_vfs *vfs = wasi_fdinfo_vfs(pi->dirfdinfo);
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
        int lfs_o_flags = 0;
        if ((params->wasmoflags & WASI_OFLAG_CREAT) != 0) {
                lfs_o_flags |= LFS_O_CREAT;
        }
        if ((params->wasmoflags & WASI_OFLAG_EXCL) != 0) {
                lfs_o_flags |= LFS_O_EXCL;
        }
        if ((params->wasmoflags & WASI_OFLAG_TRUNC) != 0) {
                lfs_o_flags |= LFS_O_TRUNC;
        }
        if ((params->fdflags & WASI_FDFLAG_APPEND) != 0) {
                lfs_o_flags |= LFS_O_APPEND;
        }
        switch (params->rights_base &
                (WASI_RIGHT_FD_READ | WASI_RIGHT_FD_WRITE)) {
        case WASI_RIGHT_FD_READ:
        default:
                lfs_o_flags |= LFS_O_RDONLY;
                break;
        case WASI_RIGHT_FD_WRITE:
                lfs_o_flags |= LFS_O_WRONLY;
                break;
        case WASI_RIGHT_FD_READ | WASI_RIGHT_FD_WRITE:
                lfs_o_flags |= LFS_O_RDWR;
                break;
        }
        struct wasi_fdinfo_lfs *fdinfo_lfs = wasi_fdinfo_to_lfs(fdinfo);
        if ((params->wasmoflags & WASI_OFLAG_DIRECTORY) != 0) {
                goto open_dir;
        }
        ret = lfs_file_open(&lfs->lfs, &fdinfo_lfs->u.file, pi->hostpath,
                            lfs_o_flags);
        if (ret == 0) {
                fdinfo_lfs->type = WASI_LFS_TYPE_FILE;
        } else if (ret == LFS_ERR_ISDIR) {
open_dir:
                ret = lfs_dir_open(&lfs->lfs, &fdinfo_lfs->u.dir.dir,
                                   pi->hostpath);
                if (ret == 0) {
                        fdinfo_lfs->type = WASI_LFS_TYPE_DIR;
                        fdinfo_lfs->user.path = pi->hostpath;
                        pi->hostpath = NULL;
                }
        }
        if (ret != 0) {
                xlog_trace("%s: lfs_file_open or lfs_dir_open on %s failed "
                           "with %d",
                           __func__, pi->hostpath, ret);
                return lfs_error_to_errno(ret);
        }
        fdinfo_lfs->user.blocking =
                (params->fdflags & WASI_FDFLAG_NONBLOCK) == 0;
        return 0;
}

int
wasi_lfs_path_unlink(const struct path_info *pi)
{
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = path_to_lfs_dir(pi, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        /* XXX lfs_remove removes directory too */
        ret = lfs_remove(&lfs->lfs, pi->hostpath);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_path_mkdir(const struct path_info *pi)
{
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = path_to_lfs_dir(pi, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        ret = lfs_mkdir(&lfs->lfs, pi->hostpath);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_path_rmdir(const struct path_info *pi)
{
        struct wasi_vfs_lfs *lfs;
        lfs_dir_t *dir;
        int ret = path_to_lfs_dir(pi, &lfs, &dir);
        if (ret != 0) {
                return ret;
        }
        /* XXX lfs_remove removes regular files too */
        ret = lfs_remove(&lfs->lfs, pi->hostpath);
        return lfs_error_to_errno(ret);
}

int
wasi_lfs_path_symlink(const char *target_buf, const struct path_info *pi)
{
        /* littlefs doesn't have symlink */
        xlog_error("%s: unimplemented", __func__);
        return ENOTSUP;
}

int
wasi_lfs_path_readlink(const struct path_info *pi, char *buf, size_t buflen,
                       size_t *resultp)
{
        /* littlefs doesn't have symlink */
        xlog_error("%s: unimplemented", __func__);
        return ENOTSUP;
}

int
wasi_lfs_path_link(const struct path_info *pi1, const struct path_info *pi2)
{
        /* littlefs doesn't have hard link */
        xlog_error("%s: unimplemented", __func__);
        return ENOTSUP;
}

int
wasi_lfs_path_rename(const struct path_info *pi1, const struct path_info *pi2)
{
        if (wasi_fdinfo_is_prestat(pi1->dirfdinfo) ||
            wasi_fdinfo_is_prestat(pi2->dirfdinfo)) {
                return ENOTSUP;
        }
        struct wasi_vfs *vfs = wasi_fdinfo_vfs(pi1->dirfdinfo);
        struct wasi_vfs_lfs *lfs = wasi_vfs_to_lfs(vfs);
        int ret = lfs_rename(&lfs->lfs, pi1->hostpath, pi2->hostpath);
        return lfs_error_to_errno(ret);
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
                xlog_trace("%s: lfs_stat on %s failed with %d", __func__,
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
        /* littlefs doesn't have timestamp */
        xlog_trace("%s: ignore unimplemented op", __func__);
        return 0;
}

int
wasi_lfs_path_lutimes(const struct path_info *pi,
                      const struct utimes_args *args)
{
        /* littlefs doesn't have timestamp */
        xlog_trace("%s: ignore unimplemented op", __func__);
        return 0;
}

int
wasi_lfs_fs_umount(struct wasi_vfs *vfs)
{
        xlog_trace("%s: unmounting", __func__);
        return wasi_littlefs_umount_file(vfs);
}
