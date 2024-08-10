#if !defined(__has_builtin)
#define __has_builtin(a) 0
#endif

#if !defined(__has_attribute)
#define __has_attribute(a) 0
#endif

#if !defined(__has_extension)
#define __has_extension(a) 0
#endif

#if defined(__clang__) && __has_attribute(musttail)
#define __musttail __attribute__((musttail))
#define __HAVE_MUSTTAIL
#else
#define __musttail
#undef __HAVE_MUSTTAIL
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

/* compile-time assertion */
#if !defined(ctassert)
#if __STDC_VERSION__ >= 201112L || __has_extension(c_static_assert)
#define ctassert(e) _Static_assert(e, #e)
#else
#define ctassert(e)
#endif
#endif

/*
 * https://gcc.gnu.org/onlinedocs/gcc/Typeof.html
 * clang also has it.
 *
 * Note: we don't want to rely on typeof.
 * any use of it in toywasm should have a fallback implementation.
 */
#if !defined(toywasm_typeof)
#if defined(__GNUC__)
#define toywasm_typeof(a) __typeof__(a)
#else
#undef toywasm_typeof
#endif
#endif

/*
 * https://gcc.gnu.org/onlinedocs/gcc/Offsetof.html
 * clang also has it.
 *
 */
#if !defined(ctassert_offset)
#if defined(__GNUC__)
#define toywasm_offsetof(a, b) __builtin_offsetof(a, b)
#define ctassert_offset(a, b, c) ctassert(toywasm_offsetof(a, b) == c)
#else
/* note: this implementation is not an integral constant. */
#define toywasm_offsetof(a, b) ((size_t)(&((a *)0)->b))
#define ctassert_offset(a, b, c)
#endif
#endif

#if !defined(xassert)
#if defined(__clang__)
#define xassert(e)                                                            \
        do {                                                                  \
                assert(e);                                                    \
                __builtin_assume(e);                                          \
        } while (0)
#else
#define xassert(e) assert(e)
#endif
#endif /* !defined(xassert) */

#if !defined(__purefunc)
#if defined(__GNUC__)
#define __purefunc __attribute__((pure))
#else
#define __purefunc
#endif /* defined(__GNUC__) */
#endif /* !defined(__purefunc) */

#if !defined(__constfunc)
#if defined(__GNUC__)
#define __constfunc __attribute__((const))
#else
#define __constfunc
#endif /* defined(__GNUC__) */
#endif /* !defined(__constfunc) */

#if !defined(__aligned)
#if defined(__GNUC__)
#define __aligned(N) __attribute__((aligned(N)))
#else
#define __aligned(N)
#endif /* defined(__GNUC__) */
#endif /* !defined(__aligned) */

#if !defined(__noinline)
#if __has_attribute(noinline)
#define __noinline __attribute__((noinline))
#else
#define __noinline
#endif
#endif /* !defined(__noinline) */

#if !defined(__printflike)
#if __has_attribute(__format__)
#define __printflike(a, b) __attribute__((__format__(__printf__, a, b)))
#else
#define __printflike(a, b)
#endif
#endif /* !defined(__printflike) */

#if __has_builtin(__builtin_add_overflow)
#define ADD_U32_OVERFLOW(a, b, c) __builtin_add_overflow(a, b, c)
#else
#define ADD_U32_OVERFLOW(a, b, c) ((UINT32_MAX - a < b) ? 1 : (*c = a + b, 0))
#endif

#if __has_builtin(__builtin_mul_overflow)
#define MUL_SIZE_OVERFLOW(a, b, c) __builtin_mul_overflow(a, b, c)
#else
/*
 * Note: (floor(x) < b) == (x < b) where
 * x is a real number and b is an integer.
 */
#define MUL_SIZE_OVERFLOW(a, b, c)                                            \
        (a != 0 && (SIZE_MAX / a < b) ? 1 : (*c = a * b, 0))
#endif

#if defined(_MSC_VER)
#define _Atomic
#endif

#if !defined(__BEGIN_EXTERN_C)
#if defined(__cplusplus)
#define __BEGIN_EXTERN_C extern "C" {
#define __END_EXTERN_C }
#else
#define __BEGIN_EXTERN_C
#define __END_EXTERN_C
#endif
#endif
