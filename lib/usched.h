#include <stdbool.h>
#include <time.h>

#include "platform.h"
#include "slist.h"

struct exec_context;

struct sched {
        SLIST_HEAD_NAMED(struct exec_context, runq) runq;
        struct timespec next_resched;
};

__BEGIN_EXTERN_C

void sched_enqueue(struct sched *sched, struct exec_context *ctx);
void sched_run(struct sched *sched, struct exec_context *ctx);
void sched_init(struct sched *sched);
void sched_clear(struct sched *sched);
bool sched_need_resched(struct sched *sched);

__END_EXTERN_C
