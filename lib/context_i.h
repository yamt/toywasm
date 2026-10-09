#include <assert.h>

#include "module.h"

#if defined(TOYWASM_PTR2PC_INLINE)
#define TOYWASM_INLINE static inline
#else
#define TOYWASM_INLINE
#endif

TOYWASM_INLINE uint32_t
ptr2pc(const struct module *m, const uint8_t *p)
{
        assert(p >= m->bin);
        assert(p - m->bin <= UINT32_MAX);
        return (uint32_t)(p - m->bin);
}

#undef TOYWASM_INLINE
