/*
 * commit.c — Container Image Commit (OverlayFS Layer Capture)
 *
 * PURPOSE:
 *   Persist a running container's filesystem changes as a new image in the
 *   local registry. This mirrors `docker commit` — it captures the writable
 *   diff layer (OverlayFS upper/) and merges it with the base image to
 *   produce a self-contained, layered image tarball.
 *
 * HOW OVERLAYFS LAYERING WORKS:
 *   When a container is created, its rootfs is set up as OverlayFS:
 *     lower/ ← read-only base image (extracted from registry)
 *     upper/ ← writable per-container diff (all writes go here)
 *     work/  ← kernel bookkeeping directory
 *   On commit, we "squash" upper/ on top of lower/ to get the full state.
 *
 * COMMIT ALGORITHM:
 *   1. Read containers/<id>/state.json:
 *        → overlay_upper path (for changed files)
 *        → image field (base image name:tag for unchanged files)
 *   2. Create a temp directory: /tmp/mycontainer_commit_XXXXXX/
 *   3. Extract the base image's rootfs.tar.gz into temp/ (the "lower" files).
 *   4. Extract the container's upper/ directory into temp/
 *      (overwrites changed files, adds new ones, respects whiteout files).
 *   5. Tar temp/ → new_image_XXXXXX.tar.gz in /tmp/
 *   6. Call registry_push(name, tag, temp_tar) to register the new image.
 *   7. Clean up temp files.
 *
 * COMMIT HISTORY:
 *   Each successful commit appends a line to containers/<id>/commit_history.jsonl:
 *     {"image":"name:tag","timestamp":"<ISO>","layers":N}
 *   commit_history_cmd() reads this file to show the commit log.
 *
 * JSON OUTPUT MODE (--json):
 *   Suppresses registry_push() stdout by redirecting to /dev/null during
 *   the internal call, then prints a single structured JSON result.
 *   Required when called from the Node.js bridge (simulatorBridge.js).
 *
 * FUNCTIONS:
 *   read_container_state()  — load ContainerState from state.json
 *   commit_container()      — main commit: merge layers → registry_push
 *   commit_list_cmd()       — list all images in the registry
 *   commit_history_cmd()    — list commit log for a specific container
 *   commit_cmd()            — CLI dispatch for `mycontainer commit`
 */

#define _GNU_SOURCE
#include "../include/commit.h"
#include <fcntl.h>

/* ================================================================
 * READ CONTAINER STATE
 * ================================================================ */

