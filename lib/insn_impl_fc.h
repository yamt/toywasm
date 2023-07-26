CVTOP(i32_trunc_sat_f32_s, f32, i32, TRUNC_SAT_S_32_32)
CVTOP(i32_trunc_sat_f32_u, f32, i32, TRUNC_SAT_U_32_32)
CVTOP(i32_trunc_sat_f64_s, f64, i32, TRUNC_SAT_S_64_32)
CVTOP(i32_trunc_sat_f64_u, f64, i32, TRUNC_SAT_U_64_32)

CVTOP(i64_trunc_sat_f32_s, f32, i64, TRUNC_SAT_S_32_64)
CVTOP(i64_trunc_sat_f32_u, f32, i64, TRUNC_SAT_U_32_64)
CVTOP(i64_trunc_sat_f64_s, f64, i64, TRUNC_SAT_S_64_64)
CVTOP(i64_trunc_sat_f64_u, f64, i64, TRUNC_SAT_U_64_64)

INSN_IMPL(memory_init)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(dataidx);
        READ_MEMIDX(memidx);
        const struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t d = val_d.u.i32;
                uint32_t s = val_s.u.i32;
                uint32_t n = val_n.u.i32;
                ret = memory_init(ectx, memidx, dataidx, d, s, n);
                if (ret != 0) {
                        goto fail;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                CHECK(vctx->has_datacount);
                CHECK(dataidx < vctx->ndatas_in_datacount);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(data_drop)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(dataidx);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                data_drop(ectx, dataidx);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                CHECK(vctx->has_datacount);
                CHECK(dataidx < vctx->ndatas_in_datacount);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(memory_copy)
{
        int ret;
        LOAD_PC;
        READ_MEMIDX(memidx_dst);
        READ_MEMIDX(memidx_src);
        const struct module *m = MODULE;
        CHECK(memidx_dst < m->nimportedmems + m->nmems);
        CHECK(memidx_src < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t n = val_n.u.i32;
                void *src_p;
                void *dst_p;
                bool moved;
retry:
                ret = memory_getptr(ectx, memidx_src, val_s.u.i32, 0, n,
                                    &src_p);
                if (ret != 0) {
                        goto fail;
                }
                moved = false;
                ret = memory_getptr2(ectx, memidx_dst, val_d.u.i32, 0, n,
                                     &dst_p, &moved);
                if (ret != 0) {
                        goto fail;
                }
                if (moved) {
                        goto retry;
                }
                memmove(dst_p, src_p, n);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(memory_fill)
{
        int ret;
        LOAD_PC;
        READ_MEMIDX(memidx);
        const struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, val);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                void *vp;
                uint32_t n = val_n.u.i32;
                ret = memory_getptr(ectx, memidx, val_d.u.i32, 0, n, &vp);
                if (ret != 0) {
                        goto fail;
                }
                memset(vp, (uint8_t)val_val.u.i32, n);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(table_init)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(elemidx);
        READ_LEB_U32(tableidx);
        const struct module *m = MODULE;
        CHECK(elemidx < m->nelems);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        CHECK(m->tables[tableidx].et == m->elems[elemidx].type);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t d = val_d.u.i32;
                uint32_t s = val_s.u.i32;
                uint32_t n = val_n.u.i32;
                ret = table_init(ectx, tableidx, elemidx, d, s, n);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(elem_drop)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(elemidx);
        const struct module *m = MODULE;
        CHECK(elemidx < m->nelems);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                elem_drop(ectx, elemidx);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(table_copy)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx_dst);
        READ_LEB_U32(tableidx_src);
        const struct module *m = MODULE;
        CHECK(tableidx_dst < m->nimportedtables + m->ntables);
        CHECK(tableidx_src < m->nimportedtables + m->ntables);
        CHECK(m->tables[tableidx_dst].et == m->tables[tableidx_src].et);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                const struct instance *inst = ectx->instance;
                uint32_t d = val_d.u.i32;
                uint32_t s = val_s.u.i32;
                uint32_t n = val_n.u.i32;
                ret = table_access(ectx, tableidx_dst, d, n);
                if (ret != 0) {
                        goto fail;
                }
                ret = table_access(ectx, tableidx_src, s, n);
                if (ret != 0) {
                        goto fail;
                }
                const struct tableinst *t_dst =
                        VEC_ELEM(inst->tables, tableidx_dst);
                const struct tableinst *t_src =
                        VEC_ELEM(inst->tables, tableidx_src);
                assert(t_src->type->et == t_dst->type->et);
                uint32_t csz = valtype_cellsize(t_src->type->et);
                cells_move(&t_dst->cells[d * csz], &t_src->cells[s * csz],
                           n * csz);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(table_grow)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx);
        const struct module *m = MODULE;
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(TYPE_i32, n);
        POP_VAL(module_tabletype(m, tableidx)->et, val);
        struct val val_result;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                const struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t n = val_n.u.i32;
                val_result.u.i32 = table_grow(t, &val_val, n);
        }
        PUSH_VAL(TYPE_i32, result);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(table_size)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx);
        const struct module *m = MODULE;
        CHECK(tableidx < m->nimportedtables + m->ntables);
        struct val val_n;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                const struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                val_n.u.i32 = t->size;
        }
        PUSH_VAL(TYPE_i32, n);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(table_fill)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx);
        const struct module *m = MODULE;
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(TYPE_i32, n);
        POP_VAL(module_tabletype(m, tableidx)->et, val);
        POP_VAL(TYPE_i32, i);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t start = val_i.u.i32;
                uint32_t n = val_n.u.i32;
                ret = table_access(ectx, tableidx, start, n);
                if (ret != 0) {
                        goto fail;
                }
                const struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t end = start + n;
                uint32_t i;
                uint32_t csz = valtype_cellsize(t->type->et);
                for (i = start; i < end; i++) {
                        val_to_cells(&val_val, &t->cells[i * csz], csz);
                }
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}
