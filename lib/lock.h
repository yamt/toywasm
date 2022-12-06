#include "toywasm_config.h"

#if defined(TOYWASM_ENABLE_WASM_THREADS)
#include <pthread.h>

struct toywasm_mutex {
        pthread_mutex_t lock;
};

void toywasm_mutex_init(struct toywasm_mutex *lock);
void toywasm_mutex_destroy(struct toywasm_mutex *lock);
void toywasm_mutex_lock(struct toywasm_mutex *lock);
void toywasm_mutex_unlock(struct toywasm_mutex *lock);
#else /* defined(TOYWASM_ENABLE_WASM_THREADS) */
#define toywasm_mutex_init(a)
#define toywasm_mutex_destroy(a)
#define toywasm_mutex_lock(a)
#define toywasm_mutex_unlock(a)
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
