#if !defined(_TOYWASM_OPTIONS_H)
#define _TOYWASM_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>

struct load_options {
        bool generate_jump_table;
        bool generate_resulttype_cellidx;
        bool generate_localtype_cellidx;
};

struct exec_options {
        uint32_t max_frames;
        uint32_t max_stackcells;
        /*
         * REVISIT: consider to have similar limits on locals and labels
         */
};

void load_options_set_defaults(struct load_options *opts);
void exec_options_set_defaults(struct exec_options *opts);
#endif /* !defined(_TOYWASM_OPTIONS_H) */
