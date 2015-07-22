#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "informer.h"
#include "bool.h"

struct informer_t {};

#define print(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

typedef void *(*inform_func)(void *);

typedef struct runner_data_t {
  int sleep_time; // microseconds
  inform_func func;
  void *data;
  pthread_t current_thread;
} runner_data_t;

static pthread_mutex_t set_processing_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t runner_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool is_processing = false;

static runner_data_t runner_data = { .sleep_time     = 0,
				      .func           = NULL,
				      .data           = NULL,
				      .current_thread = 0 };

/* static void set_runner_data(int sleep_time, inform_func func, void *data) { */
/*   pthread_mutex_lock(&runner_data.runner_data_mutex); */
/*   runner_data.sleep_time = sleep_time; */
/*   runner_data.func = func; */
/*   runner_data.data = data; */
/*   pthread_mutex_unlock(&runner_data.runner_data_mutex); */
/* } */

// If cancel is called on thread which is processing the function 'inform'
// then we want to release the lock so that the current call to 'informer'
// may proceed
static void inform_cleanup(void *arg) {

  print("inform_cleanup...\n");
  is_processing = false;

  pthread_mutex_unlock(&runner_data_mutex);
}

static void *inform_runner(void *arg) {

  pthread_cleanup_push(inform_cleanup, NULL);

  is_processing = true;
  
  // Let the user on the main thread try to cancel 
  pthread_mutex_unlock(&set_processing_lock);

  void *data = runner_data.data;
  inform_func inform = runner_data.func;
  
  // Delay the function call for 'sleep_time' as specified by the user in 'informer'
  usleep(runner_data.sleep_time);
  
  // Call the function
  inform(data);

  pthread_cleanup_pop(true);

  return NULL;
}

/*
  Possible future expansion/modification would be to use the following signature:
  int informer(int sleep_time, void *(*inform)(void *), void *data, bool (data_cmp)(void*, void*));
  With the when the user calls 'informer' with the same function and same data as determined
  by the data_cmp function, than the previous thread is automatically cancelled. If either
  the function 'inform' or the data are different (again, as determined by the data_cmp
  function) then just create a new thread with and continue independently. If you wanted
  to use the same function but different and still interrupt the previous execution than a 
  call to cancel_informer would need to be made
 */


int informer(int sleep_time, void *(*inform)(void *), void *data) {

  int err = 0;

  // Don't allow progress in this function until is_processing is
  // set or the cancellation of the previous call is finished up
  if ((err = pthread_mutex_lock(&set_processing_lock)) != 0)
    return err;

  // If we're already processing a function than cancel and start anew
  if (is_processing)
    pthread_cancel(runner_data.current_thread);

  // We don't want the user to be able to modify the function or the
  // data it works until we've stopped processing of the previous function
  if ((err = pthread_mutex_lock(&runner_data_mutex)) != 0) {
    pthread_mutex_unlock(&set_processing_lock);
    return err;
  }
  
  // Add the user data
  // These are global because we will need this information in
  // the unimplemented functions 'restart_informer' and 'cancel_informer' below
  runner_data.sleep_time = sleep_time;
  runner_data.func = inform;
  runner_data.data = data;

  /* Start a thread and then send it a cancellation request */
  if ((err = pthread_create(&runner_data.current_thread, NULL, &inform_runner, NULL)) != 0) {
    pthread_mutex_unlock(&set_processing_lock);
    pthread_mutex_unlock(&runner_data_mutex);
    return err;
  }

  if ((err = pthread_detach(runner_data.current_thread)) == 0)
    return err;

  return 0;
}

/* static pthread_mutex_t restart_sync = PTHREAD_MUTEX_INITIALIZER; */
void restart_informer() {
  // TODO: Not implemented
}

void cancel_informer() {
  // TODO: Not implemented
}

















