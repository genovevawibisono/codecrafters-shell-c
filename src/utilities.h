#ifndef UTILITIES_H
#define UTILITIES_H

#include "structs.h"

void parse_command_line(char *line, struct command_context *ctx);
bool is_executable(const char *path);

#endif