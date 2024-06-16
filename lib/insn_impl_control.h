
INSN_IMPL(unreachable)
{
        if (EXECUTING) {
                LOAD_PC;
                struct exec_context *ectx = ECTX;
                const struct module *m = ectx->instance->module;
                return trap_with_id(ectx, TRAP_UNREACHABLE,
                                    "unreachable at %06" PRIx32,
                                    ptr2pc(m, ORIG_PC - 1));
        } else if (VALIDATING) {
                mark_unreachable(VCTX);
        }
        INSN_SUCCESS;
}

INSN_IMPL(nop) { INSN_SUCCESS; }

INSN_IMPL(block)
{
        int ret;
        struct mem_context *mctx = NULL;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        if (EXECUTING) {
                push_label(ORIG_PC, STACK, ECTX);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                mctx = validation_mctx(vctx);
                ret = get_functype_for_blocktype(mctx, m, blocktype,
                                                 &rt_parameter, &rt_result);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt_parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_BLOCK, 0, rt_parameter,
                                     rt_result, vctx);
                if (ret != 0) {
                        goto fail;
                }
                rt_parameter = NULL;
                rt_result = NULL;
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        if (mctx != NULL) {
                resulttype_free(mctx, rt_parameter);
                resulttype_free(mctx, rt_result);
        }
        INSN_FAIL;
}

INSN_IMPL(loop)
{
        struct mem_context *mctx = NULL;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;
        int ret;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        if (EXECUTING) {
                push_label(ORIG_PC, STACK, ECTX);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                mctx = validation_mctx(vctx);
                ret = get_functype_for_blocktype(mctx, m, blocktype,
                                                 &rt_parameter, &rt_result);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt_parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_LOOP, 0, rt_parameter,
                                     rt_result, vctx);
                if (ret != 0) {
                        goto fail;
                }
                rt_parameter = NULL;
                rt_result = NULL;
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        if (mctx != NULL) {
                resulttype_free(mctx, rt_parameter);
                resulttype_free(mctx, rt_result);
        }
        INSN_FAIL;
}

INSN_IMPL(if)
{
        struct mem_context *mctx = NULL;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;
        int ret;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        POP_VAL(TYPE_i32, c);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                push_label(ORIG_PC, STACK, ECTX);
                if (val_c.u.i32 == 0) {
                        schedule_goto_else(ectx);
                        /*
                         * Note: We don't bother to call SAVE_PC as we will
                         * jump anyway.
                         */
                        INSN_SUCCESS_RETURN;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                mctx = validation_mctx(vctx);
                ret = get_functype_for_blocktype(mctx, m, blocktype,
                                                 &rt_parameter, &rt_result);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt_parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_IF, 0, rt_parameter,
                                     rt_result, vctx);
                if (ret != 0) {
                        goto fail;
                }
                rt_parameter = NULL;
                rt_result = NULL;
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        if (mctx != NULL) {
                resulttype_free(mctx, rt_parameter);
                resulttype_free(mctx, rt_result);
        }
        INSN_FAIL;
}

INSN_IMPL(else)
{
        if (EXECUTING) {
                /* equivalent of "br 0" */
                struct exec_context *ectx = ECTX;
                schedule_br(ectx, 0);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct ctrlframe cframe;
                int ret;

                LOAD_PC;
                uint32_t pc = ptr2pc(vctx->module, p);
                ret = pop_ctrlframe(pc, true, &cframe, vctx);
                if (ret != 0) {
                        return ret;
                }
                ret = push_ctrlframe(pc - 1, FRAME_OP_ELSE, cframe.jumpslot,
                                     cframe.start_types, cframe.end_types,
                                     vctx);
                cframe.start_types = NULL;
                cframe.end_types = NULL;
                ctrlframe_clear(validation_mctx(vctx), &cframe);
                if (ret != 0) {
                        return ret;
                }
        }
        INSN_SUCCESS_RETURN;
}

INSN_IMPL(end)
{
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                assert(ectx->frames.lsize > 0);
                struct funcframe *frame = &VEC_LASTELEM(ectx->frames);
                xlog_trace_insn("end: nlabels %" PRIu32 " labelidx %" PRIu32,
                                ectx->labels.lsize, frame->labelidx);
                if (__predict_true(ectx->labels.lsize > frame->labelidx)) {
                        VEC_POP_DROP(ectx->labels);
                } else {
                        schedule_return(ectx);
                        INSN_SUCCESS_RETURN;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct ctrlframe cframe;
                int ret;

                LOAD_PC;
                uint32_t pc = ptr2pc(vctx->module, p);
                ret = pop_ctrlframe(pc, false, &cframe, vctx);
                if (ret != 0) {
                        return ret;
                }
                if (cframe.op == FRAME_OP_IF) {
                        /* no "else" is same as an empty "else" */
                        xlog_trace_insn("emulating an empty else");
                        ret = push_ctrlframe(pc, FRAME_OP_EMPTY_ELSE, 0,
                                             cframe.start_types,
                                             cframe.end_types, vctx);
                        if (ret == 0) {
                                cframe.start_types = NULL;
                                cframe.end_types = NULL;
                        }
                        ctrlframe_clear(validation_mctx(vctx), &cframe);
                        if (ret != 0) {
                                return ret;
                        }
                        ret = pop_ctrlframe(pc, false, &cframe, vctx);
                        if (ret != 0) {
                                return ret;
                        }
                }
                assert((cframe.op == FRAME_OP_INVOKE) ==
                       (vctx->cframes.lsize == 0));
                struct mem_context *mctx = validation_mctx(vctx);
                if (cframe.op == FRAME_OP_INVOKE) {
                        ctrlframe_clear(mctx, &cframe);
                } else {
                        ret = push_valtypes(cframe.end_types, vctx);
                        ctrlframe_clear(mctx, &cframe);
                        if (ret != 0) {
                                return ret;
                        }
                }
                INSN_SUCCESS_BLOCK_END;
        }
        INSN_SUCCESS;
}

