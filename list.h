#ifndef __LIST_H__
#define __LIST_H__

#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if HAVE_STDDEF_H
#  include <stddef.h>
# endif
#endif

#ifndef offsetof
#define offsetof(st, m) \
     ((size_t) ( (char *)&((st *)(0))->m - (char *)0 ))
#endif

#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})

typedef struct list_elem
{
    struct list_elem *next;
    struct list_elem *prev;
} list_elem;

static inline void list_init(list_elem *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_add_after(list_elem *pos, list_elem *elem)
{
    elem->next = pos->next;
    elem->prev = pos;
    pos->next->prev = elem;
    pos->next = elem;
}

static inline void list_add_before(list_elem *pos, list_elem *elem)
{
    elem->prev = pos->prev;
    elem->next = pos;
    pos->prev->next = elem;
    pos->prev = elem;
}

static inline void list_del(list_elem *elem)
{
    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
}

static inline void list_item_init(list_elem *elem)
{
    elem->next = elem;
    elem->prev = elem;
}

static inline int list_empty(list_elem *elem)
{
    return elem->next == elem;
}

#define list_for_each_elem(list, curr)            \
    for ((curr) = (list)->next; (curr) != (list); (curr) = (curr)->next)

#define list_for_each_elem_safe(list, curr, next)    \
    for ((curr) = (list)->next, (next) = (curr)->next;    \
         (curr) != (list);                    \
         (curr) = (next), (next) = (curr)->next)

#endif /* __LIST_H__ */
