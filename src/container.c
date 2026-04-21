/*
 * container.c — Container Lifecycle Management
 *
 * PURPOSE:
 *   Implement the full lifecycle of a simulated Linux container:
 *   creation, state persistence, environment injection, volume mounting,
 *   network registration, restart policy, and renaming.
 *
 * CONTAINER RUNTIME MODEL:
 *   Each container is represented by a directory on disk:
 *     containers/<id>/
 *       state.json          ← full container metadata (status, image, pid,
 *                              ip, env, volumes, healthcheck, overlay paths)
 *       overlay/
 *         lower/            ← read-only base image layer
 *         upper/            ← writable diff layer (captures all changes)
 *         work/             ← OverlayFS work dir (kernel bookkeeping)
 *       rootfs/             ← merged view (lower + upper) mounted here
 *
 * OVERLAYFS:
 *   OverlayFS presents a unified view of `lower` (read-only base image)
 *   and `upper` (writable per-container layer). Reads from lower if not
 *   in upper; writes go to upper. `commit` captures upper → new image.
 *
 * RESTART POLICY:
 *   Stored in state.json as "restart_policy": "no"|"always"|"on-failure"
 *   The should_restart() function checks policy + restart_count < max.
 *   restart_container() increments the counter and appends a log entry.
 *
 * STATE I/O FUNCTIONS:
 *   container_get_state_path() — build path to state.json
 *   container_load_state_json()— read + parse state.json → JsonObject
 *   container_save_state_json()— serialize JsonObject → state.json
 *   read_container_state()     — high-level: state.json → ContainerState
 *
 * KEY FUNCTIONS:
 *   run_container()            — parse CLI flags, build state, write state.json
 *   container_rename()         — update name in state + network + DNS
 *   env_parse()/env_apply()    — collect --env=K=V flags, setenv in process
 *   container_monitor()        — check/apply restart policy after exit
 *
 * SIMULATION NOTE:
 *   The engine records state and sets up overlay directories but does NOT
 *   actually exec a namespace-isolated process in the current build.
 *   The "[STUB] Runtime metadata recorded" message marks this boundary.
 *   Replace with clone(CLONE_NEWPID|CLONE_NEWNET|…) + execve() to go live.
 */

#define _GNU_SOURCE
#include "../include/container.h"
#include "../include/commit.h"
#include "../include/dns.h"
#include "../include/health.h"
#include "../include/logs.h"
#include "../include/network.h"
#include "../include/registry.h"
#include "../include/security.h"
#include "../include/volume.h"

static int has_flag_local(int argc, char *argv[], const char *flag) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) return 1;
    }
    return 0;
}

static int container_is_known_flag(const char *arg) {
    const char *flags[] = {
        "--name", "--image", "--cpuset", "--restart", "--max-restarts",
        "--security", "--stack", "--healthcheck", "--healthcheck-interval",
        "--healthcheck-timeout", "--healthcheck-retries", "--env", "--volume",
        NULL
    };

    if (!arg) return 0;
    if (strcmp(arg, "--rootless") == 0 || strcmp(arg, "--privileged") == 0 ||
        strcmp(arg, "--json") == 0) {
        return 1;
    }

    for (int i = 0; flags[i] != NULL; i++) {
        size_t len = strlen(flags[i]);
        if (strcmp(arg, flags[i]) == 0) return 1;
        if (strncmp(arg, flags[i], len) == 0 && arg[len] == '=') return 1;
    }
    return 0;
}

static int container_flag_takes_value(const char *arg) {
    const char *flags[] = {
        "--name", "--image", "--cpuset", "--restart", "--max-restarts",
        "--security", "--stack", "--healthcheck", "--healthcheck-interval",
        "--healthcheck-timeout", "--healthcheck-retries", "--env", "--volume",
        NULL
    };

    if (!arg) return 0;
    for (int i = 0; flags[i] != NULL; i++) {
        if (strcmp(arg, flags[i]) == 0) return 1;
    }
    return 0;
}

