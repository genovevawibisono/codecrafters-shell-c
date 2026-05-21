#ifndef EXECUTABLES_H
#define EXECUTABLES_H

#include "structs.h"
#include "builtins.h"

void shell_exec_pipeline(struct command_context *ctx);
char *find_executable_in_path(const char *command_name);
void execute_builtin_in_fork(const char *command_name, char **argv, int argc, 
                                   int stdin_fd, int stdout_fd);

#endif