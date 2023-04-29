/*
 * https://webassembly.github.io/spec/core/binary/modules.html#binary-module
 * https://webassembly.github.io/spec/core/binary/modules.html#sections
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "decode.h"
#include "expr.h"
#include "leb128.h"
#include "load_context.h"
#include "module.h"
#include "nbio.h"
#include "report.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

struct section {
        uint8_t id;
        uint32_t size;
        const uint8_t *data;
};

static int
section_load(struct section *s, const uint8_t **pp, const uint8_t *ep)
{
        const uint8_t *p = *pp;
        uint8_t v;
        int ret;

        ret = read_u8(&p, ep, &v);
        if (ret != 0) {
                goto fail;
        }
        s->id = v;

        uint32_t v32;
        ret = read_leb_u32(&p, ep, &v32);
        if (ret != 0) {
                goto fail;
        }
        s->size = v32;

        if (p + s->size > ep) {
                ret = EINVAL;
                goto fail;
        }
        s->data = p;
        p += s->size;
        *pp = p;

        return 0;
fail:
        return ret;
}

#if defined(TOYWASM_ENABLE_TRACING)
static const char *
valtype_str(enum valtype vt)
{
        static const char *types[] = {
                [TYPE_i32] = "i32",
                [TYPE_i64] = "i64",
                [TYPE_f32] = "f32",
                [TYPE_f64] = "f64",
                [TYPE_v128] = "v128",
                [TYPE_FUNCREF] = "funcref",
                [TYPE_EXTERNREF] = "externref",
        };
        return types[vt];
}
#endif

static int
read_valtype(const uint8_t **pp, const uint8_t *ep, enum valtype *vt)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        if (!is_valtype(u8)) {
                ret = EINVAL;
                goto fail;
        }
        *vt = u8;
        *pp = p;
        return 0;
fail:
        return ret;
}

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
static int
populate_resulttype_cellidx(struct resulttype *rt)
{
        uint16_t *idxes = calloc(rt->ntypes + 1, sizeof(*idxes));
        int ret;
        if (idxes == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        uint32_t i;
        uint32_t off = 0;
        for (i = 0; i < rt->ntypes; i++) {
                uint32_t csz = valtype_cellsize(rt->types[i]);
                if (UINT16_MAX - off < csz) {
                        ret = EOVERFLOW; /* implementation limit */
                        goto fail;
                }
                off += csz;
                idxes[i + 1] = off;
        }
        rt->cellidx.cellidxes = idxes;
        return 0;
fail:
        free(idxes);
        return ret;
}
#endif

static int
read_resulttype(const uint8_t **pp, const uint8_t *ep, struct resulttype *rt,
                const struct load_context *ctx, bool populate_idx)
{
        const uint8_t *p = *pp;
        int ret;

        rt->ntypes = 0;
        rt->types = NULL;
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        rt->cellidx.cellidxes = NULL;
#endif
        ret = read_vec(&p, ep, sizeof(*rt->types),
                       (read_elem_func_t)read_valtype, NULL, &rt->ntypes,
                       (void *)&rt->types);
        if (ret != 0) {
                goto fail;
        }
        rt->is_static = true;
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        if (populate_idx && rt->ntypes > 0 &&
            ctx->options.generate_resulttype_cellidx) {
                ret = populate_resulttype_cellidx(rt);
                if (ret != 0) {
                        /* this failure is not critical. let's ignore. */
                        xlog_error("populate_resulttype_cellidx failed with "
                                   "%d. It can cause very slow execution.",
                                   ret);
                }
        }
#endif
        *pp = p;
        return 0;
fail:
        return ret;
}

void
clear_resulttype(struct resulttype *rt)
{
        free(rt->types);
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        free(rt->cellidx.cellidxes);
#endif
}

static int
read_functype(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
              struct functype *ft, void *vp)
{
        struct load_context *ctx = vp;
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        if (u8 != 0x60) {
                ret = EINVAL;
                goto fail;
        }

        ret = read_resulttype(&p, ep, &ft->parameter, ctx, true);
        if (ret != 0) {
                goto fail;
        }

        /*
         * Note: unlike parameters, results doesn't need fast access
         * for local.get. while resulttype_cellsize still matters,
         * functions with many results are rare.
         */
        ret = read_resulttype(&p, ep, &ft->result, ctx, false);
        if (ret != 0) {
                clear_resulttype(&ft->parameter);
                goto fail;
        }

        *pp = p;
fail:
        return ret;
}

void
clear_functype(struct functype *ft)
{
        clear_resulttype(&ft->parameter);
        clear_resulttype(&ft->result);
}

#if 0
static void
print_resulttype(const struct resulttype *rt)
{
        const char *sep = "";
        uint32_t i;

        xlog_printf_raw("[");
        for (i = 0; i < rt->ntypes; i++) {
                xlog_printf_raw("%s%s", sep, valtype_str(rt->types[i]));
                sep = ", ";
        }
        xlog_printf_raw("]");
}
#endif

static void
print_functype(uint32_t i, const struct functype *ft)
{
#if 0
        xlog_printf("func type [%" PRIu32 "]: ", i);
        print_resulttype(&ft->parameter);
        xlog_printf_raw(" -> ");
        print_resulttype(&ft->result);
        xlog_printf_raw("\n");
#endif
}

