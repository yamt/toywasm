
#define CMPXCHG(p, op, n) atomic_compare_exchange_strong(p, op, n)
#define FENCE() atomic_thread_fence(memory_order_seq_cst)

#define ATOMIC_WAIT(NAME, BITS)                                               \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                PREPARE_FOR_POSSIBLE_RESTART;                                 \
                LOAD_PC;                                                      \
                struct memarg memarg;                                         \
                READ_MEMARG##BITS(&memarg);                                   \
                const struct module *m = MODULE;                              \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                POP_VAL(TYPE_i64, timeout_ns);                                \
                POP_VAL(TYPE_i##BITS, expected);                              \
                POP_VAL(TYPE_i32, address);                                   \
                struct val val_result;                                        \
                if (EXECUTING) {                                              \
                        struct exec_context *ectx = ECTX;                     \
                        uint32_t address = val_address.u.i32;                 \
                        uint64_t expected = val_expected.u.i##BITS;           \
                        int64_t timeout_ns = (int64_t)val_timeout_ns.u.i64;   \
                        uint32_t result;                                      \
                        ret = memory_wait(ectx, memarg.memidx, address,       \
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
                INSN_FAIL_RESTARTABLE(NAME);                                  \
        }

#define ATOMIC_LOADOP2(NAME, MEM, STACK, CAST, I_OR_F)                        \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                const struct module *m = MODULE;                              \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                POP_VAL(TYPE_i32, i);                                         \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        void *vp;                                             \
                        ret = memory_atomic_getptr(                           \
                                ECTX, memarg.memidx, val_i.u.i32,             \
                                memarg.offset, MEM / 8, &vp, NULL);           \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        _Atomic uint##MEM##_t *ap = vp;                       \
                        uint##STACK##_t v = le##MEM##_to_host(*ap);           \
                        val_c.u.i##STACK = CAST v;                            \
                }                                                             \
                PUSH_VAL(TYPE_##I_OR_F##STACK, c);                            \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define ATOMIC_STOREOP2(NAME, MEM, STACK, CAST, I_OR_F)                       \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                const struct module *m = MODULE;                              \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                POP_VAL(TYPE_##I_OR_F##STACK, v);                             \
                POP_VAL(TYPE_i32, i);                                         \
                if (EXECUTING) {                                              \
                        void *vp;                                             \
                        ret = memory_atomic_getptr(                           \
                                ECTX, memarg.memidx, val_i.u.i32,             \
                                memarg.offset, MEM / 8, &vp, NULL);           \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        uint##STACK##_t v = CAST val_v.u.i##STACK;            \
                        _Atomic uint##MEM##_t *ap = vp;                       \
                        *ap = host_to_le##MEM(v);                             \
                }                                                             \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define ATOMIC_RMW(NAME, MEM, STACK, OP)                                      \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                const struct module *m = MODULE;                              \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                POP_VAL(TYPE_i##STACK, v);                                    \
                POP_VAL(TYPE_i32, i);                                         \
                struct val val_readv;                                         \
                if (EXECUTING) {                                              \
                        void *vp;                                             \
                        ret = memory_atomic_getptr(                           \
                                ECTX, memarg.memidx, val_i.u.i32,             \
                                memarg.offset, MEM / 8, &vp, NULL);           \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        _Atomic uint##MEM##_t *ap = vp;                       \
                        uint##MEM##_t old_le;                                 \
                        uint##STACK##_t old_h;                                \
                        uint##MEM##_t new_le;                                 \
                        do {                                                  \
                                old_le = *ap;                                 \
                                old_h = (uint##STACK##_t)le##MEM##_to_host(   \
                                        old_le);                              \
                                uint##STACK##_t new_h =                       \
                                        OP(STACK, old_h, val_v.u.i##STACK);   \
                                new_le = host_to_le##MEM(                     \
                                        (uint##MEM##_t)new_h);                \
                        } while (!CMPXCHG(ap, &old_le, new_le));              \
                        val_readv.u.i##STACK = old_h;                         \
                }                                                             \
                PUSH_VAL(TYPE_i##STACK, readv);                               \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

/*
 * Consider a mutex implementation which uses an atomic opcode
 * (eg. x86 `cmpxchg`) to acquire a mutex and release it with a non-atomic
 * opcode. (eg. x86 `mov`) On x86, it just works.
 * However, on wasm, such a mutex implementation is considered broken.
 * It might or might not work, depending on the runtimes. Especially for
 * lock-based implmentations of atomic opcodes, where non-atomic store
 * (eg. `i32.store`) is not expected to honor the lock.
 * cf. https://github.com/WebAssembly/threads/issues/197
 *
 * Note: This particalar implementation uses C11 atomics to implement
 * atomic opcodes and ordinary assignments (eg. le32_encode) for
 * non-atomic opcodes. The latter might not be even a single assignment
 * at C level.
 */

#define ATOMIC_RMW_CMPXCHG(NAME, MEM, STACK)                                  \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                const struct module *m = MODULE;                              \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG##MEM(&memarg);                                    \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                POP_VAL(TYPE_i##STACK, replacement);                          \
                POP_VAL(TYPE_i##STACK, expected);                             \
                POP_VAL(TYPE_i32, i);                                         \
                struct val val_readv;                                         \
                if (EXECUTING) {                                              \
                        void *vp;                                             \
                        ret = memory_atomic_getptr(                           \
                                ECTX, memarg.memidx, val_i.u.i32,             \
                                memarg.offset, MEM / 8, &vp, NULL);           \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        _Atomic uint##MEM##_t *ap = vp;                       \
                        uint##MEM##_t truncated =                             \
                                (uint##MEM##_t)val_expected.u.i##STACK;       \
                        uint##MEM##_t read_le;                                \
                        if (truncated == val_expected.u.i##STACK) {           \
                                uint##MEM##_t expected_le =                   \
                                        host_to_le##MEM(truncated);           \
                                uint##MEM##_t replacement_le =                \
                                        host_to_le##MEM(                      \
                                                (uint##MEM##_t)               \
                                                        val_replacement.u     \
                                                                .i##STACK);   \
                                CMPXCHG(ap, &expected_le, replacement_le);    \
                                read_le = expected_le;                        \
                        } else {                                              \
                                read_le = *ap;                                \
                        }                                                     \
                        val_readv.u.i##STACK = le##MEM##_to_host(read_le);    \
                }                                                             \
                PUSH_VAL(TYPE_i##STACK, readv);                               \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

INSN_IMPL(memory_atomic_notify)
{
        int ret;
        LOAD_PC;
        struct memarg memarg;
        READ_MEMARG32(&memarg);
        const struct module *m = MODULE;
        CHECK(memarg.memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, count);
        POP_VAL(TYPE_i32, address);
        struct val val_nwoken;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t address = val_address.u.i32;
                uint32_t count = val_count.u.i32;
                uint32_t nwoken;
                ret = memory_notify(ectx, memarg.memidx, address,
                                    memarg.offset, count, &nwoken);
                if (ret != 0) {
                        goto fail;
                }
                val_nwoken.u.i32 = nwoken;
        }
        PUSH_VAL(TYPE_i32, nwoken);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
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
                FENCE();
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
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