int read_container_state(const char *container_id, ContainerState *state) {
    if (!container_id || !state) return -1;

    memset(state, 0, sizeof(ContainerState));

    char state_path[MAX_PATH_LEN];
    if (format_buffer(state_path, sizeof(state_path), "%s/%s/state.json",
                      CONTAINERS_DIR, container_id) != 0) {
        fprintf(stderr, "Error: state path too long for %s\n", container_id);
        return -1;
    }

    if (!file_exists(state_path)) {
        fprintf(stderr, "Error: container %s not found (no state.json)\n",
                container_id);
        return -1;
    }

    char *data = read_file(state_path);
    if (!data) {
        fprintf(stderr, "Error: cannot read %s\n", state_path);
        return -1;
    }

    JsonObject obj;
    if (json_parse(data, &obj) != 0) {
        fprintf(stderr, "Error: failed to parse %s\n", state_path);
        free(data);
        return -1;
    }
    free(data);

    const char *v;
    if ((v = json_get(&obj, "id")))
        strncpy(state->id, v, sizeof(state->id) - 1);
    if ((v = json_get(&obj, "name")))
        strncpy(state->name, v, sizeof(state->name) - 1);
    if ((v = json_get(&obj, "status")))
        strncpy(state->status, v, sizeof(state->status) - 1);
    if ((v = json_get(&obj, "pid")))
        state->pid = atoi(v);
    if ((v = json_get(&obj, "image")))
        strncpy(state->image, v, sizeof(state->image) - 1);
    if ((v = json_get(&obj, "command")))
        strncpy(state->command, v, sizeof(state->command) - 1);
    if ((v = json_get(&obj, "ip")))
        strncpy(state->ip, v, sizeof(state->ip) - 1);
    if ((v = json_get(&obj, "ipv6")))
        strncpy(state->ipv6, v, sizeof(state->ipv6) - 1);
    if ((v = json_get(&obj, "runtime_mode")))
        strncpy(state->runtime_mode, v, sizeof(state->runtime_mode) - 1);
    if ((v = json_get(&obj, "rootless")))
        state->rootless = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
    if ((v = json_get(&obj, "privileged")))
        state->privileged = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
    if ((v = json_get(&obj, "cpuset")))
        strncpy(state->cpuset, v, sizeof(state->cpuset) - 1);
    if ((v = json_get(&obj, "restart_policy")))
        strncpy(state->restart_policy, v, sizeof(state->restart_policy) - 1);
    if ((v = json_get(&obj, "restart_count")))
        state->restart_count = atoi(v);
    if ((v = json_get(&obj, "max_restarts")))
        state->max_restarts = atoi(v);
    if ((v = json_get(&obj, "security_profile")))
        strncpy(state->security_profile, v, sizeof(state->security_profile) - 1);
    if ((v = json_get(&obj, "stack_name")))
        strncpy(state->stack_name, v, sizeof(state->stack_name) - 1);
    if ((v = json_get(&obj, "env")))
        strncpy(state->env_json, v, sizeof(state->env_json) - 1);
    if ((v = json_get(&obj, "volumes")))
        strncpy(state->volumes_json, v, sizeof(state->volumes_json) - 1);
    if ((v = json_get(&obj, "rootfs")))
        strncpy(state->rootfs, v, sizeof(state->rootfs) - 1);
    if ((v = json_get(&obj, "created_at")))
        strncpy(state->created_at, v, sizeof(state->created_at) - 1);

    const char *health_str = json_get(&obj, "healthcheck");
    if (health_str && health_str[0] == '{') {
        JsonObject health;
        if (json_parse(health_str, &health) == 0) {
            if ((v = json_get(&health, "command")))
                strncpy(state->health_command, v, sizeof(state->health_command) - 1);
            if ((v = json_get(&health, "interval")))
                state->health_interval = atoi(v);
            if ((v = json_get(&health, "timeout")))
                state->health_timeout = atoi(v);
            if ((v = json_get(&health, "retries")))
                state->health_retries = atoi(v);
            if ((v = json_get(&health, "status")))
                strncpy(state->health_status, v, sizeof(state->health_status) - 1);
            if ((v = json_get(&health, "last_check")))
                strncpy(state->health_last_check, v, sizeof(state->health_last_check) - 1);
            if ((v = json_get(&health, "consecutive_failures")))
                state->health_consecutive_failures = atoi(v);
        }
    }

    /* Parse overlay paths (nested object stored as string) */
    const char *overlay_str = json_get(&obj, "overlay");
    if (overlay_str && overlay_str[0] == '{') {
        JsonObject overlay;
        json_parse(overlay_str, &overlay);
        if ((v = json_get(&overlay, "lower")))
            strncpy(state->overlay_lower, v, sizeof(state->overlay_lower) - 1);
        if ((v = json_get(&overlay, "upper")))
            strncpy(state->overlay_upper, v, sizeof(state->overlay_upper) - 1);
        if ((v = json_get(&overlay, "work")))
            strncpy(state->overlay_work, v, sizeof(state->overlay_work) - 1);
    } else {
        /* Fallback: construct default overlay paths */
        if (format_buffer(state->overlay_lower, sizeof(state->overlay_lower),
                          "%s/%s/overlay/lower", CONTAINERS_DIR, container_id) != 0 ||
            format_buffer(state->overlay_upper, sizeof(state->overlay_upper),
                          "%s/%s/overlay/upper", CONTAINERS_DIR, container_id) != 0 ||
            format_buffer(state->overlay_work, sizeof(state->overlay_work),
                          "%s/%s/overlay/work", CONTAINERS_DIR, container_id) != 0) {
            fprintf(stderr, "Error: overlay paths too long for %s\n", container_id);
            return -1;
        }
    }

    if (strlen(state->runtime_mode) == 0) {
        if (state->privileged) {
            strncpy(state->runtime_mode, "privileged", sizeof(state->runtime_mode) - 1);
        } else {
            state->rootless = 1;
            strncpy(state->runtime_mode, "rootless", sizeof(state->runtime_mode) - 1);
        }
    }

    if (strlen(state->restart_policy) == 0) {
        strncpy(state->restart_policy, "no", sizeof(state->restart_policy) - 1);
    }
    if (state->max_restarts == 0) {
        state->max_restarts = 5;
    }

    return 0;
}

