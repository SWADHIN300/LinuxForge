/*
 * container.h — Container run logic (existing feature stub)
 *
 * Provides the main run_container() function that sets up
 * PID/UTS/Mount namespaces, chroot, and cgroups.
 * This is a stub — replace with your actual implementation.
 */

#ifndef CONTAINER_H
#define CONTAINER_H

#include "utils.h"

#define MAX_ENV_VARS    32
#define MAX_VOLUMES     16

typedef struct {
    char *items[MAX_ENV_VARS];
    int count;
} EnvList;

/*
 * Run a container with the given command.
 * Sets up namespaces, chroot, cgroups, then execve().
 * Returns 0 on success, -1 on error.
 */
int run_container(int argc, char *argv[]);

int env_parse(int argc, char *argv[], char **envs, int *count);
int env_apply(char **envs, int count);
int env_list_cmd(const char *container_id, int json_output);

int container_monitor(const char *container_id);
int should_restart(const char *container_id, int exit_status);
int restart_container(const char *container_id);
int container_rename(const char *container_id, const char *new_name, int json_output);

int container_load_state_json(const char *container_id, JsonObject *obj);
int container_save_state_json(const char *container_id, const JsonObject *obj);
int container_get_state_path(const char *container_id, char *path_out, size_t path_len);

#endif /* CONTAINER_H */
