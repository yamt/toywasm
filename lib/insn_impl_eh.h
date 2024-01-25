/*
 * https://github.com/WebAssembly/exception-handling
 */

INSN_IMPL(try_table)
{
        int ret;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        READ_LEB_U32(vec_count);
        if (EXECUTING) {
                ret = ENOTSUP;
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
                uint32_t i;
                for (i = 0; i < vec_count; i++) {
                        uint32_t tagidx;
                        const struct tagtype *tt;
                        const struct functype *ft;
                        const struct resulttype *tag_rt = NULL;
                        bool with_exnref = false;
                        READ_U8(catch_op);
                        switch (catch_op) {
                        case CATCH_REF:
                                with_exnref = true;
                                /* fallthrough */
                        case CATCH:
                                READ_LEB_U32_TO(tagidx);
                                CHECK(tagidx < m->nimportedtags + m->ntags);
                                tt = module_tagtype(m, tagidx);
                                ft = module_tagtype_functype(m, tt);
                                tag_rt = &ft->parameter;
                                break;
                        case CATCH_ALL_REF:
                                with_exnref = true;
                                /* fallthrough */
                        case CATCH_ALL:
                                break;
                        default:
                                return validation_failure(
                                        vctx, "unknown catch op %u", catch_op);
                        }
                        READ_LEB_U32(labelidx);
                        const struct resulttype *label_rt;
                        ret = target_label_types(vctx, labelidx, &label_rt);
                        if (ret != 0) {
                                goto fail;
                        }
                        uint32_t saved_height = vctx->valtypes.lsize;
                        if (tag_rt != NULL) {
                                ret = push_valtypes(tag_rt, vctx);
                                if (ret != 0) {
                                        goto fail;
                                }
                        }
                        if (with_exnref) {
                                ret = push_valtype(TYPE_EXNREF, vctx);
                                if (ret != 0) {
                                        goto fail;
                                }
                        }
                        if (vctx->valtypes.lsize - saved_height !=
                            label_rt->ntypes) {
                                ret = EINVAL;
                                goto fail;
                        }
                        ret = pop_valtypes(label_rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                        assert(vctx->valtypes.lsize == saved_height);
                }
                /*
                 * Note: we should push our control frame _after_
                 * validating catch labels.
                 * cf.
                 * https://github.com/WebAssembly/exception-handling/issues/286
                 */
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_TRY_TABLE, 0, rt_parameter,
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

INSN_IMPL(throw)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(tagidx);
        if (EXECUTING) {
                return ENOTSUP;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct module *m = vctx->module;
                if (tagidx >= m->nimportedtags + m->ntags) {
                        ret = validation_failure(
                                vctx, "out of range tag %" PRIu32, tagidx);
                        goto fail;
                }
                const struct tagtype *tt = module_tagtype(m, tagidx);
                const struct functype *ft = module_tagtype_functype(m, tt);
                const struct resulttype *rt = &ft->parameter;
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

INSN_IMPL(throw_ref)
{
        int ret;

        LOAD_PC;
        if (EXECUTING) {
                return ENOTSUP;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                enum valtype t;
                ret = pop_valtype(TYPE_EXNREF, &t, vctx);
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
