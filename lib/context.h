#include <stdbool.h>
#include <stdint.h>

#include "toywasm_config.h"

#include "cell.h"
#include "platform.h"
#include "report.h"
#include "vec.h"

struct localtype;
struct module;

enum ctrlframe_op {
        FRAME_OP_BLOCK = 0x02,
        FRAME_OP_LOOP = 0x03,
        FRAME_OP_IF = 0x04,
        FRAME_OP_ELSE = 0x05,

        FRAME_OP_END = 0x0b,

        /* pseudo op */
        FRAME_OP_EMPTY_ELSE = 0xfe,
        FRAME_OP_INVOKE = 0xff,
};

struct label {
        uint32_t pc;
        uint32_t height; /* saved height of operand stack */
};

struct funcframe {
        struct instance *instance;
        uint32_t funcidx;

        /* this doesn't include the implicit label */
        uint32_t labelidx;

#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        uint32_t localidx;
#endif

        uint32_t callerpc;
        uint32_t height;   /* saved height of operand stack */
        uint32_t nresults; /* number of cells for the result */
};

/* For funcframe.funcidx */
#define FUNCIDX_INVALID UINT32_MAX

enum trapid {
        TRAP_MISC,
        TRAP_DIV_BY_ZERO,
        TRAP_INTEGER_OVERFLOW,
        TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
        TRAP_UNREACHABLE,
        TRAP_TOO_MANY_FRAMES,
        TRAP_TOO_MANY_STACKCELLS,
        TRAP_CALL_INDIRECT_OUT_OF_BOUNDS_TABLE_ACCESS,
        TRAP_CALL_INDIRECT_NULL_FUNCREF,
        TRAP_CALL_INDIRECT_FUNCTYPE_MISMATCH,
        TRAP_INVALID_CONVERSION_TO_INTEGER,
        TRAP_VOLUNTARY_EXIT,
        TRAP_VOLUNTARY_THREAD_EXIT,
        TRAP_OUT_OF_BOUNDS_DATA_ACCESS,
        TRAP_OUT_OF_BOUNDS_TABLE_ACCESS,
        TRAP_OUT_OF_BOUNDS_ELEMENT_ACCESS,
        TRAP_ATOMIC_WAIT_ON_NON_SHARED_MEMORY,
        TRAP_UNALIGNED_ATOMIC_OPERATION,
};

enum exec_event {
        EXEC_EVENT_NONE,
        EXEC_EVENT_CALL,
        EXEC_EVENT_BRANCH,
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        EXEC_EVENT_RETURN_CALL,
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
};

struct exec_stat {
        uint64_t call;
        uint64_t host_call;      /* included in call */
        uint64_t tail_call;      /* included in call */
        uint64_t host_tail_call; /* included in host_call and call */
        uint64_t branch;
        uint64_t branch_goto_else;
        uint64_t jump_cache_hit;
        uint64_t jump_cache2_hit;
        uint64_t jump_table_search;
        uint64_t type_annotation_lookup1;
        uint64_t type_annotation_lookup2;
        uint64_t type_annotation_lookup3;
};

#define STAT_INC(s) (s)++

struct jump_cache {
        uint32_t key;
        uint32_t param_arity;
        uint32_t arity;
        bool stay_in_block;
        const uint8_t *target;
};

struct trap_info {
        enum trapid trapid;
        uint32_t exit_code; /* wasi */
};

struct exec_context {
        /* Some cached info about the current frame. */
        struct instance *instance;
        const struct resulttype *paramtype;
        const struct localtype *localtype;
        const struct expr_exec_info *ei;

        const uint8_t *p;
#if defined(TOYWASM_USE_LOCALS_CACHE)
        struct cell *current_locals;
#endif
#if defined(TOYWASM_USE_JUMP_CACHE)
        const struct jump *jump_cache;
#endif
#if TOYWASM_JUMP_CACHE2_SIZE > 0
        struct jump_cache cache[TOYWASM_JUMP_CACHE2_SIZE];
#endif

        VEC(, struct funcframe) frames;
        VEC(, struct cell) stack; /* operand stack */
        VEC(, struct label) labels;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        VEC(, struct cell) locals;
#endif

        const uint32_t *intrp;

        bool trapped; /* used with a combination with EFAULT */
        struct trap_info trap;
        struct report *report;
        struct report report0;

        enum exec_event event;
        union {
                struct {
                        const struct funcinst *func;
                } call;
                struct {
                        bool goto_else;
                        uint32_t index;
                } branch;
        } event_u;
        struct exec_stat stats;
};

struct context {
        struct exec_context *exec;
        struct validation_context *validation;
};

/*
 * This "pc" is intended to be compatible with
 * the offset shown by "wasm-objdump -d".
 *
 * It's also compatible with "pcOffset" in the JS conventions:
 * https://webassembly.github.io/spec/web-api/index.html#conventions
 *
 * Besides logging, we use it in a few places to save space as it's
 * smaller than host pointers on 64-bit archs.
 */
uint32_t ptr2pc(const struct module *m, const uint8_t *p) __purefunc;
const uint8_t *pc2ptr(const struct module *m, uint32_t pc) __purefunc;

int resulttype_alloc(uint32_t ntypes, const enum valtype *types,
                     struct resulttype **resultp);
void resulttype_free(struct resulttype *p);

/* execution */

int trap(struct exec_context *ctx, const char *fmt, ...)
        __attribute__((__format__(__printf__, 2, 3)));
int trap_with_id(struct exec_context *ctx, enum trapid id, const char *fmt,
                 ...) __attribute__((__format__(__printf__, 3, 4)));
int memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                  uint32_t offset, uint32_t size, void **pp);
int memory_getptr2(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                   uint32_t offset, uint32_t size, void **pp, bool *movedp);
struct atomics_mutex;
int memory_atomic_getptr(struct exec_context *ctx, uint32_t memidx,
                         uint32_t ptr, uint32_t offset, uint32_t size,
                         void **pp, struct atomics_mutex **lockp);
void memory_atomic_unlock(struct atomics_mutex *lock);
int frame_enter(struct exec_context *ctx, struct instance *inst,
                uint32_t funcidx, const struct expr_exec_info *ei,
                const struct localtype *localtype,
                const struct resulttype *paramtype, uint32_t nresults,
                const struct cell *params);
void frame_clear(struct funcframe *frame);
void frame_exit(struct exec_context *ctx);
struct cell *frame_locals(struct exec_context *ctx,
                          const struct funcframe *frame) __purefunc;
void exec_context_init(struct exec_context *ctx, struct instance *inst);
void exec_context_clear(struct exec_context *ctx);
void exec_context_print_stats(struct exec_context *ctx);

uint32_t find_type_annotation(struct exec_context *ectx, const uint8_t *p);
