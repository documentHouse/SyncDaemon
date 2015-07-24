#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
// To check for the existence of the mounted harddrive
#include <sys/types.h>
#include <sys/stat.h>
// The file watching data types, inotify
#include <sys/inotify.h>
// FD_ZERO, FD_SET, and select to watch the inotify file descriptor
#include <sys/select.h>
// Threading
#include <pthread.h>
// 
#include <sys/types.h>
#include <fts.h>
#include <err.h>

// Hash table that associates descriptors with their paths
#include "uthash.h"

#include "bool.h"
#include "list.h"
#include "informer.h"
#include "tracker.h"

#include "configuration.h"

#ifdef DEBUG
#define print(...) printf(__VA_ARGS__)
#else
#define print(...)
#endif

#define LOG_FILENAME          "sync_daemon.log"
#define CONFIG_FILENAME       "driveSync.conf"

#define NOTIFY_TIME_SECONDS          3
static const int NOTIFY_TIME = NOTIFY_TIME_SECONDS * 1000000;

#define SUCCESS                      0
#define UNKNOWN_ERROR                1
#define BAD_USAGE                    2
#define LOG_OPEN_FAILURE             3
#define FORKING_FAILED               4
#define CHANGING_SID_FAILED          5
#define CHANGING_DIR_FAILED          6
#define CREATING_SYNC_CMD_FAILED     7
#define CREATING_RESP_BUF_FAILED     8
#define INIT_FILE_WATCHING_FAILED    9
#define SETUP_FILE_WATCHING_FAILED   10
#define SETUP_TIMER_HANDLER_FAILED   11
#define MEMORY_ALLOCATION_FAILED     12


static FILE *log_file                   = NULL;

static size_t command_length            = 1024;
static size_t response_length           = 1024;

void *sync_thread(void *data) {

  hardware_config_t *hardware_conf = (hardware_config_t *)data;

  static char *command_buffer = NULL;
  if (!command_buffer) {
    command_buffer = (char *)malloc(command_length * sizeof(char));
    if (!command_buffer) {
      fprintf(log_file, "Failure creating the command buffer for rsync. Terminating Daemon.\n");
      exit(CREATING_SYNC_CMD_FAILED);
    }
    fprintf(log_file, "The command buffer exists.\n");
    memset(command_buffer, 0, command_length);
  }
   
  static char *response_buffer = NULL;
  if (!response_buffer) {
    response_buffer = (char *)malloc(response_length * sizeof(char));
    if (!response_buffer) {
      fprintf(log_file, "Failure creating the response buffer for rsync. Terminating Daemon.\n");
      exit(CREATING_RESP_BUF_FAILED);
    }
    fprintf(log_file, "The command buffer exists.\n");
    memset(response_buffer, 0, response_length);
  }

  char *master_mount = get_master_repo_mount(hardware_conf);
  char *slave_mount = get_slave_repo_mount(hardware_conf);  

  // Create the command to sync the drives
  /* snprintf(command_buffer, command_length, "rsync -avz --inplace --delete-after %s/ %s/", hardware_conf->master_repo_mount, hardware_conf->slave_repo_mount); */
  snprintf(command_buffer, command_length, "rsync -avz --inplace --delete-after %s/ %s/", master_mount, slave_mount);  
  /* snprintf(command_buffer, command_length, "ls"); */

  fprintf(log_file, "The command: %s\n", command_buffer);

  FILE *command_stream = popen(command_buffer,"r");

  if (!command_stream)
    fprintf(log_file, "Couldn't create command stream. Error: %s\n", strerror(errno));

  while (fgets(response_buffer, response_length, command_stream) != NULL) {
    fprintf(log_file, "%s", response_buffer);
  }
    
  int retVal = pclose(command_stream) >> 8;
  // Something bad happened while trying to sync the drives
  if (retVal != 0) {
    // Received SIGUSR1 or SIGINT signal
    if (retVal == 20)
      fprintf(log_file, "Syncing halted due to signal received.\n");
    // There was a different issue
    else
      fprintf(log_file, "Syncing failed. Rsync error = %d\n", retVal);
  }

  return NULL;
}

void sig_handler(int signum) {

  if (signum == SIGTERM) {
    if (log_file) {
      fprintf(log_file, "Received terminate signal.\n");
      fflush(log_file);
      fclose(log_file);
      exit(SUCCESS);
    }
  }
  else if (signum == SIGINT) {
    if (log_file) fprintf(log_file, "Received interrupt signal.\n");
  }
  else {
    if (log_file) fprintf(log_file, "Signal with number %d, is not handled by the daemon.\n", signum);
  }
}

