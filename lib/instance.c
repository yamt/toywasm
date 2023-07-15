#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "exec.h"
#include "instance.h"
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
                size_t j;
                for (j = 0; j < impobj->nentries; j++) {
                        const struct import_object_entry *e =
                                &impobj->entries[j];
                        if (!compare_name(e->module_name, &im->module_name) &&
                            !compare_name(e->name, &im->name)) {
                                if (e->type != im->desc.type) {
                                        report_error(
                                                report,
                                                "Type mismatch for import "
                                                "%.*s:%.*s (%u != %u)",
                                                CSTR(&im->module_name),
                                                CSTR(&im->name),
                                                (unsigned int)e->type,
                                                (unsigned int)im->desc.type);
                                        return EINVAL;
                                }
                                int ret = check(e, checkarg);
                                if (ret == 0) {
                                        xlog_trace("Found an entry for import "
                                                   "%.*s:%.*s",
                                                   CSTR(&im->module_name),
                                                   CSTR(&im->name));
                                        *resultp = e;
                                        return 0;
                                }
                                mismatch = true;
                        }
                }
                impobj = impobj->next;
        }
        if (mismatch) {
                report_error(report, "No matching entry for import %.*s:%.*s",
                             CSTR(&im->module_name), CSTR(&im->name));
        } else {
                report_error(report, "No entry for import %.*s:%.*s",
                             CSTR(&im->module_name), CSTR(&im->name));
        }
        return ENOENT;
}

