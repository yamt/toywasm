#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileio.h"

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
        vp = mmap(NULL, sz, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
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
