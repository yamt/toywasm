/*
 * https://github.com/WebAssembly/exception-handling
 *
 * Implementation notes:
 *
 * - Because we don't have GC, we implement exnref as a copy-able type,
 *   rather than a reference to an object.
 *   cf. https://github.com/WebAssembly/exception-handling/issues/287
 *
 *   Because of this approach, we have a fixed size limit on
 *   exception parameters. (TOYWASM_EXCEPTION_MAX_CELLS)
 *   It also means that we can't accept exnref as a parameter for exceptions.
 *
 *   Note that this approach is incompatible with configurations with
 *   fixed-sized values because one value (exnref) needs to contain
 *   another. (eg. i32)  For now, we just require TOYWASM_USE_SMALL_CELLS.
 *
 * - We don't have embedder APIs to deal with exceptions.
 *   cf.
 * https://github.com/WebAssembly/exception-handling/blob/main/proposals/exception-handling/Exceptions.md#js-api
 */

INSN_IMPL(try_table)
{
        int ret;
        struct mem_context *mctx = NULL;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;
#if defined(__GNUC__) && !defined(__clang__)
        ret = 0; /* suppress maybe-uninitialized warning */
#endif

        LOAD_PC;
        READ_LEB_S33(blocktype);
        READ_LEB_U32(vec_count);
        if (VALIDATING) {
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
                                ret = push_valtype(TYPE_exnref, vctx);
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
                 * validating catch labels. ("catch 0" points to the
                 * surrounding block, not the try_table block itself.)
                 *
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
        } else {
                /*
                 * skip and ignore catch clauses.
                 * we will process them when an exception is
                 * actually thrown. (find_catch)
                 */
                uint32_t i;
                for (i = 0; i < vec_count; i++) {
                        uint32_t tagidx;
                        READ_U8(catch_op);
                        switch (catch_op) {
                        case CATCH_REF:
                        case CATCH:
                                READ_LEB_U32_TO(tagidx);
                                break;
                        case CATCH_ALL_REF:
                        case CATCH_ALL:
                                break;
                        default:
                                assert(false);
                        }
                        READ_LEB_U32(labelidx);
                }
                if (EXECUTING) {
                        /*
                         * now it's same as block.
                         */
                        push_label(ORIG_PC, STACK, ECTX);
                }
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

INSN_IMPL(throw)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(tagidx);
        if (EXECUTING || VALIDATING) {
                const struct module *m = MODULE;
                CHECK(tagidx < m->nimportedtags + m->ntags);
                const struct tagtype *tt = module_tagtype(m, tagidx);
                const struct functype *ft = module_tagtype_functype(m, tt);
                const struct resulttype *rt = &ft->parameter;
                if (EXECUTING) {
                        struct exec_context *ectx = ECTX;
                        /*
                         * pop the exception parameters, create an exception,
                         * and push exnref.
                         */
                        SAVE_STACK_PTR;
                        push_exception(ectx, tagidx, rt);
                        LOAD_STACK_PTR;
                        /*
                         * now it's same as throw_ref.
                         */
                        schedule_exception(ectx);
                } else {
                        struct validation_context *vctx = VCTX;
                        ret = pop_valtypes(rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                        /*
                         * ensure to allocate enough stack for push_exception
                         */
                        ret = push_valtype(TYPE_exnref, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                        mark_unreachable(vctx);
                }
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
                schedule_exception(ECTX);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                enum valtype t;
                ret = pop_valtype(TYPE_exnref, &t, vctx);
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
