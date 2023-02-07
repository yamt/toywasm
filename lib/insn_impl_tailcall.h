/*
 * https://github.com/WebAssembly/tail-call
 */

INSN_IMPL(return_call)
{
        const struct module *m;
        int ret;

        LOAD_PC;
        READ_LEB_U32(funcidx);
        m = MODULE;
        CHECK(funcidx < m->nimportedfuncs + m->nfuncs);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                ectx->event_u.call.func =
                        VEC_ELEM(ectx->instance->funcs, funcidx);
                ectx->event = EXEC_EVENT_RETURN_CALL;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct functype *ft = module_functype(m, funcidx);
                ret = pop_valtypes(&ft->parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                if (compare_resulttype(returntype(vctx), &ft->result)) {
                        ret = validation_failure(
                                vctx, "return type mismatch for return_call");
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}

INSN_IMPL(return_call_indirect)
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
                ret = get_func_indirect(ectx, tableidx, typeidx, i, &func);
                if (__predict_false(ret != 0)) {
                        goto fail;
                }
                ectx->event_u.call.func = func;
                ectx->event = EXEC_EVENT_RETURN_CALL;
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
                if (compare_resulttype(returntype(vctx), &ft->result)) {
                        ret = validation_failure(vctx,
                                                 "return type mismatch for "
                                                 "return_call_indirect");
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        INSN_FAIL;
}
