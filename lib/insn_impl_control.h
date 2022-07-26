
INSN_IMPL(unreachable)
{
        if (EXECUTING) {
                return trap_with_id(ECTX, TRAP_UNREACHABLE, "unreachable");
        } else if (VALIDATING) {
                mark_unreachable(VCTX);
        }
        return 0;
}

INSN_IMPL(nop) { INSN_SUCCESS; }

INSN_IMPL(block)
{
        int ret;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        if (EXECUTING) {
                push_label(ORIG_PC, STACK, ECTX);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                ret = get_functype_for_blocktype(m, blocktype, &rt_parameter,
                                                 &rt_result);
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
        INSN_SUCCESS_RETURN;
fail:
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        return ret;
}

INSN_IMPL(loop)
{
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
                ret = get_functype_for_blocktype(m, blocktype, &rt_parameter,
                                                 &rt_result);
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
        INSN_SUCCESS_RETURN;
fail:
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        return ret;
}

INSN_IMPL(if)
{
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
                        ectx->event_u.branch.index = 0;
                        ectx->event_u.branch.goto_else = true;
                        ectx->event = EXEC_EVENT_BRANCH;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                ret = get_functype_for_blocktype(m, blocktype, &rt_parameter,
                                                 &rt_result);
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
        INSN_SUCCESS_RETURN;
fail:
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        return ret;
}

INSN_IMPL(else)
{
        if (EXECUTING) {
                /* equivalent of "br 0" */
                struct exec_context *ectx = ECTX;
                ectx->event_u.branch.index = 0;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
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
                ctrlframe_clear(&cframe);
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
                xlog_trace("end: nlabels %" PRIu32 " labelidx %" PRIu32,
                           ectx->labels.lsize, frame->labelidx);
                if (ectx->labels.lsize > frame->labelidx) {
                        VEC_POP_DROP(ectx->labels);
                } else {
                        frame_exit(ectx);
                        SAVE_STACK_PTR;
                        rewind_stack(ectx, frame->height, frame->nresults);
                        LOAD_STACK_PTR;
                        RELOAD_PC;
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
                        xlog_trace("emulating an empty else");
                        ret = push_ctrlframe(pc, FRAME_OP_EMPTY_ELSE, 0,
                                             cframe.start_types,
                                             cframe.end_types, vctx);
                        if (ret == 0) {
                                cframe.start_types = NULL;
                                cframe.end_types = NULL;
                        }
                        ctrlframe_clear(&cframe);
                        if (ret != 0) {
                                return ret;
                        }
                        ret = pop_ctrlframe(pc, false, &cframe, vctx);
                        if (ret != 0) {
                                return ret;
                        }
                }
                assert((cframe.op == FRAME_OP_INVOKE) ==
                       (vctx->ncframes == 0));
                if (cframe.op == FRAME_OP_INVOKE) {
                        ctrlframe_clear(&cframe);
                } else {
                        ret = push_valtypes(cframe.end_types, vctx);
                        ctrlframe_clear(&cframe);
                        if (ret != 0) {
                                return ret;
                        }
                }
        }
        INSN_SUCCESS_RETURN;
}

INSN_IMPL(br)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(labelidx);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                ectx->event_u.branch.index = labelidx;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
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
        INSN_SUCCESS_RETURN;
fail:
        return ret;
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
                        ectx->event_u.branch.index = labelidx;
                        ectx->event_u.branch.goto_else = false;
                        ectx->event = EXEC_EVENT_BRANCH;
                }
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
                ret = push_valtypes(rt, vctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        return ret;
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
                do {
                        idx = read_leb_u32_nocheck(&p);
                } while (l-- > 0);
                ectx->event_u.branch.index = idx;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
                SAVE_PC;
                INSN_SUCCESS_RETURN;
        }
        ret = read_vec_u32(&p, ep, &vec_count, &table);
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
        free(table);
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        free(table);
        return ret;
}

INSN_IMPL(return )
{
        int ret;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                assert(ectx->frames.lsize > 0);
                const struct funcframe *frame = &VEC_LASTELEM(ectx->frames);
                uint32_t nlabels = ectx->labels.lsize - frame->labelidx;
                xlog_trace("return as tr %" PRIu32, nlabels);
                ectx->event_u.branch.index = nlabels;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
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
        return ret;
}

INSN_IMPL(call)
{
        struct module *m;
        int ret;

        LOAD_PC;
        READ_LEB_U32(funcidx);
        m = MODULE;
        CHECK(funcidx < m->nimportedfuncs + m->nfuncs);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                ectx->event_u.call.func =
                        VEC_ELEM(ectx->instance->funcs, funcidx);
                ectx->event = EXEC_EVENT_CALL;
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
        return ret;
}

INSN_IMPL(call_indirect)
{
        struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(typeidx);
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        CHECK(typeidx < m->ntypes);
        const struct tabletype *tab;
        const struct functype *ft;
#if defined(__GNUC__) && !defined(__clang__)
        /* suppress warnings */
        tab = NULL;
        ft = NULL;
#endif
        if (EXECUTING || VALIDATING) {
                tab = module_tabletype(m, tableidx);
                ft = &m->types[typeidx];
        }
        CHECK(tab->et == TYPE_FUNCREF);
        POP_VAL(TYPE_i32, a);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                const struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                assert(t->type->et == TYPE_FUNCREF);
                uint32_t i = val_a.u.i32;
                if (i >= t->size) {
                        ret = trap_with_id(
                                ectx,
                                TRAP_CALL_INDIRECT_OUT_OF_BOUNDS_TABLE_ACCESS,
                                "call_indirect (table idx out of range) "
                                "%" PRIu32 " %" PRIu32 " %" PRIu32,
                                typeidx, tableidx, i);
                        goto fail;
                }
                struct val val;
                uint32_t csz = valtype_cellsize(t->type->et);
                val_from_cells(&val, &t->cells[i * csz], csz);
                const struct funcinst *func = val.u.funcref.func;
                if (func == NULL) {
                        ret = trap_with_id(
                                ectx, TRAP_CALL_INDIRECT_NULL_FUNCREF,
                                "call_indirect (null funcref) %" PRIu32
                                " %" PRIu32 " %" PRIu32,
                                typeidx, tableidx, i);
                        goto fail;
                }
                const struct functype *actual_ft = funcinst_functype(func);
                if (compare_functype(ft, actual_ft)) {
                        ret = trap_with_id(
                                ectx, TRAP_CALL_INDIRECT_FUNCTYPE_MISMATCH,
                                "call_indirect (functype mismatch) %" PRIu32
                                " %" PRIu32 " %" PRIu32,
                                typeidx, tableidx, i);
                        goto fail;
                }
                ectx->event_u.call.func = func;
                ectx->event = EXEC_EVENT_CALL;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                xlog_trace("call_indirect (table %u type %u) %u %u", tableidx,
                           typeidx, ft->parameter.ntypes, ft->result.ntypes);
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
        return ret;
}
