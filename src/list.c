#include "list.h"
#include <stdio.h>
#include <string.h>

void list_destroy(list *list) {
	node *cur = list->head;
	node *temp;
	while (cur != NULL) {
		temp = cur;
		cur = cur->next;
		free(temp->value);
		free(temp);
	}
	free(list);
}

list *list_new(const void *first_val, size_t size) {
	list *l = calloc(1, sizeof *l);
	l->head = calloc(1, sizeof(node));
	l->tail = l->head;
	// copy arbitraty value into the location
	l->head->value = malloc(size);
	l->head->size = size;
	memcpy(l->head->value, first_val, size);
	return l;
}

list *list_append(list *l, const void *newval, size_t size) {
	l->tail->next = calloc(1, sizeof(node));
	l->tail = l->tail->next;
	l->tail->value = malloc(size);
	l->tail->size = size;
	memcpy(l->tail->value, newval, size);
	return l;
}

void list_print(list *l) {
	size_t index = 0;
	node *cur = l->head;
	while (cur != NULL) {
		printf("%lu\n", index++);
		cur = cur->next;
	}
}

size_t list_len(list *l) {
	node *cur = l->head;
	size_t len = 0;
	while (cur != NULL) {
		cur = cur->next;
		len++;
	}
	return len;
}
