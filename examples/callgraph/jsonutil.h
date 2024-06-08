#include <stdint.h>

#include <jansson.h>

void jsonutil_fatal(void);
void jsonutil_object_set_u32(json_t *o, const char *key, uint32_t u32);
void jsonutil_pack_and_append(json_t *a, const char *fmt, ...);
json_t *jsonutil_object_set_array(json_t *o, const char *key);
