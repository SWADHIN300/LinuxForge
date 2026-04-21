#ifndef EXPORT_H
#define EXPORT_H

#include "commit.h"

int container_export(const char *container_id, const char *output_path, int json_output);
int container_import(const char *tar_path, const char *name, const char *tag, int json_output);
int export_cmd(int argc, char *argv[]);
int import_cmd(int argc, char *argv[]);

#endif