int add_to_queue(list_t *list, uint8_t *buf, const int buf_size, ssize_t bytes_read) {

  struct inotify_event *event = NULL;
  const static int event_size = sizeof(struct inotify_event);
  uint32_t name_length = 0;
  uint8_t *pos = buf;
  int num_events = 0;
  
  ssize_t processed_bytes = 0;

  if (event_size * (bytes_read / event_size) != bytes_read)
    fprintf(log_file, "Sanity check failed: The number of bytes in the queue contains a partial event. Bytes = %zd\n", bytes_read);

  while (processed_bytes < bytes_read) {
    name_length = ((struct inotify_event *)pos)->len;
    event = (struct inotify_event *) malloc (event_size + name_length);

    print("event struct + length = %d\n", event_size + name_length);
    memcpy(event, pos, event_size + name_length);
    
    pos += event_size + name_length;
    processed_bytes += event_size + name_length;
    print("processed_bytes = %zd, bytes_read = %zd, event_size = %d\n", processed_bytes, bytes_read, event_size);

    append_item(list, event);

    num_events++;

    if (event->mask & IN_MODIFY)
      print("Inotify: Modify event.\n");
    else if (event->mask & IN_ATTRIB)
      print("Inotify: Attribute event.\n");
    else if (event->mask & IN_MOVED_FROM) {
      print("Inotify: Move out event.\n");
      print("Event name length: %d\n", event->len);
      print("Event: %s\n", event->name);
      print("Tracker path: %s, wd = %d\n", get_tracker(event->wd), event->wd);
    }
    else if (event->mask & IN_MOVED_TO) {
      print("Inotify: Move in event.\n");
      print("Event name length: %d\n", event->len);
      print("Event: %s\n", event->name);
      print("Tracker path: %s, wd = %d\n", get_tracker(event->wd), event->wd);
    }
    else if (event->mask & IN_CREATE) {
      print("Inotify: Create event.\n");
      print("Event name length: %d\n", event->len);
      print("Event: %s\n", event->name);
    }
    else if (event->mask & IN_DELETE) {
      print("Inotify: Delete event.\n");
    }
    else {
      print("Unknown event.\n");
    }
  }

  return num_events;
}

int printer(void *elem, void *aux) {
  print("%s\n", (char *)elem);
  return 0;
}

typedef struct inotify_info_t {
  int inotify_fd;
  int mask;
} inotify_info_t;

int add_watcher(void *path, void *aux) {

  inotify_info_t *info = (inotify_info_t *)aux;
  int inotify_fd = info->inotify_fd;
  int mask = info->mask;

  char *copied_path = strdup(path);

  int wd;
  if (!copied_path) {
    fprintf(log_file, "Path to watched file could not be copied. strdup failed with error = %d\n.", errno);
    wd = -1;
  }
  else
    wd = inotify_add_watch(inotify_fd, copied_path, mask);

  if (wd < 0)
    return SETUP_FILE_WATCHING_FAILED;

#ifdef DEBUG
  printf("%s: path = %s, wd = %d, fd = %d\n", __PRETTY_FUNCTION__, copied_path, wd, inotify_fd);
#endif

  add_tracker(wd, copied_path);

  return 0;
}

/* Compare files by name. */
int entry_cmp(const FTSENT **a, const FTSENT **b)
{
  return strcmp((*a)->fts_name, (*b)->fts_name);
}
 
/*
 Start watching all subdirectories of the mount point
 */
