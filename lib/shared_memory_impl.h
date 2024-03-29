#if defined(TOYWASM_ENABLE_WASM_THREADS)
#include <stdint.h>

#include "lock.h"
#include "waitlist.h"

struct shared_meminst {
        /* atomic operations, esp. wait/notify */
        struct waiter_list_table tab;

        /*
         * to serialize memory.grow etc on a shared memory.
         */
        TOYWASM_MUTEX_DEFINE(lock);
};
#endif
