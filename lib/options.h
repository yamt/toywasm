#if !defined(_TOYWASM_OPTIONS_H)
#define _TOYWASM_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "toywasm_config.h"

struct load_options {
        bool generate_jump_table;
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        bool generate_resulttype_cellidx;
#endif
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        bool generate_localtype_cellidx;
#endif
};

struct exec_options {
        uint32_t max_frames;
        uint32_t max_stackcells;
        /*
         * REVISIT: consider to have similar limits on locals and labels
         */
};

__BEGIN_EXTERN_C

void load_options_set_defaults(struct load_options *opts);
void exec_options_set_defaults(struct exec_options *opts);

__END_EXTERN_C

#endif /* !defined(_TOYWASM_OPTIONS_H) */
