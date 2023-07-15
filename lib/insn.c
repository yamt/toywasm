#define _NETBSD_SOURCE /* old NetBSD math.h bug workaround */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "context.h"
#include "decode.h"
#include "endian.h"
#include "exec.h"
#include "expr.h"
#include "insn.h"
#include "insn_macros.h"
#include "insn_op.h"
#include "insn_op_helpers.h"
#include "leb128.h"
#include "platform.h"
#include "type.h"
#include "util.h"
#include "validation.h"
#include "xlog.h"

/*
 * https://webassembly.github.io/spec/core/binary/instructions.html
 * https://webassembly.github.io/spec/core/appendix/index-instructions.html
 * https://webassembly.github.io/spec/core/appendix/algorithm.html
 */

#if defined(TOYWASM_USE_SEPARATE_EXECUTE) && defined(TOYWASM_USE_SMALL_CELLS)
static void
stack_push_val(const struct exec_context *ctx, const struct val *val,
               struct cell **stackp, uint32_t csz)
{
        assert(ctx->stack.p <= *stackp);
        assert(*stackp + csz <= ctx->stack.p + ctx->stack.psize);
        switch (csz) {
        case 1:
                xlog_trace_insn("stack push %08" PRIx32, val->u.i32);
                break;
        case 2:
                xlog_trace_insn("stack push %016" PRIx64, val->u.i64);
                break;
        case 4:
                /* Note: val->u.v128 is in little-endian */
                xlog_trace_insn("stack push %016" PRIx64 " %016" PRIx64,
                                le64_to_host(val->u.v128.i64[1]),
                                le64_to_host(val->u.v128.i64[0]));
                break;
        default:
                assert(false);
        }
        val_to_cells(val, *stackp, csz);
        *stackp += csz;
}

static void
stack_pop_val(const struct exec_context *ctx, struct val *val,
              struct cell **stackp, uint32_t csz)
{
        assert(ctx->stack.p + csz <= *stackp);
        assert(*stackp <= ctx->stack.p + ctx->stack.psize);
        *stackp -= csz;
        val_from_cells(val, *stackp, csz);
        switch (csz) {
        case 1:
                xlog_trace_insn("stack pop  %08" PRIx32, val->u.i32);
                break;
        case 2:
                xlog_trace_insn("stack pop  %016" PRIx64, val->u.i64);
                break;
        case 4:
                /* Note: val->u.v128 is in little-endian */
                xlog_trace_insn("stack pop  %016" PRIx64 " %016" PRIx64,
                                le64_to_host(val->u.v128.i64[1]),
                                le64_to_host(val->u.v128.i64[0]));
                break;
        default:
                assert(false);
        }
}
#endif

static void
push_val(const struct val *val, uint32_t csz, struct exec_context *ctx)
{
        val_to_cells(val, &VEC_NEXTELEM(ctx->stack), csz);
        ctx->stack.lsize += csz;
        xlog_trace_insn("stack push %016" PRIx64, val->u.i64);
}

static void
pop_val(struct val *val, uint32_t csz, struct exec_context *ctx)
{
        assert(ctx->stack.lsize >= csz);
        ctx->stack.lsize -= csz;
        val_from_cells(val, &VEC_NEXTELEM(ctx->stack), csz);
        xlog_trace_insn("stack pop  %016" PRIx64, val->u.i64);
}

static void
push_label(const uint8_t *p, struct cell *stack, struct exec_context *ctx)
{
        /*
         * "- 1" for the first byte of the opcode parsed by
         * eg. fetch_exec_next_insn.
         *
         * Note: Currently we don't have any control opcodes with
         * sub opcodes.
         */
        uint32_t pc = ptr2pc(ctx->instance->module, p - 1);
        struct label *l = VEC_PUSH(ctx->labels);
        l->pc = pc;
        l->height = stack - ctx->stack.p;
}

static struct cell *
local_getptr(struct exec_context *ectx, uint32_t localidx, uint32_t *cszp)
{
        uint32_t cidx = frame_locals_cellidx(ectx, localidx, cszp);
#if defined(TOYWASM_USE_LOCALS_CACHE)
        return &ectx->current_locals[cidx];
#else
        const struct funcframe *frame = &VEC_LASTELEM(ectx->frames);
        return &frame_locals(ectx, frame)[cidx];
#endif
}

