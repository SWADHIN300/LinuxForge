/*
 * fs.c — Container Root Filesystem Setup (OverlayFS + Namespace Isolation)
 *
 * PURPOSE:
 *   Configure the container's root filesystem environment after clone(2)
 *   and before execve(2). This includes mounting OverlayFS, populating
 *   virtual filesystems (/proc, /sys, /dev), performing a chroot(2) into
 *   the merged view, and pivoting the process into the container namespace.
 *
 * OVERLAYFS MOUNT (requires Linux ≥ 3.18, kernel CONFIG_OVERLAY_FS=y):
 *   OverlayFS merges a read-only "lower" directory with a writable "upper"
 *   directory into a unified "merged" view. All writes go to "upper" while
 *   the lower layer remains untouched — the basis for copy-on-write images.
 *
 *   mount("overlay", merged, "overlay", MS_NODEV,
 *         "lowerdir=<lower>,upperdir=<upper>,workdir=<work>");
 *
 *   Paths used by mycontainer:
 *     lower  → containers/<id>/overlay/lower/  (base image files)
 *     upper  → containers/<id>/overlay/upper/  (writable diff layer)
 *     work   → containers/<id>/overlay/work/   (kernel bookkeeping)
 *     merged → containers/<id>/rootfs/         (what the container sees)
 *
 * VIRTUAL FS MOUNTS (inside the merged rootfs):
 *   /proc   → proc filesystem  (needed by ps, top, /proc/self/…)
 *   /sys    → sysfs            (device/network information)
 *   /dev    → tmpfs            (device nodes; mknod for null, zero, urandom…)
 *   /dev/pts→ devpts           (pseudo-terminals for interactive shells)
 *
 * CHROOT / PIVOT_ROOT:
 *   Option A — simple chroot(2):
 *     chroot(merged);   ← change apparent root for the current process
 *     chdir("/");       ← ensure CWD is inside the new root
 *
 *   Option B — pivot_root(2) (more secure, requires mount namespace):
 *     mount(merged, merged, NULL, MS_BIND, NULL);   ← bind-mount to self
 *     mkdir(merged + "/.old_root");
 *     pivot_root(merged, merged + "/.old_root");
 *     umount2("/.old_root", MNT_DETACH);
 *     rmdir("/.old_root");
 *
 * CURRENT STATE:
 *   Stub implementation that logs the intended steps and returns 0.
 *   Replace with the real mount/chroot/pivot_root calls listed above.
 *   This function must be called in the child process AFTER clone(2)
 *   and BEFORE execve(2) — never call it from the host control process.
 *
 * PRIVILEGE REQUIREMENTS:
 *   Requires CAP_SYS_ADMIN (or running as root) for mount(2).
 *   In rootless mode, use user namespaces: clone(CLONE_NEWUSER|CLONE_NEWNS)
 *   then newuidmap/newgidmap to set up UID/GID mapping.
 */

#include "../include/fs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * setup_rootfs() — Mount overlay + virtual filesystems, then chroot
 *
 * @rootfs_path: path to the container's merged rootfs directory
 *
 * Returns 0 on success, -1 on mount/chroot failure.
 *
 * IMPLEMENTATION STEPS:
 *   1. Mount OverlayFS at rootfs_path (see header comment above).
 *   2. mount("proc",  rootfs_path+"/proc",    "proc",  0, "")
 *   3. mount("sysfs", rootfs_path+"/sys",     "sysfs", 0, "")
 *   4. mount("tmpfs", rootfs_path+"/dev",     "tmpfs", MS_NOSUID|MS_STRICTATIME, "mode=755")
 *   5. mknod(rootfs_path+"/dev/null", S_IFCHR|0666, makedev(1,3))
 *   6. mknod(rootfs_path+"/dev/zero", S_IFCHR|0666, makedev(1,5))
 *   7. mknod(rootfs_path+"/dev/urandom", S_IFCHR|0666, makedev(1,9))
 *   8. chroot(rootfs_path) + chdir("/")
 */
int setup_rootfs(const char *rootfs_path) {
    /* --- STUB: log intended operations --- */
    printf("[fs] Setting up rootfs at %s\n", rootfs_path);
    printf("[fs] Would mount OverlayFS (lower/upper/work → rootfs)\n");
    printf("[fs] Would mount /proc (procfs)\n");
    printf("[fs] Would mount /sys  (sysfs)\n");
    printf("[fs] Would mount /dev  (tmpfs + device nodes)\n");
    printf("[fs] Would chroot(%s) + chdir(\"/\")\n", rootfs_path);

    /*
     * TODO — Replace stub with real implementation:
     *
     * char opts[512];
     * // 1. Mount overlay
     * snprintf(opts, sizeof(opts),
     *          "lowerdir=%s/overlay/lower,upperdir=%s/overlay/upper,workdir=%s/overlay/work",
     *          base, base, base);
     * if (mount("overlay", rootfs_path, "overlay", MS_NODEV, opts) != 0) {
     *     perror("overlay mount"); return -1;
     * }
     * // 2. Mount /proc
     * char proc_path[256];
     * snprintf(proc_path, sizeof(proc_path), "%s/proc", rootfs_path);
     * mkdir(proc_path, 0555);
     * mount("proc", proc_path, "proc", 0, "");
     * // 3. Mount /sys
     * ...same pattern for sysfs...
     * // 4. Mount /dev
     * ...tmpfs...
     * // 5. chroot
     * chroot(rootfs_path);
     * chdir("/");
     */

    return 0; /* Stub always succeeds */
}

