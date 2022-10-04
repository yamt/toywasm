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
#include "xlog.h"

/*
 * https://webassembly.github.io/spec/core/binary/instructions.html
 * https://webassembly.github.io/spec/core/appendix/index-instructions.html
 * https://webassembly.github.io/spec/core/appendix/algorithm.html
 */

#if defined(USE_SEPARATE_EXECUTE) && defined(USE_SMALL_CELLS)
static void
stack_push_val(const struct exec_context *ctx, const struct val *val,
               struct cell **stackp, uint32_t csz)
{
        assert(ctx->stack.p <= *stackp);
        assert(*stackp + csz <= ctx->stack.p + ctx->stack.psize);
        xlog_trace("stack push %016" PRIx64, val->u.i64);
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
        xlog_trace("stack pop  %016" PRIx64, val->u.i64);
}
#endif

static void
push_val(const struct val *val, uint32_t csz, struct exec_context *ctx)
{
        val_to_cells(val, &VEC_NEXTELEM(ctx->stack), csz);
        ctx->stack.lsize += csz;
        xlog_trace("stack push %016" PRIx64, val->u.i64);
}

static void
pop_val(struct val *val, uint32_t csz, struct exec_context *ctx)
{
        assert(ctx->stack.lsize >= csz);
        ctx->stack.lsize -= csz;
        val_from_cells(val, &VEC_NEXTELEM(ctx->stack), csz);
        xlog_trace("stack pop  %016" PRIx64, val->u.i64);
}

static void
push_label(const uint8_t *p, struct cell *stack, struct exec_context *ctx)
{
        uint32_t pc = ptr2pc(ctx->instance->module, p - 1);
        struct label *l = VEC_PUSH(ctx->labels);
        l->pc = pc;
        l->height = stack - ctx->stack.p;
}

static struct cell *
local_getptr(struct exec_context *ectx, uint32_t localidx, uint32_t *cszp)
{
        uint32_t cidx = frame_locals_cellidx(ectx, localidx, cszp);
#if defined(USE_LOCALS_CACHE)
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

static float
wasm_fminf(float a, float b)
{
        if (isnan(a)) {
                return a;
        }
        if (isnan(b)) {
                return b;
        }
        if (a == b) {
                return signbit(a) ? a : b;
        }
        return fminf(a, b);
}

static float
wasm_fmaxf(float a, float b)
{
        if (isnan(a)) {
                return a;
        }
        if (isnan(b)) {
                return b;
        }
        if (a == b) {
                return signbit(a) ? b : a;
        }
        return fmaxf(a, b);
}

static double
wasm_fmin(double a, double b)
{
        if (isnan(a)) {
                return a;
        }
        if (isnan(b)) {
                return b;
        }
        if (a == b) {
                return signbit(a) ? a : b;
        }
        return fmin(a, b);
}

static double
wasm_fmax(double a, double b)
{
        if (isnan(a)) {
                return a;
        }
        if (isnan(b)) {
                return b;
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
popcount(uint32_t v)
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
        uint32_t low = v;
        if (low == 0) {
                return 32 + __builtin_ctz(v >> 32);
        }
        return __builtin_ctz(low);
}

static uint64_t
popcount64(uint64_t v)
{
        return popcount(v) + popcount(v >> 32);
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
        .ntypes = 0,
        .is_static = true,
};

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
};

static int
read_memarg(const uint8_t **pp, const uint8_t *ep, struct memarg *arg)
{
        const uint8_t *p = *pp;
        uint32_t offset;
        uint32_t align;
        int ret;

        ret = read_leb_u32(&p, ep, &align);
        if (ret != 0) {
                goto fail;
        }
        ret = read_leb_u32(&p, ep, &offset);
        if (ret != 0) {
                goto fail;
        }
        arg->offset = offset;
        arg->align = align;
        *pp = p;
        ret = 0;
fail:
        return ret;
}

#define EXECUTING (ctx->exec != NULL)
#define VALIDATING (ctx->validation != NULL)
#define ECTX (ctx->exec)
#define VCTX (ctx->validation)
#define INSN_IMPL(NAME)                                                       \
        int process_##NAME(const uint8_t **pp, const uint8_t *ep,             \
                           struct context *ctx)
#define LOAD_PC const uint8_t *p = *pp
#define SAVE_PC *pp = p
#define RELOAD_PC
#define SAVE_STACK_PTR
#define LOAD_STACK_PTR
#define ORIG_PC (*pp)
#define INSN_SUCCESS return 0
#define INSN_SUCCESS_RETURN INSN_SUCCESS
#define STACK &VEC_NEXTELEM(ECTX->stack)
#define STACK_ADJ(n) ECTX->stack.lsize += (n)

#include "insn_impl.h"

#undef EXECUTING
#undef VALIDATING
#undef ECTX
#undef VCTX
#undef INSN_IMPL
#undef LOAD_PC
#undef SAVE_PC
#undef RELOAD_PC
#undef SAVE_STACK_PTR
#undef LOAD_STACK_PTR
#undef ORIG_PC
#undef INSN_SUCCESS
#undef INSN_SUCCESS_RETURN
#undef STACK
#undef STACK_ADJ

