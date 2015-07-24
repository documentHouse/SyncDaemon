#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tracker.h"

/* void add_tracker(int wd, char *path); */
/* char *get_tracker(int wd); */
/* void delete_tracker(int wd); */

int main () {

  /* char *path1 = "/home/andy/mydir"; */
  /* char *path2 = "/home/andy/thoughts"; */

  int path1Len = strlen("/home/andy/mydir");
  int path2Len = strlen("/home/andy/thoughts");
  char *path1 = (char *)malloc(path1Len + 1);
  char *path2 = (char *)malloc(path2Len + 1);
  path1[path1Len] = '\0';
  path2[path2Len] = '\0';
  
  memcpy(path1,"/home/andy/mydir", path1Len);
  memcpy(path2,"/home/andy/thoughts", path2Len);  

  int wd1 = 5;
  int wd2 = 7;

  add_tracker(wd1, path1);
  add_tracker(wd2, path2);

  printf("Second path: %s\n", get_tracker(wd2));

  char *path = delete_tracker(wd2);
  printf("returned path = %s\n", path);

  printf("first path: %s\n", get_tracker(wd1));

  delete_tracker(wd1);

  free(path2);

  return 0;

}
