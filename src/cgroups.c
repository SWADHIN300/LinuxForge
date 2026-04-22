/*
 * cgroups.c — Linux Control Groups resource enforcement
 */

#define _GNU_SOURCE
#include "../include/cgroups.h"
#include "../include/utils.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int cg_write_text(const char *path, const char *value) {
    int fd;
    ssize_t len;

    if (!path || !value) return -1;

    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    len = (ssize_t) strlen(value);
    if (write(fd, value, (size_t) len) != len) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int cg_path(char *out, size_t out_len, const char *dir, const char *file) {
    return format_buffer(out, out_len, "%s/%s", dir, file);
}

static int cg_read_first_existing(char *out, size_t out_len,
                                  const char *dir, const char *file_a,
                                  const char *file_b, const char *fallback) {
    char path[MAX_PATH_LEN];
    char *data = NULL;

    if (file_a && cg_path(path, sizeof(path), dir, file_a) == 0 && file_exists(path)) {
        data = read_file(path);
    }
    if (!data && file_b && cg_path(path, sizeof(path), dir, file_b) == 0 && file_exists(path)) {
        data = read_file(path);
    }

    if (data) {
        strncpy(out, data, out_len - 1);
        out[out_len - 1] = '\0';
        free(data);
        str_trim(out);
        return 0;
    }

    if (fallback) {
        strncpy(out, fallback, out_len - 1);
        out[out_len - 1] = '\0';
        return 0;
    }

    return -1;
}

static int setup_cgroups_v2(const char *container_id, pid_t pid, const char *cpuset) {
    char base_dir[MAX_PATH_LEN];
    char cg_dir[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    char pid_str[32];
    char cpus[256];
    char mems[256];

    if (format_buffer(base_dir, sizeof(base_dir), "/sys/fs/cgroup/mycontainer") != 0 ||
        format_buffer(cg_dir, sizeof(cg_dir), "%s/%s", base_dir, container_id) != 0) {
        return -1;
    }

    mkdir(base_dir, 0755);

    if (cg_path(path, sizeof(path), "/sys/fs/cgroup", "cgroup.subtree_control") == 0) {
        cg_write_text(path, "+cpu +memory +cpuset");
    }
    if (cg_path(path, sizeof(path), base_dir, "cgroup.subtree_control") == 0) {
        cg_write_text(path, "+cpu +memory +cpuset");
    }

    if (mkdir(cg_dir, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    if (cg_read_first_existing(cpus, sizeof(cpus), base_dir,
                               "cpuset.cpus.effective", "cpuset.cpus", NULL) == 0 &&
        cg_path(path, sizeof(path), cg_dir, "cpuset.cpus") == 0) {
        const char *value = (cpuset && strlen(cpuset) > 0) ? cpuset : cpus;
        cg_write_text(path, value);
    }

    if (cg_read_first_existing(mems, sizeof(mems), base_dir,
                               "cpuset.mems.effective", "cpuset.mems", "0") == 0 &&
        cg_path(path, sizeof(path), cg_dir, "cpuset.mems") == 0) {
        cg_write_text(path, mems);
    }

    if (cg_path(path, sizeof(path), cg_dir, "cpu.max") == 0) {
        cg_write_text(path, "50000 100000");
    }
    if (cg_path(path, sizeof(path), cg_dir, "memory.max") == 0) {
        cg_write_text(path, "536870912");
    }
    if (cg_path(path, sizeof(path), cg_dir, "memory.swap.max") == 0) {
        cg_write_text(path, "0");
    }

    snprintf(pid_str, sizeof(pid_str), "%d", (int) pid);
    if (cg_path(path, sizeof(path), cg_dir, "cgroup.procs") != 0) return -1;
    return cg_write_text(path, pid_str);
}

static int setup_cgroups_v1(const char *container_id, pid_t pid, const char *cpuset) {
    char cpu_dir[MAX_PATH_LEN];
    char memory_dir[MAX_PATH_LEN];
    char cpuset_dir[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    char pid_str[32];
    char cpus[256];
    char mems[256];

    if (format_buffer(cpu_dir, sizeof(cpu_dir), "/sys/fs/cgroup/cpu/mycontainer/%s", container_id) != 0 ||
        format_buffer(memory_dir, sizeof(memory_dir), "/sys/fs/cgroup/memory/mycontainer/%s", container_id) != 0 ||
        format_buffer(cpuset_dir, sizeof(cpuset_dir), "/sys/fs/cgroup/cpuset/mycontainer/%s", container_id) != 0) {
        return -1;
    }

    mkdir("/sys/fs/cgroup/cpu/mycontainer", 0755);
    mkdir("/sys/fs/cgroup/memory/mycontainer", 0755);
    mkdir("/sys/fs/cgroup/cpuset/mycontainer", 0755);

    if (mkdir(cpu_dir, 0755) != 0 && errno != EEXIST) return -1;
    if (mkdir(memory_dir, 0755) != 0 && errno != EEXIST) return -1;

    snprintf(pid_str, sizeof(pid_str), "%d", (int) pid);

    if (cg_path(path, sizeof(path), cpu_dir, "cpu.cfs_quota_us") == 0) {
        cg_write_text(path, "50000");
    }
    if (cg_path(path, sizeof(path), cpu_dir, "cpu.cfs_period_us") == 0) {
        cg_write_text(path, "100000");
    }
    if (cg_path(path, sizeof(path), cpu_dir, "tasks") == 0 && cg_write_text(path, pid_str) != 0) {
        return -1;
    }

    if (cg_path(path, sizeof(path), memory_dir, "memory.limit_in_bytes") == 0) {
        cg_write_text(path, "536870912");
    }
    if (cg_path(path, sizeof(path), memory_dir, "memory.memsw.limit_in_bytes") == 0) {
        cg_write_text(path, "536870912");
    }
    if (cg_path(path, sizeof(path), memory_dir, "tasks") == 0 && cg_write_text(path, pid_str) != 0) {
        return -1;
    }

    if (dir_exists("/sys/fs/cgroup/cpuset")) {
        mkdir(cpuset_dir, 0755);
        if (cg_read_first_existing(mems, sizeof(mems), "/sys/fs/cgroup/cpuset",
                                   "cpuset.mems", NULL, "0") == 0) {
            if (cg_path(path, sizeof(path), cpuset_dir, "cpuset.mems") == 0) {
                cg_write_text(path, mems);
            }
        }
        if (cg_path(path, sizeof(path), cpuset_dir, "cpuset.cpus") == 0) {
            if (cpuset && strlen(cpuset) > 0) {
                cg_write_text(path, cpuset);
            } else if (cg_read_first_existing(cpus, sizeof(cpus), "/sys/fs/cgroup/cpuset",
                                              "cpuset.cpus", NULL, "0") == 0) {
                cg_write_text(path, cpus);
            }
        }
        if (cg_path(path, sizeof(path), cpuset_dir, "tasks") == 0) {
            cg_write_text(path, pid_str);
        }
    }

    return 0;
}

int setup_cgroups(const char *container_id, pid_t pid, const char *cpuset) {
    if (!container_id || pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (file_exists("/sys/fs/cgroup/cgroup.controllers")) {
        return setup_cgroups_v2(container_id, pid, cpuset);
    }

    return setup_cgroups_v1(container_id, pid, cpuset);
}
