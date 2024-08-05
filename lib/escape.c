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
        const size_t escaped_char_size = 4; /* strlen("\123") */
        const size_t omitted_size = 2;      /* strlen("..") */
        const char *sp = n->data;
        const char *ep = sp + n->nbytes;
        assert(sp <= ep);

        const char *p;
        size_t escaped_len = 0;
        for (p = sp; p < ep; p++) {
                if (need_escape(*p)) {
                        escaped_len += escaped_char_size;
                } else {
                        escaped_len++;
                }
        }
        e->orig = sp;
        e->escaped_len = escaped_len;
        if (escaped_len == ep - sp) {
                e->escaped = (char *)e->orig; /* discard const */
        } else {
                int lim = INT_MAX;
                if (escaped_len <= sizeof(e->small)) {
                        e->escaped = e->small;
                } else {
                        e->escaped = malloc(escaped_len);
                        if (e->escaped == NULL) {
                                e->escaped = e->small;
                                lim = sizeof(e->small) - omitted_size;
                        }
                }
                char *dp = e->escaped;
                const char *dep = dp + lim;
                for (p = sp; p < ep; p++) {
                        char ch = *p;
                        if (need_escape(ch)) {
                                if (dp + escaped_char_size > dep) {
                                        break;
                                }
                                *dp++ = '\\';
                                *dp++ = oct(((uint8_t)ch >> 6) & 0x07);
                                *dp++ = oct(((uint8_t)ch >> 3) & 0x07);
                                *dp++ = oct((uint8_t)ch & 0x07);
                        } else {
                                if (dp + 1 > dep) {
                                        break;
                                }
                                *dp++ = ch;
                        }
                }
                assert(dp <= dep);
                if (lim != INT_MAX) {
                        assert(e->escaped == e->small);
                        assert(dp + omitted_size <=
                               e->small + sizeof(e->small));
                        *dp++ = '.';
                        *dp++ = '.';
                        e->escaped_len = dp - e->escaped;
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
