#include <stdio.h>
#include <stdlib.h>

#include "jsonutil.h"

void
json_fatal(void)
{
        fprintf(stderr, "json error\n");
        exit(1);
}

static void
json_fatal_error(const json_error_t *e)
{
        fprintf(stderr,
                "json error\ntext: %s\nsource: %s\nline: %d\ncolumn: "
                "%d\nposition %d\n",
                e->text, e->source, e->line, e->column, e->position);
        exit(1);
}

void
json_object_set_u32(json_t *o, const char *key, uint32_t u32)
{
        json_t *v = json_integer(u32);
        if (v == NULL) {
                json_fatal();
        }
        if (json_object_set_new(o, key, v)) {
                json_fatal();
        }
}

void
json_pack_and_append(json_t *a, const char *fmt, ...)
{
        json_error_t error;
        va_list ap;
        va_start(ap, fmt);
        json_t *e = json_vpack_ex(&error, 0, fmt, ap);
        va_end(ap);
        if (e == NULL) {
                json_fatal_error(&error);
        }
        if (json_array_append_new(a, e)) {
                json_fatal();
        }
}

json_t *
json_object_set_array(json_t *o, const char *key)
{
        json_t *a = json_array();
        if (a == NULL) {
                json_fatal();
        }
        if (json_object_set_new(o, key, a)) {
                json_fatal();
        }
        return a;
}