static void
local_get(struct exec_context *ctx, uint32_t localidx, struct cell *stack,
          uint32_t *cszp)
{
        assert(ctx->stack.p <= stack);
        assert(stack < ctx->stack.p + ctx->stack.psize);
        const struct cell *cells;
        cells = local_getptr(ctx, localidx, cszp);
        cells_copy(stack, cells, *cszp);
}

static void
local_set(struct exec_context *ctx, uint32_t localidx,
          const struct cell *stack, uint32_t *cszp)
{
        assert(ctx->stack.p < stack);
        assert(stack <= ctx->stack.p + ctx->stack.psize);
        struct cell *cells;
        cells = local_getptr(ctx, localidx, cszp);
        cells_copy(cells, stack - *cszp, *cszp);
}

/*
 * https://webassembly.github.io/spec/core/exec/instructions.html#exec-call-indirect
 */
static int
get_func_indirect(struct exec_context *ectx, uint32_t tableidx,
                  uint32_t typeidx, uint32_t i, const struct funcinst **fip)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        const struct functype *ft = &m->types[typeidx];
        const struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        return table_get_func(ectx, t, i, ft, fip);
}

/*
 * WASM float min/max follow IEEE 754-2019 minimum/maximum
 * semantics:
 *
 * - If either operand is a NaN (Note: can be an sNaN), returns a qNaN.
 * - -0.0 < +0.0
 */

static float
wasm_fminf(float a, float b)
{
        if (isnan(a) || isnan(b)) {
                return NAN;
        }
        if (a == b) {
                return signbit(a) ? a : b;
        }
        return fminf(a, b);
}

static float
wasm_fmaxf(float a, float b)
{
        if (isnan(a) || isnan(b)) {
                return NAN;
        }
        if (a == b) {
                return signbit(a) ? b : a;
        }
        return fmaxf(a, b);
}

static double
wasm_fmin(double a, double b)
{
        if (isnan(a) || isnan(b)) {
                return NAN;
        }
        if (a == b) {
                return signbit(a) ? a : b;
        }
        return fmin(a, b);
}

static double
wasm_fmax(double a, double b)
{
        if (isnan(a) || isnan(b)) {
                return NAN;
        }
        if (a == b) {
                return signbit(a) ? b : a;
        }
        return fmax(a, b);
}

static uint32_t
clz(uint32_t v)
{
        if (v == 0) {
                return 32;
        }
        return __builtin_clz(v);
}

static uint32_t
ctz(uint32_t v)
{
        if (v == 0) {
                return 32;
        }
        return __builtin_ctz(v);
}

static uint32_t
wasm_popcount(uint32_t v)
{
        return __builtin_popcount(v);
}

static uint64_t
clz64(uint64_t v)
{
        if (v == 0) {
                return 64;
        }
        uint32_t high = v >> 32;
        if (high == 0) {
                return 32 + __builtin_clz(v);
        }
        return __builtin_clz(high);
}

static uint64_t
ctz64(uint64_t v)
{
        if (v == 0) {
                return 64;
        }
        uint32_t low = (uint32_t)v;
        if (low == 0) {
                return 32 + __builtin_ctz((uint32_t)(v >> 32));
        }
        return __builtin_ctz(low);
}

static uint64_t
wasm_popcount64(uint64_t v)
{
        return wasm_popcount((uint32_t)v) + wasm_popcount((uint32_t)(v >> 32));
}

static int
get_functype(struct module *m, uint32_t typeidx, struct functype **ftp)
{
        if (typeidx >= m->ntypes) {
                return EINVAL;
        }
        *ftp = &m->types[typeidx];
        return 0;
}

static const struct resulttype rt_empty = {
        .ntypes = 0, .is_static = true, CELLIDX_NONE};

#define BYTE_AS_S33(b) ((int)(signed char)((b) + 0x80))

