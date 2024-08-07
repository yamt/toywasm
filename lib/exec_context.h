#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "toywasm_config.h"

#include "list.h"
#include "options.h"
#include "platform.h"
#include "report.h"
#include "vec.h"

struct val;
struct mem_context;

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

/*
 * Note: errno.h macros are positive numbers. (C, posix)
 * we use negative numbers for our purposes.
 * some of them are restartable. (see IS_RESTARTABLE macro)
 *
 * ETOYWASMTRAP:
 *
 *   the exection was terminated by a wasm trap.
 *   the caller can investigate ctx->trap for details.
 *
 * ETOYWASMRESTART:
 *
 *   the execution has been suspended for some reasons.
 *   possible reasons include:
 *
 *   - suspend_threads mechanism, which is used for
 *   memory.grow on multithreaded configuration.
 *
 *   - context switch requests for TOYWASM_USE_USER_SCHED
 *
 *   the caller should usually resume the execution by
 *   calling instance_execute_handle_restart.
 *
 * ETOYWASMUSERINTERRUPT:
 *
 *   see the comment on exec_context::intrp.
 */
#define ETOYWASMTRAP -1
#define ETOYWASMRESTART -2
#define ETOYWASMUSERINTERRUPT -3

#define IS_RESTARTABLE(error) ((error) <= ETOYWASMRESTART)

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
        TRAP_UNALIGNED_MEMORY_ACCESS,
        TRAP_INDIRECT_FUNCTION_TABLE_NOT_FOUND,
        TRAP_UNCAUGHT_EXCEPTION,
        TRAP_THROW_REF_NULL,
        TRAP_UNRESOLVED_IMPORTED_FUNC,
        TRAP_DEFAULT_MEMORY_NOT_FOUND,
};

enum exec_event {
        EXEC_EVENT_NONE,
        EXEC_EVENT_CALL,
        EXEC_EVENT_BRANCH,
        EXEC_EVENT_RESTART_INSN,
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        EXEC_EVENT_RETURN_CALL,
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        EXEC_EVENT_EXCEPTION,
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */
};

enum restart_type {
        RESTART_NONE,
        RESTART_TIMER,
        RESTART_HOSTFUNC,
};

struct exec_stat {
        uint64_t call;
        uint64_t host_call; /* included in call */
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        uint64_t tail_call;      /* included in call */
        uint64_t host_tail_call; /* included in host_call and call */
#endif
        uint64_t branch;
        uint64_t branch_goto_else;
#if defined(TOYWASM_USE_JUMP_CACHE)
        uint64_t jump_cache_hit;
#endif
#if TOYWASM_JUMP_CACHE2_SIZE > 0
        uint64_t jump_cache2_hit;
#endif
        uint64_t jump_table_search;
        uint64_t jump_loop;
#if defined(TOYWASM_USE_SMALL_CELLS)
        uint64_t type_annotation_lookup1;
        uint64_t type_annotation_lookup2;
        uint64_t type_annotation_lookup3;
#endif
        uint64_t interrupt_exit;
        uint64_t interrupt_suspend;
#if defined(TOYWASM_USE_USER_SCHED)
        uint64_t interrupt_usched;
#endif
        uint64_t interrupt_user;
        uint64_t interrupt_debug;
        uint64_t exec_loop_restart;
        uint64_t call_restart;
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        uint64_t tail_call_restart;
#endif
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        uint64_t atomic_wait_restart;
#endif
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        uint64_t exception;
#endif
};

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

struct sched;
struct context;

struct restart_info {
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

                /*
                 * RESTART_HOSTFUNC
                 *
                 * REVISIT: this structure is a bit too large to have
                 * in a union for my taste
                 */
                struct restart_hostfunc {
                        const struct funcinst *func;
                        uint32_t saved_bottom;
                        uint32_t stack_adj;
                        uint32_t user1;
                        uint32_t user2;
                } hostfunc;
        } restart_u;
};

struct exec_context {
        /* Some cached info about the current frame. */
        struct instance *instance;
        const struct expr_exec_info *ei;
#if defined(TOYWASM_USE_LOCALS_FAST_PATH)
        bool fast;
#endif
        union {
#if defined(TOYWASM_USE_LOCALS_FAST_PATH)
                struct local_info_fast {
                        const uint16_t *paramtype_cellidxes;
                        const uint16_t *localtype_cellidxes;
                        uint32_t nparams;
                        uint32_t paramcsz;
                } fast;
#endif
                struct local_info_slow {
                        const struct resulttype *paramtype;
                        const struct localtype *localtype;
                } slow;
        } local_u;

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
        /*
         * The `intrp` field enables user-interrupts.
         * It's intended to allow embedders to implement functionalities
         * like async termination of the execution.
         *
         * To use the functionality, before starting execution of
         * the wasm module, an embedder should set it to a memory location
         * which describes the interrupt request state.
         * An interrupt is level-trigger.
         * An embedder can request an interrupt by setting it to
         * a non-zero value. (eg. `*intrp = 1`)
         * An embedder can clear the interrupt, by setting it to
         * zero. (ie. `*intrp = 0`)
         *
         * When an interrupt is requested, toywasm execution logic
         * (eg. `instance_execute_func`) returns `ETOYWASMUSERINTERRUPT`.
         * As `ETOYWASMUSERINTERRUPT` is a restartable error,
         * the embedder can either abort or continue the execution after
         * handling the interrupt.
         *
         * REVISIT: this API is partly historical and a bit awkward.
         * probably it's simpler to have a higher-level API like
         * raise_user_interrupt(ectx).
         */
        const atomic_uint *intrp;
        struct cluster *cluster;
        unsigned int user_intr_delay_count;
        unsigned int user_intr_delay;
        uint32_t check_interval;

#if defined(TOYWASM_USE_USER_SCHED)
        /* scheduler */
        struct sched *sched;
        LIST_ENTRY(struct exec_context) rq;
#endif

        /* Trap */
        bool trapped; /* for sanity check. apps should check ETOYWASMTRAP. */
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
        VEC(, struct restart_info) restarts;
        uint32_t bottom;

#if defined(TOYWASM_USE_USER_SCHED)
        int exec_ret;
        void (*exec_done)(struct exec_context *);
        void *exec_done_arg;
#endif

        struct mem_context *mctx;

        /* Options */
        struct exec_options options;

        /* Statistics */
        struct exec_stat stats;
};

#define exec_mctx(ectx) (ectx)->mctx

/* For funcframe.funcidx */
#define FUNCIDX_INVALID UINT32_MAX

/* for exec_stats */
#define STAT_INC(CTX, NAME) (CTX)->stats.NAME++

__BEGIN_EXTERN_C

void exec_context_init(struct exec_context *ctx, struct instance *inst,
                       struct mem_context *mctx);
void exec_context_clear(struct exec_context *ctx);
void exec_context_print_stats(struct exec_context *ctx);

int exec_push_vals(struct exec_context *ctx, const struct resulttype *rt,
                   const struct val *params);
void exec_pop_vals(struct exec_context *ctx, const struct resulttype *rt,
                   struct val *results);

int check_interrupt(struct exec_context *ctx);
int check_interrupt_interval_ms(struct exec_context *ctx);

int vtrap(struct exec_context *ctx, enum trapid id, const char *fmt,
          va_list ap);
int trap_with_id(struct exec_context *ctx, enum trapid id, const char *fmt,
                 ...) __attribute__((__format__(__printf__, 3, 4)));

__END_EXTERN_C
