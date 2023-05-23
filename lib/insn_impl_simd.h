/*
 * https://github.com/WebAssembly/simd
 */

#define READ_LANEIDX(VAR, N)                                                  \
        READ_U8(VAR);                                                         \
        CHECK(VAR < N)

#define READ_LANEIDX_TO(VAR, N)                                               \
        READ_U8_TO(VAR);                                                      \
        CHECK(VAR < N)

/*
 * Note: i128x1 is just for implementation convenience
 */

#define LANEPTRi128(val) (&(val)->u.v128)
#define LANEPTRi64(val) (val)->u.v128.i64
#define LANEPTRi32(val) (val)->u.v128.i32
#define LANEPTRi16(val) (val)->u.v128.i16
#define LANEPTRi8(val) (val)->u.v128.i8
#define LANEPTRf64(val) (val)->u.v128.f64
#define LANEPTRf32(val) (val)->u.v128.f32

#define READ_LANEIDX1(VAR) const uint8_t VAR = 0
#define READ_LANEIDX2(VAR) READ_LANEIDX(VAR, 2)
#define READ_LANEIDX4(VAR) READ_LANEIDX(VAR, 4)
#define READ_LANEIDX8(VAR) READ_LANEIDX(VAR, 8)
#define READ_LANEIDX16(VAR) READ_LANEIDX(VAR, 16)
#define READ_LANEIDX32(VAR) READ_LANEIDX(VAR, 32)

#define READ_LANEIDX32_TO(VAR) READ_LANEIDX_TO(VAR, 32)

/*
 * MEM - num of bits in memory
 * STACK_TYPE - type on stack (v128)
 * CP - CP(DST, SRC) to load and convert
 * LS - lane bits
 * NL - number of lanes
 */
#define SIMD_LOADOP(NAME, MEM, STACK_TYPE, CP)                                \
        SIMD_LOADOP_LANE(NAME, MEM, 128, 1, STACK_TYPE, CP)
