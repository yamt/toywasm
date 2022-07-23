#define MODULE                                                                \
        (EXECUTING ? ECTX->instance->module : VALIDATING ? VCTX->module : NULL)

#define TRAP(id, ...)                                                         \
        trap_with_id(ECTX, id, __VA_ARGS__);                                  \
        ret = EFAULT;                                                         \
        goto fail

#define CHECK_RET(ret)                                                        \
        do {                                                                  \
                if (VALIDATING) {                                             \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                } else {                                                      \
                        assert(ret == 0);                                     \
                }                                                             \
        } while (false)

#define CHECK(cond)                                                           \
        do {                                                                  \
                if (VALIDATING) {                                             \
                        if (!(cond)) {                                        \
                                ret = validation_failure(                     \
                                        VCTX, "CHECK failed %s", #cond);      \
                                goto fail;                                    \
                        }                                                     \
                } else if (EXECUTING) {                                       \
                        assert(cond);                                         \
                }                                                             \
        } while (false)

#define POP_VAL(t, var)                                                       \
        struct val val_##var;                                                 \
        enum valtype type_##var;                                              \
        do {                                                                  \
                if (EXECUTING) {                                              \
                        pop_val(&val_##var, ECTX);                            \
                } else if (VALIDATING) {                                      \
                        ret = pop_valtype(t, &type_##var, VCTX);              \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                }                                                             \
        } while (false)

#define PUSH_VAL(t, var)                                                      \
        do {                                                                  \
                if (EXECUTING) {                                              \
                        push_val(&val_##var, ECTX);                           \
                } else if (VALIDATING) {                                      \
                        ret = push_valtype(t, VCTX);                          \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                }                                                             \
        } while (false)

#define READ_IMM(TYPE, VAR, READ, READ_NOCHECK)                               \
        TYPE VAR;                                                             \
        do {                                                                  \
                if (VALIDATING) {                                             \
                        ret = READ;                                           \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                } else {                                                      \
                        VAR = READ_NOCHECK;                                   \
                }                                                             \
        } while (false)

#define READ_LEB_S33(VAR)                                                     \
        READ_IMM(int64_t, VAR, read_leb_s(&p, ep, 33, &VAR),                  \
                 read_leb_s33_nocheck(&p))

#define READ_LEB_U32(VAR)                                                     \
        READ_IMM(uint32_t, VAR, read_leb_u32(&p, ep, &VAR),                   \
                 read_leb_u32_nocheck(&p))

#define READ_LEB_I32(VAR)                                                     \
        READ_IMM(uint32_t, VAR, read_leb_i32(&p, ep, &VAR),                   \
                 read_leb_i32_nocheck(&p))

#define READ_LEB_I64(VAR)                                                     \
        READ_IMM(uint64_t, VAR, read_leb_i64(&p, ep, &VAR),                   \
                 read_leb_i64_nocheck(&p))
