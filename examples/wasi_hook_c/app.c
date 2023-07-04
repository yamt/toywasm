#include <assert.h>
#include <sys/uio.h>

#include <stdio.h>
#include <unistd.h>

#define bufsize 100

int
main()
{
        char buf[bufsize];
        ssize_t len;

        len = read(STDIN_FILENO, buf, sizeof(buf));
        assert(len > 0);
        printf("hello, \"%.*s\"\n", (int)len, buf);

        char fub[bufsize];
        struct iovec iov[sizeof(fub)];
        ssize_t len2;
        int i;
        for (i = 0; i < sizeof(fub); i++) {
                iov[i].iov_base = &fub[sizeof(fub) - 1 - i];
                iov[i].iov_len = 1;
        }
        len2 = readv(STDIN_FILENO, iov, sizeof(fub));
        assert(len2 == len);
        /* skip NUL if any */
        if (fub[sizeof(fub) - len2] == 0) {
                len2--;
        }
        printf("hello, \"%.*s\" (reversed)\n", (int)len2,
               &fub[sizeof(fub) - len2]);
}
