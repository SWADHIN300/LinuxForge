/*
 * container.c — Container Lifecycle Management
 */

#define _GNU_SOURCE
#include "../include/container.h"
#include "../include/cgroups.h"
#include "../include/commit.h"
#include "../include/dns.h"
#include "../include/fs.h"
#include "../include/health.h"
#include "../include/logs.h"
#include "../include/network.h"
#include "../include/registry.h"
#include "../include/security.h"
#include "../include/volume.h"

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONTAINER_STACK_SIZE (1024 * 1024)

typedef struct {
    char container_id[ID_LEN + 1];
    char container_name[64];
    char rootfs[MAX_PATH_LEN];
    char overlay_lower[MAX_PATH_LEN];
    char overlay_upper[MAX_PATH_LEN];
    char overlay_work[MAX_PATH_LEN];
    char state_path[MAX_PATH_LEN];
    char log_path[MAX_PATH_LEN];
    char security_profile[32];
    char command[256];
    char env_values[MAX_ENV_VARS][MAX_LINE];
    int env_count;
    Volume volumes[MAX_VOLUMES];
    int volume_count;
    int sync_fd;
} ContainerRuntimeConfig;

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

static int container_extract_image(const char *image_ref, const char *dest_path) {
    char name[64];
    char tag[32];
    Image image;
    char cmd[MAX_CMD_LEN];

    if (!image_ref || strlen(image_ref) == 0 || !dest_path) return 0;

    if (split_name_tag(image_ref, name, sizeof(name), tag, sizeof(tag)) != 0 ||
        registry_find(name, tag, &image) != 0) {
        fprintf(stderr, "Error: image %s not found\n", image_ref);
        return -1;
    }

    if (mkdir_p(dest_path) != 0) return -1;
    if (format_buffer(cmd, sizeof(cmd), "tar -xzf \"%s\" -C \"%s\" 2>/dev/null",
                      image.rootfs_path, dest_path) != 0) {
        return -1;
    }

    return system(cmd) == 0 ? 0 : -1;
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

static int container_log_path(const char *container_id, char *path_out, size_t path_len) {
    return format_buffer(path_out, path_len, "%s/%s.log", LOGS_DIR, container_id);
}

static int container_update_state(const char *container_id,
                                  const char *status,
                                  int pid,
                                  const char *ip,
                                  const char *ipv6) {
    JsonObject state;
    char pid_str[16];

    if (container_load_state_json(container_id, &state) != 0) return -1;

    if (status) json_set(&state, "status", status);
    if (pid >= 0) {
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        json_set_raw(&state, "pid", pid_str);
    }
    if (ip) json_set(&state, "ip", ip);
    if (ipv6) json_set(&state, "ipv6", ipv6);

    return container_save_state_json(container_id, &state);
}

static int container_sync_network_state(const char *container_id,
                                        char *ip_out, size_t ip_len,
                                        char *ipv6_out, size_t ipv6_len) {
    NetworkState state;
    int idx;

    if (ip_out && ip_len > 0) ip_out[0] = '\0';
    if (ipv6_out && ipv6_len > 0) ipv6_out[0] = '\0';

    if (network_read_state(&state) != 0) return -1;
    idx = network_find_connection(&state, container_id);
    if (idx < 0) return -1;

    if (ip_out && ip_len > 0) {
        strncpy(ip_out, state.connections[idx].ip, ip_len - 1);
        ip_out[ip_len - 1] = '\0';
    }
    if (ipv6_out && ipv6_len > 0) {
        strncpy(ipv6_out, state.connections[idx].ipv6, ipv6_len - 1);
        ipv6_out[ipv6_len - 1] = '\0';
    }

    return 0;
}

static int container_prepare_runtime_config(ContainerRuntimeConfig *cfg,
                                            const char *container_id,
                                            const char *container_name,
                                            const char *rootfs,
                                            const char *overlay_lower,
                                            const char *overlay_upper,
                                            const char *overlay_work,
                                            const char *state_path,
                                            const char *security_profile,
                                            const char *command,
                                            char **envs,
                                            int env_count,
                                            Volume *volumes,
                                            int volume_count,
                                            int sync_fd) {
    if (!cfg) return -1;

    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->container_id, container_id, sizeof(cfg->container_id) - 1);
    strncpy(cfg->container_name, container_name, sizeof(cfg->container_name) - 1);
    strncpy(cfg->rootfs, rootfs, sizeof(cfg->rootfs) - 1);
    strncpy(cfg->overlay_lower, overlay_lower, sizeof(cfg->overlay_lower) - 1);
    strncpy(cfg->overlay_upper, overlay_upper, sizeof(cfg->overlay_upper) - 1);
    strncpy(cfg->overlay_work, overlay_work, sizeof(cfg->overlay_work) - 1);
    strncpy(cfg->state_path, state_path, sizeof(cfg->state_path) - 1);
    strncpy(cfg->security_profile, security_profile ? security_profile : "none",
            sizeof(cfg->security_profile) - 1);
    strncpy(cfg->command, command, sizeof(cfg->command) - 1);
    cfg->sync_fd = sync_fd;

    if (container_log_path(container_id, cfg->log_path, sizeof(cfg->log_path)) != 0) {
        return -1;
    }

    cfg->env_count = env_count < MAX_ENV_VARS ? env_count : MAX_ENV_VARS;
    for (int i = 0; i < cfg->env_count; i++) {
        strncpy(cfg->env_values[i], envs[i], sizeof(cfg->env_values[i]) - 1);
    }

    cfg->volume_count = volume_count < MAX_VOLUMES ? volume_count : MAX_VOLUMES;
    for (int i = 0; i < cfg->volume_count; i++) {
        memcpy(&cfg->volumes[i], &volumes[i], sizeof(Volume));
    }

    return 0;
}