int
get_functype_for_blocktype(struct module *m, int64_t blocktype,
                           struct resulttype **parameter,
                           struct resulttype **result)
{
        int ret;

        if (blocktype < 0) {
                uint8_t u8 = (uint8_t)(blocktype & 0x7f);

                if (BYTE_AS_S33(u8) != blocktype) {
                        return EINVAL;
                }
                if (u8 == 0x40) {
                        *parameter = (void *)&rt_empty; /* unconst */
                        *result = (void *)&rt_empty;    /* unconst */
                        return 0;
                }
                if (is_valtype(u8)) {
                        struct resulttype *rt;
                        enum valtype t = u8;

                        ret = resulttype_alloc(1, &t, &rt);
                        if (ret != 0) {
                                return ret;
                        }
                        *parameter = (void *)&rt_empty; /* unconst */
                        *result = rt;
                        return 0;
                }
                return EINVAL;
        }
        assert(blocktype <= UINT32_MAX);
        struct functype *ft;
        ret = get_functype(m, (uint32_t)blocktype, &ft);
        if (ret != 0) {
                return ret;
        }
        *parameter = &ft->parameter;
        *result = &ft->result;
        return 0;
}

struct memarg {
        uint32_t offset;
        uint32_t align;
        uint32_t memidx;
};

static int
read_memarg(const uint8_t **pp, const uint8_t *ep, struct memarg *arg)
{
        const uint8_t *p = *pp;
        uint32_t offset;
        uint32_t align;
        uint32_t memidx = 0;
        int ret;

        ret = read_leb_u32(&p, ep, &align);
        if (ret != 0) {
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASM_MULTI_MEMORY)
        /* if bit 6 is set, memidx follows. otherwise memidx is 0. */
        if ((align & (1 << 6)) != 0) {
                align &= ~(1 << 6);
                ret = read_leb_u32(&p, ep, &memidx);
                if (ret != 0) {
                        goto fail;
                }
        }
#endif
        ret = read_leb_u32(&p, ep, &offset);
        if (ret != 0) {
                goto fail;
        }
        arg->offset = offset;
        arg->memidx = memidx;
        arg->align = align;
        *pp = p;
        ret = 0;
fail:
        return ret;
}

static void
read_memarg_nocheck(const uint8_t **pp, struct memarg *arg)
{
        uint32_t align;
        uint32_t offset;
        uint32_t memidx = 0;

        align = read_leb_u32_nocheck(pp);
#if defined(TOYWASM_ENABLE_WASM_MULTI_MEMORY)
        /* if bit 6 is set, memidx follows. otherwise memidx is 0. */
        if ((align & (1 << 6)) != 0) {
                align &= ~(1 << 6);
                memidx = read_leb_u32_nocheck(pp);
        }
#endif
        offset = read_leb_u32_nocheck(pp);
        arg->offset = offset;
        arg->memidx = memidx;
        arg->align = align;
}

/*
 * LOAD_PC: prepare PC on the entry of the function.
 *
 * SAVE_PC: use this when you advanced PC by parsing immediates.
 *
 * RELOAD_PC: sync the local copy of PC from ETX.
 *
 * SAVE_STACK_PTR/LOAD_STACK_PTR: sync the local copy of stack pointer
 * to/from ECTX.
 *
 * ORIG_PC: the PC value at the point of LOAD_PC on the entry of the function.
 *
 * INSN_SUCCESS: successfully finish the execution of the opcode. Possibly
 * with a tail-call to the function for the next opcode.
 *
 * INSN_SUCCESS_RETURN: same as INSN_SUCCESS, but ensure returning to
 * the main loop for extra processing of events.
 */

#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
#define EXECUTING false
#define ECTX ((struct exec_context *)NULL)
#else
#define EXECUTING (ctx->exec != NULL)
#define ECTX (ctx->exec)
#endif
#define VALIDATING (ctx->validation != NULL)
#define VCTX (ctx->validation)
#define INSN_IMPL(NAME)                                                       \
        int process_##NAME(const uint8_t **pp, const uint8_t *ep,             \
                           struct context *ctx)
