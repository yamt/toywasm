/* https://webassembly.github.io/spec/core/exec/numerics.html#op-trunc-sat-s */
#define TRUNC_SAT(R, FTYPE, MIN, MAX, TRUNC, A)                               \
        do {                                                                  \
                if (isnan(A)) {                                               \
                        R = 0;                                                \
                } else if (A <= (FTYPE)MIN) {                                 \
                        R = MIN;                                              \
                } else if (A >= (FTYPE)MAX) {                                 \
                        R = MAX;                                              \
                } else {                                                      \
                        R = TRUNC(A);                                         \
                }                                                             \
        } while (false)

#define TRUNC(R, FMIN, FMAX, MIN, MAX, TRUNC, A)                              \
        do {                                                                  \
                if (isnan(A)) {                                               \
                        TRAP(TRAP_INVALID_CONVERSION_TO_INTEGER,              \
                             "integer can't represent nan");                  \
                }                                                             \
                if (A <= FMIN) {                                              \
                        TRAP(TRAP_INTEGER_OVERFLOW,                           \
                             "too small for the type");                       \
                } else if (A >= FMAX) {                                       \
                        TRAP(TRAP_INTEGER_OVERFLOW, "too big for the type");  \
                }                                                             \
                R = TRUNC(A);                                                 \
        } while (false)

#define EQZ(a) ((a == 0) ? 1 : 0)

#define ADD(N, a, b) ((a) + (b))
#define SUB(N, a, b) ((a) - (b))
#define MUL(N, a, b) ((a) * (b))

#define DIV_U(N, a, b) ((a) / (b))
#define DIV_S(N, a, b) (((int##N##_t)a) / ((int##N##_t)b))
#define REM_U(N, a, b) ((a) % (b))
#define REM_S(N, a, b) (((int##N##_t)a) % ((int##N##_t)b))

#define AND(N, a, b) ((a) & (b))
#define OR(N, a, b) ((a) | (b))
#define XOR(N, a, b) ((a) ^ (b))

#define SHR_S(N, a, b) ((int##N##_t)a >> (b % N))
#define SHR_U(N, a, b) (a >> (b % N))
#define SHL(N, a, b) (a << (b % N))
#define ROTL(N, a, b) ((a << (b % N)) | (a >> (N - (b % N))))
#define ROTR(N, a, b) ((a >> (b % N)) | (a << (N - (b % N))))

#define FDIV(N, a, b) ((a) / (b))

#define FMIN32(N, a, b) wasm_fminf(a, b)
#define FMAX32(N, a, b) wasm_fmaxf(a, b)
#define FCOPYSIGN32(N, a, b) copysignf(a, b)

#define FMIN64(N, a, b) wasm_fmin(a, b)
#define FMAX64(N, a, b) wasm_fmax(a, b)
#define FCOPYSIGN64(N, a, b) copysign(a, b)

#define TRUNC_SAT_S_32_32(R, A)                                               \
        TRUNC_SAT(R, float, INT32_MIN, INT32_MAX, truncf, A)
#define TRUNC_SAT_U_32_32(R, A) TRUNC_SAT(R, float, 0, UINT32_MAX, truncf, A)
#define TRUNC_SAT_S_64_32(R, A)                                               \
        TRUNC_SAT(R, double, INT32_MIN, INT32_MAX, trunc, A)
#define TRUNC_SAT_U_64_32(R, A) TRUNC_SAT(R, double, 0, UINT32_MAX, trunc, A)

#define TRUNC_SAT_S_32_64(R, A)                                               \
        TRUNC_SAT(R, float, INT64_MIN, INT64_MAX, truncf, A)
#define TRUNC_SAT_U_32_64(R, A) TRUNC_SAT(R, float, 0, UINT64_MAX, truncf, A)
#define TRUNC_SAT_S_64_64(R, A)                                               \
        TRUNC_SAT(R, double, INT64_MIN, INT64_MAX, trunc, A)
#define TRUNC_SAT_U_64_64(R, A) TRUNC_SAT(R, double, 0, UINT64_MAX, trunc, A)

/*
 * Note: we need separate min/max values for float and integer here
 * because of rounding errors.
 *
 * For example,
 * - -0x800001p+8 is the largest f32 value which is smaller than INT32_MIN.
 * - 0x800000p+8 is the smallest f32 value which is larger than INT32_MAX.
 *
 * Note: INT32_MIN can be represented exactly by f32. On the other hand,
 * INT32_MAX can't:
 *    -0x800000p+8 == INT32_MIN == 0x80000000
 *    0x7fffffp+8 < INT32_MAX == 0x7fffffff < 0x800000p+8
 */
#define TRUNC_S_32_32(R, A)                                                   \
        TRUNC(R, -0x800001p+8, 0x800000p+8, INT32_MIN, INT32_MAX, truncf, A)
#define TRUNC_U_32_32(R, A)                                                   \
        TRUNC(R, -1.0, 0x800000p+9, 0, UINT32_MAX, truncf, A)
#define TRUNC_S_64_32(R, A)                                                   \
        TRUNC(R, INT32_MIN - 1.0, INT32_MAX + 1.0, INT32_MIN, INT32_MAX,      \
              trunc, A)
#define TRUNC_U_64_32(R, A)                                                   \
        TRUNC(R, -1.0, UINT32_MAX + 1.0, 0, UINT32_MAX, trunc, A)

#define TRUNC_S_32_64(R, A)                                                   \
        TRUNC(R, -0x800001p+40, 0x800000p+40, INT64_MIN, INT64_MAX, truncf, A)
#define TRUNC_U_32_64(R, A)                                                   \
        TRUNC(R, -1.0f, 0x800000p+41, 0, UINT64_MAX, truncf, A)
#define TRUNC_S_64_64(R, A)                                                   \
        TRUNC(R, -0x10000000000001p+11, 0x10000000000000p+11, INT64_MIN,      \
              INT64_MAX, trunc, A)
#define TRUNC_U_64_64(R, A)                                                   \
        TRUNC(R, -1.0, 0x10000000000000p+12, 0, UINT64_MAX, trunc, A)
