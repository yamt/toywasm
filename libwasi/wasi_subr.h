#include <stdint.h>

struct wasi_fdinfo;
struct wasi_filestet;
struct wasi_unstable_filestat;
struct exec_context;
struct iovec;
struct meminst;

int wasi_unstable_convert_filestat(const struct wasi_filestat *wst,
                                   struct wasi_unstable_filestat *uwst);
int wasi_userfd_reject_directory(struct wasi_fdinfo *fdinfo);
int wasi_copyin_iovec(struct exec_context *ctx, struct meminst *mem,
                      uint32_t iov_uaddr, uint32_t iov_count,
                      struct iovec **resultp, int *usererrorp);
int wasi_validate_fstflags(uint32_t fstflags);