#if defined(USE_SEPARATE_EXECUTE)
#define EXECUTING true
#define VALIDATING false
#define ECTX ctx
#define VCTX ((struct validation_context *)NULL)
#define INSN_IMPL(NAME)                                                       \
        int execute_##NAME(const uint8_t *p, struct cell *stack,              \
                           struct exec_context *ctx)
#define LOAD_PC const uint8_t *p0 __attribute__((__unused__)) = p
#define SAVE_PC
#define RELOAD_PC p = ctx->p
#define SAVE_STACK_PTR ctx->stack.lsize = stack - ctx->stack.p
#define LOAD_STACK_PTR stack = &VEC_NEXTELEM(ctx->stack)
#define ORIG_PC p0
#if defined(USE_TAILCALL)
#define INSN_SUCCESS __musttail return exec_next_insn(p, stack, ctx)
#else
#define INSN_SUCCESS INSN_SUCCESS_RETURN
#endif
#define INSN_SUCCESS_RETURN                                                   \
        SAVE_STACK_PTR;                                                       \
        ctx->p = p;                                                           \
        return 0
#define ep NULL
#define STACK stack
#define STACK_ADJ(n) stack += (n)
#if defined(USE_SMALL_CELLS)
#define push_val(v, csz, ctx) stack_push_val(ctx, v, &stack, csz)
#define pop_val(v, csz, ctx) stack_pop_val(ctx, v, &stack, csz)
#else
#define push_val(v, csz, ctx)                                                 \
        do {                                                                  \
                assert(csz == 1);                                             \
                (stack++)->x = (v)->u.i64;                                    \
        } while (0)
#define pop_val(v, csz, ctx)                                                  \
        do {                                                                  \
                assert(csz == 1);                                             \
                (v)->u.i64 = (--stack)->x;                                    \
        } while (0)
#endif

#include "insn_impl.h"

#undef EXECUTING
#undef VALIDATING
#undef ECTX
#undef VCTX
#undef INSN_IMPL
#undef LOAD_PC
#undef SAVE_PC
#undef RELOAD_PC
#undef SAVE_STACK_PTR
#undef LOAD_STACK_PTR
#undef ORIG_PC
#undef INSN_SUCCESS
#undef INSN_SUCCESS_RETURN
#undef ep
#undef STACK
#undef STACK_ADJ
#endif /* defined(USE_SEPARATE_EXECUTE) */

#if defined(USE_SEPARATE_EXECUTE)
const static struct exec_instruction_desc exec_instructions_fc[];

static int
exec_next_insn_fc(const uint8_t *p, struct cell *stack,
                  struct exec_context *ctx)
{
#if !(defined(USE_SEPARATE_EXECUTE) && defined(USE_TAILCALL))
        assert(ctx->p + 1 == p);
#endif
        assert(ctx->event == EXEC_EVENT_NONE);
        assert(ctx->frames.lsize > 0);
#if defined(ENABLE_TRACING)
        uint32_t pc = ptr2pc(ctx->instance->module, p);
#endif
        uint32_t op = *p++;
        const struct exec_instruction_desc *desc = &exec_instructions_fc[op];
        xlog_trace("exec %06" PRIx32 ": %s (2nd byte %02" PRIx32 ")", pc,
                   instructions[op].name, op);
#if defined(USE_TAILCALL)
        __musttail
#endif
                return desc->execute(p, stack, ctx);
}

#define INSTRUCTION(b, n, f, FLAGS)                                           \
        [b] = {                                                               \
                .execute = execute_##f,                                       \
        },

#define INSTRUCTION_INDIRECT(b, n, t)                                         \
        [b] = {                                                               \
                .execute = exec_next_insn_fc,                                 \
        },

const static struct exec_instruction_desc exec_instructions_fc[] = {
#include "insn_list_fc.h"
};

const struct exec_instruction_desc exec_instructions[] = {
#include "insn_list_base.h"
};

#undef INSTRUCTION
#undef INSTRUCTION_INDIRECT

#endif /* defined(USE_SEPARATE_EXECUTE) */

#define INSTRUCTION(b, n, f, FLAGS)                                           \
        [b] = {                                                               \
                .name = n,                                                    \
                .process = process_##f,                                       \
                .flags = FLAGS,                                               \
                .next_table = NULL,                                           \
        },

#define INSTRUCTION_INDIRECT(b, n, t)                                         \
        [b] = {                                                               \
                .name = n,                                                    \
                .next_table = t,                                              \
                .next_table_size = ARRAYCOUNT(t),                             \
        },

const static struct instruction_desc instructions_fc[] = {
#include "insn_list_fc.h"
};

const struct instruction_desc instructions[] = {
#include "insn_list_base.h"
};

const size_t instructions_size = ARRAYCOUNT(instructions);
