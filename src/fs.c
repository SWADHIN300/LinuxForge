/*
 * fs.c — Container Root Filesystem Setup
 *
 * Mounts an OverlayFS-backed root filesystem plus the minimal virtual
 * filesystems that container payloads expect to find at runtime.
 */

#define _GNU_SOURCE
#include "../include/fs.h"
#include "../include/utils.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

static int fs_prepare_dir(const char *path, mode_t mode) {
    if (!path) return -1;
    if (mkdir_p(path) != 0) return -1;
    if (chmod(path, mode) != 0 && errno != EPERM) return -1;
    return 0;
}

static int fs_mount_point(char *out, size_t out_len,
                          const char *rootfs_path, const char *suffix,
                          mode_t mode) {
    if (format_buffer(out, out_len, "%s/%s", rootfs_path, suffix) != 0) {
        return -1;
    }
    return fs_prepare_dir(out, mode);
}

static int fs_create_char_device(const char *path, mode_t mode,
                                 unsigned int major_num, unsigned int minor_num) {
    struct stat st;

    if (stat(path, &st) == 0) {
        return 0;
    }

    if (mknod(path, S_IFCHR | mode, makedev(major_num, minor_num)) == 0) {
        return 0;
    }

    if (errno == EEXIST) {
        return 0;
    }

    return -1;
}

int setup_rootfs(const char *rootfs_path,
                 const char *lowerdir,
                 const char *upperdir,
                 const char *workdir) {
    char overlay_opts[MAX_PATH_LEN * 3];
    char proc_path[MAX_PATH_LEN];
    char sys_path[MAX_PATH_LEN];
    char dev_path[MAX_PATH_LEN];
    char pts_path[MAX_PATH_LEN];
    char shm_path[MAX_PATH_LEN];
    char devnull[MAX_PATH_LEN];
    char devzero[MAX_PATH_LEN];
    char devurandom[MAX_PATH_LEN];

    if (!rootfs_path || !lowerdir || !upperdir || !workdir) {
        errno = EINVAL;
        return -1;
    }

    if (mkdir_p(rootfs_path) != 0) return -1;

    if (snprintf(overlay_opts, sizeof(overlay_opts),
                 "lowerdir=%s,upperdir=%s,workdir=%s",
                 lowerdir, upperdir, workdir) >= (int) sizeof(overlay_opts)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (mount("overlay", rootfs_path, "overlay", MS_NODEV, overlay_opts) != 0) {
        return -1;
    }

    if (fs_mount_point(proc_path, sizeof(proc_path), rootfs_path, "proc", 0555) != 0 ||
        fs_mount_point(sys_path, sizeof(sys_path), rootfs_path, "sys", 0555) != 0 ||
        fs_mount_point(dev_path, sizeof(dev_path), rootfs_path, "dev", 0755) != 0 ||
        fs_mount_point(pts_path, sizeof(pts_path), rootfs_path, "dev/pts", 0755) != 0 ||
        fs_mount_point(shm_path, sizeof(shm_path), rootfs_path, "dev/shm", 01777) != 0) {
        return -1;
    }

    if (mount("proc", proc_path, "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, "") != 0) {
        return -1;
    }

    if (mount("sysfs", sys_path, "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_RDONLY, "") != 0) {
        return -1;
    }

    if (mount("tmpfs", dev_path, "tmpfs",
              MS_NOSUID | MS_STRICTATIME, "mode=755,size=64k") != 0) {
        return -1;
    }

    if (mount("devpts", pts_path, "devpts",
              MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666,mode=0620") != 0) {
        return -1;
    }

    if (mount("tmpfs", shm_path, "tmpfs",
              MS_NOSUID | MS_NODEV, "mode=1777,size=64k") != 0) {
        return -1;
    }

    if (format_buffer(devnull, sizeof(devnull), "%s/null", dev_path) != 0 ||
        format_buffer(devzero, sizeof(devzero), "%s/zero", dev_path) != 0 ||
        format_buffer(devurandom, sizeof(devurandom), "%s/urandom", dev_path) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (fs_create_char_device(devnull, 0666, 1, 3) != 0 ||
        fs_create_char_device(devzero, 0666, 1, 5) != 0 ||
        fs_create_char_device(devurandom, 0666, 1, 9) != 0) {
        return -1;
    }

    return 0;
}
