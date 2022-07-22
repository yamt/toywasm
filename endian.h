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
