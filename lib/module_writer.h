#include "platform.h"

__BEGIN_EXTERN_C

struct module;
int module_write(const char *filename, const struct module *m);

__END_EXTERN_C
