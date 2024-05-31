#define _DARWIN_C_SOURCE /* strnstr */
#define _GNU_SOURCE      /* strnlen */
#define _NETBSD_SOURCE   /* strnlen */

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
        assert(sz > 0);
        void *p = malloc(sz);
        if (p != NULL) {
                memset(p, 0, sz);
        }
        return p;
}
#endif

char *
xstrnstr(const char *haystack, const char *needle, size_t len)
{
#if defined(__APPLE__)
        return strnstr(haystack, needle, len);
#else
        size_t haystack_len = strnlen(haystack, len);
        size_t needle_len = strlen(needle);
        if (needle_len > haystack_len) {
                return NULL;
        }
        size_t max_offset = haystack_len - needle_len;
        size_t i;
        for (i = 0; i <= max_offset; i++) {
                if (!memcmp(&haystack[i], needle, needle_len)) {
                        return (char *)&haystack[i]; /* discard const */
                }
        }
        return NULL;
#endif
}
