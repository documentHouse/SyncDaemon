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


#include "bool.h"
#include "list.h"
#include "informer.h"

#define LOG_FILENAME          "sync_daemon.log"
#define CONFIG_FILENAME       "driveSync.conf"

#define NOTIFY_TIME_SECONDS          2
static const int NOTIFY_TIME = NOTIFY_TIME_SECONDS * 1000000;

#define SUCCESS                      0
#define BAD_USAGE                    1
#define LOG_OPEN_FAILURE             2
#define FORKING_FAILED               3
#define CREATING_SID_FAILED          4
#define CHANGING_DIR_FAILED          5
#define CREATING_SYNC_CMD_FAILED     6
#define CREATING_RESP_BUF_FAILED     7
#define INIT_FILE_WATCHING_FAILED    8
#define SETUP_FILE_WATCHING_FAILED   9
#define SETUP_TIMER_HANDLER_FAILED   10
#define MEMORY_ALLOCATION_FAILED     11

#define CONFIG_SUCCESS               0
#define CONFIG_FILE_ACCESS_FAILURE   1
#define CONFIG_FILE_READ_FAILURE     2
#define CONFIG_FILE_WRITE_FAILURE    3
#define CONFIG_PARSE_ERROR           4
#define CONFIG_MULTI_MASTER_ERROR    5
#define CONFIG_NO_MASTER_ERROR       6

typedef struct hardware_config_t {
  const char *version;
  char *master_repo_uuid;
  char *slave_repo_uuid;
  char *master_repo_mount;
  char *slave_repo_mount;
} hardware_config_t;

const static char *CURRENT_HARDWARE_CONFIG_VERSION = "1.0";
static hardware_config_t *hardware_conf = NULL;

static FILE *log_file                   = NULL;

static size_t command_length            = 1024;
static size_t response_length           = 1024;

