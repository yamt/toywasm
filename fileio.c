#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileio.h"

#if defined(__wasi__)

#include <stdlib.h>

int
map_file(const char *path, void **pp, size_t *sizep)
{
        struct stat st;
        void *p;
        size_t size;
        ssize_t ssz;
        int fd;
        int ret;

        fd = open(path, O_RDONLY);
        if (fd == -1) {
                return errno;
        }
        ret = fstat(fd, &st);
        if (ret == -1) {
                close(fd);
                return errno;
        }
        size = st.st_size;
        p = malloc(size);
        if (p == NULL) {
                close(fd);
                return ENOMEM;
        }
        ssz = read(fd, p, size);
        if (ssz != size) {
                close(fd);
                return EIO;
        }
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
