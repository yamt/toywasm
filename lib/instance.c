#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "escape.h"
#include "exec.h"
#include "instance.h"
#include "mem.h"
#include "module.h"
#include "nbio.h"
#include "shared_memory_impl.h"
#include "suspend.h"
#include "type.h"
#include "usched.h"
#include "util.h"
#include "xlog.h"

static int
find_entry_for_import(
        const struct import_object *imports, const struct import *im,
        int (*check)(const struct import_object_entry *e, const void *arg),
        const void *checkarg, const struct import_object_entry **resultp,
        struct report *report)
{
        const struct import_object *impobj = imports;
        bool mismatch = false;
        while (impobj != NULL) {
                const struct import_object_entry *e;
                int ret = import_object_find_entry(impobj, im, check, checkarg,
                                                   &e, report);
                if (ret == EINVAL) {
                        mismatch = true;
                } else if (ret != ENOENT) {
                        *resultp = e;
                        return ret;
                }
                impobj = impobj->next;
        }
        struct escaped_string module_name;
        struct escaped_string name;
        escape_name(&module_name, &im->module_name);
        escape_name(&name, &im->name);
        if (mismatch) {
                report_error(report, "No matching entry for import %.*s:%.*s",
                             ECSTR(&module_name), ECSTR(&name));
        } else {
                report_error(report, "No entry for import %.*s:%.*s",
                             ECSTR(&module_name), ECSTR(&name));
        }
        escaped_string_clear(&module_name);
        escaped_string_clear(&name);
        return ENOENT;
}

/*
 * https://webassembly.github.io/spec/core/exec/modules.html#external-typing
 * https://webassembly.github.io/spec/core/valid/types.html#import-subtyping
 */

/* return if a (which is an external type) matches b */

static bool
match_limits(const struct limits *a, const struct limits *b,
             uint32_t actual_a_min)
{
        assert(a->min <= actual_a_min); /* never shrink */
        if (actual_a_min >= b->min &&
            (b->max == UINT32_MAX ||
             (a->max != UINT32_MAX && b->max != UINT32_MAX &&
              a->max <= b->max))) {
                return true;
        }
        return false;
}

static bool
match_tabletype(const struct tabletype *a, const struct tabletype *b,
                uint32_t a_min)
{
        return a->et == b->et && match_limits(&a->lim, &b->lim, a_min);
}

static int
check_functype(const struct import_object_entry *e, const void *vp)
{
        const struct functype *ft = vp;
        assert(e->type == EXTERNTYPE_FUNC);
        struct funcinst *fi = e->u.func;
        const struct functype *ft_imported = funcinst_functype(fi);
        if (compare_functype(ft, ft_imported)) {
                return EINVAL;
        }
        return 0;
}

static int
check_tabletype(const struct import_object_entry *e, const void *vp)
{
        const struct tabletype *tt = vp;
        assert(e->type == EXTERNTYPE_TABLE);
        const struct tableinst *ti = e->u.table;
        const struct tabletype *tt_imported = ti->type;
        if (!match_tabletype(tt_imported, tt, ti->size)) {
                return EINVAL;
        }
        return 0;
}

static int
check_memtype(const struct import_object_entry *e, const void *vp)
{
        const struct memtype *mt = vp;
        assert(e->type == EXTERNTYPE_MEMORY);
        const struct meminst *mi = e->u.mem;
        const struct memtype *mt_imported = mi->type;
        if (mt_imported->flags != mt->flags) {
                return EINVAL;
        }
#if defined(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES)
        if (mt_imported->page_shift != mt->page_shift) {
                return EINVAL;
        }
#endif
        if (!match_limits(&mt_imported->lim, &mt->lim, mi->size_in_pages)) {
                return EINVAL;
        }
        return 0;
}

