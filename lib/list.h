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

void list_head_init(struct list_head *h);
void list_remove(struct list_head *h, void *elem, struct list_entry *e);
void list_insert_tail(struct list_head *h, void *elem, struct list_entry *e);

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

#define LIST_FOREACH(VAR, HEAD, NAME)                                         \
        for (VAR = (HEAD)->first; VAR != NULL; VAR = (VAR)->NAME.next)

#if defined(toywasm_typeof)
#define CHECK_TYPE(a, b)                                                      \
        do {                                                                  \
                __attribute__((__unused__)) toywasm_typeof(a) __dummy = b;    \
        } while (0)
#else
#define CHECK_TYPE(a, b)
#endif

#define LIST_FIRST(HEAD) (HEAD)->first

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

#define LIST_HEAD_INIT(HEAD)                                                  \
        ctassert(sizeof(*(HEAD)) == sizeof(struct list_head));                \
        list_head_init((struct list_head *)(HEAD));                           \
        CHECK_TYPE(&(HEAD)->first, (HEAD)->tailnextp)
