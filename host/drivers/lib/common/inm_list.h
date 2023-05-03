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
 * It is going to replace the old entry by the new entry.
 */
static inline void
inm_list_replace(inm_list_head_t *old_entry, inm_list_head_t *new_entry)
{
	new_entry->next = old_entry->next;
	new_entry->next->prev = new_entry;
	new_entry->prev = old_entry->prev;
	new_entry->prev->next = new_entry;
}

static inline void
inm_list_replace_init(inm_list_head_t *old_entry, inm_list_head_t *new_entry)
{
	inm_list_replace(old_entry, new_entry);
	INM_INIT_LIST_HEAD(old_entry);
}

/*
 * Remove the ntry from the list and initialize it again.
 */
static inline void
inm_list_del_init(inm_list_head_t *list_entry)
{
	__inm_list_del(list_entry->prev, list_entry->next);
	INM_INIT_LIST_HEAD(list_entry);
}

/*
 * Remove the entry from one list and insert it in another list at the head.
 */
static inline void
inm_list_move(inm_list_head_t *entry, inm_list_head_t *list)
{
        __inm_list_del(entry->prev, entry->next);
        inm_list_add(entry, list);
}

/*
 * Check if the entry @list is the last entry in the list.
 */
static inline int
inm_list_is_last(const inm_list_head_t *entry, const inm_list_head_t *list)
{
	return entry->next == list;
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
 * Check if the list is empty and is not under modification.
 */
static inline int
inm_list_empty_careful(const inm_list_head_t *list)
{
	inm_list_head_t *entry = list->next;
	return (entry == list) && (entry == list->prev);
}

static inline void
__inm_list_splice(inm_list_head_t *new_list, inm_list_head_t *list)
{
	inm_list_head_t *first_entry = new_list->next;
	inm_list_head_t *last_entry = new_list->prev;
	inm_list_head_t *at_entry = list->next;

	first_entry->prev = list;
	list->next = first_entry;

	last_entry->next = at_entry;
	at_entry->prev = last_entry;
}

/*
 * Merge the lists.
 */
static inline void
inm_list_splice(inm_list_head_t *new_list, inm_list_head_t *list)
{
	if (!inm_list_empty(new_list))
		__inm_list_splice(new_list, list);
}

/*
 * Merge the lists and initialize the empty list again.
 */
static inline void
inm_list_splice_init(inm_list_head_t *new_list, inm_list_head_t *list)
{
	if (!inm_list_empty(new_list)) {
		__inm_list_splice(new_list, list);
		INM_INIT_LIST_HEAD(new_list);
	}
}

static inline inm_list_head_t *
inm_list_first(inm_list_head_t *head)
{
    return head->next;
}

static inline inm_list_head_t *
inm_list_last(inm_list_head_t *head)
{
    return head->prev;
}

/*
 * Returns the address of the struct ontaining the entry.
 */
#define inm_list_entry(addr, struct_type, struct_member) \
	inm_container_of(addr, struct_type, struct_member)

/*
 * Returns the address of the struct from the head of the list
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

#endif /* _INM_LIST_H */
