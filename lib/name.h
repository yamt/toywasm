#include "type.h"

struct naming {
        uint32_t idx;
        uint32_t offset;
};

struct namemap {
        uint32_t nentries;
        struct naming *entries;
};

struct nametable {
        const struct module *module;
        struct name module_name;
        struct namemap funcs;
};

void nametable_init(struct nametable *table);
void nametable_clear(struct nametable *table);

void nametable_lookup_func(struct nametable *table, const struct module *m,
                           uint32_t funcidx, struct name *name);
void nametable_lookup_module(struct nametable *table, const struct module *m,
                             struct name *name);
