#if !defined(_OPTIONS_H)
#define _OPTIONS_H

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
};
#endif /* !defined(_OPTIONS_H) */
