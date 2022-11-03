#include <string.h>

#include "endian.h"

/*
 * Note: clang is smart enough to make these no-op for amd64.
 */

/*
 * Note: This kind of use of union is explicitly allowed by GCC.
 * https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#Type%2Dpunning
 *
 * Also, it seems that the recent C standards aims to allow it.
 * https://www.open-std.org/jtc1/sc22/wg14/www/docs/dr_257.htm
 */

uint8_t
le8_to_host(uint8_t v)
{
        return v;
}

uint16_t
le16_to_host(uint16_t v)
{
        union {
                uint8_t u8[2];
                uint16_t u16;
        } u;
        u.u16 = v;
        return ((uint16_t)u.u8[1] << 8) | u.u8[0];
}

uint32_t
le32_to_host(uint32_t v)
{
        union {
                uint16_t u16[2];
                uint32_t u32;
        } u;
        u.u32 = v;
        return ((uint32_t)le16_to_host(u.u16[1]) << 16) |
               le16_to_host(u.u16[0]);
}

uint64_t
le64_to_host(uint64_t v)
{
        union {
                uint32_t u32[2];
                uint64_t u64;
        } u;
        u.u64 = v;
        return ((uint64_t)le32_to_host(u.u32[1]) << 32) |
               le32_to_host(u.u32[0]);
}

uint8_t
host_to_le8(uint8_t v)
{
        return v;
}

uint16_t
host_to_le16(uint16_t v)
{
        union {
                uint8_t u8[2];
                uint16_t u16;
        } u;
        u.u8[0] = v;
        u.u8[1] = v >> 8;
        return u.u16;
}

uint32_t
host_to_le32(uint32_t v)
{
        union {
                uint16_t u16[2];
                uint32_t u32;
        } u;
        u.u16[0] = host_to_le16(v);
        u.u16[1] = host_to_le16(v >> 16);
        return u.u32;
}

uint64_t
host_to_le64(uint64_t v)
{
        union {
                uint32_t u32[2];
                uint64_t u64;
        } u;
        u.u32[0] = host_to_le32(v);
        u.u32[1] = host_to_le32(v >> 32);
        return u.u64;
}

void
le8_encode(void *p, uint8_t v)
{
        uint8_t le = v;
        memcpy(p, &le, sizeof(le));
}

void
le16_encode(void *p, uint16_t v)
{
        uint16_t le = host_to_le16(v);
        memcpy(p, &le, sizeof(le));
}

void
le32_encode(void *p, uint32_t v)
{
        uint32_t le = host_to_le32(v);
        memcpy(p, &le, sizeof(le));
}

void
le64_encode(void *p, uint64_t v)
{
        uint64_t le = host_to_le64(v);
        memcpy(p, &le, sizeof(le));
}

uint8_t
le8_decode(const void *p)
{
        uint8_t le;
        memcpy(&le, p, sizeof(le));
        return le;
}

uint16_t
le16_decode(const void *p)
{
        uint16_t le;
        memcpy(&le, p, sizeof(le));
        return le16_to_host(le);
}

uint32_t
le32_decode(const void *p)
{
        uint32_t le;
        memcpy(&le, p, sizeof(le));
        return le32_to_host(le);
}

uint64_t
le64_decode(const void *p)
{
        uint64_t le;
        memcpy(&le, p, sizeof(le));
        return le64_to_host(le);
}
