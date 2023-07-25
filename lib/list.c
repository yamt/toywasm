#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "list.h"

static void
list_check(const struct list_head *h)
{
        assert(*h->tailnextp == NULL);
        assert((h->tailnextp == &h->first) == (h->first == NULL));
}

void
list_check2(const struct list_head *h, const void *elem0,
            const struct list_entry *e0, bool elem0_on_list)
{
#if !defined(NDEBUG)
        bool on_list = false;
        list_check(h);
        uintptr_t offset = (uintptr_t)e0 - (uintptr_t)elem0;
        const void *elem;
        elem = h->first;
        while (elem != NULL) {
                if (elem == elem0) {
                        on_list = true;
                }
                const struct list_entry *e =
                        (void *)((uintptr_t)elem + offset);
                assert(*e->prevnextp == elem);
                const void *nextelem = e->next;
                assert(nextelem != elem);
                if (nextelem == NULL) {
                        assert(h->tailnextp == &e->next);
                }
                elem = nextelem;
        }
        assert(on_list == elem0_on_list);
#endif
}

void
list_remove(struct list_head *h, void *elem, struct list_entry *e)
{
        list_check2(h, elem, e, true);
        assert(h->first != NULL);
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
        list_check2(h, elem, e, false);
}

void
list_insert_tail(struct list_head *h, void *elem, struct list_entry *e)
{
        list_check2(h, elem, e, false);
        e->prevnextp = h->tailnextp;
        *h->tailnextp = elem;
        h->tailnextp = &e->next;
        e->next = NULL;
        assert(h->first != NULL);
        assert(*e->prevnextp == elem);
        list_check2(h, elem, e, true);
}

void
list_insert_head(struct list_head *h, void *elem, struct list_entry *e)
{
        list_check2(h, elem, e, false);
        if (h->first == NULL) {
                e->next = NULL;
                h->tailnextp = &e->next;
        } else {
                uintptr_t offset = (uintptr_t)e - (uintptr_t)elem;
                struct list_entry *firstentry =
                        (void *)((uintptr_t)h->first + offset);
                assert(firstentry->prevnextp == &h->first);
                firstentry->prevnextp = &e->next;
                e->next = h->first;
                e->prevnextp = &h->first;
        }
        h->first = elem;
        assert(h->first != NULL);
        assert(*e->prevnextp == elem);
        list_check2(h, elem, e, true);
}

void
list_head_init(struct list_head *h)
{
        h->first = NULL;
        h->tailnextp = &h->first;
        list_check(h);
}