INSN_IMPL(br)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(labelidx);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                schedule_br(ectx, labelidx);
                /*
                 * Note: We don't bother to call SAVE_PC as we will
                 * jump anyway.
                 */
                INSN_SUCCESS_RETURN;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct resulttype *rt;
                ret = target_label_types(vctx, labelidx, &rt);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt, vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(br_if)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(labelidx);
        POP_VAL(TYPE_i32, l);
        if (EXECUTING) {
                if (val_l.u.i32 != 0) {
                        struct exec_context *ectx = ECTX;
                        schedule_br(ectx, labelidx);
                        /*
                         * Note: We don't bother to call SAVE_PC as we will
                         * jump anyway.
                         */
                        INSN_SUCCESS_RETURN;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct resulttype *rt;
                ret = target_label_types(vctx, labelidx, &rt);
                if (ret != 0) {
                        goto fail;
                }
                const struct ctrlframe *cframe = &VEC_LASTELEM(vctx->cframes);
                if (!cframe->unreachable) {
                        /*
                         * this is just a small optimization.
                         * usually doesn't matter. but "rt" theoretically
                         * can have a ton of types.
                         */
                        ret = peek_valtypes(rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                } else {
                        ret = pop_valtypes(rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                        ret = push_valtypes(rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                }
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}

INSN_IMPL(br_table)
{
        uint32_t *table = NULL;
        uint32_t vec_count = 0;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        /*
         * GCC w/o optimization produces a "maybe-uninitialized"
         * false-positive here.
         */
        ret = 0;
#endif

        LOAD_PC;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                vec_count = read_leb_u32_nocheck(&p);
                POP_VAL(TYPE_i32, l);
                /*
                 * Note: as we will jump anyway, we don't bother to
                 * update the instruction pointer (p) precisely here.
                 */
                uint32_t l = val_l.u.i32;
                if (l >= vec_count) {
                        l = vec_count;
                }
                uint32_t idx;
                while (true) {
                        idx = read_leb_u32_nocheck(&p);
                        if (l == 0) {
                                break;
                        }
                        l--;
                }
                schedule_br(ectx, idx);
                /*
                 * Note: We don't bother to call SAVE_PC as we will
                 * jump anyway.
                 */
                INSN_SUCCESS_RETURN;
        }
        struct mem_context *mctx;
        struct mem_context mctx0;
        if (VALIDATING) {
                mctx = VCTX->mctx;
        } else {
                mctx = &mctx0;
                mem_context_init(mctx);
        }
        ret = read_vec_u32(mctx, &p, ep, &vec_count, &table);
        CHECK_RET(ret);
        READ_LEB_U32(defaultidx);
        POP_VAL(TYPE_i32, l);
        if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct resulttype *rt_default;
                ret = target_label_types(vctx, defaultidx, &rt_default);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t arity = rt_default->ntypes;
                uint32_t i;
                for (i = 0; i < vec_count; i++) {
                        uint32_t idx = table[i];
                        const struct resulttype *rt;
                        ret = target_label_types(vctx, idx, &rt);
                        if (ret != 0) {
                                goto fail;
                        }
                        if (rt->ntypes != arity) {
                                ret = EINVAL;
                                goto fail;
                        }
                        ret = peek_valtypes(rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                ret = pop_valtypes(rt_default, vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        mem_free(mctx, table, vec_count * sizeof(uint32_t));
        if (!VALIDATING) {
                mem_context_clear(mctx);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        if (table != NULL) {
                mem_free(mctx, table, vec_count * sizeof(uint32_t));
        }
        INSN_FAIL;
}

INSN_IMPL(return)
{
        int ret;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                schedule_return(ectx);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                ret = pop_valtypes(returntype(vctx), vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}

INSN_IMPL(call)
{
        const struct module *m;
        int ret;

        LOAD_PC;
        READ_LEB_U32(funcidx);
        m = MODULE;
        CHECK(funcidx < m->nimportedfuncs + m->nfuncs);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                schedule_call(ectx, VEC_ELEM(ectx->instance->funcs, funcidx));
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct functype *ft = module_functype(m, funcidx);
                ret = pop_valtypes(&ft->parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = push_valtypes(&ft->result, vctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}

INSN_IMPL(call_indirect)
{
        const struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(typeidx);
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        CHECK(typeidx < m->ntypes);
        POP_VAL(TYPE_i32, a);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                const struct funcinst *func;
                uint32_t i = val_a.u.i32;
#if defined(__GNUC__) && !defined(__clang__)
                func = NULL;
#endif
                ret = get_func_indirect(ectx, tableidx, typeidx, i, &func);
                if (__predict_false(ret != 0)) {
                        goto fail;
                }
                schedule_call(ectx, func);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct tabletype *tab = module_tabletype(m, tableidx);
                if (tab->et != TYPE_FUNCREF) {
                        ret = validation_failure(
                                vctx,
                                "call_indirect unexpected table type %" PRIu32,
                                tableidx);
                        goto fail;
                }
                const struct functype *ft = &m->types[typeidx];
                xlog_trace_insn("call_indirect (table %u type %u) %u %u",
                                tableidx, typeidx, ft->parameter.ntypes,
                                ft->result.ntypes);
                ret = pop_valtypes(&ft->parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = push_valtypes(&ft->result, vctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}