#define LOAD_PC const uint8_t *p __attribute__((__unused__)) = *pp
#define SAVE_PC *pp = p
#define RELOAD_PC
#define SAVE_STACK_PTR
#define LOAD_STACK_PTR
#define ORIG_PC (*pp)
#define INSN_SUCCESS return 0
#define INSN_SUCCESS_RETURN INSN_SUCCESS
#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
#define PREPARE_FOR_POSSIBLE_RESTART
#define INSN_FAIL_RESTARTABLE(NAME) INSN_FAIL
#else
#define PREPARE_FOR_POSSIBLE_RESTART                                          \
        uint32_t saved_stack_ptr;                                             \
        if (EXECUTING) {                                                      \
                saved_stack_ptr = ECTX->stack.lsize;                          \
        }
#define INSN_FAIL_RESTARTABLE(NAME)                                           \
        assert(ret != 0);                                                     \
        if (EXECUTING) {                                                      \
                if (IS_RESTARTABLE(ret)) {                                    \
                        struct exec_context *ectx = ECTX;                     \
                        ectx->stack.lsize = saved_stack_ptr;                  \
                        ectx->event = EXEC_EVENT_RESTART_INSN;                \
                        ectx->event_u.restart_insn.process = process_##NAME;  \
                }                                                             \
        }                                                                     \
        return ret
#endif
#define INSN_FAIL                                                             \
        assert(ret != 0);                                                     \
        assert(!IS_RESTARTABLE(ret));                                         \
        return ret
#define STACK &VEC_NEXTELEM(ECTX->stack)
#define STACK_ADJ(n) ECTX->stack.lsize += (n)

#include "insn_impl.h"

#undef EXECUTING
#undef ECTX
#undef VALIDATING
#undef VCTX
#undef INSN_IMPL
#undef LOAD_PC
#undef SAVE_PC
#undef RELOAD_PC
#undef SAVE_STACK_PTR
#undef LOAD_STACK_PTR
#undef ORIG_PC
#undef PREPARE_FOR_POSSIBLE_RESTART
#undef INSN_SUCCESS
#undef INSN_SUCCESS_RETURN
#undef INSN_FAIL
#undef INSN_FAIL_RESTARTABLE
#undef STACK
#undef STACK_ADJ

#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
#define EXECUTING true
#define ECTX ctx
#define VALIDATING false
#define VCTX ((struct validation_context *)NULL)
#define INSN_IMPL(NAME)                                                       \
        int fetch_exec_##NAME(const uint8_t *p, struct cell *stack,           \
                              struct exec_context *ctx)
#define LOAD_PC const uint8_t *p0 __attribute__((__unused__)) = p
#define SAVE_PC
#define RELOAD_PC p = ctx->p
#define SAVE_STACK_PTR ctx->stack.lsize = stack - ctx->stack.p
#define LOAD_STACK_PTR stack = &VEC_NEXTELEM(ctx->stack)
#define ORIG_PC p0
#if defined(TOYWASM_USE_TAILCALL) &&                                          \
        (defined(__HAVE_MUSTTAIL) || defined(TOYWASM_FORCE_USE_TAILCALL))
#define INSN_SUCCESS __musttail return fetch_exec_next_insn(p, stack, ctx)
#else
#define INSN_SUCCESS INSN_SUCCESS_RETURN
#endif
#define INSN_SUCCESS_RETURN                                                   \
        SAVE_STACK_PTR;                                                       \
        ctx->p = p;                                                           \
        return 0
#define PREPARE_FOR_POSSIBLE_RESTART struct cell *saved_stack_ptr = stack
#define INSN_FAIL_RESTARTABLE(NAME)                                           \
        assert(ret != 0);                                                     \
        if (IS_RESTARTABLE(ret)) {                                            \
                ctx->p = ORIG_PC;                                             \
                stack = saved_stack_ptr;                                      \
                SAVE_STACK_PTR;                                               \
                ctx->event = EXEC_EVENT_RESTART_INSN;                         \
                ctx->event_u.restart_insn.fetch_exec = fetch_exec_##NAME;     \
        }                                                                     \
        return ret
