/*
 * fs.h — Filesystem setup (existing feature stub)
 *
 * Provides chroot and mount namespace configuration.
 * This is a stub — replace with your actual implementation.
 */

#ifndef FS_H
#define FS_H

/*
 * Set up the root filesystem for a container.
 * Mounts the container overlay plus /proc, /sys, and /dev inside the
 * provided merged rootfs path. This must run in the container's mount
 * namespace before the payload execs.
 * Returns 0 on success, -1 on error.
 */
int setup_rootfs(const char *rootfs_path,
                 const char *lowerdir,
                 const char *upperdir,
                 const char *workdir);

#endif /* FS_H */
