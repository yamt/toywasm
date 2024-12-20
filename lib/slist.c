#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "slist.h"

void
slist_remove(struct slist_head *h, struct slist_entry *prev,
             struct slist_entry *e)
{
        assert(h->sh_first != NULL);
        if (prev == NULL) {
                /* removing the first entry */
                h->sh_first = e->se_next;
                if (e->se_next == NULL) {
                        /* removing the only entry */
                        h->sh_tailnextp = &h->sh_first;
                }
        } else {
                prev->se_next = e->se_next;
                if (h->sh_tailnextp == &e->se_next) {
                        /* removing the last entry */
                        assert(e->se_next == NULL);
                        h->sh_tailnextp = &prev->se_next;
                }
        }
}

void
slist_insert_tail(struct slist_head *h, void *elem, struct slist_entry *e)
{
        *h->sh_tailnextp = elem;
        h->sh_tailnextp = &e->se_next;
        e->se_next = NULL;
        assert(h->sh_first != NULL);
}

void
slist_insert_head(struct slist_head *h, void *elem, struct slist_entry *e)
{
        if (h->sh_first == NULL) {
                h->sh_tailnextp = &e->se_next;
        }
        e->se_next = h->sh_first;
        h->sh_first = elem;
        assert(h->sh_first != NULL);
}

void
slist_head_init(struct slist_head *h)
{
        h->sh_first = NULL;
        h->sh_tailnextp = &h->sh_first;
}
