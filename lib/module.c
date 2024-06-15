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
#include "dylink_type.h"
#include "expr.h"
#include "leb128.h"
#include "load_context.h"
#include "mem.h"
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
        ret = read_vec(load_mctx(ctx), &p, ep, sizeof(*rt->types),
                       (read_elem_func_t)read_valtype, &rt->ntypes,
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
                        xlog_trace("populate_resulttype_cellidx failed with "
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
clear_resulttype(struct mem_context *mctx, struct resulttype *rt)
{
        if (rt->types != NULL) {
                mem_free(mctx, rt->types, rt->ntypes * sizeof(*rt->types));
        }
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
                clear_resulttype(load_mctx(ctx), &ft->parameter);
                goto fail;
        }

        *pp = p;
fail:
        return ret;
}

void
clear_functype(struct mem_context *mctx, struct functype *ft)
{
        clear_resulttype(mctx, &ft->parameter);
        clear_resulttype(mctx, &ft->result);
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
            uint8_t *extra_memory_flagsp, uint8_t *shiftp,
            uint8_t default_shift, uint64_t typemax)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        bool has_max;
        bool has_shift = false;
        uint8_t shift;
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
        if (shiftp != NULL) {
#if defined(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES)
                const uint8_t mask = MEMTYPE_FLAG_CUSTOM_PAGE_SIZE;
#else
                const uint8_t mask = 0;
#endif
                has_shift = u8 & mask;
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
        }
        if (has_shift) {
                uint32_t u32;
                ret = read_leb_u32(&p, ep, &u32);
                if (ret != 0) {
                        goto fail;
                }
                /* only 1 byte and 64KB pages are allowed */
                if (u32 != 0 && u32 != WASM_PAGE_SHIFT) {
                        ret = EINVAL;
                        goto fail;
                }
                shift = u32;
        } else {
                shift = default_shift;
        }
        if (shiftp != NULL) {
                *shiftp = shift;
        }
        uint64_t typemax_shifted = typemax >> shift;
        assert(typemax == (uint64_t)typemax_shifted << shift);
        assert(typemax_shifted <= (uint64_t)UINT32_MAX + 1);
        if (has_max) {
                if (typemax_shifted < lim->max) {
                        ret = EOVERFLOW;
                        goto fail;
                }
        } else {
                lim->max = UINT32_MAX;
                if (typemax_shifted < lim->max) {
                        lim->max = typemax_shifted;
                }
        }
        if (typemax_shifted < lim->min) {
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
        return read_limits(pp, ep, &mt->lim, &mt->flags,
#if defined(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES)
                           &mt->page_shift,
#else
                           NULL,
#endif
                           WASM_PAGE_SHIFT, WASM_MAX_MEMORY_SIZE);
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

int
read_name(const uint8_t **pp, const uint8_t *ep, struct name *namep)
{
        const uint8_t *p = *pp;
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
        return ret;
}

void
set_name_cstr(struct name *name, const char *cstr)
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
        ret = read_limits(&p, ep, &tt->lim, NULL, NULL, 0, UINT32_MAX);
        if (ret != 0) {
                goto fail;
        }
        *pp = p;
        return 0;
fail:
        return ret;
}

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
static int
read_tagtype(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
             struct tagtype *tag, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        uint32_t type;
        uint32_t typeidx;
        int ret;

        ret = read_leb_u32(&p, ep, &type);
        if (ret != 0) {
                goto fail;
        }
        if (type != TAG_TYPE_exception) {
                ret = EINVAL;
                goto fail;
        }
        ret = read_leb_u32(&p, ep, &typeidx);
        if (ret != 0) {
                goto fail;
        }
        const struct module *m = ctx->module;
        if (typeidx >= m->ntypes) {
                ret = EINVAL;
                goto fail;
        }
        const struct functype *ft = &m->types[typeidx];
        if (ft->result.ntypes > 0) {
                ret = EINVAL;
                goto fail;
        }
        uint32_t csz = resulttype_cellsize(&ft->parameter);
        if (csz > TOYWASM_EXCEPTION_MAX_CELLS) {
                report_error(&ctx->report,
                             "too big exception %" PRIu32 " > %" PRIu32, csz,
                             (uint32_t)TOYWASM_EXCEPTION_MAX_CELLS);
                ret = ENOTSUP;
                goto fail;
        }
        tag->typeidx = typeidx;
        ret = 0;
        *pp = p;
        return 0;
fail:
        return ret;
}
#endif

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
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        case 0x04: /* tag */
                ret = read_tagtype(&p, ep, 0, &desc->u.tagtype, ctx);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedtags++;
                break;
#endif
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
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        case 0x04: /* tag */
#endif
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
        case EXTERNTYPE_FUNC:
                if (desc->idx >= m->nimportedfuncs + m->nfuncs) {
                        xlog_trace("export idx (%" PRIu32
                                   ") out of range for type %u",
                                   desc->idx, u8);
                        ret = EINVAL;
                        goto fail;
                }
                bitmap_set(&ctx->refs, desc->idx);
                break;
        case EXTERNTYPE_TABLE:
                if (desc->idx >= m->nimportedtables + m->ntables) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXTERNTYPE_MEMORY:
                if (desc->idx >= m->nimportedmems + m->nmems) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXTERNTYPE_GLOBAL:
                if (desc->idx >= m->nimportedglobals + m->nglobals) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        case EXTERNTYPE_TAG:
                if (desc->idx >= m->nimportedtags + m->ntags) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
#endif
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
clear_import(struct mem_context *mctx, struct import *im)
{
        clear_name(&im->module_name);
        clear_name(&im->name);
}

static int
read_export(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct wasm_export *ex, void *vctx)
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
clear_export(struct mem_context *mctx, struct wasm_export *ex)
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
print_export(const struct wasm_export *ex)
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

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->types),
                                read_functype, clear_functype, ctx, &m->ntypes,
                                &m->types);
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

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(struct import),
                                read_import, clear_import, ctx, &m->nimports,
                                &m->imports);
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

