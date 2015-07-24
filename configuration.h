#ifndef LOAD_CONFIGURATION
#define LOAD_CONFIGURATION
#include <stdio.h>

typedef struct hardware_config_t hardware_config_t;

#define CONFIG_SUCCESS               0
#define CONFIG_FILE_ACCESS_FAILURE   1
#define CONFIG_FILE_READ_FAILURE     2
#define CONFIG_FILE_WRITE_FAILURE    3
#define CONFIG_PARSE_ERROR           4
#define CONFIG_MULTI_MASTER_ERROR    5
#define CONFIG_NO_MASTER_ERROR       6

/* int load_configuration(hardware_config_t **hardware_config); */

int load_configuration(char *configuration_file, hardware_config_t **hardware_conf);
/* int load_configuration(hardware_config_t **hardware_conf, char *configuration_file); */
void destroy_hardware_configuration(hardware_config_t *hardware_config);
char *get_master_repo_mount(hardware_config_t *hardware_config);
char *get_slave_repo_mount(hardware_config_t *hardware_config);

void write_configuration_error(FILE *log_file, int error);

#endif
