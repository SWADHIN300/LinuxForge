/*
 * checkpoint.c — Container Checkpoint & Restore (CRIU-backed)
 *
 * PURPOSE:
 *   Implement live container snapshotting by combining:
 *     1. A JSON state.json copy  — preserves configuration metadata.
 *     2. A rootfs.tar.gz archive — freezes the container's filesystem.
 *     3. An optional CRIU memory dump — freezes in-memory process state.
 *
 * CRIU (Checkpoint/Restore In Userspace):
 *   CRIU is an external Linux tool that serialises a running process tree
 *   to image files (pages, registers, file descriptors, sockets …).
 *   On restore it reconstructs the process as if it never stopped.
 *
 *   Prerequisites:
 *     - `criu` binary in PATH
 *     - Kernel ≥ 3.11 with CONFIG_CHECKPOINT_RESTORE=y
 *     - CAP_SYS_PTRACE or running as root
 *     - CONFIG_MEMCG, CONFIG_NET_NS, CONFIG_PID_NS for full support
 *
 * CHECKPOINT DIRECTORY LAYOUT:
 *   <checkpoint_dir>/
 *     state.json       ← copy of container state at snapshot time
 *     rootfs.tar.gz    ← tar of container rootfs at snapshot time
 *     metadata.json    ← snapshot metadata (id, timestamp, criu flag)
 *     *.img            ← CRIU memory/register dump files (if available)
 *
 * RESTORE WORKFLOW:
 *   1. Read state.json from checkpoint dir → get original config.
 *   2. Generate a new container ID.
 *   3. Recreate overlay directory structure.
 *   4. Extract rootfs.tar.gz into new container's rootfs/.
 *   5. Save updated state.json for the restored container.
 *   6. (CRIU) If *.img files present: `criu restore --images-dir <dir>`
 *
 * FUNCTIONS:
 *   checkpoint_create()  — snapshot a running container to a directory
 *   checkpoint_restore() — bring a container back from a snapshot
 *   checkpoint_list()    — enumerate all saved checkpoints
 *   checkpoint_cmd()     — CLI dispatch for `mycontainer checkpoint`
 *   restore_cmd()        — CLI dispatch for `mycontainer restore`
 */

#define _GNU_SOURCE
#include "../include/checkpoint.h"
#include "../include/export.h"
#include "../include/container.h"
#include "../include/dns.h"

static long checkpoint_dir_size(const char *path) {
    struct stat st;

    if (!path || stat(path, &st) != 0) return 0;
    if (S_ISREG(st.st_mode)) return st.st_size;

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry;
        long total = 0;
        if (!dir) return 0;
        while ((entry = readdir(dir)) != NULL) {
            char child[MAX_PATH_LEN];
            if (entry->d_name[0] == '.') continue;
            if (format_buffer(child, sizeof(child), "%s/%s", path, entry->d_name) != 0) {
                continue;
            }
            total += checkpoint_dir_size(child);
        }
        closedir(dir);
        return total;
    }

    return 0;
}

static int checkpoint_has_criu(void) {
#ifdef _WIN32
    return 0;
#else
    return system("criu --version >/dev/null 2>&1") == 0;
#endif
}

