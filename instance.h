#include <stdint.h>

struct module;
struct instance;
struct exec_context;
struct resulttype;
struct val;
struct import_object;
struct report;
struct name;

int instance_create(struct module *m, struct instance **instp,
                    const struct import_object *imports,
                    struct report *report);
/*
 * Note: If you have multiple instances linked together
 * with import/export, usually the only safe way to destroy those
 * instances is to destroy them together at once. Note that import/export
 * is not necessarily a one-way dependency. Because funcref values,
 * which are implemented as bare host pointers in this engine, can be
 * freely passed among instances, linked instances can have references
 * each other.
 */
void instance_destroy(struct instance *inst);

int instance_execute_func(struct exec_context *ctx, const struct name *name,
                          const struct resulttype *paramtype,
                          const struct resulttype *resulttype,
                          const struct val *params, struct val *results);

int import_object_create_for_exports(struct instance *inst,
                                     const struct name *module_name,
                                     struct import_object **resultp);
void import_object_destroy(struct import_object *export);
int import_object_alloc(uint32_t nentries, struct import_object **resultp);