void *sync_thread(void *data) {

  static char *command_buffer = NULL;
  if (!command_buffer) {
    command_buffer = (char *) malloc(command_length * sizeof(char));
    if (!command_buffer) {
      fprintf(log_file, "Failure creating the command buffer for rsync. Terminating Daemon.\n");
      exit(CREATING_SYNC_CMD_FAILED);
    }
    fprintf(log_file, "The command buffer exists.\n");
    memset(command_buffer, 0, command_length);
  }
   
  static char *response_buffer = NULL;
  if (!response_buffer) {
    response_buffer = (char *) malloc(response_length * sizeof(char));
    if (!response_buffer) {
      fprintf(log_file, "Failure creating the response buffer for rsync. Terminating Daemon.\n");
      exit(CREATING_RESP_BUF_FAILED);
    }
    fprintf(log_file, "The command buffer exists.\n");
    memset(response_buffer, 0, response_length);
  }

  // Create the command to sync the drives
  snprintf(command_buffer, command_length, "rsync -avz --inplace --delete-after %s/ %s/", hardware_conf->master_repo_mount, hardware_conf->slave_repo_mount);
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

bool isValidMount(int maxMountLength, char *mount) {

  if ((!mount) || (maxMountLength < strlen(mount)+1))
    return false;

  struct stat s;
  int err = stat(mount, &s);
  if(err == -1) {
    // Check if the file/directory doesn't exist
    // if(ENOENT == errno) {}
    return false;
  } else {
    // We have a valid directory
    if(S_ISDIR(s.st_mode)) {
      // TODO: Do some checks to make sure the directory is readable and writeable. Look at the man page for 'access()'
      return true;
    } 
    // File system entry was not a directory
    else
      return false;
  }
}

int add_to_queue(list_t *list, uint8_t *buf, const int buf_size, ssize_t bytes_read) {

  struct inotify_event *event = NULL;
  const static int event_size = sizeof(struct inotify_event);
  uint8_t *pos = buf;
  int num_events = 0;
  
  ssize_t processed_bytes = 0;

  if (event_size * (bytes_read / event_size) != bytes_read)
    fprintf(log_file, "Sanity check failed: The number of bytes in the queue contains a partial event. Bytes = %zd\n", bytes_read);

  while (processed_bytes < bytes_read) {
    event = (struct inotify_event *) malloc (event_size);
    memcpy(event, pos, event_size);

    pos += event_size;
    processed_bytes += event_size;
    /* printf("processed_bytes = %zd, bytes_read = %zd, event_size = %d\n", processed_bytes, bytes_read, event_size); */

    append_item(list, event);

    num_events++;
    if (event->mask & IN_MODIFY)
      printf("Modify event.\n");
    else if (event->mask & IN_ATTRIB)
      printf("Attribute event.\n");
    else if (event->mask & IN_MOVE)
      printf("Move event.\n");
    else if (event->mask & IN_CREATE) {
      printf("Create event.\n");
      printf("Event name length: %d\n", event->len);
      printf("Event: %s\n", event->name);
    }
    else if (event->mask & IN_DELETE)
      printf("Delete event.\n");
    else {
      printf("Different event.\n");
    }
  }

  return num_events;
}

int load_configuration() {

  // TODO: Use a file not in the current location
  FILE *configFile = fopen(CONFIG_FILENAME, "r");

  if (!configFile)
    return CONFIG_FILE_ACCESS_FAILURE;
 
  int config_error = CONFIG_SUCCESS;
  hardware_conf = (hardware_config_t *) malloc(sizeof(hardware_config_t));
  hardware_conf->version = CURRENT_HARDWARE_CONFIG_VERSION;
  hardware_conf->master_repo_mount = NULL;
  hardware_conf->slave_repo_mount = NULL;
      
  const static int MAX_MOUNT_LENGTH = 1024;
  char relationship[16];
  char storagetype[16];
  char mount[MAX_MOUNT_LENGTH];

  int retVal = fscanf(configFile,"%s %s %s", relationship, storagetype, mount);

  if ((retVal != 3) || (retVal == EOF)) {
    free(hardware_conf);
    fclose(configFile);
    return CONFIG_FILE_READ_FAILURE;
  }

  if (strcmp(relationship, "master") == 0) {
    if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	hardware_conf->master_repo_mount = newMount;
      }
      else
	config_error = CONFIG_PARSE_ERROR;
    }
    else
      config_error = CONFIG_PARSE_ERROR;
  }
  else if(strcmp(relationship, "slave") == 0) {
    if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	hardware_conf->slave_repo_mount = newMount;
      }
      else
	config_error = CONFIG_PARSE_ERROR;
    }
    else
      config_error = CONFIG_PARSE_ERROR;
  }
  else
    config_error = CONFIG_PARSE_ERROR;


  if (config_error != CONFIG_SUCCESS) {
    free(hardware_conf->master_repo_mount);
    free(hardware_conf->slave_repo_mount);
    free(hardware_conf);
    fclose(configFile);
    return config_error;
  }

  retVal = fscanf(configFile,"%s %s %s", relationship, storagetype, mount);

  if ((retVal != 3) || (retVal == EOF)) {
    free(hardware_conf);
    fclose(configFile);
    return CONFIG_FILE_READ_FAILURE;
  }

  if (strcmp(relationship, "master") == 0) {
    // For the time being, we can not have more than one master
    if (hardware_conf->master_repo_mount != NULL)
      config_error = CONFIG_MULTI_MASTER_ERROR;
    else if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	hardware_conf->master_repo_mount = newMount;
      }
      else
	config_error = CONFIG_PARSE_ERROR;
    }
    else
      config_error = CONFIG_PARSE_ERROR;
  }
  else if(strcmp(relationship, "slave") == 0) {
    // We haven't found a master drive yet and we're only looking at two drives so there are no masters
    if (hardware_conf->slave_repo_mount != NULL)
      config_error = CONFIG_NO_MASTER_ERROR;
    else if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	hardware_conf->slave_repo_mount = newMount;
      }
      else
	config_error = CONFIG_PARSE_ERROR;
    }
    else
      config_error = CONFIG_PARSE_ERROR;
  }
  else
    config_error = CONFIG_PARSE_ERROR;

  if (config_error != CONFIG_SUCCESS) {
    free(hardware_conf);
    fclose(configFile);
    return config_error;
  }
  return CONFIG_SUCCESS;
}


void printer(void *elem, void *aux) {
  printf("%s\n", (char *)elem);
}

typedef struct inotify_info_t {
  int inotify_fd;
  int mask;
} inotify_info_t;

void add_watcher(void *path, void *aux) {

  inotify_info_t *info = (inotify_info_t *)aux;
  int inotify_fd = info->inotify_fd;
  int mask = info->mask;

  printf("path = %s, fd = %d\n", (char *)path, inotify_fd);
  fflush(stdout);

  inotify_add_watch(inotify_fd, (char *)path, mask);
}

/* Compare files by name. */
int
entry_cmp(const FTSENT **a, const FTSENT **b)
{
  return strcmp((*a)->fts_name, (*b)->fts_name);
}
 
/*
 * Print all files in the directory tree that match the glob pattern.
 * Example: pmatch("/usr/src", "*.c");
 */
