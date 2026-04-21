#define _GNU_SOURCE
#include "../include/stats.h"
#include "../include/container.h"
#include "../include/network.h"
#include <signal.h>

static int stats_read_long_file(const char *path, long *value) {
    char *data;

    if (!path || !value || !file_exists(path)) return -1;

    data = read_file(path);
    if (!data) return -1;
    *value = atol(data);
    free(data);
    return 0;
}

static void stats_format_timestamp(time_t value, char *out, size_t out_len) {
    struct tm *tm_info;
    tm_info = gmtime(&value);
    if (!tm_info) {
        if (out_len > 0) out[0] = '\0';
        return;
    }
    strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

static long stats_parse_network_dev(const char *content, int column_index) {
    char copy[MAX_BUF * 4];
    char *line;
    char *cursor;

    if (!content) return 0;

    strncpy(copy, content, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    cursor = copy;

    while ((line = strsep_local(&cursor, "\n")) != NULL) {
        char iface[64];
        unsigned long values[16] = {0};
        int parsed;

        if (strstr(line, "eth0") == NULL) continue;

        parsed = sscanf(line,
                        " %63[^:]: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                        iface,
                        &values[0], &values[1], &values[2], &values[3],
                        &values[4], &values[5], &values[6], &values[7],
                        &values[8], &values[9], &values[10], &values[11],
                        &values[12], &values[13], &values[14], &values[15]);
        if (parsed >= 10 && column_index >= 0 && column_index < 16) {
            return (long) values[column_index];
        }
    }

    return 0;
}

static int stats_collect_all_current(int json_output) {
    DIR *dir;
    struct dirent *entry;
    JsonArray *arr;

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
        arr = json_array_new();
        if (!arr) {
            closedir(dir);
            return -1;
        }
    } else {
        arr = NULL;
        printf("%-10s %-8s %-18s %-18s %s\n", "ID", "CPU%", "MEMORY", "NET RX/TX", "TIME");
        printf("%-10s %-8s %-18s %-18s %s\n", "--", "----", "------", "---------", "----");
    }

    while ((entry = readdir(dir)) != NULL) {
        ContainerStats stats;
        char timestamp[32];

        if (entry->d_name[0] == '.') continue;
        if (stats_collect(entry->d_name, &stats) != 0) continue;
        stats_save(&stats);

        if (json_output) {
            char ts[32];
            char cpu_str[32];
            char rx_str[32];
            char tx_str[32];
            char mem_str[32];
            char limit_str[32];
            JsonObject item;

            stats_format_timestamp(stats.timestamp, ts, sizeof(ts));
            snprintf(cpu_str, sizeof(cpu_str), "%.2f", stats.cpu_percent);
            snprintf(mem_str, sizeof(mem_str), "%ld", stats.memory_usage);
            snprintf(limit_str, sizeof(limit_str), "%ld", stats.memory_limit);
            snprintf(rx_str, sizeof(rx_str), "%ld", stats.net_rx_bytes);
            snprintf(tx_str, sizeof(tx_str), "%ld", stats.net_tx_bytes);

            item.count = 0;
            json_set(&item, "container_id", stats.container_id);
            json_set_raw(&item, "cpu_percent", cpu_str);
            json_set_raw(&item, "memory_usage", mem_str);
            json_set_raw(&item, "memory_limit", limit_str);
            json_set_raw(&item, "net_rx_bytes", rx_str);
            json_set_raw(&item, "net_tx_bytes", tx_str);
            json_set(&item, "timestamp", ts);
            json_array_append(arr, &item);
        } else {
            char mem_usage[16];
            char mem_limit[16];
            char net_rx[16];
            char net_tx[16];
            stats_format_timestamp(stats.timestamp, timestamp, sizeof(timestamp));
            format_size(stats.memory_usage, mem_usage, sizeof(mem_usage));
            format_size(stats.memory_limit, mem_limit, sizeof(mem_limit));
            format_size(stats.net_rx_bytes, net_rx, sizeof(net_rx));
            format_size(stats.net_tx_bytes, net_tx, sizeof(net_tx));
            printf("%-10s %-8.1f %-18s %-18s %s\n",
                   stats.container_id,
                   stats.cpu_percent,
                   mem_usage,
                   net_rx,
                   timestamp);
            printf("           memory: %s/%s   tx: %s\n",
                   mem_usage, mem_limit, net_tx);
        }
    }

    closedir(dir);

    if (json_output) {
        char *json = json_array_stringify_pretty(arr);
        json_array_free(arr);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    }

    return 0;
}

int stats_read_cpu(const char *container_id, double *cpu_percent) {
    char path[MAX_PATH_LEN];
    long usage_ns;

    if (!container_id || !cpu_percent) return -1;
    *cpu_percent = 0.0;

    if (format_buffer(path, sizeof(path),
                      "/sys/fs/cgroup/cpu/%s/cpuacct.usage", container_id) == 0 &&
        stats_read_long_file(path, &usage_ns) == 0) {
        *cpu_percent = (double) usage_ns / 100000000.0;
        return 0;
    }

    return 0;
}

int stats_read_memory(const char *container_id, long *usage, long *limit) {
    char path[MAX_PATH_LEN];
    ContainerState state;

    if (!container_id || !usage || !limit) return -1;
    *usage = 0;
    *limit = 512L * 1024L * 1024L;

    if (format_buffer(path, sizeof(path),
                      "/sys/fs/cgroup/memory/%s/memory.usage_in_bytes", container_id) == 0 &&
        stats_read_long_file(path, usage) == 0) {
        if (format_buffer(path, sizeof(path),
                          "/sys/fs/cgroup/memory/%s/memory.limit_in_bytes", container_id) == 0) {
            stats_read_long_file(path, limit);
        }
        return 0;
    }

    if (read_container_state(container_id, &state) == 0 && dir_exists(state.rootfs)) {
        *usage = 0;
    }

    return 0;
}

int stats_read_network(const char *container_id, long *rx_bytes, long *tx_bytes) {
    ContainerState state;
    char path[MAX_PATH_LEN];
    char *data;

    if (!container_id || !rx_bytes || !tx_bytes) return -1;
    *rx_bytes = 0;
    *tx_bytes = 0;

    if (read_container_state(container_id, &state) != 0) return -1;
    if (state.pid <= 0) return 0;

    if (format_buffer(path, sizeof(path), "/proc/%d/net/dev", state.pid) != 0 ||
        !file_exists(path)) {
        return 0;
    }

    data = read_file(path);
    if (!data) return 0;
    *rx_bytes = stats_parse_network_dev(data, 0);
    *tx_bytes = stats_parse_network_dev(data, 8);
    free(data);
    return 0;
}

int stats_collect(const char *container_id, ContainerStats *stats) {
    if (!container_id || !stats) return -1;

    memset(stats, 0, sizeof(ContainerStats));
    strncpy(stats->container_id, container_id, sizeof(stats->container_id) - 1);
    stats_read_cpu(container_id, &stats->cpu_percent);
    stats_read_memory(container_id, &stats->memory_usage, &stats->memory_limit);
    stats_read_network(container_id, &stats->net_rx_bytes, &stats->net_tx_bytes);
    stats->timestamp = time(NULL);
    return 0;
}

int stats_save(const ContainerStats *stats) {
    char dir[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    char line[MAX_BUF];

    if (!stats) return -1;
    if (mkdir_p(STATS_DIR) != 0) return -1;
    if (format_buffer(dir, sizeof(dir), "%s/%s.db", STATS_DIR, stats->container_id) != 0) {
        return -1;
    }

    if (format_buffer(path, sizeof(path), "%s", dir) != 0) return -1;
    if (snprintf(line, sizeof(line),
                 "{\"container_id\":\"%s\",\"cpu_percent\":%.2f,"
                 "\"memory_usage\":%ld,\"memory_limit\":%ld,"
                 "\"net_rx\":%ld,\"net_tx\":%ld,\"timestamp\":%ld}\n",
                 stats->container_id,
                 stats->cpu_percent,
                 stats->memory_usage,
                 stats->memory_limit,
                 stats->net_rx_bytes,
                 stats->net_tx_bytes,
                 (long) stats->timestamp) >= (int) sizeof(line)) {
        fprintf(stderr, "Error: stats record too large for %s\n", stats->container_id);
        return -1;
    }

    return append_file(path, line);
}

int stats_history(const char *container_id, int minutes, int json_output) {
    char path[MAX_PATH_LEN];
    char *data;
    char *line;
    char *cursor;
    time_t cutoff;
    JsonArray *arr;

    if (!container_id) return -1;
    if (format_buffer(path, sizeof(path), "%s/%s.db", STATS_DIR, container_id) != 0) return -1;
    if (!file_exists(path)) {
        fprintf(stderr, "Error: no stats history for %s\n", container_id);
        return -1;
    }

    data = read_file(path);
    if (!data) return -1;
    cursor = data;
    cutoff = time(NULL) - (minutes > 0 ? minutes * 60 : 0);
    arr = json_array_new();
    if (!arr) {
        free(data);
        return -1;
    }

    while ((line = strsep_local(&cursor, "\n")) != NULL) {
        JsonObject obj;
        const char *ts;
        if (strlen(line) == 0) continue;
        if (json_parse(line, &obj) != 0) continue;
        ts = json_get(&obj, "timestamp");
        if (minutes > 0 && ts && atol(ts) < cutoff) continue;
        json_array_append(arr, &obj);
    }

    if (json_output) {
        JsonObject result;
        char *arr_str = json_array_stringify_pretty(arr);
        char *json;

        if (!arr_str) {
            json_array_free(arr);
            free(data);
            return -1;
        }

        result.count = 0;
        json_set(&result, "container_id", container_id);
        json_set_raw(&result, "datapoints", arr_str);
        json = json_stringify_pretty(&result);
        free(arr_str);
        if (!json) {
            json_array_free(arr);
            free(data);
            return -1;
        }
        printf("%s\n", json);
        free(json);
    } else {
        printf("%-22s %-8s %-12s %-12s %s\n", "TIMESTAMP", "CPU%", "MEMORY", "LIMIT", "NET");
        printf("%-22s %-8s %-12s %-12s %s\n", "---------", "----", "------", "-----", "---");
        for (int i = 0; i < arr->count; i++) {
            char ts_iso[32];
            char usage[16];
            char limit[16];
            char net[32];
            const char *ts = json_get(&arr->objects[i], "timestamp");
            const char *cpu = json_get(&arr->objects[i], "cpu_percent");
            const char *mem = json_get(&arr->objects[i], "memory_usage");
            const char *lim = json_get(&arr->objects[i], "memory_limit");
            const char *rx = json_get(&arr->objects[i], "net_rx");
            const char *tx = json_get(&arr->objects[i], "net_tx");

            stats_format_timestamp((time_t) atol(ts ? ts : "0"), ts_iso, sizeof(ts_iso));
            format_size(atol(mem ? mem : "0"), usage, sizeof(usage));
            format_size(atol(lim ? lim : "0"), limit, sizeof(limit));
            snprintf(net, sizeof(net), "%s/%s", rx ? rx : "0", tx ? tx : "0");
            printf("%-22s %-8s %-12s %-12s %s\n",
                   ts_iso, cpu ? cpu : "0", usage, limit, net);
        }
    }

    json_array_free(arr);
    free(data);
    return 0;
}

int stats_print_live(const char *container_id) {
    signal(SIGINT, SIG_DFL);

    while (1) {
        ContainerStats stats;
        char usage[16];
        char limit[16];
        char rx[16];
        char tx[16];

        if (stats_collect(container_id, &stats) != 0) return -1;
        stats_save(&stats);

        format_size(stats.memory_usage, usage, sizeof(usage));
        format_size(stats.memory_limit, limit, sizeof(limit));
        format_size(stats.net_rx_bytes, rx, sizeof(rx));
        format_size(stats.net_tx_bytes, tx, sizeof(tx));

        printf("\033[2J\033[H");
        printf("CONTAINER  CPU%%   MEM USAGE        MEM LIMIT        NET I/O\n");
        printf("%-10s %-6.1f %-16s %-16s %s down / %s up\n",
               stats.container_id, stats.cpu_percent, usage, limit, rx, tx);
        fflush(stdout);
        sleep(1);
    }
}

int stats_cmd(int argc, char *argv[]) {
    const char *history = get_flag_value(argc, argv, "--history");
    int watch = 0;
    int json_output = has_json_flag(argc, argv);

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--watch") == 0) watch = 1;
    }

    if (argc >= 3 && strcmp(argv[2], "--all") == 0) {
        return stats_collect_all_current(json_output);
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer stats <id> [--watch] [--json] [--history=N]\n"
                        "       mycontainer stats --all [--json]\n");
        return 1;
    }

    if (history) return stats_history(argv[2], atoi(history), json_output);
    if (watch) return stats_print_live(argv[2]);

    ContainerStats stats;
    if (stats_collect(argv[2], &stats) != 0) return 1;
    stats_save(&stats);

    if (json_output) {
        char timestamp[32];
        printf("{\n");
        printf("  \"container_id\": \"%s\",\n", stats.container_id);
        printf("  \"cpu_percent\": %.2f,\n", stats.cpu_percent);
        printf("  \"memory_usage\": %ld,\n", stats.memory_usage);
        printf("  \"memory_limit\": %ld,\n", stats.memory_limit);
        printf("  \"memory_percent\": %.2f,\n",
               stats.memory_limit > 0 ? (100.0 * (double) stats.memory_usage / (double) stats.memory_limit) : 0.0);
        printf("  \"net_rx_bytes\": %ld,\n", stats.net_rx_bytes);
        printf("  \"net_tx_bytes\": %ld,\n", stats.net_tx_bytes);
        stats_format_timestamp(stats.timestamp, timestamp, sizeof(timestamp));
        printf("  \"timestamp\": \"%s\"\n", timestamp);
        printf("}\n");
    } else {
        char usage[16];
        char limit[16];
        char rx[16];
        char tx[16];
        format_size(stats.memory_usage, usage, sizeof(usage));
        format_size(stats.memory_limit, limit, sizeof(limit));
        format_size(stats.net_rx_bytes, rx, sizeof(rx));
        format_size(stats.net_tx_bytes, tx, sizeof(tx));
        printf("CONTAINER  CPU%%   MEM USAGE        MEM LIMIT        NET I/O\n");
        printf("%-10s %-6.1f %-16s %-16s %s down / %s up\n",
               stats.container_id, stats.cpu_percent, usage, limit, rx, tx);
    }

    return 0;
}
