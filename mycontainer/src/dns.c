#define _GNU_SOURCE
#include "../include/dns.h"
#include "../include/container.h"

typedef struct {
    char id[16];
    char name[64];
    char ip[32];
    char rootfs[MAX_PATH_LEN];
} DnsEntry;

static int dns_collect_entries(DnsEntry *entries, int max_entries) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if (!dir_exists(CONTAINERS_DIR)) return 0;

    dir = opendir(CONTAINERS_DIR);
    if (!dir) return -1;

    while ((entry = readdir(dir)) != NULL && count < max_entries) {
        JsonObject state;
        const char *ip;

        if (entry->d_name[0] == '.') continue;
        if (container_load_state_json(entry->d_name, &state) != 0) continue;

        ip = json_get(&state, "ip");
        if (!ip || strlen(ip) == 0) continue;

        strncpy(entries[count].id, entry->d_name, sizeof(entries[count].id) - 1);
        strncpy(entries[count].name,
                json_get(&state, "name") ? json_get(&state, "name") : entry->d_name,
                sizeof(entries[count].name) - 1);
        strncpy(entries[count].ip, ip, sizeof(entries[count].ip) - 1);
        strncpy(entries[count].rootfs,
                json_get(&state, "rootfs") ? json_get(&state, "rootfs") : "",
                sizeof(entries[count].rootfs) - 1);
        count++;
    }

    closedir(dir);
    return count;
}

static int dns_write_hosts_file(const char *rootfs_path, DnsEntry *entries, int count) {
    char etc_dir[MAX_PATH_LEN];
    char hosts_path[MAX_PATH_LEN];
    char *buffer;
    size_t buf_size = MAX_BUF * 4;
    int pos = 0;

    if (!rootfs_path || strlen(rootfs_path) == 0) return 0;

    buffer = calloc(1, buf_size);
    if (!buffer) return -1;

    if (format_buffer(etc_dir, sizeof(etc_dir), "%s/etc", rootfs_path) != 0 ||
        format_buffer(hosts_path, sizeof(hosts_path), "%s/hosts", etc_dir) != 0) {
        free(buffer);
        return -1;
    }

    if (mkdir_p(etc_dir) != 0) {
        free(buffer);
        return -1;
    }

    pos += snprintf(buffer + pos, buf_size - pos,
                    "127.0.0.1 localhost\n"
                    "::1 localhost ip6-localhost ip6-loopback\n"
                    "172.18.0.1 host\n");
    for (int i = 0; i < count; i++) {
        pos += snprintf(buffer + pos, buf_size - pos, "%s %s\n",
                        entries[i].ip, entries[i].name);
        if ((size_t) pos >= buf_size) break;
    }

    if (write_file(hosts_path, buffer) != 0) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

int dns_update_hosts(const char *container_id) {
    DnsEntry entries[64];
    int count;

    count = dns_collect_entries(entries, 64);
    if (count < 0) {
        fprintf(stderr, "Error: failed to read container DNS state\n");
        return -1;
    }

    if (container_id && strlen(container_id) > 0) {
        JsonObject state;
        if (container_load_state_json(container_id, &state) != 0) return -1;
        return dns_write_hosts_file(json_get(&state, "rootfs"), entries, count);
    }

    for (int i = 0; i < count; i++) {
        if (dns_write_hosts_file(entries[i].rootfs, entries, count) != 0) {
            return -1;
        }
    }

    return 0;
}

int dns_add_entry(const char *name, const char *ip) {
    (void) name;
    (void) ip;
    return dns_update_hosts(NULL);
}

int dns_remove_entry(const char *name) {
    (void) name;
    return dns_update_hosts(NULL);
}

int dns_list(int json_output) {
    DnsEntry entries[64];
    int count = dns_collect_entries(entries, 64);

    if (count < 0) return -1;

    if (json_output) {
        JsonArray *arr = json_array_new();
        JsonObject result;
        char *arr_str;
        char *json;

        if (!arr) return -1;
        for (int i = 0; i < count; i++) {
            JsonObject item;
            item.count = 0;
            json_set(&item, "name", entries[i].name);
            json_set(&item, "ip", entries[i].ip);
            json_set(&item, "container_id", entries[i].id);
            json_array_append(arr, &item);
        }

        arr_str = json_array_stringify_pretty(arr);
        json_array_free(arr);
        if (!arr_str) return -1;

        result.count = 0;
        json_set_raw(&result, "entries", arr_str);
        json = json_stringify_pretty(&result);
        free(arr_str);
        if (!json) return -1;
        printf("%s\n", json);
        free(json);
    } else {
        printf("%-18s %-16s %s\n", "NAME", "IP", "CONTAINER ID");
        printf("%-18s %-16s %s\n", "----", "--", "------------");
        for (int i = 0; i < count; i++) {
            printf("%-18s %-16s %s\n",
                   entries[i].name, entries[i].ip, entries[i].id);
        }
        if (count == 0) {
            printf("No DNS entries available.\n");
        }
    }

    return 0;
}

int dns_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer dns ls [--json]\n"
                        "       mycontainer dns update [--json]\n");
        return 1;
    }

    if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) {
        return dns_list(json_output);
    }

    if (strcmp(argv[2], "update") == 0 || strcmp(argv[2], "refresh") == 0) {
        if (dns_update_hosts(NULL) != 0) return 1;
        if (json_output) {
            printf("{\n  \"success\": true\n}\n");
        } else {
            printf("Updated container hosts files.\n");
        }
        return 0;
    }

    fprintf(stderr, "Unknown dns command: %s\n", argv[2]);
    return 1;
}
