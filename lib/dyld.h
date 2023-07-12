#include <stdint.h>

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

        struct dyld_object *next;
};

struct dyld {
        uint32_t memory_base;
        uint32_t table_base;
        struct meminst *meminst;
        struct tableinst *tableinst;
        struct dyld_object *objs;
};

#if 0
int dyld_resolve_symbol(struct dyld_object *refobj, enum importtype type,
                        const struct name *name, void **resultp);
#endif