static int
find_import_entry(const struct module *m, enum externtype type, uint32_t idx,
                  const struct import_object *imports,
                  int (*check)(const struct import_object_entry *e,
                               const void *arg),
                  const void *checkarg,
                  const struct import_object_entry **resultp,
                  struct report *report)
{
        const struct import *im = module_find_import(m, type, idx);
        assert(im != NULL);
        return find_entry_for_import(imports, im, check, checkarg, resultp,
                                     report);
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

int
memory_instance_create(struct meminst **mip,
                       const struct memtype *mt) NO_THREAD_SAFETY_ANALYSIS
{
        struct meminst *mp;
        int ret;

        mp = zalloc(sizeof(*mp));
        if (mp == NULL) {
                ret = ENOMEM;
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        if ((mt->flags & MEMTYPE_FLAG_SHARED) != 0) {
#if defined(TOYWASM_PREALLOC_SHARED_MEMORY)
                uint32_t need_in_pages = mt->lim.max;
#else
                uint32_t need_in_pages = mt->lim.min;
#endif /* defined(TOYWASM_PREALLOC_SHARED_MEMORY) */
                uint64_t need_in_bytes = need_in_pages * WASM_PAGE_SIZE;
                if (need_in_bytes > SIZE_MAX) {
                        free(mp);
                        ret = EOVERFLOW;
                        goto fail;
                }
                mp->shared = zalloc(sizeof(*mp->shared));
                if (mp->shared == NULL) {
                        free(mp);
                        ret = ENOMEM;
                        goto fail;
                }
                if (need_in_bytes > 0) {
                        mp->data = zalloc(need_in_bytes);
                        if (mp->data == NULL) {
                                free(mp->shared);
                                free(mp);
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
        *mip = mp;
        return 0;
fail:
        return ret;
}

void
memory_instance_destroy(struct meminst *mi)
{
        if (mi != NULL) {
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                struct shared_meminst *shared = mi->shared;
                if (shared != NULL) {
                        toywasm_mutex_destroy(&shared->lock);
                        free(shared);
                }
#endif
                free(mi->data);
        }
        free(mi);
}

int
global_instance_create(struct globalinst **gip, const struct globaltype *gt)
{
        struct globalinst *ginst;
        int ret;
        ginst = zalloc(sizeof(*ginst));
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
global_instance_destroy(struct globalinst *gi)
{
        free(gi);
}

int
table_instance_create(struct tableinst **tip, const struct tabletype *tt)
{
        struct tableinst *tinst;
        int ret;
        tinst = zalloc(sizeof(*tinst));
        if (tinst == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        tinst->type = tt;
        tinst->size = tinst->type->lim.min;
        uint32_t csz = valtype_cellsize(tt->et);
        uint32_t ncells = tinst->size * csz;
        if (ncells / csz != tinst->size) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = ARRAY_RESIZE(tinst->cells, ncells);
        if (ret != 0) {
                free(tinst);
                goto fail;
        }
        memset(tinst->cells, 0, ncells * sizeof(*tinst->cells));
        *tip = tinst;
fail:
        return ret;
}

void
table_instance_destroy(struct tableinst *ti)
{
        if (ti != NULL) {
                free(ti->cells);
        }
        free(ti);
}

/* https://webassembly.github.io/spec/core/exec/modules.html#exec-instantiation
 */

int
instance_create(const struct module *m, struct instance **instp,
                const struct import_object *imports, struct report *report)
{
        struct instance *inst;
        int ret;

        ret = instance_create_no_init(m, &inst, imports, report);
        if (ret != 0) {
                return ret;
        }

        struct exec_context ctx0;
        struct exec_context *ctx = NULL;
        ctx = &ctx0;
        exec_context_init(ctx, inst);
        ctx->report = report;
        ret = instance_create_execute_init(inst, ctx);
        exec_context_clear(ctx);
        if (ret != 0) {
                instance_destroy(inst);
        } else {
                *instp = inst;
        }
        return ret;
}

int
instance_create_no_init(const struct module *m, struct instance **instp,
                        const struct import_object *imports,
                        struct report *report)
{
        struct instance *inst;
        uint32_t i;
        int ret;

        inst = zalloc(sizeof(*inst));
        if (inst == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        inst->module = m;

        uint32_t nfuncs = m->nimportedfuncs + m->nfuncs;
        ret = VEC_RESIZE(inst->funcs, nfuncs);
        if (ret != 0) {
                goto fail;
        }
        for (i = 0; i < nfuncs; i++) {
                struct funcinst *fp;
                if (i < m->nimportedfuncs) {
                        const struct import_object_entry *e;
                        ret = find_import_entry(
                                m, EXTERNTYPE_FUNC, i, imports, check_functype,
                                module_functype(m, i), &e, report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == EXTERNTYPE_FUNC);
                        fp = e->u.func;
                        assert(fp != NULL);
                } else {
                        fp = zalloc(sizeof(*fp));
                        if (fp == NULL) {
                                ret = ENOMEM;
                                goto fail;
                        }
                        fp->is_host = false;
                        fp->u.wasm.instance = inst;
                        fp->u.wasm.funcidx = i;
                }
                VEC_ELEM(inst->funcs, i) = fp;
        }

        uint32_t nmems = m->nimportedmems + m->nmems;
        ret = VEC_RESIZE(inst->mems, nmems);
        if (ret != 0) {
                goto fail;
        }
        for (i = 0; i < nmems; i++) {
                struct meminst *mp;
                const struct memtype *mt = module_memtype(m, i);
                if (i < m->nimportedmems) {
                        const struct import_object_entry *e;
                        ret = find_import_entry(m, EXTERNTYPE_MEMORY, i,
                                                imports, check_memtype, mt, &e,
                                                report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == EXTERNTYPE_MEMORY);
                        mp = e->u.mem;
                        assert(mp != NULL);
                } else {
                        ret = memory_instance_create(&mp, mt);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                VEC_ELEM(inst->mems, i) = mp;
        }

        uint32_t nglobals = m->nimportedglobals + m->nglobals;
        ret = VEC_RESIZE(inst->globals, nglobals);
        if (ret != 0) {
                goto fail;
        }
        for (i = 0; i < nglobals; i++) {
                struct globalinst *ginst;
                const struct globaltype *gt = module_globaltype(m, i);
                if (i < m->nimportedglobals) {
                        const struct import_object_entry *e;
                        ret = find_import_entry(m, EXTERNTYPE_GLOBAL, i,
                                                imports, check_globaltype, gt,
                                                &e, report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == EXTERNTYPE_GLOBAL);
                        ginst = e->u.global;
                        assert(ginst != NULL);
                } else {
                        ret = global_instance_create(&ginst, gt);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                VEC_ELEM(inst->globals, i) = ginst;
        }

        uint32_t ntables = m->nimportedtables + m->ntables;
        ret = VEC_RESIZE(inst->tables, ntables);
        if (ret != 0) {
                goto fail;
        }
        for (i = 0; i < ntables; i++) {
                struct tableinst *tinst;
                const struct tabletype *tt = module_tabletype(m, i);
                if (i < m->nimportedtables) {
                        const struct import_object_entry *e;
                        ret = find_import_entry(m, EXTERNTYPE_TABLE, i,
                                                imports, check_tabletype, tt,
                                                &e, report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == EXTERNTYPE_TABLE);
                        tinst = e->u.table;
                        assert(tinst != NULL);
                } else {
                        ret = table_instance_create(&tinst, tt);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                VEC_ELEM(inst->tables, i) = tinst;
        }
        ret = bitmap_alloc(&inst->data_dropped, m->ndatas);
        if (ret != 0) {
                goto fail;
        }
        ret = bitmap_alloc(&inst->elem_dropped, m->nelems);
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
instance_create_execute_init(struct instance *inst, struct exec_context *ctx)
{
        const struct module *m = inst->module;
        uint32_t i;
        int ret;

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
                assert(m->start < m->nimportedfuncs + m->nfuncs);
                struct funcinst *finst = VEC_ELEM(inst->funcs, m->start);
                ret = invoke(finst, NULL, NULL, ctx);
                while (IS_RESTARTABLE(ret)) {
                        xlog_trace("%s: restarting execution of the start "
                                   "function\n",
                                   __func__);
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                        suspend_parked(ctx->cluster);
#endif
                        ret = exec_expr_continue(ctx);
                }
                if (ret != 0) {
                        goto fail;
                }
        }
        return 0;
fail:
        return ret;
}

void
instance_destroy(struct instance *inst)
{
        const struct module *m = inst->module;
        uint32_t i;
        struct funcinst **fp;
        VEC_FOREACH_IDX(i, fp, inst->funcs) {
                if (i < m->nimportedfuncs) {
                        continue;
                }
                free(*fp);
        }
        VEC_FREE(inst->funcs);
        struct meminst **mp;
        VEC_FOREACH_IDX(i, mp, inst->mems) {
                if (i < m->nimportedmems) {
                        continue;
                }
                memory_instance_destroy(*mp);
        }
        VEC_FREE(inst->mems);
        struct globalinst **gp;
        VEC_FOREACH_IDX(i, gp, inst->globals) {
                if (i < m->nimportedglobals) {
                        continue;
                }
                global_instance_destroy(*gp);
        }
        VEC_FREE(inst->globals);
        struct tableinst **tp;
        VEC_FOREACH_IDX(i, tp, inst->tables) {
                if (i < m->nimportedtables) {
                        continue;
                }
                table_instance_destroy(*tp);
        }
        VEC_FREE(inst->tables);
        bitmap_free(&inst->data_dropped);
        bitmap_free(&inst->elem_dropped);
        free(inst);
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
        printf("=== instance statistics ===\n");
        uint32_t i;
        for (i = 0; i < inst->mems.lsize; i++) {
                const struct meminst *mi = VEC_ELEM(inst->mems, i);
                nbio_printf("memory[%" PRIu32 "] %zu bytes allocated\n", i,
                            mi->allocated);
        }
}
