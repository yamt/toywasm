#include <stdint.h>
#include <string.h>

#include "options.h"

void
load_options_set_defaults(struct load_options *opts)
{
        memset(opts, 0, sizeof(*opts));
        opts->generate_jump_table = true;
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        opts->generate_localtype_cellidx = true;
#endif
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        opts->generate_resulttype_cellidx = true;
#endif
}

void
exec_options_set_defaults(struct exec_options *opts)
{
        memset(opts, 0, sizeof(*opts));
        opts->max_frames = UINT32_MAX;
        opts->max_stackcells = UINT32_MAX;
}
