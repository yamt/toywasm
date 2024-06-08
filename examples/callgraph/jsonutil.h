#include <stdint.h>

#include <jansson.h>

void json_fatal(void);
void json_object_set_u32(json_t *o, const char *key, uint32_t u32);
void json_pack_and_append(json_t *a, const char *fmt, ...);
json_t *json_object_set_array(json_t *o, const char *key);
