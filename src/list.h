#ifndef LIST
#define LIST

#include <stdlib.h>

typedef struct node {
	void *value;
	size_t size;
	struct node *next;
} node;

typedef struct list {
	node *head;
	node *tail;
} list;

// Free list and all associated values
void list_destroy(list *list);
// Create a new list with a first initial value and size copied into the list
list *list_new(const void *first_val, size_t size);
// Append a value, will be copied into the list
list *list_append(list *l, const void *newval, size_t size);
// Print list items
void list_print(list *l);
// Length of list
size_t list_len(list *l);
#endif
