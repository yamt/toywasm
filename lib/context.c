#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "mem.h"
#include "type.h"

static int
resulttype_bytesize(uint32_t ntypes, size_t *resultp)
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
        *resultp = bytesize;
        return 0;
}

static int
resulttype_alloc0(struct mem_context *mctx, uint32_t ntypes,
                  struct resulttype **resultp)
{
        size_t bytesize;
        int ret = resulttype_bytesize(ntypes, &bytesize);
        if (ret != 0) {
                return ret;
        }
        struct resulttype *p;
        p = mem_alloc(mctx, bytesize);
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
resulttype_alloc(struct mem_context *mctx, uint32_t ntypes,
                 const enum valtype *types, struct resulttype **resultp)
{
        if (ntypes == 1) {
                /* fast path for common cases */
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, i32);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, i64);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, f32);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, f64);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, v128);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, exnref);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, funcref);
                DEFINE_RESULTTYPE_WITH_SINGLE_TYPE(static const, externref);
                enum valtype t = types[0];
                const struct resulttype *p;
                switch (t) {
                        HANDLE_TYPE(i32)
                        HANDLE_TYPE(i64)
                        HANDLE_TYPE(f32)
                        HANDLE_TYPE(f64)
                        HANDLE_TYPE(v128)
                        HANDLE_TYPE(exnref)
                        HANDLE_TYPE(funcref)
                        HANDLE_TYPE(externref)
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
        int ret = resulttype_alloc0(mctx, ntypes, &p);
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
resulttype_free(struct mem_context *mctx, struct resulttype *p)
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
        size_t bytesize;
        int ret = resulttype_bytesize(p->ntypes, &bytesize);
        assert(ret == 0);
        mem_free(mctx, p, bytesize);
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
