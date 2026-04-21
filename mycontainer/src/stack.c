#define _GNU_SOURCE
#include "../include/stack.h"
#include "../include/container.h"
#include "../include/health.h"
#include "../include/dns.h"
#include "../include/network.h"
#include "../include/volume.h"

typedef struct {
    char name[64];
    char image[96];
    char port[32];
    char memory[32];
    char cpu[32];
    char env_json[MAX_BUF];
    char volumes_json[MAX_BUF];
    char healthcheck[256];
    char restart[16];
    int network;
} StackContainerConfig;

typedef struct {
    char name[64];
    char version[16];
    char source_file[MAX_PATH_LEN];
    int connect_all;
    StackContainerConfig containers[16];
    int count;
} StackConfig;

static int stack_state_path(const char *stack_name, char *path, size_t path_len) {
    if (format_buffer(path, path_len, "stacks/%s.state.json", stack_name) != 0) {
        fprintf(stderr, "Error: stack state path too long for %s\n", stack_name);
        return -1;
    }
    return 0;
}

static int stack_parse_file(const char *stack_file, StackConfig *config) {
    char *data;
    JsonObject root;
    JsonArray *containers;
    JsonObject network;
    const char *container_json;
    const char *network_json;

    if (!stack_file || !config) return -1;
    if (!file_exists(stack_file)) {
        fprintf(stderr, "Error: stack file not found: %s\n", stack_file);
        return -1;
    }

    data = read_file(stack_file);
    if (!data) return -1;
    if (json_parse(data, &root) != 0) {
        free(data);
        fprintf(stderr, "Error: failed to parse stack file %s\n", stack_file);
        return -1;
    }
    free(data);

    memset(config, 0, sizeof(StackConfig));
    strncpy(config->name, json_get(&root, "name") ? json_get(&root, "name") : "stack",
            sizeof(config->name) - 1);
    strncpy(config->version, json_get(&root, "version") ? json_get(&root, "version") : "1.0",
            sizeof(config->version) - 1);
    strncpy(config->source_file, stack_file, sizeof(config->source_file) - 1);

    network_json = json_get(&root, "network");
    if (network_json && json_parse(network_json, &network) == 0) {
        config->connect_all = (strcmp(json_get(&network, "connect_all") ? json_get(&network, "connect_all") : "false", "true") == 0 ||
                               strcmp(json_get(&network, "connect_all") ? json_get(&network, "connect_all") : "0", "1") == 0);
    }

    container_json = json_get(&root, "containers");
    if (!container_json) {
        container_json = json_get(&root, "services");
    }
    if (!container_json) {
        fprintf(stderr, "Error: stack file missing \"containers\" (or \"services\") array\n");
        return -1;
    }

    containers = json_array_new();
    if (!containers) return -1;
    if (json_parse_array(container_json, containers) != 0) {
        json_array_free(containers);
        return -1;
    }

    for (int i = 0; i < containers->count && i < 16; i++) {
        const JsonObject *obj = &containers->objects[i];
        StackContainerConfig *item = &config->containers[config->count];
        const char *image = json_get(obj, "image");
        Image existing;
        char img_name[64];
        char img_tag[32];

        if (!image) {
            fprintf(stderr, "Error: stack container %d missing image\n", i);
            json_array_free(containers);
            return -1;
        }

        if (split_name_tag(image, img_name, sizeof(img_name), img_tag, sizeof(img_tag)) != 0 ||
            registry_find(img_name, img_tag, &existing) != 0) {
            fprintf(stderr, "Error: image not found in registry: %s\n", image);
            json_array_free(containers);
            return -1;
        }

        strncpy(item->name, json_get(obj, "name") ? json_get(obj, "name") : image,
                sizeof(item->name) - 1);
        strncpy(item->image, image, sizeof(item->image) - 1);
        strncpy(item->port, json_get(obj, "port") ? json_get(obj, "port") : "",
                sizeof(item->port) - 1);
        strncpy(item->memory, json_get(obj, "memory") ? json_get(obj, "memory") : "",
                sizeof(item->memory) - 1);
        strncpy(item->cpu, json_get(obj, "cpu") ? json_get(obj, "cpu") : "",
                sizeof(item->cpu) - 1);
        strncpy(item->restart, json_get(obj, "restart") ? json_get(obj, "restart") : "no",
                sizeof(item->restart) - 1);
        strncpy(item->healthcheck, json_get(obj, "healthcheck") ? json_get(obj, "healthcheck") : "",
                sizeof(item->healthcheck) - 1);
        strncpy(item->env_json, json_get(obj, "env") ? json_get(obj, "env") : "[]",
                sizeof(item->env_json) - 1);
        strncpy(item->volumes_json, json_get(obj, "volumes") ? json_get(obj, "volumes") : "[]",
                sizeof(item->volumes_json) - 1);
        item->network = (strcmp(json_get(obj, "network") ? json_get(obj, "network") : "false", "true") == 0 ||
                         strcmp(json_get(obj, "network") ? json_get(obj, "network") : "0", "1") == 0);
        config->count++;
    }

    json_array_free(containers);
    return 0;
}

