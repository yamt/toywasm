#include <sys/types.h>

#include <dirent.h>

struct wasi_fdinfo;
struct timeval;
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
DIR *wasi_userfd_fdopendir(struct wasi_fdinfo *fdinfo);
int wasi_userfd_fstat(struct wasi_fdinfo *fdinfo, struct stat *stp);
off_t wasi_userfd_lseek(struct wasi_fdinfo *fdinfo, off_t offset, int whence);
int wasi_userfd_fsync(struct wasi_fdinfo *fdinfo);
int wasi_userfd_fdatasync(struct wasi_fdinfo *fdinfo);
int wasi_userfd_futimes(struct wasi_fdinfo *fdinfo, const struct timeval *tvp);
