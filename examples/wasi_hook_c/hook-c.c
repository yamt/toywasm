#include <string.h>
#include <sys/uio.h>

/*
 * CAVEAT: only very limited C environment is available here.
 *
 * - no data/bss (as it overlaps with the main app)
 * - no tls
 */

int
my_fd_read(int fd, struct iovec *iov, size_t iovlen, size_t *retp)
{
        /*
         * this function does something unnecessarily complex to ensure
         * using C stack
         */

        char buf[10];

        /*
         * we avoid using string literals because
         * they are usually placed in (ro)data section.
         * we assume that the compiler is not smart enough to
         * turn this into a single memcpy from rodata.
         *
         * note: linear memory is little endian.
         */
        unsigned int meow = 0x776f654d;
        unsigned int in_c = 0x43206e69;
        memcpy(buf, &meow, 4);
        buf[4] = ' ';
        memcpy(&buf[5], &in_c, 4);
        buf[9] = 0;

        const char *cp = buf;
        size_t len = strlen(cp) + 1;
        while (iovlen > 0) {
                size_t n;
                if (len > iov->iov_len) {
                        n = iov->iov_len;
                } else {
                        n = len;
                }
                memcpy(iov->iov_base, cp, n);
                cp += n;
                len -= n;
                iov++;
                iovlen--;
        }
        *retp = cp - buf;
        return 0;
}

void
init_C()
{
        void set_func(int, void *);

        /*
         * the first argument of set_func is the index in $func-table
         * in pivot.wat.
         */
        set_func(0, my_fd_read);
}
