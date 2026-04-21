#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include "commit.h"

#define CHECKPOINTS_DIR "checkpoints"

int checkpoint_create(const char *container_id, const char *checkpoint_dir, int json_output);
int checkpoint_restore(const char *checkpoint_dir, const char *new_name, int json_output);
int checkpoint_list(int json_output);
int checkpoint_cmd(int argc, char *argv[]);
int restore_cmd(int argc, char *argv[]);

#endif
