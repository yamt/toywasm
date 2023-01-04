#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "list.h"

void
list_remove(struct list_head *h, void *elem, struct list_entry *e)
{
        assert(h->first != NULL);
        assert(*h->tailnextp == NULL);
        assert(*e->prevnextp == elem);
        *e->prevnextp = e->next;
        if (e->next == NULL) {
                assert(h->tailnextp == &e->next);
                h->tailnextp = e->prevnextp;
        } else {
                uintptr_t offset = (uintptr_t)e - (uintptr_t)elem;
                struct list_entry *nextentry =
                        (void *)((uintptr_t)e->next + offset);
                assert(nextentry->prevnextp == &e->next);
                nextentry->prevnextp = e->prevnextp;
        }
        assert(*h->tailnextp == NULL);
}

void
list_insert_tail(struct list_head *h, void *elem, struct list_entry *e)
{
        assert(*h->tailnextp == NULL);
        e->prevnextp = h->tailnextp;
        *h->tailnextp = elem;
        h->tailnextp = &e->next;
        e->next = NULL;
        assert(h->first != NULL);
        assert(*h->tailnextp == NULL);
        assert(*e->prevnextp == elem);
}

void
list_head_init(struct list_head *h)
{
        h->first = NULL;
        h->tailnextp = &h->first;
}
