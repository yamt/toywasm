struct module;
struct instance;
struct exec_context;
struct resulttype;
struct val;
struct import_object;

int instance_create(struct module *m, struct instance **instp,
                    const struct import_object *imports);
void instance_destroy(struct instance *inst);

int instance_execute_func(struct exec_context *ctx, const char *name,
                          const struct resulttype *paramtype,
                          const struct resulttype *resulttype,
                          const struct val *params, struct val *results);

int import_object_create_for_exports(struct instance *inst,
                                     const char *module_name,
                                     struct import_object **resultp);
void import_object_destroy(struct import_object *export);
