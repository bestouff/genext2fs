#ifndef __CACHE_H__
#define __CACHE_H__

#include "list.h"

#define CACHE_LISTS 256

typedef struct
{
    list_elem link;
    list_elem lru_link;
} cache_link;

typedef struct
{
    /* LRU list holds unused items */
    unsigned int lru_entries;
    list_elem lru_list;
    unsigned int max_free_entries;

    unsigned int entries;
    list_elem lists[CACHE_LISTS];
    unsigned int (*elem_val)(cache_link *elem);
    void (*freed)(cache_link *elem);
} listcache;

static inline void
cache_add(listcache *c, cache_link *elem)
{
    unsigned int hash = c->elem_val(elem) % CACHE_LISTS;
    int delcount = c->lru_entries - c->max_free_entries;

    if (delcount > 0) {
        /* Delete some unused items. */
        list_elem *lru, *next;
        cache_link *l;
        list_for_each_elem_safe(&c->lru_list, lru, next) {
            l = container_of(lru, cache_link, lru_link);
            list_del(lru);
            list_del(&l->link);
            c->entries--;
            c->lru_entries--;
            c->freed(l);
            delcount--;
            if (delcount <= 0)
                break;
        }
    }

    c->entries++;
    list_item_init(&elem->lru_link); /* Mark it not in the LRU list */
    list_add_after(&c->lists[hash], &elem->link);
}

static inline void
cache_item_set_unused(listcache *c, cache_link *elem)
{
    list_add_before(&c->lru_list, &elem->lru_link);
    c->lru_entries++;
}

static inline cache_link *
cache_find(listcache *c, unsigned int val)
{
    unsigned int hash = val % CACHE_LISTS;
    list_elem *elem;

    list_for_each_elem(&c->lists[hash], elem) {
        cache_link *l = container_of(elem, cache_link, link);
        if (c->elem_val(l) == val) {
            if (!list_empty(&l->lru_link)) {
                /* It's in the unused list, remove it. */
                list_del(&l->lru_link);
                list_item_init(&l->lru_link);
                c->lru_entries--;
            }
            return l;
        }
    }
    return NULL;
}

static inline int
cache_flush(listcache *c)
{
    list_elem *elem, *next;
    cache_link *l;
    int i;

    list_for_each_elem_safe(&c->lru_list, elem, next) {
        l = container_of(elem, cache_link, lru_link);
        list_del(elem);
        list_del(&l->link);
        c->entries--;
        c->lru_entries--;
        c->freed(l);
    }

    for (i = 0; i < CACHE_LISTS; i++) {
        list_for_each_elem_safe(&c->lists[i], elem, next) {
            l = container_of(elem, cache_link, link);
            list_del(&l->link);
            c->entries--;
            c->freed(l);
        }
    }

    return c->entries || c->lru_entries;
}

static inline void
cache_init(listcache *c, unsigned int max_free_entries,
       unsigned int (*elem_val)(cache_link *elem),
       void (*freed)(cache_link *elem))
{
    int i;

    c->entries = 0;
    c->lru_entries = 0;
    c->max_free_entries = max_free_entries;
    list_init(&c->lru_list);
    for (i = 0; i < CACHE_LISTS; i++)
        list_init(&c->lists[i]);
    c->elem_val = elem_val;
    c->freed = freed;
}

#endif /* __CACHE_H__ */
