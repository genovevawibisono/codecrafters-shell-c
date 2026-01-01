#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_COMMAND_LENGTH 1024
#define DEFAULT_EXIT_STATUS 0
#define EXIT_LENGTH 4
#define ECHO_LENGTH 4

struct command_context {
	bool redirect;
	char *out_file;
	char *args;
};

typedef void (*command_function)(struct command_context *);

struct command {
	const char *name;
	command_function func;
};

static void trim_newline(char *s);
static void split_command(char *line, char **cmd, char **args);
static void shell_exit(struct command_context *ctx);
static void shell_echo(struct command_context *ctx);
static void shell_type(struct command_context *ctx);

struct command commands[] = {
    { "exit", shell_exit },
    { "echo", shell_echo },
	{ "type", shell_type },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

int main(void) {
    char line[MAX_COMMAND_LENGTH];

    while (1) {
        setbuf(stdout, NULL);
        printf("$ ");

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        trim_newline(line);

        char *cmd;
        char *args;
        split_command(line, &cmd, &args);

        struct command_context ctx = {
            .redirect = false,
            .out_file = NULL,
            .args = args
        };

        bool found = false;
        for (size_t i = 0; i < NUM_COMMANDS; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                commands[i].func(&ctx);
                found = true;
                break;
            }
        }

        if (!found) {
            printf("%s: command not found\n", cmd);
        }
    }
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}

static void split_command(char *line, char **cmd, char **args) {
    *cmd = line;

    char *space = strchr(line, ' ');
    if (space) {
        *space = '\0';
        *args = space + 1;
    } else {
        *args = "";
    }
}

static void shell_exit(struct command_context *ctx) {
	(void) ctx;
    exit(DEFAULT_EXIT_STATUS);
}

static void shell_echo(struct command_context *ctx) {
    fprintf(stdout, "%s\n", ctx->args);
}

static void shell_type(struct command_context *ctx) {
	bool found = false;
	for (size_t i = 0; i < NUM_COMMANDS; i++) {
		if (strcmp(commands[i].name, ctx->args) == 0) {
			fprintf(stdout, "%s is a shell builtin\n", ctx->args);
			found = true;
			return;
		}
	}

	fprintf(stdout, "%s: not found\n", ctx->args);
}