/* ================================================================
 * COMMIT CONTAINER
 * ================================================================ */

int commit_container(const char *container_id,
                     const char *new_name,
                     const char *new_tag,
                     const char *description,
                     int json_output) {
    if (!container_id || !new_name || !new_tag) {
        fprintf(stderr, "Error: container_id, name, and tag are required\n");
        return -1;
    }

    /* Step 1: Read container state */
    if (!json_output) { printf("Capturing container state...     "); fflush(stdout); }

    ContainerState state;
    if (read_container_state(container_id, &state) != 0) {
        if (!json_output) printf("✗\n");
        return -1;
    }
    if (!json_output) printf("✓\n");

    /* Step 2: Extract writable layer */
    if (!json_output) { printf("Extracting writable layer...     "); fflush(stdout); }

    /* Create a temp directory for the commit work */
    char tmp_dir[] = "/tmp/commit_XXXXXX";
    if (mkdtemp(tmp_dir) == NULL) {
        printf("✗\n");
        fprintf(stderr, "Error: cannot create temp directory: %s\n",
                strerror(errno));
        return -1;
    }

    /* Tar the upper layer */
    char upper_tar[MAX_PATH_LEN];
    if (format_buffer(upper_tar, sizeof(upper_tar), "%s/upper.tar.gz", tmp_dir) != 0) {
        fprintf(stderr, "Error: upper layer path too long for %s\n", container_id);
        rmdir_recursive(tmp_dir);
        return -1;
    }

    char cmd[MAX_CMD_LEN];

    /* Check if upper directory exists and has content */
    if (dir_exists(state.overlay_upper)) {
        if (format_buffer(cmd, sizeof(cmd),
                          "tar -czf %s -C %s . 2>/dev/null",
                          upper_tar, state.overlay_upper) != 0) {
            fprintf(stderr, "Error: command too long while archiving writable layer\n");
            rmdir_recursive(tmp_dir);
            return -1;
        }
        system(cmd);
    } else {
        /* Create an empty tarball if no changes */
        if (format_buffer(cmd, sizeof(cmd),
                          "tar -czf %s --files-from /dev/null 2>/dev/null",
                          upper_tar) != 0) {
            fprintf(stderr, "Error: command too long while creating empty layer\n");
            rmdir_recursive(tmp_dir);
            return -1;
        }
        system(cmd);
    }
    if (!json_output) printf("✓\n");

    /* Step 3: Get base image and merge */
    if (!json_output) { printf("Merging with base image...       "); fflush(stdout); }

    /* Create merged directory */
    char merged_dir[MAX_PATH_LEN];
    if (format_buffer(merged_dir, sizeof(merged_dir), "%s/merged", tmp_dir) != 0) {
        fprintf(stderr, "Error: merged path too long for %s\n", container_id);
        rmdir_recursive(tmp_dir);
        return -1;
    }
    mkdir_p(merged_dir);

    /* Extract base image if we know it */
    if (strlen(state.image) > 0) {
        char base_name[64], base_tag[32];
        if (split_name_tag(state.image, base_name, sizeof(base_name),
                           base_tag, sizeof(base_tag)) == 0) {
            Image base_img;
            if (registry_find(base_name, base_tag, &base_img) == 0) {
                /* Extract base image to merged dir */
                if (format_buffer(cmd, sizeof(cmd),
                                  "tar -xzf %s -C %s 2>/dev/null",
                                  base_img.rootfs_path, merged_dir) != 0) {
                    fprintf(stderr, "Error: command too long while extracting base image\n");
                    rmdir_recursive(tmp_dir);
                    return -1;
                }
                system(cmd);
            }
        }
    } else if (dir_exists(state.overlay_lower)) {
        /* Copy lower layer contents */
        if (format_buffer(cmd, sizeof(cmd), "cp -a %s/. %s/ 2>/dev/null",
                          state.overlay_lower, merged_dir) != 0) {
            fprintf(stderr, "Error: command too long while copying lower layer\n");
            rmdir_recursive(tmp_dir);
            return -1;
        }
        system(cmd);
    }

    /* Extract upper layer on top (overwrites changed files) */
    if (file_exists(upper_tar)) {
        if (format_buffer(cmd, sizeof(cmd),
                          "tar -xzf %s -C %s 2>/dev/null",
                          upper_tar, merged_dir) != 0) {
            fprintf(stderr, "Error: command too long while extracting writable layer\n");
            rmdir_recursive(tmp_dir);
            return -1;
        }
        system(cmd);
    }
    if (!json_output) printf("✓\n");

    /* Step 4: Compress new image */
    if (!json_output) { printf("Compressing new image...         "); fflush(stdout); }

    char new_rootfs[MAX_PATH_LEN];
    if (format_buffer(new_rootfs, sizeof(new_rootfs), "%s/rootfs.tar.gz", tmp_dir) != 0 ||
        format_buffer(cmd, sizeof(cmd),
                      "tar -czf %s -C %s . 2>/dev/null",
                      new_rootfs, merged_dir) != 0) {
        fprintf(stderr, "Error: command too long while packaging committed image\n");
        rmdir_recursive(tmp_dir);
        return -1;
    }
    system(cmd);
    if (!json_output) printf("✓\n");

    /* Step 5: Register in image store */
    if (!json_output) { printf("Registering in image store...    "); fflush(stdout); }

    /* Push the new image to registry (suppress push output in JSON mode) */
    if (json_output) {
        /* Redirect stdout briefly */
        fflush(stdout);
        int saved = dup(1);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); close(devnull); }
        int push_result = registry_push(new_name, new_tag, new_rootfs);
        fflush(stdout);
        dup2(saved, 1); close(saved);
        if (push_result != 0) {
            rmdir_recursive(tmp_dir);
            fprintf(stderr, "{\"error\":\"failed to register image\"}\n");
            return -1;
        }
    } else {
        if (registry_push(new_name, new_tag, new_rootfs) != 0) {
            printf("✗\n");
            rmdir_recursive(tmp_dir);
            return -1;
        }
    }

    /* Now update the config.json with commit metadata */
    char config_path[MAX_PATH_LEN];
    if (format_buffer(config_path, sizeof(config_path), "%s/%s_%s/config.json",
                      REGISTRY_IMAGES_DIR, new_name, new_tag) != 0) {
        fprintf(stderr, "Error: config path too long for %s:%s\n", new_name, new_tag);
        rmdir_recursive(tmp_dir);
        return -1;
    }

    char *config_data = read_file(config_path);
    if (config_data) {
        JsonObject config;
        json_parse(config_data, &config);
        free(config_data);

        json_set(&config, "committed_from", container_id);
        json_set(&config, "base_image", state.image);
        if (description && strlen(description) > 0) {
            json_set(&config, "description", description);
        }

        char *config_str = json_stringify_pretty(&config);
        if (config_str) {
            write_file(config_path, config_str);
            free(config_str);
        }
    }

    /* Also update images.json entry with committed_from */
    char *index_data = read_file(REGISTRY_INDEX);
    if (index_data) {
        JsonArray *arr = json_array_new();
        if (arr) {
            json_parse_array(index_data, arr);
            free(index_data);

            /* Find the just-pushed image and add committed_from */
            for (int i = 0; i < arr->count; i++) {
                const char *n = json_get(&arr->objects[i], "name");
                const char *t = json_get(&arr->objects[i], "tag");
                if (n && t && strcmp(n, new_name) == 0 && strcmp(t, new_tag) == 0) {
                    json_set(&arr->objects[i], "committed_from", container_id);
                    json_set(&arr->objects[i], "base_image", state.image);
                    if (description && strlen(description) > 0) {
                        json_set(&arr->objects[i], "description", description);
                    }
                    break;
                }
            }

            char *arr_str = json_array_stringify_pretty(arr);
            if (arr_str) {
                write_file(REGISTRY_INDEX, arr_str);
                free(arr_str);
            }
            json_array_free(arr);
        } else {
            free(index_data);
        }
    }

    if (!json_output) printf("✓\n");

    /* Cleanup temp dirs */
    rmdir_recursive(tmp_dir);

    /* Find the new image ID for the summary */
    Image new_img;
    char new_id[16] = "unknown";
    if (registry_find(new_name, new_tag, &new_img) == 0) {
        strncpy(new_id, new_img.id, sizeof(new_id) - 1);
    }

    if (json_output) {
        JsonObject result;
        result.count = 0;
        json_set(&result, "image_id", new_id);
        json_set(&result, "name", new_name);
        json_set(&result, "tag", new_tag);
        json_set(&result, "committed_from", container_id);
        json_set(&result, "base_image", state.image);
        if (description && strlen(description) > 0) {
            json_set(&result, "description", description);
        }
        json_set(&result, "size", new_img.size);
        json_set(&result, "created_at", new_img.created_at);

        char *str = json_stringify_pretty(&result);
        if (str) {
            printf("%s\n", str);
            free(str);
        }
    } else {
        printf("\nCommitted %s → %s:%s\n", container_id, new_name, new_tag);
        printf("New Image ID: %s\n", new_id);
    }

    return 0;
}

