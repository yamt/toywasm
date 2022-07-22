/*
 * https://webassembly.github.io/spec/core/binary/values.html#binary-int
 * https://en.wikipedia.org/wiki/LEB128
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include "decode.h"
#include "leb128.h"
#include "xlog.h"

#include <inttypes.h>

int
read_leb(const uint8_t **pp, const uint8_t *ep, unsigned int bits,
         bool is_signed, uint64_t *resultp)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;
        unsigned int shift = 0;
        uint64_t result = 0;
        bool is_minus = false;

        while (true) {
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        return ret;
                }
                uint8_t v = u8 & 0x7f;
                if (shift >= bits) {
                        if (v != (is_minus ? 0x7f : 0)) {
                                return E2BIG;
                        }
                } else {
                        if (is_signed) {
                                is_minus = v & 0x40;
                        }
                        unsigned int bits_left = bits - shift;
#if 0
                        xlog_printf("sign %u u8 %x v %x bits %u shift %u\n",
                                    is_signed, u8, v, bits, shift);
#endif
                        if (bits_left < 7) {
                                if (is_signed) {
                                        uint8_t mask = ((unsigned int)-1)
                                                       << (bits_left - 1);
#if 0
                                        xlog_printf("sign %u is_minus %u v %x "
                                                    "mask %x bits %u "
                                                    "shift %u\n",
                                                    is_signed, is_minus, v,
                                                    mask, bits, shift);
#endif
                                        if (is_minus) {
                                                if ((((~v) & 0x7f) & mask) !=
                                                    0) {
                                                        return E2BIG;
                                                }
                                        } else {
                                                if ((v & mask) != 0) {
                                                        return E2BIG;
                                                }
                                        }
                                } else {
                                        uint8_t mask = ((unsigned int)-1)
                                                       << bits_left;
                                        if ((v & mask) != 0) {
                                                return E2BIG;
                                        }
                                }
                        }
                }
                result |= ((uint64_t)v) << shift;
                shift += 7;
                if ((u8 & 0x80) == 0) {
                        break;
                }
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

int
read_leb_i32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        /*
         * https://webassembly.github.io/spec/core/binary/values.html#integers
         * uninterpreted integers are encodeded as signed
         */
        ret = read_leb(&p, ep, 32, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = r;
        *pp = p;
        return 0;
}

int
read_leb_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        ret = read_leb(&p, ep, 32, false, &r);
        if (ret != 0) {
                return ret;
        }
        assert(r <= UINT32_MAX);
        *resultp = r;
        *pp = p;
        return 0;
}

int
read_leb_s32(const uint8_t **pp, const uint8_t *ep, int32_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        ret = read_leb(&p, ep, 32, true, &r);
        if (ret != 0) {
                return ret;
        }
        int64_t s = (int64_t)r;
        assert(s <= INT32_MAX && s >= INT32_MIN);
        *resultp = (int32_t)s;
        *pp = p;
        return 0;
}

int
read_leb_i64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        /*
         * https://webassembly.github.io/spec/core/binary/values.html#integers
         * uninterpreted integers are encodeded as signed
         */
        ret = read_leb(&p, ep, 64, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = r;
        *pp = p;
        return 0;
}

int
read_leb_u64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        ret = read_leb(&p, ep, 64, false, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = r;
        *pp = p;
        return 0;
}

int
read_leb_s64(const uint8_t **pp, const uint8_t *ep, int64_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        ret = read_leb(&p, ep, 64, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = (int64_t)r;
        *pp = p;
        return 0;
}

int
read_leb_u(const uint8_t **pp, const uint8_t *ep, unsigned int bits,
           uint64_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        ret = read_leb(&p, ep, bits, false, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = r;
        *pp = p;
        return 0;
}

int
read_leb_s(const uint8_t **pp, const uint8_t *ep, unsigned int bits,
           int64_t *resultp)
{
        const uint8_t *p = *pp;
        uint64_t r;
        int ret;

        ret = read_leb(&p, ep, bits, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = (int64_t)r;
        *pp = p;
        return 0;
}
