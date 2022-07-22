#define MODULE                                                                \
        (EXECUTING ? ECTX->instance->module : VALIDATING ? VCTX->module : NULL)

#define TRAP(id, ...)                                                         \
        trap_with_id(ECTX, id, __VA_ARGS__);                                  \
        ret = EFAULT;                                                         \
        goto fail

#define CHECK(cond)                                                           \
        do {                                                                  \
                if (VALIDATING) {                                             \
                        if (!(cond)) {                                        \
                                ret = validation_failure(                     \
                                        VCTX, "CHECK failed %s", #cond);      \
                                goto fail;                                    \
                        }                                                     \
                } else {                                                      \
                        assert(cond);                                         \
                }                                                             \
        } while (false)

#define POP_VAL(t, var)                                                       \
        struct val val_##var;                                                 \
        enum valtype type_##var;                                              \
        do {                                                                  \
                if (EXECUTING) {                                              \
                        ret = pop_val(&val_##var, ECTX);                      \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
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
                        ret = push_val(&val_##var, ECTX);                     \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                } else if (VALIDATING) {                                      \
                        ret = push_valtype(t, VCTX);                          \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                }                                                             \
        } while (false)
