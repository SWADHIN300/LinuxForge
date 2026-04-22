/*
 * volume.c — Container Volume Management (Bind-Mount Simulation)
 *
 * PURPOSE:
 *   Implement host-directory bind mounts for containers, allowing a
 *   directory on the host to appear at a specified path inside the
 *   container's rootfs. This mirrors the --volume / -v flag in Docker.
 *
 * VOLUME SPECIFICATION FORMAT:
 *   <host_path>:<container_path>[:<mode>]
 *   Examples:
 *     /data:/app/data          → read-write bind mount (default)
 *     /config:/etc/app:ro      → read-only bind mount
 *
 * IMPLEMENTATION STRATEGY:
 *   Since the runtime currently runs as a host-side simulation (no real
 *   mount namespaces yet), volume_mount() approximates bind-mount behaviour
 *   by:
 *     1. Creating the target directory inside the container's rootfs:
 *        mkdir_p(rootfs + container_path)
 *     2. Writing a .mycontainer-volume.json marker file at that path
 *        to record host_path, container_path, and mode for later cleanup.
 *
 *   Real bind-mount implementation (post-clone):
 *     mount(host_path, merged_path + container_path, NULL,
 *           MS_BIND | (mode=="ro" ? MS_RDONLY : 0), NULL)
 *
 * VOLUME STATE:
 *   Each container's state.json stores a "volumes" JSON array:
 *     [{"host_path":"/data","container_path":"/app/data","mode":"rw"}]
 *   volume_list() reads this array from state.json and renders it.
 *   volume_unmount() removes the marker files during stack teardown.
 *
 * FUNCTIONS:
 *   volume_parse()     — parse "host:container[:mode]" string → Volume struct
 *   volume_mount()     — create dest dirs + write marker files in rootfs
 *   volume_unmount()   — remove marker files (called during stack_down)
 *   volume_list()      — display volume table for a container
 *   volume_cmd()       — CLI dispatch for `mycontainer volume`
 */

#define _GNU_SOURCE
#include "../include/volume.h"
#include "../include/container.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <unistd.h>

static int volume_parse_array_json(const char *json_str, Volume *volumes, int max_count) {
    JsonArray *arr;
    int count = 0;

    if (!json_str || !volumes || max_count <= 0) return -1;

    arr = json_array_new();
    if (!arr) return -1;
    if (json_parse_array(json_str, arr) != 0) {
        json_array_free(arr);
        return -1;
    }

    for (int i = 0; i < arr->count && count < max_count; i++) {
        const char *host = json_get(&arr->objects[i], "host_path");
        const char *container = json_get(&arr->objects[i], "container_path");
        const char *mode = json_get(&arr->objects[i], "mode");

        if (host) strncpy(volumes[count].host_path, host, sizeof(volumes[count].host_path) - 1);
        if (container) strncpy(volumes[count].container_path, container, sizeof(volumes[count].container_path) - 1);
        if (mode) strncpy(volumes[count].mode, mode, sizeof(volumes[count].mode) - 1);
        if (strlen(volumes[count].mode) == 0) {
            strncpy(volumes[count].mode, "rw", sizeof(volumes[count].mode) - 1);
        }
        count++;
    }

    json_array_free(arr);
    return count;
}