static int
read_limits(const uint8_t **pp, const uint8_t *ep, struct limits *lim,
            uint8_t *extra_memory_flagsp, uint32_t typemax)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        bool has_max;
        if (u8 & 0x01) {
                has_max = true;
        } else {
                has_max = false;
                if ((u8 & MEMTYPE_FLAG_SHARED) != 0) {
                        /* shared memory should have max */
                        ret = EINVAL;
                        goto fail;
                }
        }
        u8 &= ~0x01;
        if (extra_memory_flagsp != NULL) {
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                const uint8_t mask = MEMTYPE_FLAG_SHARED;
#else
                const uint8_t mask = 0;
#endif
                *extra_memory_flagsp = u8 & mask;
                u8 &= ~mask;
        }
        if (u8 != 0) {
                ret = EINVAL;
                goto fail;
        }

        ret = read_leb_u32(&p, ep, &lim->min);
        if (ret != 0) {
                goto fail;
        }
        if (has_max) {
                ret = read_leb_u32(&p, ep, &lim->max);
                if (ret != 0) {
                        goto fail;
                }
                if (typemax < lim->max) {
                        ret = EOVERFLOW;
                        goto fail;
                }
        } else {
                lim->max = UINT32_MAX;
                if (typemax < lim->max) {
                        lim->max = typemax;
                }
        }
        if (typemax < lim->min) {
                ret = EOVERFLOW;
                goto fail;
        }
        if (lim->min > lim->max) {
                ret = EINVAL;
                goto fail;
        }
        *pp = p;
fail:
        return ret;
}

static int
read_memtype(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
             struct memtype *mt, void *vctx)
{
        return read_limits(pp, ep, &mt->lim, &mt->flags, WASM_MAX_PAGES);
}

static int
read_globaltype(const uint8_t **pp, const uint8_t *ep, struct globaltype *g)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_valtype(&p, ep, &g->t);
        if (ret != 0) {
                goto fail;
        }

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        switch (u8) {
        case 0x01: /* var */
                g->mut = GLOBAL_VAR;
                break;
        case 0x00: /* const */
                g->mut = GLOBAL_CONST;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }

        *pp = p;
fail:
        return ret;
}

static int
check_utf8(const uint8_t *p, const uint8_t *ep)
{
        int ret;
        while (p < ep) {
                uint32_t c;
                uint32_t min;
                uint32_t max;
                uint8_t b;
                unsigned int n;
                ret = read_u8(&p, ep, &b);
                if (ret != 0) {
                        goto fail;
                }
                if (b < 0x80) {
                        n = 1;
                        min = 0;
                        max = 0x7f;
                        c = b;
                } else if (b < 0xc0) {
                        ret = EINVAL;
                        goto fail;
                } else if (b < 0xe0) {
                        n = 2;
                        min = 0x80;
                        max = 0x7ff;
                        c = b - 0xc0;
                } else if (b < 0xf0) {
                        n = 3;
                        min = 0x800;
                        max = 0xffff;
                        c = b - 0xe0;
                } else if (b < 0xf8) {
                        n = 4;
                        min = 0x10000;
                        max = 0x10ffff;
                        c = b - 0xf0;
                } else {
                        ret = EINVAL;
                        goto fail;
                }
                unsigned int i;
                for (i = 1; i < n; i++) {
                        ret = read_u8(&p, ep, &b);
                        if (ret != 0) {
                                goto fail;
                        }
                        if (b < 0x80 || b >= 0xc0) {
                                ret = EINVAL;
                                goto fail;
                        }
                        c <<= 6;
                        c += b - 0x80;
                }
                if (c < min || max < c) {
                        ret = EINVAL;
                        goto fail;
                }
                /* reject surrogate halves */
                if (0xd800 <= c && c < 0xe000) {
                        ret = EINVAL;
                        goto fail;
                }
        }
        return 0;
fail:
        return ret;
}

static int
read_name(const uint8_t **pp, const uint8_t *ep, struct name *namep)
{
        const uint8_t *p = *pp;
        char *name = NULL;
        uint32_t vec_count;
        int ret;

        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }

        if (vec_count > ep - p) {
                ret = E2BIG;
                goto fail;
        }

        ret = check_utf8(p, p + vec_count);
        if (ret != 0) {
                goto fail;
        }

        namep->nbytes = vec_count;
        namep->data = (const char *)p;
        p += vec_count;
        *pp = p;
        return 0;
fail:
        free(name);
        return ret;
}

void
set_name_cstr(struct name *name, char *cstr)
{
        name->nbytes = strlen(cstr);
        name->data = cstr;
}

void
clear_name(struct name *name)
{
}

static int
read_tabletype(const uint8_t **pp, const uint8_t *ep, struct tabletype *tt)
{
        const uint8_t *p = *pp;
        int ret;

        ret = read_valtype(&p, ep, &tt->et);
        if (ret != 0) {
                goto fail;
        }
        if (!is_reftype(tt->et)) {
                ret = EINVAL;
                goto fail;
        }
        ret = read_limits(&p, ep, &tt->lim, NULL, UINT32_MAX);
        if (ret != 0) {
                goto fail;
        }
        *pp = p;
        return 0;
fail:
        return ret;
}

