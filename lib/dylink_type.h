#include "type.h"

struct dylink_mem_info {
        uint32_t memorysize;
        uint32_t memoryalignment;
        uint32_t tablesize;
        uint32_t tablealignment;
};

struct dylink_needs {
        uint32_t count;
        struct name *names;
};

struct dylink_import_info {
        struct name module_name;
        struct name name;
        uint32_t flags; /* WASM_SYM_xxx */
};

#define WASM_SYM_BINDING_WEAK 1

struct dylink {
        struct dylink_mem_info mem_info;
        struct dylink_needs needs;
        uint32_t nimport_info;
        struct dylink_import_info *import_info;
};
