#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

int
resize_array(void **p, size_t elem_size, uint32_t new_elem_count)
{
        void *np;
        size_t bytesize;

        assert(elem_size > 0);
        bytesize = elem_size * new_elem_count;
        if (bytesize / elem_size != new_elem_count) {
                return EOVERFLOW;
        }
        if (bytesize == 0) {
                free(*p);
                np = NULL;
        } else {
                np = realloc(*p, bytesize);
                if (np == NULL) {
                        return ENOMEM;
                }
        }
        *p = np;
        return 0;
}

#if !defined(__NuttX__) /* Avoid conflicting with libc zalloc */
void *
zalloc(size_t sz)
{
        void *p = malloc(sz);
        if (p != NULL) {
                memset(p, 0, sz);
        }
        return p;
}
#endif
