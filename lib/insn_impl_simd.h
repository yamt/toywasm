/*
 * https://github.com/WebAssembly/simd
 */

#define READ_LANEIDX(VAR, N)                                                  \
        READ_U8(VAR);                                                         \
        CHECK(VAR < N)

/*
 * Note: i128x1 is just for implementation convenience
 */

#define LANEPTR128(val) (&(val)->u.v128)
#define LANEPTR64(val) (val)->u.v128.i64
#define LANEPTR32(val) (val)->u.v128.i32
#define LANEPTR16(val) (val)->u.v128.i16
#define LANEPTR8(val) (val)->u.v128.i8

#define READ_LANEIDX1(VAR) const uint8_t VAR = 0
#define READ_LANEIDX2(VAR) READ_LANEIDX(VAR, 2)
#define READ_LANEIDX4(VAR) READ_LANEIDX(VAR, 4)
#define READ_LANEIDX8(VAR) READ_LANEIDX(VAR, 8)
#define READ_LANEIDX16(VAR) READ_LANEIDX(VAR, 16)
#define READ_LANEIDX32(VAR) READ_LANEIDX(VAR, 32)

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
                        CP(&LANEPTR##LS(&val_c)[lane], datap);                \
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
                        CP(datap, &LANEPTR##LS(&val_v)[lane]);                \
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
                        CP(&le, &LANEPTR##LS(&val_v)[lane]);                  \
                        val_c.u.i##STACK =                                    \
                                EXTEND_##SIGN(LS, le##LS##_decode(&le));      \
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
                        le##LS##_encode(&le, val_x.u.i##STACK);               \
                        CP(&LANEPTR##LS(&val_v)[lane], &le);                  \
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

#define SIMD_UNARYOP(NAME, OP)                                                \
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

#define SIMD_BINOP(NAME, OP)                                                  \
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

#define EXTEND_s(B, S) (int##B##_t)(S)
#define EXTEND_u(B, S) (uint##B##_t)(S)
#define EXTEND_noop(B, S) (S)

#define _EXTEND1(S_OR_U, BDST, BSRC, a, b, I)                                 \
        le##BDST##_encode(                                                    \
                &(a)->i##BDST[I],                                             \
                EXTEND_##S_OR_U(BSRC, le##BSRC##_decode(&((                   \
                                              const uint##BSRC##_t *)b)[I])))

#define EXTEND1(S_OR_U, BDST, BSRC, a, b, I)                                  \
        _EXTEND1(S_OR_U, BDST, BSRC, a, b, I)

#define DBL8 16
#define DBL16 32
#define DBL32 64

#define HALF16 8
#define HALF32 16
#define HALF64 32

#define EXTEND1_s(LS, a, b, I) EXTEND1(s, LS, HALF##LS, a, b, I)
#define EXTEND1_u(LS, a, b, I) EXTEND1(u, LS, HALF##LS, a, b, I)

#define EXTEND_8x8_s(a, b) FOREACH_LANES(16, a, b, EXTEND1_s)
#define EXTEND_8x8_u(a, b) FOREACH_LANES(16, a, b, EXTEND1_u)
#define EXTEND_16x4_s(a, b) FOREACH_LANES(32, a, b, EXTEND1_s)
#define EXTEND_16x4_u(a, b) FOREACH_LANES(32, a, b, EXTEND1_u)
#define EXTEND_32x2_s(a, b) FOREACH_LANES(64, a, b, EXTEND1_s)
#define EXTEND_32x2_u(a, b) FOREACH_LANES(64, a, b, EXTEND1_u)

#define SPLAT1(LS, D, S, I) le##LS##_encode(&(D)->i##LS[I], S)

#define SPLAT_8(D, S) FOREACH_LANES(8, D, *(const uint8_t *)S, SPLAT1)
#define SPLAT_16(D, S) FOREACH_LANES(16, D, *(const uint16_t *)S, SPLAT1)
#define SPLAT_32(D, S) FOREACH_LANES(32, D, *(const uint32_t *)S, SPLAT1)
#define SPLAT_64(D, S) FOREACH_LANES(64, D, *(const uint64_t *)S, SPLAT1)

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
        le32_encode(&(D)->i32[0], *(const uint32_t *)(S));                    \
        (D)->i32[1] = 0;                                                      \
        (D)->i32[2] = 0;                                                      \
        (D)->i32[3] = 0

#define ZERO64(D, S)                                                          \
        le64_encode(&(D)->i64[0], *(const uint64_t *)(S));                    \
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

#define SHL1(LS, a, b, c, I)                                                  \
        LANEPTR##LS(a)[I] =                                                   \
                (uint##LS##_t)(LANEPTR##LS(b)[I] << ((c)->u.i32 % LS))
#define SHR_s1(LS, a, b, c, I)                                                \
        LANEPTR##LS(a)[I] = (uint##LS##_t)((int##LS##_t)LANEPTR##LS(b)[I] >>  \
                                           ((c)->u.i32 % LS))
#define SHR_u1(LS, a, b, c, I)                                                \
        LANEPTR##LS(a)[I] = (LANEPTR##LS(b)[I] >> ((c)->u.i32 % LS))

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
