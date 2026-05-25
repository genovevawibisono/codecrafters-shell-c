#ifndef STRUCTS_H
#define STRUCTS_H

/* INCLUDE LIBRARIES */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

/* DEFINE CONSTANTS */
#define MAX_COMMAND_LENGTH 1024
#define ARGV_MAX_CAPACITY 1024

/* DEFINE STRUCTS AND TYPEDEFS */
struct command_context {
	bool redirect;
	char *out_file;
    int out_mode;
    bool redirect_err;
    char *error_file;
    int err_mode;
	char *command_name;
    int argc;
    char **argv;
    int num_commands;
    char ***all_commands;
    int *all_argc;
    char **all_command_names;
    int background_job;
};

typedef void (*command_function)(struct command_context *);

struct command {
	const char *name;
	command_function func;
};

#endif