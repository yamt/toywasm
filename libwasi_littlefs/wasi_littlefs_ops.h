#include "wasi_vfs_types.h"

int wasi_lfs_fd_fallocate(struct wasi_fdinfo *fdinfo, uint64_t offset,
                          wasi_off_t len);
int wasi_lfs_fd_ftruncate(struct wasi_fdinfo *fdinfo, wasi_off_t size);
int wasi_lfs_fd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                       int iovcnt, size_t *result);
int wasi_lfs_fd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                        int iovcnt, wasi_off_t off, size_t *result);
int wasi_lfs_fd_get_flags(struct wasi_fdinfo *fdinfo, uint16_t *result);
int wasi_lfs_fd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                      int iovcnt, size_t *result);
int wasi_lfs_fd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                       int iovcnt, wasi_off_t off, size_t *result);
int wasi_lfs_fd_fstat(struct wasi_fdinfo *fdinfo, struct wasi_filestat *stp);
int wasi_lfs_fd_lseek(struct wasi_fdinfo *fdinfo, wasi_off_t offset,
                      int whence, wasi_off_t *result);
int wasi_lfs_fd_fsync(struct wasi_fdinfo *fdinfo);
int wasi_lfs_fd_fdatasync(struct wasi_fdinfo *fdinfo);
int wasi_lfs_fd_futimes(struct wasi_fdinfo *fdinfo,
                        const struct utimes_args *args);
int wasi_lfs_fd_close(struct wasi_fdinfo *fdinfo);
int wasi_lfs_dir_rewind(struct wasi_fdinfo *fdinfo);
int wasi_lfs_dir_seek(struct wasi_fdinfo *fdinfo, uint64_t offset);
int wasi_lfs_dir_read(struct wasi_fdinfo *fdinfo, struct wasi_dirent *wde,
                      const uint8_t **namep, bool *eod);
int wasi_lfs_path_fdinfo_alloc(struct path_info *pi,
                               struct wasi_fdinfo **fdinfop);
int wasi_lfs_path_open(struct path_info *pi,
                       const struct path_open_params *params,
                       struct wasi_fdinfo *fdinfo);
int wasi_lfs_path_unlink(const struct path_info *pi);
int wasi_lfs_path_mkdir(const struct path_info *pi);
int wasi_lfs_path_rmdir(const struct path_info *pi);
int wasi_lfs_path_symlink(const char *target_buf, const struct path_info *pi);
int wasi_lfs_path_readlink(const struct path_info *pi, char *buf,
                           size_t buflen, size_t *resultp);
int wasi_lfs_path_link(const struct path_info *pi1,
                       const struct path_info *pi2);
int wasi_lfs_path_rename(const struct path_info *pi1,
                         const struct path_info *pi2);
int wasi_lfs_path_stat(const struct path_info *pi, struct wasi_filestat *stp);
int wasi_lfs_path_lstat(const struct path_info *pi, struct wasi_filestat *stp);
int wasi_lfs_path_utimes(const struct path_info *pi,
                         const struct utimes_args *args);
int wasi_lfs_path_lutimes(const struct path_info *pi,
                          const struct utimes_args *args);
int wasi_lfs_fs_umount(struct wasi_vfs *vfs);