int volume_parse(const char *volume_str, Volume *vol) {
    char buffer[MAX_PATH_LEN * 2];
    char *mode_sep;
    char *mount_sep;

    if (!volume_str || !vol) return -1;

    memset(vol, 0, sizeof(Volume));
    strncpy(buffer, volume_str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    mode_sep = strrchr(buffer, ':');
    if (!mode_sep) {
        fprintf(stderr, "Error: invalid volume format. Use host:container[:ro|rw]\n");
        return -1;
    }

    if (strcmp(mode_sep + 1, "ro") == 0 || strcmp(mode_sep + 1, "rw") == 0) {
        strncpy(vol->mode, mode_sep + 1, sizeof(vol->mode) - 1);
        *mode_sep = '\0';
    } else {
        strncpy(vol->mode, "rw", sizeof(vol->mode) - 1);
    }

    mount_sep = strrchr(buffer, ':');
    if (!mount_sep) {
        fprintf(stderr, "Error: invalid volume format. Use host:container[:ro|rw]\n");
        return -1;
    }

    *mount_sep = '\0';
    strncpy(vol->host_path, buffer, sizeof(vol->host_path) - 1);
    strncpy(vol->container_path, mount_sep + 1, sizeof(vol->container_path) - 1);

    if (strlen(vol->host_path) == 0 || strlen(vol->container_path) == 0) {
        fprintf(stderr, "Error: invalid volume mount '%s'\n", volume_str);
        return -1;
    }

    if (!dir_exists(vol->host_path) && !file_exists(vol->host_path)) {
        fprintf(stderr, "Error: host path not found: %s\n", vol->host_path);
        return -1;
    }

    return 0;
}

static int volume_parent_dir(const char *path, char *parent, size_t parent_len) {
    const char *slash;
    size_t len;

    if (!path || !parent || parent_len == 0) return -1;
    slash = strrchr(path, '/');
    if (!slash || slash == path) {
        strncpy(parent, "/", parent_len - 1);
        parent[parent_len - 1] = '\0';
        return 0;
    }

    len = (size_t) (slash - path);
    if (len >= parent_len) return -1;
    memcpy(parent, path, len);
    parent[len] = '\0';
    return 0;
}

static int volume_touch(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}

int volume_mount(Volume *volumes, int count, const char *rootfs_path) {
    if (!rootfs_path) return -1;

    for (int i = 0; i < count; i++) {
        char dest[MAX_PATH_LEN];
        char parent[MAX_PATH_LEN];
        char marker[MAX_PATH_LEN];
        JsonObject meta;
        char *json;
        int source_is_dir;

        if (format_buffer(dest, sizeof(dest), "%s%s", rootfs_path,
                          volumes[i].container_path) != 0) {
            fprintf(stderr, "Error: volume destination path too long\n");
            return -1;
        }

        source_is_dir = dir_exists(volumes[i].host_path);
        if (source_is_dir) {
            if (mkdir_p(dest) != 0) return -1;
        } else {
            if (volume_parent_dir(dest, parent, sizeof(parent)) != 0) return -1;
            if (mkdir_p(parent) != 0) return -1;
            if (volume_touch(dest) != 0) return -1;
        }

        if (mount(volumes[i].host_path, dest, NULL, MS_BIND | MS_REC, NULL) != 0) {
            return -1;
        }

        if (strcmp(volumes[i].mode, "ro") == 0) {
            if (mount(NULL, dest, NULL,
                      MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, NULL) != 0) {
                return -1;
            }
        }

        if (format_buffer(marker, sizeof(marker), "%s/.mycontainer-volume.json",
                          source_is_dir ? dest : parent) != 0) {
            fprintf(stderr, "Error: volume marker path too long\n");
            return -1;
        }

        meta.count = 0;
        json_set(&meta, "host_path", volumes[i].host_path);
        json_set(&meta, "container_path", volumes[i].container_path);
        json_set(&meta, "mode", volumes[i].mode);
        json = json_stringify_pretty(&meta);
        if (!json) return -1;
        if (write_file(marker, json) != 0) {
            free(json);
            return -1;
        }
        free(json);
    }

    return 0;
}

int volume_list(const char *container_id, int json_output) {
    JsonObject state;
    const char *volumes_json;
    Volume volumes[MAX_VOLUMES];
    int count;

    if (!container_id) return -1;
    if (container_load_state_json(container_id, &state) != 0) return -1;

    volumes_json = json_get(&state, "volumes");
    if (!volumes_json || strlen(volumes_json) == 0) {
        volumes_json = "[]";
    }

    count = volume_parse_array_json(volumes_json, volumes, MAX_VOLUMES);
    if (count < 0) count = 0;

    if (json_output) {
        JsonArray *arr = json_array_new();
        JsonObject result;
        char count_str[16];
        char *arr_str;
        char *json;

        if (!arr) return -1;
        for (int i = 0; i < count; i++) {
            JsonObject item;
            item.count = 0;
            json_set(&item, "host_path", volumes[i].host_path);
            json_set(&item, "container_path", volumes[i].container_path);
            json_set(&item, "mode", volumes[i].mode);
            json_array_append(arr, &item);
        }

        arr_str = json_array_stringify_pretty(arr);
        json_array_free(arr);
        if (!arr_str) return -1;

        result.count = 0;
        snprintf(count_str, sizeof(count_str), "%d", count);
        json_set(&result, "container_id", container_id);
        json_set_raw(&result, "volumes", arr_str);
        json_set_raw(&result, "count", count_str);
        json = json_stringify_pretty(&result);
        free(arr_str);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("%-32s %-24s %s\n", "HOST PATH", "CONTAINER PATH", "MODE");
        printf("%-32s %-24s %s\n", "---------", "--------------", "----");
        for (int i = 0; i < count; i++) {
            printf("%-32s %-24s %s\n",
                   volumes[i].host_path,
                   volumes[i].container_path,
                   volumes[i].mode);
        }
        if (count == 0) {
            printf("No volumes configured for %s\n", container_id);
        }
    }

    return 0;
}

int volume_unmount(const char *container_id) {
    JsonObject state;
    const char *volumes_json;
    const char *rootfs;
    Volume volumes[MAX_VOLUMES];
    int count;

    if (!container_id) return -1;
    if (container_load_state_json(container_id, &state) != 0) return -1;

    rootfs = json_get(&state, "rootfs");
    volumes_json = json_get(&state, "volumes");
    if (!rootfs || !volumes_json) return 0;

    count = volume_parse_array_json(volumes_json, volumes, MAX_VOLUMES);
    if (count < 0) return 0;

    for (int i = 0; i < count; i++) {
        char marker[MAX_PATH_LEN];
        if (format_buffer(marker, sizeof(marker), "%s%s/.mycontainer-volume.json",
                          rootfs, volumes[i].container_path) != 0) {
            continue;
        }
        remove(marker);
    }

    return 0;
}

int volume_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 4 || strcmp(argv[2], "ls") != 0) {
        fprintf(stderr, "Usage: mycontainer volume ls <id> [--json]\n");
        return 1;
    }

    return volume_list(argv[3], json_output);
}