static void container_free_envs(char **envs, int count) {
    for (int i = 0; i < count; i++) {
        free(envs[i]);
    }
}

static char *container_volumes_to_json(Volume *volumes, int count) {
    JsonArray *arr;
    char *json;

    arr = json_array_new();
    if (!arr) return NULL;

    for (int i = 0; i < count; i++) {
        JsonObject item;
        item.count = 0;
        json_set(&item, "host_path", volumes[i].host_path);
        json_set(&item, "container_path", volumes[i].container_path);
        json_set(&item, "mode", volumes[i].mode);
        json_array_append(arr, &item);
    }

    json = json_array_stringify_pretty(arr);
    json_array_free(arr);
    return json;
}

static int container_default_command(const char *image_ref, char *out, size_t out_len) {
    char name[64];
    char tag[32];
    Image image;
    char *data;
    JsonObject config;

    if (!out || out_len == 0) return -1;
    out[0] = '\0';

    if (!image_ref) {
        strncpy(out, "/bin/sh", out_len - 1);
        out[out_len - 1] = '\0';
        return 0;
    }

    if (split_name_tag(image_ref, name, sizeof(name), tag, sizeof(tag)) != 0 ||
        registry_find(name, tag, &image) != 0) {
        strncpy(out, "/bin/sh", out_len - 1);
        out[out_len - 1] = '\0';
        return 0;
    }

    data = read_file(image.config_path);
    if (data && json_parse(data, &config) == 0 && json_get(&config, "cmd")) {
        strncpy(out, json_get(&config, "cmd"), out_len - 1);
        out[out_len - 1] = '\0';
    }
    free(data);

    if (strlen(out) == 0) {
        strncpy(out, "/bin/sh", out_len - 1);
        out[out_len - 1] = '\0';
    }

    return 0;
}

static int container_register_network_stub(const char *container_id, const char *name,
                                           char *ip_out, char *ipv6_out) {
    NetworkState state;
    int existing;

    if (!file_exists(NETWORK_STATE)) return 0;
    if (network_assign_ip(container_id, ip_out, ipv6_out) != 0) return -1;
    if (network_read_state(&state) != 0) return -1;

    existing = network_find_connection(&state, container_id);
    if (existing == -1 && state.connection_count < MAX_CONNECTIONS) {
        NetConnection *conn = &state.connections[state.connection_count];
        memset(conn, 0, sizeof(NetConnection));
        strncpy(conn->container_id, container_id, sizeof(conn->container_id) - 1);
        strncpy(conn->container_name, name, sizeof(conn->container_name) - 1);
        snprintf(conn->veth_host, sizeof(conn->veth_host), "stub_%s", container_id);
        strncpy(conn->veth_container, "eth0", sizeof(conn->veth_container) - 1);
        strncpy(conn->ip, ip_out, sizeof(conn->ip) - 1);
        strncpy(conn->ipv6, ipv6_out, sizeof(conn->ipv6) - 1);
        state.connection_count++;
    } else if (existing >= 0) {
        NetConnection *conn = &state.connections[existing];
        strncpy(conn->container_name, name, sizeof(conn->container_name) - 1);
        strncpy(conn->ip, ip_out, sizeof(conn->ip) - 1);
        strncpy(conn->ipv6, ipv6_out, sizeof(conn->ipv6) - 1);
    }

    return network_write_state(&state);
}

int container_get_state_path(const char *container_id, char *path_out, size_t path_len) {
    if (!container_id || !path_out) return -1;
    if (format_buffer(path_out, path_len, "%s/%s/state.json",
                      CONTAINERS_DIR, container_id) != 0) {
        fprintf(stderr, "Error: state path too long for %s\n", container_id);
        return -1;
    }
    return 0;
}

int container_load_state_json(const char *container_id, JsonObject *obj) {
    char path[MAX_PATH_LEN];
    char *data;

    if (!container_id || !obj) return -1;
    if (container_get_state_path(container_id, path, sizeof(path)) != 0) return -1;
    data = read_file(path);
    if (!data) {
        fprintf(stderr, "Error: container state not found for %s\n", container_id);
        return -1;
    }
    if (json_parse(data, obj) != 0) {
        free(data);
        fprintf(stderr, "Error: failed to parse %s\n", path);
        return -1;
    }
    free(data);
    return 0;
}

