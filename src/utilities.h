#ifndef UTILITIES_H
#define UTILITIES_H

#include <regex.h>
#include <stdio.h>

#include "structs.h"
#include "variables.h"

void parse_command_line(char *line, struct command_context *ctx);
bool is_executable(const char *path);
char *resolve_executable(const char *name);
bool starts_with_regex(const char *str, const char *pattern);

#endif