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

struct dylink {
        struct dylink_mem_info mem_info;
        struct dylink_needs needs;
};

void clear_dylink(struct dylink *dy);