void add_subdir_watchers(char *dir, int inotify_fd, int inotify_mask)
{

  FTSENT *f;
  char *main_dir[] = { dir, NULL };
 
  list_t *dir_list = create_list();

  FTS *tree = fts_open(main_dir, FTS_LOGICAL | FTS_NOSTAT, entry_cmp);
	  
  bool has_skipped_first = false;

  char *item = NULL;
  while ((f = fts_read(tree))) {

    // The first directory returned is the current directory which
    // we already have a watcher for
    if (!has_skipped_first) {
      has_skipped_first = true;
      continue;
    }
      
    switch (f->fts_info) {
    case FTS_D:    // We have a directory
      item = (char *) malloc((strlen(f->fts_path)+1) * sizeof(char));
      strcpy(item, f->fts_path);
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
    iterate(dir_list, printer, NULL);

    inotify_info_t inotify_info;
    inotify_info.inotify_fd = inotify_fd;
    inotify_info.mask = inotify_mask;
    iterate(dir_list, add_watcher, &inotify_info);
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


int main(int argc, char *argv[]) {

  printf("Starting Sync Daemon...\n");

  // Begin daemonizing this process
  pid_t process_id = 0;
  pid_t sid = 0;

  process_id = fork();

  if (process_id < 0) {
    printf("Forking failed. Sync daemon can not be started.\n");
    exit(FORKING_FAILED);
  }
  if (process_id > 0) {
    exit(SUCCESS);
  }

  // Allow the child process to create files that are writable
  umask(0);

  log_file = fopen(LOG_FILENAME, "a");

  if (!log_file) {
    fprintf(stderr, "Could not open the log file. Aborting daemon.\n ");
    exit(LOG_OPEN_FAILURE);
  }

#ifdef DEBUG
  log_file = stdout;
#endif

  if (load_configuration() != CONFIG_SUCCESS) {
    fprintf(log_file, "The hardrive sync daemon could not be configured. Terminating daemon.\n");
    exit(BAD_USAGE);
  }

  if (signal(SIGTERM, sig_handler) == SIG_ERR)
    fprintf(log_file, "Can't register handler for SIGTERM");

  if (signal(SIGINT, sig_handler) == SIG_ERR)
    fprintf(log_file, "Can't register handler for SIGINT");

  if (signal(SIGALRM, sig_handler) == SIG_ERR) {
    fprintf(log_file, "Can't register handler for SIGALRM. Unable to schedule mirroring. Terminating Deamon.\n");
    exit(SETUP_TIMER_HANDLER_FAILED);
  }

  sid = setsid();
  if (sid < 0) {
    fprintf(log_file, "Session id could not created. Terminating Daemon.\n");
    exit(CREATING_SID_FAILED);
  }

  if (chdir("/") < 0) {
    fprintf(log_file, "Changing the directory to root failed. Terminating Daemon.\n");
    exit(CHANGING_DIR_FAILED);
  }
  
#ifndef DEBUG
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
#endif
  // Daemonizing the process is complete after this point
  
  // Use inotify to monitor changes in the file system at the mount point
  int inotify_fd = inotify_init();

  if (inotify_fd < 0) {
    fprintf(log_file, "inotify_init failed so we can't watch for changes. Terminating Daemon.\n");
    exit(INIT_FILE_WATCHING_FAILED);
  }

  int attributes_mask = IN_MODIFY | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE;
  /* int attributes_mask = IN_CREATE; */
  int wd = inotify_add_watch(inotify_fd, hardware_conf->master_repo_mount, attributes_mask);

  if (wd < 0) {
    fprintf(log_file, "inotify_add_watch failed so we can't watch for changes. Terminating Daemon.\n");
    exit(SETUP_FILE_WATCHING_FAILED);
  }

  fprintf(log_file, "Monitored mount point = %s\n", hardware_conf->master_repo_mount);

  // Watch all the subdirectories of the mount point
  add_subdir_watchers(hardware_conf->master_repo_mount, inotify_fd, attributes_mask);

  bool monitoring = true;
  fd_set inotify_set;

  FD_ZERO(&inotify_set);
  FD_SET(inotify_fd, &inotify_set);

  // Buffer for storing file system events as they are read
  const static int EVENT_BUF_SIZE = 512 * sizeof(struct inotify_event); // 16382 = 2^14
  uint8_t buf[EVENT_BUF_SIZE];
  int read_bytes = 0;

  // A queue for storing file system events as they become available. Currently this is just location
  // to hold the events but nothing is done with them other than to remind the program it's time to sync
  list_t *event_list = create_list();

  while (monitoring) {

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
	informer(NOTIFY_TIME, sync_thread, NULL);
      }
    }
  }

  fclose(log_file);

  exit(SUCCESS);
}
