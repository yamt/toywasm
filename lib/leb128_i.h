#if defined(TOYWASM_LEB128_INLINE)
#define TOYWASM_INLINE static inline
#else
#define TOYWASM_INLINE
#endif

static int
leb128_read_u8(const uint8_t **pp, const uint8_t *ep, uint8_t *resultp)
{
        if (ep != NULL && 1 > ep - *pp) {
                return EINVAL;
        }
        *resultp = *(*pp)++;
        return 0;
}

TOYWASM_INLINE int
read_leb(const uint8_t **pp, const uint8_t *ep, unsigned int bits,
         bool is_signed, uint64_t *resultp)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        /*
         * A fast path for small values.
         * While it might look silly, it's used very frequently
         * for localidx etc.
         */
        ret = leb128_read_u8(&p, ep, &u8);
        if (ret != 0) {
                return ret;
        }
        if (__predict_true((u8 & 0x80) == 0)) {
                uint64_t result = u8;
                if (is_signed && (u8 & 0x40) != 0) {
                        result |= (uint64_t)(int64_t)(int8_t)0xc0;
                }
                *pp = p;
                *resultp = result;
                return 0;
        }

        unsigned int shift = 0;
        uint64_t result = 0;
        bool is_minus = false;
        const bool error_check = ep != NULL;
        if (error_check) {
                /*
                 * https://webassembly.github.io/spec/core/binary/values.html#integers
                 *
                 * > the total number of bytes encoding a value of type uN
                 * > must not exceed ceil(N/7) bytes.
                 *
                 * > the total number of bytes encoding a value of type sN
                 * > must not exceed ceil(N/7) bytes.
                 *
                 * Note: we have already consumed 1 byte, thus p - 1.
                 */
                const uint8_t *nep = p - 1 + (bits + 7 - 1) / 7;
                if (ep > nep) {
                        ep = nep;
                }
        }
        while (true) {
                uint8_t v = u8 & 0x7f;
                if (error_check) {
                        if (shift >= bits) {
                                if (v != (is_minus ? 0x7f : 0)) {
                                        return E2BIG;
                                }
                        } else {
                                if (is_signed) {
                                        is_minus = v & 0x40;
                                }
                                unsigned int bits_left;
                                if ((bits_left = bits - shift) < 7) {
                                        if (is_signed) {
                                                uint8_t mask =
                                                        (uint8_t)(((unsigned int)-1)
                                                                  << (bits_left -
                                                                      1));
                                                if (is_minus) {
                                                        if ((((~v) & 0x7f) &
                                                             mask) != 0) {
                                                                return E2BIG;
                                                        }
                                                } else {
                                                        if ((v & mask) != 0) {
                                                                return E2BIG;
                                                        }
                                                }
                                        } else {
                                                uint8_t mask =
                                                        (uint8_t)(((unsigned int)-1)
                                                                  << bits_left);
                                                if ((v & mask) != 0) {
                                                        return E2BIG;
                                                }
                                        }
                                }
                        }
                }
                result |= ((uint64_t)v) << shift;
                shift += 7;
                if ((u8 & 0x80) == 0) {
                        break;
                }
                ret = leb128_read_u8(&p, ep, &u8);
                if (ret != 0) {
                        return ret;
                }
        }
        if (!error_check && is_signed) {
                is_minus = u8 & 0x40;
        }
        if (is_minus) {
                if (shift < 64) {
                        result |= (~UINT64_C(0)) << shift;
                }
        }

        *pp = p;
        *resultp = result;
        return 0;
}

TOYWASM_INLINE uint32_t
read_leb_u32_nocheck(const uint8_t **pp)
{
        uint64_t r;
        int ret;

        ret = read_leb(pp, NULL, 32, false, &r);
        assert(ret == 0);
        assert(r <= UINT32_MAX);
        return (uint32_t)r;
}

TOYWASM_INLINE uint32_t
read_leb_i32_nocheck(const uint8_t **pp)
{
        uint64_t r;
        int ret;

        /*
         * https://webassembly.github.io/spec/core/binary/values.html#integers
         * uninterpreted integers are encodeded as signed
         */
        ret = read_leb(pp, NULL, 32, true, &r);
        assert(ret == 0);
        return (uint32_t)r;
}

TOYWASM_INLINE uint64_t
read_leb_i64_nocheck(const uint8_t **pp)
{
        uint64_t r;
        int ret;

        /*
         * https://webassembly.github.io/spec/core/binary/values.html#integers
         * uninterpreted integers are encodeded as signed
         */
        ret = read_leb(pp, NULL, 64, true, &r);
        assert(ret == 0);
        return r;
}

TOYWASM_INLINE int64_t
read_leb_s33_nocheck(const uint8_t **pp)
{
        uint64_t r;
        int ret;

        ret = read_leb(pp, NULL, 33, true, &r);
        assert(ret == 0);
        return (int64_t)r;
}

#undef TOYWASM_INLINE
