#include <stdbool.h>

#include "list.h"
#include "mem.h"
#include "type.h"

struct dyld;

enum symtype {
        SYM_TYPE_FUNC,
        SYM_TYPE_MEM,
};

struct dyld_plt {
        const struct funcinst *finst;
        const struct name *sym;
        struct dyld_object *refobj;
        struct funcinst pltfi;
};

struct dyld_object {
        const struct name *name;

        uint32_t memory_base;
        uint32_t table_base;
        uint32_t table_export_base;

        uint32_t nexports; /* # of reserved entries from table_export_base */

        struct globalinst memory_base_global;
        struct globalinst table_base_global;

        uint32_t ngots;
        struct globalinst *gots;
        uint32_t nplts;
        struct dyld_plt *plts;
        struct import_object *local_import_obj;

        const uint8_t *bin;
        size_t binsz;
        struct module *module;
        struct instance *instance;

        struct mem_context module_mctx;
        struct mem_context instance_mctx;

        struct dyld *dyld;
        LIST_ENTRY(struct dyld_object) q;

        /* for tsort */
        bool visited;
        LIST_ENTRY(struct dyld_object) tq;
};

struct dyld_dynamic_object {
        struct name name;
        struct dyld_object *obj;
};

int dyld_resolve_dependencies(struct dyld *d, struct dyld_object *obj,
                              bool bindnow);
int dyld_execute_all_init_funcs(struct dyld *d, struct dyld_object *start);

int dyld_resolve_symbol(struct dyld_object *refobj, enum symtype symtype,
                        const struct name *sym, uint32_t *resultp);
int dyld_resolve_symbol_in_obj(struct dyld_object *refobj,
                               struct dyld_object *obj, enum symtype symtype,
                               const struct name *sym, uint32_t *resultp);
int dyld_search_and_load_object_from_file(struct dyld *d,
                                          const struct name *name,
                                          struct dyld_object **objp);
