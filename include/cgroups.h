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
 * Returns 0 on success, -1 on error.
 */
int setup_cgroups(pid_t pid);

#endif /* CGROUPS_H */
