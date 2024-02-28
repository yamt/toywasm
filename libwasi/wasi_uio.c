#include <sys/types.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "wasi_uio.h"

int
wasi_iovec_flatten(const struct iovec *iov, int iovcnt, void **bufp,
                   size_t *lenp)
{
        size_t sz = 0;
        int i;
        for (i = 0; i < iovcnt; i++) {
                sz += iov->iov_len;
        }
        uint8_t *buf = malloc(sz);
        if (buf == NULL) {
                return ENOMEM;
        }
        uint8_t *p = buf;
        for (i = 0; i < iovcnt; i++) {
                memcpy(p, iov->iov_base, iov->iov_len);
                p += iov->iov_len;
        }
        assert(buf + sz == p);
        *bufp = (void *)buf;
        *lenp = sz;
        return 0;
}

int
wasi_iovec_flatten_uninitialized(const struct iovec *iov, int iovcnt,
                                 void **bufp, size_t *lenp)
{
        size_t sz = 0;
        int i;
        for (i = 0; i < iovcnt; i++) {
                sz += iov->iov_len;
        }
        uint8_t *buf = malloc(sz);
        if (buf == NULL) {
                return ENOMEM;
        }
        *bufp = (void *)buf;
        *lenp = sz;
        return 0;
}

void
wasi_iovec_commit_flattened_data(const struct iovec *iov, int iovcnt,
                                 const void *buf, size_t len)
{
        const uint8_t *p = buf;
        size_t left = len;
        int i;
        for (i = 0; i < iovcnt; i++) {
                size_t sz = iov->iov_len;
                if (left < sz) {
                        sz = left;
                }
                memcpy(iov->iov_base, p, sz);
                p += sz;
                left -= sz;
        }
        assert((const uint8_t *)buf + len == p);
}

void
wasi_iovec_free_flattened_buffer(void *buf)
{
        free(buf);
}
