/*
 * security.c — Seccomp Security Profile Management
 *
 * Loads JSON blocklists from the security directory and installs a simple
 * seccomp filter in the container payload right before exec.
 */

#define _GNU_SOURCE
#include "../include/security.h"

#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

typedef struct {
    const char *name;
    int number;
} SecuritySyscall;

static int security_syscall_number(const char *name) {
    static const SecuritySyscall table[] = {
#ifdef __NR_reboot
        { "reboot", __NR_reboot },
#endif
#ifdef __NR_kexec_load
        { "kexec_load", __NR_kexec_load },
#endif
#ifdef __NR_mount
        { "mount", __NR_mount },
#endif
#ifdef __NR_umount2
        { "umount2", __NR_umount2 },
#endif
#ifdef __NR_ptrace
        { "ptrace", __NR_ptrace },
#endif
#ifdef __NR_process_vm_readv
        { "process_vm_readv", __NR_process_vm_readv },
#endif
#ifdef __NR_process_vm_writev
        { "process_vm_writev", __NR_process_vm_writev },
#endif
#ifdef __NR_swapon
        { "swapon", __NR_swapon },
#endif
#ifdef __NR_swapoff
        { "swapoff", __NR_swapoff },
#endif
#ifdef __NR_init_module
        { "init_module", __NR_init_module },
#endif
#ifdef __NR_finit_module
        { "finit_module", __NR_finit_module },
#endif
#ifdef __NR_delete_module
        { "delete_module", __NR_delete_module },
#endif
#ifdef __NR_syslog
        { "syslog", __NR_syslog },
#endif
#ifdef __NR_chroot
        { "chroot", __NR_chroot },
#endif
#ifdef __NR_acct
        { "acct", __NR_acct },
#endif
#ifdef __NR_sethostname
        { "sethostname", __NR_sethostname },
#endif
#ifdef __NR_setdomainname
        { "setdomainname", __NR_setdomainname },
#endif
#ifdef __NR_iopl
        { "iopl", __NR_iopl },
#endif
#ifdef __NR_ioperm
        { "ioperm", __NR_ioperm },
#endif
#ifdef __NR_create_module
        { "create_module", __NR_create_module },
#endif
#ifdef __NR_lookup_dcookie
        { "lookup_dcookie", __NR_lookup_dcookie },
#endif
#ifdef __NR_open_by_handle_at
        { "open_by_handle_at", __NR_open_by_handle_at },
#endif
#ifdef __NR_name_to_handle_at
        { "name_to_handle_at", __NR_name_to_handle_at },
#endif
#ifdef __NR_perf_event_open
        { "perf_event_open", __NR_perf_event_open },
#endif
        { NULL, -1 }
    };

    for (int i = 0; table[i].name != NULL; i++) {
        if (strcmp(table[i].name, name) == 0) {
            return table[i].number;
        }
    }

    return -1;
}
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
    struct sock_filter *filter = NULL;
    struct sock_fprog prog;
    int count = 0;
    int idx = 0;

    if (!profile_name || strcmp(profile_name, "none") == 0) return 0;
    if (seccomp_load_profile(profile_name, &profile) != 0) return -1;

    for (int i = 0; i < profile.blocked_count; i++) {
        if (security_syscall_number(profile.blocked[i]) >= 0) {
            count++;
        }
    }

    filter = calloc((size_t) (count * 2 + 2), sizeof(*filter));
    if (!filter) return -1;

    filter[idx++] = (struct sock_filter) BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                                                  (unsigned int) offsetof(struct seccomp_data, nr));

    for (int i = 0; i < profile.blocked_count; i++) {
        int nr = security_syscall_number(profile.blocked[i]);
        if (nr < 0) continue;
        filter[idx++] = (struct sock_filter) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                                                      (unsigned int) nr, 0, 1);
        filter[idx++] = (struct sock_filter) BPF_STMT(BPF_RET | BPF_K,
                                                      SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA));
    }

    filter[idx++] = (struct sock_filter) BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    prog.len = (unsigned short) idx;
    prog.filter = filter;

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        free(filter);
        return -1;
    }

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
        free(filter);
        return -1;
    }

    free(filter);
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
