#include <stdint.h>

#include "platform.h"

struct load_context;
struct module;
struct name;
struct mem_context;

__BEGIN_EXTERN_C

int module_create(struct module **mp, const uint8_t *p, const uint8_t *ep,
                  struct load_context *ctx);
void module_destroy(struct mem_context *mctx, struct module *m);

/*
 * note: unlike import names, export names are unique within a module.
 * cf. https://www.w3.org/TR/wasm-core-2/#exports%E2%91%A0
 */
int module_find_export(const struct module *m, const struct name *name,
                       uint32_t type, uint32_t *idxp);
void module_print_stats(const struct module *m);

__END_EXTERN_C
