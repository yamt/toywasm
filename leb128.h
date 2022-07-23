#include <stdint.h>

/*
 * our wasm -> C mapping:
 *
 * uninterpreted -> uXXX_t
 * unsigned      -> uXXX_t
 * signed        -> sXXX_t
 */

int read_leb_i32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp);
int read_leb_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp);
int read_leb_s32(const uint8_t **pp, const uint8_t *ep, int32_t *resultp);

int read_leb_i64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp);
int read_leb_u64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp);
int read_leb_s64(const uint8_t **pp, const uint8_t *ep, int64_t *resultp);

int read_leb_u(const uint8_t **pp, const uint8_t *ep, unsigned int bits,
               uint64_t *resultp);
int read_leb_s(const uint8_t **pp, const uint8_t *ep, unsigned int bits,
               int64_t *resultp);

/* "nocheck" variations for commonly used ones */
uint32_t read_leb_u32_nocheck(const uint8_t **pp);
uint32_t read_leb_i32_nocheck(const uint8_t **pp);
uint64_t read_leb_i64_nocheck(const uint8_t **pp);
int64_t read_leb_s33_nocheck(const uint8_t **pp);
