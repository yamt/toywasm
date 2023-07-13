#include <stdint.h>

#include "list.h"
#include "type.h"

struct dyld_sym {
        const struct name *name;
        enum importtype type;
        uint32_t idx;
};

struct dyld_plt {
        struct finst *finst;
        struct tableinst *tableinst;
        uint32_t idx_in_table;
};

struct dyld_object {
        uint32_t memory_base;
        uint32_t table_base;

        const uint8_t *bin;
        size_t binsz;
        struct module *module;
        struct instance *instance;

        LIST_ENTRY(struct dyld_object) q;
};

struct dyld {
        uint32_t memory_base;
        uint32_t table_base;

        struct memtype mt;
        struct meminst *meminst;

        struct tabletype tt;
        struct tableinst *tableinst;

        LIST_HEAD(struct dyld_object) objs;
};

#if 0
int dyld_resolve_symbol(struct dyld_object *refobj, enum importtype type,
                        const struct name *name, void **resultp);
#endif
int dyld_load_main_object_from_file(struct dyld *d, const char *name);