static int stack_find_container_by_name(const char *name, char *container_id, size_t id_len) {
    DIR *dir;
    struct dirent *entry;
    int found = 0;
    char best_created_at[32] = "";

    if (!name || !container_id) return -1;
    if (!dir_exists(CONTAINERS_DIR)) return -1;

    dir = opendir(CONTAINERS_DIR);
    if (!dir) return -1;

    while ((entry = readdir(dir)) != NULL) {
        JsonObject state;
        const char *container_name;
        if (entry->d_name[0] == '.') continue;
        if (container_load_state_json(entry->d_name, &state) != 0) continue;
        container_name = json_get(&state, "name");
        if (container_name && strcmp(container_name, name) == 0) {
            const char *created_at = json_get(&state, "created_at");

            if (!found ||
                (created_at && strcmp(created_at, best_created_at) > 0)) {
                strncpy(container_id, entry->d_name, id_len - 1);
                container_id[id_len - 1] = '\0';
                strncpy(best_created_at, created_at ? created_at : "",
                        sizeof(best_created_at) - 1);
                best_created_at[sizeof(best_created_at) - 1] = '\0';
                found = 1;
            }
        }
    }

    closedir(dir);
    return found ? 0 : -1;
}

static int stack_guess_command(const char *image_ref, char *command, size_t command_len) {
    char name[64];
    char tag[32];
    Image image;
    char *data;
    JsonObject config;

    if (!image_ref || !command || command_len == 0) return -1;
    command[0] = '\0';

    if (split_name_tag(image_ref, name, sizeof(name), tag, sizeof(tag)) != 0 ||
        registry_find(name, tag, &image) != 0) {
        strncpy(command, "/bin/sh", command_len - 1);
        command[command_len - 1] = '\0';
        return 0;
    }

    data = read_file(image.config_path);
    if (data && json_parse(data, &config) == 0 && json_get(&config, "cmd")) {
        strncpy(command, json_get(&config, "cmd"), command_len - 1);
        command[command_len - 1] = '\0';
    }
    free(data);

    if (strlen(command) == 0) {
        strncpy(command, "/bin/sh", command_len - 1);
        command[command_len - 1] = '\0';
    }

    return 0;
}

static int stack_write_state(const StackConfig *config, char ids[][16], int id_count) {
    char path[MAX_PATH_LEN];
    JsonObject obj;
    char *values[16];
    char *ids_json;
    char *json;
    char timestamp[32];

    if (!config) return -1;
    if (mkdir_p("stacks") != 0) return -1;
    if (stack_state_path(config->name, path, sizeof(path)) != 0) return -1;

    for (int i = 0; i < id_count; i++) {
        values[i] = ids[i];
    }
    ids_json = json_string_array_from_list(values, id_count);
    if (!ids_json) return -1;

    get_timestamp(timestamp, sizeof(timestamp));
    obj.count = 0;
    json_set(&obj, "name", config->name);
    json_set(&obj, "status", "running");
    json_set(&obj, "source_file", config->source_file);
    json_set_raw(&obj, "containers", ids_json);
    json_set(&obj, "created_at", timestamp);
    json = json_stringify_pretty(&obj);
    free(ids_json);
    if (!json) return -1;
    if (write_file(path, json) != 0) {
        free(json);
        return -1;
    }
    free(json);
    return 0;
}

