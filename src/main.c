/* INCLUDE LIBRARIES */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

/* DEFINE CONSTANTS */
#define MAX_COMMAND_LENGTH 1024
#define DEFAULT_EXIT_STATUS 0
#define EXIT_LENGTH 4
#define ECHO_LENGTH 4
#define MAX_PATH_LENGTH 1024

/* DEFINE STRUCTS AND TYPEDEFS */
struct command_context {
	bool redirect;
	char *out_file;
	char *command_name;
    int argc;
    char **argv;
};

typedef void (*command_function)(struct command_context *);

struct command {
	const char *name;
	command_function func;
};

/* FUNCTION HEADERS */
static void trim_newline(char *s);
static void split_command(char *line, char **cmd, char **args);
static void populate_argv(struct command_context *ctx, char *cmd);
static void debug_print_context(struct command_context *ctx);
static void shell_exit(struct command_context *ctx);
static void shell_echo(struct command_context *ctx);
static void shell_type(struct command_context *ctx);
static void shell_exec(struct command_context *ctx);
static void shell_pwd(struct command_context *ctx);
static void shell_cd(struct command_context *ctx);

/* OTHER HELPERS TO MAKE LIFE EASIER */
struct command commands[] = {
    { "exit", shell_exit },
    { "echo", shell_echo },
	{ "type", shell_type },
    { "pwd", shell_pwd }, 
    { "cd", shell_cd },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* MAIN FUNCTION */

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
            .command_name = cmd,
            .argc = 0,
            .argv = NULL,
        };

        populate_argv(&ctx, args);

        // debug_print_context(&ctx);

        bool found = false;
        for (size_t i = 0; i < NUM_COMMANDS; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                commands[i].func(&ctx);
                found = true;
                break;
            }
        }

        // If not a builtin, try to execute it as an external program
        if (!found) {
            shell_exec(&ctx);
        }
    }
}

/* FUNCTION FUNCTIONS, LIKE THE REAL THINGS THAT DO THE WORK */
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

static void populate_argv(struct command_context *ctx, char *args) {
    int count = 1;
    
    if (strlen(args) > 0) {
        char *temp = strdup(args);
        char *token = strtok(temp, " ");
        
        while (token != NULL) {
            count++;
            token = strtok(NULL, " ");
        }
        free(temp);
    }
    
    ctx->argv = malloc((count + 1) * sizeof(char *));
    
    ctx->argv[0] = strdup(ctx->command_name);
    
    int i = 1;
    if (strlen(args) > 0) {
        char *token = strtok(args, " ");
        while (token != NULL) {
            ctx->argv[i] = strdup(token);
            i++;
            token = strtok(NULL, " ");
        }
    }
    
    ctx->argv[i] = NULL;
    ctx->argc = count;
}

static void debug_print_context(struct command_context *ctx) {
    fprintf(stderr, "=== Command Context Debug ===\n");
    fprintf(stderr, "Command name: %s\n", ctx->command_name ? ctx->command_name : "(null)");
    fprintf(stderr, "Redirect: %s\n", ctx->redirect ? "true" : "false");
    fprintf(stderr, "Output file: %s\n", ctx->out_file ? ctx->out_file : "(null)");
    fprintf(stderr, "argc: %d\n", ctx->argc);
    fprintf(stderr, "argv:\n");
    
    if (ctx->argv) {
        for (int i = 0; i < ctx->argc; i++) {
            fprintf(stderr, "  argv[%d]: %s\n", i, ctx->argv[i]);
        }
        fprintf(stderr, "  argv[%d]: %s (terminator)\n", ctx->argc, 
                ctx->argv[ctx->argc] ? ctx->argv[ctx->argc] : "NULL");
    } else {
        fprintf(stderr, "  (argv is NULL)\n");
    }
    
    fprintf(stderr, "=============================\n");
}

static void shell_exit(struct command_context *ctx) {
	(void) ctx;
    exit(DEFAULT_EXIT_STATUS);
}

static void shell_echo(struct command_context *ctx) {
    for (int i = 1; i < ctx->argc; i++) {
        fprintf(stdout, "%s ", ctx->argv[i]);
    }
    fprintf(stdout, "\n");
}

