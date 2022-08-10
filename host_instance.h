#include <stddef.h>

#include "type.h"

struct host_func {
        struct name name;
        const char *type;
        host_func_t func;
};

struct host_instance {
        int dummy;
};

struct import_object;
int import_object_create_for_host_funcs(const struct name *module_name,
                                        const struct host_func *funcs,
                                        size_t nfuncs,
                                        struct host_instance *hi,
                                        struct import_object **impp);
