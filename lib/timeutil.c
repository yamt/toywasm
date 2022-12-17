#define _POSIX_C_SOURCE 199309 /* clock_gettime */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

/*
 * time_t overflow check in portable code is combersome
 * because time_t can be any integer types (posix) or numbers (C).
 *
 * this implementation assumes it's an integer.
 */

static void
assert_normalized(const struct timespec *a)
{
        assert(a->tv_sec >= 0);
        assert(a->tv_nsec >= 0);
        assert(a->tv_nsec < 1000000000);
}

int
timespec_cmp(const struct timespec *a, const struct timespec *b)
{
        if (a->tv_sec == b->tv_sec) {
                if (a->tv_nsec == b->tv_nsec) {
                        return 0;
                }
                if (a->tv_nsec <= b->tv_nsec) {
                        return -1;
                }
                return 1;
        }
        if (a->tv_sec < b->tv_sec) {
                return -1;
        }
        return 1;
}

/*
 * c = a + b
 *
 * can return EOVERFLOW.
 */
int
timespec_add(const struct timespec *a, const struct timespec *b,
             struct timespec *c)
{
        assert_normalized(a);
        assert_normalized(b);
        c->tv_nsec = (uint64_t)a->tv_nsec + b->tv_nsec;
        time_t ovfl = 0;
        if (c->tv_nsec >= 1000000000) {
                c->tv_nsec -= 1000000000;
                ovfl = 1;
        }
        c->tv_sec = a->tv_sec + b->tv_sec + ovfl;
        if (timespec_cmp(c, a) < 0) {
                return EOVERFLOW;
        }
        assert_normalized(c);
        return 0;
}

/*
 * c = a - b
 *
 * assumption: a >= b.
 */
void
timespec_sub(const struct timespec *a, const struct timespec *b,
             struct timespec *c)
{
        assert_normalized(a);
        assert_normalized(b);
        assert(timespec_cmp(a, b) >= 0);
        time_t under = 0;
        if (a->tv_nsec < b->tv_nsec) {
                c->tv_nsec = 1000000000 + a->tv_nsec - b->tv_nsec;
                under = 1;
        } else {
                c->tv_nsec = a->tv_nsec - b->tv_nsec;
        }
        c->tv_sec = a->tv_sec - b->tv_sec - under;
        assert_normalized(c);
}

int
timespec_from_ns(struct timespec *a, uint64_t ns)
{
        time_t sec = ns / 1000000000;
        uint64_t nsec = ns % 1000000000;
        if ((uint64_t)sec * 1000000000 + nsec != ns) {
                return EOVERFLOW;
        }
        a->tv_sec = sec;
        a->tv_nsec = nsec;
        assert_normalized(a);
        return 0;
}

int
timespec_now(struct timespec *a)
{
        int ret = clock_gettime(CLOCK_REALTIME, a);
        if (ret != 0) {
                assert(errno != 0);
                return errno;
        }
        assert_normalized(a);
        return 0;
}

int
abstime_from_reltime_ns(struct timespec *abstime, uint64_t reltime_ns)
{
        struct timespec now;
        struct timespec reltime;
        int ret;
        ret = timespec_from_ns(&reltime, reltime_ns);
        if (ret != 0) {
                goto fail;
        }
        ret = timespec_now(&now);
        if (ret != 0) {
                goto fail;
        }
        ret = timespec_add(&now, &reltime, abstime);
        if (ret != 0) {
                goto fail;
        }
fail:
        return ret;
}
