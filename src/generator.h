#ifndef GENERATOR_H
#define GENERATOR_H

#include "structs.h"
#include "builtins.h"
#include "utilities.h"

static char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);
static char *filename_generator(const char *text, int state);
static char *directory_generator(const char *text, int state);
static char *path_executable_generator(const char *text, int state);

#endif