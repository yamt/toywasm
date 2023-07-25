#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include <cmocka.h>

#include "endian.h"
#include "idalloc.h"
#include "leb128.h"
#include "list.h"
#include "timeutil.h"
#include "type.h"

#define TEST_OK(type, encoded_bytes, expected_value)                          \
        p = encoded_bytes;                                                    \
        ep = p + sizeof(encoded_bytes);                                       \
        ret = read_leb_##type(&p, ep, &type);                                 \
        assert_int_equal(ret, 0);                                             \
        assert_ptr_equal(p, ep);                                              \
        assert_int_equal(type, expected_value)

#define TEST_NOCHECK(type, encoded_bytes, expected_value)                     \
        p = encoded_bytes;                                                    \
        ep = p + sizeof(encoded_bytes);                                       \
        type = read_leb_##type##_nocheck(&p);                                 \
        assert_ptr_equal(p, ep);                                              \
        assert_int_equal(type, expected_value)

#define TEST_E2BIG(type, encoded_bytes)                                       \
        p = op = encoded_bytes;                                               \
        ep = p + sizeof(encoded_bytes);                                       \
        ret = read_leb_##type(&p, ep, &type);                                 \
        assert_int_equal(ret, E2BIG);                                         \
        assert_ptr_equal(p, op)

