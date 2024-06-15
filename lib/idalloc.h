
#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "vec.h"

struct mem_context;

struct idalloc {
        VEC(, void *) vec;
        uint32_t base;
        uint32_t maxid;
};

__BEGIN_EXTERN_C

void idalloc_init(struct idalloc *ida, uint32_t minid, uint32_t maxid);
void idalloc_destroy(struct idalloc *ida, struct mem_context *mctx);

int idalloc_alloc(struct idalloc *ida, uint32_t *idp,
                  struct mem_context *mctx);
void idalloc_free(struct idalloc *ida, uint32_t id, struct mem_context *mctx);
bool idalloc_test(struct idalloc *ida, uint32_t id);

void idalloc_set_user(struct idalloc *ida, uint32_t id, void *user_data);
void *idalloc_get_user(struct idalloc *ida, uint32_t id);

__END_EXTERN_C
