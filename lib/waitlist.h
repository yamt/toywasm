#include <stdint.h>

struct waiter_list;

struct waiter_list_table {
        struct waiter_list *lists[1];
};

uint32_t atomics_notify(struct waiter_list_table *tab, uint32_t ident,
                        uint32_t count);

int atomics_wait(struct waiter_list_table *tab, uint32_t ident,
                 uint32_t timeout_ms);
