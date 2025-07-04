/* this file is generated by genvfs.sh */
#include "wasi_vfs_types.h"

int
wasi_vfs_fd_fallocate(struct wasi_fdinfo *fdinfo, uint64_t offset,
                      wasi_off_t len)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_fallocate == NULL) {
                return ENOTSUP;
        }
        return ops->fd_fallocate(fdinfo, offset, len);
}

int
wasi_vfs_fd_ftruncate(struct wasi_fdinfo *fdinfo, wasi_off_t size)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_ftruncate == NULL) {
                return ENOTSUP;
        }
        return ops->fd_ftruncate(fdinfo, size);
}

int
wasi_vfs_fd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, size_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_writev == NULL) {
                return ENOTSUP;
        }
        return ops->fd_writev(fdinfo, iov, iovcnt, result);
}

int
wasi_vfs_fd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                    int iovcnt, wasi_off_t off, size_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_pwritev == NULL) {
                return ENOTSUP;
        }
        return ops->fd_pwritev(fdinfo, iov, iovcnt, off, result);
}

int
wasi_vfs_fd_get_flags(struct wasi_fdinfo *fdinfo, uint16_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_get_flags == NULL) {
                return ENOTSUP;
        }
        return ops->fd_get_flags(fdinfo, result);
}

int
wasi_vfs_fd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                  int iovcnt, size_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_readv == NULL) {
                return ENOTSUP;
        }
        return ops->fd_readv(fdinfo, iov, iovcnt, result);
}

int
wasi_vfs_fd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                   int iovcnt, wasi_off_t off, size_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_preadv == NULL) {
                return ENOTSUP;
        }
        return ops->fd_preadv(fdinfo, iov, iovcnt, off, result);
}

int
wasi_vfs_fd_fstat(struct wasi_fdinfo *fdinfo, struct wasi_filestat *stp)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_fstat == NULL) {
                return ENOTSUP;
        }
        return ops->fd_fstat(fdinfo, stp);
}

int
wasi_vfs_fd_lseek(struct wasi_fdinfo *fdinfo, wasi_off_t offset, int whence,
                  wasi_off_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_lseek == NULL) {
                return ENOTSUP;
        }
        return ops->fd_lseek(fdinfo, offset, whence, result);
}

int
wasi_vfs_fd_fsync(struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_fsync == NULL) {
                return ENOTSUP;
        }
        return ops->fd_fsync(fdinfo);
}

int
wasi_vfs_fd_fdatasync(struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_fdatasync == NULL) {
                return ENOTSUP;
        }
        return ops->fd_fdatasync(fdinfo);
}

int
wasi_vfs_fd_futimes(struct wasi_fdinfo *fdinfo, const struct utimes_args *args)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_futimes == NULL) {
                return ENOTSUP;
        }
        return ops->fd_futimes(fdinfo, args);
}

int
wasi_vfs_fd_close(struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->fd_close == NULL) {
                return ENOTSUP;
        }
        return ops->fd_close(fdinfo);
}

int
wasi_vfs_dir_rewind(struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->dir_rewind == NULL) {
                return ENOTSUP;
        }
        return ops->dir_rewind(fdinfo);
}

int
wasi_vfs_dir_seek(struct wasi_fdinfo *fdinfo, uint64_t offset)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->dir_seek == NULL) {
                return ENOTSUP;
        }
        return ops->dir_seek(fdinfo, offset);
}

int
wasi_vfs_dir_read(struct wasi_fdinfo *fdinfo, struct wasi_dirent *wde,
                  const uint8_t **namep, bool *eod)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->dir_read == NULL) {
                return ENOTSUP;
        }
        return ops->dir_read(fdinfo, wde, namep, eod);
}

int
wasi_vfs_path_fdinfo_alloc(struct path_info *pi, struct wasi_fdinfo **fdinfop)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_fdinfo_alloc == NULL) {
                return ENOTSUP;
        }
        return ops->path_fdinfo_alloc(pi, fdinfop);
}

int
wasi_vfs_path_open(struct path_info *pi, const struct path_open_params *params,
                   struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_open == NULL) {
                return ENOTSUP;
        }
        return ops->path_open(pi, params, fdinfo);
}

int
wasi_vfs_path_unlink(const struct path_info *pi)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_unlink == NULL) {
                return ENOTSUP;
        }
        return ops->path_unlink(pi);
}