void add_subdir_watchers(char *dir, int inotify_fd, int inotify_mask)
{

  FTSENT *f;
  char *main_dir[] = { dir, NULL };
 
  list_t *dir_list = create_list();

  FTS *tree = fts_open(main_dir, FTS_LOGICAL | FTS_NOSTAT, entry_cmp);
	  
  bool has_skipped_first = false;

  char *item = NULL;
  int path_length = 0;
  while ((f = fts_read(tree))) {

    // The first directory returned is the current directory which
    // we already have a watcher for
    if (!has_skipped_first) {
      has_skipped_first = true;
      continue;
    }
      
    switch (f->fts_info) {
    case FTS_D:    // We have a directory
      path_length = strlen(f->fts_path);
      /* item = (char *) malloc((path_length+1) * sizeof(char)); */
      /* strcpy(item, f->fts_path); */
      item = strdup(f->fts_path);
      print("Dir item: %s\n", item);
      append_item(dir_list, item);
      break;
    case FTS_DNR:  // Cannot read directory
      break;
    case FTS_ERR:  // Miscellaneous error
      fprintf(log_file, "There was a general error at %s\n", f->fts_path);
      break;
    case FTS_NS:
      fprintf(log_file, "There was a stat error at %s\n", f->fts_path);
      continue;
    default:
      continue;
    }
 
    // Symbolic link cycle
    if (f->fts_info == FTS_DC)
      fprintf(log_file, "%s: cycle in directory tree", f->fts_path);
  }
 
  /* fts_read() sets errno = 0 unless it has error. */
  if (errno != 0)
    fprintf(log_file, "fts_read failed.");
  else {
    print("Iterating...\n");
    iterate(dir_list, printer, NULL);

    inotify_info_t inotify_info;
    inotify_info.inotify_fd = inotify_fd;
    inotify_info.mask = inotify_mask;
    int watcher_error = 0;

    if ((watcher_error = iterate(dir_list, add_watcher, &inotify_info)) != 0) {
      if (watcher_error != -1) {
	fprintf(log_file, "There was problem adding file watchers. Terminating Daemon.\n");
	exit(watcher_error);
      }
    }

    destroy_list_and_items(dir_list);
  }

  if (fts_close(tree) < 0)
    fprintf(log_file, "fts_close failed.\n");
}
 
/* int main() */
/* { */
/*   load_configuration(); */
/*   add_subdir_watchers(hardware_conf->master_repo_mount, 0, 0); */
/*   return 0; */
/* } */


/* void daemonize() { */

/*  // Begin daemonizing this process */
/*   pid_t process_id = 0; */
/*   pid_t sid = 0; */

/*   process_id = fork(); */

/*   if (process_id < 0) { */
/*     printf("Forking failed. Sync daemon can not be started.\n"); */
/*     exit(FORKING_FAILED); */
/*   } */
/*   if (process_id > 0) { */
/*     exit(SUCCESS); */
/*   } */

/*   // Allow the child process to create files that are writable */
/*   umask(0); */

/*   log_file = fopen(LOG_FILENAME, "a"); */

/*   if (!log_file) { */
/*     fprintf(stderr, "Could not open the log file. Aborting daemon.\n "); */
/*     exit(LOG_OPEN_FAILURE); */
/*   } */

/* #ifdef DEBUG */
/*   log_file = stdout; */
/* #endif */

/*   if (signal(SIGTERM, sig_handler) == SIG_ERR) */
/*     fprintf(log_file, "Can't register handler for SIGTERM"); */

/*   if (signal(SIGINT, sig_handler) == SIG_ERR) */
/*     fprintf(log_file, "Can't register handler for SIGINT"); */

/*   sid = setsid(); */
/*   if (sid < 0) { */
/*     fprintf(log_file, "Session id could not created. Terminating Daemon.\n"); */
/*     exit(CREATING_SID_FAILED); */
/*   } */

/*   if (chdir("/") < 0) { */
/*     fprintf(log_file, "Changing the directory to root failed. Terminating Daemon.\n"); */
/*     exit(CHANGING_DIR_FAILED); */
/*   } */
  
/* #ifndef DEBUG */
/*   close(STDIN_FILENO); */
/*   close(STDOUT_FILENO); */
/*   close(STDERR_FILENO); */
/* #endif */
/* } */

void daemonize() {

  // In debugging mode we just print to standard out
#ifdef DEBUG
  log_file = stdout;
#else
  log_file = fopen(LOG_FILENAME, "a");

  if (!log_file) {
    fprintf(stderr, "Could not open the log file. Aborting daemon.\n ");
    exit(LOG_OPEN_FAILURE);
  }
#endif
  
#ifdef DEBUG
  bool dont_redirect_inout = true;
  bool dont_change_to_root = false;
#else
  bool dont_redirect_inout = false; 
  bool dont_change_to_root = false;
#endif

  if (daemon(dont_change_to_root, dont_redirect_inout) != 0) {
    fprintf(log_file, "Process couldn't be daemonized.\n");

    if (errno == EAGAIN) 
      fprintf(log_file, "fork failed: EAGAIN.\n");
    else if (errno == ENOMEM)
      fprintf(log_file, "fork failed: ENOMEM.\n");
    else if (errno == ENOSYS)
      fprintf(log_file, "fork failed: ENOSYS.\n");    
    else if (errno == EPERM)
      fprintf(log_file, "Setsid failed: EPERM.\n");
    else
      fprintf(log_file, "Unknown error.\n");

    if ((errno == EAGAIN) || (errno == ENOMEM) || (errno == ENOSYS))
      exit(FORKING_FAILED);
    else if (errno == EPERM)
      exit(CHANGING_SID_FAILED);
    else
      exit(UNKNOWN_ERROR);
  }
}