static void
clear_expr_exec_info(struct mem_context *mctx, struct expr_exec_info *ei)
{
        mem_free(mctx, ei->jumps, ei->njumps * sizeof(*ei->jumps));
#if defined(TOYWASM_USE_SMALL_CELLS)
        struct type_annotations *an = &ei->type_annotations;
        mem_free(mctx, an->types, an->ntypes * sizeof(*an->types));
#endif
}

static void
clear_expr(struct mem_context *mctx, struct expr *expr)
{
        clear_expr_exec_info(mctx, &expr->ei);
}

static void
clear_func(struct mem_context *mctx, struct func *func)
{
        struct localtype *lt = &func->localtype;
        free(lt->localchunks);
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        free(lt->cellidx.cellidxes);
#endif
        clear_expr(mctx, &func->e);
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
                if (UINT32_MAX - nlocals < count) {
                        ret = E2BIG;
                        goto fail;
                }
                nlocals += count;
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
                        xlog_trace("populate_localtype_cellidx failed with "
                                   "%d. It can cause very slow execution.",
                                   ret);
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

        func->e.ei.jumps = NULL;
#if defined(TOYWASM_USE_SMALL_CELLS)
        func->e.ei.type_annotations.types = NULL;
#endif
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
        if (ep < cep) {
                ret = EINVAL;
                goto fail;
        }
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
        clear_func(load_mctx(ctx), func);
        return ret;
}

static int
read_function_section(const uint8_t **pp, const uint8_t *ep,
                      struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_u32(load_mctx(ctx), &p, ep, &m->nfuncs,
                           &m->functypeidxes);
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

        ret = read_vec(load_mctx(ctx), &p, ep, sizeof(*m->tables),
                       (read_elem_func_t)read_tabletype, &m->ntables,
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

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->mems),
                                read_memtype, NULL, ctx, &m->nmems, &m->mems);
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
clear_global(struct mem_context *mctx, struct global *g)
{
        clear_expr(mctx, &g->init);
}

static int
read_global_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->globals),
                                read_global, clear_global, ctx, &m->nglobals,
                                &m->globals);
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

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep,
                                sizeof(struct wasm_export), read_export,
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

