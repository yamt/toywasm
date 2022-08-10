#include <stdbool.h>
#include <stdint.h>

#include "bitmap.h"
#include "report.h"

struct load_context {
        struct module *module;
        bool generate_jump_table;
        struct report report;
        uint32_t expected_ndatas;
        struct bitmap refs; /* C.refs */
};

void load_context_init(struct load_context *ctx);
void load_context_clear(struct load_context *ctx);
