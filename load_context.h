#include <stdbool.h>
#include <stdint.h>

struct load_context {
        struct module *module;
        bool generate_jump_table;
};

void load_context_init(struct load_context *ctx);
void load_context_clear(struct load_context *ctx);