static void shell_type(struct command_context *ctx) {
    if (ctx->argc < 2) {
        fprintf(stderr, "type: missing argument\n");
        return;
    }
    
    char *target = ctx->argv[1]; 
    
	bool found = false;
	for (size_t i = 0; i < NUM_COMMANDS; i++) {
		if (strcmp(commands[i].name, target) == 0) {
			fprintf(stdout, "%s is a shell builtin\n", target);
			found = true;
			return;
		}
	}

	char *path_env = getenv("PATH");
	if (path_env) {
		char *path_copy = malloc(strlen(path_env) * sizeof(char));
        if (path_copy == NULL) {
            fprintf(stderr, "[shell type] failed to malloc for path copy string\n");
        }
        strcpy(path_copy, path_env);
        char *token = strtok(path_copy, ":");
        if (token == NULL) {
            fprintf(stderr, "[shell type] token is null\n");
        }
        while (token) {
            DIR *dir = opendir(token);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, target) == 0) {
                        char full_path[1024];
                        snprintf(full_path, sizeof(full_path), "%s/%s", token, entry->d_name);
                        if (access(full_path, X_OK) == 0) {
                            struct stat path_stat;
                            stat(full_path, &path_stat);
                            if (S_ISREG(path_stat.st_mode)) {
                                fprintf(stdout, "%s is %s\n", entry->d_name, full_path);
                                found = true;
                                break;
                            }
                        }
                    }
                }
                closedir(dir);
            }
            if (found) {
                break;
            }
            token = strtok(NULL, ":");
        }
	}
    if (!found) {
	    fprintf(stdout, "%s: not found\n", target);
    }
}

static void shell_exec(struct command_context *ctx) {
    char *path_env = getenv("PATH");
    char *executable_path = NULL;
    
    if (path_env) {
        char *path_copy = strdup(path_env);
        char *token = strtok(path_copy, ":");
        
        while (token) {
            DIR *dir = opendir(token);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ctx->command_name) == 0) {
                        char full_path[1024];
                        snprintf(full_path, sizeof(full_path), "%s/%s", 
                                token, entry->d_name);
                        
                        if (access(full_path, X_OK) == 0) {
                            struct stat path_stat;
                            stat(full_path, &path_stat);
                            if (S_ISREG(path_stat.st_mode)) {
                                executable_path = strdup(full_path);
                                break;
                            }
                        }
                    }
                }
                closedir(dir);
            }
            if (executable_path) {
                break;
            }
            token = strtok(NULL, ":");
        }
        free(path_copy);
    }
    
    if (!executable_path) {
        fprintf(stdout, "%s: command not found\n", ctx->command_name);
        return;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "[shell exec] failed to fork\n");
        free(executable_path);
        return;
    }

    if (pid == 0) {
        execv(executable_path, ctx->argv);
        fprintf(stderr, "[shell exec] error in execv\n");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    free(executable_path);
}

static void shell_pwd(struct command_context *ctx) {
    if (ctx == NULL) {
        fprintf(stderr, "[shell pwd] command context is NULL\n");
        return;
    }

    char current_directory[MAX_PATH_LENGTH];
    char *cwd;

    cwd = getcwd(current_directory, sizeof(current_directory));
    if (cwd == NULL) {
        fprintf(stderr, "[shell pwd] failed to getcwd\n");
        return;
    } 

    fprintf(stdout, "%s\n", current_directory);
}

static void shell_cd(struct command_context *ctx) {
    if (ctx == NULL) {
        fprintf(stderr, "[shell cd] command context is NULL\n");
        return;
    }

    if (ctx->argc < 2 || ctx->argv[1] == NULL) {
        fprintf(stderr, "cd: missing argument\n");
        return;
    }

    char *target_dir = ctx->argv[1];

    if (strcmp(target_dir, "~") == 0) {
        target_dir = getenv("HOME");
        if (target_dir == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
    }

    int res = chdir(target_dir);
    if (res == -1) {
        fprintf(stderr, "cd: %s: No such file or directory\n", ctx->argv[1]);
        return;
    }
}