#define INSN_FAIL                                                             \
        assert(ret != 0);                                                     \
        assert(!IS_RESTARTABLE(ret));                                         \
        return ret
#define ep NULL
#define STACK stack
#define STACK_ADJ(n) stack += (n)
#if defined(TOYWASM_USE_SMALL_CELLS)
#define push_val(v, csz, ctx) stack_push_val(ctx, v, &stack, csz)
#define pop_val(v, csz, ctx) stack_pop_val(ctx, v, &stack, csz)
#else
#define push_val(v, csz, ctx)                                                 \
        do {                                                                  \
                assert(csz == 1);                                             \
                *stack++ = (v)->u.cells[0];                                   \
        } while (0)
#define pop_val(v, csz, ctx)                                                  \
        do {                                                                  \
                assert(csz == 1);                                             \
                (v)->u.cells[0] = *(--stack);                                 \
        } while (0)
#endif

#include "insn_impl.h"

#undef EXECUTING
#undef ECTX
#undef VALIDATING
#undef VCTX
#undef INSN_IMPL
#undef LOAD_PC
#undef SAVE_PC
#undef RELOAD_PC
#undef SAVE_STACK_PTR
#undef LOAD_STACK_PTR
#undef ORIG_PC
#undef PREPARE_FOR_POSSIBLE_RESTART
#undef INSN_SUCCESS
#undef INSN_SUCCESS_RETURN
#undef INSN_FAIL
#undef INSN_FAIL_RESTARTABLE
#undef ep
#undef STACK
#undef STACK_ADJ
#endif /* defined(TOYWASM_USE_SEPARATE_EXECUTE) */

#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
typedef int (*exec_func_t)(const uint8_t *, struct cell *,
                           struct exec_context *);

#if defined(TOYWASM_ENABLE_TRACING_INSN)
static const char *
instruction_name(const struct exec_instruction_desc *exec_table, uint32_t op);
#endif /* defined(TOYWASM_ENABLE_TRACING_INSN) */

static int fetch_exec_next_insn_fc(const uint8_t *p, struct cell *stack,
                                   struct exec_context *ctx);
#if defined(TOYWASM_ENABLE_WASM_SIMD)
static int fetch_exec_next_insn_fd(const uint8_t *p, struct cell *stack,
                                   struct exec_context *ctx);
#endif
#if defined(TOYWASM_ENABLE_WASM_THREADS)
static int fetch_exec_next_insn_fe(const uint8_t *p, struct cell *stack,
                                   struct exec_context *ctx);
#endif

#define INSTRUCTION(b, n, f, FLAGS)                                           \
        [b] = {                                                               \
                .fetch_exec = fetch_exec_##f,                                 \
        },

#define INSTRUCTION_INDIRECT(b, n)                                            \
        [b] = {                                                               \
                .fetch_exec = fetch_exec_next_insn_##n,                       \
        },

#define __exec_table_align __aligned(64)

const static struct exec_instruction_desc
        exec_instructions_fc[] __exec_table_align = {
#include "insn_list_fc.h"
};

#if defined(TOYWASM_ENABLE_WASM_SIMD)
const static struct exec_instruction_desc
        exec_instructions_fd[] __exec_table_align = {
#include "insn_list_simd.h"
};
#endif

#if defined(TOYWASM_ENABLE_WASM_THREADS)
const static struct exec_instruction_desc
        exec_instructions_fe[] __exec_table_align = {
#include "insn_list_threads.h"
};
#endif

const struct exec_instruction_desc exec_instructions[] __exec_table_align = {
#include "insn_list_base.h"
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
#include "insn_list_tailcall.h"
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
};

#undef INSTRUCTION
#undef INSTRUCTION_INDIRECT

