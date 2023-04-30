#include "valtype.h"
#include "vec.h"

struct ctrlframe {
        enum ctrlframe_op op;
        uint32_t jumpslot;

        struct resulttype *start_types;
        struct resulttype *end_types;

        uint32_t height;
        uint32_t height_cell;
        bool unreachable;
};

struct validation_context {
        /* ctrl frames */
        VEC(, struct ctrlframe) cframes;

        /* operand stack */

        VEC(, enum valtype) valtypes;
        uint32_t ncells;

        struct module *module;
        struct expr_exec_info *ei;

        uint32_t nlocals;
        enum valtype *locals;

        bool const_expr;

        bool has_datacount;
        uint32_t ndatas_in_datacount;

        struct report *report;

        /*
         * C.refs
         *
         * https://webassembly.github.io/spec/core/valid/conventions.html#context
         * > References: the list of function indices that occur in
         * > the module outside functions and can hence be used to
         * > form references inside them.
         *
         * Functions in this list can have references and thus can be
         * called with call.indirect. Having this list before looking
         * at the code section can benefit 1-pass compilation.
         *
         * https://github.com/WebAssembly/reference-types/issues/31
         * https://github.com/WebAssembly/reference-types/issues/76
         */
        struct bitmap *refs;

        const struct load_options *options;
};

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
