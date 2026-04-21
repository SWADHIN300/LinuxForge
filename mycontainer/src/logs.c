#define _GNU_SOURCE
#include "../include/logs.h"

#include <fcntl.h>
#include <signal.h>

static volatile sig_atomic_t g_follow_logs = 1;

static void logs_sigint_handler(int sig) {
    (void) sig;
    g_follow_logs = 0;
}

static int logs_get_path(const char *container_id, char *path, size_t path_len) {
    if (!container_id || !path) return -1;
    if (format_buffer(path, path_len, "%s/%s.log", LOGS_DIR, container_id) != 0) {
        fprintf(stderr, "Error: log path too long for %s\n", container_id);
        return -1;
    }
    return 0;
}

static void logs_parse_line(const char *line, char *timestamp, size_t ts_len,
                            char *message, size_t msg_len) {
    const char *start;
    const char *end;

    if (timestamp && ts_len > 0) timestamp[0] = '\0';
    if (message && msg_len > 0) message[0] = '\0';
    if (!line) return;

    start = strchr(line, '[');
    end = start ? strchr(start, ']') : NULL;
    if (start && end && end > start + 1) {
        size_t len = (size_t) (end - start - 1);
        if (len >= ts_len) len = ts_len - 1;
        memcpy(timestamp, start + 1, len);
        timestamp[len] = '\0';

        while (*(end + 1) == ' ') end++;
        strncpy(message, end + 1, msg_len - 1);
        message[msg_len - 1] = '\0';
    } else {
        get_timestamp(timestamp, ts_len);
        strncpy(message, line, msg_len - 1);
        message[msg_len - 1] = '\0';
    }

    str_trim(message);
}

