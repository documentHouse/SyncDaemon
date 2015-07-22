#ifndef LIST_H
#define LIST_H

#include "bool.h"

typedef struct list_t list_t;

list_t *create_list();

void destroy_list(list_t *list);

void destroy_list_and_items(list_t *list);

void append_item(list_t *list, void *item);

void iterate(list_t *list, void (*process)(void *elem, void *aux), void *aux);

void *front(list_t *list);

void *remove_front(list_t *list);

int list_size(list_t *list);

void clear(list_t *list);

bool is_empty(list_t *list);

#endif