#define SIMD_LOADOP_LANE(NAME, MEM, LS, NL, STACK_TYPE, CP)                   \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                const struct module *m = MODULE;                              \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG(&memarg);                                         \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                CHECK(1 <= (MEM / 8) >>                                       \
                      memarg.align); /* 2 ** align <= N / 8 */                \
                READ_LANEIDX##NL(lane);                                       \
                struct val val_c;                                             \
                if (NL != 1) {                                                \
                        POP_VAL(TYPE_##STACK_TYPE, x);                        \
                        if (EXECUTING) {                                      \
                                val_c = val_x;                                \
                        }                                                     \
                }                                                             \
                POP_VAL(TYPE_i32, i);                                         \
                if (EXECUTING) {                                              \
                        void *datap;                                          \
                        ret = memory_getptr(ECTX, memarg.memidx, val_i.u.i32, \
                                            memarg.offset, MEM / 8, &datap);  \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        CP(&LANEPTRi##LS(&val_c)[lane], datap);               \
                }                                                             \
                PUSH_VAL(TYPE_##STACK_TYPE, c);                               \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_STOREOP(NAME, MEM, STACK_TYPE, CP)                               \
        SIMD_STOREOP_LANE(NAME, MEM, 128, 1, STACK_TYPE, CP)
#define SIMD_STOREOP_LANE(NAME, MEM, LS, NL, STACK_TYPE, CP)                  \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                const struct module *m = MODULE;                              \
                struct memarg memarg;                                         \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_MEMARG(&memarg);                                         \
                CHECK(memarg.memidx < m->nimportedmems + m->nmems);           \
                CHECK(1 <= (MEM / 8) >>                                       \
                      memarg.align); /* 2 ** align <= N / 8 */                \
                READ_LANEIDX##NL(lane);                                       \
                POP_VAL(TYPE_##STACK_TYPE, v);                                \
                POP_VAL(TYPE_i32, i);                                         \
                if (EXECUTING) {                                              \
                        void *datap;                                          \
                        ret = memory_getptr(ECTX, memarg.memidx, val_i.u.i32, \
                                            memarg.offset, MEM / 8, &datap);  \
                        if (ret != 0) {                                       \
                                goto fail;                                    \
                        }                                                     \
                        CP(datap, &LANEPTRi##LS(&val_v)[lane]);               \
                }                                                             \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_EXTRACTOP_LANE(NAME, SIGN, I_OR_F, STACK, LS, NL, CP)            \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_LANEIDX##NL(lane);                                       \
                POP_VAL(TYPE_v128, v);                                        \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        uint##LS##_t le;                                      \
                        CP(&le, &LANEPTRi##LS(&val_v)[lane]);                 \
                        val_c.u.i##STACK = EXTEND_##SIGN(                     \
                                STACK, LS, le##LS##_decode(&le));             \
                }                                                             \
                PUSH_VAL(TYPE_##I_OR_F##STACK, c);                            \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_REPLACEOP_LANE(NAME, I_OR_F, STACK, LS, NL, CP)                  \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                READ_LANEIDX##NL(lane);                                       \
                POP_VAL(TYPE_##I_OR_F##STACK, x);                             \
                POP_VAL(TYPE_v128, v);                                        \
                if (EXECUTING) {                                              \
                        uint##LS##_t le;                                      \
                        le##LS##_encode(&le, (uint##LS##_t)val_x.u.i##STACK); \
                        CP(&LANEPTRi##LS(&val_v)[lane], &le);                 \
                }                                                             \
                PUSH_VAL(TYPE_v128, v);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_SPLATOP(NAME, I_OR_F, STACK, LS)                                 \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_##I_OR_F##STACK, x);                             \
                struct val val_v;                                             \
                if (EXECUTING) {                                              \
                        uint##LS##_t le;                                      \
                        le##LS##_encode(&le, (uint##LS##_t)val_x.u.i##STACK); \
                        FOREACH_LANES(LS, &val_v.u.v128, &le, SPLAT1);        \
                }                                                             \
                PUSH_VAL(TYPE_v128, v);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_CONSTOP(NAME)                                                    \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                if (VALIDATING) {                                             \
                        CHECK((const uint8_t *)ep - p >= 16);                 \
                }                                                             \
                const uint8_t *immp = p;                                      \
                p += 16;                                                      \
                struct val val_v;                                             \
                if (EXECUTING) {                                              \
                        COPYBITS128(&val_v.u.v128, immp);                     \
                }                                                             \
                PUSH_VAL(TYPE_v128, v);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_BOOLOP(NAME, INIT, OP)                                           \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_result;                                        \
                if (EXECUTING) {                                              \
                        uint32_t r = INIT;                                    \
                        OP(r, &val_a);                                        \
                        val_result.u.i32 = r;                                 \
                }                                                             \
                PUSH_VAL(TYPE_i32, result);                                   \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_OP1(NAME, OP)                                                    \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        OP(&val_c, &val_a);                                   \
                }                                                             \
                PUSH_VAL(TYPE_v128, c);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_OP2(NAME, OP)                                                    \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_v128, b);                                        \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        OP(&val_c, &val_a, &val_b);                           \
                }                                                             \
                PUSH_VAL(TYPE_v128, c);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_OP3(NAME, OP)                                                    \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_v128, c);                                        \
                POP_VAL(TYPE_v128, b);                                        \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_r;                                             \
                if (EXECUTING) {                                              \
                        OP(&val_r, &val_a, &val_b, &val_c);                   \
                }                                                             \
                PUSH_VAL(TYPE_v128, r);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_FOREACH_LANES_OP2(NAME, I_OR_F, LS, OP)                          \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_v128, b);                                        \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        FOREACH_LANES3_2(I_OR_F, LS, &val_c, &val_a, &val_b,  \
                                         OP);                                 \
                }                                                             \
                PUSH_VAL(TYPE_v128, c);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_FOREACH_LANES_OP1(NAME, I_OR_F, LS, OP)                          \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        FOREACH_LANES2_2(I_OR_F, LS, &val_c, &val_a, OP);     \
                }                                                             \
                PUSH_VAL(TYPE_v128, c);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define SIMD_SHIFTOP(NAME, OP)                                                \
        INSN_IMPL(NAME)                                                       \
        {                                                                     \
                int ret;                                                      \
                LOAD_PC;                                                      \
                POP_VAL(TYPE_i32, b);                                         \
                POP_VAL(TYPE_v128, a);                                        \
                struct val val_c;                                             \
                if (EXECUTING) {                                              \
                        OP(&val_c, &val_a, &val_b);                           \
                }                                                             \
                PUSH_VAL(TYPE_v128, c);                                       \
                SAVE_PC;                                                      \
                INSN_SUCCESS;                                                 \
fail:                                                                         \
                INSN_FAIL;                                                    \
        }

#define COPYBITS(a, b, n) memcpy(a, b, n / 8)
#define COPYBITS8(a, b) COPYBITS(a, b, 8)
#define COPYBITS16(a, b) COPYBITS(a, b, 16)
#define COPYBITS32(a, b) COPYBITS(a, b, 32)
#define COPYBITS64(a, b) COPYBITS(a, b, 64)
#define COPYBITS128(a, b) COPYBITS(a, b, 128)

#define EXTEND_s(BDST, BSRC, S) (int##BDST##_t)(int##BSRC##_t)(S)
#define EXTEND_u(BDST, BSRC, S) (uint##BDST##_t)(uint##BSRC##_t)(S)
#define EXTEND_noop(B, B2, S) (S)

#define _EXTEND1(S_OR_U, BDST, BSRC, a, b, I)                                 \
        le##BDST##_encode(                                                    \
                &(a)->i##BDST[I],                                             \
                EXTEND_##S_OR_U(                                              \
                        BDST, BSRC,                                           \
                        le##BSRC##_decode(&((const uint##BSRC##_t *)b)[I])))
#define EXTEND1(S_OR_U, BDST, BSRC, a, b, I)                                  \
        _EXTEND1(S_OR_U, BDST, BSRC, a, b, I)

#define _EXTADD1(S_OR_U, BDST, BSRC, a, b, I)                                 \
        le##BDST##_encode(                                                    \
                &(a)[I],                                                      \
                ADD(BDST,                                                     \
                    EXTEND_##S_OR_U(BDST, BSRC,                               \
                                    le##BSRC##_decode(&(b)[I * 2])),          \
                    EXTEND_##S_OR_U(BDST, BSRC,                               \
                                    le##BSRC##_decode(&(b)[I * 2 + 1]))))

#define EXTADD1(S_OR_U, BDST, BSRC, a, b, I)                                  \
        _EXTADD1(S_OR_U, BDST, BSRC, a, b, I)

/* extmul_low(a, b) = mul(ext_low(a), ext_low(b)) */
#define _EXTMUL1(S_OR_U, BDST, BSRC, a, b, c, I)                              \
        le##BDST##_encode(                                                    \
                &(a)[I],                                                      \
                MUL(BDST,                                                     \
                    EXTEND_##S_OR_U(BDST, BSRC,                               \
                                    le##BSRC##_decode(&(                      \
                                            (const uint##BSRC##_t *)b)[I])),  \
                    EXTEND_##S_OR_U(BDST, BSRC,                               \
                                    le##BSRC##_decode(&(                      \
                                            (const uint##BSRC##_t *)c)[I]))))

#define EXTMUL1(S_OR_U, BDST, BSRC, a, b, c, I)                               \
        _EXTMUL1(S_OR_U, BDST, BSRC, a, b, c, I)

#define DBL8 16
#define DBL16 32
#define DBL32 64

#define HALF16 8
#define HALF32 16
#define HALF64 32

#define EXTEND1_s(LS, a, b, I) EXTEND1(s, LS, HALF##LS, a, b, I)
#define EXTEND1_u(LS, a, b, I) EXTEND1(u, LS, HALF##LS, a, b, I)

#define EXTADD1_s(LS, a, b, I) EXTADD1(s, LS, HALF##LS, a, b, I)
#define EXTADD1_u(LS, a, b, I) EXTADD1(u, LS, HALF##LS, a, b, I)

#define EXTEND_8x8_s(a, b) FOREACH_LANES(16, a, b, EXTEND1_s)
#define EXTEND_8x8_u(a, b) FOREACH_LANES(16, a, b, EXTEND1_u)
#define EXTEND_16x4_s(a, b) FOREACH_LANES(32, a, b, EXTEND1_s)
#define EXTEND_16x4_u(a, b) FOREACH_LANES(32, a, b, EXTEND1_u)
#define EXTEND_32x2_s(a, b) FOREACH_LANES(64, a, b, EXTEND1_s)
#define EXTEND_32x2_u(a, b) FOREACH_LANES(64, a, b, EXTEND1_u)

#define EXTADD_16x8_s(a, b) FOREACH_LANES(16, a, b, EXTADD1_s)
#define EXTADD_16x8_u(a, b) FOREACH_LANES(16, a, b, EXTADD1_u)
#define EXTADD_32x4_s(a, b) FOREACH_LANES(32, a, b, EXTADD1_s)
#define EXTADD_32x4_u(a, b) FOREACH_LANES(32, a, b, EXTADD1_u)

#define EXTMUL1_s(LS, a, b, c, I) EXTMUL1(s, LS, HALF##LS, a, b, c, I)
#define EXTMUL1_u(LS, a, b, c, I) EXTMUL1(u, LS, HALF##LS, a, b, c, I)

#define EXTMUL_16_s(a, b, c) FOREACH_LANES3(16, a, b, c, EXTMUL1_s)
#define EXTMUL_16_u(a, b, c) FOREACH_LANES3(16, a, b, c, EXTMUL1_u)
#define EXTMUL_32_s(a, b, c) FOREACH_LANES3(32, a, b, c, EXTMUL1_s)
#define EXTMUL_32_u(a, b, c) FOREACH_LANES3(32, a, b, c, EXTMUL1_u)
#define EXTMUL_64_s(a, b, c) FOREACH_LANES3(64, a, b, c, EXTMUL1_s)
#define EXTMUL_64_u(a, b, c) FOREACH_LANES3(64, a, b, c, EXTMUL1_u)

#define SPLAT1(LS, D, S, I) COPYBITS##LS(&(D)->i##LS[I], S)

#define SPLAT_8(D, S) FOREACH_LANES(8, D, S, SPLAT1)
#define SPLAT_16(D, S) FOREACH_LANES(16, D, S, SPLAT1)
#define SPLAT_32(D, S) FOREACH_LANES(32, D, S, SPLAT1)
#define SPLAT_64(D, S) FOREACH_LANES(64, D, S, SPLAT1)

#define FOREACH_LANES(LS, D, S, OP)                                           \
        do {                                                                  \
                unsigned int _i;                                              \
                for (_i = 0; _i < 128 / LS; _i++) {                           \
                        OP(LS, D, S, _i);                                     \
                }                                                             \
        } while (0)

#define FOREACH_LANES3(LS, D, S, x, OP)                                       \
        do {                                                                  \
                unsigned int _i;                                              \
                for (_i = 0; _i < 128 / LS; _i++) {                           \
                        OP(LS, D, S, x, _i);                                  \
                }                                                             \
        } while (0)

#define FOREACH_LANES3_2(I_OR_F, LS, a, b, c, OP)                             \
        do {                                                                  \
                unsigned int _i;                                              \
                for (_i = 0; _i < 128 / LS; _i++) {                           \
                        OP(I_OR_F, LS, a, b, c, _i);                          \
                }                                                             \
        } while (0)

#define FOREACH_LANES2_2(I_OR_F, LS, a, b, OP)                                \
        do {                                                                  \
                unsigned int _i;                                              \
                for (_i = 0; _i < 128 / LS; _i++) {                           \
                        OP(I_OR_F, LS, a, b, _i);                             \
                }                                                             \
        } while (0)

SIMD_LOADOP(v128_load, 128, v128, COPYBITS128)
SIMD_LOADOP(v128_load8x8_s, 64, v128, EXTEND_8x8_s)
SIMD_LOADOP(v128_load8x8_u, 64, v128, EXTEND_8x8_u)
SIMD_LOADOP(v128_load16x4_s, 64, v128, EXTEND_16x4_s)
SIMD_LOADOP(v128_load16x4_u, 64, v128, EXTEND_16x4_u)
SIMD_LOADOP(v128_load32x2_s, 64, v128, EXTEND_32x2_s)
SIMD_LOADOP(v128_load32x2_u, 64, v128, EXTEND_32x2_u)
SIMD_LOADOP(v128_load8_splat, 8, v128, SPLAT_8)
SIMD_LOADOP(v128_load16_splat, 16, v128, SPLAT_16)
SIMD_LOADOP(v128_load32_splat, 32, v128, SPLAT_32)
SIMD_LOADOP(v128_load64_splat, 64, v128, SPLAT_64)

#define ZERO32(D, S)                                                          \
        memcpy(&(D)->i32[0], (S), 4);                                         \
        (D)->i32[1] = 0;                                                      \
        (D)->i32[2] = 0;                                                      \
        (D)->i32[3] = 0

#define ZERO64(D, S)                                                          \
        memcpy(&(D)->i64[0], (S), 8);                                         \
        (D)->i64[1] = 0

SIMD_LOADOP(v128_load32_zero, 32, v128, ZERO32)
SIMD_LOADOP(v128_load64_zero, 64, v128, ZERO64)

SIMD_STOREOP(v128_store, 128, v128, COPYBITS128)

SIMD_CONSTOP(v128_const)

SIMD_LOADOP_LANE(v128_load8_lane, 8, 8, 16, v128, COPYBITS8)
SIMD_LOADOP_LANE(v128_load16_lane, 16, 16, 8, v128, COPYBITS16)
SIMD_LOADOP_LANE(v128_load32_lane, 32, 32, 4, v128, COPYBITS32)
SIMD_LOADOP_LANE(v128_load64_lane, 64, 64, 2, v128, COPYBITS64)

SIMD_STOREOP_LANE(v128_store8_lane, 8, 8, 16, v128, COPYBITS8)
SIMD_STOREOP_LANE(v128_store16_lane, 16, 16, 8, v128, COPYBITS16)
SIMD_STOREOP_LANE(v128_store32_lane, 32, 32, 4, v128, COPYBITS32)
SIMD_STOREOP_LANE(v128_store64_lane, 64, 64, 2, v128, COPYBITS64)

SIMD_EXTRACTOP_LANE(i8x16_extract_lane_s, s, i, 32, 8, 16, COPYBITS8)
SIMD_EXTRACTOP_LANE(i8x16_extract_lane_u, u, i, 32, 8, 16, COPYBITS8)
SIMD_EXTRACTOP_LANE(i16x8_extract_lane_s, s, i, 32, 16, 8, COPYBITS16)
SIMD_EXTRACTOP_LANE(i16x8_extract_lane_u, u, i, 32, 16, 8, COPYBITS16)
SIMD_EXTRACTOP_LANE(i32x4_extract_lane, noop, i, 32, 32, 4, COPYBITS32)
SIMD_EXTRACTOP_LANE(i64x2_extract_lane, noop, i, 64, 64, 2, COPYBITS64)
SIMD_EXTRACTOP_LANE(f32x4_extract_lane, noop, f, 32, 32, 4, COPYBITS32)
SIMD_EXTRACTOP_LANE(f64x2_extract_lane, noop, f, 64, 64, 2, COPYBITS64)

SIMD_REPLACEOP_LANE(i8x16_replace_lane, i, 32, 8, 16, COPYBITS8)
SIMD_REPLACEOP_LANE(i16x8_replace_lane, i, 32, 16, 8, COPYBITS16)
SIMD_REPLACEOP_LANE(i32x4_replace_lane, i, 32, 32, 4, COPYBITS32)
SIMD_REPLACEOP_LANE(i64x2_replace_lane, i, 64, 64, 2, COPYBITS64)
SIMD_REPLACEOP_LANE(f32x4_replace_lane, f, 32, 32, 4, COPYBITS32)
SIMD_REPLACEOP_LANE(f64x2_replace_lane, f, 64, 64, 2, COPYBITS64)

SIMD_SPLATOP(i8x16_splat, i, 32, 8)
SIMD_SPLATOP(i16x8_splat, i, 32, 16)
SIMD_SPLATOP(i32x4_splat, i, 32, 32)
SIMD_SPLATOP(i64x2_splat, i, 64, 64)
SIMD_SPLATOP(f32x4_splat, f, 32, 32)
SIMD_SPLATOP(f64x2_splat, f, 64, 64)

#define SHL1(LS, a, b, c, I)                                                  \
        LANEPTRi##LS(a)[I] =                                                  \
                (uint##LS##_t)(LANEPTRi##LS(b)[I] << ((c)->u.i32 % LS))
#define SHR_s1(LS, a, b, c, I)                                                \
        LANEPTRi##LS(a)[I] = (uint##LS##_t)(                                  \
                (int##LS##_t)LANEPTRi##LS(b)[I] >> ((c)->u.i32 % LS))
#define SHR_u1(LS, a, b, c, I)                                                \
        LANEPTRi##LS(a)[I] = (LANEPTRi##LS(b)[I] >> ((c)->u.i32 % LS))

#define SHL_8(a, b, c) FOREACH_LANES3(8, a, b, c, SHL1)
#define SHL_16(a, b, c) FOREACH_LANES3(16, a, b, c, SHL1)
#define SHL_32(a, b, c) FOREACH_LANES3(32, a, b, c, SHL1)
#define SHL_64(a, b, c) FOREACH_LANES3(64, a, b, c, SHL1)

#define SHR_s_8(a, b, c) FOREACH_LANES3(8, a, b, c, SHR_s1)
#define SHR_s_16(a, b, c) FOREACH_LANES3(16, a, b, c, SHR_s1)
#define SHR_s_32(a, b, c) FOREACH_LANES3(32, a, b, c, SHR_s1)
#define SHR_s_64(a, b, c) FOREACH_LANES3(64, a, b, c, SHR_s1)

#define SHR_u_8(a, b, c) FOREACH_LANES3(8, a, b, c, SHR_u1)
#define SHR_u_16(a, b, c) FOREACH_LANES3(16, a, b, c, SHR_u1)
#define SHR_u_32(a, b, c) FOREACH_LANES3(32, a, b, c, SHR_u1)
#define SHR_u_64(a, b, c) FOREACH_LANES3(64, a, b, c, SHR_u1)

SIMD_SHIFTOP(i8x16_shl, SHL_8)
SIMD_SHIFTOP(i8x16_shr_s, SHR_s_8)
SIMD_SHIFTOP(i8x16_shr_u, SHR_u_8)

SIMD_SHIFTOP(i16x8_shl, SHL_16)
SIMD_SHIFTOP(i16x8_shr_s, SHR_s_16)
SIMD_SHIFTOP(i16x8_shr_u, SHR_u_16)

SIMD_SHIFTOP(i32x4_shl, SHL_32)
SIMD_SHIFTOP(i32x4_shr_s, SHR_s_32)
SIMD_SHIFTOP(i32x4_shr_u, SHR_u_32)

SIMD_SHIFTOP(i64x2_shl, SHL_64)
SIMD_SHIFTOP(i64x2_shr_s, SHR_s_64)
SIMD_SHIFTOP(i64x2_shr_u, SHR_u_64)

#define V128_OP1(a, b, OP)                                                    \
        do {                                                                  \
                (a)->u.v128.i64[1] = OP(64, (b)->u.v128.i64[1]);              \
                (a)->u.v128.i64[0] = OP(64, (b)->u.v128.i64[0]);              \
        } while (0)

#define V128_OP2(a, b, c, OP)                                                 \
        do {                                                                  \
                (a)->u.v128.i64[1] =                                          \
                        OP(64, (b)->u.v128.i64[1], (c)->u.v128.i64[1]);       \
                (a)->u.v128.i64[0] =                                          \
                        OP(64, (b)->u.v128.i64[0], (c)->u.v128.i64[0]);       \
        } while (0)

#define V128_OP3(a, b, c, d, OP)                                              \
        do {                                                                  \
                (a)->u.v128.i64[1] =                                          \
                        OP(64, (b)->u.v128.i64[1], (c)->u.v128.i64[1],        \
                           (d)->u.v128.i64[1]);                               \
                (a)->u.v128.i64[0] =                                          \
                        OP(64, (b)->u.v128.i64[0], (c)->u.v128.i64[0],        \
                           (d)->u.v128.i64[0]);                               \
        } while (0)

#define V128_NOT(a, b) V128_OP1(a, b, NOT)
#define V128_AND(a, b, c) V128_OP2(a, b, c, AND)
#define V128_AND(a, b, c) V128_OP2(a, b, c, AND)
#define V128_OR(a, b, c) V128_OP2(a, b, c, OR)
#define V128_XOR(a, b, c) V128_OP2(a, b, c, XOR)
#define V128_ANDNOT(a, b, c) V128_OP2(a, b, c, ANDNOT)
#define V128_BITSELECT(a, b, c, d) V128_OP3(a, b, c, d, BITSELECT)

SIMD_OP1(v128_not, V128_NOT)
SIMD_OP2(v128_and, V128_AND)
SIMD_OP2(v128_or, V128_OR)
SIMD_OP2(v128_xor, V128_XOR)
SIMD_OP2(v128_andnot, V128_ANDNOT)
SIMD_OP3(v128_bitselect, V128_BITSELECT)

#define V128_ANY_TRUE(r, v)                                                   \
        r = ((v)->u.v128.i64[0] != 0 || (v)->u.v128.i64[1] != 0)

SIMD_BOOLOP(v128_any_true, 0, V128_ANY_TRUE)

#define ALL_TRUE(LS, a, b, I) a &= (LANEPTRi##LS(b)[I] != 0)

#define ALL_TRUE_8x16(r, v) FOREACH_LANES(8, r, v, ALL_TRUE)
#define ALL_TRUE_16x8(r, v) FOREACH_LANES(16, r, v, ALL_TRUE)
#define ALL_TRUE_32x4(r, v) FOREACH_LANES(32, r, v, ALL_TRUE)
#define ALL_TRUE_64x2(r, v) FOREACH_LANES(64, r, v, ALL_TRUE)

SIMD_BOOLOP(i8x16_all_true, 1, ALL_TRUE_8x16)
SIMD_BOOLOP(i16x8_all_true, 1, ALL_TRUE_16x8)
SIMD_BOOLOP(i32x4_all_true, 1, ALL_TRUE_32x4)
SIMD_BOOLOP(i64x2_all_true, 1, ALL_TRUE_64x2)

#define BITMASK(LS, a, b, I)                                                  \
        a |= (uint32_t)((int##LS##_t)LANEPTRi##LS(b)[I] < 0) << I

#define BITMASK_8x16(r, v) FOREACH_LANES(8, r, v, BITMASK)
#define BITMASK_16x8(r, v) FOREACH_LANES(16, r, v, BITMASK)
#define BITMASK_32x4(r, v) FOREACH_LANES(32, r, v, BITMASK)
#define BITMASK_64x2(r, v) FOREACH_LANES(64, r, v, BITMASK)

SIMD_BOOLOP(i8x16_bitmask, 0, BITMASK_8x16)
SIMD_BOOLOP(i16x8_bitmask, 0, BITMASK_16x8)
SIMD_BOOLOP(i32x4_bitmask, 0, BITMASK_32x4)
SIMD_BOOLOP(i64x2_bitmask, 0, BITMASK_64x2)

#define LANE_OP2(I_OR_F, LS, a, b, I, OP)                                     \
        LANEPTR##I_OR_F##LS(a)[I] = OP(LS, LANEPTR##I_OR_F##LS(b)[I])

#define LANE_OP3(I_OR_F, LS, a, b, c, I, OP)                                  \
        LANEPTR##I_OR_F##LS(a)[I] =                                           \
                OP(LS, LANEPTR##I_OR_F##LS(b)[I], LANEPTR##I_OR_F##LS(c)[I])

#define ADD1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, ADD)
#define FADD1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FADD)

#define ADD_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, ADD1)
#define ADD_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, ADD1)
#define ADD_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, ADD1)
#define ADD_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, ADD1)
#define FADD_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FADD1)
#define FADD_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FADD1)

SIMD_OP2(i8x16_add, ADD_8x16)
SIMD_OP2(i16x8_add, ADD_16x8)
SIMD_OP2(i32x4_add, ADD_32x4)
SIMD_OP2(i64x2_add, ADD_64x2)
SIMD_OP2(f32x4_add, FADD_32x4)
SIMD_OP2(f64x2_add, FADD_64x2)

#define SUB1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, SUB)
#define FSUB1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FSUB)

#define SUB_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, SUB1)
#define SUB_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, SUB1)
#define SUB_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, SUB1)
#define SUB_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, SUB1)
#define FSUB_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FSUB1)
#define FSUB_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FSUB1)

SIMD_OP2(i8x16_sub, SUB_8x16)
SIMD_OP2(i16x8_sub, SUB_16x8)
SIMD_OP2(i32x4_sub, SUB_32x4)
SIMD_OP2(i64x2_sub, SUB_64x2)
SIMD_OP2(f32x4_sub, FSUB_32x4)
SIMD_OP2(f64x2_sub, FSUB_64x2)

#define SAT_s(LS, a)                                                          \
        (uint##LS##_t)((a >= INT##LS##_MAX)   ? INT##LS##_MAX                 \
                       : (a <= INT##LS##_MIN) ? INT##LS##_MIN                 \
                                              : a)

#define SAT_u(LS, a)                                                          \
        (uint##LS##_t)((a >= UINT##LS##_MAX) ? UINT##LS##_MAX                 \
                       : (a <= 0)            ? 0                              \
                                             : a)

#define ADD_SAT_s(N, a, b) SAT_s(N, (int##N##_t)a + (int##N##_t)b)
#define ADD_SAT_u(N, a, b) SAT_u(N, (uint##N##_t)a + (uint##N##_t)b)
#define SUB_SAT_s(N, a, b) SAT_s(N, (int##N##_t)a - (int##N##_t)b)
#define SUB_SAT_u(N, a, b) SAT_u(N, (uint##N##_t)a - (uint##N##_t)b)

#define ADD_SAT_s1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, ADD_SAT_s)
#define ADD_SAT_u1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, ADD_SAT_u)
#define SUB_SAT_s1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, SUB_SAT_s)
#define SUB_SAT_u1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, SUB_SAT_u)

#define ADD_SAT_8_s(a, b, c) FOREACH_LANES3(8, a, b, c, ADD_SAT_s1)
#define ADD_SAT_8_u(a, b, c) FOREACH_LANES3(8, a, b, c, ADD_SAT_u1)
#define ADD_SAT_16_s(a, b, c) FOREACH_LANES3(16, a, b, c, ADD_SAT_s1)
#define ADD_SAT_16_u(a, b, c) FOREACH_LANES3(16, a, b, c, ADD_SAT_u1)
#define SUB_SAT_8_s(a, b, c) FOREACH_LANES3(8, a, b, c, SUB_SAT_s1)
#define SUB_SAT_8_u(a, b, c) FOREACH_LANES3(8, a, b, c, SUB_SAT_u1)
#define SUB_SAT_16_s(a, b, c) FOREACH_LANES3(16, a, b, c, SUB_SAT_s1)
#define SUB_SAT_16_u(a, b, c) FOREACH_LANES3(16, a, b, c, SUB_SAT_u1)

SIMD_OP2(i8x16_add_sat_s, ADD_SAT_8_s)
SIMD_OP2(i16x8_add_sat_s, ADD_SAT_16_s)
SIMD_OP2(i8x16_sub_sat_s, SUB_SAT_8_s)
SIMD_OP2(i16x8_sub_sat_s, SUB_SAT_16_s)
SIMD_OP2(i8x16_add_sat_u, ADD_SAT_8_u)
SIMD_OP2(i16x8_add_sat_u, ADD_SAT_16_u)
SIMD_OP2(i8x16_sub_sat_u, SUB_SAT_8_u)
SIMD_OP2(i16x8_sub_sat_u, SUB_SAT_16_u)

#define MUL1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, MUL)
#define FMUL1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FMUL)

#define MUL_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, MUL1)
#define MUL_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, MUL1)
#define MUL_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, MUL1)
#define MUL_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, MUL1)
#define FMUL_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FMUL1)
#define FMUL_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FMUL1)

SIMD_OP2(i8x16_mul, MUL_8x16)
SIMD_OP2(i16x8_mul, MUL_16x8)
SIMD_OP2(i32x4_mul, MUL_32x4)
SIMD_OP2(i64x2_mul, MUL_64x2)
SIMD_OP2(f32x4_mul, FMUL_32x4)
SIMD_OP2(f64x2_mul, FMUL_64x2)

#define FDIV1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FDIV)

#define FDIV_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FDIV1)
#define FDIV_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FDIV1)

SIMD_OP2(f32x4_div, FDIV_32x4)
SIMD_OP2(f64x2_div, FDIV_64x2)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MAX_s(N, a, b) (uint##N##_t) MAX((int##N##_t)a, (int##N##_t)b)
#define MAX_u(N, a, b) MAX((uint##N##_t)a, (uint##N##_t)b)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MIN_s(N, a, b) (uint##N##_t) MIN((int##N##_t)a, (int##N##_t)b)
#define MIN_u(N, a, b) MIN((uint##N##_t)a, (uint##N##_t)b)

#define MAX_s1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, MAX_s)
#define MAX_u1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, MAX_u)
#define MIN_s1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, MIN_s)
#define MIN_u1(LS, a, b, c, I) LANE_OP3(i, LS, a, b, c, I, MIN_u)

#define MAX_s_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, MAX_s1)
#define MAX_s_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, MAX_s1)
#define MAX_s_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, MAX_s1)
#define MAX_u_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, MAX_u1)
#define MAX_u_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, MAX_u1)
#define MAX_u_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, MAX_u1)
#define MIN_s_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, MIN_s1)
#define MIN_s_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, MIN_s1)
#define MIN_s_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, MIN_s1)
#define MIN_u_8x16(a, b, c) FOREACH_LANES3(8, a, b, c, MIN_u1)
#define MIN_u_16x8(a, b, c) FOREACH_LANES3(16, a, b, c, MIN_u1)
#define MIN_u_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, MIN_u1)

SIMD_OP2(i8x16_max_s, MAX_s_8x16)
SIMD_OP2(i16x8_max_s, MAX_s_16x8)
SIMD_OP2(i32x4_max_s, MAX_s_32x4)
SIMD_OP2(i8x16_max_u, MAX_u_8x16)
SIMD_OP2(i16x8_max_u, MAX_u_16x8)
SIMD_OP2(i32x4_max_u, MAX_u_32x4)
SIMD_OP2(i8x16_min_s, MIN_s_8x16)
SIMD_OP2(i16x8_min_s, MIN_s_16x8)
SIMD_OP2(i32x4_min_s, MIN_s_32x4)
SIMD_OP2(i8x16_min_u, MIN_u_8x16)
SIMD_OP2(i16x8_min_u, MIN_u_16x8)
SIMD_OP2(i32x4_min_u, MIN_u_32x4)

#define FMAX(N, a, b) wasm_fmax((a), (b))
#define FMIN(N, a, b) wasm_fmin((a), (b))
#define FMAXF(N, a, b) wasm_fmaxf((a), (b))
#define FMINF(N, a, b) wasm_fminf((a), (b))
#define FPMAX(N, a, b) ((a) < (b) ? (b) : (a))
#define FPMIN(N, a, b) ((b) < (a) ? (b) : (a))

#define FMAX1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FMAX)
#define FMIN1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FMIN)
#define FMAXF1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FMAXF)
#define FMINF1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FMINF)
#define FPMAX1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FPMAX)
#define FPMIN1(LS, a, b, c, I) LANE_OP3(f, LS, a, b, c, I, FPMIN)

#define FMAX_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FMAXF1)
#define FMAX_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FMAX1)
#define FMIN_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FMINF1)
#define FMIN_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FMIN1)
#define FPMAX_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FPMAX1)
#define FPMAX_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FPMAX1)
#define FPMIN_32x4(a, b, c) FOREACH_LANES3(32, a, b, c, FPMIN1)
#define FPMIN_64x2(a, b, c) FOREACH_LANES3(64, a, b, c, FPMIN1)

SIMD_OP2(f32x4_max, FMAX_32x4)
SIMD_OP2(f64x2_max, FMAX_64x2)
SIMD_OP2(f32x4_min, FMIN_32x4)
SIMD_OP2(f64x2_min, FMIN_64x2)
SIMD_OP2(f32x4_pmax, FPMAX_32x4)
SIMD_OP2(f64x2_pmax, FPMAX_64x2)
SIMD_OP2(f32x4_pmin, FPMIN_32x4)
SIMD_OP2(f64x2_pmin, FPMIN_64x2)

#define FABS32(a) fabsf(a)
#define FNEG32(a) (-(a))
#define FSQRT32(a) sqrtf(a)
#define FABS64(a) fabs(a)
#define FNEG64(a) (-(a))
#define FSQRT64(a) sqrt(a)

#define FABS(N, a) FABS##N(a)
#define FNEG(N, a) FNEG##N(a)
#define FSQRT(N, a) FSQRT##N(a)

#define FABS1(LS, a, b, I) LANE_OP2(f, LS, a, b, I, FABS)
#define FNEG1(LS, a, b, I) LANE_OP2(f, LS, a, b, I, FNEG)
#define FSQRT1(LS, a, b, I) LANE_OP2(f, LS, a, b, I, FSQRT)

#define FABS_32(a, b) FOREACH_LANES(32, a, b, FABS1)
#define FNEG_32(a, b) FOREACH_LANES(32, a, b, FNEG1)
#define FSQRT_32(a, b) FOREACH_LANES(32, a, b, FSQRT1)
#define FABS_64(a, b) FOREACH_LANES(64, a, b, FABS1)
#define FNEG_64(a, b) FOREACH_LANES(64, a, b, FNEG1)
#define FSQRT_64(a, b) FOREACH_LANES(64, a, b, FSQRT1)

SIMD_OP1(f32x4_abs, FABS_32)
SIMD_OP1(f32x4_neg, FNEG_32)
SIMD_OP1(f32x4_sqrt, FSQRT_32)
SIMD_OP1(f64x2_abs, FABS_64)
SIMD_OP1(f64x2_neg, FNEG_64)
SIMD_OP1(f64x2_sqrt, FSQRT_64)

#define CONVERT_s1(LS, a, b, I)                                               \
        lef##LS##_encode(&LANEPTRf##LS(a)[I],                                 \
                         (int##LS##_t)le##LS##_decode(&LANEPTRi##LS(b)[I]))
#define CONVERT_u1(LS, a, b, I)                                               \
        lef##LS##_encode(&LANEPTRf##LS(a)[I],                                 \
                         le##LS##_decode(&LANEPTRi##LS(b)[I]))
#define CONVERT_LOW_s1(LS, a, b, I)                                           \
        lef##LS##_encode(&LANEPTRf##LS(a)[I],                                 \
                         (int32_t)le32_decode(&LANEPTRi32(b)[I]))
#define CONVERT_LOW_u1(LS, a, b, I)                                           \
        lef##LS##_encode(&LANEPTRf##LS(a)[I], le32_decode(&LANEPTRi32(b)[I]))

#define CONVERT_32_s(a, b) FOREACH_LANES(32, a, b, CONVERT_s1)
#define CONVERT_32_u(a, b) FOREACH_LANES(32, a, b, CONVERT_u1)
#define CONVERT_LOW_64_s(a, b) FOREACH_LANES(64, a, b, CONVERT_LOW_s1)
#define CONVERT_LOW_64_u(a, b) FOREACH_LANES(64, a, b, CONVERT_LOW_u1)

SIMD_OP1(f32x4_convert_i32x4_s, CONVERT_32_s)
SIMD_OP1(f32x4_convert_i32x4_u, CONVERT_32_u)
SIMD_OP1(f64x2_convert_low_i32x4_s, CONVERT_LOW_64_s)
SIMD_OP1(f64x2_convert_low_i32x4_u, CONVERT_LOW_64_u)

/*
 * Note: for narrowing ops, the input lanes are always interpreted signed.
 */

#define NARROW1(s, LS, LSSRC, a, b, c, I)                                     \
        do {                                                                  \
                int##LSSRC##_t _src;                                          \
                if (I < 128 / LSSRC) {                                        \
                        _src = (int##LSSRC##_t)LANEPTRi##LSSRC(b)[I];         \
                } else {                                                      \
                        _src = (int##LSSRC##_t)LANEPTRi##LSSRC(               \
                                c)[I - 128 / LSSRC];                          \
                }                                                             \
                LANEPTRi##LS(a)[I] = (uint##LS##_t)SAT_##s(LS, _src);         \
        } while (0)

#define _NARROW_s1(LS, FROM, a, b, c, I) NARROW1(s, LS, FROM, a, b, c, I)
#define NARROW_s1(LS, a, b, c, I) _NARROW_s1(LS, DBL##LS, a, b, c, I)

#define _NARROW_u1(LS, FROM, a, b, c, I) NARROW1(u, LS, FROM, a, b, c, I)
#define NARROW_u1(LS, a, b, c, I) _NARROW_u1(LS, DBL##LS, a, b, c, I)

#define NARROW_8_s(a, b, c) FOREACH_LANES3(8, a, b, c, NARROW_s1)
#define NARROW_8_u(a, b, c) FOREACH_LANES3(8, a, b, c, NARROW_u1)
#define NARROW_16_s(a, b, c) FOREACH_LANES3(16, a, b, c, NARROW_s1)
#define NARROW_16_u(a, b, c) FOREACH_LANES3(16, a, b, c, NARROW_u1)

SIMD_OP2(i8x16_narrow_i16x8_s, NARROW_8_s)
SIMD_OP2(i8x16_narrow_i16x8_u, NARROW_8_u)
SIMD_OP2(i16x8_narrow_i32x4_s, NARROW_16_s)
SIMD_OP2(i16x8_narrow_i32x4_u, NARROW_16_u)

#define DEMOTE_32(a, b)                                                       \
        LANEPTRf32(a)[0] = LANEPTRf64(b)[0];                                  \
        LANEPTRf32(a)[1] = LANEPTRf64(b)[1];                                  \
        LANEPTRf32(a)[2] = 0;                                                 \
        LANEPTRf32(a)[3] = 0
#define PROMOTE_LOW_64(a, b)                                                  \
        LANEPTRf64(a)[0] = LANEPTRf32(b)[0];                                  \
        LANEPTRf64(a)[1] = LANEPTRf32(b)[1]

SIMD_OP1(f32x4_demote_f64x2_zero, DEMOTE_32)
SIMD_OP1(f64x2_promote_low_f32x4, PROMOTE_LOW_64)

#define EXTEND_LOW_16_s(a, b) EXTEND_8x8_s(LANEPTRi128(a), LANEPTRi8(b))
#define EXTEND_HIGH_16_s(a, b) EXTEND_8x8_s(LANEPTRi128(a), &LANEPTRi8(b)[8])
#define EXTEND_LOW_16_u(a, b) EXTEND_8x8_u(LANEPTRi128(a), LANEPTRi8(b))
#define EXTEND_HIGH_16_u(a, b) EXTEND_8x8_u(LANEPTRi128(a), &LANEPTRi8(b)[8])
#define EXTEND_LOW_32_s(a, b) EXTEND_16x4_s(LANEPTRi128(a), LANEPTRi16(b))
#define EXTEND_HIGH_32_s(a, b) EXTEND_16x4_s(LANEPTRi128(a), &LANEPTRi16(b)[4])
#define EXTEND_LOW_32_u(a, b) EXTEND_16x4_u(LANEPTRi128(a), LANEPTRi16(b))
#define EXTEND_HIGH_32_u(a, b) EXTEND_16x4_u(LANEPTRi128(a), &LANEPTRi16(b)[4])
#define EXTEND_LOW_64_s(a, b) EXTEND_32x2_s(LANEPTRi128(a), LANEPTRi32(b))
#define EXTEND_HIGH_64_s(a, b) EXTEND_32x2_s(LANEPTRi128(a), &LANEPTRi32(b)[2])
#define EXTEND_LOW_64_u(a, b) EXTEND_32x2_u(LANEPTRi128(a), LANEPTRi32(b))
#define EXTEND_HIGH_64_u(a, b) EXTEND_32x2_u(LANEPTRi128(a), &LANEPTRi32(b)[2])

#define EXTADD_16_s(a, b) EXTADD_16x8_s(LANEPTRi16(a), LANEPTRi8(b))
#define EXTADD_16_u(a, b) EXTADD_16x8_u(LANEPTRi16(a), LANEPTRi8(b))
#define EXTADD_32_s(a, b) EXTADD_32x4_s(LANEPTRi32(a), LANEPTRi16(b))
#define EXTADD_32_u(a, b) EXTADD_32x4_u(LANEPTRi32(a), LANEPTRi16(b))

SIMD_OP1(i16x8_extend_low_i8x16_s, EXTEND_LOW_16_s)
SIMD_OP1(i16x8_extend_high_i8x16_s, EXTEND_HIGH_16_s)
SIMD_OP1(i16x8_extend_low_i8x16_u, EXTEND_LOW_16_u)
SIMD_OP1(i16x8_extend_high_i8x16_u, EXTEND_HIGH_16_u)
SIMD_OP1(i32x4_extend_low_i16x8_s, EXTEND_LOW_32_s)
SIMD_OP1(i32x4_extend_high_i16x8_s, EXTEND_HIGH_32_s)
SIMD_OP1(i32x4_extend_low_i16x8_u, EXTEND_LOW_32_u)
SIMD_OP1(i32x4_extend_high_i16x8_u, EXTEND_HIGH_32_u)
SIMD_OP1(i64x2_extend_low_i32x4_s, EXTEND_LOW_64_s)
SIMD_OP1(i64x2_extend_high_i32x4_s, EXTEND_HIGH_64_s)
SIMD_OP1(i64x2_extend_low_i32x4_u, EXTEND_LOW_64_u)
SIMD_OP1(i64x2_extend_high_i32x4_u, EXTEND_HIGH_64_u)

SIMD_OP1(i16x8_extadd_pairwise_i8x16_s, EXTADD_16_s)
SIMD_OP1(i16x8_extadd_pairwise_i8x16_u, EXTADD_16_u)
SIMD_OP1(i32x4_extadd_pairwise_i16x8_s, EXTADD_32_s)
SIMD_OP1(i32x4_extadd_pairwise_i16x8_u, EXTADD_32_u)

#define EXTMUL_LOW_16_s(a, b, c)                                              \
        EXTMUL_16_s(LANEPTRi16(a), &LANEPTRi8(b)[0], &LANEPTRi8(c)[0])
#define EXTMUL_LOW_16_u(a, b, c)                                              \
        EXTMUL_16_u(LANEPTRi16(a), &LANEPTRi8(b)[0], &LANEPTRi8(c)[0])
#define EXTMUL_LOW_32_s(a, b, c)                                              \
        EXTMUL_32_s(LANEPTRi32(a), &LANEPTRi16(b)[0], &LANEPTRi16(c)[0])
#define EXTMUL_LOW_32_u(a, b, c)                                              \
        EXTMUL_32_u(LANEPTRi32(a), &LANEPTRi16(b)[0], &LANEPTRi16(c)[0])
#define EXTMUL_LOW_64_s(a, b, c)                                              \
        EXTMUL_64_s(LANEPTRi64(a), &LANEPTRi32(b)[0], &LANEPTRi32(c)[0])
#define EXTMUL_LOW_64_u(a, b, c)                                              \
        EXTMUL_64_u(LANEPTRi64(a), &LANEPTRi32(b)[0], &LANEPTRi32(c)[0])
#define EXTMUL_HIGH_16_s(a, b, c)                                             \
        EXTMUL_16_s(LANEPTRi16(a), &LANEPTRi8(b)[8], &LANEPTRi8(c)[8])
#define EXTMUL_HIGH_16_u(a, b, c)                                             \
        EXTMUL_16_u(LANEPTRi16(a), &LANEPTRi8(b)[8], &LANEPTRi8(c)[8])
#define EXTMUL_HIGH_32_s(a, b, c)                                             \
        EXTMUL_32_s(LANEPTRi32(a), &LANEPTRi16(b)[4], &LANEPTRi16(c)[4])
#define EXTMUL_HIGH_32_u(a, b, c)                                             \
        EXTMUL_32_u(LANEPTRi32(a), &LANEPTRi16(b)[4], &LANEPTRi16(c)[4])
#define EXTMUL_HIGH_64_s(a, b, c)                                             \
        EXTMUL_64_s(LANEPTRi64(a), &LANEPTRi32(b)[2], &LANEPTRi32(c)[2])
#define EXTMUL_HIGH_64_u(a, b, c)                                             \
        EXTMUL_64_u(LANEPTRi64(a), &LANEPTRi32(b)[2], &LANEPTRi32(c)[2])

SIMD_OP2(i16x8_extmul_low_i8x16_s, EXTMUL_LOW_16_s)
SIMD_OP2(i16x8_extmul_low_i8x16_u, EXTMUL_LOW_16_u)
SIMD_OP2(i32x4_extmul_low_i16x8_s, EXTMUL_LOW_32_s)
SIMD_OP2(i32x4_extmul_low_i16x8_u, EXTMUL_LOW_32_u)
SIMD_OP2(i64x2_extmul_low_i32x4_s, EXTMUL_LOW_64_s)
SIMD_OP2(i64x2_extmul_low_i32x4_u, EXTMUL_LOW_64_u)
SIMD_OP2(i16x8_extmul_high_i8x16_s, EXTMUL_HIGH_16_s)
SIMD_OP2(i16x8_extmul_high_i8x16_u, EXTMUL_HIGH_16_u)
SIMD_OP2(i32x4_extmul_high_i16x8_s, EXTMUL_HIGH_32_s)
SIMD_OP2(i32x4_extmul_high_i16x8_u, EXTMUL_HIGH_32_u)
SIMD_OP2(i64x2_extmul_high_i32x4_s, EXTMUL_HIGH_64_s)
SIMD_OP2(i64x2_extmul_high_i32x4_u, EXTMUL_HIGH_64_u)

#define CMP_LANE(I_OR_F, LS, a, b, c, I, CAST, OP)                            \
        LANEPTRi##LS(a)[I] = (CAST LANEPTR##I_OR_F##LS(                       \
                                     b)[I] OP CAST LANEPTR##I_OR_F##LS(c)[I]) \
                                     ? (uint##LS##_t) - 1                     \
                                     : 0

#define LANE_OP2_LS(I_OR_F, LS, a, b, c, I, OP)                               \
        LANEPTR##I_OR_F##LS(a)[I] =                                           \
                OP(LS, LANEPTR##I_OR_F##LS(b)[I], LANEPTR##I_OR_F##LS(c)[I])

#define EQ(I_OR_F, LS, a, b, c, I) CMP_LANE(I_OR_F, LS, a, b, c, I, , ==)
#define NE(I_OR_F, LS, a, b, c, I) CMP_LANE(I_OR_F, LS, a, b, c, I, , !=)
#define LT(I_OR_F, LS, a, b, c, I) CMP_LANE(I_OR_F, LS, a, b, c, I, , <)
#define GT(I_OR_F, LS, a, b, c, I) CMP_LANE(I_OR_F, LS, a, b, c, I, , >)
#define LE(I_OR_F, LS, a, b, c, I) CMP_LANE(I_OR_F, LS, a, b, c, I, , <=)
#define GE(I_OR_F, LS, a, b, c, I) CMP_LANE(I_OR_F, LS, a, b, c, I, , >=)
#define LT_S(I_OR_F, LS, a, b, c, I)                                          \
        CMP_LANE(I_OR_F, LS, a, b, c, I, (int##LS##_t), <)
#define GT_S(I_OR_F, LS, a, b, c, I)                                          \
        CMP_LANE(I_OR_F, LS, a, b, c, I, (int##LS##_t), >)
#define LE_S(I_OR_F, LS, a, b, c, I)                                          \
        CMP_LANE(I_OR_F, LS, a, b, c, I, (int##LS##_t), <=)
#define GE_S(I_OR_F, LS, a, b, c, I)                                          \
        CMP_LANE(I_OR_F, LS, a, b, c, I, (int##LS##_t), >=)

#define AVGR(N, a, b) (((a) + (b) + 1) / 2)

#define LANE_AVGR(I_OR_F, LS, a, b, c, I)                                     \
        LANE_OP2_LS(I_OR_F, LS, a, b, c, I, AVGR)

SIMD_FOREACH_LANES_OP2(f32x4_eq, f, 32, EQ)
SIMD_FOREACH_LANES_OP2(f32x4_ne, f, 32, NE)
SIMD_FOREACH_LANES_OP2(f32x4_lt, f, 32, LT)
SIMD_FOREACH_LANES_OP2(f32x4_gt, f, 32, GT)
SIMD_FOREACH_LANES_OP2(f32x4_le, f, 32, LE)
SIMD_FOREACH_LANES_OP2(f32x4_ge, f, 32, GE)

SIMD_FOREACH_LANES_OP2(f64x2_eq, f, 64, EQ)
SIMD_FOREACH_LANES_OP2(f64x2_ne, f, 64, NE)
SIMD_FOREACH_LANES_OP2(f64x2_lt, f, 64, LT)
SIMD_FOREACH_LANES_OP2(f64x2_gt, f, 64, GT)
SIMD_FOREACH_LANES_OP2(f64x2_le, f, 64, LE)
SIMD_FOREACH_LANES_OP2(f64x2_ge, f, 64, GE)

SIMD_FOREACH_LANES_OP2(i8x16_eq, i, 8, EQ)
SIMD_FOREACH_LANES_OP2(i8x16_ne, i, 8, NE)
SIMD_FOREACH_LANES_OP2(i8x16_lt_s, i, 8, LT_S)
SIMD_FOREACH_LANES_OP2(i8x16_lt_u, i, 8, LT)
SIMD_FOREACH_LANES_OP2(i8x16_gt_s, i, 8, GT_S)
SIMD_FOREACH_LANES_OP2(i8x16_gt_u, i, 8, GT)
SIMD_FOREACH_LANES_OP2(i8x16_le_s, i, 8, LE_S)
SIMD_FOREACH_LANES_OP2(i8x16_le_u, i, 8, LE)
SIMD_FOREACH_LANES_OP2(i8x16_ge_s, i, 8, GE_S)
SIMD_FOREACH_LANES_OP2(i8x16_ge_u, i, 8, GE)

SIMD_FOREACH_LANES_OP2(i16x8_eq, i, 16, EQ)
SIMD_FOREACH_LANES_OP2(i16x8_ne, i, 16, NE)
SIMD_FOREACH_LANES_OP2(i16x8_lt_s, i, 16, LT_S)
SIMD_FOREACH_LANES_OP2(i16x8_lt_u, i, 16, LT)
SIMD_FOREACH_LANES_OP2(i16x8_gt_s, i, 16, GT_S)
SIMD_FOREACH_LANES_OP2(i16x8_gt_u, i, 16, GT)
SIMD_FOREACH_LANES_OP2(i16x8_le_s, i, 16, LE_S)
SIMD_FOREACH_LANES_OP2(i16x8_le_u, i, 16, LE)
SIMD_FOREACH_LANES_OP2(i16x8_ge_s, i, 16, GE_S)
SIMD_FOREACH_LANES_OP2(i16x8_ge_u, i, 16, GE)

SIMD_FOREACH_LANES_OP2(i32x4_eq, i, 32, EQ)
SIMD_FOREACH_LANES_OP2(i32x4_ne, i, 32, NE)
SIMD_FOREACH_LANES_OP2(i32x4_lt_s, i, 32, LT_S)
SIMD_FOREACH_LANES_OP2(i32x4_lt_u, i, 32, LT)
SIMD_FOREACH_LANES_OP2(i32x4_gt_s, i, 32, GT_S)
SIMD_FOREACH_LANES_OP2(i32x4_gt_u, i, 32, GT)
SIMD_FOREACH_LANES_OP2(i32x4_le_s, i, 32, LE_S)
SIMD_FOREACH_LANES_OP2(i32x4_le_u, i, 32, LE)
SIMD_FOREACH_LANES_OP2(i32x4_ge_s, i, 32, GE_S)
SIMD_FOREACH_LANES_OP2(i32x4_ge_u, i, 32, GE)

SIMD_FOREACH_LANES_OP2(i64x2_eq, i, 64, EQ)
SIMD_FOREACH_LANES_OP2(i64x2_ne, i, 64, NE)
SIMD_FOREACH_LANES_OP2(i64x2_lt_s, i, 64, LT_S)
SIMD_FOREACH_LANES_OP2(i64x2_gt_s, i, 64, GT_S)
SIMD_FOREACH_LANES_OP2(i64x2_le_s, i, 64, LE_S)
SIMD_FOREACH_LANES_OP2(i64x2_ge_s, i, 64, GE_S)

SIMD_FOREACH_LANES_OP2(i8x16_avgr_u, i, 8, LANE_AVGR)
SIMD_FOREACH_LANES_OP2(i16x8_avgr_u, i, 16, LANE_AVGR)

#define LANE_OP1(I_OR_F, LS, a, b, I, OP)                                     \
        LANEPTR##I_OR_F##LS(a)[I] = OP(LANEPTR##I_OR_F##LS(b)[I])

#define LANE_OP1_LS(I_OR_F, LS, a, b, I, OP)                                  \
        LANEPTR##I_OR_F##LS(a)[I] = OP(LS, LANEPTR##I_OR_F##LS(b)[I])

#define LANE_CEILF(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, ceilf)
#define LANE_TRUNCF(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, truncf)
#define LANE_FLOORF(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, floorf)
#define LANE_NEARESTF(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, rintf)

#define LANE_CEIL(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, ceil)
#define LANE_TRUNC(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, trunc)
#define LANE_FLOOR(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, floor)
#define LANE_NEAREST(I_OR_F, LS, a, b, I) LANE_OP1(I_OR_F, LS, a, b, I, rint)

#define INT_ABS(LS, n) (uint##LS##_t) llabs((int##LS##_t)n)
#define INT_NEG(LS, n) (uint##LS##_t) - n

#define LANE_ABS(I_OR_F, LS, a, b, I) LANE_OP1_LS(I_OR_F, LS, a, b, I, INT_ABS)
#define LANE_NEG(I_OR_F, LS, a, b, I) LANE_OP1_LS(I_OR_F, LS, a, b, I, INT_NEG)

SIMD_FOREACH_LANES_OP1(f32x4_ceil, f, 32, LANE_CEILF)
SIMD_FOREACH_LANES_OP1(f32x4_trunc, f, 32, LANE_TRUNCF)
SIMD_FOREACH_LANES_OP1(f32x4_floor, f, 32, LANE_FLOORF)
SIMD_FOREACH_LANES_OP1(f32x4_nearest, f, 32, LANE_NEARESTF)

SIMD_FOREACH_LANES_OP1(f64x2_ceil, f, 64, LANE_CEIL)
SIMD_FOREACH_LANES_OP1(f64x2_trunc, f, 64, LANE_TRUNC)
SIMD_FOREACH_LANES_OP1(f64x2_floor, f, 64, LANE_FLOOR)
SIMD_FOREACH_LANES_OP1(f64x2_nearest, f, 64, LANE_NEAREST)

SIMD_FOREACH_LANES_OP1(i8x16_abs, i, 8, LANE_ABS)
SIMD_FOREACH_LANES_OP1(i8x16_neg, i, 8, LANE_NEG)
SIMD_FOREACH_LANES_OP1(i16x8_abs, i, 16, LANE_ABS)
SIMD_FOREACH_LANES_OP1(i16x8_neg, i, 16, LANE_NEG)
SIMD_FOREACH_LANES_OP1(i32x4_abs, i, 32, LANE_ABS)
SIMD_FOREACH_LANES_OP1(i32x4_neg, i, 32, LANE_NEG)
SIMD_FOREACH_LANES_OP1(i64x2_abs, i, 64, LANE_ABS)
SIMD_FOREACH_LANES_OP1(i64x2_neg, i, 64, LANE_NEG)

#define Q15MULR(N, a, b)                                                      \
        (uint##N##_t) SAT_s(                                                  \
                N,                                                            \
                ((EXTEND_s(32, N, a)) * (EXTEND_s(32, N, b)) + 0x4000) >> 15)

#define LANE_Q15MULR(I_OR_F, LS, a, b, c, I)                                  \
        LANE_OP2_LS(I_OR_F, LS, a, b, c, I, Q15MULR)

SIMD_FOREACH_LANES_OP2(i16x8_q15mulr_sat_s, i, 16, LANE_Q15MULR)

#define DOT32_1(LS, a, b, c, I)                                               \
        le32_encode(                                                          \
                &LANEPTRi32(a)[I],                                            \
                MUL(32, EXTEND_s(32, 16, le16_decode(&LANEPTRi16(b)[I * 2])), \
                    EXTEND_s(32, 16, le16_decode(&LANEPTRi16(c)[I * 2]))) +   \
                        MUL(32,                                               \
                            EXTEND_s(32, 16,                                  \
                                     le16_decode(&LANEPTRi16(b)[I * 2 + 1])), \
                            EXTEND_s(                                         \
                                    32, 16,                                   \
                                    le16_decode(&LANEPTRi16(c)[I * 2 + 1]))))

#define DOT32(a, b, c) FOREACH_LANES3(32, a, b, c, DOT32_1)

SIMD_OP2(i32x4_dot_i16x8_s, DOT32)

#define TRUNC_SAT_s1(LS, a, b, I)                                             \
        do {                                                                  \
                int32_t r;                                                    \
                float f = lef32_decode(&LANEPTRf32(b)[I]);                    \
                TRUNC_SAT_S_32_32(r, f);                                      \
                le32_encode(&LANEPTRi32(a)[I], (uint32_t)r);                  \
        } while (0)

#define TRUNC_SAT_u1(LS, a, b, I)                                             \
        do {                                                                  \
                uint32_t r;                                                   \
                float f = lef32_decode(&LANEPTRf32(b)[I]);                    \
                TRUNC_SAT_U_32_32(r, f);                                      \
                le32_encode(&LANEPTRi32(a)[I], r);                            \
        } while (0)

#define TRUNC_SAT_32_s(a, b) FOREACH_LANES(32, a, b, TRUNC_SAT_s1)
#define TRUNC_SAT_32_u(a, b) FOREACH_LANES(32, a, b, TRUNC_SAT_u1)

SIMD_OP1(i32x4_trunc_sat_f32x4_s, TRUNC_SAT_32_s)
SIMD_OP1(i32x4_trunc_sat_f32x4_u, TRUNC_SAT_32_u)

#define TRUNC_SAT_64_s1(LS, a, b, I)                                          \
        do {                                                                  \
                int32_t r;                                                    \
                double f = lef64_decode(&LANEPTRf64(b)[I]);                   \
                TRUNC_SAT_S_64_32(r, f);                                      \
                le32_encode(&LANEPTRi32(a)[I], (uint32_t)r);                  \
        } while (0)

#define TRUNC_SAT_64_u1(LS, a, b, I)                                          \
        do {                                                                  \
                uint32_t r;                                                   \
                double f = lef64_decode(&LANEPTRf64(b)[I]);                   \
                TRUNC_SAT_U_64_32(r, f);                                      \
                le32_encode(&LANEPTRi32(a)[I], r);                            \
        } while (0)

#define TRUNC_SAT_64_s(a, b)                                                  \
        FOREACH_LANES(64, a, b, TRUNC_SAT_64_s1);                             \
        LANEPTRi32(a)[2] = 0;                                                 \
        LANEPTRi32(a)[3] = 0
#define TRUNC_SAT_64_u(a, b)                                                  \
        FOREACH_LANES(64, a, b, TRUNC_SAT_64_u1);                             \
        LANEPTRi32(a)[2] = 0;                                                 \
        LANEPTRi32(a)[3] = 0

SIMD_OP1(i32x4_trunc_sat_f64x2_s_zero, TRUNC_SAT_64_s)
SIMD_OP1(i32x4_trunc_sat_f64x2_u_zero, TRUNC_SAT_64_u)

#define POPCNT(LS, a) wasm_popcount(a)

#define LANE_POPCNT(I_OR_F, LS, a, b, I)                                      \
        LANE_OP1_LS(I_OR_F, LS, a, b, I, POPCNT)

SIMD_FOREACH_LANES_OP1(i8x16_popcnt, i, 8, LANE_POPCNT)

#define LANE_SWIZZLE(I_OR_F, LS, a, b, c, I)                                  \
        do {                                                                  \
                uint##LS##_t s = LANEPTR##I_OR_F##LS(c)[I];                   \
                uint##LS##_t r;                                               \
                if (s < 128 / LS) {                                           \
                        r = LANEPTR##I_OR_F##LS(b)[s];                        \
                } else {                                                      \
                        r = 0;                                                \
                }                                                             \
                LANEPTR##I_OR_F##LS(a)[I] = r;                                \
        } while (0)

SIMD_FOREACH_LANES_OP2(i8x16_swizzle, i, 8, LANE_SWIZZLE)

INSN_IMPL(i8x16_shuffle)
{
        int ret;
        LOAD_PC;
        uint8_t lane[16];
        uint32_t i;
        for (i = 0; i < 16; i++) {
                READ_LANEIDX32_TO(lane[i]);
        }
        POP_VAL(TYPE_v128, b);
        POP_VAL(TYPE_v128, a);
        struct val val_c;
        if (EXECUTING) {
                uint8_t *result = val_c.u.v128.i8;
                const uint8_t *a = val_a.u.v128.i8;
                const uint8_t *b = val_b.u.v128.i8;
                for (i = 0; i < 16; i++) {
                        uint8_t s = lane[i];
                        if (s < 16) {
                                result[i] = a[s];
                        } else {
                                result[i] = b[s - 16];
                        }
                }
        }
        PUSH_VAL(TYPE_v128, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        INSN_FAIL;
}
