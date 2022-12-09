#define ATOMIC_WAIT(NAME, BITS)                                               \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                uint32_t memidx = 0;                                          \
                struct memarg memarg;                                         \
                READ_MEMARG##BITS(&memarg);                                   \
                struct module *m = MODULE;                                    \
                CHECK(memidx < m->nimportedmems + m->nmems);                  \
                POP_VAL(TYPE_i64, timeout_ns);                                \
                POP_VAL(TYPE_i##BITS, expected);                              \
                POP_VAL(TYPE_i32, address);                                   \
                struct val val_result;                                        \
                if (EXECUTING) {                                              \
                        struct exec_context *ectx = ECTX;                     \
                        uint32_t address = val_address.u.i32;                 \
                        uint64_t expected = val_expected.u.i##BITS;           \
                        int64_t timeout_ns = val_timeout_ns.u.i64;            \
                        uint32_t result;                                      \
                        ret = memory_wait(ectx, memidx, address,              \
                                          memarg.offset, expected, &result,   \
                                          timeout_ns, BITS == 64);            \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        val_result.u.i32 = result;                            \
                }                                                             \
                PUSH_VAL(TYPE_i32, result);                                   \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                return ret;                                                   \
        }

#define ATOMIC_LOADOP2(NAME, MEM, STACK, CAST, I_OR_F)                        \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                struct module *m = MODULE;                                    \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                uint32_t memidx = 0;                                          \
                CHECK(memidx < m->nimportedmems + m->nmems);                  \
                POP_VAL(TYPE_i32, i);                                         \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        void *datap;                                          \
                        struct atomics_mutex *lock;                           \
                        ret = memory_atomic_getptr(ECTX, memidx, val_i.u.i32, \
                                                   memarg.offset, MEM / 8,    \
                                                   &datap, &lock);            \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        val_c.u.i##STACK = CAST le##MEM##_decode(datap);      \
                        memory_atomic_unlock(lock);                           \
                }                                                             \
                PUSH_VAL(TYPE_##I_OR_F##STACK, c);                            \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                return ret;                                                   \
        }

#define ATOMIC_STOREOP2(NAME, MEM, STACK, CAST, I_OR_F)                       \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                struct module *m = MODULE;                                    \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                uint32_t memidx = 0;                                          \
                CHECK(memidx < m->nimportedmems + m->nmems);                  \
                POP_VAL(TYPE_##I_OR_F##STACK, v);                             \
                POP_VAL(TYPE_i32, i);                                         \
                if (EXECUTING) {                                              \
                        void *datap;                                          \
                        struct atomics_mutex *lock;                           \
                        ret = memory_atomic_getptr(ECTX, memidx, val_i.u.i32, \
                                                   memarg.offset, MEM / 8,    \
                                                   &datap, &lock);            \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        le##MEM##_encode(datap, CAST val_v.u.i##STACK);       \
                        memory_atomic_unlock(lock);                           \
                }                                                             \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                return ret;                                                   \
        }