static exec_func_t
fetch_multibyte_opcode(const uint8_t **pp, struct exec_context *ctx,
                       const struct exec_instruction_desc *table)
{
#if !(defined(TOYWASM_USE_SEPARATE_EXECUTE) && defined(TOYWASM_USE_TAILCALL))
        assert(ctx->p + 1 == *pp);
#endif
        assert(ctx->event == EXEC_EVENT_NONE);
        assert(ctx->frames.lsize > 0);
#if defined(TOYWASM_ENABLE_TRACING_INSN)
        uint32_t pc = ptr2pc(ctx->instance->module, *pp);
#endif
        uint32_t op = read_leb_u32_nocheck(pp);
        const struct exec_instruction_desc *desc = &table[op];
        xlog_trace_insn("exec %06" PRIx32 ": %s (2nd byte %02" PRIx32 ")", pc,
                        instruction_name(table, op), op);
        return desc->fetch_exec;
}

static int
fetch_exec_next_insn_fc(const uint8_t *p, struct cell *stack,
                        struct exec_context *ctx)
{
#if defined(TOYWASM_USE_TAILCALL)
        __musttail
#endif
                return fetch_multibyte_opcode(&p, ctx, exec_instructions_fc)(
                        p, stack, ctx);
}

#if defined(TOYWASM_ENABLE_WASM_SIMD)
static int
fetch_exec_next_insn_fd(const uint8_t *p, struct cell *stack,
                        struct exec_context *ctx)
{
#if defined(TOYWASM_USE_TAILCALL)
        __musttail
#endif
                return fetch_multibyte_opcode(&p, ctx, exec_instructions_fd)(
                        p, stack, ctx);
}
#endif /* defined(TOYWASM_ENABLE_WASM_SIMD) */

#if defined(TOYWASM_ENABLE_WASM_THREADS)
/*
 * XXX duplicate of fetch_exec_next_insn_fc.
 * it isn't obvious to me how i can clean this up preserving tailcall.
 * while a macro can do, i'm not sure if i like it.
 */
static int
fetch_exec_next_insn_fe(const uint8_t *p, struct cell *stack,
                        struct exec_context *ctx)
{
#if defined(TOYWASM_USE_TAILCALL)
        __musttail
#endif
                return fetch_multibyte_opcode(&p, ctx, exec_instructions_fe)(
                        p, stack, ctx);
}
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */

#endif /* defined(TOYWASM_USE_SEPARATE_EXECUTE) */

#define INSTRUCTION(b, n, f, FLAGS)                                           \
        [b] = {                                                               \
                .name = n,                                                    \
                .process = process_##f,                                       \
                .flags = FLAGS,                                               \
                .next_table = NULL,                                           \
        },

#define INSTRUCTION_INDIRECT(b, n)                                            \
        [b] = {                                                               \
                .name = #n,                                                   \
                .next_table = instructions_##n,                               \
                .next_table_size = ARRAYCOUNT(instructions_##n),              \
        },

const static struct instruction_desc instructions_fc[] = {
#include "insn_list_fc.h"
};

#if defined(TOYWASM_ENABLE_WASM_SIMD)
const static struct instruction_desc instructions_fd[] = {
#include "insn_list_simd.h"
};
#endif

#if defined(TOYWASM_ENABLE_WASM_THREADS)
const static struct instruction_desc instructions_fe[] = {
#include "insn_list_threads.h"
};
#endif

const struct instruction_desc instructions[] = {
#include "insn_list_base.h"
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
#include "insn_list_tailcall.h"
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
};

const size_t instructions_size = ARRAYCOUNT(instructions);

#if defined(TOYWASM_USE_SEPARATE_EXECUTE) &&                                  \
        defined(TOYWASM_ENABLE_TRACING_INSN)
static const char *
instruction_name(const struct exec_instruction_desc *exec_table, uint32_t op)
{
        const struct instruction_desc *table;
        if (exec_table == exec_instructions) {
                table = instructions;
        } else if (exec_table == exec_instructions_fc) {
                table = instructions_fc;
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        } else if (exec_table == exec_instructions_fd) {
                table = instructions_fd;
#endif /* defined(TOYWASM_ENABLE_WASM_SIMD) */
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        } else if (exec_table == exec_instructions_fe) {
                table = instructions_fe;
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
        } else {
                return "unknown";
        }
        return table[op].name;
}
#endif /* defined(TOYWASM_USE_SEPARATE_EXECUTE) &&                            \
          defined(TOYWASM_ENABLE_TRACING_INSN) */