int container_save_state_json(const char *container_id, const JsonObject *obj) {
    char path[MAX_PATH_LEN];
    char *json;

    if (!container_id || !obj) return -1;
    if (container_get_state_path(container_id, path, sizeof(path)) != 0) return -1;
    json = json_stringify_pretty(obj);
    if (!json) return -1;
    if (write_file(path, json) != 0) {
        free(json);
        return -1;
    }
    free(json);
    return 0;
}

int env_parse(int argc, char *argv[], char **envs, int *count) {
    int env_count = 0;

    if (!envs || !count) return -1;

    for (int i = 2; i < argc && env_count < MAX_ENV_VARS; i++) {
        const char *value = NULL;

        if (strncmp(argv[i], "--env=", 6) == 0) {
            value = argv[i] + 6;
        } else if (strcmp(argv[i], "--env") == 0 && i + 1 < argc) {
            value = argv[++i];
        }

        if (!value) continue;
        if (!strchr(value, '=')) {
            fprintf(stderr, "Error: invalid env format '%s'. Use KEY=VALUE\n", value);
            return -1;
        }

        envs[env_count] = strdup(value);
        if (!envs[env_count]) return -1;
        env_count++;
    }

    *count = env_count;
    return 0;
}

int env_apply(char **envs, int count) {
    for (int i = 0; i < count; i++) {
        char *sep;
        if (!envs[i]) continue;
        sep = strchr(envs[i], '=');
        if (!sep) continue;
        *sep = '\0';
        setenv(envs[i], sep + 1, 1);
        *sep = '=';
    }
    return 0;
}

int env_list_cmd(const char *container_id, int json_output) {
    JsonObject state;
    const char *env_json;
    char envs[MAX_ENV_VARS][MAX_LINE];
    int count;

    if (!container_id) return -1;
    if (container_load_state_json(container_id, &state) != 0) return -1;

    env_json = json_get(&state, "env");
    if (!env_json) env_json = "[]";

    count = json_parse_string_array(env_json, envs, MAX_ENV_VARS);
    if (count < 0) count = 0;

    if (json_output) {
        char *values[MAX_ENV_VARS];
        JsonObject result;
        char *arr_json;
        char *json;
        for (int i = 0; i < count; i++) values[i] = envs[i];
        arr_json = json_string_array_from_list(values, count);
        if (!arr_json) return -1;
        result.count = 0;
        json_set(&result, "container_id", container_id);
        json_set_raw(&result, "env", arr_json);
        json = json_stringify_pretty(&result);
        free(arr_json);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("Environment for %s:\n", container_id);
        for (int i = 0; i < count; i++) printf("%s\n", envs[i]);
        if (count == 0) printf("(none)\n");
    }

    return 0;
}

int should_restart(const char *container_id, int exit_status) {
    JsonObject state;
    const char *policy;
    int restart_count;
    int max_restarts;

    if (!container_id) return 0;
    if (container_load_state_json(container_id, &state) != 0) return 0;

    policy = json_get(&state, "restart_policy");
    restart_count = atoi(json_get(&state, "restart_count") ? json_get(&state, "restart_count") : "0");
    max_restarts = atoi(json_get(&state, "max_restarts") ? json_get(&state, "max_restarts") : "5");

    if (!policy || strcmp(policy, "no") == 0) return 0;
    if (restart_count >= max_restarts) return 0;
    if (strcmp(policy, "always") == 0) return 1;
    if (strcmp(policy, "on-failure") == 0) return exit_status != 0;
    return 0;
}

