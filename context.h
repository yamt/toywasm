#include <stdbool.h>
#include <stdint.h>

#include "cell.h"
#include "report.h"
#include "vec.h"

enum valtype;
struct localtype;

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

struct ctrlframe {
        enum ctrlframe_op op;
        uint32_t jumpslot;

        struct resulttype *start_types;
        struct resulttype *end_types;

        uint32_t height;
        uint32_t height_cell;
        bool unreachable;
};

struct label {
        uint32_t pc;
        uint32_t height;
};

struct funcframe {
        /* this doesn't include the implicit label */
        uint32_t labelidx;

#if defined(USE_SEPARATE_LOCALS)
        uint32_t localidx;
#endif

        /* REVISIT: simpler to have func/functype references? */
        const struct resulttype *paramtype;
        const struct localtype *localtype;
        const struct expr_exec_info *ei;

        struct instance *instance;
        uint32_t callerpc;
        uint32_t height;
        uint32_t nresults;
};

enum trapid {
        TRAP_MISC,
        TRAP_DIV_BY_ZERO,
        TRAP_INTEGER_OVERFLOW,
        TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
        TRAP_UNREACHABLE,
        TRAP_TOO_MANY_FRAMES,
        TRAP_TOO_MANY_STACKVALS,
        TRAP_CALL_INDIRECT_OUT_OF_BOUNDS_TABLE_ACCESS,
        TRAP_CALL_INDIRECT_NULL_FUNCREF,
        TRAP_CALL_INDIRECT_FUNCTYPE_MISMATCH,
        TRAP_INVALID_CONVERSION_TO_INTEGER,
        TRAP_VOLUNTARY_EXIT,
        TRAP_OUT_OF_BOUNDS_DATA_ACCESS,
        TRAP_OUT_OF_BOUNDS_TABLE_ACCESS,
        TRAP_OUT_OF_BOUNDS_ELEMENT_ACCESS,
};

struct validation_context {
        /* ctrl frames */
        struct ctrlframe *cframes;
        uint32_t ncframes;

        /* operand stack */

        enum valtype *valtypes;
        uint32_t nvaltypes;
        uint32_t ncells;

        struct module *module;
        struct expr_exec_info *ei;

        uint32_t nlocals;
        enum valtype *locals;

        bool const_expr;
        bool generate_jump_table;

        bool has_datacount;
        uint32_t ndatas_in_datacount;

        struct report *report;
        struct bitmap *refs; /* C.refs */
};

enum exec_event {
        EXEC_EVENT_NONE,
        EXEC_EVENT_CALL,
        EXEC_EVENT_BRANCH,
};

struct exec_stat {
        uint64_t call;
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

struct exec_context {
        struct instance *instance; /* REVISIT: redundant */
        const uint8_t *p;
#if defined(USE_LOCALS_CACHE)
        struct cell *current_locals;
#endif
#if defined(USE_JUMP_CACHE)
        const struct jump *jump_cache;
#endif
#if JUMP_CACHE2_SIZE > 0
        struct jump_cache cache[JUMP_CACHE2_SIZE];
#endif

        VEC(, struct funcframe) frames;
        VEC(, struct cell) stack; /* operand stack */
        VEC(, struct label) labels;
#if defined(USE_SEPARATE_LOCALS)
        VEC(, struct cell) locals;
#endif

        bool trapped; /* used with a combination with EFAULT */
        enum trapid trapid;
        uint32_t exit_code; /* wasi */
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
 * Also, it might be more space efficient than a host pointer.
 */
uint32_t ptr2pc(struct module *m, const uint8_t *p);
const uint8_t *pc2ptr(struct module *m, uint32_t pc);

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
int frame_enter(struct exec_context *ctx, struct instance *inst,
                const struct expr_exec_info *ei,
                const struct localtype *localtype,
                const struct resulttype *paramtype, uint32_t nresults,
                const struct cell *params);
void frame_clear(struct funcframe *frame);
void frame_exit(struct exec_context *ctx);
struct cell *frame_locals(struct exec_context *ctx,
                          const struct funcframe *frame);
void exec_context_init(struct exec_context *ctx, struct instance *inst);
void exec_context_clear(struct exec_context *ctx);
void exec_context_print_stats(struct exec_context *ctx);

uint32_t find_type_annotation(struct exec_context *ectx, const uint8_t *p);

/* validation */

int push_valtype(enum valtype type, struct validation_context *ctx);
int pop_valtype(enum valtype expected_type, enum valtype *typep,
                struct validation_context *ctx);

int push_valtypes(const struct resulttype *types,
                  struct validation_context *ctx);
int pop_valtypes(const struct resulttype *types,
                 struct validation_context *ctx);
int peek_valtypes(const struct resulttype *types,
                  struct validation_context *ctx);

int push_ctrlframe(uint32_t pc, enum ctrlframe_op op, uint32_t jumpslot,
                   struct resulttype *start_types,
                   struct resulttype *end_types,
                   struct validation_context *ctx);
int pop_ctrlframe(uint32_t pc, bool is_else, struct ctrlframe *cframe,
                  struct validation_context *ctx);
void mark_unreachable(struct validation_context *ctx);
const struct resulttype *label_types(struct ctrlframe *cframe);
int validation_failure(struct validation_context *ctx, const char *fmt, ...)
        __attribute__((__format__(__printf__, 2, 3)));
struct resulttype *returntype(struct validation_context *ctx);
void validation_context_init(struct validation_context *ctx);
void validation_context_clear(struct validation_context *ctx);
void ctrlframe_clear(struct ctrlframe *cframe);
int target_label_types(struct validation_context *ctx, uint32_t labelidx,
                       const struct resulttype **rtp);

int record_type_annotation(struct validation_context *vctx, const uint8_t *p,
                           enum valtype t);
