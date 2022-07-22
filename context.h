#include <stdbool.h>
#include <stdint.h>

#include "vec.h"

enum valtype;

struct val;

enum ctrlframe_op {
        FRAME_OP_BLOCK = 0x02,
        FRAME_OP_LOOP = 0x03,
        FRAME_OP_IF = 0x04,
        FRAME_OP_ELSE = 0x05,

        FRAME_OP_END = 0x0b,

        /* pseudo op */
        FRAME_OP_INVOKE = 0xff,
};

struct ctrlframe {
        enum ctrlframe_op op;
        uint32_t jumpslot;

        struct resulttype *start_types;
        struct resulttype *end_types;

        uint32_t height;
        bool unreachable;
};

struct label {
        uint32_t pc;
        uint32_t height;
};

struct funcframe {
        /* this doesn't include the implicit label */
        uint32_t labelidx;

#if !defined(NDEBUG)
        uint32_t nlocals;
#endif
        uint32_t localidx;

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
};

struct validation_context {
        /* ctrl frames */
        struct ctrlframe *cframes;
        uint32_t ncframes;

        /* operand stack */

        enum valtype *valtypes;
        uint32_t nvaltypes;

        struct module *module;
        struct expr_exec_info *ei;

        uint32_t nlocals;
        enum valtype *locals;

        bool const_expr;
        bool generate_jump_table;
};

struct exec_context {
        struct instance *instance; /* REVISIT: redundant */
        const uint8_t *p;

        VEC(, struct funcframe) frames;
        VEC(, struct val) stack; /* operand stack */
        VEC(, struct label) labels;
        VEC(, struct val) locals;

        bool trapped; /* used with a combination with EFAULT */
        enum trapid trapid;
        char *trapmsg;

        bool call_pending;
        const struct funcinst *call_func;
        bool branch_pending;
        bool branch_else;
        uint32_t branch_index;
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

int push_val(const struct val *val, struct exec_context *ctx);
int pop_val(struct val *val, struct exec_context *ctx);
void pop_label(struct exec_context *ctx);
void push_label(struct exec_context *ctx);
int trap(struct exec_context *ctx, const char *fmt, ...)
        __attribute__((__format__(__printf__, 2, 3)));
int trap_with_id(struct exec_context *ctx, enum trapid id, const char *fmt,
                 ...) __attribute__((__format__(__printf__, 3, 4)));
int memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                  uint32_t offset, uint32_t size, void **pp);
int frame_enter(struct exec_context *ctx, struct instance *inst,
                const struct expr_exec_info *ei, uint32_t nlocals,
                uint32_t nparams, uint32_t nresults, const struct val *params);
void frame_clear(struct funcframe *frame);
void frame_exit(struct exec_context *ctx);
const struct jump *jump_lookup(const struct expr_exec_info *ei,
                               uint32_t blockpc);
void exec_context_init(struct exec_context *ctx, struct instance *inst);
void exec_context_clear(struct exec_context *ctx);

/* validation */

int push_valtype(enum valtype type, struct validation_context *ctx);
int pop_valtype(enum valtype expected_type, enum valtype *typep,
                struct validation_context *ctx);

int push_valtypes(const struct resulttype *types,
                  struct validation_context *ctx);
int pop_valtypes(const struct resulttype *types,
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
