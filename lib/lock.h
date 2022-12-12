#if !defined(_LOCK_H)
#define _LOCK_H

#include "toywasm_config.h"

#if defined(TOYWASM_ENABLE_WASM_THREADS) && defined(__clang__)
/*
 * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
 *
 * limitations important for us:
 * https://github.com/llvm/llvm-project/issues/20777
 * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#no-conditionally-held-locks
 */
#define _TAS(x) x
#else /* defined(TOYWASM_ENABLE_WASM_THREADS) && defined(__clang__) */
#define _TAS(x)
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) && defined(__clang__) */

#define CAPABILITY(x) _TAS(__attribute__((capability(x))))
#define GUARDED_BY(l) _TAS(__attribute__((guarded_by(l))))
#define GUARDED_VAR(l) _TAS(__attribute__((guarded_var)))
#define REQUIRES(...) _TAS(__attribute__((requires_capability((__VA_ARGS__)))))
#define EXCLUDES(...) _TAS(__attribute__((locks_excluded((__VA_ARGS__)))))
#define ACQUIRES(...) _TAS(__attribute__((acquire_capability((__VA_ARGS__)))))
#define RELEASES(...) _TAS(__attribute__((release_capability((__VA_ARGS__)))))
#define NO_THREAD_SAFETY_ANALYSIS                                             \
        _TAS(__attribute__((no_thread_safety_analysis)))

#if defined(TOYWASM_ENABLE_WASM_THREADS)
#include <pthread.h>

struct CAPABILITY("mutex") toywasm_mutex {
        pthread_mutex_t lock;
};

#define TOYWASM_MUTEX_DEFINE(name) struct toywasm_mutex name
void toywasm_mutex_init(struct toywasm_mutex *lock);
void toywasm_mutex_destroy(struct toywasm_mutex *lock);
void toywasm_mutex_lock(struct toywasm_mutex *lock) ACQUIRES(lock);
void toywasm_mutex_unlock(struct toywasm_mutex *lock) RELEASES(lock);
#else /* defined(TOYWASM_ENABLE_WASM_THREADS) */
#define TOYWASM_MUTEX_DEFINE(name)
#define toywasm_mutex_init(a)
#define toywasm_mutex_destroy(a)
#define toywasm_mutex_lock(a)
#define toywasm_mutex_unlock(a)
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
#endif /* !defined(_LOCK_H) */