int
wasi_vfs_path_mkdir(const struct path_info *pi)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_mkdir == NULL) {
                return ENOTSUP;
        }
        return ops->path_mkdir(pi);
}

int
wasi_vfs_path_rmdir(const struct path_info *pi)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_rmdir == NULL) {
                return ENOTSUP;
        }
        return ops->path_rmdir(pi);
}

int
wasi_vfs_path_symlink(const char *target_buf, const struct path_info *pi)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_symlink == NULL) {
                return ENOTSUP;
        }
        return ops->path_symlink(target_buf, pi);
}

int
wasi_vfs_path_readlink(const struct path_info *pi, char *buf, size_t buflen,
                       size_t *resultp)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_readlink == NULL) {
                return ENOTSUP;
        }
        return ops->path_readlink(pi, buf, buflen, resultp);
}

int
wasi_vfs_path_link(const struct path_info *pi1, const struct path_info *pi2)
{
        if (check_xdev(pi1, pi2)) {
                return EXDEV;
        }
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi1);
        if (ops->path_link == NULL) {
                return ENOTSUP;
        }
        return ops->path_link(pi1, pi2);
}

int
wasi_vfs_path_rename(const struct path_info *pi1, const struct path_info *pi2)
{
        if (check_xdev(pi1, pi2)) {
                return EXDEV;
        }
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi1);
        if (ops->path_rename == NULL) {
                return ENOTSUP;
        }
        return ops->path_rename(pi1, pi2);
}

int
wasi_vfs_path_stat(const struct path_info *pi, struct wasi_filestat *stp)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_stat == NULL) {
                return ENOTSUP;
        }
        return ops->path_stat(pi, stp);
}

int
wasi_vfs_path_lstat(const struct path_info *pi, struct wasi_filestat *stp)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_lstat == NULL) {
                return ENOTSUP;
        }
        return ops->path_lstat(pi, stp);
}

int
wasi_vfs_path_utimes(const struct path_info *pi,
                     const struct utimes_args *args)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_utimes == NULL) {
                return ENOTSUP;
        }
        return ops->path_utimes(pi, args);
}

int
wasi_vfs_path_lutimes(const struct path_info *pi,
                      const struct utimes_args *args)
{
        const struct wasi_vfs_ops *ops = path_vfs_ops(pi);
        if (ops->path_lutimes == NULL) {
                return ENOTSUP;
        }
        return ops->path_lutimes(pi, args);
}

int
wasi_vfs_sock_fdinfo_alloc(struct wasi_fdinfo *fdinfo,
                           struct wasi_fdinfo **fdinfop)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->sock_fdinfo_alloc == NULL) {
                return ENOTSUP;
        }
        return ops->sock_fdinfo_alloc(fdinfo, fdinfop);
}

int
wasi_vfs_sock_accept(struct wasi_fdinfo *fdinfo, uint16_t fdflags,
                     struct wasi_fdinfo *fdinfo2)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->sock_accept == NULL) {
                return ENOTSUP;
        }
        return ops->sock_accept(fdinfo, fdflags, fdinfo2);
}

int
wasi_vfs_sock_recv(struct wasi_fdinfo *fdinfo, struct iovec *iov, int iovcnt,
                   uint16_t riflags, uint16_t *roflagsp, size_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->sock_recv == NULL) {
                return ENOTSUP;
        }
        return ops->sock_recv(fdinfo, iov, iovcnt, riflags, roflagsp, result);
}

int
wasi_vfs_sock_send(struct wasi_fdinfo *fdinfo, struct iovec *iov, int iovcnt,
                   uint16_t siflags, size_t *result)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->sock_send == NULL) {
                return ENOTSUP;
        }
        return ops->sock_send(fdinfo, iov, iovcnt, siflags, result);
}

int
wasi_vfs_sock_shutdown(struct wasi_fdinfo *fdinfo, uint16_t sdflags)
{
        const struct wasi_vfs_ops *ops = fdinfo_vfs_ops(fdinfo);
        if (ops->sock_shutdown == NULL) {
                return ENOTSUP;
        }
        return ops->sock_shutdown(fdinfo, sdflags);
}

int
wasi_vfs_fs_umount(struct wasi_vfs *vfs)
{
        const struct wasi_vfs_ops *ops = vfs->ops;
        if (ops->fs_umount == NULL) {
                return ENOTSUP;
        }
        return ops->fs_umount(vfs);
}