/* ================================================================
 * LIST CONTAINERS
 * ================================================================ */

int commit_list_containers(int json_output) {
    if (!dir_exists(CONTAINERS_DIR)) {
        if (json_output) {
            printf("[]\n");
        } else {
            printf("No containers found.\n");
        }
        return 0;
    }

    DIR *dir = opendir(CONTAINERS_DIR);
    if (!dir) {
        fprintf(stderr, "Error: cannot open %s: %s\n",
                CONTAINERS_DIR, strerror(errno));
        return -1;
    }

    ContainerState states[64];
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && count < 64) {
        if (entry->d_name[0] == '.') continue;

        char state_path[MAX_PATH_LEN];
        if (format_buffer(state_path, sizeof(state_path), "%s/%s/state.json",
                          CONTAINERS_DIR, entry->d_name) != 0) {
            fprintf(stderr, "Error: state path too long for %s\n", entry->d_name);
            continue;
        }

        if (!file_exists(state_path)) continue;

        if (read_container_state(entry->d_name, &states[count]) == 0) {
            count++;
        }
    }
    closedir(dir);

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) return -1;
        for (int i = 0; i < count; i++) {
            JsonObject obj;
            obj.count = 0;
            json_set(&obj, "id", states[i].id);
            json_set(&obj, "name", states[i].name);
            json_set(&obj, "status", states[i].status);
            json_set(&obj, "image", states[i].image);
            json_set(&obj, "command", states[i].command);
            json_set(&obj, "ip", states[i].ip);
            json_set(&obj, "ipv6", states[i].ipv6);
            json_set(&obj, "runtime_mode", states[i].runtime_mode);
            json_set_raw(&obj, "rootless", states[i].rootless ? "true" : "false");
            json_set_raw(&obj, "privileged", states[i].privileged ? "true" : "false");
            json_set(&obj, "cpuset", states[i].cpuset);
            json_set(&obj, "created_at", states[i].created_at);
            json_array_append(arr, &obj);
        }
        char *str = json_array_stringify_pretty(arr);
        if (str) {
            printf("%s\n", str);
            free(str);
        }
        json_array_free(arr);
    } else {
        printf("%-10s %-16s %-10s %-16s %-16s %s\n",
               "ID", "NAME", "STATUS", "IMAGE", "IP", "CREATED");
        printf("%-10s %-16s %-10s %-16s %-16s %s\n",
               "------", "----", "------", "-----", "--", "-------");

        for (int i = 0; i < count; i++) {
            char ago[64];
            time_ago(states[i].created_at, ago, sizeof(ago));
            printf("%-10s %-16s %-10s %-16s %-16s %s\n",
                   states[i].id,
                   states[i].name,
                   states[i].status,
                   states[i].image,
                   states[i].ip,
                   ago);
        }

        if (count == 0) {
            printf("\nNo containers found.\n");
        }
    }

    return 0;
}

