#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "bool.h"
#include "configuration.h"

// Add a function to return the version information
const static char *CURRENT_HARDWARE_CONFIG_VERSION = "1.0";

struct hardware_config_t {
  const char *version;
  char *master_repo_uuid;
  char *slave_repo_uuid;
  char *master_repo_mount;
  char *slave_repo_mount;
};
  
/* #define CONFIG_FILENAME "driveSync.conf" */
/* const static char *CONFIG_FILENAME = "driveSync.conf"; */

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


/* int load_configuration(hardware_config_t **hardware_conf, char *configuration_file) { */
int load_configuration(char *configuration_file, hardware_config_t **hardware_conf) {
  // TODO: Use a file not in the current location
  FILE *configFile = fopen(configuration_file, "r");

  if (!configFile)
    return CONFIG_FILE_ACCESS_FAILURE;
 
  int config_error = CONFIG_SUCCESS;
  *hardware_conf = (hardware_config_t *) malloc(sizeof(hardware_config_t));
  (*hardware_conf)->version = CURRENT_HARDWARE_CONFIG_VERSION;
  (*hardware_conf)->master_repo_mount = NULL;
  (*hardware_conf)->slave_repo_mount = NULL;
      
  const static int MAX_MOUNT_LENGTH = 1024;
  char relationship[16];
  char storagetype[16];
  char mount[MAX_MOUNT_LENGTH];

  int retVal = fscanf(configFile,"%s %s %s", relationship, storagetype, mount);

  if ((retVal != 3) || (retVal == EOF)) {
    free(*hardware_conf);
    fclose(configFile);
    return CONFIG_FILE_READ_FAILURE;
  }

  if (strcmp(relationship, "master") == 0) {
    if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	(*hardware_conf)->master_repo_mount = newMount;
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
	(*hardware_conf)->slave_repo_mount = newMount;
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
    free((*hardware_conf)->master_repo_mount);
    free((*hardware_conf)->slave_repo_mount);
    free(*hardware_conf);
    fclose(configFile);
    return config_error;
  }

  retVal = fscanf(configFile,"%s %s %s", relationship, storagetype, mount);

  if ((retVal != 3) || (retVal == EOF)) {
    free(*hardware_conf);
    fclose(configFile);
    return CONFIG_FILE_READ_FAILURE;
  }

  if (strcmp(relationship, "master") == 0) {
    // For the time being, we can not have more than one master
    if ((*hardware_conf)->master_repo_mount != NULL)
      config_error = CONFIG_MULTI_MASTER_ERROR;
    else if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	(*hardware_conf)->master_repo_mount = newMount;
      }
      else
	config_error = CONFIG_PARSE_ERROR;
    }
    else
      config_error = CONFIG_PARSE_ERROR;
  }
  else if(strcmp(relationship, "slave") == 0) {
    // We haven't found a master drive yet and we're only looking at two drives so there are no masters
    if ((*hardware_conf)->slave_repo_mount != NULL)
      config_error = CONFIG_NO_MASTER_ERROR;
    else if (strcmp(storagetype, "repo") == 0) {
      if(isValidMount(MAX_MOUNT_LENGTH, mount)) {
	int len = strlen(mount);
	char *newMount = (char *) malloc ((len + 1) * sizeof(char));
	strcpy(newMount, mount);
	(*hardware_conf)->slave_repo_mount = newMount;
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
    free(*hardware_conf);
    fclose(configFile);
    return config_error;
  }
  return CONFIG_SUCCESS;
}

void destroy_hardware_configuration(hardware_config_t *hardware_config) {
  free(hardware_config->master_repo_mount);
  free(hardware_config->slave_repo_mount);
  free(hardware_config->master_repo_uuid);
  free(hardware_config->slave_repo_uuid);
  free(hardware_config);
}

char *get_master_repo_mount(hardware_config_t *hardware_config) {
  return hardware_config->master_repo_mount;
}

char *get_slave_repo_mount(hardware_config_t *hardware_config) {
  return hardware_config->slave_repo_mount;
}

/* #define CONFIG_SUCCESS               0 */
/* #define CONFIG_FILE_ACCESS_FAILURE   1 */
/* #define CONFIG_FILE_READ_FAILURE     2 */
/* #define CONFIG_FILE_WRITE_FAILURE    3 */
/* #define CONFIG_PARSE_ERROR           4 */
/* #define CONFIG_MULTI_MASTER_ERROR    5 */
/* #define CONFIG_NO_MASTER_ERROR       6 */

void write_configuration_error(FILE *log_file, int error) {

  if (error == CONFIG_SUCCESS)
    fprintf(log_file,"Configuration was successful.\n");
  else if (error == CONFIG_FILE_ACCESS_FAILURE)
    fprintf(log_file,"There was an error opening the configuration file.\n");
  else if (error == CONFIG_FILE_READ_FAILURE)
    fprintf(log_file,"Reading the configuration file failed.\n");
  else if (error == CONFIG_FILE_WRITE_FAILURE)
    fprintf(log_file,"Writing to the configuration file failed.\n");
  else if (error == CONFIG_PARSE_ERROR)
    fprintf(log_file,"Parsing the configuration file failed.\n");
  else if (error == CONFIG_MULTI_MASTER_ERROR)
    fprintf(log_file,"There was more than one master drive specified in the configuration file. Please reconfigure.\n");
  else if (error == CONFIG_NO_MASTER_ERROR)
    fprintf(log_file,"There was no master drive specified in the configuration file. Please reconfigure.\n");
  else
    fprintf(log_file,"There was an unrecognized error in the configuration.\n");
}