static int
read_importdesc(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
                struct importdesc *desc, struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        switch (u8) {
        case 0x00: /* typeidx */
                ret = read_leb_u32(&p, ep, &desc->u.typeidx);
                if (ret != 0) {
                        goto fail;
                }
                if (desc->u.typeidx >= ctx->module->ntypes) {
                        ret = EINVAL;
                        goto fail;
                }
                m->nimportedfuncs++;
                break;
        case 0x01: /* tabletype */
                ret = read_tabletype(&p, ep, &desc->u.tabletype);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedtables++;
                break;
        case 0x02: /* memtype */
                ret = read_memtype(&p, ep, 0, &desc->u.memtype, ctx);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedmems++;
                break;
        case 0x03: /* globaltype */
                ret = read_globaltype(&p, ep, &desc->u.globaltype);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedglobals++;
                break;
        default:
                xlog_trace("unknown import desc type %u", u8);
                ret = EINVAL;
                goto fail;
        }
        desc->type = u8;
        *pp = p;
        return 0;
fail:
        return ret;
}

static int
read_exportdesc(const uint8_t **pp, const uint8_t *ep, struct exportdesc *desc,
                struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        switch (u8) {
        case 0x00: /* typeidx */
        case 0x01: /* tableidx */
        case 0x02: /* memidx */
        case 0x03: /* globalidx */
                desc->type = u8;
                ret = read_leb_u32(&p, ep, &desc->idx);
                if (ret != 0) {
                        goto fail;
                }
                break;
        default:
                xlog_trace("unknown export desc type %u", u8);
                ret = EINVAL;
                goto fail;
        }
        switch (desc->type) {
        case EXPORT_FUNC:
                if (desc->idx >= m->nimportedfuncs + m->nfuncs) {
                        xlog_trace("export idx (%" PRIu32
                                   ") out of range for type %u",
                                   desc->idx, u8);
                        ret = EINVAL;
                        goto fail;
                }
                bitmap_set(&ctx->refs, desc->idx);
                break;
        case EXPORT_TABLE:
                if (desc->idx >= m->nimportedtables + m->ntables) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXPORT_MEMORY:
                if (desc->idx >= m->nimportedmems + m->nmems) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXPORT_GLOBAL:
                if (desc->idx >= m->nimportedglobals + m->nglobals) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        }
        *pp = p;
        return 0;
fail:
        return ret;
}

