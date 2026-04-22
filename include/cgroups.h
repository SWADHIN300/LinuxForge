/*
 * cgroups.h — Cgroup setup (existing feature stub)
 *
 * Provides CPU and memory cgroup configuration.
 * This is a stub — replace with your actual implementation.
 */

#ifndef CGROUPS_H
#define CGROUPS_H

#include <sys/types.h>

/*
 * Set up cgroups (CPU + memory) for a container process.
 * The cgroup directory is keyed by the container ID so stats can
 * locate the same subtree later.
 * Returns 0 on success, -1 on error.
 */
int setup_cgroups(const char *container_id, pid_t pid, const char *cpuset);

#endif /* CGROUPS_H */
