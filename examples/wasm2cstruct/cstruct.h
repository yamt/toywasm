struct module;

int dump_module_as_cstruct(FILE *out, const char *name,
                           const struct module *m);
