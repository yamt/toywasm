#include <errno.h>
#include <inttypes.h>

#include "str_to_uint.h"

int
str_to_uint(const char *s, int base, uintmax_t *resultp)
{
        uintmax_t v;
        char *ep;
        errno = 0;
        v = strtoumax(s, &ep, base);
        if (s == ep) {
                return EINVAL;
        }
        if (*ep != 0) {
                return EINVAL;
        }
        if (errno != 0) {
                return errno;
        }
        *resultp = v;
        return 0;
}

int
str_to_u32(const char *s, int base, uint32_t *resultp)
{
        uintmax_t u;
        int ret = str_to_uint(s, base, &u);
        if (ret != 0) {
                return ret;
        }
        if (u > UINT32_MAX) {
                return EOVERFLOW;
        }
        *resultp = u;
        return 0;
}