static int
check_globaltype(const struct import_object_entry *e, const void *vp)
{
        const struct globaltype *gt = vp;
        assert(e->type == EXTERNTYPE_GLOBAL);
        const struct globaltype *gt_imported = e->u.global->type;
        if (gt_imported->t != gt->t || gt_imported->mut != gt->mut) {
                return EINVAL;
        }
        return 0;
}

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
static int
check_tagtype(const struct import_object_entry *e, const void *vp)
{
        const struct functype *ft = vp;
        assert(e->type == EXTERNTYPE_TAG);
        const struct taginst *tag_imported = e->u.tag;
        const struct functype *ft_imported = taginst_functype(tag_imported);
        if (compare_functype(ft, ft_imported)) {
                return EINVAL;
        }
        return 0;
}
#endif

int
memory_instance_create(struct mem_context *mctx, struct meminst **mip,
                       const struct memtype *mt) NO_THREAD_SAFETY_ANALYSIS
{
        struct meminst *mp;
        int ret;

        mp = mem_zalloc(mctx, sizeof(*mp));
        if (mp == NULL) {
                ret = ENOMEM;
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        if ((mt->flags & MEMTYPE_FLAG_SHARED) != 0) {
#if defined(TOYWASM_PREALLOC_SHARED_MEMORY)
                /*
                 * REVISIT: a lim.max allocation failure below is not fatal.
                 * we can just fall back to a smaller allocation. (we may
                 * need some heuristics to decide the size.)
                 * if the application eventually ends up with growing the
                 * memory up to lim.max, it will probably fail. i guess it's
                 * rare, though.
                 */
                uint32_t need_in_pages = mt->lim.max;
#else
                uint32_t need_in_pages = mt->lim.min;
#endif /* defined(TOYWASM_PREALLOC_SHARED_MEMORY) */
                uint32_t page_shift = memtype_page_shift(mt);
                uint64_t need_in_bytes = need_in_pages << page_shift;
                if (need_in_bytes > SIZE_MAX) {
                        mem_free(mctx, mp, sizeof(*mp));
                        ret = EOVERFLOW;
                        goto fail;
                }
                mp->shared = mem_zalloc(mctx, sizeof(*mp->shared));
                if (mp->shared == NULL) {
                        mem_free(mctx, mp, sizeof(*mp));
                        ret = ENOMEM;
                        goto fail;
                }
                if (need_in_bytes > 0) {
                        mp->data = mem_zalloc(mctx, need_in_bytes);
                        if (mp->data == NULL) {
                                mem_free(mctx, mp->shared,
                                         sizeof(*mp->shared));
                                mem_free(mctx, mp, sizeof(*mp));
                                ret = ENOMEM;
                                goto fail;
                        }
                }
                mp->allocated = need_in_bytes;
                waiter_list_table_init(&mp->shared->tab);
                toywasm_mutex_init(&mp->shared->lock);
        }
#endif
        mp->size_in_pages = mt->lim.min;
        mp->type = mt;
        mp->mctx = mctx;
        *mip = mp;
        return 0;
fail:
        return ret;
}

void
memory_instance_destroy(struct mem_context *mctx, struct meminst *mi)
{
        if (mi == NULL) {
                return;
        }
        assert(mctx == mi->mctx);
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct shared_meminst *shared = mi->shared;
        if (shared != NULL) {
                toywasm_mutex_destroy(&shared->lock);
                mem_free(mctx, shared, sizeof(*shared));
        }
#endif
        mem_free(mctx, mi->data, mi->allocated);
        mem_free(mctx, mi, sizeof(*mi));
}

int
global_instance_create(struct mem_context *mctx, struct globalinst **gip,
                       const struct globaltype *gt)
{
        struct globalinst *ginst;
        int ret;
        ginst = mem_alloc(mctx, sizeof(*ginst));
        if (ginst == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ginst->type = gt;
        memset(&ginst->val, 0, sizeof(ginst->val));
        *gip = ginst;
        ret = 0;
fail:
        return ret;
}

void
global_instance_destroy(struct mem_context *mctx, struct globalinst *gi)
{
        mem_free(mctx, gi, sizeof(*gi));
}

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
int
tag_instance_create(struct mem_context *mctx, struct taginst **tip,
                    const struct functype *ft)
{
        struct taginst *ti;
        int ret;
        ti = mem_zalloc(mctx, sizeof(*ti));
        if (ti == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ti->type = ft;
        *tip = ti;
        ret = 0;
fail:
        return ret;
}

void
tag_instance_destroy(struct mem_context *mctx, struct taginst *ti)
{
        mem_free(mctx, ti, sizeof(*ti));
}
#endif

int
table_instance_create(struct mem_context *mctx, struct tableinst **tip,
                      const struct tabletype *tt)
{
        struct tableinst *tinst;
        int ret;
        tinst = mem_zalloc(mctx, sizeof(*tinst));
        if (tinst == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        tinst->type = tt;
        tinst->mctx = mctx;
        tinst->size = tinst->type->lim.min;
        uint32_t csz = valtype_cellsize(tt->et);
        size_t ncells;
        if (MUL_SIZE_OVERFLOW((size_t)tinst->size, (size_t)csz, &ncells)) {
                ret = EOVERFLOW;
                goto fail;
        }
        if (ncells > 0) {
                tinst->cells = mem_calloc(mctx, sizeof(*tinst->cells), ncells);
                if (tinst->cells == NULL) {
                        ret = ENOMEM;
                        goto fail;
                }
        }
        *tip = tinst;
        return 0;
fail:
        mem_free(mctx, tinst, sizeof(*tinst));
        return ret;
}

void
table_instance_destroy(struct mem_context *mctx, struct tableinst *ti)
{
        if (ti == NULL) {
                return;
        }
        assert(mctx == ti->mctx);
        uint32_t csz = valtype_cellsize(ti->type->et);
        size_t ncells = (size_t)ti->size * csz;
        mem_free(mctx, ti->cells, ncells * sizeof(*ti->cells));
        mem_free(mctx, ti, sizeof(*ti));
}

/* https://webassembly.github.io/spec/core/exec/modules.html#exec-instantiation
 */

int
instance_create(struct mem_context *mctx, const struct module *m,
                struct instance **instp, const struct import_object *imports,
                struct report *report)
{
        struct instance *inst;
        int ret;

		fprintf(stderr, "calling instance_create_no_init\n");
        ret = instance_create_no_init(mctx, m, &inst, imports, report);
		fprintf(stderr, "instance_create_no_init returned %d\n", ret);
        if (ret != 0) {
                return ret;
        }

        struct exec_context ctx0;
        struct exec_context *ctx = NULL;
        ctx = &ctx0;
        exec_context_init(ctx, inst, mctx);
        ctx->report = report;
		fprintf(stderr, "calling instance_execute_init\n");
        ret = instance_execute_init(ctx);
		fprintf(stderr, "instance_execute_handle_restart\n");
        ret = instance_execute_handle_restart(ctx, ret);
		fprintf(stderr, "result %d\n", ret);
        exec_context_clear(ctx);
        if (ret != 0) {
                instance_destroy(inst);
        } else {
                *instp = inst;
        }
        return ret;
}

static int
resolve_imports(struct instance *inst, const struct import_object *imports,
                struct report *report)
{
        const struct module *m = inst->module;
        uint32_t i;
        int ret;

        uint32_t funcidx = 0;
        uint32_t memidx = 0;
        uint32_t globalidx = 0;
        uint32_t tableidx = 0;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        uint32_t tagidx = 0;
#endif
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                const struct importdesc *imd = &im->desc;
                int (*check)(const struct import_object_entry *e,
                             const void *);
                const void *type;
#if defined(__GNUC__) && !defined(__clang__)
                check = NULL;
                type = NULL;
#endif
                switch (imd->type) {
                case EXTERNTYPE_FUNC:
                        check = check_functype;
                        type = &m->types[imd->u.typeidx];
                        break;
                case EXTERNTYPE_TABLE:
                        check = check_tabletype;
                        type = &imd->u.tabletype;
                        break;
                case EXTERNTYPE_MEMORY:
                        check = check_memtype;
                        type = &imd->u.memtype;
                        break;
                case EXTERNTYPE_GLOBAL:
                        check = check_globaltype;
                        type = &imd->u.globaltype;
                        break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                case EXTERNTYPE_TAG:
                        check = check_tagtype;
                        type = &m->types[imd->u.tagtype.typeidx];
                        break;
#endif
                default:
                        assert(false);
                }
                const struct import_object_entry *e;
                ret = find_entry_for_import(imports, im, check, type, &e,
                                            report);
                if (ret != 0) {
                        goto fail;
                }
                assert(e->type == imd->type);
                switch (imd->type) {
                case EXTERNTYPE_FUNC:
                        VEC_ELEM(inst->funcs, funcidx) = e->u.func;
                        funcidx++;
                        break;
                case EXTERNTYPE_TABLE:
                        VEC_ELEM(inst->tables, tableidx) = e->u.table;
                        tableidx++;
                        break;
                case EXTERNTYPE_MEMORY:
                        VEC_ELEM(inst->mems, memidx) = e->u.mem;
                        memidx++;
                        break;
                case EXTERNTYPE_GLOBAL:
                        VEC_ELEM(inst->globals, globalidx) = e->u.global;
                        globalidx++;
                        break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                case EXTERNTYPE_TAG:
                        VEC_ELEM(inst->tags, tagidx) = e->u.tag;
                        tagidx++;
                        break;
#endif
                }
        }
        assert(funcidx == m->nimportedfuncs);
        assert(tableidx == m->nimportedtables);
        assert(memidx == m->nimportedmems);
        assert(globalidx == m->nimportedglobals);
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        assert(tagidx == m->nimportedtags);
#endif

        return 0;
fail:
        return ret;
}

int
instance_create_no_init(struct mem_context *mctx, const struct module *m,
                        struct instance **instp,
                        const struct import_object *imports,
                        struct report *report)
{
        struct instance *inst;
        uint32_t i;
        int ret;

        inst = mem_zalloc(mctx, sizeof(*inst));
        if (inst == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        inst->module = m;
        inst->mctx = mctx;

        uint32_t nfuncs = m->nimportedfuncs + m->nfuncs;
        ret = VEC_RESIZE(mctx, inst->funcs, nfuncs);
        if (ret != 0) {
                goto fail;
        }
        uint32_t nmems = m->nimportedmems + m->nmems;
        ret = VEC_RESIZE(mctx, inst->mems, nmems);
        if (ret != 0) {
                goto fail;
        }
        uint32_t nglobals = m->nimportedglobals + m->nglobals;
        ret = VEC_RESIZE(mctx, inst->globals, nglobals);
        if (ret != 0) {
                goto fail;
        }
        uint32_t ntables = m->nimportedtables + m->ntables;
        ret = VEC_RESIZE(mctx, inst->tables, ntables);
        if (ret != 0) {
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        uint32_t ntags = m->nimportedtags + m->ntags;
        ret = VEC_RESIZE(mctx, inst->tags, ntags);
        if (ret != 0) {
                goto fail;
        }
#endif

        ret = resolve_imports(inst, imports, report);
        if (ret != 0) {
                goto fail;
        }

        for (i = m->nimportedfuncs; i < nfuncs; i++) {
                struct funcinst *fp = mem_zalloc(mctx, sizeof(*fp));
                if (fp == NULL) {
                        ret = ENOMEM;
                        goto fail;
                }
                fp->is_host = false;
                fp->u.wasm.instance = inst;
                fp->u.wasm.funcidx = i;
                VEC_ELEM(inst->funcs, i) = fp;
        }

        for (i = m->nimportedmems; i < nmems; i++) {
                struct meminst *mp;
                const struct memtype *mt = module_memtype(m, i);
                ret = memory_instance_create(mctx, &mp, mt);
                if (ret != 0) {
                        goto fail;
                }
                VEC_ELEM(inst->mems, i) = mp;
        }

        for (i = m->nimportedglobals; i < nglobals; i++) {
                struct globalinst *ginst;
                const struct globaltype *gt = module_globaltype(m, i);
                ret = global_instance_create(mctx, &ginst, gt);
                if (ret != 0) {
                        goto fail;
                }
                VEC_ELEM(inst->globals, i) = ginst;
        }

        for (i = m->nimportedtables; i < ntables; i++) {
                struct tableinst *tinst;
                const struct tabletype *tt = module_tabletype(m, i);
                ret = table_instance_create(mctx, &tinst, tt);
                if (ret != 0) {
                        goto fail;
                }
                VEC_ELEM(inst->tables, i) = tinst;
        }

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        for (i = m->nimportedtags; i < ntags; i++) {
                struct taginst *tinst;
                const struct tagtype *tt = module_tagtype(m, i);
                const struct functype *ft = module_tagtype_functype(m, tt);
                ret = tag_instance_create(mctx, &tinst, ft);
                if (ret != 0) {
                        goto fail;
                }
                VEC_ELEM(inst->tags, i) = tinst;
        }
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */
        ret = bitmap_alloc(mctx, &inst->data_dropped, m->ndatas);
        if (ret != 0) {
                goto fail;
        }
        ret = bitmap_alloc(mctx, &inst->elem_dropped, m->nelems);
        if (ret != 0) {
                goto fail;
        }
        *instp = inst;
        return 0;
fail:
        if (inst != NULL) {
                instance_destroy(inst);
        }
        return ret;
}

int
instance_execute_init(struct exec_context *ctx)
{
        struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        uint32_t i;
        int ret;

		fprintf(stderr, "initializing globals\n");
        for (i = 0; i < m->nglobals; i++) {
                struct globalinst *ginst =
                        VEC_ELEM(inst->globals, m->nimportedglobals + i);
                ret = exec_const_expr(&m->globals[i].init, ginst->type->t,
                                      &ginst->val, ctx);
                if (ret != 0) {
                        goto fail;
                }
                xlog_trace("global [%" PRIu32 "] initialized to %016" PRIx64,
                           m->nimportedglobals + i, ginst->val.u.i64);
        }
		fprintf(stderr, "initializing data segments\n");
        for (i = 0; i < m->nelems; i++) {
                const struct element *elem = &m->elems[i];
                if (elem->mode == ELEM_MODE_ACTIVE) {
                        struct val val;
                        ret = exec_const_expr(&elem->offset, TYPE_i32, &val,
                                              ctx);
                        if (ret != 0) {
                                goto fail;
                        }
                        uint32_t offset = val.u.i32;
                        ret = table_init(ctx, elem->table, i, offset, 0,
                                         elem->init_size);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                if (elem->mode != ELEM_MODE_PASSIVE) {
                        elem_drop(ctx, i);
                }
        }
		fprintf(stderr, "initializing data segments\n");
        for (i = 0; i < m->ndatas; i++) {
                const struct data *d = &m->datas[i];
                if (d->mode != DATA_MODE_ACTIVE) {
                        continue;
                }
                struct val val;
                ret = exec_const_expr(&d->offset, TYPE_i32, &val, ctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t offset = val.u.i32;
                ret = memory_init(ctx, d->memory, i, offset, 0, d->init_size);
                if (ret != 0) {
                        goto fail;
                }
                data_drop(ctx, i);
        }
        if (m->has_start) {
                fprintf(stderr, "calling start function\n");
                assert(m->start < m->nimportedfuncs + m->nfuncs);
                struct funcinst *finst = VEC_ELEM(inst->funcs, m->start);
                return invoke(finst, NULL, NULL, ctx);
        }
        return 0;
fail:
        return ret;
}

void
instance_destroy(struct instance *inst)
{
        const struct module *m = inst->module;
        struct mem_context *mctx = inst->mctx;
        uint32_t i;
        struct funcinst **fp;
        VEC_FOREACH_IDX(i, fp, inst->funcs) {
                if (i < m->nimportedfuncs) {
                        continue;
                }
                mem_free(mctx, *fp, sizeof(**fp));
        }
        VEC_FREE(mctx, inst->funcs);
        struct meminst **mp;
        VEC_FOREACH_IDX(i, mp, inst->mems) {
                if (i < m->nimportedmems) {
                        continue;
                }
                memory_instance_destroy(mctx, *mp);
        }
        VEC_FREE(mctx, inst->mems);
        struct globalinst **gp;
        VEC_FOREACH_IDX(i, gp, inst->globals) {
                if (i < m->nimportedglobals) {
                        continue;
                }
                global_instance_destroy(mctx, *gp);
        }
        VEC_FREE(mctx, inst->globals);
        struct tableinst **tp;
        VEC_FOREACH_IDX(i, tp, inst->tables) {
                if (i < m->nimportedtables) {
                        continue;
                }
                table_instance_destroy(mctx, *tp);
        }
        VEC_FREE(mctx, inst->tables);
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        struct taginst **tagp;
        VEC_FOREACH_IDX(i, tagp, inst->tags) {
                if (i < m->nimportedtags) {
                        continue;
                }
                tag_instance_destroy(mctx, *tagp);
        }
        VEC_FREE(mctx, inst->tags);
#endif
        bitmap_free(mctx, &inst->data_dropped, m->ndatas);
        bitmap_free(mctx, &inst->elem_dropped, m->nelems);
        mem_free(mctx, inst, sizeof(*inst));
}

int
instance_execute_func(struct exec_context *ctx, uint32_t funcidx,
                      const struct resulttype *paramtype,
                      const struct resulttype *resulttype)
{
        struct funcinst *finst = VEC_ELEM(ctx->instance->funcs, funcidx);
        return invoke(finst, paramtype, resulttype, ctx);
}

int
instance_execute_continue(struct exec_context *ctx)
{
        return exec_expr_continue(ctx);
}

int
instance_execute_func_nocheck(struct exec_context *ctx, uint32_t funcidx)
{
        const struct module *m = ctx->instance->module;
        const struct functype *ft = module_functype(m, funcidx);
        const struct resulttype *ptype = &ft->parameter;
        const struct resulttype *rtype = &ft->result;
        return instance_execute_func(ctx, funcidx, ptype, rtype);
}

int
instance_execute_handle_restart_once(struct exec_context *ctx, int exec_ret)
{
        int ret = exec_ret;
        if (IS_RESTARTABLE(ret)) {
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                suspend_parked(ctx->cluster);
#endif
                xlog_trace("%s: restarting execution\n", __func__);
#if defined(TOYWASM_USE_USER_SCHED)
                struct sched *sched = ctx->sched;
                if (sched != NULL) {
                        sched_enqueue(sched, ctx);
                        sched_run(sched, ctx);
                        ret = ctx->exec_ret;
                } else
#endif
                {
                        ret = instance_execute_continue(ctx);
                }
        }
        return ret;
}

int
instance_execute_handle_restart(struct exec_context *ctx, int exec_ret)
{
        int ret = exec_ret;
        do {
                ret = instance_execute_handle_restart_once(ctx, ret);
        } while (IS_RESTARTABLE(ret));
        return ret;
}

void
instance_print_stats(const struct instance *inst)
{
        nbio_printf("=== instance statistics ===\n");
        uint32_t i;
        for (i = 0; i < inst->mems.lsize; i++) {
                const struct meminst *mi = VEC_ELEM(inst->mems, i);
                const struct limits *lim = &mi->type->lim;
                nbio_printf("memory[%" PRIu32
                            "] %zu bytes allocated for %" PRIu32
                            " pages (min/max=%" PRIu32 "/%" PRIu32
                            " pagesize=%" PRIu32 ")\n",
                            i, mi->allocated, mi->size_in_pages, lim->min,
                            lim->max, 1 << memtype_page_shift(mi->type));
        }
}
