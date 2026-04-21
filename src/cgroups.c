/*
 * cgroups.c — Linux Control Groups (cgroups) Resource Enforcement
 *
 * PURPOSE:
 *   Apply per-container CPU and memory resource limits using the Linux
 *   cgroups v1/v2 hierarchy. Each container gets its own cgroup subtree
 *   identified by its PID, preventing runaway processes from consuming
 *   host resources.
 *
 * CGROUP v1 HIERARCHY (legacy, two separate mount points):
 *   /sys/fs/cgroup/cpu/mycontainer/<pid>/
 *     cpu.shares            ← relative CPU weight (1024 = default)
 *     cpu.cfs_quota_us      ← hard quota per period; -1 = unlimited
 *     cpu.cfs_period_us     ← CFS period length (default 100000 µs = 100ms)
 *     tasks                 ← write container PID here to enlist
 *   /sys/fs/cgroup/memory/mycontainer/<pid>/
 *     memory.limit_in_bytes ← hard memory cap (e.g. 536870912 = 512MB)
 *     tasks                 ← write PID here
 *
 * CGROUP v2 (unified hierarchy, kernel ≥ 5.2 recommended):
 *   /sys/fs/cgroup/mycontainer/<pid>/
 *     cgroup.procs  ← write PID (replaces tasks)
 *     cpu.max       ← "quota period", e.g. "50000 100000" = 50% CPU
 *     memory.max    ← hard limit in bytes, e.g. "536870912"
 *     memory.swap.max ← swap cap; "0" to disable swap
 *
 * CURRENT STATE:
 *   Stub implementation. Replace setup_cgroups() body with real
 *   cgroup file writes as described in the roadmap below.
 *
 * PRIVILEGE REQUIREMENTS:
 *   Must be run as root or with CAP_SYS_ADMIN to write cgroup files.
 */

#include "../include/cgroups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/*
 * setup_cgroups() — Enlist a process in its resource cgroup
 *
 * @pid: PID of the container init process to limit
 *
 * Returns 0 on success, -1 on failure.
 *
 * Implementation steps:
 *   1. Detect v1 vs v2 (check /sys/fs/cgroup/cgroup.controllers).
 *   2. Create per-container cgroup directory under /sys/fs/cgroup/.
 *   3. Write pid to tasks (v1) or cgroup.procs (v2).
 *   4. Set CPU limit: cpu.max "50000 100000" (v2) or
 *      cpu.cfs_quota_us/cfs_period_us (v1) → 50% of one core.
 *   5. Set memory limit: memory.max "536870912" (v2) or
 *      memory.limit_in_bytes (v1) → 512 MB.
 *   6. Optionally disable swap: memory.swap.max "0" (v2) or
 *      memory.memsw.limit_in_bytes = same as step 5 (v1).
 */
int setup_cgroups(pid_t pid) {
    /* --- STUB: log intended limits, return success --- */
    printf("[cgroups] Enrolling PID %d into resource control group\n", pid);
    printf("[cgroups] CPU limit:    50%% (50ms per 100ms CFS period)\n");
    printf("[cgroups] Memory limit: 512 MB hard cap\n");
    printf("[cgroups] Swap limit:   disabled\n");

    /*
     * TODO — replace stub with real implementation:
     *
     * char path[256], pid_str[16];
     * snprintf(pid_str, sizeof(pid_str), "%d", (int)pid);
     *
     * // Detect cgroup version
     * int v2 = (access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0);
     *
     * if (v2) {
     *     snprintf(path, sizeof(path),
     *              "/sys/fs/cgroup/mycontainer/%d", pid);
     *     mkdir(path, 0755);
     *     cg_write(path, "cgroup.procs",  pid_str);
     *     cg_write(path, "cpu.max",       "50000 100000");
     *     cg_write(path, "memory.max",    "536870912");
     *     cg_write(path, "memory.swap.max", "0");
     * } else {
     *     // v1 CPU subsystem
     *     snprintf(path, sizeof(path),
     *              "/sys/fs/cgroup/cpu/mycontainer/%d", pid);
     *     mkdir(path, 0755);
     *     cg_write(path, "tasks",              pid_str);
     *     cg_write(path, "cpu.cfs_quota_us",   "50000");
     *     cg_write(path, "cpu.cfs_period_us",  "100000");
     *     // v1 memory subsystem
     *     snprintf(path, sizeof(path),
     *              "/sys/fs/cgroup/memory/mycontainer/%d", pid);
     *     mkdir(path, 0755);
     *     cg_write(path, "tasks", pid_str);
     *     cg_write(path, "memory.limit_in_bytes", "536870912");
     * }
     */

    return 0; /* Stub always succeeds */
}