static void
clear_element(struct mem_context *mctx, struct element *elem)
{
        if (elem->init_exprs != NULL) {
                uint32_t i;
                for (i = 0; i < elem->init_size; i++) {
                        clear_expr(mctx, &elem->init_exprs[i]);
                }
                mem_free(mctx, elem->init_exprs,
                         elem->init_size * sizeof(*elem->init_exprs));
        } else {
                mem_free(mctx, elem->funcs,
                         elem->init_size * sizeof(*elem->funcs));
        }
        clear_expr(mctx, &elem->offset);
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
                ret = read_vec_u32(load_mctx(ctx), &p, ep, &elem->init_size,
                                   &elem->funcs);
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
                ret = read_vec_with_ctx(
                        load_mctx(ctx), &p, ep, sizeof(*elem->init_exprs),
                        read_element_init_expr, clear_expr, &init_expr_ctx,
                        &elem->init_size, &elem->init_exprs);
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
        clear_element(load_mctx(ctx), elem);
        return ret;
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

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->elems),
                                read_element, clear_element, ctx, &m->nelems,
                                &m->elems);
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
        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->funcs),
                                read_func, clear_func, ctx, &nfuncs_in_code,
                                &m->funcs);
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
        mem_free(load_mctx(ctx), m->funcs, nfuncs_in_code * sizeof(*m->funcs));
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
fail:
        return ret;
}

static void
clear_data(struct mem_context *mctx, struct data *data)
{
        clear_expr(mctx, &data->offset);
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

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->datas),
                                read_data, clear_data, ctx, &m->ndatas,
                                &m->datas);
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

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
static void
clear_tagtype(struct mem_context *mctx, struct tagtype *tag)
{
}

static int
read_tag_section(const uint8_t **pp, const uint8_t *ep,
                 struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx(load_mctx(ctx), &p, ep, sizeof(*m->tags),
                                read_tagtype, clear_tagtype, ctx, &m->ntags,
                                &m->tags);
        if (ret != 0) {
                goto fail;
        }
        ret = 0;
        *pp = p;
fail:
        return ret;
}
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */

static int
read_section(const uint8_t **pp, const uint8_t *ep, const char *name,
             int (*read_fn)(const uint8_t **pp, const uint8_t *ep,
                            struct load_context *ctx),
             struct load_context *ctx)
{
        const uint8_t *p = *pp;
        int ret;
        ret = read_fn(&p, ep, ctx);
        if (ret != 0) {
                report_error(&ctx->report,
                             "error (%d) while decoding section (%s)", ret,
                             name);
                goto fail;
        }
        if (p != ep) {
                report_error(&ctx->report,
                             "section (%s) has %zu bytes extra data", name,
                             ep - p);
                ret = EINVAL;
                goto fail;
        }
        *pp = p;
fail:
        return ret;
}

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
/* https://webassembly.github.io/spec/core/appendix/custom.html#name-section */
static int
read_name_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        const uint8_t *p = *pp;
        struct module *m = ctx->module;
        /*
         * Just record the pointers so that we can process the section later
         * if/when necessary.
         *
         * The actual parsing logic is implemeted in name.c.
         */
        xlog_trace("name section %p .. %p", (const void *)p, (const void *)ep);
        m->name_section_start = p;
        m->name_section_end = ep;
        *pp = ep;
        return 0;
}
#endif /* defined(TOYWASM_ENABLE_WASM_NAME_SECTION) */