int logs_rotate(const char *container_id) {
    char path[MAX_PATH_LEN];
    char rotated[MAX_PATH_LEN];
    long size;

    if (logs_get_path(container_id, path, sizeof(path)) != 0) return -1;
    if (!file_exists(path)) return 0;

    size = get_file_size(path);
    if (size < 0 || size <= 10L * 1024L * 1024L) {
        return 0;
    }

    if (format_buffer(rotated, sizeof(rotated), "%s.1", path) != 0) {
        fprintf(stderr, "Error: rotated log path too long for %s\n", container_id);
        return -1;
    }

    remove(rotated);
    if (rename(path, rotated) != 0) {
        fprintf(stderr, "Error: failed to rotate log %s: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

int logs_init(const char *container_id) {
    char path[MAX_PATH_LEN];
    char timestamp[32];
    char header[MAX_LINE];
    int fd;

    if (!container_id) return -1;
    if (mkdir_p(LOGS_DIR) != 0) return -1;
    if (logs_rotate(container_id) != 0) return -1;
    if (logs_get_path(container_id, path, sizeof(path)) != 0) return -1;

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    get_timestamp(timestamp, sizeof(timestamp));
    snprintf(header, sizeof(header), "=== Container started %s ===\n", timestamp);
    if (write(fd, header, strlen(header)) < 0) {
        fprintf(stderr, "Error: cannot write log header for %s: %s\n",
                container_id, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int logs_append_line(const char *container_id, const char *message) {
    char path[MAX_PATH_LEN];
    char timestamp[32];
    int fd;
    char copy[MAX_BUF];
    char *line;
    char *cursor;

    if (!container_id || !message) return -1;
    if (mkdir_p(LOGS_DIR) != 0) return -1;
    if (logs_rotate(container_id) != 0) return -1;
    if (logs_get_path(container_id, path, sizeof(path)) != 0) return -1;

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    strncpy(copy, message, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    cursor = copy;

    while ((line = strsep_local(&cursor, "\n")) != NULL) {
        char entry[MAX_BUF];
        if (strlen(line) == 0) continue;
        get_timestamp(timestamp, sizeof(timestamp));
        snprintf(entry, sizeof(entry), "[%s] %s\n", timestamp, line);
        if (write(fd, entry, strlen(entry)) < 0) {
            fprintf(stderr, "Error: cannot append log for %s: %s\n",
                    container_id, strerror(errno));
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

int logs_read(const char *container_id, int tail_lines, int json_output) {
    char path[MAX_PATH_LEN];
    char *data;
    char **lines = NULL;
    int total = 0;
    int capacity = 0;
    int start = 0;

    if (!container_id) return -1;
    if (logs_get_path(container_id, path, sizeof(path)) != 0) return -1;
    if (!file_exists(path)) {
        fprintf(stderr, "Error: logs not found for %s\n", container_id);
        return -1;
    }

    data = read_file(path);
    if (!data) {
        fprintf(stderr, "Error: cannot read %s\n", path);
        return -1;
    }

    for (char *line = strtok(data, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (capacity <= total) {
            int next_capacity = capacity == 0 ? 32 : capacity * 2;
            char **next = realloc(lines, (size_t) next_capacity * sizeof(char *));
            if (!next) {
                free(lines);
                free(data);
                return -1;
            }
            lines = next;
            capacity = next_capacity;
        }
        lines[total] = strdup(line);
        if (!lines[total]) {
            for (int i = 0; i < total; i++) free(lines[i]);
            free(lines);
            free(data);
            return -1;
        }
        total++;
    }

    free(data);
    if (tail_lines > 0 && tail_lines < total) {
        start = total - tail_lines;
    }

    if (json_output) {
        JsonArray *arr = json_array_new();
        JsonObject result;
        char total_str[32];
        char *arr_str;
        char *json;

        if (!arr) {
            for (int i = 0; i < total; i++) free(lines[i]);
            free(lines);
            return -1;
        }

        for (int i = start; i < total; i++) {
            char ts[64];
            char msg[MAX_BUF];
            JsonObject item;

            logs_parse_line(lines[i], ts, sizeof(ts), msg, sizeof(msg));
            item.count = 0;
            json_set(&item, "timestamp", ts);
            json_set(&item, "message", msg);
            json_array_append(arr, &item);
        }

        arr_str = json_array_stringify_pretty(arr);
        if (!arr_str) {
            json_array_free(arr);
            for (int i = 0; i < total; i++) free(lines[i]);
            free(lines);
            return -1;
        }

        result.count = 0;
        snprintf(total_str, sizeof(total_str), "%d", total);
        json_set(&result, "container_id", container_id);
        json_set_raw(&result, "lines", arr_str);
        json_set_raw(&result, "total_lines", total_str);
        json = json_stringify_pretty(&result);
        free(arr_str);
        json_array_free(arr);

        if (!json) {
            for (int i = 0; i < total; i++) free(lines[i]);
            free(lines);
            return -1;
        }

        printf("%s\n", json);
        free(json);
    } else {
        for (int i = start; i < total; i++) {
            printf("%s\n", lines[i]);
        }
    }

    for (int i = 0; i < total; i++) free(lines[i]);
    free(lines);
    return 0;
}

int logs_follow(const char *container_id) {
    char path[MAX_PATH_LEN];
    FILE *fp;
    long position = 0;

    if (!container_id) return -1;
    if (logs_get_path(container_id, path, sizeof(path)) != 0) return -1;
    if (!file_exists(path)) {
        fprintf(stderr, "Error: logs not found for %s\n", container_id);
        return -1;
    }

    signal(SIGINT, logs_sigint_handler);

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    position = ftell(fp);

    while (g_follow_logs) {
        long size = get_file_size(path);
        if (size < position) {
            fclose(fp);
            fp = fopen(path, "r");
            if (!fp) return -1;
            position = 0;
        }

        if (size > position) {
            char line[MAX_BUF];
            fseek(fp, position, SEEK_SET);
            while (fgets(line, sizeof(line), fp)) {
                fputs(line, stdout);
            }
            fflush(stdout);
            position = ftell(fp);
        }

        usleep(500000);
    }

    fclose(fp);
    return 0;
}

int logs_clear(const char *container_id, int json_output) {
    char path[MAX_PATH_LEN];

    if (!container_id) return -1;
    if (logs_get_path(container_id, path, sizeof(path)) != 0) return -1;
    if (write_file(path, "") != 0) return -1;

    if (json_output) {
        printf("{\n  \"success\": true,\n  \"container_id\": \"%s\"\n}\n", container_id);
    } else {
        printf("Logs cleared for %s\n", container_id);
    }

    return 0;
}

int logs_cmd(int argc, char *argv[]) {
    const char *tail = get_flag_value(argc, argv, "--tail");
    int follow = 0;
    int clear = 0;
    int json_output = has_json_flag(argc, argv);

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer logs <id> [--tail=N] [--follow] [--clear] [--json]\n");
        return 1;
    }

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--follow") == 0) follow = 1;
        if (strcmp(argv[i], "--clear") == 0) clear = 1;
    }

    if (clear) return logs_clear(argv[2], json_output);
    if (follow) return logs_follow(argv[2]);
    return logs_read(argv[2], tail ? atoi(tail) : 0, json_output);
}
