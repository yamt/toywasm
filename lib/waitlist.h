#include <stdint.h>

struct toywasm_mutex;
struct waiter_list;
struct timespec;

struct waiter_list_table {
        struct waiter_list *lists[1];
};

uint32_t atomics_notify(struct waiter_list_table *tab, uint32_t ident,
                        uint32_t count);

void waiter_list_table_init(struct waiter_list_table *tab);

int atomics_wait(struct waiter_list_table *tab, uint32_t ident,
                 const struct timespec *abstimeout);

struct toywasm_mutex *atomics_mutex_getptr(struct waiter_list_table *tab,
                                           uint32_t ident);
