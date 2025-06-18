#include <stdint.h>

struct wasi_fdinfo;
struct iovec;

int wasi_host_sock_fdinfo_alloc(struct wasi_fdinfo *fdinfo,
                                struct wasi_fdinfo **fdinfop);
int wasi_host_sock_accept(struct wasi_fdinfo *fdinfo, uint16_t fdflags,
                          struct wasi_fdinfo *fdinfo2);
int wasi_host_sock_recv(struct wasi_fdinfo *fdinfo, struct iovec *iov,
                        int iovcnt, uint16_t riflags, uint16_t *roflagsp,
                        size_t *result);
int wasi_host_sock_send(struct wasi_fdinfo *fdinfo, struct iovec *iov,
                        int iovcnt, uint16_t siflags, size_t *result);
int wasi_host_sock_shutdown(struct wasi_fdinfo *fdinfo, uint16_t sdflags);
