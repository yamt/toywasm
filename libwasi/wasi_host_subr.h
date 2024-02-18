#include <stdint.h>
#include <time.h>

struct timespec;
struct timeval;
struct stat;

struct wasi_filestat;
struct wasi_unstable_filestat;
struct utimes_args;

uint64_t timespec_to_ns(const struct timespec *ts);
void timeval_from_ns(struct timeval *tv, uint64_t ns);
int prepare_utimes_tv(const struct utimes_args *args, struct timeval *tvstore,
                      const struct timeval **resultp);
uint8_t wasi_convert_filetype(mode_t mode);
void wasi_convert_filestat(const struct stat *hst, struct wasi_filestat *wst);
int wasi_unstable_convert_filestat(const struct wasi_filestat *wst,
                                   struct wasi_unstable_filestat *uwst);
uint8_t wasi_convert_dirent_filetype(uint8_t hosttype);
int wasi_convert_clockid(uint32_t clockid, clockid_t *hostidp);
uint32_t wasi_convert_errno(int host_errno);
