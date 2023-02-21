#include <stdatomic.h>
#include <stdint.h>

#include "lock.h"

/*
 * a group of instances.
 * something similar to the "agent cluster" concept in web.
 */
struct cluster {
        TOYWASM_MUTEX_DEFINE(lock);
        TOYWASM_CV_DEFINE(cv);
        uint32_t nrunners;
        atomic_uint interrupt;
};

void cluster_init(struct cluster *c);
void cluster_destroy(struct cluster *c);
void cluster_join(struct cluster *c);
void cluster_add_thread(struct cluster *c) REQUIRES(c->lock);
void cluster_remove_thread(struct cluster *c) REQUIRES(c->lock);