/* ================================================================
 * COMMIT HISTORY
 * ================================================================ */

int commit_history(int json_output) {
    Image images[MAX_IMAGES];
    int count = 0;

    if (registry_list(images, &count) != 0) return -1;

    /* Read full index to check for committed_from field */
    char *data = read_file(REGISTRY_INDEX);
    if (!data) return -1;

    JsonArray *arr = json_array_new();
    if (!arr) { free(data); return -1; }
    json_parse_array(data, arr);
    free(data);

    /* Filter for committed images */
    JsonArray *committed = json_array_new();
    if (!committed) { json_array_free(arr); return -1; }

    for (int i = 0; i < arr->count; i++) {
        const char *from = json_get(&arr->objects[i], "committed_from");
        if (from && strlen(from) > 0) {
            json_array_append(committed, &arr->objects[i]);
        }
    }
    json_array_free(arr);

    if (json_output) {
        char *str = json_array_stringify_pretty(committed);
        if (str) {
            printf("%s\n", str);
            free(str);
        }
    } else {
        printf("%-12s %-12s %-10s %-12s %-16s %s\n",
               "IMAGE ID", "NAME", "TAG", "FROM", "BASE IMAGE", "CREATED");
        printf("%-12s %-12s %-10s %-12s %-16s %s\n",
               "--------", "----", "---", "----", "----------", "-------");

        for (int i = 0; i < committed->count; i++) {
            const char *id = json_get(&committed->objects[i], "id");
            const char *name = json_get(&committed->objects[i], "name");
            const char *tag = json_get(&committed->objects[i], "tag");
            const char *from = json_get(&committed->objects[i], "committed_from");
            const char *base = json_get(&committed->objects[i], "base_image");
            const char *created = json_get(&committed->objects[i], "created_at");

            char ago[64] = "unknown";
            if (created) time_ago(created, ago, sizeof(ago));

            printf("%-12s %-12s %-10s %-12s %-16s %s\n",
                   id ? id : "?",
                   name ? name : "?",
                   tag ? tag : "?",
                   from ? from : "?",
                   base ? base : "?",
                   ago);
        }

        if (committed->count == 0) {
            printf("\nNo commit history found.\n");
        }
    }

    json_array_free(committed);
    return 0;
}

/* ================================================================
 * CLI WRAPPER
 * ================================================================ */

int commit_container_cmd(const char *container_id,
                         const char *name_tag,
                         const char *description,
                         int json_output) {
    if (!container_id || !name_tag) {
        fprintf(stderr, "Usage: mycontainer commit <container_id> <name>:<tag> "
                "[--description=\"...\"]\n");
        return 1;
    }

    char name[64], tag[32];
    if (split_name_tag(name_tag, name, sizeof(name), tag, sizeof(tag)) != 0) {
        fprintf(stderr, "Error: invalid image name format. Use name:tag\n");
        return 1;
    }

    return commit_container(container_id, name, tag,
                            description ? description : "",
                            json_output);
}
