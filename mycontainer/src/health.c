#define _GNU_SOURCE
#include "../include/health.h"
#include "../include/container.h"
#include "../include/logs.h"

static int health_get_history_path(const char *container_id, char *path, size_t path_len) {
    if (format_buffer(path, path_len, "%s/%s/health_history.jsonl",
                      CONTAINERS_DIR, container_id) != 0) {
        fprintf(stderr, "Error: health history path too long for %s\n", container_id);
        return -1;
    }
    return 0;
}

static int health_read_config_with_options(const char *container_id, JsonObject *state,
                                           JsonObject *health, int quiet_missing) {
    const char *health_str;

    if (!container_id || !state || !health) return -1;
    if (container_load_state_json(container_id, state) != 0) return -1;

    health_str = json_get(state, "healthcheck");
    if (!health_str || strlen(health_str) == 0) {
        if (!quiet_missing) {
            fprintf(stderr, "Error: no health check configured for %s\n", container_id);
        }
        return -1;
    }

    if (json_parse(health_str, health) != 0) {
        if (!quiet_missing) {
            fprintf(stderr, "Error: invalid healthcheck state for %s\n", container_id);
        }
        return -1;
    }

    return 0;
}

static int health_read_config(const char *container_id, JsonObject *state, JsonObject *health) {
    return health_read_config_with_options(container_id, state, health, 0);
}

static char *health_history_json(const char *container_id) {
    char path[MAX_PATH_LEN];
    char *data;
    char *cursor;
    char *line;
    JsonArray *arr;
    char *json;

    if (health_get_history_path(container_id, path, sizeof(path)) != 0) return NULL;
    if (!file_exists(path)) {
        return strdup("[]");
    }

    data = read_file(path);
    if (!data) return strdup("[]");

    arr = json_array_new();
    if (!arr) {
        free(data);
        return NULL;
    }

    cursor = data;
    while ((line = strsep_local(&cursor, "\n")) != NULL) {
        JsonObject item;
        if (strlen(line) == 0) continue;
        if (json_parse(line, &item) == 0) {
            json_array_append(arr, &item);
        }
    }

    json = json_array_stringify_pretty(arr);
    json_array_free(arr);
    free(data);
    return json ? json : strdup("[]");
}

static int health_print_all(int json_output) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_exists(CONTAINERS_DIR)) {
        printf(json_output ? "[]\n" : "No containers found.\n");
        return 0;
    }

    dir = opendir(CONTAINERS_DIR);
    if (!dir) {
        fprintf(stderr, "Error: cannot open %s: %s\n", CONTAINERS_DIR, strerror(errno));
        return -1;
    }

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) {
            closedir(dir);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            JsonObject state;
            JsonObject health;
            const char *status;
            if (entry->d_name[0] == '.') continue;
            if (health_read_config_with_options(entry->d_name, &state, &health, 1) != 0) continue;
            status = json_get(&health, "status");
            if (!status) status = "unknown";
            json_set(&health, "container_id", entry->d_name);
            json_array_append(arr, &health);
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
        printf("%-10s %-12s %-24s %s\n", "ID", "STATUS", "LAST CHECK", "FAILURES");
        printf("%-10s %-12s %-24s %s\n", "--", "------", "----------", "--------");
        while ((entry = readdir(dir)) != NULL) {
            JsonObject state;
            JsonObject health;
            const char *status;
            const char *last_check;
            const char *failures;
            const char *retries;

            if (entry->d_name[0] == '.') continue;
            if (health_read_config_with_options(entry->d_name, &state, &health, 1) != 0) continue;

            status = json_get(&health, "status");
            last_check = json_get(&health, "last_check");
            failures = json_get(&health, "consecutive_failures");
            retries = json_get(&health, "retries");
            printf("%-10s %-12s %-24s %s/%s\n",
                   entry->d_name,
                   status ? status : "unknown",
                   last_check ? last_check : "-",
                   failures ? failures : "0",
                   retries ? retries : "0");
        }
    }

    closedir(dir);
    return 0;
}

int health_run_check(const char *container_id) {
    JsonObject state;
    JsonObject health;
    const char *command;
    char cmd[MAX_CMD_LEN];
    if (health_read_config(container_id, &state, &health) != 0) return -1;

    command = json_get(&health, "command");
    if (!command || strlen(command) == 0) {
        fprintf(stderr, "Error: no health check command configured for %s\n", container_id);
        return -1;
    }

#ifdef _WIN32
    if (format_buffer(cmd, sizeof(cmd), "cmd /C %s >NUL 2>&1", command) != 0) {
        return -1;
    }
#else
    const char *timeout_value;
    int timeout_seconds = 0;
    timeout_value = json_get(&health, "timeout");
    if (timeout_value) timeout_seconds = atoi(timeout_value);

    if (timeout_seconds > 0) {
        if (format_buffer(cmd, sizeof(cmd),
                          "timeout %d sh -lc '%s' >/dev/null 2>&1",
                          timeout_seconds, command) != 0) {
            return -1;
        }
    } else {
        if (format_buffer(cmd, sizeof(cmd),
                          "sh -lc '%s' >/dev/null 2>&1", command) != 0) {
            return -1;
        }
    }
#endif

    return system(cmd);
}

