/*
 * commit.h — Container commit operations
 *
 * Captures a running container's writable layer (OverlayFS upper/)
 * and merges it with the base image to create a new registry image.
 */

#ifndef COMMIT_H
#define COMMIT_H

#include "utils.h"
#include "registry.h"

/* ---- Constants ---- */
#define CONTAINERS_DIR "containers"

/* ---- Container State (runtime) ---- */
typedef struct {
    char id[16];
    char name[64];
    char status[16];       /* "running", "stopped", "exited" */
    int  pid;
    char image[96];        /* "alpine:3.18" */
    char command[256];     /* command invoked inside the container */
    char ip[32];           /* "172.18.0.2" */
    char ipv6[64];         /* "fd42:18::2" */
    char runtime_mode[16]; /* "rootless" or "privileged" */
    int  rootless;
    int  privileged;
    char cpuset[64];       /* "0,2-3" */
    char restart_policy[16];
    int  restart_count;
    int  max_restarts;
    char security_profile[32];
    char stack_name[64];
    char env_json[MAX_BUF];
    char volumes_json[MAX_BUF];
    char health_command[256];
    int  health_interval;
    int  health_timeout;
    int  health_retries;
    char health_status[16];
    char health_last_check[32];
    int  health_consecutive_failures;
    char rootfs[MAX_PATH_LEN];      /* "./containers/<id>/rootfs" */
    char overlay_lower[MAX_PATH_LEN];
    char overlay_upper[MAX_PATH_LEN];
    char overlay_work[MAX_PATH_LEN];
    char created_at[32];
} ContainerState;

/* ---- Core Commit Functions ---- */

/*
 * Commit a container's current state as a new image.
 * - container_id: the container to commit
 * - new_name: name for the new image
 * - new_tag: tag for the new image
 * - description: optional commit message
 * Returns 0 on success, -1 on error.
 */
int commit_container(const char *container_id,
                     const char *new_name,
                     const char *new_tag,
                     const char *description,
                     int json_output);

/*
 * List all containers eligible for commit.
 * Scans containers/ directory and reads state.json files.
 * If json_output is non-zero, print JSON.
 * Returns 0 on success.
 */
int commit_list_containers(int json_output);

/*
 * Show commit history — images that were created from commits.
 * Filters images.json for entries with a "committed_from" field.
 * If json_output is non-zero, print JSON.
 * Returns 0 on success.
 */
int commit_history(int json_output);

/* ---- Helper Functions ---- */

/*
 * Read a container's state.json file.
 * Returns 0 on success, -1 if container not found.
 */
int read_container_state(const char *container_id, ContainerState *state);

/* ---- CLI Wrapper ---- */
int commit_container_cmd(const char *container_id,
                         const char *name_tag,
                         const char *description,
                         int json_output);

#endif /* COMMIT_H */
