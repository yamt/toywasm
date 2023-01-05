#include <stddef.h>

#include "type.h"

struct host_func {
        struct name name;
        const char *type;
        host_func_t func;
};

#define HOST_FUNC_CONVERT_PARAMS(FT, PARAMS)                                  \
        struct val converted_params[(FT)->parameter.ntypes];                  \
        vals_from_cells(converted_params, (PARAMS), &(FT)->parameter);
#define HOST_FUNC_PARAM(FT, PARAMS, IDX, TYPE) converted_params[IDX].u.TYPE

#define HOST_FUNC_RESULT_SET(FT, RESULTS, IDX, TYPE, V)                       \
        do {                                                                  \
                struct val tmp;                                               \
                tmp.u.TYPE = V;                                               \
                uint32_t csz;                                                 \
                uint32_t cidx =                                               \
                        resulttype_cellidx(&(FT)->result, (IDX), &csz);       \
                val_to_cells(&tmp, &(RESULTS)[cidx], csz);                    \
        } while (0)

struct host_instance {
        int dummy;
};

struct host_module {
        const struct name *module_name;
        const struct host_func *funcs;
        size_t nfuncs;
};

struct import_object;
int import_object_create_for_host_funcs(const struct host_module *hm, size_t n,
                                        struct host_instance *hi,
                                        struct import_object **impp);

void host_func_dump_params(const struct functype *ft,
                           const struct cell *params);