#define TEST_BITS_OK(type, bits, encoded_bytes, expected_value)               \
        p = encoded_bytes;                                                    \
        ep = p + sizeof(encoded_bytes);                                       \
        ret = read_leb_##type(&p, ep, bits, &type##64);                       \
        assert_int_equal(ret, 0);                                             \
        assert_ptr_equal(p, ep);                                              \
        assert_int_equal(type##64, expected_value)

#define TEST_BITS_E2BIG(type, bits, encoded_bytes)                            \
        p = op = encoded_bytes;                                               \
        ep = p + sizeof(encoded_bytes);                                       \
        ret = read_leb_##type(&p, ep, bits, &type##64);                       \
        assert_int_equal(ret, E2BIG);                                         \
        assert_ptr_equal(p, op)

void
test_leb128(void **state)
{
        uint64_t u64;
        uint32_t u32;
        uint32_t i32;
        int32_t s32;
        int64_t s64;
        uint64_t i64;
        const uint8_t *p;
        const uint8_t *op;
        const uint8_t *ep;
        int ret;

        /* https://en.wikipedia.org/wiki/LEB128#Unsigned_LEB128 */
        const uint8_t u_624485[] = {
                0xe5,
                0x8e,
                0x26,
        };

        TEST_OK(u32, u_624485, 624485);
        TEST_NOCHECK(u32, u_624485, 624485);
        TEST_OK(s32, u_624485, 624485);
        TEST_OK(u64, u_624485, 624485);
        TEST_OK(s64, u_624485, 624485);

        TEST_BITS_OK(u, 64, u_624485, 624485);
        TEST_BITS_OK(u, 20, u_624485, 624485);
        TEST_BITS_E2BIG(u, 19, u_624485);
        TEST_BITS_E2BIG(u, 1, u_624485);

        TEST_BITS_OK(s, 64, u_624485, 624485);
        TEST_BITS_OK(s, 21, u_624485, 624485);
        TEST_BITS_E2BIG(s, 20, u_624485);
        TEST_BITS_E2BIG(s, 1, u_624485);

        /* https://en.wikipedia.org/wiki/LEB128#Signed_LEB128 */
        const uint8_t s_minus123456[] = {
                0xc0,
                0xbb,
                0x78,
        };

        TEST_OK(u32, s_minus123456, 0x1e1dc0);
        TEST_NOCHECK(u32, s_minus123456, 0x1e1dc0);
        TEST_OK(s32, s_minus123456, -123456);
        TEST_OK(u64, s_minus123456, 0x1e1dc0);
        TEST_OK(s64, s_minus123456, -123456);

        TEST_BITS_OK(s, 64, s_minus123456, -123456);
        TEST_BITS_OK(s, 18, s_minus123456, -123456);
        TEST_BITS_E2BIG(s, 17, s_minus123456);
        TEST_BITS_E2BIG(s, 1, s_minus123456);

        const uint8_t u_0x100000000[] = {
                0x80, 0x80, 0x80, 0x80, 0x10,
        };
        const uint8_t u_0xffffffff[] = {
                0xff, 0xff, 0xff, 0xff, 0x0f,
        };
        const uint8_t s_minus1[] = {
                0x7f,
        };
        const uint8_t s_minus0x80000000[] = {
                0x80, 0x80, 0x80, 0x80, 0x78,
        };
        const uint8_t s_minus0x80000001[] = {
                0xff, 0xff, 0xff, 0xff, 0x77,
        };
        const uint8_t u_0x7fffffff[] = {
                0xff, 0xff, 0xff, 0xff, 0x07,
        };
        const uint8_t u_0x80000000[] = {
                0x80, 0x80, 0x80, 0x80, 0x08,
        };

        TEST_OK(u32, u_0xffffffff, 0xffffffff);
        TEST_NOCHECK(u32, u_0xffffffff, 0xffffffff);
        TEST_E2BIG(s32, u_0xffffffff);
        TEST_OK(u64, u_0xffffffff, 0xffffffff);
        TEST_OK(s64, u_0xffffffff, 0xffffffff);

        TEST_OK(u32, u_0x7fffffff, 0x7fffffff);
        TEST_NOCHECK(u32, u_0x7fffffff, 0x7fffffff);
        TEST_OK(s32, u_0x7fffffff, 0x7fffffff);
        TEST_OK(u64, u_0x7fffffff, 0x7fffffff);
        TEST_OK(s64, u_0x7fffffff, 0x7fffffff);

        TEST_OK(u32, u_0x80000000, 0x80000000);
        TEST_NOCHECK(u32, u_0x80000000, 0x80000000);
        TEST_E2BIG(s32, u_0x80000000);
        TEST_OK(u64, u_0x80000000, 0x80000000);
        TEST_OK(s64, u_0x80000000, 0x80000000);

        TEST_E2BIG(u32, u_0x100000000);
        TEST_E2BIG(s32, u_0x100000000);
        TEST_OK(u64, u_0x100000000, 0x100000000);
        TEST_OK(s64, u_0x100000000, 0x100000000);

        TEST_OK(u32, s_minus1, 0x7f);
        TEST_NOCHECK(u32, s_minus1, 0x7f);
        TEST_OK(s32, s_minus1, -1);
        TEST_OK(u64, s_minus1, 0x7f);
        TEST_OK(s64, s_minus1, -1);

        TEST_E2BIG(u32, s_minus0x80000000);
        TEST_OK(s32, s_minus0x80000000, -(int64_t)0x80000000);
        TEST_OK(u64, s_minus0x80000000, 0x780000000);
        TEST_OK(s64, s_minus0x80000000, -(int64_t)0x80000000);

        TEST_E2BIG(u32, s_minus0x80000001);
        TEST_E2BIG(s32, s_minus0x80000001);
        TEST_OK(u64, s_minus0x80000001, 0x77fffffff);
        TEST_OK(s64, s_minus0x80000001, -(int64_t)0x80000001);

        const uint8_t u_0x10000000000000000[] = {
                0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02,
        };

#if 0
        TEST_E2BIG(u32, u_0x10000000000000000);
        TEST_E2BIG(s32, u_0x10000000000000000);
#endif
        TEST_E2BIG(u64, u_0x10000000000000000);
        TEST_E2BIG(s64, u_0x10000000000000000);

        const uint8_t u_0xffffffffffffffff[] = {
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
        };

#if 0
        TEST_E2BIG(u32, u_0xffffffffffffffff);
        TEST_E2BIG(s32, u_0xffffffffffffffff);
#endif
        TEST_OK(u64, u_0xffffffffffffffff, 0xffffffffffffffff);
        TEST_E2BIG(s64, u_0xffffffffffffffff);

        /*
         * Uninterpreted integers are encoded as signed.
         */
        const uint8_t i_2155905152_s[] = {
                0x80, 0x81, 0x82, 0x84, 0x78,
        };

        TEST_OK(i32, i_2155905152_s, 2155905152);
        TEST_NOCHECK(i32, i_2155905152_s, 2155905152);
        TEST_E2BIG(u32, i_2155905152_s);
        TEST_OK(s32, i_2155905152_s, (int32_t)2155905152);

        const uint8_t i_2155905152_u[] = {
                0x80, 0x81, 0x82, 0x84, 0x08,
        };

        TEST_E2BIG(i32, i_2155905152_u);
        TEST_OK(u32, i_2155905152_u, 2155905152);
        TEST_E2BIG(s32, i_2155905152_u);

        /*
         * Redundant representations are quite common in
         * actual wasm files.
         * (But it's restricted to ceil(N/7) bytes.
         */

        const uint8_t u_5_redundant[] = {
                0x85,
                0x80,
                0x80,
                0x80,
#if 0
                0x80, 0x80, 0x80, 0x80,
                0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
#endif
                0x00,
        };

        TEST_OK(i32, u_5_redundant, 5);
        TEST_NOCHECK(i32, u_5_redundant, 5);
        TEST_OK(u32, u_5_redundant, 5);
        TEST_OK(s32, u_5_redundant, 5);

        const uint8_t s_minus5_redundant[] = {
                0xfb,
                0xff,
                0xff,
                0xff,
#if 0
                0xff, 0xff, 0xff, 0xff,
#endif
                0x7f,
        };

        TEST_OK(i32, s_minus5_redundant, (uint32_t)-5);
        TEST_E2BIG(u32, s_minus5_redundant);
        TEST_OK(s32, s_minus5_redundant, -5);

        /* some special values for blocktype */
        const uint8_t s33_x40[] = {
                0x40,
        };
        const uint8_t s33_x7f[] = {
                0x7f,
        };
        const uint8_t s33_x6f[] = {
                0x6f,
        };
        TEST_BITS_OK(s, 33, s33_x40, -64);
        TEST_BITS_OK(s, 33, s33_x7f, -1);
        TEST_BITS_OK(s, 33, s33_x6f, -17);

        const uint8_t s64_min[] = {
                0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f,
        };
        TEST_OK(i64, s64_min, INT64_MIN);
        TEST_NOCHECK(i64, s64_min, INT64_MIN);
}

void
test_endian(void **state)
{
        static const uint8_t v64[] = {
                0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        };
        assert_int_equal(0xfedcba9876543210, le64_to_host(*(uint64_t *)v64));

        uint8_t buf64[8];
        *(uint64_t *)buf64 = host_to_le64(0xfedcba9876543210);
        assert_memory_equal(buf64, v64, 8);

        static const uint8_t v32[] = {
                0xaa,
                0xbb,
                0xcc,
                0xdd,
        };
        assert_int_equal(0xddccbbaa, le32_to_host(*(uint32_t *)v32));

        uint8_t buf32[4];
        *(uint32_t *)buf32 = host_to_le32(0xddccbbaa);
        assert_memory_equal(buf32, v32, 4);

        static const uint8_t v16[] = {
                0x11,
                0x92,
        };
        assert_int_equal(0x9211, le16_to_host(*(uint16_t *)v16));

        uint8_t buf16[2];
        *(uint16_t *)buf16 = host_to_le16(0x9211);
        assert_memory_equal(buf16, v16, 2);

        assert_int_equal(0x11, le8_to_host(0x11));
        assert_int_equal(0x00, le8_to_host(0x00));
        assert_int_equal(0xff, le8_to_host(0xff));
        assert_int_equal(0x11, host_to_le8(0x11));
        assert_int_equal(0x00, host_to_le8(0x00));
        assert_int_equal(0xff, host_to_le8(0xff));

        uint8_t buf[10];
        uint8_t poison;
        uint64_t v;

        poison = 0xa1;
        v = 0x1122334455667788;
        memset(buf, poison, sizeof(buf));
        le64_encode(buf, v);
        assert_int_equal(buf[0], v & 0xff);
        assert_int_equal(buf[1], (v >> 1 * 8) & 0xff);
        assert_int_equal(buf[2], (v >> 2 * 8) & 0xff);
        assert_int_equal(buf[3], (v >> 3 * 8) & 0xff);
        assert_int_equal(buf[4], (v >> 4 * 8) & 0xff);
        assert_int_equal(buf[5], (v >> 5 * 8) & 0xff);
        assert_int_equal(buf[6], (v >> 6 * 8) & 0xff);
        assert_int_equal(buf[7], v >> 7 * 8);
        assert_int_equal(buf[8], poison);
        assert_int_equal(buf[9], poison);
        assert_int_equal(v, le64_decode(buf));

        poison = 0xee;
        v = 0xaabbccdd;
        memset(buf, poison, sizeof(buf));
        le32_encode(buf, v);
        assert_int_equal(buf[0], v & 0xff);
        assert_int_equal(buf[1], (v >> 1 * 8) & 0xff);
        assert_int_equal(buf[2], (v >> 2 * 8) & 0xff);
        assert_int_equal(buf[3], (v >> 3 * 8) & 0xff);
        assert_int_equal(buf[4], poison);
        assert_int_equal(buf[5], poison);
        assert_int_equal(buf[6], poison);
        assert_int_equal(buf[7], poison);
        assert_int_equal(buf[8], poison);
        assert_int_equal(buf[9], poison);
        assert_int_equal(v, le32_decode(buf));

        poison = 0x11;
        v = 0xabcd;
        memset(buf, poison, sizeof(buf));
        le16_encode(buf, v);
        assert_int_equal(buf[0], v & 0xff);
        assert_int_equal(buf[1], (v >> 1 * 8) & 0xff);
        assert_int_equal(buf[2], poison);
        assert_int_equal(buf[3], poison);
        assert_int_equal(buf[4], poison);
        assert_int_equal(buf[5], poison);
        assert_int_equal(buf[6], poison);
        assert_int_equal(buf[7], poison);
        assert_int_equal(buf[8], poison);
        assert_int_equal(buf[9], poison);
        assert_int_equal(v, le16_decode(buf));

        poison = 0xdc;
        v = 0x77;
        memset(buf, poison, sizeof(buf));
        le8_encode(buf, v);
        assert_int_equal(buf[0], v & 0xff);
        assert_int_equal(buf[1], poison);
        assert_int_equal(buf[2], poison);
        assert_int_equal(buf[3], poison);
        assert_int_equal(buf[4], poison);
        assert_int_equal(buf[5], poison);
        assert_int_equal(buf[6], poison);
        assert_int_equal(buf[7], poison);
        assert_int_equal(buf[8], poison);
        assert_int_equal(buf[9], poison);
        assert_int_equal(v, le8_decode(buf));
}

void
test_functype(void **state)
{
        struct functype *ft;
        int ret;

        ret = functype_from_string("(iIi)fF", &ft);
        assert_int_equal(ret, 0);
        assert_int_equal(ft->parameter.ntypes, 3);
        assert_int_equal(ft->parameter.types[0], TYPE_i32);
        assert_int_equal(ft->parameter.types[1], TYPE_i64);
        assert_int_equal(ft->parameter.types[2], TYPE_i32);
        assert_int_equal(ft->result.ntypes, 2);
        assert_int_equal(ft->result.types[0], TYPE_f32);
        assert_int_equal(ft->result.types[1], TYPE_f64);
        functype_free(ft);

        ret = functype_from_string("()i", &ft);
        assert_int_equal(ret, 0);
        assert_int_equal(ft->parameter.ntypes, 0);
        assert_int_equal(ft->result.ntypes, 1);
        assert_int_equal(ft->result.types[0], TYPE_i32);
        functype_free(ft);

        ret = functype_from_string("(i)", &ft);
        assert_int_equal(ret, 0);
        assert_int_equal(ft->parameter.ntypes, 1);
        assert_int_equal(ft->parameter.types[0], TYPE_i32);
        assert_int_equal(ft->result.ntypes, 0);
        functype_free(ft);

        ret = functype_from_string("()", &ft);
        assert_int_equal(ret, 0);
        assert_int_equal(ft->parameter.ntypes, 0);
        assert_int_equal(ft->result.ntypes, 0);
        functype_free(ft);

        ret = functype_from_string("", &ft);
        assert_int_equal(ret, EINVAL);

        ret = functype_from_string("(X)", &ft);
        assert_int_equal(ret, EINVAL);

        ret = functype_from_string("()X", &ft);
        assert_int_equal(ret, EINVAL);

        ret = functype_from_string("(i", &ft);
        assert_int_equal(ret, EINVAL);

        ret = functype_from_string("i)", &ft);
        assert_int_equal(ret, EINVAL);

        ret = functype_from_string("i", &ft);
        assert_int_equal(ret, EINVAL);
}

void
test_idalloc(void **state)
{
        struct idalloc a;
        uint32_t bm = 0;
        uint32_t id;
        int dummy;
        int ret;

        /* allocater with ids 1..3 */
        idalloc_init(&a, 1, 3);

        /* allocate all slots */
        ret = idalloc_alloc(&a, &id);
        assert_int_equal(ret, 0);
        assert_in_range(id, 1, 3);
        bm |= 1 << id;
        ret = idalloc_alloc(&a, &id);
        assert_int_equal(ret, 0);
        assert_in_range(id, 1, 3);
        bm |= 1 << id;
        ret = idalloc_alloc(&a, &id);
        assert_int_equal(ret, 0);
        assert_in_range(id, 1, 3);
        bm |= 1 << id;
        assert_int_equal(bm, 14);

        /* no slots to allocate */
        ret = idalloc_alloc(&a, &id);
        assert_int_equal(ret, ERANGE);

        /* check initial user data is NULL */
        assert_ptr_equal(idalloc_get_user(&a, 1), NULL);
        assert_ptr_equal(idalloc_get_user(&a, 2), NULL);
        assert_ptr_equal(idalloc_get_user(&a, 3), NULL);

        /* set some user data */
        idalloc_set_user(&a, 1, NULL);
        idalloc_set_user(&a, 3, &dummy);

        /* check user data */
        assert_ptr_equal(idalloc_get_user(&a, 1), NULL);
        assert_ptr_equal(idalloc_get_user(&a, 2), NULL);
        assert_ptr_equal(idalloc_get_user(&a, 3), &dummy);

        /* still no slots to allocate */
        ret = idalloc_alloc(&a, &id);
        assert_int_equal(ret, ERANGE);

        /* free one of them and reallocate it */
        idalloc_free(&a, 2);
        ret = idalloc_alloc(&a, &id);
        assert_int_equal(ret, 0);
        assert_int_equal(id, 2);

        /* done */
        idalloc_destroy(&a);
}

void
test_timeutil(void **state)
{
        struct timespec a;
        struct timespec b;
        struct timespec c;
        int ret;

        ret = timespec_now(CLOCK_REALTIME, &a);
        assert_int_equal(ret, 0);
        ret = timespec_now(CLOCK_REALTIME, &b);
        assert_int_equal(ret, 0);
        ret = timespec_cmp(&a, &b);
        assert_true(ret <= 0);

        a.tv_sec = 1000;
        a.tv_nsec = 700000000;
        b.tv_sec = 999;
        b.tv_nsec = 900000000;
        ret = timespec_cmp(&a, &a);
        assert_int_equal(ret, 0);
        ret = timespec_cmp(&b, &b);
        assert_int_equal(ret, 0);
        ret = timespec_cmp(&a, &b);
        assert_true(ret > 0);
        a.tv_sec = 1000;
        a.tv_nsec = 700000000;
        b.tv_sec = 1000;
        b.tv_nsec = 900000000;
        ret = timespec_cmp(&a, &a);
        assert_int_equal(ret, 0);
        ret = timespec_cmp(&b, &b);
        assert_int_equal(ret, 0);
        ret = timespec_cmp(&a, &b);
        assert_true(ret < 0);

        a.tv_sec = 1000;
        a.tv_nsec = 700000000;
        b.tv_sec = 100;
        b.tv_nsec = 200000000;
        ret = timespec_add(&a, &b, &c);
        assert_int_equal(ret, 0);
        assert_int_equal(c.tv_sec, 1100);
        assert_int_equal(c.tv_nsec, 900000000);
        ret = timespec_add(&c, &b, &c);
        assert_int_equal(ret, 0);
        assert_int_equal(c.tv_sec, 1201);
        assert_int_equal(c.tv_nsec, 100000000);

        a.tv_sec = 1000;
        a.tv_nsec = 700000000;
        b.tv_sec = 100;
        b.tv_nsec = 400000000;
        ret = timespec_cmp(&a, &b);
        assert_true(ret > 0);
        timespec_sub(&a, &b, &c);
        assert_int_equal(c.tv_sec, 900);
        assert_int_equal(c.tv_nsec, 300000000);
        ret = timespec_cmp(&c, &b);
        assert_true(ret > 0);
        timespec_sub(&c, &b, &c);
        assert_int_equal(c.tv_sec, 799);
        assert_int_equal(c.tv_nsec, 900000000);

        ret = timespec_from_ns(&a, 1000);
        assert_int_equal(ret, 0);
        assert_int_equal(a.tv_sec, 0);
        assert_int_equal(a.tv_nsec, 1000);
        ret = timespec_from_ns(&a, 999999999);
        assert_int_equal(ret, 0);
        assert_int_equal(a.tv_sec, 0);
        assert_int_equal(a.tv_nsec, 999999999);
        ret = timespec_from_ns(&a, 1000000000);
        assert_int_equal(ret, 0);
        assert_int_equal(a.tv_sec, 1);
        assert_int_equal(a.tv_nsec, 0);
        ret = timespec_from_ns(&a, 1000000001);
        assert_int_equal(ret, 0);
        assert_int_equal(a.tv_sec, 1);
        assert_int_equal(a.tv_nsec, 1);
}

void
test_timeutil_int64(void **state)
{
        struct timespec a;
        struct timespec b;
        struct timespec c;
        int ret;

        /* this function assumes time_t is int64_t. */
        if (sizeof(time_t) != sizeof(int64_t) || (time_t)-1 > 0) {
                skip();
        }
        a.tv_sec = (time_t)INT64_MAX; /* cast to avoid build-time warning */
        a.tv_nsec = 500000000;
        b.tv_sec = 0;
        b.tv_nsec = 500000000;
        ret = timespec_add(&a, &b, &c);
        assert_int_equal(ret, EOVERFLOW);

        ret = timespec_from_ns(&a, UINT64_MAX);
        assert_int_equal(ret, 0);
        assert_int_equal(a.tv_sec, UINT64_MAX / 1000000000);
        assert_int_equal(a.tv_nsec, UINT64_MAX % 1000000000);
}

void
test_list(void **state)
{
        struct item {
                void *dummy1;
                LIST_ENTRY(struct item) entry;
                int dummy2;
        };
        LIST_HEAD(struct item) h;

        LIST_HEAD_INIT(&h);
        assert_true(LIST_EMPTY(&h));
        assert_null(LIST_FIRST(&h));
        assert_null(LIST_LAST(&h, struct item, entry));

        struct item item;
        LIST_INSERT_TAIL(&h, &item, entry);
        assert_false(LIST_EMPTY(&h));
        assert_ptr_equal(LIST_FIRST(&h), &item);
        assert_ptr_equal(LIST_LAST(&h, struct item, entry), &item);
        assert_null(LIST_NEXT(&item, entry));
        assert_null(LIST_PREV(&item, &h, struct item, entry));
        LIST_REMOVE(&h, &item, entry);
        assert_true(LIST_EMPTY(&h));
        assert_null(LIST_FIRST(&h));
        assert_null(LIST_LAST(&h, struct item, entry));

        struct item items[10];
        int i;
        for (i = 0; i < 10; i++) {
                LIST_INSERT_TAIL(&h, &items[i], entry);
                assert_false(LIST_EMPTY(&h));
                assert_null(LIST_NEXT(&items[i], entry));
                if (i == 0) {
                        assert_null(
                                LIST_PREV(&items[i], &h, struct item, entry));
                } else {
                        assert_ptr_equal(
                                LIST_PREV(&items[i], &h, struct item, entry),
                                &items[i - 1]);
                        assert_ptr_equal(LIST_NEXT(&items[i - 1], entry),
                                         &items[i]);
                }
        }
        assert_ptr_equal(LIST_FIRST(&h), &items[0]);

        struct item *it;
        i = 0;
        LIST_FOREACH(it, &h, entry) {
                assert_int_equal(it - items, i);
                i++;
        }
        i = 0;
        LIST_FOREACH_REVERSE(it, &h, struct item, entry)
        {
                assert_int_equal(it - items, 9 - i);
                i++;
        }

        LIST_REMOVE(&h, &items[0], entry);
        LIST_REMOVE(&h, &items[2], entry);
        LIST_REMOVE(&h, &items[4], entry);
        LIST_REMOVE(&h, &items[8], entry);
        LIST_REMOVE(&h, &items[6], entry);

        i = 0;
        LIST_FOREACH(it, &h, entry) {
                assert_int_equal(it - items, i * 2 + 1);
                i++;
        }
        assert_int_equal(i, 5);
        i = 0;
        LIST_FOREACH_REVERSE(it, &h, struct item, entry)
        {
                assert_int_equal(it - items, (4 - i) * 2 + 1);
                i++;
        }
        assert_int_equal(i, 5);

        i = 0;
        while ((it = LIST_FIRST(&h)) != NULL) {
                assert_int_equal(it - items, i * 2 + 1);
                i++;
                LIST_REMOVE(&h, it, entry);
        }
        assert_int_equal(i, 5);
        assert_true(LIST_EMPTY(&h));
        assert_null(LIST_FIRST(&h));
        assert_null(LIST_LAST(&h, struct item, entry));
}

int
main(int argc, char **argv)
{
        const struct CMUnitTest tests[] = {
                cmocka_unit_test(test_leb128),
                cmocka_unit_test(test_endian),
                cmocka_unit_test(test_functype),
                cmocka_unit_test(test_idalloc),
                cmocka_unit_test(test_timeutil),
                cmocka_unit_test(test_timeutil_int64),
                cmocka_unit_test(test_list),
        };
        return cmocka_run_group_tests(tests, NULL, NULL);
}