#if defined(TOYWASM_ENABLE_DYLD)
static int
read_dylink_mem_info(const uint8_t **pp, const uint8_t *ep,
                     struct load_context *ctx)
{
        const uint8_t *p = *pp;
        struct dylink_mem_info *minfo = &ctx->module->dylink->mem_info;
        int ret;
        ret = read_leb_u32(&p, ep, &minfo->memorysize);
        if (ret != 0) {
                goto fail;
        }
        ret = read_leb_u32(&p, ep, &minfo->memoryalignment);
        if (ret != 0) {
                goto fail;
        }
        ret = read_leb_u32(&p, ep, &minfo->tablesize);
        if (ret != 0) {
                goto fail;
        }
        ret = read_leb_u32(&p, ep, &minfo->tablealignment);
        if (ret != 0) {
                goto fail;
        }
        xlog_trace("dylink memorysize %" PRIu32, minfo->memorysize);
        xlog_trace("dylink memoryalignment %" PRIu32, minfo->memoryalignment);
        xlog_trace("dylink tablesize %" PRIu32, minfo->tablesize);
        xlog_trace("dylink tablealignment %" PRIu32, minfo->tablealignment);
        *pp = p;
fail:
        return ret;
}

static int
read_dylink_needs(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        const uint8_t *p = *pp;
        struct dylink_needs *needs = &ctx->module->dylink->needs;
        int ret;
        needs->count = 0;
        needs->names = NULL;
        ret = read_vec(load_mctx(ctx), &p, ep, sizeof(*needs->names),
                       (read_elem_func_t)read_name, &needs->count,
                       (void *)&needs->names);
        if (ret != 0) {
                goto fail;
        }
        uint32_t i;
        for (i = 0; i < needs->count; i++) {
                xlog_trace("dylink needs: %.*s", CSTR(&needs->names[i]));
        }
        *pp = p;
fail:
        return ret;
}