#define ATOMIC_RMW(NAME, MEM, STACK, OP)                                      \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                struct module *m = MODULE;                                    \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                uint32_t memidx = 0;                                          \
                CHECK(memidx < m->nimportedmems + m->nmems);                  \
                POP_VAL(TYPE_i##STACK, v);                                    \
                POP_VAL(TYPE_i32, i);                                         \
                struct val val_readv;                                         \
                if (EXECUTING) {                                              \
                        void *datap;                                          \
                        struct atomics_mutex *lock;                           \
                        ret = memory_atomic_getptr(ECTX, memidx, val_i.u.i32, \
                                                   memarg.offset, MEM / 8,    \
                                                   &datap, &lock);            \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        uint##STACK##_t tmp = le##MEM##_decode(datap);        \
                        val_readv.u.i##STACK = tmp;                           \
                        tmp = OP(STACK, tmp, val_v.u.i##STACK);               \
                        le##MEM##_encode(datap, tmp);                         \
                        memory_atomic_unlock(lock);                           \
                }                                                             \
                PUSH_VAL(TYPE_i##STACK, readv);                               \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                return ret;                                                   \
        }

#define ATOMIC_RMW_CMPXCHG(NAME, MEM, STACK)                                  \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                struct module *m = MODULE;                                    \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                uint32_t memidx = 0;                                          \
                CHECK(memidx < m->nimportedmems + m->nmems);                  \
                POP_VAL(TYPE_i##STACK, replacement);                          \
                POP_VAL(TYPE_i##STACK, expected);                             \
                POP_VAL(TYPE_i32, i);                                         \
                struct val val_readv;                                         \
                if (EXECUTING) {                                              \
                        void *datap;                                          \
                        struct atomics_mutex *lock;                           \
                        ret = memory_atomic_getptr(ECTX, memidx, val_i.u.i32, \
                                                   memarg.offset, MEM / 8,    \
                                                   &datap, &lock);            \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        uint##STACK##_t tmp = le##MEM##_decode(datap);        \
                        val_readv.u.i##STACK = tmp;                           \
                        if (tmp == val_expected.u.i##STACK) {                 \
                                le##MEM##_encode(datap,                       \
                                                 val_replacement.u.i##STACK); \
                        }                                                     \
                        memory_atomic_unlock(lock);                           \
                }                                                             \
                PUSH_VAL(TYPE_i##STACK, readv);                               \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                return ret;                                                   \
        }

INSN_IMPL(memory_atomic_notify)
{
        int ret;
        LOAD_PC;
        uint32_t memidx = 0;
        struct memarg memarg;
        READ_MEMARG32(&memarg);
        struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, count);
        POP_VAL(TYPE_i32, address);
        struct val val_nwoken;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t address = val_address.u.i32;
                uint32_t count = val_address.u.i32;
                uint32_t nwoken;
                ret = memory_notify(ectx, memidx, address, memarg.offset,
                                    count, &nwoken);
                if (ret != 0) {
                        goto fail;
                }
                val_nwoken.u.i32 = nwoken;
        }
        PUSH_VAL(TYPE_i32, nwoken);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

ATOMIC_WAIT(memory_atomic_wait32, 32)
ATOMIC_WAIT(memory_atomic_wait64, 64)

INSN_IMPL(atomic_fence)
{
        int ret;
        LOAD_PC;
        uint8_t zero;
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        if (EXECUTING) {
                /*
                 * is it better to use stdatomic
                 * atomic_thread_fence(memory_order_seq_cst) ?
                 */
                __sync_synchronize();
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

ATOMIC_LOADOP2(i32_atomic_load, 32, 32, , i)
ATOMIC_LOADOP2(i64_atomic_load, 64, 64, , i)
ATOMIC_LOADOP2(i32_atomic_load8_u, 8, 32, , i)
ATOMIC_LOADOP2(i32_atomic_load16_u, 16, 32, , i)
ATOMIC_LOADOP2(i64_atomic_load8_u, 8, 64, , i)
ATOMIC_LOADOP2(i64_atomic_load16_u, 16, 64, , i)
ATOMIC_LOADOP2(i64_atomic_load32_u, 32, 64, , i)

ATOMIC_STOREOP2(i32_atomic_store, 32, 32, , i)
ATOMIC_STOREOP2(i64_atomic_store, 64, 64, , i)
ATOMIC_STOREOP2(i32_atomic_store8_u, 8, 32, , i)
ATOMIC_STOREOP2(i32_atomic_store16_u, 16, 32, , i)
ATOMIC_STOREOP2(i64_atomic_store8_u, 8, 64, , i)
ATOMIC_STOREOP2(i64_atomic_store16_u, 16, 64, , i)
ATOMIC_STOREOP2(i64_atomic_store32_u, 32, 64, , i)

ATOMIC_RMW(i32_atomic_rmw8_add_u, 8, 32, ADD)
ATOMIC_RMW(i32_atomic_rmw16_add_u, 16, 32, ADD)
ATOMIC_RMW(i32_atomic_rmw_add, 32, 32, ADD)
ATOMIC_RMW(i64_atomic_rmw8_add_u, 8, 64, ADD)
ATOMIC_RMW(i64_atomic_rmw16_add_u, 16, 64, ADD)
ATOMIC_RMW(i64_atomic_rmw32_add_u, 32, 64, ADD)
ATOMIC_RMW(i64_atomic_rmw_add, 64, 64, ADD)

ATOMIC_RMW(i32_atomic_rmw8_sub_u, 8, 32, SUB)
ATOMIC_RMW(i32_atomic_rmw16_sub_u, 16, 32, SUB)
ATOMIC_RMW(i32_atomic_rmw_sub, 32, 32, SUB)
ATOMIC_RMW(i64_atomic_rmw8_sub_u, 8, 64, SUB)
ATOMIC_RMW(i64_atomic_rmw16_sub_u, 16, 64, SUB)
ATOMIC_RMW(i64_atomic_rmw32_sub_u, 32, 64, SUB)
ATOMIC_RMW(i64_atomic_rmw_sub, 64, 64, SUB)

ATOMIC_RMW(i32_atomic_rmw8_and_u, 8, 32, AND)
ATOMIC_RMW(i32_atomic_rmw16_and_u, 16, 32, AND)
ATOMIC_RMW(i32_atomic_rmw_and, 32, 32, AND)
ATOMIC_RMW(i64_atomic_rmw8_and_u, 8, 64, AND)
ATOMIC_RMW(i64_atomic_rmw16_and_u, 16, 64, AND)
ATOMIC_RMW(i64_atomic_rmw32_and_u, 32, 64, AND)
ATOMIC_RMW(i64_atomic_rmw_and, 64, 64, AND)

ATOMIC_RMW(i32_atomic_rmw8_or_u, 8, 32, OR)
ATOMIC_RMW(i32_atomic_rmw16_or_u, 16, 32, OR)
ATOMIC_RMW(i32_atomic_rmw_or, 32, 32, OR)
ATOMIC_RMW(i64_atomic_rmw8_or_u, 8, 64, OR)
ATOMIC_RMW(i64_atomic_rmw16_or_u, 16, 64, OR)
ATOMIC_RMW(i64_atomic_rmw32_or_u, 32, 64, OR)
ATOMIC_RMW(i64_atomic_rmw_or, 64, 64, OR)

ATOMIC_RMW(i32_atomic_rmw8_xor_u, 8, 32, XOR)
ATOMIC_RMW(i32_atomic_rmw16_xor_u, 16, 32, XOR)
ATOMIC_RMW(i32_atomic_rmw_xor, 32, 32, XOR)
ATOMIC_RMW(i64_atomic_rmw8_xor_u, 8, 64, XOR)
ATOMIC_RMW(i64_atomic_rmw16_xor_u, 16, 64, XOR)
ATOMIC_RMW(i64_atomic_rmw32_xor_u, 32, 64, XOR)
ATOMIC_RMW(i64_atomic_rmw_xor, 64, 64, XOR)

ATOMIC_RMW(i32_atomic_rmw8_xchg_u, 8, 32, XCHG)
ATOMIC_RMW(i32_atomic_rmw16_xchg_u, 16, 32, XCHG)
ATOMIC_RMW(i32_atomic_rmw_xchg, 32, 32, XCHG)
ATOMIC_RMW(i64_atomic_rmw8_xchg_u, 8, 64, XCHG)
ATOMIC_RMW(i64_atomic_rmw16_xchg_u, 16, 64, XCHG)
ATOMIC_RMW(i64_atomic_rmw32_xchg_u, 32, 64, XCHG)
ATOMIC_RMW(i64_atomic_rmw_xchg, 64, 64, XCHG)

ATOMIC_RMW_CMPXCHG(i32_atomic_rmw8_cmpxchg_u, 8, 32)
ATOMIC_RMW_CMPXCHG(i32_atomic_rmw16_cmpxchg_u, 16, 32)
ATOMIC_RMW_CMPXCHG(i32_atomic_rmw_cmpxchg, 32, 32)
ATOMIC_RMW_CMPXCHG(i64_atomic_rmw8_cmpxchg_u, 8, 64)
ATOMIC_RMW_CMPXCHG(i64_atomic_rmw16_cmpxchg_u, 16, 64)
ATOMIC_RMW_CMPXCHG(i64_atomic_rmw32_cmpxchg_u, 32, 64)
ATOMIC_RMW_CMPXCHG(i64_atomic_rmw_cmpxchg, 64, 64)