static int container_runtime_supported(void) {
#ifdef __linux__
    return geteuid() == 0;
#else
    return 0;
#endif
}

static int container_redirect_logs(const char *log_path) {
    int fd;

    fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;

    if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
        close(fd);
        return -1;
    }

    if (fd > STDERR_FILENO) close(fd);
    return 0;
}

static int container_exec_payload(const ContainerRuntimeConfig *cfg) {
    char *env_ptrs[MAX_ENV_VARS];

    if (container_redirect_logs(cfg->log_path) != 0) {
        perror("redirect logs");
        _exit(112);
    }

    if (chroot(cfg->rootfs) != 0 || chdir("/") != 0) {
        perror("chroot");
        _exit(113);
    }

    for (int i = 0; i < cfg->env_count; i++) {
        env_ptrs[i] = (char *) cfg->env_values[i];
    }
    if (env_apply(env_ptrs, cfg->env_count) != 0) {
        perror("env_apply");
        _exit(114);
    }

    if (seccomp_apply(cfg->security_profile) != 0) {
        perror("seccomp");
        _exit(115);
    }

    execl("/bin/sh", "sh", "-lc", cfg->command, (char *) NULL);
    perror("exec");
    _exit(127);
}

static int container_runtime_main(void *arg) {
    ContainerRuntimeConfig *cfg = (ContainerRuntimeConfig *) arg;
    char ready = 0;
    pid_t payload_pid;
    int status;

    if (!cfg) return 125;

    if (read(cfg->sync_fd, &ready, 1) != 1) {
        close(cfg->sync_fd);
        return 126;
    }
    close(cfg->sync_fd);

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        perror("mount private");
        return 127;
    }

    if (sethostname(cfg->container_name, strlen(cfg->container_name)) != 0) {
        perror("sethostname");
        return 128;
    }

    if (setup_rootfs(cfg->rootfs, cfg->overlay_lower, cfg->overlay_upper, cfg->overlay_work) != 0) {
        perror("setup_rootfs");
        return 129;
    }

    if (volume_mount(cfg->volumes, cfg->volume_count, cfg->rootfs) != 0) {
        perror("volume_mount");
        return 130;
    }

    payload_pid = fork();
    if (payload_pid < 0) {
        perror("fork");
        return 131;
    }

    if (payload_pid == 0) {
        container_exec_payload(cfg);
    }

    while (waitpid(payload_pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        status = 255;
        break;
    }

    if (WIFEXITED(status)) {
        char message[MAX_LINE];
        snprintf(message, sizeof(message), "Process exited with code %d", WEXITSTATUS(status));
        logs_append_line(cfg->container_id, message);
    } else if (WIFSIGNALED(status)) {
        char message[MAX_LINE];
        snprintf(message, sizeof(message), "Process terminated by signal %d", WTERMSIG(status));
        logs_append_line(cfg->container_id, message);
    }

    container_update_state(cfg->container_id, "exited", 0, NULL, NULL);
    network_cleanup(cfg->container_id);
    dns_update_hosts(NULL);

    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
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

    {
        char message[MAX_LINE];
        snprintf(message, sizeof(message),
                 "Restarting container %s (attempt %d/%d)",
                 container_id, restart_count, max_restarts);
        logs_append_line(container_id, message);
    }
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
    int runtime_live = 0;

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
        if (container_extract_image(image, overlay_lower) != 0) {
            container_free_envs(envs, env_count);
            return 1;
        }
    }

    if (seccomp_load_profile(security_profile, &(SecurityProfile){0}) != 0 &&
        strcmp(security_profile, "none") != 0) {
        container_free_envs(envs, env_count);
        return 1;
    }

    if (logs_init(container_id) == 0) {
        char message[MAX_LINE];
        snprintf(message, sizeof(message), "Started %s", command);
        logs_append_line(container_id, message);
        if (image && strlen(image) > 0) {
            snprintf(message, sizeof(message), "Using image %s", image);
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

    {
        JsonObject overlay;
        overlay.count = 0;
        json_set(&overlay, "lower", overlay_lower);
        json_set(&overlay, "upper", overlay_upper);
        json_set(&overlay, "work", overlay_work);
        overlay_json = json_stringify_pretty(&overlay);
    }
    if (!overlay_json) {
        free(env_json);
        free(volumes_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    {
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
    }
    if (!health_json) {
        free(env_json);
        free(volumes_json);
        free(overlay_json);
        container_free_envs(envs, env_count);
        return 1;
    }

    {
        JsonObject state;
        char pid_str[16];
        char max_restarts_str[16];

        snprintf(pid_str, sizeof(pid_str), "%d", 0);
        snprintf(max_restarts_str, sizeof(max_restarts_str), "%d", max_restarts);
        state.count = 0;
        json_set(&state, "id", container_id);
        json_set(&state, "name", container_name);
        json_set(&state, "status", "created");
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
    }
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

    if (container_runtime_supported()) {
        ContainerRuntimeConfig runtime_cfg;
        char *stack_mem = NULL;
        int sync_pipe[2] = { -1, -1 };
        pid_t init_pid;

        if (pipe(sync_pipe) == 0 &&
            container_prepare_runtime_config(&runtime_cfg,
                                             container_id, container_name,
                                             rootfs, overlay_lower, overlay_upper, overlay_work,
                                             state_path, security_profile, command,
                                             envs, env_count, volumes, volume_count,
                                             sync_pipe[0]) == 0) {
            stack_mem = malloc(CONTAINER_STACK_SIZE);
            if (stack_mem) {
                int clone_flags = CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID |
                                  CLONE_NEWIPC | CLONE_NEWNET | SIGCHLD;
                init_pid = clone(container_runtime_main,
                                 stack_mem + CONTAINER_STACK_SIZE,
                                 clone_flags,
                                 &runtime_cfg);
                if (init_pid > 0) {
                    close(sync_pipe[0]);
                    if (setup_cgroups(container_id, init_pid, cpuset) == 0) {
                        if (file_exists(NETWORK_STATE) && network_setup_container(container_id, init_pid) == 0) {
                            container_sync_network_state(container_id, ip, sizeof(ip), ipv6, sizeof(ipv6));
                            if (strlen(ip) > 0) {
                                char message[MAX_LINE];
                                snprintf(message, sizeof(message), "Assigned network %s (%s)", ip, ipv6);
                                logs_append_line(container_id, message);
                            }
                        }
                        container_update_state(container_id, "running", (int) init_pid,
                                               strlen(ip) > 0 ? ip : NULL,
                                               strlen(ipv6) > 0 ? ipv6 : NULL);
                        if (write(sync_pipe[1], "1", 1) == 1) {
                            runtime_live = 1;
                        }
                    }

                    close(sync_pipe[1]);
                    if (!runtime_live) {
                        kill(init_pid, SIGKILL);
                        waitpid(init_pid, NULL, 0);
                    }
                } else {
                    close(sync_pipe[0]);
                    close(sync_pipe[1]);
                }
            } else {
                close(sync_pipe[0]);
                close(sync_pipe[1]);
            }
        } else {
            if (sync_pipe[0] >= 0) close(sync_pipe[0]);
            if (sync_pipe[1] >= 0) close(sync_pipe[1]);
        }

        free(stack_mem);
    }

    if (!runtime_live) {
        if (container_register_network_stub(container_id, container_name, ip, ipv6) != 0) {
            ip[0] = '\0';
            ipv6[0] = '\0';
        }
        container_update_state(container_id, "running", 0,
                               strlen(ip) > 0 ? ip : NULL,
                               strlen(ipv6) > 0 ? ipv6 : NULL);
        logs_append_line(container_id,
                         "Runtime bootstrap unavailable; recorded metadata only.");
    }

    if (json_output) {
        printf("{\n");
        printf("  \"id\": \"%s\",\n", container_id);
        printf("  \"name\": \"%s\",\n", container_name);
        printf("  \"status\": \"running\",\n");
        printf("  \"image\": \"%s\",\n", image ? image : "");
        printf("  \"command\": \"%s\",\n", command);
        printf("  \"ip\": \"%s\",\n", ip);
        printf("  \"ipv6\": \"%s\",\n", ipv6);
        printf("  \"runtime_live\": %s\n", runtime_live ? "true" : "false");
        printf("}\n");
    } else {
        printf("Container %s is running.\n", container_id);
        printf("  Name:    %s\n", container_name);
        printf("  Mode:    %s\n", privileged ? "privileged" : "rootless");
        printf("  Command: %s\n", command);
        if (image) printf("  Image:   %s\n", image);
        if (strlen(ip) > 0) printf("  Network: %s (%s)\n", ip, ipv6);
        printf("  Rootfs:  %s\n", rootfs);
        if (!runtime_live) {
            printf("[INFO] Runtime metadata recorded; live namespace exec path requires Linux root privileges.\n");
        }
    }

    free(env_json);
    free(volumes_json);
    free(overlay_json);
    free(health_json);
    free(state_json);
    container_free_envs(envs, env_count);
    return 0;
}
