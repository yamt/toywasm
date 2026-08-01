#if !defined(_TOYWASM_GETPTR_I_H)
#define _TOYWASM_GETPTR_I_H

#include "platform.h"
#include "mem.h"
#include "xlog.h"

#if defined(TOYWASM_GETPTR_INLINE)
#define TOYWASM_INLINE __always_inline static inline
#else
#define TOYWASM_INLINE
#endif

TOYWASM_INLINE int
memory_instance_getptr2(struct meminst *meminst, uint32_t ptr, uint32_t offset,
                        uint32_t size, void **pp, bool *movedp)
{
        assert(meminst->allocated <=
               (uint64_t)meminst->size_in_pages
                       << memtype_page_shift(meminst->type));
        uint32_t ea;
        if (ADD_U32_OVERFLOW(ptr, offset, &ea)) {
                /*
                 * i failed to find this in the spec.
                 * but some of spec tests seem to test this.
                 */
                goto do_trap;
        }
        if (__predict_false(size == 0)) {
                /*
                 * a zero-length access still needs address check.
                 * this can be either from host functions or
                 * bulk instructions like memory.copy.
                 */
                const uint32_t page_shift = memtype_page_shift(meminst->type);
                if (ea > 0 &&
                    (ea - 1) >> page_shift >= meminst->size_in_pages) {
                        goto do_trap;
                }
                goto success;
        }
        uint32_t last_byte;
        if (ADD_U32_OVERFLOW(ea, size - 1, &last_byte)) {
                goto do_trap;
        }
        if (__predict_false(last_byte >= meminst->allocated)) {
                const uint32_t page_shift = memtype_page_shift(meminst->type);
                uint32_t need_in_pages = (last_byte >> page_shift) + 1;
                if (need_in_pages > meminst->size_in_pages) {
do_trap:
                        return ETOYWASMTRAP;
                }
                /*
                 * Note: shared memories do never come here because
                 * we handle their growth in memory_grow.
                 */
                assert((meminst->type->flags & MEMTYPE_FLAG_SHARED) == 0);
#if SIZE_MAX <= UINT32_MAX
                if (last_byte >= SIZE_MAX) {
                        goto do_trap;
                }
#endif
                size_t need = (size_t)last_byte + 1;
                assert(need > meminst->allocated);
                void *np = mem_extend(meminst->mctx, meminst->data,
                                      meminst->allocated, need);
                if (np == NULL) {
                        return ENOMEM;
                }
                meminst->data = np;
                xlog_trace_insn("extend memory from %zu to %zu",
                                meminst->allocated, need);
                if (movedp != NULL) {
                        *movedp = true;
                }
                memset(meminst->data + meminst->allocated, 0,
                       need - meminst->allocated);
                meminst->allocated = need;
        }
success:
        xlog_trace_insn("memory access: at %08" PRIx32 " + %08" PRIx32
                        ", size %" PRIu32 ", meminst size %" PRIu32,
                        ptr, offset, size, meminst->size_in_pages);
        *pp = meminst->data + ea;
        return 0;
}

TOYWASM_INLINE int
memory_getptr2(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
               uint32_t offset, uint32_t size, void **pp, bool *movedp)
{
        const struct instance *inst = ctx->instance;
        assert(memidx < inst->module->nmems + inst->module->nimportedmems);
        struct meminst *meminst = VEC_ELEM(inst->mems, memidx);
        int ret = memory_instance_getptr2(meminst, ptr, offset, size, pp,
                                          movedp);

        if (ret == ETOYWASMTRAP) {
                ret = trap_with_id(
                        ctx, TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
                        "invalid memory access at %04" PRIx32 " %08" PRIx32
                        " + %08" PRIx32 ", size %" PRIu32
                        ", meminst size %" PRIu32 ", pagesize %" PRIu32,
                        memidx, ptr, offset, size, meminst->size_in_pages,
                        memtype_page_size(meminst->type));
                assert(ret != 0);
        }
        return ret;
}

TOYWASM_INLINE int
memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
              uint32_t offset, uint32_t size, void **pp)
{
        return memory_getptr2(ctx, memidx, ptr, offset, size, pp, NULL);
}

#undef TOYWASM_INLINE
#endif /* !defined(_TOYWASM_GETPTR_I_H) */
