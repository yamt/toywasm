#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

struct wasi_fdinfo;
struct utimes_args;
struct iovec;
struct stat;

int wasi_userfd_reject_directory(struct wasi_fdinfo *fdinfo);
int wasi_userfd_fallocate(struct wasi_fdinfo *fdinfo, off_t offset, off_t len);
int wasi_userfd_ftruncate(struct wasi_fdinfo *fdinfo, off_t size);
ssize_t wasi_userfd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                           int iovcnt);
ssize_t wasi_userfd_pwritev(struct wasi_fdinfo *fdinfo,
                            const struct iovec *iov, int iovcnt, off_t off);
ssize_t wasi_userfd_fcntl(struct wasi_fdinfo *fdinfo, int cmd, int data);
ssize_t wasi_userfd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                          int iovcnt);
ssize_t wasi_userfd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                           int iovcnt, off_t off);
int wasi_userfd_fstat(struct wasi_fdinfo *fdinfo, struct stat *stp);
off_t wasi_userfd_lseek(struct wasi_fdinfo *fdinfo, off_t offset, int whence);
int wasi_userfd_fsync(struct wasi_fdinfo *fdinfo);
int wasi_userfd_fdatasync(struct wasi_fdinfo *fdinfo);
int wasi_userfd_futimes(struct wasi_fdinfo *fdinfo,
                        const struct utimes_args *args);
int wasi_userfd_close(struct wasi_fdinfo *fdinfo);

int wasi_userfd_fdopendir(struct wasi_fdinfo *fdinfo, void **dirp);
int wasi_userdir_close(void *dir);
/*
 * Note about directory offset:
 * offset 0 should be the start of the directory.
 * other offset values are implementation defined.
 */
int wasi_userdir_seek(void *dir, uint64_t offset);
struct wasi_dirent;
int wasi_userdir_read(void *dir, struct wasi_dirent *wde,
                      const uint8_t **namep, bool *eod);