static int
read_import_info(const uint8_t **pp, const uint8_t *ep,
                 struct dylink_import_info *ii)
{
        const uint8_t *p = *pp;
        int ret;
        ret = read_name(&p, ep, &ii->module_name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_name(&p, ep, &ii->name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_leb_i32(&p, ep, &ii->flags);
        if (ret != 0) {
                goto fail;
        }
        *pp = p;
fail:
        return ret;
}

static int
read_dylink_import_info(const uint8_t **pp, const uint8_t *ep,
                        struct load_context *ctx)
{
        const uint8_t *p = *pp;
        struct dylink *dy = ctx->module->dylink;
        int ret;
        ret = read_vec(load_mctx(ctx), &p, ep, sizeof(*dy->import_info),
                       (read_elem_func_t)read_import_info, &dy->nimport_info,
                       (void *)&dy->import_info);
        if (ret != 0) {
                goto fail;
        }
        *pp = p;
fail:
        return ret;
}

enum dylink_subsection_type {
        WASM_dylink_mem_info = 1,
        WASM_dylink_needs = 2,
        WASM_dylink_export_info = 3,
        WASM_dylink_import_info = 4,
};

#define DYLINK_TYPE(NAME)                                                     \
        {                                                                     \
                .type = WASM_dylink_##NAME, .name = #NAME,                    \
                .read = read_dylink_##NAME,                                   \
        }

static const struct dylink_subsection {
        enum dylink_subsection_type type;
        const char *name;
        int (*read)(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx);
} dylink_subsections[] = {
        DYLINK_TYPE(mem_info), DYLINK_TYPE(needs), DYLINK_TYPE(import_info),
        /*
         * TODO: WASM_dylink_export_info is necessary for TLS support.
         */
};

static void
clear_dylink_needs(struct mem_context *mctx, struct dylink_needs *needs)
{
        if (needs->names != NULL) {
                uint32_t i;
                for (i = 0; i < needs->count; i++) {
                        clear_name(&needs->names[i]);
                }
                mem_free(mctx, needs->names,
                         needs->count * sizeof(*needs->names));
        }
}

static void
clear_dylink(struct mem_context *mctx, struct dylink *dy)
{
        clear_dylink_needs(mctx, &dy->needs);
        mem_free(mctx, dy->import_info,
                 dy->nimport_info * sizeof(*dy->import_info));
}

/* https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md
 */
static int
read_dylink_0_section(const uint8_t **pp, const uint8_t *ep,
                      struct load_context *ctx)
{
        const uint8_t *p = *pp;
        struct module *m = ctx->module;
        int ret;
        struct mem_context *mctx = load_mctx(ctx);
        m->dylink = mem_zalloc(mctx, sizeof(*m->dylink));
        if (m->dylink == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        do {
                uint8_t type;
                uint32_t payload_len;
                ret = read_u8(&p, ep, &type);
                if (ret != 0) {
                        goto fail;
                }
                ret = read_leb_u32(&p, ep, &payload_len);
                if (ret != 0) {
                        goto fail;
                }
                const uint8_t *sep = p + payload_len;
                unsigned int i;
                for (i = 0; i < ARRAYCOUNT(dylink_subsections); i++) {
                        const struct dylink_subsection *ss =
                                &dylink_subsections[i];
                        if (ss->type == type) {
                                ret = read_section(&p, sep, ss->name, ss->read,
                                                   ctx);
                                if (ret != 0) {
                                        goto fail;
                                }
                                break;
                        }
                }
                if (i >= ARRAYCOUNT(dylink_subsections)) {
                        xlog_trace("skipping unimplemented subsection (%u)",
                                   (unsigned int)type);
                        p = sep;
                        break;
                }
        } while (p < ep);
        ret = 0;
        *pp = p;
        return 0;
fail:
        clear_dylink(mctx, m->dylink);
        mem_free(mctx, m->dylink, sizeof(*m->dylink));
        m->dylink = NULL;
        return ret;
}
#endif /* defined(TOYWASM_ENABLE_DYLD) */

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION) || defined(TOYWASM_ENABLE_DYLD)
static const struct known_custom_section {
        const char *name;
        int (*read)(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx);
} known_custom_sections[] = {
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        {
                .name = "name",
                .read = read_name_section,
        },
#endif /* defined(TOYWASM_ENABLE_WASM_NAME_SECTION) */
#if defined(TOYWASM_ENABLE_DYLD)
        {
                .name = "dylink.0",
                .read = read_dylink_0_section,
        },
#endif /* defined(TOYWASM_ENABLE_DYLD) */
};
#endif /* defined(TOYWASM_ENABLE_WASM_NAME_SECTION) ||                        \
          defined(TOYWASM_ENABLE_DYLD) */

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
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION) || defined(TOYWASM_ENABLE_DYLD)
        const struct known_custom_section *k;
        unsigned int i;
        for (i = 0; i < ARRAYCOUNT(known_custom_sections); i++) {
                k = &known_custom_sections[i];
                struct name kname = NAME_FROM_CSTR(k->name);
                if (compare_name(&name, &kname)) {
                        xlog_trace("skipping unknown custom section \"%.*s\"",
                                   CSTR(&name));
                        continue;
                }
                xlog_trace("known custom section %s found", k->name);
                break;
        }
#endif
        clear_name(&name);
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION) || defined(TOYWASM_ENABLE_DYLD)
        if (i < ARRAYCOUNT(known_custom_sections)) {
                ret = read_section(&p, ep, k->name, k->read, ctx);
                if (ret != 0) {
                        goto fail;
                }
                *pp = p;
        } else
#endif
        {
                /*
                 * unspecified bytes follow. just skip them.
                 */
                ret = 0;
                *pp = ep;
        }
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
        SECTION(global, 7),     SECTION(export, 8), SECTION(start, 9),
        SECTION(element, 10),   SECTION(code, 12),  SECTION(data, 13),
        SECTION(datacount, 11),
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        SECTION(tag, 6),
#endif
};

static const struct section_type *
get_section_type(uint8_t id)
{
        if (id >= ARRAYCOUNT(section_types)) {
                return NULL;
        }
        return &section_types[id];
}

#if defined(TOYWASM_SORT_EXPORTS)
static int
cmp_export(const void *vp_a, const void *vp_b)
{
        const struct wasm_export *a = vp_a;
        const struct wasm_export *b = vp_b;
        return compare_name(&a->name, &b->name);
}
#endif

