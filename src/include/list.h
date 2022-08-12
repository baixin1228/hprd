#ifndef __LIST_H__
#define __LIST_H__

#include <stdbool.h>

#ifndef container_of
#define container_of(p, type, member_p) ((type *)((char *)p - \
                    ((size_t) &((type *)0)->member_p)))
#endif

#define prefetch(x) ((void)x)

struct list_node {
	struct list_node *next, *prev;
};

#define LIST_HEAD(name) \
    struct list_node name = { &(name), &(name) }

__attribute__((unused))
static void init_list_node(struct list_node *list)
{
	list->next = list;
	list->prev = list;
}

__attribute__((unused))
static void _clear_list_node(struct list_node *list)
{
    init_list_node(list);
}

__attribute__((unused))
static void __list_insert(struct list_node *prev,
					   struct list_node *next,
					   struct list_node *_new)
{
	next->prev = _new;
	_new->next = next;
	_new->prev = prev;
	prev->next = _new;
}

__attribute__((unused))
static void list_prepend(struct list_node *list, struct list_node *_new)
{
	__list_insert(list, list->next, _new);
}

__attribute__((unused))
static void list_append(struct list_node *list, struct list_node *_new)
{
	__list_insert(list->prev, list, _new);
}

__attribute__((unused))
static void __list_del(struct list_node *prev, struct list_node *next)
{
	next->prev = prev;
	prev->next = next;
}

__attribute__((unused))
static void __list_del_node(struct list_node *entry)
{
	__list_del(entry->prev, entry->next);
}

__attribute__((unused))
static struct list_node *list_get_first(struct list_node *list)
{
	return list->next;
}

__attribute__((unused))
static struct list_node *list_get_tail(struct list_node *list)
{
	return list->prev;
}

__attribute__((unused))
static struct list_node *list_shift(struct list_node *list)
{
	struct list_node * node = list->next;
	__list_del_node(node);
	node->next = NULL;
	node->prev = NULL;
	return node;
}

__attribute__((unused))
static struct list_node *list_pop(struct list_node *list)
{
	struct list_node * node = list->prev;
	__list_del_node(node);
	node->next = NULL;
	node->prev = NULL;
	return node;
}

__attribute__((unused))
static void list_del(struct list_node *node)
{
	__list_del_node(node);
	node->next = NULL;
	node->prev = NULL;
}

__attribute__((unused))
static void list_replace(struct list_node *old,
						 struct list_node *_new)
{
	_new->next = old->next;
	_new->next->prev = _new;
	_new->prev = old->prev;
	_new->prev->next = _new;
}

__attribute__((unused))
static bool list_is_empty(const struct list_node *list)
{
	return list->next == list;
}

#define list_node_to_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_node_to_entry((ptr)->next, type, member)

#define list_for_each(pos, node) \
    for (pos = (node)->next; prefetch(pos->next), pos != (node); \
            pos = pos->next)

#define list_for_each_prev(pos, node) \
    for (pos = (node)->prev; prefetch(pos->prev), pos != (node); \
            pos = pos->prev)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; \
         prefetch(pos->prev), pos != (head); \
         pos = n, n = pos->prev)

#define list_for_each_entry(pos, head, member)                \
    for (pos = list_node_to_entry((head)->next, typeof(*pos), member);    \
         prefetch(pos->member.next), &pos->member != (head);     \
         pos = list_node_to_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_prev(pos, head, member)            \
    for (pos = list_node_to_entry((head)->prev, typeof(*pos), member);    \
         prefetch(pos->member.prev), &pos->member != (head);     \
         pos = list_node_to_entry(pos->member.prev, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)            \
    for (pos = list_node_to_entry((head)->next, typeof(*pos), member),    \
        n = list_node_to_entry(pos->member.next, typeof(*pos), member);    \
         &pos->member != (head);                     \
         pos = n, n = list_node_to_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_safe_reverse(pos, n, head, member)        \
    for (pos = list_node_to_entry((head)->prev, typeof(*pos), member),    \
        n = list_node_to_entry(pos->member.prev, typeof(*pos), member);    \
         &pos->member != (head);                     \
         pos = n, n = list_node_to_entry(n->member.prev, typeof(*n), member))

#define list_safe_reset_next(pos, n, member)                \
    n = list_node_to_entry(pos->member.next, typeof(*pos), member)

#endif
