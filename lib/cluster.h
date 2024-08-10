#if !defined(_MSC_VER)
#include <stdatomic.h>
#endif
#include <stdbool.h>
#include <stdint.h>

#include "lock.h"

enum suspend_state {
        SUSPEND_STATE_NONE = 0,
        SUSPEND_STATE_STOPPING,
        SUSPEND_STATE_RESUMING,
};

/*
 * a group of instances.
 * something similar to the "agent cluster" concept in web.
 */
struct cluster {
        TOYWASM_MUTEX_DEFINE(lock);
        TOYWASM_CV_DEFINE(cv);
        uint32_t nrunners;
        atomic_uint interrupt;

        /* suspend */
        _Atomic enum suspend_state suspend_state;
        uint32_t nparked;
        TOYWASM_CV_DEFINE(stop_cv);
};

void cluster_init(struct cluster *c);
void cluster_destroy(struct cluster *c);
void cluster_join(struct cluster *c);
void cluster_add_thread(struct cluster *c) REQUIRES(c->lock);
void cluster_remove_thread(struct cluster *c) REQUIRES(c->lock);

struct exec_context;
int cluster_check_interrupt(struct exec_context *ctx, const struct cluster *c);
bool cluster_set_interrupt(struct cluster *c);
