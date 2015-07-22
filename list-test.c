#include <stdlib.h>
#include <stdio.h>
#include "list.h"

void printer(void *elem, void *aux) {

  printf("%d ", *(int *)elem);
}

void sumer(void *elem, void *aux) {

  *(int*)aux += *(int*)elem;
}

int main() {

  printf("Starting the list test...\n");

  int *a = (int *) malloc (sizeof(int));
  int *b = (int *) malloc (sizeof(int));
  int *c = (int *) malloc (sizeof(int));

  *a = 5;
  *b = 7;
  *c = 3;

  list_t *alist = create_list();
  append_item(alist, (void *)a);
  append_item(alist, (void *)b);
  append_item(alist, (void *)c);

  iterate(alist, printer, NULL);
  
  int sum = 0;
  iterate(alist, sumer, &sum);

  printf("Here is the sum: %d\n", sum);

  int *val = remove_front(alist);
  printf("The val: %d\n", *val);
  if (is_empty(alist))
    printf("The list is empty\n");
  else
    printf("The list is not empty\n");

  val = remove_front(alist);
  printf("The val: %d\n", *val);

  if (is_empty(alist))
    printf("The list is empty\n");
  else
    printf("The list is not empty\n");

  val = remove_front(alist);
  printf("The val: %d\n", *val);

  if (is_empty(alist))
    printf("The list is empty\n");
  else
    printf("The list is not empty\n");

  destroy_list_and_items(alist);

  printf("End.\n");

  return 0;

}
