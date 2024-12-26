#if !defined(_TOYWASM_SLIST_H)
#define _TOYWASM_SLIST_H

/*
 * This is a singly-linked queue implementation
 *
 * - the structure is similar to BSD queue.h STAILQ.
 *   re-invented just for NIH.
 */

#include <stdint.h>

#include "platform.h"

struct slist_elem;

struct slist_entry {
        struct slist_elem *se_next;
};

struct slist_head {
        struct slist_elem *sh_first;
        struct slist_elem **sh_tailnextp;
};

__BEGIN_EXTERN_C

void slist_head_init(struct slist_head *h);
void slist_remove(struct slist_head *h, struct slist_entry *prev,
                  struct slist_entry *e);
void slist_remove_head(struct slist_head *h, struct slist_entry *e);
void slist_insert_tail(struct slist_head *h, void *elem,
                       struct slist_entry *e);
void slist_insert_head(struct slist_head *h, void *elem,
                       struct slist_entry *e);

__END_EXTERN_C

#if defined(toywasm_typeof)
#define CHECK_TYPE(a, b)                                                      \
        do {                                                                  \
                __unused toywasm_typeof(a) *__dummy = &b;                     \
        } while (0)
#else
#define CHECK_TYPE(a, b)
#endif

#define SLIST_ENTRY(TYPE)                                                     \
        struct {                                                              \
                TYPE *se_next;                                                \
        }

#define SLIST_HEAD(TYPE)                                                      \
        struct {                                                              \
                TYPE *sh_first;                                               \
                TYPE **sh_tailnextp;                                          \
        }

#define SLIST_HEAD_NAMED(TYPE, NAME)                                          \
        struct NAME {                                                         \
                TYPE *sh_first;                                               \
                TYPE **sh_tailnextp;                                          \
        }

#define SLIST_FOREACH(VAR, HEAD, NAME)                                        \
        for (VAR = SLIST_FIRST(HEAD); VAR != NULL; VAR = SLIST_NEXT(VAR, NAME))

#define _SLIST_NEXT_PTR_TO_ELEM(ELEM, TYPE, NAME)                             \
        (TYPE *)((uintptr_t)(ELEM) - toywasm_offsetof(TYPE, NAME.se_next))

#define SLIST_FIRST(HEAD) (HEAD)->sh_first
#define SLIST_LAST(HEAD, TYPE, NAME)                                          \
        (SLIST_EMPTY(HEAD)                                                    \
                 ? NULL                                                       \
                 : _SLIST_NEXT_PTR_TO_ELEM((HEAD)->sh_tailnextp, TYPE, NAME))
#define SLIST_NEXT(VAR, NAME) (VAR)->NAME.se_next

#define SLIST_EMPTY(HEAD) (SLIST_FIRST(HEAD) == NULL)

/*
 * Note: when removing the first item on the list,
 * PREV should be a NULL with the correct type.
 */
#define SLIST_REMOVE(HEAD, PREV, ELEM, NAME)                                  \
        CHECK_TYPE(&(HEAD)->sh_first, (HEAD)->sh_tailnextp);                  \
        CHECK_TYPE((HEAD)->sh_first, (ELEM)->NAME.se_next);                   \
        ctassert(sizeof(*(HEAD)) == sizeof(struct slist_head));               \
        ctassert(sizeof((PREV)->NAME) == sizeof(struct slist_entry));         \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct slist_entry));         \
        slist_remove((struct slist_head *)(HEAD),                             \
                     (struct slist_entry *)((PREV == NULL) ? NULL             \
                                                           : &(PREV)->NAME),  \
                     (struct slist_entry *)&(ELEM)->NAME)

/*
 * SLIST_REMOVE_HEAD(h, e, name) is an equivalent of
 * SLIST_REMOVE(h, (TYPE *)NULL, e, name).
 */
#define SLIST_REMOVE_HEAD(HEAD, ELEM, NAME)                                   \
        CHECK_TYPE(&(HEAD)->sh_first, (HEAD)->sh_tailnextp);                  \
        CHECK_TYPE((HEAD)->sh_first, (ELEM)->NAME.se_next);                   \
        ctassert(sizeof(*(HEAD)) == sizeof(struct slist_head));               \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct slist_entry));         \
        slist_remove_head((struct slist_head *)(HEAD),                        \
                          (struct slist_entry *)&(ELEM)->NAME)

#define SLIST_INSERT_TAIL(HEAD, ELEM, NAME)                                   \
        CHECK_TYPE(&(HEAD)->sh_first, (HEAD)->sh_tailnextp);                  \
        CHECK_TYPE((HEAD)->sh_first, (ELEM)->NAME.se_next);                   \
        ctassert(sizeof(*(HEAD)) == sizeof(struct slist_head));               \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct slist_entry));         \
        slist_insert_tail((struct slist_head *)(HEAD), (ELEM),                \
                          (struct slist_entry *)&(ELEM)->NAME)

#define SLIST_INSERT_HEAD(HEAD, ELEM, NAME)                                   \
        CHECK_TYPE(&(HEAD)->sh_first, (HEAD)->sh_tailnextp);                  \
        CHECK_TYPE((HEAD)->sh_first, (ELEM)->NAME.se_next);                   \
        ctassert(sizeof(*(HEAD)) == sizeof(struct slist_head));               \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct slist_entry));         \
        slist_insert_head((struct slist_head *)(HEAD), (ELEM),                \
                          (struct slist_entry *)&(ELEM)->NAME)

#define SLIST_HEAD_INIT(HEAD)                                                 \
        ctassert(sizeof(*(HEAD)) == sizeof(struct slist_head));               \
        slist_head_init((struct slist_head *)(HEAD));                         \
        CHECK_TYPE(&(HEAD)->sh_first, (HEAD)->sh_tailnextp)

#define SLIST_SPLICE_TAIL(DST, SRC, NAME)                                     \
        do {                                                                  \
                if (!SLIST_EMPTY(SRC)) {                                      \
                        *(DST)->sh_tailnextp = (SRC)->sh_first;               \
                        (DST)->sh_tailnextp = (SRC)->sh_tailnextp;            \
                }                                                             \
        } while (0)

#define SLIST_SPLICE_HEAD(DST, SRC, NAME)                                     \
        do {                                                                  \
                if (!SLIST_EMPTY(SRC)) {                                      \
                        if (SLIST_EMPTY(DST)) {                               \
                                (DST)->sh_tailnextp = (SRC)->sh_tailnextp;    \
                        } else {                                              \
                                *(SRC)->sh_tailnextp = (DST)->sh_first;       \
                        }                                                     \
                        (DST)->sh_first = (SRC)->sh_first;                    \
                }                                                             \
        } while (0)

#endif /* !defined(_TOYWASM_SLIST_H) */
