#include <poll.h>

struct exec_context;
struct wasi_fdinfo;

int wasi_poll(struct exec_context *ctx, struct pollfd *fds, nfds_t nfds,
              int timeout_ms, int *retp, int *neventsp);
int wait_fd_ready(struct exec_context *ctx, int hostfd, short event,
                  int *retp);
bool emulate_blocking(struct exec_context *ctx, struct wasi_fdinfo *fdinfo,
                      short poll_event, int orig_ret, int *host_retp,
                      int *retp);
