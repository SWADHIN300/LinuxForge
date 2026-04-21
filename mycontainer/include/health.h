#ifndef HEALTH_H
#define HEALTH_H

#include "commit.h"

int health_run_check(const char *container_id);
int health_update_status(const char *container_id, int exit_code);
void *health_monitor_loop(void *arg);
int health_get_status(const char *container_id, int json_output);
int health_cmd(int argc, char *argv[]);

#endif
