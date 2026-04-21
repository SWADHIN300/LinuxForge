#ifndef STACK_H
#define STACK_H

#include "commit.h"

int stack_up(const char *stack_file, int json_output);
int stack_down(const char *stack_file_or_name, int json_output);
int stack_status(const char *stack_name, int json_output);
int stack_list(int json_output);
int stack_cmd(int argc, char *argv[]);

#endif
