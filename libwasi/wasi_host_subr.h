#include <stdint.h>
#include <time.h>

struct timespec;
struct timeval;
struct stat;

struct wasi_filestat;
struct wasi_unstable_filestat;

uint64_t timespec_to_ns(const struct timespec *ts);
void timeval_from_ns(struct timeval *tv, uint64_t ns);
int prepare_utimes_tv(uint32_t fstflags, uint64_t atim, uint64_t mtim,
                      struct timeval tvstore[2],
                      const struct timeval **resultp);
uint8_t wasi_convert_filetype(mode_t mode);
void wasi_convert_filestat(const struct stat *hst, struct wasi_filestat *wst);
void wasi_unstable_convert_filestat(const struct stat *hst,
                                    struct wasi_unstable_filestat *wst);
uint8_t wasi_convert_dirent_filetype(uint8_t hosttype);
int wasi_convert_clockid(uint32_t clockid, clockid_t *hostidp);
uint32_t wasi_convert_errno(int host_errno);
