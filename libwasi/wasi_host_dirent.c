#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include "endian.h"
#include "wasi_host_dirent.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"

int
wasi_host_dir_close(void *dir)
{
        int ret = closedir(dir);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                return ret;
        }
        return 0;
}

int
wasi_host_dir_rewind(void *dir)
{
        rewinddir(dir);
        return 0;
}

int
wasi_host_dir_seek(void *dir, uint64_t offset)
{
        seekdir(dir, offset);
        return 0;
}

int
wasi_host_dir_read(void *dir, struct wasi_dirent *wde, const uint8_t **namep,
                   bool *eod)
{
        int ret;
        *eod = false;
        errno = 0;
        struct dirent *d = readdir(dir);
        if (d == NULL) {
                if (errno != 0) {
                        ret = errno;
                        xlog_trace("fd_readdir: readdir failed with %d", ret);
                        goto fail;
                }
                xlog_trace("fd_readdir: EOD");
                *eod = true;
                return 0;
        }
        /*
         * Note: if the below telldir happens to returns 0 and it's
         * used by the next fd_readdir call, we will mistakenly rewind
         * the directory because it can't be distinguished from
         * WASI_DIRCOOKIE_START.
         * it can't be fixed w/o changing the wasi preview1 ABI.
         */
        long nextloc = telldir(dir);
        le64_encode(&wde->d_next, nextloc);
#if defined(__NuttX__)
        /* NuttX doesn't have d_ino */
        wde->d_ino = 0;
#else
        le64_encode(&wde->d_ino, d->d_ino);
#endif
        uint32_t namlen = strlen(d->d_name);
        le32_encode(&wde->d_namlen, namlen);
        wde->d_type = wasi_convert_dirent_filetype(d->d_type);
        xlog_trace("fd_readdir: ino %" PRIu64 " nam %.*s", (uint64_t)d->d_ino,
                   (int)namlen, d->d_name);
        *namep = (const void *)d->d_name;
        return 0;
fail:
        return ret;
}