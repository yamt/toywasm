#if defined(__clang__) && defined(__has_attribute) && __has_attribute(musttail)
#define __musttail __attribute__((musttail))
#else
#define __musttail
#endif
