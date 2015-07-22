#include <stdlib.h>
#include <stdio.h>
#include "bool.h"
#include "list.h"

typedef struct node_t {
  void *value;
  struct node_t *next;
} node_t;

struct list_t {
  int size;
  node_t *head;
  node_t *tail;
};

list_t *create_list() {

  list_t *aList = (list_t *) malloc(sizeof(list_t));
  aList->size = 0;
  aList->head = NULL;
  aList->tail = NULL;
  
  return aList;
};

static void private_destroy(list_t *list, bool destroy_items) {

  list->size = 0;
  node_t *curr = list->head;
  node_t *temp = NULL;

  while (curr) {
    temp = curr;
    curr = curr->next;
    if (destroy_items)
      free(temp->value);
    free(temp);
  }

  list->head = NULL;
  list->tail = NULL;

  free(list);
}

void destroy_list(list_t *list) {

  private_destroy(list, false);
};

void destroy_list_and_items(list_t *list) {

  private_destroy(list, true);
}

void append_item(list_t *list, void *item) {

  node_t *list_item = (node_t *) malloc (sizeof(node_t));
  list_item->value = item;
  list_item->next = NULL;

  if (!list->head) {
    list->head = list_item;
    list->tail = list_item;
  }
  else {
    list->tail->next = list_item;
    list->tail = list_item;
  }

  list->size++;
}

void iterate(list_t *list, void (*process)(void *elem, void *aux), void *aux) {

  if ((!list) || (is_empty(list)))
    return;
  
  node_t *curr = list->head;
  while (curr) {
    process(curr->value,aux);
    curr = curr->next;
  }
}

void *front(list_t *list) {

  return (list->head) ? list->head->value : NULL;
}

void *remove_front(list_t *list) {
  if (!list->head)
    return NULL;
  
  void *value = list->head->value;
  
  node_t *temp = list->head;

  list->head = list->head->next;

  temp->next = NULL;
  free(temp);

  if (list->head == NULL)
    list->tail = NULL;
 
  list->size--;
  return value;
}

int list_size(list_t *list) {

  return list->size;
}

void clear(list_t *list) {

  node_t *curr = list->head;
  node_t *temp = NULL;

  while(curr) {
    temp = curr;
    curr = curr->next;

    free(temp->value);    
    free(temp);
  }

  list->head = NULL;
  list->tail = NULL;

  list->size = 0;
}

bool is_empty(list_t *list) {

  return list->head == NULL;
}