static int
read_import(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct import *im, void *ctx)
{
        const uint8_t *p = *pp;
        int ret;

        memset(im, 0, sizeof(*im));
        ret = read_name(&p, ep, &im->module_name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_name(&p, ep, &im->name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_importdesc(&p, ep, idx, &im->desc, ctx);
        if (ret != 0) {
                goto fail;
        }

        *pp = p;
        return 0;
fail:
        return ret;
}

static void
clear_import(struct import *im)
{
        clear_name(&im->module_name);
        clear_name(&im->name);
}

static int
read_export(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct export *ex, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        int ret;

        memset(ex, 0, sizeof(*ex));
        ret = read_name(&p, ep, &ex->name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_exportdesc(&p, ep, &ex->desc, ctx);
        if (ret != 0) {
                goto fail;
        }

        *pp = p;
        return 0;
fail:
        return ret;
}

static void
clear_export(struct export *ex)
{
        clear_name(&ex->name);
}

static void
print_import(const struct import *im)
{
        xlog_trace("import module %.*s name %.*s type %u",
                   CSTR(&im->module_name), CSTR(&im->name), im->desc.type);
}

static void
print_export(const struct export *ex)
{
        xlog_trace("export name %.*s type %u idx %" PRIu32, CSTR(&ex->name),
                   ex->desc.type, ex->desc.idx);
}

static int
read_type_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(*m->types), read_functype,
                                clear_functype, ctx, &m->ntypes, &m->types);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->ntypes; i++) {
                print_functype(i, &m->types[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_import_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(struct import), read_import,
                                clear_import, ctx, &m->nimports, &m->imports);
        if (ret != 0) {
                goto fail;
        }
        if (m->nimportedfuncs > 0) {
                ret = bitmap_alloc(&ctx->refs, m->nimportedfuncs);
                if (ret != 0) {
                        goto fail;
                }
        }

        uint32_t i;
        for (i = 0; i < m->nimports; i++) {
                print_import(&m->imports[i]);
        }
        ret = 0;
        *pp = p;
fail:
        return ret;
}

#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
static int
populate_localtype_cellidx(struct localtype *lt)
{
        uint16_t *idxes = calloc(lt->nlocals + 1, sizeof(*idxes));
        int ret;
        if (idxes == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        uint32_t i;
        uint32_t off = 0;
        const struct localchunk *chunk = lt->localchunks - 1;
        uint32_t csz;
        uint32_t n = 0;
        for (i = 0; i < lt->nlocals; i++) {
                if (n == 0) {
                        chunk++;
                        csz = valtype_cellsize(chunk->type);
                        n = chunk->n;
                        assert(csz > 0);
                        assert(n > 0);
                }
                assert(chunk >= lt->localchunks);
                assert(chunk < lt->localchunks + lt->nlocalchunks);
                if (UINT16_MAX - off < csz) {
                        ret = EOVERFLOW; /* implementation limit */
                        goto fail;
                }
                off += csz;
                idxes[i + 1] = off;
                n--;
        }
        lt->cellidx.cellidxes = idxes;
        return 0;
fail:
        free(idxes);
        return ret;
}
#endif

static int
read_locals(const uint8_t **pp, const uint8_t *ep, struct func *func,
            const struct load_context *ctx)
{
        const uint8_t *p = *pp;
        uint32_t vec_count;
        int ret;
        struct localchunk *chunks = NULL;
        uint32_t nlocals = 0;

        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }
        struct localtype *lt = &func->localtype;
        lt->nlocalchunks = vec_count;
        if (vec_count > 0) {
                chunks = calloc(vec_count, sizeof(*chunks));
        }

        uint32_t i;
        struct localchunk *chunk = chunks;
        for (i = 0; i < vec_count; i++) {
                uint32_t count;
                uint8_t u8;

                ret = read_leb_u32(&p, ep, &count);
                if (ret != 0) {
                        goto fail;
                }
                nlocals += count;
                if (UINT32_MAX - nlocals < count) {
                        ret = E2BIG;
                        goto fail;
                }
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        goto fail;
                }
                if (!is_valtype(u8)) {
                        ret = EINVAL;
                        goto fail;
                }
                if (count == 0) {
                        /*
                         * local count can be 0.
                         * cf. https://github.com/WebAssembly/spec/pull/980
                         */
                        continue;
                }
                chunk->n = count;
                chunk->type = u8;
                chunk++;
        }
        lt->localchunks = chunks;
        lt->nlocals = nlocals;
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        if (lt->nlocals > 0 && ctx->options.generate_localtype_cellidx) {
                ret = populate_localtype_cellidx(lt);
                if (ret != 0) {
                        /* this failure is not critical. let's ignore. */
                        xlog_error("populate_localtype_cellidx failed with "
                                   "%d. It can cause very slow execution.",
                                   ret);
                        lt->localchunks = NULL;
                }
        }
#endif
        *pp = p;
        return 0;
fail:
        free(chunks);
        return ret;
}

static int
read_func(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
          struct func *func, void *vp)
{
        struct load_context *ctx = vp;
        struct module *m = ctx->module;
        struct localtype *lt = &func->localtype;
        const uint8_t *p = *pp;
        uint32_t size;
        int ret;

        lt->localchunks = NULL;
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        lt->cellidx.cellidxes = NULL;
#endif
        uint32_t funcidx = m->nimportedfuncs + idx;
        if (funcidx >= m->nimportedfuncs + m->nfuncs) {
                xlog_trace("read_func: funcidx out of range");
                ret = EINVAL;
                goto fail;
        }
        uint32_t functypeidx = m->functypeidxes[idx];
        assert(functypeidx < m->ntypes);
        struct functype *ft = &m->types[functypeidx];
        xlog_trace("reading func [%" PRIu32 "] (code [%" PRIu32
                   "], type[%" PRIu32 "])",
                   funcidx, idx, functypeidx);

        /* code */
        ret = read_leb_u32(&p, ep, &size);
        if (ret != 0) {
                goto fail;
        }

        const uint8_t *cep = p + size;
        ret = read_locals(&p, cep, func, ctx);
        if (ret != 0) {
                goto fail;
        }
        ret = read_expr(&p, cep, &func->e, lt->nlocals, lt->localchunks,
                        &ft->parameter, &ft->result, ctx);
        if (ret != 0) {
                goto fail;
        }
        if (p != cep) {
                xlog_trace("func has %zu trailing bytes", cep - p);
                ret = EINVAL;
                goto fail;
        }
        *pp = p;
        return 0;
fail:
        free(lt->localchunks);
        return ret;
}

static void
clear_expr_exec_info(struct expr_exec_info *ei)
{
        free(ei->jumps);
#if defined(TOYWASM_USE_SMALL_CELLS)
        free(ei->type_annotations.types);
#endif
}

static void
clear_expr(struct expr *expr)
{
        clear_expr_exec_info(&expr->ei);
}

static void
clear_func(struct func *func)
{
        struct localtype *lt = &func->localtype;
        free(lt->localchunks);
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        free(lt->cellidx.cellidxes);
#endif
        clear_expr(&func->e);
}

static int
read_function_section(const uint8_t **pp, const uint8_t *ep,
                      struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_u32(&p, ep, &m->nfuncs, &m->functypeidxes);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                if (m->functypeidxes[i] >= m->ntypes) {
                        xlog_trace("func type idx out of range");
                        return EINVAL;
                }
                xlog_trace("func [%" PRIu32 "] typeidx %" PRIu32, i,
                           m->functypeidxes[i]);
        }
        /*
         * Note: if nimportedfuncs > 0,
         * ctx->refs is already allocated by read_import_section.
         */
        if (m->nfuncs > 0) {
                if (m->nimportedfuncs > 0) {
                        bitmap_free(&ctx->refs);
                }
                ret = bitmap_alloc(&ctx->refs, m->nimportedfuncs + m->nfuncs);
                if (ret != 0) {
                        goto fail;
                }
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_table_section(const uint8_t **pp, const uint8_t *ep,
                   struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec(&p, ep, sizeof(*m->tables),
                       (read_elem_func_t)read_tabletype, NULL, &m->ntables,
                       (void *)&m->tables);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->ntables; i++) {
                xlog_trace("table [%" PRIu32 "] %s %" PRIu32 " - %" PRIu32, i,
                           valtype_str(m->tables[i].et), m->tables[i].lim.min,
                           m->tables[i].lim.max);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_memory_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(*m->mems), read_memtype, NULL,
                                ctx, &m->nmems, &m->mems);
        if (ret != 0) {
                xlog_trace("failed to load mems with %d", ret);
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nmems; i++) {
                xlog_trace("mem [%" PRIu32 "] %" PRIu32 " - %" PRIu32, i,
                           m->mems[i].lim.min, m->mems[i].lim.max);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_global(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct global *g, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        int ret;

        ret = read_globaltype(&p, ep, &g->type);
        if (ret != 0) {
                goto fail;
        }
        ret = read_const_expr(&p, ep, &g->init, g->type.t, ctx);
        if (ret != 0) {
                goto fail;
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static void
clear_global(struct global *g)
{
        clear_expr(&g->init);
}

static int
read_global_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(*m->globals), read_global,
                                clear_global, ctx, &m->nglobals, &m->globals);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nglobals; i++) {
                xlog_trace("global [%" PRIu32 "] %s %s", i,
                           valtype_str(m->globals[i].type.t),
                           m->globals[i].type.mut ? "var" : "const");
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_export_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(struct export), read_export,
                                clear_export, ctx, &m->nexports, &m->exports);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                print_export(&m->exports[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_start_section(const uint8_t **pp, const uint8_t *ep,
                   struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_leb_u32(&p, ep, &m->start);
        if (ret != 0) {
                goto fail;
        }
        m->has_start = true;
        if (m->start >= m->nimportedfuncs + m->nfuncs) {
                ret = EINVAL;
                goto fail;
        }
        const struct functype *ft = module_functype(m, m->start);
        if (ft->parameter.ntypes > 0 || ft->result.ntypes > 0) {
                ret = EINVAL;
                goto fail;
        }

        xlog_trace("start %" PRIu32, m->start);

        ret = 0;
        *pp = p;
fail:
        return ret;
}

struct read_element_init_expr_context {
        struct load_context *lctx;
        struct element *elem;
};

static int
read_element_init_expr(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
                       struct expr *e, void *vctx)
{
        struct read_element_init_expr_context *ctx = vctx;
        struct element *elem = ctx->elem;
        /*
         * TODO this should be more restrictive than other const expr
         * https://github.com/WebAssembly/spec/blob/main/proposals/bulk-memory-operations/Overview.md#element-segments
         */
        return read_const_expr(pp, ep, e, elem->type, ctx->lctx);
}

/*
 * https://webassembly.github.io/spec/core/binary/modules.html#element-section
 * https://webassembly.github.io/spec/core/valid/modules.html#element-segments
 */
static int
read_element(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
             struct element *elem, void *vctx)
{
        struct load_context *ctx = vctx;
        struct module *module = ctx->module;
        const uint8_t *p = *pp;
        uint32_t u32;
        uint32_t tableidx;
        enum valtype et;
        uint8_t u8;
        struct read_element_init_expr_context init_expr_ctx = {
                .lctx = ctx,
                .elem = elem,
        };
        uint32_t i;
        int ret;

        memset(elem, 0, sizeof(*elem));
        ret = read_leb_u32(&p, ep, &u32);
        if (ret != 0) {
                goto fail;
        }
        if (u32 > 7) {
                report_error(&ctx->report, "unimplemented element %" PRIu32,
                             u32);
                ret = EINVAL;
                goto fail;
        }
        switch (u32) {
        case 2:
        case 6:
                /* tableidx */
                ret = read_leb_u32(&p, ep, &tableidx);
                if (ret != 0) {
                        goto fail;
                }
                elem->table = tableidx;
                break;
        default:
                /*
                 * 0, 4 -> tableidx 0 is implicit
                 *
                 * others -> non-active, no tableidx here
                 */
                elem->table = 0;
                break;
        }
        switch (u32) {
        case 0:
        case 2:
        case 4:
        case 6:
                elem->mode = ELEM_MODE_ACTIVE;
                /* offset */
                ret = read_const_expr(&p, ep, &elem->offset, TYPE_i32, ctx);
                if (ret != 0) {
                        goto fail;
                }
                break;
        case 1:
        case 5:
                elem->mode = ELEM_MODE_PASSIVE;
                break;
        default:
                elem->mode = ELEM_MODE_DECLARATIVE;
                break;
        }
        switch (u32) {
        case 1:
        case 2:
        case 3:
                /* elemkind */
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        goto fail;
                }
                if (u8 != 0) {
                        report_error(&ctx->report, "unexpected elemkind %u",
                                     u8);
                        ret = EINVAL;
                        goto fail;
                }
                /* fallthrough */
        default: /* 0, 4 */
                elem->type = TYPE_FUNCREF;
                break;
        case 5:
        case 6:
        case 7:
                /* reftype */
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        goto fail;
                }
                et = u8;
                if (!is_reftype(et)) {
                        report_error(&ctx->report, "unexpected reftype %u",
                                     u8);
                        ret = EINVAL;
                        goto fail;
                }
                elem->type = et;
                break;
        }
        switch (u32) {
        case 0:
        case 1:
        case 2:
        case 3:
                /*
                 * vec(funcidx)
                 */
                assert(elem->type == TYPE_FUNCREF);
                ret = read_vec_u32(&p, ep, &elem->init_size, &elem->funcs);
                if (ret != 0) {
                        goto fail;
                }
                for (i = 0; i < elem->init_size; i++) {
                        if (elem->funcs[i] >= ctx->module->nimportedfuncs +
                                                      ctx->module->nfuncs) {
                                ret = EINVAL;
                                goto fail;
                        }
                        bitmap_set(&ctx->refs, elem->funcs[i]);
                }
                break;
        case 4:
        case 5:
        case 6:
        case 7:
                /*
                 * vec(expr)
                 */
                ret = read_vec_with_ctx(&p, ep, sizeof(*elem->init_exprs),
                                        read_element_init_expr, clear_expr,
                                        &init_expr_ctx, &elem->init_size,
                                        &elem->init_exprs);
                if (ret != 0) {
                        goto fail;
                }
        }
        if (elem->mode == ELEM_MODE_ACTIVE) {
                if (elem->table >= module->nimportedtables + module->ntables) {
                        report_error(&ctx->report,
                                     "element tableidx out of range");
                        ret = EINVAL;
                        goto fail;
                }
                enum valtype table_type =
                        module_tabletype(module, elem->table)->et;
                if (table_type != elem->type) {
                        report_error(&ctx->report,
                                     "element type mismatch %u != %u",
                                     table_type, elem->type);
                        ret = EINVAL;
                        goto fail;
                }
        }
        ret = 0;
        *pp = p;
        return 0;
fail:
        return ret;
}

static void
clear_element(struct element *elem)
{
        if (elem->init_exprs != NULL) {
                uint32_t i;
                for (i = 0; i < elem->init_size; i++) {
                        clear_expr(&elem->init_exprs[i]);
                }
                free(elem->init_exprs);
        }
        free(elem->funcs);
        clear_expr(&elem->offset);
}

static void
print_element(uint32_t idx, const struct element *elem)
{
        xlog_trace("element [%" PRIu32 "] %s", idx, valtype_str(elem->type));
}

static int
read_element_section(const uint8_t **pp, const uint8_t *ep,
                     struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(*m->elems), read_element,
                                clear_element, ctx, &m->nelems, &m->elems);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nelems; i++) {
                print_element(i, &m->elems[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_code_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        assert(m->funcs == NULL);
        uint32_t nfuncs_in_code = 0;
        ret = read_vec_with_ctx(&p, ep, sizeof(*m->funcs), read_func,
                                clear_func, ctx, &nfuncs_in_code, &m->funcs);
        if (ret != 0) {
                assert(nfuncs_in_code == 0);
                goto fail;
        }
        if (nfuncs_in_code != m->nfuncs) {
                xlog_trace("nfunc mismatch %" PRIu32 " != %" PRIu32,
                           nfuncs_in_code, m->nfuncs);
                m->nfuncs = nfuncs_in_code; /* for unload */
                ret = EINVAL;
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                xlog_trace("func nlocals %u", m->funcs[i].localtype.nlocals);
        }

        *pp = p;
        return 0;
fail:
        xlog_trace("read_code_section failed");
        /* avoid accessing uninitializing data in module_unload */
        free(m->funcs);
        m->funcs = NULL;
        return ret;
}

static int
read_data(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
          struct data *data, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        uint32_t u32;
        int ret;

        memset(data, 0, sizeof(*data));
        ret = read_leb_u32(&p, ep, &u32);
        if (ret != 0) {
                goto fail;
        }
        data->memory = 0;
        struct module *m = ctx->module;
        switch (u32) {
        case 1:
                data->mode = DATA_MODE_PASSIVE;
                break;
        case 2:
                ret = read_leb_u32(&p, ep, &data->memory);
                if (ret != 0) {
                        goto fail;
                }
                /* fallthrough */
        case 0:
                data->mode = DATA_MODE_ACTIVE;
                if (data->memory >= m->nimportedmems + m->nmems) {
                        ret = EINVAL;
                        goto fail;
                }
                ret = read_const_expr(&p, ep, &data->offset, TYPE_i32, ctx);
                if (ret != 0) {
                        goto fail;
                }
                break;
        default:
                report_error(&ctx->report, "unknown data %" PRIu32, u32);
                ret = EINVAL;
                goto fail;
        }

        uint32_t vec_count;
        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }
        if (ep - p < vec_count) {
                ret = E2BIG;
                goto fail;
        }
        data->init_size = vec_count;
        data->init = p;
        p += vec_count;

        ret = 0;
        *pp = p;
        return 0;
fail:
        return ret;
}

static void
clear_data(struct data *data)
{
        clear_expr(&data->offset);
}

static void
print_data(uint32_t idx, const struct data *data)
{
        xlog_trace("data [%" PRIu32 "] %" PRIu32 " bytes", idx,
                   data->init_size);
}

static int
read_data_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(&p, ep, sizeof(*m->datas), read_data,
                                clear_data, ctx, &m->ndatas, &m->datas);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->ndatas; i++) {
                print_data(i, &m->datas[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

static int
read_datacount_section(const uint8_t **pp, const uint8_t *ep,
                       struct load_context *ctx)
{
        const uint8_t *p = *pp;
        uint32_t count;
        int ret;
        ret = read_leb_u32(&p, ep, &count);
        if (ret != 0) {
                goto fail;
        }
        ctx->has_datacount = true;
        ctx->ndatas_in_datacount = count;
        ret = 0;
        *pp = ep;
fail:
        return ret;
}

static int
read_custom_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        const uint8_t *p = *pp;
        int ret;
        /*
         * read the name just for validation.
         */
        struct name name;
        ret = read_name(&p, ep, &name);
        if (ret != 0) {
                goto fail;
        }
        clear_name(&name);
        /*
         * unspecified bytes follow. just skip them.
         */
        ret = 0;
        *pp = ep;
fail:
        return ret;
}

struct section_type {
        const char *name;
        int (*read)(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx);
        int order;
};

/*
 * https://webassembly.github.io/spec/core/binary/modules.html#binary-typeidx
 */

#define SECTION(n, o)                                                         \
        [SECTION_ID_##n] = {.name = #n, .read = read_##n##_section, .order = o}

static const struct section_type section_types[] = {
        SECTION(custom, 0),     SECTION(type, 1),   SECTION(import, 2),
        SECTION(function, 3),   SECTION(table, 4),  SECTION(memory, 5),
        SECTION(global, 6),     SECTION(export, 7), SECTION(start, 8),
        SECTION(element, 9),    SECTION(code, 11),  SECTION(data, 12),
        SECTION(datacount, 10),
};

static const struct section_type *
get_section_type(uint8_t id)
{
        if (id >= ARRAYCOUNT(section_types)) {
                return NULL;
        }
        return &section_types[id];
}

int
module_load(struct module *m, const uint8_t *p, const uint8_t *ep,
            struct load_context *ctx)
{
        uint32_t v;
        int ret;

        memset(m, 0, sizeof(*m));
        ctx->module = m;
        m->bin = p;

        ret = read_u32(&p, ep, &v);
        if (ret != 0) {
                goto fail;
        }
        if (v != WASM_MAGIC) { /* magic */
                report_error(&ctx->report, "wrong magic: %" PRIx32, v);
                ret = EINVAL;
                goto fail;
        }

        ret = read_u32(&p, ep, &v);
        if (ret != 0) {
                goto fail;
        }
        if (v != 1) { /* version */
                report_error(&ctx->report, "wrong version: %u", v);
                ret = EINVAL;
                goto fail;
        }

        uint8_t max_seen_section_id = 0;
        while (p < ep) {
                struct section s;
                ret = section_load(&s, &p, ep);
                if (ret != 0) {
                        report_error(&ctx->report,
                                     "section_load failed with %d", ret);
                        goto fail;
                }
                const struct section_type *t = get_section_type(s.id);

                if (t == NULL) {
                        report_error(&ctx->report, "unknown section %u", s.id);
                        ret = EINVAL;
                        goto fail;
                }
                const char *name = t->name;
                /*
                 * sections except the custom section (id=0) should be
                 * seen in order, at most once.
                 */
                if (s.id > 0) {
                        if (max_seen_section_id >= t->order) {
                                report_error(&ctx->report,
                                             "unexpected section %u (%s)",
                                             s.id, name);
                                ret = EINVAL;
                                goto fail;
                        }
                        max_seen_section_id = t->order;
                }
                xlog_trace("section %u (%s), size %" PRIu32, s.id, name,
                           s.size);
                if (t != NULL && t->read != NULL) {
                        const uint8_t *sp = s.data;
                        const uint8_t *sep = sp + s.size;

                        ret = t->read(&sp, sep, ctx);
                        if (ret != 0) {
                                report_error(&ctx->report,
                                             "error (%d) while decoding "
                                             "section (%s)",
                                             ret, name);
                                goto fail;
                        }
                        if (sp != sep) {
                                report_error(
                                        &ctx->report,
                                        "section (%s) has %zu bytes extra "
                                        "data",
                                        name, sep - sp);
                                ret = EINVAL;
                                goto fail;
                        }
                }
        }

        /*
         * TODO some of module validations probably need to be done here
         * https://webassembly.github.io/spec/core/valid/modules.html
         */

#if !defined(TOYWASM_ENABLE_WASM_MULTI_MEMORY)
        if (m->nimportedmems + m->nmems > 1) {
                ret = EINVAL;
                goto fail;
        }
#endif

        if ((m->funcs == NULL) != (m->nfuncs == 0)) {
                /* maybe there was no code section */
                ret = EINVAL;
                goto fail;
        }

        if (ctx->has_datacount && ctx->ndatas_in_datacount != m->ndatas) {
                ret = EINVAL;
                goto fail;
        }

        /*
         * export names should be unique.
         * https://webassembly.github.io/spec/core/syntax/modules.html#exports
         * TODO use something which is not O(n^2)
         *
         * Note: on the other hand, import names are not necessarily unique.
         * https://webassembly.github.io/spec/core/syntax/modules.html#imports
         */
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                uint32_t j;
                for (j = i + 1; j < m->nexports; j++) {
                        if (!compare_name(&m->exports[i].name,
                                          &m->exports[j].name)) {
                                ret = EINVAL;
                                goto fail;
                        }
                }
        }

        ret = 0;
fail:
        return ret;
}

static void
module_unload(struct module *m)
{
        uint32_t i;

        for (i = 0; i < m->ntypes; i++) {
                clear_functype(&m->types[i]);
        }
        free(m->types);

        if (m->funcs != NULL) {
                for (i = 0; i < m->nfuncs; i++) {
                        clear_func(&m->funcs[i]);
                }
                free(m->funcs);
        }
        free(m->functypeidxes);

        free(m->tables);
        free(m->mems);

        for (i = 0; i < m->nglobals; i++) {
                clear_global(&m->globals[i]);
        }
        free(m->globals);

        for (i = 0; i < m->nelems; i++) {
                clear_element(&m->elems[i]);
        }
        free(m->elems);

        for (i = 0; i < m->ndatas; i++) {
                clear_data(&m->datas[i]);
        }
        free(m->datas);

        for (i = 0; i < m->nimports; i++) {
                clear_import(&m->imports[i]);
        }
        free(m->imports);

        for (i = 0; i < m->nexports; i++) {
                clear_export(&m->exports[i]);
        }
        free(m->exports);

        memset(m, 0, sizeof(*m));
}

int
module_create(struct module **mp)
{
        struct module *m = zalloc(sizeof(*m));
        if (m == NULL) {
                return ENOMEM;
        }
        *mp = m;
        return 0;
}

void
module_destroy(struct module *m)
{
        assert(m != NULL);
        module_unload(m);
        free(m);
}

void
load_context_init(struct load_context *ctx)
{
        memset(ctx, 0, sizeof(*ctx));
        report_init(&ctx->report);
        load_options_set_defaults(&ctx->options);
}

void
load_context_clear(struct load_context *ctx)
{
        report_clear(&ctx->report);
        bitmap_free(&ctx->refs);
}

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
static size_t
resulttype_overhead(const struct resulttype *rt)
{
        size_t sz = 0;
        sz += sizeof(rt->cellidx);
        if (rt->cellidx.cellidxes != NULL) {
                sz += rt->ntypes * sizeof(*rt->cellidx.cellidxes);
        }
        return sz;
}
#endif

void
module_print_stats(const struct module *m)
{
        nbio_printf("=== module memory usage statistics ===\n");
        uint32_t i;
        size_t jump_table_size = 0;
#if defined(TOYWASM_ENABLE_WRITER)
        size_t code_size = 0;
#endif
        size_t type_annotation_size = 0;
        size_t localtype_cellidx_size = 0;
        size_t resulttype_cellidx_size = 0;
        for (i = 0; i < m->nfuncs; i++) {
                const struct func *func = &m->funcs[i];
                const struct expr *e = &func->e;
                const struct expr_exec_info *ei = &e->ei;
                jump_table_size += sizeof(ei->jumps);
                jump_table_size += sizeof(ei->njumps);
                if (ei->jumps != NULL) {
                        jump_table_size += ei->njumps * sizeof(*ei->jumps);
                }
#if defined(TOYWASM_ENABLE_WRITER)
                code_size += e->end - e->start;
#endif
#if defined(TOYWASM_USE_SMALL_CELLS)
                const struct type_annotations *a = &ei->type_annotations;
                type_annotation_size += sizeof(*a);
                type_annotation_size += a->ntypes * sizeof(*a->types);
#endif
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
                const struct localtype *lt = &func->localtype;
                localtype_cellidx_size += sizeof(lt->cellidx);
                if (lt->cellidx.cellidxes != NULL) {
                        localtype_cellidx_size +=
                                lt->nlocals * sizeof(*lt->cellidx.cellidxes);
                }
#endif
        }
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        for (i = 0; i < m->ntypes; i++) {
                const struct functype *ft = &m->types[i];
                resulttype_cellidx_size += resulttype_overhead(&ft->parameter);
                resulttype_cellidx_size += resulttype_overhead(&ft->result);
        }
#endif
#if defined(TOYWASM_ENABLE_WRITER)
        nbio_printf("%30s %12zu bytes\n", "wasm instructions to annotate",
                    code_size);
#endif
        nbio_printf("%30s %12zu bytes\n", "jump table overhead",
                    jump_table_size);
        nbio_printf("%30s %12zu bytes\n", "type annotation overhead",
                    type_annotation_size);
        nbio_printf("%30s %12zu bytes\n", "local type cell idx overhead",
                    localtype_cellidx_size);
        nbio_printf("%30s %12zu bytes\n", "result type cell idx overhead",
                    resulttype_cellidx_size);
}
