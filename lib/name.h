#include "mem.h"
#include "type.h"

__BEGIN_EXTERN_C

struct naming;

struct namemap {
        uint32_t nentries;
        struct naming *entries;
};

struct nametable {
        const struct module *module;
        struct name module_name;
        struct namemap funcs;
        struct mem_context mctx;
};

void nametable_init(struct nametable *table);
void nametable_clear(struct nametable *table);

/*
 * these functions don't return errors.
 * on errors, they just fill the struct name with a string like "unknown".
 */
void nametable_lookup_func(struct nametable *table, const struct module *m,
                           uint32_t funcidx, struct name *name);
void nametable_lookup_module(struct nametable *table, const struct module *m,
                             struct name *name);

__END_EXTERN_C
