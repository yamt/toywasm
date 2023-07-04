#include <stdint.h>

void
format_i32(char *p, uint32_t v)
{
        uint32_t i;
        for (i = 1; i <= 32 / 4; i++) {
                uint32_t n = (v >> (32 - i * 4)) & 0xf;
                if (n >= 10) {
                        *p++ = 'a' + (n - 10);
                } else {
                        *p++ = '0' + n;
                }
        }
}
