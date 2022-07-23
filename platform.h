#if defined(__clang__)
#define __musttail __attribute__((musttail))
#else
#define __musttail
#endif
