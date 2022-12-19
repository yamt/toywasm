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

#if !defined(__must_check)
#if defined(__GNUC__)
#define __must_check __attribute__((warn_unused_result))
#else
#define __must_check
#endif
#endif

#if !defined(__malloc_like)
#if defined(__GNUC__)
#define __malloc_like __attribute__((malloc))
#else
#define __malloc_like
#endif
#endif

#if !defined(__alloc_size)
#if defined(__GNUC__)
#define __alloc_size(a) __attribute__((alloc_size(a)))
#else
#define __alloc_size(a)
#endif
#endif

#if !defined(__alloc_size2)
#if defined(__GNUC__)
#define __alloc_size2(a, b) __attribute__((alloc_size(a, b)))
#else
#define __alloc_size2(a, b)
#endif
#endif
