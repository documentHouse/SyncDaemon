#ifdef DEBUG
#include <stdio.h>
#endif

#include "uthash.h"
#include "tracker.h"

typedef struct inotify_tracker {
  int id;
  char *name;
  UT_hash_handle hh;
} inotify_tracker;

static inotify_tracker *trackers = NULL;

void add_tracker(int wd, char *path) {
  inotify_tracker *tracker = (inotify_tracker *)malloc(sizeof(inotify_tracker));
  tracker->id = wd;
  tracker->name = path;
  HASH_ADD_INT(trackers, id, tracker);

#ifdef DEBUG
  printf("%s: wd: %d, path: %s\n", __PRETTY_FUNCTION__, wd, tracker->name);
#endif
}

char *get_tracker(int wd) {
  inotify_tracker *tracker;
  HASH_FIND_INT(trackers, &wd, tracker);
#ifdef DEBUG
  printf("%s: tracker->name: %s\n", __PRETTY_FUNCTION__, tracker->name);
#endif
  return tracker->name;
}

char *delete_tracker(int wd) {
  inotify_tracker *tracker;
  HASH_FIND_INT(trackers, &wd, tracker);
  char *path = tracker->name;

  HASH_DEL(trackers, tracker);
  free(tracker);

  return path;
}
