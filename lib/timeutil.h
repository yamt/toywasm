#include <stdint.h>
#include <time.h>

#include "platform.h"

#if !defined(CLOCK_REALTIME)
#define CLOCK_REALTIME 1
#define CLOCK_MONOTONIC 2
typedef int clockid_t;
#endif

struct timespec;

__BEGIN_EXTERN_C

int timespec_cmp(const struct timespec *a, const struct timespec *b);
int timespec_add(const struct timespec *a, const struct timespec *b,
                 struct timespec *c);
void timespec_sub(const struct timespec *a, const struct timespec *b,
                  struct timespec *c);
int timespec_from_ns(struct timespec *a, uint64_t ns);
int timespec_now(clockid_t id, struct timespec *a);

int abstime_from_reltime_ns(clockid_t id, struct timespec *abstime,
                            uint64_t reltime_ns);

int abstime_from_reltime_ms(clockid_t id, struct timespec *abstime,
                            int reltime_ms);
int abstime_to_reltime_ms_roundup(clockid_t id, const struct timespec *abstime,
                                  int *reltime_ms);
int convert_timespec(clockid_t from_id, clockid_t to_id,
                     const struct timespec *from_ts, struct timespec *result);
uint64_t timespec_to_ms(const struct timespec *tv);
int timespec_sleep(clockid_t id, const struct timespec *absto);

__END_EXTERN_C
