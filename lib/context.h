#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "toywasm_config.h"

#include "cell.h"
#include "list.h"
#include "options.h"
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
        EXEC_EVENT_RESTART_INSN,
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        EXEC_EVENT_RETURN_CALL,
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
};

enum restart_type {
        RESTART_NONE,
        RESTART_TIMER,
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
        uint64_t exec_loop_restart;
        uint64_t call_restart;
        uint64_t tail_call_restart;
        uint64_t atomic_wait_restart;
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
};

/*
 * Note: errno.h macros are positive numbers. (C, posix)
 * we use negative numbers for our purposes.
 */
#define ETOYWASMTRAP -1
#define ETOYWASMRESTART -2

/* use shorter interval for userland thread */
#if defined(TOYWASM_USE_USER_SCHED) || !defined(TOYWASM_PREALLOC_SHARED_MEMORY)
#define CHECK_INTERRUPT_INTERVAL_MS 50
#else
#define CHECK_INTERRUPT_INTERVAL_MS 300
#endif

struct sched;
struct context;

struct exec_context {
        /* Some cached info about the current frame. */
        struct instance *instance;
        const struct resulttype *paramtype;
        const struct localtype *localtype;
        const struct expr_exec_info *ei;

        /* The instruction pointer */
        const uint8_t *p;

        /* Some cache stuff */
#if defined(TOYWASM_USE_LOCALS_CACHE)
        struct cell *current_locals;
#endif
#if defined(TOYWASM_USE_JUMP_CACHE)
        const struct jump *jump_cache;
#endif
#if TOYWASM_JUMP_CACHE2_SIZE > 0
        struct jump_cache cache[TOYWASM_JUMP_CACHE2_SIZE];
#endif

        /* Execution stacks */
        VEC(, struct funcframe) frames;
        VEC(, struct cell) stack; /* operand stack */
        VEC(, struct label) labels;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        VEC(, struct cell) locals;
#endif

        /* check_interrupt() */
        const atomic_uint *intrp;
        struct cluster *cluster;

        /* scheduler */
        struct sched *sched;
        LIST_ENTRY(struct exec_context) rq;

        /* Trap */
        bool trapped; /* used with a combination with ETOYWASMTRAP */
        struct trap_info trap;
        struct report *report;
        struct report report0;

        /* Pending control flow event (call, br, ...) */
        enum exec_event event;
        union {
                struct {
                        const struct funcinst *func;
                } call;
                struct {
                        bool goto_else;
                        uint32_t index;
                } branch;
                struct {
#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
                        int (*fetch_exec)(const uint8_t *p, struct cell *stack,
                                          struct exec_context *ctx);
#else
                        int (*process)(const uint8_t **pp, const uint8_t *ep,
                                       struct context *ctx);
#endif
                } restart_insn;
        } event_u;

        /* Restart */
        enum restart_type restart_type;
        union {
                /*
                 * RESTART_TIMER
                 * fd_poll_oneoff
                 * memory.atomic.wait32/64
                 */
                struct {
                        struct timespec abstimeout;
                } timer;
        } restart_u;

        /* To simplify restart api */
        struct cell *results;
        const struct resulttype *resulttype;
        struct val *results_val;
        uint32_t nresults;
        uint32_t nstackused_saved;
#if defined(TOYWASM_USE_USER_SCHED)
        int exec_ret;
        void (*exec_done)(struct exec_context *);
        void *exec_done_arg;
#endif

        /* Options */
        struct exec_options options;

        /* Statistics */
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
struct toywasm_mutex;
int memory_atomic_getptr(struct exec_context *ctx, uint32_t memidx,
                         uint32_t ptr, uint32_t offset, uint32_t size,
                         void **pp, struct toywasm_mutex **lockp);
void memory_atomic_unlock(struct toywasm_mutex *lock);
int frame_enter(struct exec_context *ctx, struct instance *inst,
                uint32_t funcidx, const struct expr_exec_info *ei,
                const struct localtype *localtype,
                const struct resulttype *paramtype, uint32_t nresults,
                const struct cell *params);
void frame_clear(struct funcframe *frame);
void frame_exit(struct exec_context *ctx);
struct cell *frame_locals(const struct exec_context *ctx,
                          const struct funcframe *frame) __purefunc;
void exec_context_init(struct exec_context *ctx, struct instance *inst);
void exec_context_clear(struct exec_context *ctx);
void exec_context_print_stats(struct exec_context *ctx);

uint32_t find_type_annotation(struct exec_context *ectx, const uint8_t *p);
