#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * time_t overflow check in portable code is combersome because time_t
 * can be any integer types (posix) or even floating-point numeric types. (C)
 *
 * this implementation assumes posix.
 * that is,
 * - it's an integer.
 * - it can be signed or unsigned.
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
        /*
         * XXX the following casts are to avoid signed integer overflow
         * where time_t is signed
         */
        c->tv_sec = (time_t)((uintmax_t)a->tv_sec + b->tv_sec + ovfl);
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
timespec_now(clockid_t id, struct timespec *a)
{
        int ret = clock_gettime(id, a);
        if (ret != 0) {
                assert(errno != 0);
                return errno;
        }
        assert_normalized(a);
        return 0;
}

int
abstime_from_reltime_ns(clockid_t id, struct timespec *abstime,
                        uint64_t reltime_ns)
{
        struct timespec now;
        struct timespec reltime;
        int ret;
        ret = timespec_from_ns(&reltime, reltime_ns);
        if (ret != 0) {
                goto fail;
        }
        ret = timespec_now(id, &now);
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
int
abstime_from_reltime_ms(clockid_t id, struct timespec *abstime, int reltime_ms)
{
        return abstime_from_reltime_ns(id, abstime,
                                       (uint64_t)reltime_ms * 1000000);
}

int
abstime_to_reltime_ms_roundup(clockid_t id, const struct timespec *abstime,
                              int *reltime_ms)
{
        struct timespec now;
        struct timespec reltime;
        int ret;
        ret = timespec_now(id, &now);
        if (ret != 0) {
                goto fail;
        }
        if (timespec_cmp(abstime, &now) < 0) {
                *reltime_ms = 0;
                return 0;
        }
        timespec_sub(abstime, &now, &reltime);
        if (ret != 0) {
                goto fail;
        }
        if (reltime.tv_sec > INT_MAX / 1000) {
                ret = EOVERFLOW;
                goto fail;
        }
        int msec = reltime.tv_sec * 1000;
        int msec2 = (reltime.tv_nsec + 999999) / 1000000;
        if (INT_MAX - msec < msec2) {
                ret = EOVERFLOW;
                goto fail;
        }
        *reltime_ms = msec + msec2;
        return 0;
fail:
        return ret;
}

int
convert_timespec(clockid_t from_id, clockid_t to_id,
                 const struct timespec *from_ts, struct timespec *result)
{
        struct timespec from_now;
        struct timespec to_now;
        int ret;
        ret = timespec_now(from_id, &from_now);
        if (ret != 0) {
                goto fail;
        }
        ret = timespec_now(to_id, &to_now);
        if (ret != 0) {
                goto fail;
        }
        if (timespec_cmp(from_ts, &from_now) >= 0) {
                struct timespec t;
                timespec_sub(from_ts, &from_now, &t);
                ret = timespec_add(&t, &to_now, result);
                if (ret != 0) {
                        goto fail;
                }
        } else {
                struct timespec t;
                timespec_sub(&from_now, from_ts, &t);
                if (timespec_cmp(&to_now, &t) > 0) {
                        ret = EOVERFLOW;
                        goto fail;
                }
                timespec_sub(&to_now, &t, result);
        }
        return 0;
fail:
        return ret;
}

uint64_t
timespec_to_ms(const struct timespec *tv)
{
        if (UINT64_MAX / 1000 < (uint64_t)tv->tv_sec) {
                return UINT64_MAX;
        }
        uint64_t ms1 = (uint64_t)tv->tv_sec * 1000;
        uint64_t ms2 = tv->tv_nsec / 1000000;
        if (UINT64_MAX - ms1 < ms2) {
                return UINT64_MAX;
        }
        return ms1 + ms2;
}

int
timespec_sleep(clockid_t id, const struct timespec *absto)
{
        while (true) {
                struct timespec now;
                int ret;

                ret = timespec_now(id, &now);
                if (ret != 0) {
                        return ret;
                }
                if (timespec_cmp(absto, &now) <= 0) {
                        return ETIMEDOUT;
                }
                struct timespec diff;
                timespec_sub(absto, &now, &diff);
                ret = nanosleep(&diff, NULL);
                if (ret != 0) {
                        return errno;
                }
        }
}