int stack_up(const char *stack_file, int json_output) {
    StackConfig config;
    char container_ids[16][16];
    int started = 0;

    if (stack_parse_file(stack_file, &config) != 0) return -1;

    if (!json_output) {
        printf("Starting %s...\n", config.name);
    }

    for (int i = 0; i < config.count; i++) {
        StackContainerConfig *item = &config.containers[i];
        char command[256];
        char *argv_run[64];
        char name_opt[128];
        char image_opt[128];
        char restart_opt[64];
        char health_opt[320];
        char env_opts[16][MAX_CMD_LEN];
        char volume_opts[16][MAX_CMD_LEN];
        char env_values[16][MAX_LINE];
        char volume_values[16][MAX_LINE];
        int env_count = 0;
        int volume_count = 0;
        int argc_run = 0;

        if (!json_output) {
            printf("[%d/%d] Starting %s (%s)...\n",
                   i + 1, config.count, item->name, item->image);
        }

        stack_guess_command(item->image, command, sizeof(command));
        env_count = json_parse_string_array(item->env_json, env_values, 16);
        if (env_count < 0) env_count = 0;
        volume_count = json_parse_string_array(item->volumes_json, volume_values, 16);
        if (volume_count < 0) volume_count = 0;

        argv_run[argc_run++] = "mycontainer";
        argv_run[argc_run++] = "run";
        snprintf(name_opt, sizeof(name_opt), "--name=%s", item->name);
        snprintf(image_opt, sizeof(image_opt), "--image=%s", item->image);
        snprintf(restart_opt, sizeof(restart_opt), "--restart=%s", item->restart);
        argv_run[argc_run++] = name_opt;
        argv_run[argc_run++] = image_opt;
        argv_run[argc_run++] = restart_opt;

        if (strlen(item->healthcheck) > 0) {
            snprintf(health_opt, sizeof(health_opt), "--healthcheck=%s", item->healthcheck);
            argv_run[argc_run++] = health_opt;
        }

        for (int j = 0; j < env_count && j < 16; j++) {
            snprintf(env_opts[j], sizeof(env_opts[j]), "--env=%s", env_values[j]);
            argv_run[argc_run++] = env_opts[j];
        }

        for (int j = 0; j < volume_count && j < 16; j++) {
            snprintf(volume_opts[j], sizeof(volume_opts[j]), "--volume=%s", volume_values[j]);
            argv_run[argc_run++] = volume_opts[j];
        }

        argv_run[argc_run++] = command;
        if (run_container(argc_run, argv_run) != 0) {
            return -1;
        }

        if (stack_find_container_by_name(item->name, container_ids[started], sizeof(container_ids[started])) != 0) {
            fprintf(stderr, "Error: failed to resolve container ID for %s\n", item->name);
            return -1;
        }

        JsonObject state;
        if (container_load_state_json(container_ids[started], &state) == 0) {
            json_set(&state, "stack_name", config.name);
            container_save_state_json(container_ids[started], &state);
        }

        if (strlen(item->healthcheck) > 0) {
            int exit_code = health_run_check(container_ids[started]);
            health_update_status(container_ids[started], exit_code);
        }

        started++;
    }

    if (config.connect_all && started > 1 && file_exists(NETWORK_STATE)) {
        for (int i = 0; i < started; i++) {
            for (int j = i + 1; j < started; j++) {
                network_connect(container_ids[i], container_ids[j]);
            }
        }
    }

    dns_update_hosts(NULL);
    stack_write_state(&config, container_ids, started);

    if (json_output) {
        printf("{\n");
        printf("  \"success\": true,\n");
        printf("  \"name\": \"%s\",\n", config.name);
        printf("  \"count\": %d\n", started);
        printf("}\n");
    } else {
        printf("Stack %s is up\n", config.name);
    }

    return 0;
}

int stack_down(const char *stack_file_or_name, int json_output) {
    StackConfig config;
    char state_path[MAX_PATH_LEN];
    char *data;
    JsonObject state;
    const char *ids_json;
    char ids[16][MAX_LINE];
    int id_count;

    if (file_exists(stack_file_or_name)) {
        if (stack_parse_file(stack_file_or_name, &config) != 0) return -1;
    } else {
        memset(&config, 0, sizeof(config));
        strncpy(config.name, stack_file_or_name, sizeof(config.name) - 1);
    }

    if (stack_state_path(config.name, state_path, sizeof(state_path)) != 0) return -1;
    data = read_file(state_path);
    if (!data || json_parse(data, &state) != 0) {
        free(data);
        fprintf(stderr, "Error: stack state not found for %s\n", config.name);
        return -1;
    }
    free(data);

    ids_json = json_get(&state, "containers");
    id_count = ids_json ? json_parse_string_array(ids_json, ids, 16) : 0;
    if (id_count < 0) id_count = 0;

    for (int i = 0; i < id_count; i++) {
        JsonObject container_state;
        if (container_load_state_json(ids[i], &container_state) == 0) {
            json_set(&container_state, "status", "stopped");
            container_save_state_json(ids[i], &container_state);
            volume_unmount(ids[i]);
            network_cleanup(ids[i]);
        }
    }

    json_set(&state, "status", "stopped");
    {
        char *state_json = json_stringify_pretty(&state);
        if (!state_json) return -1;
        if (write_file(state_path, state_json) != 0) {
            free(state_json);
            return -1;
        }
        free(state_json);
    }
    dns_update_hosts(NULL);

    if (json_output) {
        printf("{\n  \"success\": true,\n  \"name\": \"%s\"\n}\n", config.name);
    } else {
        printf("Stack %s is down\n", config.name);
    }

    return 0;
}

