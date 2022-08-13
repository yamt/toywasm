#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "context.h"
#include "exec.h"
#include "instance.h"
#include "module.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int
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

int
find_import_entry(const struct module *m, enum importtype type, uint32_t idx,
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

bool
match_limits(const struct limits *a, const struct limits *b)
{
        if (a->min >= b->min && (b->max == UINT32_MAX ||
                                 (a->max != UINT32_MAX &&
                                  b->max != UINT32_MAX && a->max <= b->max))) {
                return true;
        }
        return false;
}

bool
match_tabletype(const struct tabletype *a, const struct tabletype *b)
{
        return a->et == b->et && match_limits(&a->lim, &b->lim);
}

int
check_functype(const struct import_object_entry *e, const void *vp)
{
        const struct functype *ft = vp;
        assert(e->type == IMPORT_FUNC);
        struct funcinst *fi = e->u.func;
        const struct functype *ft_imported = funcinst_functype(fi);
        if (compare_functype(ft, ft_imported)) {
                return EINVAL;
        }
        return 0;
}

int
check_tabletype(const struct import_object_entry *e, const void *vp)
{
        const struct tabletype *tt = vp;
        assert(e->type == IMPORT_TABLE);
        const struct tabletype *tt_imported = e->u.table->type;
        if (!match_tabletype(tt_imported, tt)) {
                return EINVAL;
        }
        return 0;
}

int
check_memtype(const struct import_object_entry *e, const void *vp)
{
        const struct limits *mt = vp;
        assert(e->type == IMPORT_MEMORY);
        const struct limits *mt_imported = e->u.mem->type;
        if (!match_limits(mt_imported, mt)) {
                return EINVAL;
        }
        return 0;
}

int
check_globaltype(const struct import_object_entry *e, const void *vp)
{
        const struct globaltype *gt = vp;
        assert(e->type == IMPORT_GLOBAL);
        const struct globaltype *gt_imported = e->u.global->type;
        if (gt_imported->t != gt->t || gt_imported->mut != gt->mut) {
                return EINVAL;
        }
        return 0;
}

/* https://webassembly.github.io/spec/core/exec/modules.html#exec-instantiation
 */

int
instance_create(struct module *m, struct instance **instp,
                const struct import_object *imports, struct report *report)
{
        struct exec_context ctx0;
        struct exec_context *ctx = NULL;
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
                                m, IMPORT_FUNC, i, imports, check_functype,
                                module_functype(m, i), &e, report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == IMPORT_FUNC);
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
                const struct limits *mt = module_memtype(m, i);
                if (i < m->nimportedmems) {
                        const struct import_object_entry *e;
                        ret = find_import_entry(m, IMPORT_MEMORY, i, imports,
                                                check_memtype, mt, &e, report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == IMPORT_MEMORY);
                        mp = e->u.mem;
                        assert(mp != NULL);
                } else {
                        mp = zalloc(sizeof(*mp));
                        if (mp == NULL) {
                                ret = ENOMEM;
                                goto fail;
                        }
                        mp->type = mt;
                        mp->size_in_pages = mt->min;
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
                        ret = find_import_entry(m, IMPORT_GLOBAL, i, imports,
                                                check_globaltype, gt, &e,
                                                report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == IMPORT_GLOBAL);
                        ginst = e->u.global;
                        assert(ginst != NULL);
                } else {
                        ginst = zalloc(sizeof(*ginst));
                        if (ginst == NULL) {
                                ret = ENOMEM;
                                goto fail;
                        }
                        ginst->type = gt;
                        memset(&ginst->val, 0, sizeof(ginst->val));
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
                        ret = find_import_entry(m, IMPORT_TABLE, i, imports,
                                                check_tabletype, tt, &e,
                                                report);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(e->type == IMPORT_TABLE);
                        tinst = e->u.table;
                        assert(tinst != NULL);
                } else {
                        tinst = zalloc(sizeof(*tinst));
                        if (tinst == NULL) {
                                ret = ENOMEM;
                                goto fail;
                        }
                        tinst->type = tt;
                        tinst->size = tinst->type->lim.min;
                        ret = ARRAY_RESIZE(tinst->vals, tinst->size);
                        if (ret != 0) {
                                free(tinst);
                                goto fail;
                        }
                        memset(tinst->vals, 0,
                               tinst->size * sizeof(tinst->vals));
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
        ctx = &ctx0;
        exec_context_init(ctx, inst);
        ctx->report = report;
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
                assert(d->memory == 0);
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
                ret = invoke(m->start, NULL, NULL, NULL, NULL, ctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        exec_context_clear(ctx);
        *instp = inst;
        return 0;
fail:
        if (ctx != NULL) {
                exec_context_clear(ctx);
        }
        if (inst != NULL) {
                instance_destroy(inst);
        }
        return ret;
}

void
instance_destroy(struct instance *inst)
{
        struct module *m = inst->module;
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
                if (*mp != NULL) {
                        free((*mp)->data);
                }
                free(*mp);
        }
        VEC_FREE(inst->mems);
        struct globalinst **gp;
        VEC_FOREACH_IDX(i, gp, inst->globals) {
                if (i < m->nimportedglobals) {
                        continue;
                }
                free(*gp);
        }
        VEC_FREE(inst->globals);
        struct tableinst **tp;
        VEC_FOREACH(tp, inst->tables) {
                if (i < m->nimportedtables) {
                        continue;
                }
                if (*tp != NULL) {
                        free((*tp)->vals);
                }
                free(*tp);
        }
        VEC_FREE(inst->tables);
        bitmap_free(&inst->data_dropped);
        bitmap_free(&inst->elem_dropped);
        free(inst);
}

int
instance_execute_func(struct exec_context *ctx, uint32_t funcidx,
                      const struct resulttype *paramtype,
                      const struct resulttype *resulttype,
                      const struct val *params, struct val *results)
{
        return invoke(funcidx, paramtype, resulttype, params, results, ctx);
}
