#include <stdint.h>

#include "list.h"
#include "type.h"

enum symtype {
        SYM_TYPE_FUNC,
        SYM_TYPE_MEM,
};

struct dyld_plt {
        const struct funcinst *finst;
        const struct name *sym;
        struct dyld_object *refobj;
        struct dyld *dyld;
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

        LIST_ENTRY(struct dyld_object) q;
};

struct dyld {
        struct import_object *base_import_obj;
        struct import_object *shared_import_obj;

        uint32_t memory_base;
        uint32_t table_base;

        struct memtype mt;
        struct meminst *meminst;

        struct tabletype tt;
        struct tableinst *tableinst;

        struct globalinst stack_pointer;
        struct globalinst heap_base;
        struct globalinst heap_end;

        LIST_HEAD(struct dyld_object) objs;
};

void dyld_init(struct dyld *d);
void dyld_clear(struct dyld *d);
int dyld_load_main_object_from_file(struct dyld *d, const char *name);
struct instance *dyld_main_object_instance(struct dyld *d);

const struct name *dyld_object_name(struct dyld_object *obj);
int dyld_resolve_symbol(struct dyld *d, struct dyld_object *refobj,
                        enum symtype symtype, const struct name *sym,
                        uint32_t *resultp);
