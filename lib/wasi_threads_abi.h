#include <stdint.h>

struct wasi_thread_spawn_result {
        uint8_t is_error;
        union {
                uint8_t error;
                uint32_t tid;
        } u;
};