int main(int argc, char *argv[]) {

  printf("Starting Sync Daemon...\n");

  daemonize();

  hardware_config_t *hardware_conf = NULL;

  // You can't write to the root directory because of a permission error.
  /* /\* char *writer_file = "/home/andy/Development/Lang/C/SyncDaemon/writerfile.conf"; *\/ */
  /* char *writer_file = "writerfile.conf";   */
  /* FILE *file = fopen(writer_file, "w+"); */
  /* if (!file) { */
  /*   fprintf(log_file, "Why can't I open this file?\n"); */
  /*   fprintf(log_file, "Errno = %d. Error string = %s?\n", errno, strerror(errno)); */
  /*   exit(1); */
  /* } */
  /* fprintf(file, "This is just some basic text that I'm going to write.\n\n42.\n"); */
  /* fclose(file); */

  int config_error;
  char *configuration_file = "/home/andy/Development/Lang/C/SyncDaemon/driveSync.conf";
  
  if ((config_error = load_configuration(configuration_file, &hardware_conf)) != CONFIG_SUCCESS) {
    fprintf(log_file, "Config error = %d\n", config_error);
    fprintf(log_file, "The hardrive sync daemon could not be configured. Terminating daemon.\n");
    exit(BAD_USAGE);
  }

  // Use inotify to monitor changes in the file system at the mount point
  int inotify_fd = inotify_init();

  if (inotify_fd < 0) {
    fprintf(log_file, "inotify_init failed so we can't watch for changes. Terminating Daemon.\n");
    exit(INIT_FILE_WATCHING_FAILED);
  }

  int attributes_mask = IN_MODIFY | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE;

  // Add the root directory of the file storage to the directories being watched
  char *master_repo_mount = get_master_repo_mount(hardware_conf);
  int wd = inotify_add_watch(inotify_fd, master_repo_mount, attributes_mask);

  if (wd < 0) {
    fprintf(log_file, "inotify_add_watch failed so we can't watch for changes. Terminating Daemon.\n");
    exit(SETUP_FILE_WATCHING_FAILED);
  }

  add_tracker(wd, master_repo_mount);

  fprintf(log_file, "Monitored mount point = %s\n", master_repo_mount);

  // Watch all the subdirectories of the mount point
  add_subdir_watchers(master_repo_mount, inotify_fd, attributes_mask);

  bool monitoring = true;
  // The file descriptors that are being watched
  fd_set inotify_set;

  // Buffer for storing file system events as they are read
  const static int EVENT_BUF_SIZE = 512 * sizeof(struct inotify_event); // 16382 = 2^14
  uint8_t buf[EVENT_BUF_SIZE];
  int read_bytes = 0;

  // A queue for storing file system events as they become available. Currently this is just location
  // to hold the events but nothing is done with them other than to remind the program it's time to sync
  list_t *event_list = create_list();

  while (monitoring) {

    FD_ZERO(&inotify_set);
    FD_SET(inotify_fd, &inotify_set);

    // Wait for filesystem events at the location 'master_repo_mount'
    if (select(inotify_fd+1, &inotify_set, NULL, NULL, NULL) < 0) {
      fprintf(log_file, "Select failed.\n");
      if (errno == EINTR) {
	fprintf(log_file, "Interrupted. Will start waiting for changes again.\n");
	continue;
      }
      else if(errno == ENOMEM) {
	fprintf(log_file, "Select could not allocate memory to continue operating. Terminating Daemon.\n");
	exit(MEMORY_ALLOCATION_FAILED);
      }
    }

    // Read file system events from the inotify file descriptor
    read_bytes = read(inotify_fd, buf, EVENT_BUF_SIZE);
    fprintf(log_file, "read_bytes = %d\n", read_bytes);
    if (read_bytes < 0) {
      fprintf(log_file, "Could not read events.\n");
      fprintf(log_file, "error = %d, message = %s\n", errno, strerror(errno));
    }
    else if (read_bytes == 0);
    else {
      int num_events = add_to_queue(event_list, buf, EVENT_BUF_SIZE, read_bytes);
      if (num_events > 0) {
	// For now, we just get rid of the file system events and note that it's time to sync
	clear(event_list);
	informer(NOTIFY_TIME, sync_thread, hardware_conf);
      }
    }
  }

  fclose(log_file);

  exit(SUCCESS);
}
