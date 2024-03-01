#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *
do_open(const char *name, const char *mode)
{
        FILE *fp = fopen(name, mode);
        if (fp == NULL) {
                fprintf(stderr, "fopen %s: %s\n", name, strerror(errno));
                exit(1);
        }
        return fp;
}

void
cp(const char *src, const char *dst)
{
        FILE *srcfp = do_open(src, "r");
        FILE *dstfp = do_open(dst, "w");
        char buf[10];
        while (1) {
                size_t rsz = fread(buf, 1, sizeof(buf), srcfp);
                if (rsz == 0) {
                        if (ferror(srcfp)) {
                                perror("fread");
                                exit(1);
                        }
                        assert(feof(srcfp));
                        break;
                }
                assert(rsz <= sizeof(buf));
                size_t wsz = fwrite(buf, 1, rsz, dstfp);
                if (wsz == 0) {
                        assert(ferror(dstfp));
                        perror("fwrite");
                        exit(1);
                }
                assert(wsz == rsz);
        }
        if (fclose(srcfp) || fclose(dstfp)) {
                perror("close");
                exit(1);
        }
}

int
main(int argc, char **argv)
{
        if (argc != 3) {
                exit(2);
        }
        cp(argv[1], argv[2]);
}
