/*
 * https://webassembly.github.io/spec/core/binary/values.html#binary-int
 * https://en.wikipedia.org/wiki/LEB128
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "leb128.h"
#include "platform.h"

/* provide non-inline version */
#include "leb128_i.h"

int
read_leb_i32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp)
{
        uint64_t r;
        int ret;

        /*
         * https://webassembly.github.io/spec/core/binary/values.html#integers
         * uninterpreted integers are encodeded as signed
         */
        ret = read_leb(pp, ep, 32, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = (uint32_t)r;
        return 0;
}

WRONG_FUNC_TYPE
int
read_leb_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp)
{
        uint64_t r;
        int ret;

        ret = read_leb(pp, ep, 32, false, &r);
        if (ret != 0) {
                return ret;
        }
        assert(r <= UINT32_MAX);
        *resultp = (uint32_t)r;
        return 0;
}

int
read_leb_s32(const uint8_t **pp, const uint8_t *ep, int32_t *resultp)
{
        uint64_t r;
        int ret;

        ret = read_leb(pp, ep, 32, true, &r);
        if (ret != 0) {
                return ret;
        }
        int64_t s = (int64_t)r;
        assert(s <= INT32_MAX && s >= INT32_MIN);
        *resultp = (int32_t)s;
        return 0;
}

int
read_leb_i64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp)
{
        uint64_t r;
        int ret;

        /*
         * https://webassembly.github.io/spec/core/binary/values.html#integers
         * uninterpreted integers are encodeded as signed
         */
        ret = read_leb(pp, ep, 64, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = r;
        return 0;
}

int
read_leb_u64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp)
{
        uint64_t r;
        int ret;

        ret = read_leb(pp, ep, 64, false, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = r;
        return 0;
}

int
read_leb_s64(const uint8_t **pp, const uint8_t *ep, int64_t *resultp)
{
        uint64_t r;
        int ret;

        ret = read_leb(pp, ep, 64, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = (int64_t)r;
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
        uint64_t r;
        int ret;

        ret = read_leb(pp, ep, bits, true, &r);
        if (ret != 0) {
                return ret;
        }
        *resultp = (int64_t)r;
        return 0;
}
