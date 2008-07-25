#if !defined LIST_H
#define LIST_H

/* Put this inside a structure to build a linked list of the structures. */
struct list_node {
	struct list_node *next;
};

/* Attach to a variable to initialize as an empty list. */
#define LIST_EMPTY_NODE(name) {&(name)}

/* Adds a new element to a linked list. */
static inline void list_add(struct list_node *new_node, struct list_node *head_node) {
	new_node->next = head_node->next;
	head_node->next = new_node;
}

/* Removes an element from a linked list. */
static inline void list_del(struct list_node *node, struct list_node *prev) {
	prev->next = node->next;
	node->next = 0;
}

/* Gets the structure containing the list_node. */
#define list_entry(node, type, node_member) ({\
	const typeof(((type *) 0)->node_member) *__mptr = (node);\
	(type *) ((char *) __mptr - offsetof(type, node_member));})

/* Iterates over a list. The current element may be deleted. */
#define list_for_each(list, cur, prev) \
	for (prev = &list, cur = prev->next; cur != &list; prev = (prev->next == cur ? cur : prev), cur = prev->next)

#endif

