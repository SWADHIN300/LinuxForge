#ifndef VOLUME_H
#define VOLUME_H

#include "commit.h"

typedef struct {
    char host_path[MAX_PATH_LEN];
    char container_path[MAX_PATH_LEN];
    char mode[8];
} Volume;

int volume_parse(const char *volume_str, Volume *vol);
int volume_mount(Volume *volumes, int count, const char *rootfs_path);
int volume_list(const char *container_id, int json_output);
int volume_unmount(const char *container_id);
int volume_cmd(int argc, char *argv[]);

#endif