int restart_container(const char *container_id) {
    JsonObject state;
    char count_str[16];
    int restart_count;
    int max_restarts;

    if (!container_id) return -1;
    if (container_load_state_json(container_id, &state) != 0) return -1;

    restart_count = atoi(json_get(&state, "restart_count") ? json_get(&state, "restart_count") : "0") + 1;
    max_restarts = atoi(json_get(&state, "max_restarts") ? json_get(&state, "max_restarts") : "5");
    snprintf(count_str, sizeof(count_str), "%d", restart_count);
    json_set_raw(&state, "restart_count", count_str);

    if (container_save_state_json(container_id, &state) != 0) return -1;

    char message[MAX_LINE];
    snprintf(message, sizeof(message),
             "Restarting container %s (attempt %d/%d)",
             container_id, restart_count, max_restarts);
    logs_append_line(container_id, message);
    return 0;
}

int container_monitor(const char *container_id) {
    if (!container_id) return -1;
    if (should_restart(container_id, 1)) {
        return restart_container(container_id);
    }
    return 0;
}

int container_rename(const char *container_id, const char *new_name, int json_output) {
    JsonObject state;
    NetworkState network_state;

    if (!container_id || !new_name) return -1;
    if (container_load_state_json(container_id, &state) != 0) return -1;

    json_set(&state, "name", new_name);
    if (container_save_state_json(container_id, &state) != 0) return -1;

    if (file_exists(NETWORK_STATE) && network_read_state(&network_state) == 0) {
        int idx = network_find_connection(&network_state, container_id);
        if (idx >= 0) {
            strncpy(network_state.connections[idx].container_name, new_name,
                    sizeof(network_state.connections[idx].container_name) - 1);
            network_write_state(&network_state);
        }
    }

    dns_update_hosts(NULL);

    if (json_output) {
        printf("{\n  \"success\": true,\n  \"container_id\": \"%s\",\n  \"name\": \"%s\"\n}\n",
               container_id, new_name);
    } else {
        printf("Renamed %s -> %s\n", container_id, new_name);
    }

    return 0;
}

