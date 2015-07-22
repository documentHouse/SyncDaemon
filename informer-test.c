#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "informer.h"

static int n = 0;

void *my_inform_func(void *data) {

  printf("Inform function...\n");

  n++;
   while (1) {
     usleep(1000000);
     printf("Informing with id: %d\n", n);
     fflush(stdout);
   }

  return NULL;
}


int main(int argc, char *argv[]) {

  int seconds = 2;

  for (int i = 0; i < 5; i++) {
    informer(seconds * 1000000, &my_inform_func, NULL);
    usleep(1000000);
  }
  sleep(5);
  
  return 0;
}

/* int main(int argc, char *argv[]) { */

/*   int seconds = 1; */

/*   informer(seconds * 1000000, &my_inform_func, NULL); */
/*   sleep(5); */

/*   informer(seconds * 1000000, &my_inform_func, NULL); */
/*   sleep(5); */

/*   informer(seconds * 1000000, &my_inform_func, NULL); */
/*   sleep(5); */

/*   informer(seconds * 1000000, &my_inform_func, NULL); */
  
/*   return 0; */
/* } */


/* int main(int argc, char *argv[]) { */

/*   printf("Hey.\n"); */

/*   int seconds = 1; */

/*   informer(seconds * 1000000, &my_inform_func, NULL); */

/*   int i = 0; */
/*   while (1) { */
/*     usleep(500000); */
/*     printf("Processing: %d...\n", i++); */
/*     fflush(stdout); */

/*     if (i % 13 == 0) { */
/*       printf("Calling the function again...\n"); */
/*       fflush(stdout); */
/*       informer(seconds * 1000000, &my_inform_func, NULL); */
/*     } */
/*   } */

/*   return 0; */
/* } */

