#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "type.h"

static int
resulttype_alloc0(uint32_t ntypes, struct resulttype **resultp)
{
        struct resulttype *p;
        size_t bytesize1;
        size_t bytesize;

        bytesize1 = ntypes * sizeof(*p->types);
        if (bytesize1 / ntypes != sizeof(*p->types)) {
                return EOVERFLOW;
        }
        bytesize = sizeof(*p) + bytesize1;
        if (bytesize <= bytesize1) {
                return EOVERFLOW;
        }
        p = malloc(bytesize);
        if (p == NULL) {
                return ENOMEM;
        }
        p->ntypes = ntypes;
        p->types = (void *)p + sizeof(*p);
        p->is_static = false;
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        p->cellidx.cellidxes = NULL;
#endif
        *resultp = p;
        return 0;
}

/*
 * Note: resulttype_alloc/resulttype_free allocates the structure
 * differently from module.c.
 */
int
resulttype_alloc(uint32_t ntypes, const enum valtype *types,
                 struct resulttype **resultp)
{
        struct resulttype *p;
        uint32_t i;
        int ret = resulttype_alloc0(ntypes, &p);
        if (ret != 0) {
                return ret;
        }
        for (i = 0; i < ntypes; i++) {
                p->types[i] = types[i];
        }
        *resultp = p;
        return 0;
}

void
resulttype_free(struct resulttype *p)
{
        if (p == NULL) {
                return;
        }
        if (p->is_static) {
                return;
        }
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        assert(p->cellidx.cellidxes == NULL);
#endif
        free(p);
}

uint32_t
ptr2pc(struct module *m, const uint8_t *p)
{
        assert(p >= m->bin);
        assert(p - m->bin <= UINT32_MAX);
        return p - m->bin;
}

const uint8_t *
pc2ptr(struct module *m, uint32_t pc)
{
        return m->bin + pc;
}
