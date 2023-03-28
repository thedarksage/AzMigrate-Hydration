#ifndef _INM_LIST_H
#define _INM_LIST_H

/*
 * While removing an object from the list, the next & prev would
 * be pointed to these poison values.
 */
#define	INM_LIST_POTION1 ((void *)0x00100100)
#define	INM_LIST_POTION2 ((void *)0x00200200)

/*
 * Find the base address of the structure given the address of its member,
 * type of the structure that contains the memeber and the name of
 * the member within the structure
 */
#define inm_container_of(addr, struct_type, struct_member) (                      \
        (struct_type *)((char *)addr - offsetof(struct_type, struct_member)))

typedef struct inm_list_head {
	struct inm_list_head *next, *prev;
}inm_list_head_t;

#define INM_LIST_HEAD_INIT(list) { &(list), &(list) }

#define INM_LIST_HEAD(list) \
	inm_list_head_t list = INM_LIST_HEAD_INIT(list)
/*
 * Initializing the list with next & prev pointers pointing to the
 * head itself.
 */
static inline void
INM_INIT_LIST_HEAD(inm_list_head_t *head)
{
	head->next = head;
	head->prev = head;
}

/*
 * Add the new entry between prev & next entries.
 */
static inline void
__inm_list_add(inm_list_head_t *new_entry, inm_list_head_t *prev_entry, inm_list_head_t *next_entry)
{
	next_entry->prev = new_entry;
	new_entry->next = next_entry;
	new_entry->prev = prev_entry;
	prev_entry->next = new_entry;
}

/*
 * Add the new entry to the list as a next entry at the head.
 */
static inline void
inm_list_add(inm_list_head_t *new_entry, inm_list_head_t *list)
{
	__inm_list_add(new_entry, list, list->next);
}

/*
 * Add the new entry to the list as a previous entry at the head.
 */
static inline void
inm_list_add_tail(inm_list_head_t *new_entry, inm_list_head_t *list)
{
	__inm_list_add(new_entry, list->prev, list);
}

/*
 * Removing an entry by poitning prev & next point to each other.
 */
static inline void
__inm_list_del(inm_list_head_t * prev_entry, inm_list_head_t * next_entry)
{
	next_entry->prev = prev_entry;
	prev_entry->next = next_entry;
}

/*
 * Removes the entry from the list and next & prev will be set to
 * potion values.
 */
static inline void
inm_list_del(inm_list_head_t *ptr)
{
	__inm_list_del(ptr->prev, ptr->next);
	ptr->next = INM_LIST_POTION1;
	ptr->prev = INM_LIST_POTION2;
}

/*
 * Check if the list is empty.
 */
static inline int
inm_list_empty(const inm_list_head_t *list)
{
	return list->next == list;
}

/*
 * Returns the address of the struct ontaining the entry.
 */
#define inm_list_entry(addr, struct_type, struct_member) \
	inm_container_of(addr, struct_type, struct_member)

/*
 * Returns the address of the struct for the first entry.
 */
#define inm_list_first_entry(addr, struct_type, struct_member) \
	inm_list_entry((addr)->next, struct_type, struct_member)

/*
 * Traverse the list
 */
#define __inm_list_for_each(idx, list) \
	for (idx = (list)->next; idx != (list); idx = idx->next)

/*
 * Traverse the list safely against deletion of any entry.
 */
#define inm_list_for_each_safe(idx, temp, list) \
	for (idx = (list)->next, temp = idx->next; idx != (list); \
		idx = temp, temp = idx->next)

/*
 * Traverse the list
 */
#define inm_list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#endif /* _INM_LIST_H */
