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
        p->types = (void *)(p + 1);
        p->is_static = false;
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        p->cellidx.cellidxes = NULL;
#endif
        *resultp = p;
        return 0;
}

#define DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(QUAL, TYPE)                        \
        DEFINE_TYPES(QUAL, types_##TYPE, TYPE_##TYPE);                        \
        DEFINE_RESULTTYPE(QUAL, rt_##TYPE, &types_##TYPE, 1)

#define HANDLE_TYPE(TYPE)                                                     \
        case TYPE_##TYPE: {                                                   \
                p = &rt_##TYPE;                                               \
                break;                                                        \
        }

/*
 * Note: resulttype_alloc/resulttype_free allocates the structure
 * differently from module.c.
 */
int
resulttype_alloc(uint32_t ntypes, const enum valtype *types,
                 struct resulttype **resultp)
{
        if (ntypes == 1) {
                /* fast path for common cases */
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, i32);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, i64);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, f32);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, f64);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, v128);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, EXNREF);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, FUNCREF);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, EXTERNREF);
                enum valtype t = types[0];
                const struct resulttype *p;
                switch (t) {
                        HANDLE_TYPE(i32)
                        HANDLE_TYPE(i64)
                        HANDLE_TYPE(f32)
                        HANDLE_TYPE(f64)
                        HANDLE_TYPE(v128)
                        HANDLE_TYPE(EXNREF)
                        HANDLE_TYPE(FUNCREF)
                        HANDLE_TYPE(EXTERNREF)
                case TYPE_ANYREF:
                case TYPE_UNKNOWN:
                        p = NULL;
                        assert(false);
                }
                *resultp = (struct resulttype *)p; /* discard const */
                return 0;
        }
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
ptr2pc(const struct module *m, const uint8_t *p)
{
        assert(p >= m->bin);
        assert(p - m->bin <= UINT32_MAX);
        return p - m->bin;
}

const uint8_t *
pc2ptr(const struct module *m, uint32_t pc)
{
        return m->bin + pc;
}
