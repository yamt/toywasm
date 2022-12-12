
#include <stdbool.h>
#include <stdint.h>

#include "vec.h"

struct idalloc {
        VEC(, void *) vec;
        uint32_t maxid;
};

void idalloc_init(struct idalloc *ida, uint32_t maxid);
void idalloc_destroy(struct idalloc *ida);

int idalloc_alloc(struct idalloc *ida, uint32_t *idp);
void idalloc_free(struct idalloc *ida, uint32_t id);
bool idalloc_test(struct idalloc *ida, uint32_t id);

void idalloc_set_user(struct idalloc *ida, uint32_t id, void *user_data);
void *idalloc_get_user(struct idalloc *ida, uint32_t id);
