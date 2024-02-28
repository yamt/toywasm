#include <sys/types.h>
#include <sys/uio.h>

/* for writev */
int wasi_iovec_flatten(const struct iovec *iov, int iovcnt, void **bufp,
                       size_t *lenp);
/* for readv */
int wasi_iovec_flatten_uninitialized(const struct iovec *iov, int iovcnt,
                                     void **bufp, size_t *lenp);
void wasi_iovec_commit_flattened_data(const struct iovec *iov, int iovcnt,
                                      const void *buf, size_t len);

void wasi_iovec_free_flattened_buffer(void *buf);
