#ifndef BUILTINS_H
#define BUILTINS_H

#include "structs.h"
#include "history.h"
#include "utilities.h"
#include "complete.h"
#include "variables.h"

/* INCLUDE LIBRARIES */
#include <ctype.h>
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

static void shell_exit(struct command_context *ctx);
static void shell_echo(struct command_context *ctx);
static void shell_type(struct command_context *ctx);
void shell_exec(struct command_context *ctx);
static void shell_pwd(struct command_context *ctx);
static void shell_cd(struct command_context *ctx);
bool is_builtin(const char *command_name);
command_function get_builtin_function(const char *command_name);

static void shell_history(struct command_context *ctx);

/* DEFINE CONSTANTS */
#define DEFAULT_EXIT_STATUS 0
#define EXIT_LENGTH 4
#define ECHO_LENGTH 4
#define MAX_PATH_LENGTH 1024

/* OTHER HELPERS TO MAKE LIFE EASIER */
extern struct command commands[];
#define NUM_COMMANDS 9

extern char *command_names[];

static void shell_jobs(struct command_context *ctx);
static void shell_complete(struct command_context *ctx);
static void shell_declare(struct command_context *ctx);

#endif