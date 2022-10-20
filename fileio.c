#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileio.h"
#include "xlog.h"

/*
 * WASI doesn't have mmap.
 * Note: wasi-libc has _WASI_EMULATED_MMAN, which is probably enough
 * for our purpose. But we use our own version here.
 *
 * NuttX doesn't have working mmap.
 */
#if defined(__wasi__) || defined(__NuttX__)

#include <stdlib.h>

int
map_file(const char *path, void **pp, size_t *sizep)
{
        void *p;
        size_t size;
        ssize_t ssz;
        int fd;
        int ret;

        xlog_trace("opening %s", path);
        fd = open(path, O_RDONLY);
        if (fd == -1) {
                ret = errno;
                assert(ret != 0);
                xlog_trace("failed to open %s (error %d)", path, ret);
                return ret;
        }
#if 1
        struct stat st;
        ret = fstat(fd, &st);
        if (ret == -1) {
                ret = errno;
                assert(ret != 0);
                xlog_trace("failed to fstat %s (error %d)", path, ret);
                close(fd);
                return ret;
        }
        size = st.st_size;
#else
        off_t off;
        off = lseek(fd, 0, SEEK_END);
        if (off == -1) {
                ret = errno;
                assert(ret != 0);
                xlog_trace("failed to lseek %s (error %d)", path, ret);
                close(fd);
                return ret;
        }
        size = off;
        off = lseek(fd, 0, SEEK_SET);
        if (off == -1) {
                ret = errno;
                assert(ret != 0);
                xlog_trace("failed to lseek %s (error %d)", path, ret);
                close(fd);
                return ret;
        }
#endif
        xlog_trace("file size %zu", size);
        if (size > 0) {
                p = malloc(size);
        } else {
                /* Avoid a confusing error */
                p = malloc(1);
        }
        if (p == NULL) {
                close(fd);
                return ENOMEM;
        }
        ssz = read(fd, p, size);
        if (ssz != size) {
                ret = errno;
                assert(ret != 0);
                xlog_trace("failed to read %s (error %d)", path, ret);
                close(fd);
                return ret;
        }
        close(fd);
        *pp = p;
        *sizep = size;
        return 0;
}

void
unmap_file(void *p, size_t sz)
{
        free(p);
}

#else

#include <sys/mman.h>

int
map_file(const char *filename, void **pp, size_t *szp)
{
        struct stat st;
        int ret;
        int fd;

        xlog_trace("mapping %s", filename);
        fd = open(filename, O_RDONLY);
        if (fd == -1) {
                return errno;
        }

        ret = fstat(fd, &st);
        if (ret == -1) {
                close(fd);
                return errno;
        }

        void *vp;
        size_t sz = st.st_size;
        vp = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
        if (vp == (void *)MAP_FAILED) {
                close(fd);
                return errno;
        }
        close(fd);

        *pp = vp;
        *szp = sz;
        return 0;
}

void
unmap_file(void *p, size_t sz)
{
        munmap(p, sz);
}

#endif
