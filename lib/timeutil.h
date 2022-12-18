#include <stdint.h>

struct timespec;

int timespec_cmp(const struct timespec *a, const struct timespec *b);
int timespec_add(const struct timespec *a, const struct timespec *b,
                 struct timespec *c);
void timespec_sub(const struct timespec *a, const struct timespec *b,
                  struct timespec *c);
int timespec_from_ns(struct timespec *a, uint64_t ns);
int timespec_now(struct timespec *a);

int abstime_from_reltime_ns(struct timespec *abstime, uint64_t reltime_ns);

int abstime_from_reltime_ms(struct timespec *abstime, int reltime_ms);
int abstime_to_reltime_ms_roundup(const struct timespec *abstime,
                                  int *reltime_ms);
