#include <stdbool.h>
#include <stdint.h>

#include "bitmap.h"
#include "options.h"
#include "platform.h"
#include "report.h"

struct load_context {
        struct module *module;
        struct report report;
        struct bitmap refs; /* C.refs */
        bool has_datacount;
        uint32_t ndatas_in_datacount;
        struct load_options options;
        struct mem_context *mctx;
        struct validation_context *vctx;
};

#define load_mctx(l) (l)->mctx

__BEGIN_EXTERN_C

void load_context_init(struct load_context *ctx, struct mem_context *mctx);
void load_context_clear(struct load_context *ctx);

__END_EXTERN_C
