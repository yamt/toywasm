#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "host_instance.h"
#include "wasi_abi.h"
#include "wasi_subr.h"
#include "wasi_vfs.h"
#include "xlog.h"

int
wasi_unstable_convert_filestat(const struct wasi_filestat *wst,
                               struct wasi_unstable_filestat *uwst)
{
        uint64_t linkcount = le64_decode(&wst->linkcount);
        if (linkcount > UINT32_MAX) {
                return E2BIG;
        }
        memset(uwst, 0, sizeof(*uwst));
        uwst->dev = wst->dev;
        uwst->ino = wst->ino;
        uwst->type = wst->type;
        le32_encode(&uwst->linkcount, linkcount);
        uwst->size = wst->size;
        uwst->atim = wst->atim;
        uwst->mtim = wst->mtim;
        uwst->ctim = wst->ctim;
        return 0;
}

int
wasi_userfd_reject_directory(struct wasi_fdinfo *fdinfo)
{
        struct wasi_filestat st;
        int ret;

        ret = wasi_vfs_fd_fstat(fdinfo, &st);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if (st.type == WASI_FILETYPE_DIRECTORY) {
                /*
                 * Note: wasmtime directory_seek.rs test expects EBADF.
                 * Why not EISDIR?
                 */
                ret = EBADF;
                goto fail;
        }
fail:
        return ret;
}

int
wasi_copyin_iovec(struct exec_context *ctx, struct meminst *mem,
                  uint32_t iov_uaddr, uint32_t iov_count,
                  struct iovec **resultp, int *usererrorp)
{
        struct iovec *hostiov = NULL;
        void *p;
        int host_ret = 0;
        int ret = 0;
        if (iov_count == 0) {
                ret = EINVAL;
                goto fail;
        }
        hostiov = calloc(iov_count, sizeof(*hostiov));
        if (hostiov == NULL) {
                ret = ENOMEM;
                goto fail;
        }
retry:
        host_ret = host_func_check_align(ctx, iov_uaddr, WASI_IOV_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        host_ret = host_func_getptr(ctx, mem, iov_uaddr,
                                    iov_count * sizeof(struct wasi_iov), &p);
        if (host_ret != 0) {
                goto fail;
        }
        const struct wasi_iov *iov_in_module = p;
        uint32_t i;
        for (i = 0; i < iov_count; i++) {
                bool moved = false;
                uint32_t iov_base = le32_decode(&iov_in_module[i].iov_base);
                uint32_t iov_len = le32_decode(&iov_in_module[i].iov_len);
                xlog_trace("iov [%" PRIu32 "] base %" PRIx32 " len %" PRIu32,
                           i, iov_base, iov_len);
                host_ret = host_func_getptr2(ctx, mem, iov_base, iov_len, &p,
                                             &moved);
                if (host_ret != 0) {
                        goto fail;
                }
                if (moved) {
                        goto retry;
                }
                hostiov[i].iov_base = p;
                hostiov[i].iov_len = iov_len;
        }
        *resultp = hostiov;
        *usererrorp = 0;
        return 0;
fail:
        free(hostiov);
        *usererrorp = ret;
        return host_ret;
}

int
wasi_validate_fstflags(uint32_t fstflags)
{
        if ((~fstflags & (WASI_FSTFLAG_ATIM_NOW | WASI_FSTFLAG_ATIM)) == 0) {
                return EINVAL;
        }
        if ((~fstflags & (WASI_FSTFLAG_MTIM_NOW | WASI_FSTFLAG_MTIM)) == 0) {
                return EINVAL;
        }
        return 0;
}
