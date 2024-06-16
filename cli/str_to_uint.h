#include <stddef.h>
#include <stdint.h>

int str_to_uint(const char *s, int base, uintmax_t *resultp);
int str_to_u32(const char *s, int base, uint32_t *resultp);
int str_to_size(const char *s, int base, size_t *resultp);
