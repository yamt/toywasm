#if !defined(_TOYWASM_LIST_H)
#define _TOYWASM_LIST_H

/*
 * This is a tail queue implementation
 *
 * - the structure is similar to BSD queue.h TAILQ.
 *   re-invented just for NIH.
 *
 * - insert and remove are O(1).
 */

#include <stdint.h>

#include "platform.h"

struct list_elem;

struct list_entry {
        struct list_elem *next;
        struct list_elem **prevnextp;
};

struct list_head {
        struct list_elem *first;
        struct list_elem **tailnextp;
};

__BEGIN_EXTERN_C

void list_head_init(struct list_head *h);
void list_remove(struct list_head *h, void *elem, struct list_entry *e);
void list_insert_tail(struct list_head *h, void *elem, struct list_entry *e);
void list_insert_head(struct list_head *h, void *elem, struct list_entry *e);

__END_EXTERN_C

#define LIST_ENTRY(TYPE)                                                      \
        struct {                                                              \
                TYPE *next;                                                   \
                TYPE **prevnextp;                                             \
        }

#define LIST_HEAD(TYPE)                                                       \
        struct {                                                              \
                TYPE *first;                                                  \
                TYPE **tailnextp;                                             \
        }

#define LIST_HEAD_NAMED(TYPE, NAME)                                           \
        struct NAME {                                                         \
                TYPE *first;                                                  \
                TYPE **tailnextp;                                             \
        }

#define LIST_FOREACH(VAR, HEAD, NAME)                                         \
        for (VAR = LIST_FIRST(HEAD); VAR != NULL; VAR = LIST_NEXT(VAR, NAME))
#define LIST_FOREACH_REVERSE(VAR, HEAD, TYPE, NAME)                           \
        for (VAR = LIST_LAST(HEAD, TYPE, NAME); VAR != NULL;                  \
             VAR = LIST_PREV(VAR, HEAD, TYPE, NAME))

#if defined(toywasm_typeof)
#define CHECK_TYPE(a, b)                                                      \
        do {                                                                  \
                __unused toywasm_typeof(a) *__dummy = &b;                     \
        } while (0)
#else
#define CHECK_TYPE(a, b)
#endif

#define _LIST_NEXT_PTR_TO_ELEM(ELEM, TYPE, NAME)                              \
        (TYPE *)((uintptr_t)(ELEM) - toywasm_offsetof(TYPE, NAME.next))

#define LIST_FIRST(HEAD) (HEAD)->first
#define LIST_LAST(HEAD, TYPE, NAME)                                           \
        (LIST_EMPTY(HEAD)                                                     \
                 ? NULL                                                       \
                 : _LIST_NEXT_PTR_TO_ELEM((HEAD)->tailnextp, TYPE, NAME))
#define LIST_NEXT(VAR, NAME) (VAR)->NAME.next
#define LIST_PREV(VAR, HEAD, TYPE, NAME)                                      \
        (LIST_FIRST(HEAD) == (VAR)                                            \
                 ? NULL                                                       \
                 : _LIST_NEXT_PTR_TO_ELEM((VAR)->NAME.prevnextp, TYPE, NAME))

#define LIST_EMPTY(HEAD) (LIST_FIRST(HEAD) == NULL)

#define LIST_REMOVE(HEAD, ELEM, NAME)                                         \
        CHECK_TYPE(&(HEAD)->first, (HEAD)->tailnextp);                        \
        CHECK_TYPE((HEAD)->first, (ELEM)->NAME.next);                         \
        CHECK_TYPE(&(ELEM)->NAME.next, (ELEM)->NAME.prevnextp);               \
        ctassert(sizeof(*(HEAD)) == sizeof(struct list_head));                \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct list_entry));          \
        list_remove((struct list_head *)(HEAD), (ELEM),                       \
                    (struct list_entry *)&(ELEM)->NAME)

#define LIST_INSERT_TAIL(HEAD, ELEM, NAME)                                    \
        CHECK_TYPE(&(HEAD)->first, (HEAD)->tailnextp);                        \
        CHECK_TYPE((HEAD)->first, (ELEM)->NAME.next);                         \
        CHECK_TYPE(&(ELEM)->NAME.next, (ELEM)->NAME.prevnextp);               \
        ctassert(sizeof(*(HEAD)) == sizeof(struct list_head));                \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct list_entry));          \
        list_insert_tail((struct list_head *)(HEAD), (ELEM),                  \
                         (struct list_entry *)&(ELEM)->NAME)

#define LIST_INSERT_HEAD(HEAD, ELEM, NAME)                                    \
        CHECK_TYPE(&(HEAD)->first, (HEAD)->tailnextp);                        \
        CHECK_TYPE((HEAD)->first, (ELEM)->NAME.next);                         \
        CHECK_TYPE(&(ELEM)->NAME.next, (ELEM)->NAME.prevnextp);               \
        ctassert(sizeof(*(HEAD)) == sizeof(struct list_head));                \
        ctassert(sizeof((ELEM)->NAME) == sizeof(struct list_entry));          \
        list_insert_head((struct list_head *)(HEAD), (ELEM),                  \
                         (struct list_entry *)&(ELEM)->NAME)

#define LIST_HEAD_INIT(HEAD)                                                  \
        ctassert(sizeof(*(HEAD)) == sizeof(struct list_head));                \
        list_head_init((struct list_head *)(HEAD));                           \
        CHECK_TYPE(&(HEAD)->first, (HEAD)->tailnextp)

#endif /* !defined(_TOYWASM_LIST_H) */