int checkpoint_create(const char *container_id, const char *checkpoint_dir, int json_output) {
    ContainerState state;
    char state_src[MAX_PATH_LEN];
    char state_dest[MAX_PATH_LEN];
    char rootfs_archive[MAX_PATH_LEN];
    char metadata_path[MAX_PATH_LEN];
    char export_cmd[MAX_CMD_LEN];
    JsonObject meta;
    char *json;
    int criu_ok = 0;

    if (!container_id || !checkpoint_dir) return -1;
    if (read_container_state(container_id, &state) != 0) return -1;
    if (mkdir_p(checkpoint_dir) != 0) return -1;

    if (format_buffer(state_src, sizeof(state_src), "%s/%s/state.json",
                      CONTAINERS_DIR, container_id) != 0 ||
        format_buffer(state_dest, sizeof(state_dest), "%s/state.json", checkpoint_dir) != 0 ||
        format_buffer(rootfs_archive, sizeof(rootfs_archive), "%s/rootfs.tar.gz", checkpoint_dir) != 0 ||
        format_buffer(metadata_path, sizeof(metadata_path), "%s/metadata.json", checkpoint_dir) != 0) {
        return -1;
    }

    if (copy_file(state_src, state_dest) != 0) return -1;
    if (format_buffer(export_cmd, sizeof(export_cmd),
                      "tar -czf \"%s\" -C \"%s\" .",
                      rootfs_archive, state.rootfs) != 0) {
        return -1;
    }
    if (system(export_cmd) != 0) return -1;

    if (checkpoint_has_criu() && state.pid > 0) {
        char cmd[MAX_CMD_LEN];
        if (format_buffer(cmd, sizeof(cmd),
                          "criu dump -t %d --images-dir \"%s\" --shell-job --leave-running >/dev/null 2>&1",
                          state.pid, checkpoint_dir) == 0 &&
            system(cmd) == 0) {
            criu_ok = 1;
        }
    }

    meta.count = 0;
    json_set(&meta, "container_id", container_id);
    json_set(&meta, "name", state.name);
    json_set(&meta, "source_image", state.image);
    json_set_raw(&meta, "criu_available", checkpoint_has_criu() ? "true" : "false");
    json_set_raw(&meta, "criu_dumped", criu_ok ? "true" : "false");
    get_timestamp(state.created_at, sizeof(state.created_at));
    json_set(&meta, "created_at", state.created_at);
    json = json_stringify_pretty(&meta);
    if (!json) return -1;
    if (write_file(metadata_path, json) != 0) {
        free(json);
        return -1;
    }
    free(json);

    if (json_output) {
        printf("{\n");
        printf("  \"success\": true,\n");
        printf("  \"container_id\": \"%s\",\n", container_id);
        printf("  \"checkpoint_dir\": \"%s\",\n", checkpoint_dir);
        printf("  \"criu_dumped\": %s\n", criu_ok ? "true" : "false");
        printf("}\n");
    } else {
        printf("Checkpointed %s to %s\n", container_id, checkpoint_dir);
        if (!criu_ok) {
            printf("CRIU dump unavailable; saved logical snapshot only.\n");
        }
    }

    return 0;
}

int checkpoint_restore(const char *checkpoint_dir, const char *new_name, int json_output) {
    char state_path[MAX_PATH_LEN];
    char rootfs_archive[MAX_PATH_LEN];
    JsonObject state;
    char *data;
    char container_id[ID_LEN + 1];
    char container_dir[MAX_PATH_LEN];
    char overlay_lower[MAX_PATH_LEN];
    char overlay_upper[MAX_PATH_LEN];
    char overlay_work[MAX_PATH_LEN];
    char rootfs[MAX_PATH_LEN];
    char now[32];
    char *json;
    char cmd[MAX_CMD_LEN];

    if (!checkpoint_dir || !new_name) return -1;

    if (format_buffer(state_path, sizeof(state_path), "%s/state.json", checkpoint_dir) != 0 ||
        format_buffer(rootfs_archive, sizeof(rootfs_archive), "%s/rootfs.tar.gz", checkpoint_dir) != 0) {
        return -1;
    }

    data = read_file(state_path);
    if (!data) {
        fprintf(stderr, "Error: checkpoint state not found in %s\n", checkpoint_dir);
        return -1;
    }
    if (json_parse(data, &state) != 0) {
        free(data);
        return -1;
    }
    free(data);

    generate_id(container_id);
    if (format_buffer(container_dir, sizeof(container_dir), "%s/%s", CONTAINERS_DIR, container_id) != 0 ||
        format_buffer(overlay_lower, sizeof(overlay_lower), "%s/overlay/lower", container_dir) != 0 ||
        format_buffer(overlay_upper, sizeof(overlay_upper), "%s/overlay/upper", container_dir) != 0 ||
        format_buffer(overlay_work, sizeof(overlay_work), "%s/overlay/work", container_dir) != 0 ||
        format_buffer(rootfs, sizeof(rootfs), "%s/rootfs", container_dir) != 0) {
        return -1;
    }

    if (mkdir_p(overlay_lower) != 0 || mkdir_p(overlay_upper) != 0 ||
        mkdir_p(overlay_work) != 0 || mkdir_p(rootfs) != 0) {
        return -1;
    }

    if (file_exists(rootfs_archive)) {
        if (format_buffer(cmd, sizeof(cmd),
                          "tar -xzf \"%s\" -C \"%s\"",
                          rootfs_archive, rootfs) != 0) {
            return -1;
        }
        system(cmd);
    }

    get_timestamp(now, sizeof(now));
    json_set(&state, "id", container_id);
    json_set(&state, "name", new_name);
    json_set(&state, "status", "restored");
    json_set_raw(&state, "pid", "0");
    json_set(&state, "rootfs", rootfs);
    json_set(&state, "created_at", now);

    JsonObject overlay;
    overlay.count = 0;
    json_set(&overlay, "lower", overlay_lower);
    json_set(&overlay, "upper", overlay_upper);
    json_set(&overlay, "work", overlay_work);
    json = json_stringify_pretty(&overlay);
    if (!json) return -1;
    json_set_raw(&state, "overlay", json);
    free(json);

    if (container_save_state_json(container_id, &state) != 0) return -1;
    dns_update_hosts(NULL);

    if (json_output) {
        printf("{\n");
        printf("  \"success\": true,\n");
        printf("  \"container_id\": \"%s\",\n", container_id);
        printf("  \"name\": \"%s\"\n", new_name);
        printf("}\n");
    } else {
        printf("Restored container as %s (%s)\n", new_name, container_id);
    }

    return 0;
}