int stack_status(const char *stack_name, int json_output) {
    char state_path[MAX_PATH_LEN];
    char *data;
    JsonObject state;
    const char *ids_json;
    char ids[16][MAX_LINE];
    int id_count;

    if (!stack_name) return -1;
    if (stack_state_path(stack_name, state_path, sizeof(state_path)) != 0) return -1;

    data = read_file(state_path);
    if (!data || json_parse(data, &state) != 0) {
        free(data);
        fprintf(stderr, "Error: stack state not found for %s\n", stack_name);
        return -1;
    }
    free(data);

    ids_json = json_get(&state, "containers");
    id_count = ids_json ? json_parse_string_array(ids_json, ids, 16) : 0;
    if (id_count < 0) id_count = 0;

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) return -1;

        for (int i = 0; i < id_count; i++) {
            JsonObject container_state;
            JsonObject item;
            if (container_load_state_json(ids[i], &container_state) != 0) continue;
            item.count = 0;
            json_set(&item, "id", ids[i]);
            json_set(&item, "name", json_get(&container_state, "name") ? json_get(&container_state, "name") : ids[i]);
            json_set(&item, "status", json_get(&container_state, "status") ? json_get(&container_state, "status") : "unknown");
            json_set(&item, "ip", json_get(&container_state, "ip") ? json_get(&container_state, "ip") : "");
            json_array_append(arr, &item);
        }

        char *arr_str = json_array_stringify_pretty(arr);
        JsonObject result;
        char *json;
        json_array_free(arr);
        if (!arr_str) return -1;
        result.count = 0;
        json_set(&result, "name", stack_name);
        json_set_raw(&result, "containers", arr_str);
        json = json_stringify_pretty(&result);
        free(arr_str);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("Stack %s\n", stack_name);
        printf("%-10s %-18s %-12s %s\n", "ID", "NAME", "STATUS", "IP");
        printf("%-10s %-18s %-12s %s\n", "--", "----", "------", "--");
        for (int i = 0; i < id_count; i++) {
            JsonObject container_state;
            if (container_load_state_json(ids[i], &container_state) != 0) continue;
            printf("%-10s %-18s %-12s %s\n",
                   ids[i],
                   json_get(&container_state, "name") ? json_get(&container_state, "name") : ids[i],
                   json_get(&container_state, "status") ? json_get(&container_state, "status") : "unknown",
                   json_get(&container_state, "ip") ? json_get(&container_state, "ip") : "-");
        }
    }

    return 0;
}

int stack_list(int json_output) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_exists("stacks")) {
        printf(json_output ? "[]\n" : "No stacks found.\n");
        return 0;
    }

    dir = opendir("stacks");
    if (!dir) return -1;

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) {
            closedir(dir);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            char *suffix;
            JsonObject state;
            char path[MAX_PATH_LEN];
            char *data;

            if (entry->d_name[0] == '.') continue;
            suffix = strstr(entry->d_name, ".state.json");
            if (!suffix) continue;
            if (format_buffer(path, sizeof(path), "stacks/%s", entry->d_name) != 0) continue;
            data = read_file(path);
            if (!data || json_parse(data, &state) != 0) {
                free(data);
                continue;
            }
            free(data);
            json_array_append(arr, &state);
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
        printf("%-20s %-12s %s\n", "STACK", "STATUS", "CREATED");
        printf("%-20s %-12s %s\n", "-----", "------", "-------");
        while ((entry = readdir(dir)) != NULL) {
            char path[MAX_PATH_LEN];
            JsonObject state;
            char *data;

            if (entry->d_name[0] == '.') continue;
            if (!strstr(entry->d_name, ".state.json")) continue;
            if (format_buffer(path, sizeof(path), "stacks/%s", entry->d_name) != 0) continue;
            data = read_file(path);
            if (!data || json_parse(data, &state) != 0) {
                free(data);
                continue;
            }
            free(data);
            printf("%-20s %-12s %s\n",
                   json_get(&state, "name") ? json_get(&state, "name") : entry->d_name,
                   json_get(&state, "status") ? json_get(&state, "status") : "unknown",
                   json_get(&state, "created_at") ? json_get(&state, "created_at") : "-");
        }
    }

    closedir(dir);
    return 0;
}

int stack_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer stack up <file> [--json]\n"
                        "       mycontainer stack down <file|name> [--json]\n"
                        "       mycontainer stack status <name> [--json]\n"
                        "       mycontainer stack ls [--json]\n");
        return 1;
    }

    if (strcmp(argv[2], "up") == 0 && argc >= 4) {
        return stack_up(argv[3], json_output);
    }
    if (strcmp(argv[2], "down") == 0 && argc >= 4) {
        return stack_down(argv[3], json_output);
    }
    if (strcmp(argv[2], "status") == 0 && argc >= 4) {
        return stack_status(argv[3], json_output);
    }
    if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) {
        return stack_list(json_output);
    }

    fprintf(stderr, "Unknown stack command: %s\n", argv[2]);
    return 1;
}
