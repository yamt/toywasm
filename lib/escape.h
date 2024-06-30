#include <string.h>

#include "platform.h"

/*
 * we don't bother to support names longer than INT_MAX here because:
 *
 * - this is primarily for logging and error reporting purposes, for which
 *   INT_MAX is already too large.
 *
 * - this is intended to be used to feed printf formats using %.*s, which
 *   takes an int anyway.
 */
struct escaped_string {
        const char *orig;
        char *escaped;
        int escaped_len;
        char small[3 * 4 + 2]; /* eg. \343\201\202.. */
};

#define ECSTR(e) (e)->escaped_len, (e)->escaped

__BEGIN_EXTERN_C

struct name;

void escape_name(struct escaped_string *e, const struct name *n);
void escaped_string_clear(struct escaped_string *e);

__END_EXTERN_C
