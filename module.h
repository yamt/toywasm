#include <stdint.h>

struct load_context;
struct module;
int module_create(struct module **mp);
int module_load(struct module *m, const uint8_t *p, const uint8_t *ep,
                struct load_context *ctx);
void module_unload(struct module *m);
void module_destroy(struct module *m);
int module_find_export_func(struct module *m, const char *name,
                            uint32_t *funcidxp);
