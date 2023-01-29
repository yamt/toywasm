
INSN_IMPL(drop)
{
        LOAD_PC;
        int ret;
        if (EXECUTING) {
                uint32_t csz = find_type_annotation(ECTX, p);
                STACK_ADJ(-(int32_t)csz);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                enum valtype type_a;
                ret = pop_valtype(TYPE_UNKNOWN, &type_a, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = record_type_annotation(vctx, p, type_a);
                if (ret != 0) {
                        goto fail;
                }
        }
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(select)
{
        LOAD_PC;
        int ret;
        POP_VAL(TYPE_i32, cond);
        struct val val_c;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz = find_type_annotation(ectx, p);
                struct val val_v2;
                pop_val(&val_v2, csz, ectx);
                struct val val_v1;
                pop_val(&val_v1, csz, ectx);
                val_c = val_cond.u.i32 != 0 ? val_v1 : val_v2;
                push_val(&val_c, csz, ectx);
        } else if (VALIDATING) {
                POP_VAL(TYPE_UNKNOWN, v2);
                POP_VAL(type_v2, v1);
                ret = record_type_annotation(VCTX, p, type_v2);
                if (ret != 0) {
                        goto fail;
                }
                CHECK(is_numtype(type_v2) || is_vectype(type_v2) ||
                      type_v2 == TYPE_UNKNOWN);
                PUSH_VAL(type_v2, c);
        }
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(select_t)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(vec_count);
        CHECK(vec_count == 1);
        uint8_t u8;
        ret = read_u8(&p, ep, &u8);
        CHECK_RET(ret);
        enum valtype t = u8;
        CHECK(is_valtype(t));
        POP_VAL(TYPE_i32, cond);
        POP_VAL(t, v2);
        POP_VAL(t, v1);
        struct val val_c;
        if (EXECUTING) {
                val_c = val_cond.u.i32 != 0 ? val_v1 : val_v2;
        }
        PUSH_VAL(t, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(local_get)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(localidx);
        struct val val_c;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz;
                local_get(ectx, localidx, STACK, &csz);
                STACK_ADJ(csz);
        } else if (VALIDATING) {
                CHECK(localidx < VCTX->nlocals);
                PUSH_VAL(VCTX->locals[localidx], c);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(local_set)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(localidx);
        if (VALIDATING) {
                CHECK(localidx < VCTX->nlocals);
        }
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz;
                local_set(ectx, localidx, STACK, &csz);
                STACK_ADJ(-(int32_t)csz);
        } else {
                POP_VAL(VCTX->locals[localidx], a);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(local_tee)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(localidx);
        if (VALIDATING) {
                CHECK(localidx < VCTX->nlocals);
        }
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz;
                local_set(ectx, localidx, STACK, &csz);
        } else {
                POP_VAL(VCTX->locals[localidx], a);
                PUSH_VAL(VCTX->locals[localidx], a);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(global_get)
{
        const struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(globalidx);
        CHECK(globalidx < m->nimportedglobals + m->nglobals);
        struct val val_c;
        if (EXECUTING) {
                val_c = VEC_ELEM(ECTX->instance->globals, globalidx)->val;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                if (vctx->const_expr) {
                        /*
                         * Note: we are only allowed to refer imported globals.
                         * Basically to avoid possible complexities from
                         * cycles.
                         *
                         * cf.
                         * https://github.com/WebAssembly/spec/issues/367
                         * https://github.com/WebAssembly/spec/issues/1522
                         */
                        if (globalidx >= m->nimportedglobals) {
                                ret = EINVAL;
                                goto fail;
                        }
                        const struct globaltype *t =
                                module_globaltype(m, globalidx);
                        if (t->mut != GLOBAL_CONST) {
                                ret = EINVAL;
                                goto fail;
                        }
                }
        }
        PUSH_VAL(module_globaltype(m, globalidx)->t, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(global_set)
{
        const struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(globalidx);
        CHECK(globalidx < m->nimportedglobals + m->nglobals);
        const struct globaltype *gt;
#if defined(__GNUC__) && !defined(__clang__)
        /* suppress warnings */
        gt = NULL;
#endif
        if (EXECUTING || VALIDATING) {
                gt = module_globaltype(m, globalidx);
                CHECK(gt->mut != GLOBAL_CONST);
        }
        POP_VAL(gt->t, a);
        if (EXECUTING) {
                VEC_ELEM(ECTX->instance->globals, globalidx)->val = val_a;
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_get)
{
        const struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(TYPE_i32, offset);
        struct val val_c;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t offset = val_offset.u.i32;
                ret = table_access(ectx, tableidx, offset, 1);
                if (ret != 0) {
                        goto fail;
                }
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t csz = valtype_cellsize(t->type->et);
                val_from_cells(&val_c, &t->cells[offset * csz], csz);
        }
        PUSH_VAL(module_tabletype(m, tableidx)->et, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_set)
{
        const struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(module_tabletype(m, tableidx)->et, a);
        POP_VAL(TYPE_i32, offset);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t offset = val_offset.u.i32;
                ret = table_access(ectx, tableidx, offset, 1);
                if (ret != 0) {
                        goto fail;
                }
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t csz = valtype_cellsize(t->type->et);
                val_to_cells(&val_a, &t->cells[offset * csz], csz);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

STOREOP(i32_store, 32, 32, )
STOREOP(i64_store, 64, 64, )
STOREOP_F(f32_store, 32, 32, )
STOREOP_F(f64_store, 64, 64, )
STOREOP(i32_store8, 8, 32, )
STOREOP(i32_store16, 16, 32, )
STOREOP(i64_store8, 8, 64, )
STOREOP(i64_store16, 16, 64, )
STOREOP(i64_store32, 32, 64, )

INSN_IMPL(memory_size)
{
        int ret;
        LOAD_PC;
        READ_MEMIDX(memidx);
        const struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        struct val val_sz;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                struct meminst *minst = VEC_ELEM(inst->mems, memidx);
                val_sz.u.i32 = minst->size_in_pages;
        }
        PUSH_VAL(TYPE_i32, sz);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(memory_grow)
{
        int ret;
        LOAD_PC;
        READ_MEMIDX(memidx);
        const struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        struct val val_error;
        if (EXECUTING) {
                val_error.u.i32 = memory_grow(ECTX, memidx, val_n.u.i32);
        }
        PUSH_VAL(TYPE_i32, error);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

LOADOP(i32_load, 32, 32, )
LOADOP(i64_load, 64, 64, )
LOADOP_F(f32_load, 32, 32, )
LOADOP_F(f64_load, 64, 64, )

LOADOP(i32_load8_s, 8, 32, (int32_t)(int8_t))
LOADOP(i32_load8_u, 8, 32, (uint32_t)(uint8_t))
LOADOP(i32_load16_s, 16, 32, (int32_t)(int16_t))
LOADOP(i32_load16_u, 16, 32, (uint32_t)(uint16_t))

LOADOP(i64_load8_s, 8, 64, (int64_t)(int8_t))
LOADOP(i64_load8_u, 8, 64, (uint64_t)(uint8_t))
LOADOP(i64_load16_s, 16, 64, (int64_t)(int16_t))
LOADOP(i64_load16_u, 16, 64, (uint64_t)(uint16_t))
LOADOP(i64_load32_s, 32, 64, (int64_t)(int32_t))
LOADOP(i64_load32_u, 32, 64, (uint64_t)(uint32_t))

CONSTOP(i32_const, 32, i32)
CONSTOP(i64_const, 64, i64)
CONSTOP_F(f32_const, 32, f32)
CONSTOP_F(f64_const, 64, f64)

TESTOP(i32_eqz, i, 32, EQZ)
CMPOP(i32_eq, 32, uint32_t, ==)
CMPOP(i32_ne, 32, uint32_t, !=)
CMPOP(i32_lt_s, 32, int32_t, <)
CMPOP(i32_lt_u, 32, uint32_t, <)
CMPOP(i32_gt_s, 32, int32_t, >)
CMPOP(i32_gt_u, 32, uint32_t, >)
CMPOP(i32_le_s, 32, int32_t, <=)
CMPOP(i32_le_u, 32, uint32_t, <=)
CMPOP(i32_ge_s, 32, int32_t, >=)
CMPOP(i32_ge_u, 32, uint32_t, >=)

CMPOP_F(f32_eq, 32, ==)
CMPOP_F(f32_ne, 32, !=)
CMPOP_F(f32_lt, 32, <)
CMPOP_F(f32_gt, 32, >)
CMPOP_F(f32_le, 32, <=)
CMPOP_F(f32_ge, 32, >=)

CMPOP_F(f64_eq, 64, ==)
CMPOP_F(f64_ne, 64, !=)
CMPOP_F(f64_lt, 64, <)
CMPOP_F(f64_gt, 64, >)
CMPOP_F(f64_le, 64, <=)
CMPOP_F(f64_ge, 64, >=)

TESTOP(i64_eqz, i, 64, EQZ)
CMPOP(i64_eq, 64, uint64_t, ==)
CMPOP(i64_ne, 64, uint64_t, !=)
CMPOP(i64_lt_s, 64, int64_t, <)
CMPOP(i64_lt_u, 64, uint64_t, <)
CMPOP(i64_gt_s, 64, int64_t, >)
CMPOP(i64_gt_u, 64, uint64_t, >)
CMPOP(i64_le_s, 64, int64_t, <=)
CMPOP(i64_le_u, 64, uint64_t, <=)
CMPOP(i64_ge_s, 64, int64_t, >=)
CMPOP(i64_ge_u, 64, uint64_t, >=)

BITCOUNTOP(i32_clz, i32, clz)
BITCOUNTOP(i32_ctz, i32, ctz)
BITCOUNTOP(i32_popcnt, i32, wasm_popcount)

BINOP(i32_add, i, 32, ADD)
BINOP(i32_sub, i, 32, SUB)
BINOP(i32_mul, i, 32, MUL)

BINOP2(i32_div_u, i, 32, DIV_U, true, false, true)
BINOP2(i32_div_s, i, 32, DIV_S, true, true, true)
BINOP2(i32_rem_u, i, 32, REM_U, true, false, false)
BINOP2(i32_rem_s, i, 32, REM_S, true, true, false)

BINOP(i32_and, i, 32, AND)
BINOP(i32_or, i, 32, OR)
BINOP(i32_xor, i, 32, XOR)

BINOP(i32_shl, i, 32, SHL)
BINOP(i32_shr_s, i, 32, SHR_S)
BINOP(i32_shr_u, i, 32, SHR_U)
BINOP(i32_rotl, i, 32, ROTL)
BINOP(i32_rotr, i, 32, ROTR)

BITCOUNTOP(i64_clz, i64, clz64)
BITCOUNTOP(i64_ctz, i64, ctz64)
BITCOUNTOP(i64_popcnt, i64, wasm_popcount64)

BINOP(i64_add, i, 64, ADD)
BINOP(i64_sub, i, 64, SUB)
BINOP(i64_mul, i, 64, MUL)

BINOP2(i64_div_u, i, 64, DIV_U, true, false, true)
BINOP2(i64_div_s, i, 64, DIV_S, true, true, true)
BINOP2(i64_rem_u, i, 64, REM_U, true, false, false)
BINOP2(i64_rem_s, i, 64, REM_S, true, true, false)

BINOP(i64_and, i, 64, AND)
BINOP(i64_or, i, 64, OR)
BINOP(i64_xor, i, 64, XOR)

BINOP(i64_shl, i, 64, SHL)
BINOP(i64_shr_s, i, 64, SHR_S)
BINOP(i64_shr_u, i, 64, SHR_U)
BINOP(i64_rotl, i, 64, ROTL)
BINOP(i64_rotr, i, 64, ROTR)

UNOP(f32_abs, f, 32, fabsf)
UNOP(f32_neg, f, 32, -)
UNOP(f32_ceil, f, 32, ceilf)
UNOP(f32_floor, f, 32, floorf)
UNOP(f32_trunc, f, 32, truncf)
UNOP(f32_nearest, f, 32, rintf)
UNOP(f32_sqrt, f, 32, sqrtf)

BINOP(f32_add, f, 32, ADD);
BINOP(f32_sub, f, 32, SUB);
BINOP(f32_mul, f, 32, MUL);
BINOP(f32_div, f, 32, FDIV);
BINOP(f32_max, f, 32, FMAX32);
BINOP(f32_min, f, 32, FMIN32);
BINOP(f32_copysign, f, 32, FCOPYSIGN32);

UNOP(f64_abs, f, 64, fabs)
UNOP(f64_neg, f, 64, -)
UNOP(f64_ceil, f, 64, ceil)
UNOP(f64_floor, f, 64, floor)
UNOP(f64_trunc, f, 64, trunc)
UNOP(f64_nearest, f, 64, rint)
UNOP(f64_sqrt, f, 64, sqrt)

BINOP(f64_add, f, 64, ADD);
BINOP(f64_sub, f, 64, SUB);
BINOP(f64_mul, f, 64, MUL);
BINOP(f64_div, f, 64, FDIV);
BINOP(f64_max, f, 64, FMAX64);
BINOP(f64_min, f, 64, FMIN64);
BINOP(f64_copysign, f, 64, FCOPYSIGN64);

EXTENDOP(i32_wrap_i64, i64, i32, (uint32_t))

CVTOP(i32_trunc_f32_s, f32, i32, TRUNC_S_32_32)
CVTOP(i32_trunc_f32_u, f32, i32, TRUNC_U_32_32)
CVTOP(i32_trunc_f64_s, f64, i32, TRUNC_S_64_32)
CVTOP(i32_trunc_f64_u, f64, i32, TRUNC_U_64_32)

EXTENDOP(i64_extend_i32_s, i32, i64, (int64_t)(int32_t))
EXTENDOP(i64_extend_i32_u, i32, i64, (uint64_t)(uint32_t))

CVTOP(i64_trunc_f32_s, f32, i64, TRUNC_S_32_64)
CVTOP(i64_trunc_f32_u, f32, i64, TRUNC_U_32_64)
CVTOP(i64_trunc_f64_s, f64, i64, TRUNC_S_64_64)
CVTOP(i64_trunc_f64_u, f64, i64, TRUNC_U_64_64)

EXTENDOP(f32_convert_i32_s, i32, f32, (float)(int32_t))
EXTENDOP(f32_convert_i32_u, i32, f32, (float)(uint32_t))
EXTENDOP(f32_convert_i64_u, i64, f32, (float)(uint64_t))
EXTENDOP(f32_convert_i64_s, i64, f32, (float)(int64_t))
EXTENDOP(f32_demote_f64, f64, f32, (float))

EXTENDOP(f64_convert_i32_s, i32, f64, (double)(int32_t))
EXTENDOP(f64_convert_i32_u, i32, f64, (double)(uint32_t))
EXTENDOP(f64_convert_i64_u, i64, f64, (double)(uint64_t))
EXTENDOP(f64_convert_i64_s, i64, f64, (double)(int64_t))
EXTENDOP(f64_promote_f32, f32, f64, (double))

REINTERPRETOP(i32_reinterpret_f32, f32, i32)
REINTERPRETOP(i64_reinterpret_f64, f64, i64)
REINTERPRETOP(f32_reinterpret_i32, i32, f32)
REINTERPRETOP(f64_reinterpret_i64, i64, f64)

EXTENDOP(i32_extend8_s, i32, i32, (int32_t)(int8_t))
EXTENDOP(i32_extend16_s, i32, i32, (int32_t)(int16_t))
EXTENDOP(i64_extend8_s, i64, i64, (int64_t)(int8_t))
EXTENDOP(i64_extend16_s, i64, i64, (int64_t)(int16_t))
EXTENDOP(i64_extend32_s, i64, i64, (int64_t)(int32_t))

INSN_IMPL(ref_null)
{
        int ret;
        LOAD_PC;
        uint8_t u8;
        ret = read_u8(&p, ep, &u8);
        CHECK_RET(ret);
        enum valtype type = u8;
        CHECK(is_reftype(type));
        struct val val_null;
        if (EXECUTING) {
                memset(&val_null, 0, sizeof(val_null));
        }
        PUSH_VAL(type, null);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(ref_is_null)
{
        /*
         * REVISIT: is this a correct interpretation of the spec
         * to treat ref.is_null as a value-polymorphic instruction?
         *
         * Subtyping and "anyref" has some back-and-forth history:
         * https://github.com/WebAssembly/reference-types/pull/43
         * https://github.com/WebAssembly/reference-types/pull/87
         * https://github.com/WebAssembly/reference-types/pull/100
         * https://github.com/WebAssembly/reference-types/pull/116
         */
        int ret;
        LOAD_PC;
        struct val val_result;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz = find_type_annotation(ectx, p);
                struct val val_n;
                pop_val(&val_n, csz, ectx);
                val_result.u.i32 = (int)(val_n.u.funcref.func == NULL);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                enum valtype type_n;
                ret = pop_valtype(TYPE_ANYREF, &type_n, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = record_type_annotation(VCTX, p, type_n);
                if (ret != 0) {
                        goto fail;
                }
        }
        PUSH_VAL(TYPE_i32, result);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(ref_func)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(funcidx);
        const struct module *m = MODULE;
        CHECK(funcidx < m->nimportedfuncs + m->nfuncs);
        struct val val_result;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                val_result.u.funcref.func =
                        VEC_ELEM(ectx->instance->funcs, funcidx);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                /*
                 * REVISIT: a bit abuse of const_expr.
                 * what we want to check here is if it's in the code
                 * section or not.
                 */
                if (vctx->const_expr) {
                        bitmap_set(vctx->refs, funcidx);
                } else {
                        /*
                         * the code section.
                         */
                        if (!bitmap_test(vctx->refs, funcidx)) {
                                ret = validation_failure(
                                        vctx, "funcref %" PRIu32, funcidx);
                                goto fail;
                        }
                }
        }
        PUSH_VAL(TYPE_FUNCREF, result);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}
