#define _DARWIN_C_SOURCE /* strnstr */
#define _GNU_SOURCE      /* strnlen */
#define _NETBSD_SOURCE   /* strnlen */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "util.h"

int
array_extend(struct mem_context *mctx, void **p, size_t elem_size,
             uint32_t old_elem_count, uint32_t new_elem_count)
{
        void *np;

        assert(elem_size > 0);
        assert(old_elem_count < new_elem_count);
        const size_t old_bytesize = elem_size * old_elem_count;
        size_t bytesize;
        if (MUL_SIZE_OVERFLOW(elem_size, new_elem_count, &bytesize)) {
                return EOVERFLOW;
        }
        np = mem_extend(mctx, *p, old_bytesize, bytesize);
        if (np == NULL) {
                return ENOMEM;
        }
        *p = np;
        return 0;
}

int
array_shrink(struct mem_context *mctx, void **p, size_t elem_size,
             uint32_t old_elem_count, uint32_t new_elem_count)
{
        void *np;

        assert(elem_size > 0);
        assert(old_elem_count > new_elem_count);
        const size_t old_bytesize = elem_size * old_elem_count;
        if (new_elem_count == 0) {
                mem_free(mctx, *p, old_bytesize);
                np = NULL;
        } else {
                const size_t bytesize = elem_size * new_elem_count;
                assert(bytesize / elem_size == new_elem_count);
                np = mem_shrink(mctx, *p, old_bytesize, bytesize);
                if (np == NULL) {
                        return ENOMEM; /* XXX can this happen? */
                }
        }
        *p = np;
        return 0;
}

void *
xzalloc(size_t sz)
{
        assert(sz > 0);
        void *p = malloc(sz);
        if (p != NULL) {
                memset(p, 0, sz);
        }
        return p;
}

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