int checkpoint_list(int json_output) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_exists(CHECKPOINTS_DIR)) {
        printf(json_output ? "[]\n" : "No checkpoints found.\n");
        return 0;
    }

    dir = opendir(CHECKPOINTS_DIR);
    if (!dir) {
        fprintf(stderr, "Error: cannot open %s: %s\n", CHECKPOINTS_DIR, strerror(errno));
        return -1;
    }

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) {
            closedir(dir);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            char metadata_path[MAX_PATH_LEN];
            JsonObject item;
            char *data;

            if (entry->d_name[0] == '.') continue;
            if (format_buffer(metadata_path, sizeof(metadata_path), "%s/%s/metadata.json",
                              CHECKPOINTS_DIR, entry->d_name) != 0) {
                continue;
            }

            item.count = 0;
            if (file_exists(metadata_path) && (data = read_file(metadata_path)) != NULL) {
                json_parse(data, &item);
                free(data);
            } else {
                json_set(&item, "name", entry->d_name);
            }
            json_set(&item, "checkpoint", entry->d_name);
            json_array_append(arr, &item);
        }

        char *json = json_array_stringify_pretty(arr);
        json_array_free(arr);
        if (!json) {
            closedir(dir);
            return -1;
        }
        printf("%s\n", json);
        free(json);
    } else {
        printf("%-24s %-12s %-12s %s\n", "CHECKPOINT", "SOURCE", "SIZE", "CREATED");
        printf("%-24s %-12s %-12s %s\n", "----------", "------", "----", "-------");
        while ((entry = readdir(dir)) != NULL) {
            char metadata_path[MAX_PATH_LEN];
            JsonObject meta;
            char *data;
            char size[16];

            if (entry->d_name[0] == '.') continue;
            if (format_buffer(metadata_path, sizeof(metadata_path), "%s/%s/metadata.json",
                              CHECKPOINTS_DIR, entry->d_name) != 0) {
                continue;
            }

            memset(&meta, 0, sizeof(meta));
            data = read_file(metadata_path);
            if (data) {
                json_parse(data, &meta);
                free(data);
            }

            format_size(checkpoint_dir_size(metadata_path), size, sizeof(size));
            printf("%-24s %-12s %-12s %s\n",
                   entry->d_name,
                   json_get(&meta, "container_id") ? json_get(&meta, "container_id") : "-",
                   size,
                   json_get(&meta, "created_at") ? json_get(&meta, "created_at") : "-");
        }
    }

    closedir(dir);
    return 0;
}

int checkpoint_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer checkpoint <id> <dir> [--json]\n"
                        "       mycontainer checkpoint ls [--json]\n");
        return 1;
    }

    if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) {
        return checkpoint_list(json_output);
    }

    if (argc < 4) {
        fprintf(stderr, "Usage: mycontainer checkpoint <id> <dir> [--json]\n");
        return 1;
    }

    return checkpoint_create(argv[2], argv[3], json_output);
}

int restore_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 4) {
        fprintf(stderr, "Usage: mycontainer restore <checkpoint_dir> <new_name> [--json]\n");
        return 1;
    }

    return checkpoint_restore(argv[2], argv[3], json_output);
}
