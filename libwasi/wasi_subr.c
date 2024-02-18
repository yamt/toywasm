#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "endian.h"
#include "wasi_abi.h"
#include "wasi_host_fdop.h"
#include "wasi_subr.h"

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

        ret = wasi_host_fd_fstat(fdinfo, &st);
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
