#ifndef LOGS_H
#define LOGS_H

#include "utils.h"

#define LOGS_DIR "logs"

typedef struct {
    char container_id[16];
    char log_path[MAX_PATH_LEN];
    int  fd;
    int  max_lines;
} ContainerLog;

int logs_init(const char *container_id);
int logs_append_line(const char *container_id, const char *message);
int logs_read(const char *container_id, int tail_lines, int json_output);
int logs_follow(const char *container_id);
int logs_clear(const char *container_id, int json_output);
int logs_rotate(const char *container_id);
int logs_cmd(int argc, char *argv[]);

#endif
