#include <stddef.h>

#include "type.h"

struct host_func {
        struct name name;
        const char *type;
        host_func_t func;
};

#define HOST_FUNC_PARAM(FT, PARAMS, IDX, TYPE) PARAMS[IDX].u.TYPE
#define HOST_FUNC_RESULT_SET(FT, RESULTS, IDX, TYPE, V) RESULTS[IDX].u.TYPE = V

struct host_instance {
        int dummy;
};

struct import_object;
int import_object_create_for_host_funcs(const struct name *module_name,
                                        const struct host_func *funcs,
                                        size_t nfuncs,
                                        struct host_instance *hi,
                                        struct import_object **impp);