int health_update_status(const char *container_id, int exit_code) {
    JsonObject state;
    JsonObject health;
    const char *retries_value;
    char timestamp[32];
    int failures;
    int retries;
    char failures_str[16];
    char *health_json;
    char history_path[MAX_PATH_LEN];
    char history_entry[MAX_LINE];

    if (health_read_config(container_id, &state, &health) != 0) return -1;

    retries_value = json_get(&health, "retries");
    failures = atoi(json_get(&health, "consecutive_failures") ? json_get(&health, "consecutive_failures") : "0");
    retries = atoi(retries_value ? retries_value : "3");

    get_timestamp(timestamp, sizeof(timestamp));
    if (exit_code == 0) {
        failures = 0;
        json_set(&health, "status", "healthy");
    } else {
        failures++;
        if (failures >= retries) {
            json_set(&health, "status", "unhealthy");
        } else {
            json_set(&health, "status", "starting");
        }
    }

    snprintf(failures_str, sizeof(failures_str), "%d", failures);
    json_set(&health, "last_check", timestamp);
    json_set_raw(&health, "consecutive_failures", failures_str);

    health_json = json_stringify_pretty(&health);
    if (!health_json) return -1;
    json_set_raw(&state, "healthcheck", health_json);
    free(health_json);

    if (container_save_state_json(container_id, &state) != 0) return -1;

    if (health_get_history_path(container_id, history_path, sizeof(history_path)) == 0) {
        snprintf(history_entry, sizeof(history_entry),
                 "{\"status\":\"%s\",\"time\":\"%s\"}\n",
                 json_get(&health, "status") ? json_get(&health, "status") : "unknown",
                 timestamp);
        append_file(history_path, history_entry);
    }

    logs_append_line(container_id,
                     exit_code == 0 ? "Health check passed" : "Health check failed");

    return 0;
}

void *health_monitor_loop(void *arg) {
    char *container_id = (char *) arg;
    JsonObject state;
    JsonObject health;
    int interval = 30;

    if (!container_id) return NULL;
    if (health_read_config(container_id, &state, &health) != 0) return NULL;
    if (json_get(&health, "interval")) {
        interval = atoi(json_get(&health, "interval"));
    }
    if (interval <= 0) interval = 30;

    while (1) {
        int exit_code = health_run_check(container_id);
        health_update_status(container_id, exit_code);
        sleep((unsigned int) interval);
    }
}

int health_get_status(const char *container_id, int json_output) {
    JsonObject state;
    JsonObject health;
    char *history_json;

    if (health_read_config(container_id, &state, &health) != 0) return -1;

    history_json = health_history_json(container_id);
    if (!history_json) return -1;

    if (json_output) {
        JsonObject result;
        char *json;

        result.count = 0;
        json_set(&result, "container_id", container_id);
        json_set(&result, "status", json_get(&health, "status") ? json_get(&health, "status") : "unknown");
        json_set(&result, "command", json_get(&health, "command") ? json_get(&health, "command") : "");
        json_set_raw(&result, "interval", json_get(&health, "interval") ? json_get(&health, "interval") : "0");
        json_set_raw(&result, "timeout", json_get(&health, "timeout") ? json_get(&health, "timeout") : "0");
        json_set_raw(&result, "retries", json_get(&health, "retries") ? json_get(&health, "retries") : "0");
        json_set(&result, "last_check", json_get(&health, "last_check") ? json_get(&health, "last_check") : "");
        json_set_raw(&result, "consecutive_failures",
                     json_get(&health, "consecutive_failures") ? json_get(&health, "consecutive_failures") : "0");
        json_set_raw(&result, "history", history_json);
        json = json_stringify_pretty(&result);
        free(history_json);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("Container:  %s (%s)\n",
               container_id,
               json_get(&state, "name") ? json_get(&state, "name") : container_id);
        printf("Status:     %s\n", json_get(&health, "status") ? json_get(&health, "status") : "unknown");
        printf("Last check: %s\n", json_get(&health, "last_check") ? json_get(&health, "last_check") : "-");
        printf("Failures:   %s/%s\n",
               json_get(&health, "consecutive_failures") ? json_get(&health, "consecutive_failures") : "0",
               json_get(&health, "retries") ? json_get(&health, "retries") : "0");
        free(history_json);
    }

    return 0;
}

int health_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);
    int run_now = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0) run_now = 1;
    }

    if (argc >= 3 && strcmp(argv[2], "--all") == 0) {
        return health_print_all(json_output);
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer health <id> [--run] [--json]\n"
                        "       mycontainer health --all [--json]\n");
        return 1;
    }

    if (run_now) {
        int exit_code = health_run_check(argv[2]);
        health_update_status(argv[2], exit_code);
        if (json_output) {
            printf("{\n  \"container_id\": \"%s\",\n  \"status\": \"%s\",\n  \"exit_code\": %d\n}\n",
                   argv[2],
                   exit_code == 0 ? "healthy" : "unhealthy",
                   exit_code == 0 ? 0 : 1);
            return exit_code == 0 ? 0 : 1;
        }
    }

    return health_get_status(argv[2], json_output);
}
