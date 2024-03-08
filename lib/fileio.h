#include "platform.h"

__BEGIN_EXTERN_C

int map_file(const char *filename, void **pp, size_t *szp);
void unmap_file(void *p, size_t sz);

__END_EXTERN_C
