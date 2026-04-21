#define _GNU_SOURCE
#include "../include/security.h"

static int security_profile_path(const char *profile_name, char *path, size_t path_len) {
    if (!profile_name || !path) return -1;
    if (format_buffer(path, path_len, "%s/%s.json", SECURITY_DIR, profile_name) != 0) {
        fprintf(stderr, "Error: security profile path too long for %s\n", profile_name);
        return -1;
    }
    return 0;
}

int seccomp_load_profile(const char *profile_name, SecurityProfile *profile) {
    char path[MAX_PATH_LEN];
    char *data;
    JsonObject obj;
    const char *blocked;
    int count;

    if (!profile_name || !profile) return -1;
    if (strcmp(profile_name, "none") == 0) {
        memset(profile, 0, sizeof(SecurityProfile));
        strncpy(profile->profile, "none", sizeof(profile->profile) - 1);
        return 0;
    }

    if (security_profile_path(profile_name, path, sizeof(path)) != 0) return -1;
    if (!file_exists(path)) {
        fprintf(stderr, "Error: security profile not found: %s\n", profile_name);
        return -1;
    }

    data = read_file(path);
    if (!data) return -1;
    if (json_parse(data, &obj) != 0) {
        free(data);
        fprintf(stderr, "Error: failed to parse security profile %s\n", path);
        return -1;
    }
    free(data);

    memset(profile, 0, sizeof(SecurityProfile));
    strncpy(profile->profile,
            json_get(&obj, "profile") ? json_get(&obj, "profile") : profile_name,
            sizeof(profile->profile) - 1);

    blocked = json_get(&obj, "blocked_syscalls");
    if (blocked && blocked[0] == '[') {
        char parsed[64][MAX_LINE];
        count = json_parse_string_array(blocked, parsed, 64);
        if (count > 0) {
            profile->blocked_count = count;
            for (int i = 0; i < count; i++) {
                strncpy(profile->blocked[i], parsed[i], sizeof(profile->blocked[i]) - 1);
            }
        }
    }

    return 0;
}

int seccomp_apply(const char *profile_name) {
    SecurityProfile profile;

    if (!profile_name || strcmp(profile_name, "none") == 0) return 0;
    if (seccomp_load_profile(profile_name, &profile) != 0) return -1;

    /*
     * The current simulator run path is still a host-side stub rather than a
     * dedicated post-clone/pre-exec child process. We validate the requested
     * profile here so the CLI/runtime state is consistent, but we intentionally
     * do not install a seccomp filter on the current control process.
     */
    return 0;
}

int security_list_profiles(int json_output) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_exists(SECURITY_DIR)) {
        fprintf(stderr, "Error: %s directory not found\n", SECURITY_DIR);
        return -1;
    }

    dir = opendir(SECURITY_DIR);
    if (!dir) {
        fprintf(stderr, "Error: cannot open %s: %s\n", SECURITY_DIR, strerror(errno));
        return -1;
    }

    if (json_output) {
        JsonArray *arr = json_array_new();
        if (!arr) {
            closedir(dir);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            SecurityProfile profile;
            char name[64];
            JsonObject obj;

            if (entry->d_name[0] == '.') continue;
            strncpy(name, entry->d_name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            char *dot = strrchr(name, '.');
            if (dot) *dot = '\0';

            if (seccomp_load_profile(name, &profile) != 0) continue;
            obj.count = 0;
            json_set(&obj, "name", profile.profile);
            char blocked_count[16];
            snprintf(blocked_count, sizeof(blocked_count), "%d", profile.blocked_count);
            json_set_raw(&obj, "blocked_syscalls", blocked_count);
            json_array_append(arr, &obj);
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
        printf("%-16s %s\n", "PROFILE", "BLOCKED SYSCALLS");
        printf("%-16s %s\n", "-------", "----------------");
        while ((entry = readdir(dir)) != NULL) {
            SecurityProfile profile;
            char name[64];
            if (entry->d_name[0] == '.') continue;
            strncpy(name, entry->d_name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            char *dot = strrchr(name, '.');
            if (dot) *dot = '\0';
            if (seccomp_load_profile(name, &profile) != 0) continue;
            printf("%-16s %d\n", profile.profile, profile.blocked_count);
        }
    }

    closedir(dir);
    return 0;
}

int security_cmd(int argc, char *argv[]) {
    int json_output = has_json_flag(argc, argv);

    if (argc < 3) {
        fprintf(stderr, "Usage: mycontainer security ls [--json]\n"
                        "       mycontainer security inspect <profile> [--json]\n");
        return 1;
    }

    if (strcmp(argv[2], "ls") == 0 || strcmp(argv[2], "list") == 0) {
        return security_list_profiles(json_output);
    }

    if (strcmp(argv[2], "inspect") == 0 && argc >= 4) {
        SecurityProfile profile;
        if (seccomp_load_profile(argv[3], &profile) != 0) return 1;
        if (json_output) {
            char *blocked_values[64];
            for (int i = 0; i < profile.blocked_count; i++) {
                blocked_values[i] = profile.blocked[i];
            }
            char *blocked_json = json_string_array_from_list(blocked_values, profile.blocked_count);
            JsonObject obj;
            char *json;
            if (!blocked_json) return 1;
            obj.count = 0;
            json_set(&obj, "name", profile.profile);
            json_set_raw(&obj, "blocked_syscalls", blocked_json);
            json = json_stringify_pretty(&obj);
            free(blocked_json);
            if (!json) return 1;
            printf("%s\n", json);
            free(json);
        } else {
            printf("Profile: %s\n", profile.profile);
            printf("Blocked syscalls:\n");
            for (int i = 0; i < profile.blocked_count; i++) {
                printf("  %s\n", profile.blocked[i]);
            }
        }
        return 0;
    }

    fprintf(stderr, "Unknown security command: %s\n", argv[2]);
    return 1;
}
