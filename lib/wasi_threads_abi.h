#include <stdint.h>

#include "platform.h"

struct wasi_thread_spawn_result {
        uint8_t is_error;
        union {
                uint8_t error;
                uint32_t tid;
        } u;
};

ctassert(sizeof(struct wasi_thread_spawn_result) == 8);
ctassert_offset(struct wasi_thread_spawn_result, u.error, 4);
ctassert_offset(struct wasi_thread_spawn_result, u.tid, 4);

#define WASI_THREADS_ERROR_AGAIN 0