static int
module_load_into(struct module *m, const uint8_t *p, const uint8_t *ep,
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

                        ret = read_section(&sp, sep, name, t->read, ctx);
                        if (ret != 0) {
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
         *
         * Note: on the other hand, import names are not necessarily unique.
         * https://webassembly.github.io/spec/core/syntax/modules.html#imports
         */
#if defined(TOYWASM_SORT_EXPORTS)
        /*
         * Note: this version is O(n*log(n)).
         *
         * sort exports to make the uniqueness check cheaper.
         *
         * in-place sort should be ok because the order of exports
         * in a module doesn't have any meanings.
         *
         * Note: Naive implementions of qsort can be attacked with
         * a crafted input.
         * cf. https://www.cs.dartmouth.edu/~doug/mdmspe.pdf
         */
        qsort(m->exports, m->nexports, sizeof(*m->exports), cmp_export);
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                if (i + 1 < m->nexports &&
                    !compare_name(&m->exports[i].name,
                                  &m->exports[i + 1].name)) {
                        ret = EINVAL;
                        goto fail;
                }
        }
#else
        /*
         * Note: this version is O(n^2).
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
#endif

        ret = 0;
fail:
        return ret;
}

static int
module_create0(struct mem_context *mctx, struct module **mp)
{
        struct module *m = mem_zalloc(mctx, sizeof(*m));
        if (m == NULL) {
                return ENOMEM;
        }
        *mp = m;
        return 0;
}

int
module_create(struct module **mp, const uint8_t *p, const uint8_t *ep,
              struct load_context *ctx)
{
        struct mem_context *mctx = ctx->mctx;
        struct module *m;
        int ret = module_create0(mctx, &m);
        if (ret != 0) {
                return ret;
        }
        ret = module_load_into(m, p, ep, ctx);
        if (ret != 0) {
                module_destroy(mctx, m);
                return ret;
        }
        *mp = m;
        return 0;
}

static void
module_unload(struct mem_context *mctx, struct module *m)
{
        uint32_t i;

        for (i = 0; i < m->ntypes; i++) {
                clear_functype(mctx, &m->types[i]);
        }
        mem_free(mctx, m->types, m->ntypes * sizeof(*m->types));

        if (m->funcs != NULL) {
                for (i = 0; i < m->nfuncs; i++) {
                        clear_func(mctx, &m->funcs[i]);
                }
                mem_free(mctx, m->funcs, m->nfuncs * sizeof(*m->funcs));
        }
        mem_free(mctx, m->functypeidxes,
                 m->nfuncs * sizeof(*m->functypeidxes));

        mem_free(mctx, m->tables, m->ntables * sizeof(*m->tables));
        mem_free(mctx, m->mems, m->nmems * sizeof(*m->mems));

        for (i = 0; i < m->nglobals; i++) {
                clear_global(mctx, &m->globals[i]);
        }
        mem_free(mctx, m->globals, m->nglobals * sizeof(*m->globals));

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        for (i = 0; i < m->ntags; i++) {
                clear_tagtype(mctx, &m->tags[i]);
        }
        mem_free(mctx, m->tags, m->ntags * sizeof(*m->tags));
#endif

        for (i = 0; i < m->nelems; i++) {
                clear_element(mctx, &m->elems[i]);
        }
        mem_free(mctx, m->elems, m->nelems * sizeof(*m->elems));

        for (i = 0; i < m->ndatas; i++) {
                clear_data(mctx, &m->datas[i]);
        }
        mem_free(mctx, m->datas, m->ndatas * sizeof(*m->datas));

        for (i = 0; i < m->nimports; i++) {
                clear_import(mctx, &m->imports[i]);
        }
        mem_free(mctx, m->imports, m->nimports * sizeof(*m->imports));

        for (i = 0; i < m->nexports; i++) {
                clear_export(mctx, &m->exports[i]);
        }
        mem_free(mctx, m->exports, m->nexports * sizeof(*m->exports));

#if defined(TOYWASM_ENABLE_DYLD)
        if (m->dylink != NULL) {
                clear_dylink(mctx, m->dylink);
                mem_free(mctx, m->dylink, sizeof(*m->dylink));
        }
#endif

        memset(m, 0, sizeof(*m));
}

void
module_destroy(struct mem_context *mctx, struct module *m)
{
        assert(m != NULL);
        module_unload(mctx, m);
        mem_free(mctx, m, sizeof(*m));
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
