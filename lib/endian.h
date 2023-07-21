#if !defined(_ENDIAN_H)
#define _ENDIAN_H
#include <stdint.h>

uint8_t le8_to_host(uint8_t v);
uint16_t le16_to_host(uint16_t v);
uint32_t le32_to_host(uint32_t v);
uint64_t le64_to_host(uint64_t v);

uint8_t host_to_le8(uint8_t v);
uint16_t host_to_le16(uint16_t v);
uint32_t host_to_le32(uint32_t v);
uint64_t host_to_le64(uint64_t v);

void le8_encode(void *p, uint8_t v);
void le16_encode(void *p, uint16_t v);
void le32_encode(void *p, uint32_t v);
void le64_encode(void *p, uint64_t v);

uint8_t le8_decode(const void *p);
uint16_t le16_decode(const void *p);
uint32_t le32_decode(const void *p);
uint64_t le64_decode(const void *p);

void lef32_encode(void *p, float v);
void lef64_encode(void *p, double v);

/*
 * x87 fld/fstp does not preserve sNaN. it breaks wasm semantics.
 *
 * while in later processors we can use XMM registers (eg. -msse2) which
 * don't have the problem, x87 ST0 register is still used to return
 * float/double function results as it's specified by the i386 ABI.
 *
 * while GCC has -mno-fp-ret-in-387 to alter the ABI, Clang unfortunately
 * doesn't seem to have an equivalent.
 *
 * here we avoid the ABI problem by inlining the functions.
 */
#if defined(__i386__)
__attribute__((always_inline, used)) static float
lef32_decode(const void *p)
{
        union {
                uint32_t i;
                float f;
        } u;
        u.i = le32_decode(p);
        return u.f;
}
#else
float lef32_decode(const void *p);
#endif

#if defined(__i386__)
__attribute__((always_inline, used)) static double
lef64_decode(const void *p)
{
        union {
                uint64_t i;
                double f;
        } u;
        u.i = le64_decode(p);
        return u.f;
}
#else
double lef64_decode(const void *p);
#endif

#endif /* !defined(_ENDIAN_H) */
