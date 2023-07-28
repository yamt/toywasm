#include <stdint.h>

#include "list.h"
#include "toywasm_config.h"
#include "type.h"
#include "vec.h"

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

        struct dyld *dyld;
        LIST_ENTRY(struct dyld_object) q;
};

struct dyld_dynamic_object {
        struct name name;
        struct dyld_object *obj;
};

struct dyld_options {
        struct import_object *base_import_obj;

        /*
         * stack size.
         * something similar to wasm-ld's "-z stack-size=xxx" option.
         */
        uint32_t stack_size;

        /*
         * library search path.
         * something similar to LD_LIBRARY_PATH.
         */
        unsigned int npaths;
        const char *const *paths;

        /*
         * disable lazy plt binding.
         * something similar to LD_BIND_NOW.
         */
        bool bindnow;
};

struct dyld {
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

        struct dyld_options opts;

#if defined(TOYWASM_ENABLE_DYLD_DLFCN)
        VEC(, struct dyld_dynamic_object) dynobjs;
#endif
};

struct import_object;

void dyld_init(struct dyld *d);
void dyld_clear(struct dyld *d);
int dyld_load(struct dyld *d, const char *filename);
struct instance *dyld_main_object_instance(struct dyld *d);
void dyld_options_set_defaults(struct dyld_options *opts);
int import_object_create_for_dyld(struct dyld *d, struct import_object **impp);
