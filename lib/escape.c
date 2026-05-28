/*
 * string escaping utilities
 *
 * the primary use case is to print wasm names to stdout/stderr.
 * note that the stdout encoding depends on the C locale. it isn't
 * necessarily utf-8.
 *
 * besides that, this is also used for user-provided strings, which
 * is not necessarily in utf-8.
 *
 * we use the printf(1) style escape.
 * eg. \123
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "escape.h"
#include "type.h"

static unsigned int max_escaped_len = INT_MAX;

static bool
need_escape(char ch)
{
        /*
         * ascii is assumed here.
         *
         * we escape several extra characters which are often used as
         * delimiters in log messages.
         */
        return ch < 0x20 || ch == ' ' || ch == '=' || ch == ':' ||
               ch == '\\' || ch == ',' || ch >= 0x7f;
}

static char
oct(uint8_t v)
{
        static const char digits[16] = "01234567";
        assert(v < 8);
        return digits[v];
}

void
escape_name(struct escaped_string *e, const struct name *n)
{
        const size_t escaped_char_size = 4; /* strlen("\\123") */
        const size_t omitted_size = 2;      /* strlen("..") */
        const char *sp = n->data;
        const char *ep = sp + n->nbytes;
        bool overflow = false;
        assert(sp <= ep);
        assert(omitted_size <= sizeof(e->small));
        assert(sizeof(e->small) <= max_escaped_len);
        assert(max_escaped_len <= INT_MAX);

        const char *p;
        size_t escaped_len = 0;
        for (p = sp; p < ep; p++) {
                unsigned int needed = 1;
                if (need_escape(*p)) {
                        needed = escaped_char_size;
                }
                if (max_escaped_len - escaped_len < needed) {
                        overflow = true;
                        escaped_len = max_escaped_len; /* upper bound */
                        break;
                }
                escaped_len += needed;
        }
        assert(escaped_len <= max_escaped_len);
        e->orig = sp;
        e->escaped_len = (uint32_t)escaped_len;
        if (!overflow && escaped_len == ep - sp) {
                /* no need to escape. use the original string as it is. */
                e->escaped = (char *)e->orig; /* discard const */
        } else {
                unsigned int lim = max_escaped_len;
                if (!overflow && escaped_len <= sizeof(e->small)) {
                        assert(!overflow);
                        e->escaped = e->small;
                } else {
                        e->escaped = malloc(escaped_len);
                        if (e->escaped == NULL) {
                                e->escaped = e->small;
                                lim = sizeof(e->small) - omitted_size;
                        } else if (overflow) {
                                lim = max_escaped_len - omitted_size;
                        }
                }
                char *dp = e->escaped;
                const char *dsp = dp;
                for (p = sp; p < ep; p++) {
                        char ch = *p;
                        if (need_escape(ch)) {
                                if (dp + escaped_char_size - dsp > lim) {
                                        break;
                                }
                                *dp++ = '\\';
                                *dp++ = oct(((uint8_t)ch >> 6) & 0x07);
                                *dp++ = oct(((uint8_t)ch >> 3) & 0x07);
                                *dp++ = oct((uint8_t)ch & 0x07);
                        } else {
                                if (dp + 1 - dsp > lim) {
                                        break;
                                }
                                *dp++ = ch;
                        }
                }
                assert(dp - dsp <= lim);
                /* add ".." if we omitted something */
                if (p < ep) {
                        assert(dp - dsp + omitted_size <= max_escaped_len);
                        *dp++ = '.';
                        *dp++ = '.';
                        assert(dp - e->escaped <= max_escaped_len);
                        e->escaped_len = (int)(dp - e->escaped);
                }
        }
}

void
escaped_string_clear(struct escaped_string *e)
{
        if (e->escaped != e->orig && e->escaped != e->small) {
                free(e->escaped);
        }
}

void
set_max_escaped_len(int maxlen)
{
        assert(sizeof(((struct escaped_string *)0)->small) <= maxlen);
        max_escaped_len = maxlen;
}