int run_container(int argc, char *argv[]) {
    const char *name = get_flag_value(argc, argv, "--name");
    const char *image = get_flag_value(argc, argv, "--image");
    const char *cpuset = get_flag_value(argc, argv, "--cpuset");
    const char *restart_policy = get_flag_value(argc, argv, "--restart");
    const char *max_restart_value = get_flag_value(argc, argv, "--max-restarts");
    const char *security_profile = get_flag_value(argc, argv, "--security");
    const char *stack_name = get_flag_value(argc, argv, "--stack");
    const char *healthcheck = get_flag_value(argc, argv, "--healthcheck");
    const char *health_interval = get_flag_value(argc, argv, "--healthcheck-interval");
    const char *health_timeout = get_flag_value(argc, argv, "--healthcheck-timeout");
    const char *health_retries = get_flag_value(argc, argv, "--healthcheck-retries");
    int json_output = has_json_flag(argc, argv);
    int privileged = has_flag_local(argc, argv, "--privileged");
    int rootless = has_flag_local(argc, argv, "--rootless") || !privileged;
    char *envs[MAX_ENV_VARS] = {0};
    int env_count = 0;
    Volume volumes[MAX_VOLUMES];
    int volume_count = 0;
    int command_start = -1;
    char command[256] = "";
    char container_id[ID_LEN + 1];
    char container_name[64];
    char timestamp[32];
    char container_dir[MAX_PATH_LEN];
    char overlay_lower[MAX_PATH_LEN];
    char overlay_upper[MAX_PATH_LEN];
    char overlay_work[MAX_PATH_LEN];
    char rootfs[MAX_PATH_LEN];
    char state_path[MAX_PATH_LEN];
    char ip[32] = "";
    char ipv6[64] = "";
    char *env_json = NULL;
    char *volumes_json = NULL;
    char *overlay_json = NULL;
    char *health_json = NULL;
    char *state_json = NULL;
    int max_restarts = max_restart_value ? atoi(max_restart_value) : 5;

    if (argc < 2) {
        fprintf(stderr, "Usage: mycontainer run [options] <command>\n");
        return 1;
    }

    if (!restart_policy || strlen(restart_policy) == 0) restart_policy = "no";
    if (!security_profile || strlen(security_profile) == 0) security_profile = "none";
    if (max_restarts <= 0) max_restarts = 5;

    if (env_parse(argc, argv, envs, &env_count) != 0) {
        container_free_envs(envs, env_count);
        return 1;
    }

    for (int i = 2; i < argc && volume_count < MAX_VOLUMES; i++) {
        const char *value = NULL;
        if (strncmp(argv[i], "--volume=", 9) == 0) {
            value = argv[i] + 9;
        } else if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
            value = argv[++i];
        }
        if (!value) continue;
        if (volume_parse(value, &volumes[volume_count]) != 0) {
            container_free_envs(envs, env_count);
            return 1;
        }
        volume_count++;
    }

    for (int i = 2; i < argc; i++) {
        if (container_flag_takes_value(argv[i])) {
            i++;
            continue;
        }
        if (container_is_known_flag(argv[i])) {
            continue;
        }
        command_start = i;
        break;
    }

    if (command_start != -1) {
        for (int i = command_start; i < argc; i++) {
            if (i > command_start) {
                strncat(command, " ", sizeof(command) - strlen(command) - 1);
            }
            strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
        }
    } else {
        container_default_command(image, command, sizeof(command));
    }

    generate_id(container_id);
    strncpy(container_name, name ? name : container_id, sizeof(container_name) - 1);
    container_name[sizeof(container_name) - 1] = '\0';
    get_timestamp(timestamp, sizeof(timestamp));

    if (format_buffer(container_dir, sizeof(container_dir), "%s/%s",
                      CONTAINERS_DIR, container_id) != 0 ||
        format_buffer(overlay_lower, sizeof(overlay_lower), "%s/overlay/lower", container_dir) != 0 ||
        format_buffer(overlay_upper, sizeof(overlay_upper), "%s/overlay/upper", container_dir) != 0 ||
        format_buffer(overlay_work, sizeof(overlay_work), "%s/overlay/work", container_dir) != 0 ||
        format_buffer(rootfs, sizeof(rootfs), "%s/rootfs", container_dir) != 0 ||
        format_buffer(state_path, sizeof(state_path), "%s/state.json", container_dir) != 0) {
        container_free_envs(envs, env_count);
        return 1;
    }

    if (mkdir_p(overlay_lower) != 0 || mkdir_p(overlay_upper) != 0 ||
        mkdir_p(overlay_work) != 0 || mkdir_p(rootfs) != 0) {
        container_free_envs(envs, env_count);
        return 1;
    }

    if (image) {
        char img_name[64];
        char img_tag[32];
        if (split_name_tag(image, img_name, sizeof(img_name), img_tag, sizeof(img_tag)) == 0) {
            if (registry_pull(img_name, img_tag, rootfs) != 0) {
                container_free_envs(envs, env_count);
                return 1;
            }
        }
    }

    if (env_apply(envs, env_count) != 0 ||
        volume_mount(volumes, volume_count, rootfs) != 0 ||
        seccomp_apply(security_profile) != 0) {
        container_free_envs(envs, env_count);
        return 1;
    }

    if (container_register_network_stub(container_id, container_name, ip, ipv6) != 0) {
        ip[0] = '\0';
        ipv6[0] = '\0';
    }

    if (logs_init(container_id) == 0) {
        char message[MAX_LINE];
        snprintf(message, sizeof(message), "Started %s", command);
        logs_append_line(container_id, message);
        if (image && strlen(image) > 0) {
            snprintf(message, sizeof(message), "Using image %s", image);
            logs_append_line(container_id, message);
        }
        if (strlen(ip) > 0) {
            snprintf(message, sizeof(message), "Assigned network %s (%s)", ip, ipv6);
            logs_append_line(container_id, message);
        }
    }

    env_json = json_string_array_from_list(envs, env_count);
    volumes_json = container_volumes_to_json(volumes, volume_count);
    if (!env_json || !volumes_json) {
        free(env_json);
        free(volumes_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    JsonObject overlay;
    overlay.count = 0;
    json_set(&overlay, "lower", overlay_lower);
    json_set(&overlay, "upper", overlay_upper);
    json_set(&overlay, "work", overlay_work);
    overlay_json = json_stringify_pretty(&overlay);
    if (!overlay_json) {
        free(env_json);
        free(volumes_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    JsonObject health;
    health.count = 0;
    json_set(&health, "command", healthcheck ? healthcheck : "");
    json_set_raw(&health, "interval", health_interval ? health_interval : "30");
    json_set_raw(&health, "timeout", health_timeout ? health_timeout : "10");
    json_set_raw(&health, "retries", health_retries ? health_retries : "3");
    json_set(&health, "status",
             healthcheck && strlen(healthcheck) > 0 ? "starting" : "disabled");
    json_set(&health, "last_check", timestamp);
    json_set_raw(&health, "consecutive_failures", "0");
    health_json = json_stringify_pretty(&health);
    if (!health_json) {
        free(env_json);
        free(volumes_json);
        free(overlay_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    JsonObject state;
    char pid_str[16];
    char max_restarts_str[16];

    snprintf(pid_str, sizeof(pid_str), "%d", 0);
    snprintf(max_restarts_str, sizeof(max_restarts_str), "%d", max_restarts);
    state.count = 0;
    json_set(&state, "id", container_id);
    json_set(&state, "name", container_name);
    json_set(&state, "status", "running");
    json_set_raw(&state, "pid", pid_str);
    json_set(&state, "image", image ? image : "");
    json_set(&state, "command", command);
    json_set(&state, "ip", ip);
    json_set(&state, "ipv6", ipv6);
    json_set(&state, "runtime_mode", privileged ? "privileged" : "rootless");
    json_set_raw(&state, "rootless", rootless ? "true" : "false");
    json_set_raw(&state, "privileged", privileged ? "true" : "false");
    json_set(&state, "cpuset", cpuset ? cpuset : "");
    json_set(&state, "restart_policy", restart_policy);
    json_set_raw(&state, "restart_count", "0");
    json_set_raw(&state, "max_restarts", max_restarts_str);
    json_set(&state, "security_profile", security_profile);
    json_set(&state, "stack_name", stack_name ? stack_name : "");
    json_set_raw(&state, "env", env_json);
    json_set_raw(&state, "volumes", volumes_json);
    json_set_raw(&state, "healthcheck", health_json);
    json_set(&state, "rootfs", rootfs);
    json_set_raw(&state, "overlay", overlay_json);
    json_set(&state, "created_at", timestamp);

    state_json = json_stringify_pretty(&state);
    if (!state_json) {
        free(env_json);
        free(volumes_json);
        free(overlay_json);
        free(health_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    if (write_file(state_path, state_json) != 0) {
        free(env_json);
        free(volumes_json);
        free(overlay_json);
        free(health_json);
        free(state_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    dns_update_hosts(NULL);

    if (json_output) {
        printf("{\n");
        printf("  \"id\": \"%s\",\n", container_id);
        printf("  \"name\": \"%s\",\n", container_name);
        printf("  \"status\": \"running\",\n");
        printf("  \"image\": \"%s\",\n", image ? image : "");
        printf("  \"command\": \"%s\",\n", command);
        printf("  \"ip\": \"%s\",\n", ip);
        printf("  \"ipv6\": \"%s\"\n", ipv6);
        printf("}\n");
    } else {
        printf("Container %s is running.\n", container_id);
        printf("  Name:    %s\n", container_name);
        printf("  Mode:    %s\n", privileged ? "privileged" : "rootless");
        printf("  Command: %s\n", command);
        if (image) printf("  Image:   %s\n", image);
        if (strlen(ip) > 0) printf("  Network: %s (%s)\n", ip, ipv6);
        printf("  Rootfs:  %s\n", rootfs);
        printf("[STUB] Runtime metadata recorded; namespace exec path is still simulated.\n");
    }

    free(env_json);
    free(volumes_json);
    free(overlay_json);
    free(health_json);
    free(state_json);
    container_free_envs(envs, env_count);
    return 0;
}
