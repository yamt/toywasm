#if defined(__clang__) && defined(__has_attribute) && __has_attribute(musttail)
#define __musttail __attribute__((musttail))
#else
#define __musttail
#endif

#if !defined(__predict_true)
#if defined(__GNUC__)
#define __predict_true(x) __builtin_expect(x, 1)
#define __predict_false(x) __builtin_expect(x, 0)
#else
#define __predict_true(x) (x)
#define __predict_false(x) (!(x))
#endif
#endif /* !defined(__predict_true) */
