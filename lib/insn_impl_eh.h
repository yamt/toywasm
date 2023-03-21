/*
 * https://github.com/WebAssembly/exception-handling
 */

INSN_IMPL(try)
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
                ret = push_ctrlframe(pc, FRAME_OP_TRY, 0, rt_parameter,
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
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        INSN_FAIL;
}

INSN_IMPL(catch)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(excidx);
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
                ret = pop_ctrlframe(pc, false, &cframe, vctx);
                if (ret != 0) {
                        return ret;
                }
                if (cframe.op != FRAME_OP_TRY) {
                        ctrlframe_clear(&cframe);
                        return validation_failure(vctx, "catch without try");
                }
                /* TODO use start_types from excidx */
                ret = push_ctrlframe(pc - 1, FRAME_OP_CATCH, cframe.jumpslot,
                                     cframe.start_types, cframe.end_types,
                                     vctx);
                cframe.start_types = NULL;
                cframe.end_types = NULL;
                ctrlframe_clear(&cframe);
                if (ret != 0) {
                        return ret;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}

INSN_IMPL(throw)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(excidx);
        if (EXECUTING) {
                return ENOTSUP;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct module *m = vctx->module;
                const struct functype *ft;
                const struct resulttype *rt;
                if (excidx >= m->nimportedtags + m->ntags) {
                        ret = validation_failure(
                                vctx, "out of range tag %" PRIu32, excidx);
                        goto fail;
                }
                ft = module_tagtype(m, excidx);
                rt = &ft->parameter;
                ret = pop_valtypes(rt, vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}

INSN_IMPL(delegate)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(labelidx);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                assert(ectx->frames.lsize > 0);
                struct funcframe *frame = &VEC_LASTELEM(ectx->frames);
                ret = ENOTSUP;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct ctrlframe cframe;
                int ret;
                /*
                 * Note: it's ok for a delegate label to refer any kind
                 * of block.
                 */
                if (labelidx >= vctx->ncframes) {
                        return validation_failure(
                                vctx,
                                "out of range delegate label index %" PRIu32,
                                labelidx);
                }

                /*
                 * the rest of validation logic is same as end.
                 */
                LOAD_PC;
                uint32_t pc = ptr2pc(vctx->module, p);
                ret = pop_ctrlframe(pc, false, &cframe, vctx);
                if (ret != 0) {
                        return ret;
                }
                if (cframe.op != FRAME_OP_TRY) {
                        ctrlframe_clear(&cframe);
                        return validation_failure(vctx,
                                                  "delegate without try");
                }
                ret = push_valtypes(cframe.end_types, vctx);
                ctrlframe_clear(&cframe);
                if (ret != 0) {
                        return ret;
                }
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}
