#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

struct wasi_fdinfo;
struct wasi_filestat;
struct utimes_args;

struct iovec;

typedef uint64_t wasi_off_t;

int wasi_host_fd_fallocate(struct wasi_fdinfo *fdinfo, uint64_t offset,
                           wasi_off_t len);
int wasi_host_fd_ftruncate(struct wasi_fdinfo *fdinfo, wasi_off_t size);
int wasi_host_fd_writev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                        int iovcnt, size_t *result);
int wasi_host_fd_pwritev(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                         int iovcnt, wasi_off_t off, size_t *result);
int wasi_host_fd_fcntl(struct wasi_fdinfo *fdinfo, int cmd, int data,
                       int *result);
int wasi_host_fd_readv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                       int iovcnt, size_t *result);
int wasi_host_fd_preadv(struct wasi_fdinfo *fdinfo, const struct iovec *iov,
                        int iovcnt, wasi_off_t off, size_t *result);
int wasi_host_fd_fstat(struct wasi_fdinfo *fdinfo, struct wasi_filestat *stp);
int wasi_host_fd_lseek(struct wasi_fdinfo *fdinfo, wasi_off_t offset,
                       int whence, wasi_off_t *result);
int wasi_host_fd_fsync(struct wasi_fdinfo *fdinfo);
int wasi_host_fd_fdatasync(struct wasi_fdinfo *fdinfo);
int wasi_host_fd_futimes(struct wasi_fdinfo *fdinfo,
                         const struct utimes_args *args);
int wasi_host_fd_close(struct wasi_fdinfo *fdinfo);

int wasi_host_fd_fdopendir(struct wasi_fdinfo *fdinfo, void **dirp);
int wasi_host_dir_close(void *dir);
/*
 * Note about directory offset:
 * offset 0 should be the start of the directory.
 * other offset values are implementation defined.
 */
int wasi_host_dir_seek(void *dir, uint64_t offset);
struct wasi_dirent;
int wasi_host_dir_read(void *dir, struct wasi_dirent *wde,
                       const uint8_t **namep, bool *eod);
