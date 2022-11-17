#if !defined(_OPTIONS_H)
#define _OPTIONS_H

#include <stdbool.h>

struct load_options {
        bool generate_jump_table;
        bool generate_resulttype_cellidx;
        bool generate_localtype_cellidx;
};
#endif /* !defined(_OPTIONS_H) */
