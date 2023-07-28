#include <stdbool.h>

struct dyld;
struct dyld_object;
struct name;

int dyld_resolve_dependencies(struct dyld *d, struct dyld_object *obj,
                              bool bindnow);
int dyld_execute_all_init_funcs(struct dyld *d, struct dyld_object *start);

int dyld_resolve_symbol(struct dyld_object *refobj, enum symtype symtype,
                        const struct name *sym, uint32_t *resultp);
int dyld_search_and_load_object_from_file(struct dyld *d,
                                          const struct name *name,
                                          struct dyld_object **objp);
