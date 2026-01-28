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
#define DEFAULT_EXIT_STATUS 0
#define EXIT_LENGTH 4
#define ECHO_LENGTH 4
#define MAX_PATH_LENGTH 1024
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
};

typedef void (*command_function)(struct command_context *);

struct command {
	const char *name;
	command_function func;
};

/* FUNCTION HEADERS */
static void trim_newline(char *s);
static void parse_command_line(char *line, struct command_context *ctx);
static void debug_print_context(struct command_context *ctx);
static void shell_exit(struct command_context *ctx);
static void shell_echo(struct command_context *ctx);
static void shell_type(struct command_context *ctx);
static void shell_exec(struct command_context *ctx);
static void shell_pwd(struct command_context *ctx);
static void shell_cd(struct command_context *ctx);
static char *command_generator(const char *text, int state);
static char **command_completion(const char *text, int start, int end);

/* OTHER HELPERS TO MAKE LIFE EASIER */
struct command commands[] = {
    { "exit", shell_exit },
    { "echo", shell_echo },
	{ "type", shell_type },
    { "pwd", shell_pwd }, 
    { "cd", shell_cd },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

char *command_names[] = {
    "exit",
    "echo",
    NULL,
};

/* MAIN FUNCTION */

int main(void) {
    // Set up readline completion
    rl_attempted_completion_function = command_completion;

    char *line;

    while (1) {
        // Use readline instead of fgets - this is what enables TAB completion
        line = readline("$ ");
        
        // Check if we got EOF (Ctrl+D)
        if (!line) {
            break;
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            free(line);
            continue;
        }
        
        // Add to history (optional but nice - lets us use up arrow)
        add_history(line);
        
        struct command_context ctx = {
            .redirect = false,
            .out_file = NULL,
            .out_mode = O_TRUNC,
            .redirect_err = false,
            .error_file = NULL,
            .err_mode = O_TRUNC,
            .command_name = NULL,
            .argc = 0,
            .argv = NULL,
        };

        parse_command_line(line, &ctx);

        // Skip empty commands after parsing
        if (ctx.command_name == NULL || ctx.argc == 0) {
            free(line);
            continue;
        }

        bool found = false;
        for (size_t i = 0; i < NUM_COMMANDS; i++) {
            if (strcmp(ctx.command_name, commands[i].name) == 0) {
                commands[i].func(&ctx);
                found = true;
                break;
            }
        }

        // If not a builtin, try to execute it as an external program
        if (!found) {
            shell_exec(&ctx);
        }

        // Free allocated memory
        for (int i = 0; i < ctx.argc; i++) {
            free(ctx.argv[i]);
        }
        free(ctx.argv);

        if (ctx.out_file) {
            free(ctx.out_file);
        }

        if (ctx.error_file) {
            free(ctx.error_file);
        }
        
        free(line);
    }

    return 0;
}

/* FUNCTION FUNCTIONS, LIKE THE REAL THINGS THAT DO THE WORK */
static void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}

static void parse_command_line(char *line, struct command_context *ctx) {
    int count = 0;
    int capacity = ARGV_MAX_CAPACITY;
    ctx->argv = malloc(capacity * sizeof(char *));
    
    if (strlen(line) == 0) {
        ctx->command_name = NULL;
        ctx->argv[0] = NULL;
        ctx->argc = 0;
        return;
    }
    
    char token_buffer[MAX_COMMAND_LENGTH] = {0};
    int buffer_pos = 0;
    
    char *p = line;
    char quote_type = '\0';
    
    while (*p != '\0') {
        // Handle backslash OUTSIDE quotes
        if (*p == '\\' && quote_type == '\0') {
            p++;
            if (*p != '\0') {
                token_buffer[buffer_pos++] = *p;
                p++;
            }
            continue;
        }
        
        // Handle backslash INSIDE DOUBLE QUOTES
        if (*p == '\\' && quote_type == '"') {
            if (*(p + 1) == '"' || *(p + 1) == '\\' || 
                *(p + 1) == '$' || *(p + 1) == '`') {
                p++;
                token_buffer[buffer_pos++] = *p;
                p++;
                continue;
            }
        }
        
        // Handle quote characters
        if ((*p == '\'' || *p == '"') && quote_type == '\0') {
            quote_type = *p;
            p++;
            continue;
        }
        else if (*p == quote_type && quote_type != '\0') {
            quote_type = '\0';
            p++;
            continue;
        }
        
        // Handle spaces
        if (*p == ' ' && quote_type == '\0') {
            if (buffer_pos > 0) {
                token_buffer[buffer_pos] = '\0';
                if (count >= capacity) {
                    capacity *= 2;
                    ctx->argv = realloc(ctx->argv, capacity * sizeof(char *));
                }
                ctx->argv[count++] = strdup(token_buffer);
                buffer_pos = 0;
            }
            p++;
            continue;
        }
        
        // Regular character - accumulate it
        token_buffer[buffer_pos++] = *p;
        p++;
    }
    
    // Save last token if exists
    if (buffer_pos > 0) {
        token_buffer[buffer_pos] = '\0';
        if (count >= capacity) {
            capacity *= 2;
            ctx->argv = realloc(ctx->argv, capacity * sizeof(char *));
        }
        ctx->argv[count++] = strdup(token_buffer);
    }
    
    // Process redirect operators
    int final_argc = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(ctx->argv[i], ">") == 0 || strcmp(ctx->argv[i], "1>") == 0) {
            if (i + 1 < count) {
                ctx->redirect = true;
                ctx->out_file = strdup(ctx->argv[i + 1]);
                ctx->out_mode = O_TRUNC;  // Overwrite mode
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else if (strcmp(ctx->argv[i], ">>") == 0 || strcmp(ctx->argv[i], "1>>") == 0) {
            if (i + 1 < count) {
                ctx->redirect = true;
                ctx->out_file = strdup(ctx->argv[i + 1]);
                ctx->out_mode = O_APPEND;  // Append mode
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else if (strcmp(ctx->argv[i], "2>") == 0) {
            if (i + 1 < count) {
                ctx->redirect_err = true;
                ctx->error_file = strdup(ctx->argv[i + 1]);
                ctx->err_mode = O_TRUNC;  // Overwrite mode
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else if (strcmp(ctx->argv[i], "2>>") == 0) {
            if (i + 1 < count) {
                ctx->redirect_err = true;
                ctx->error_file = strdup(ctx->argv[i + 1]);
                ctx->err_mode = O_APPEND;  // Append mode
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else {
            ctx->argv[final_argc++] = ctx->argv[i];
        }
    }
    
    // First token is the command name
    if (final_argc > 0) {
        ctx->command_name = ctx->argv[0];
        ctx->argc = final_argc;
        ctx->argv[final_argc] = NULL;
    } else {
        ctx->command_name = NULL;
        ctx->argc = 0;
        ctx->argv[0] = NULL;
    }
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
    FILE *output = stdout;
    
    if (ctx->redirect && ctx->out_file) {
        // "w" for truncate (>), "a" for append (>>)
        const char *mode = (ctx->out_mode == O_APPEND) ? "a" : "w";
        output = fopen(ctx->out_file, mode);
        if (!output) {
            fprintf(stderr, "echo: %s: cannot create file\n", ctx->out_file);
            return;
        }
    }
    
    // Handle stderr redirection (create file even if we don't write to it)
    if (ctx->redirect_err && ctx->error_file) {
        const char *mode = (ctx->err_mode == O_APPEND) ? "a" : "w";
        FILE *err_file = fopen(ctx->error_file, mode);
        if (err_file) {
            fclose(err_file);
        }
    }
    
    for (int i = 1; i < ctx->argc; i++) {
        fprintf(output, "%s ", ctx->argv[i]);
    }
    fprintf(output, "\n");
    
    if (output != stdout) {
        fclose(output);
    }
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
        // Handle stdout redirection
        if (ctx->redirect && ctx->out_file) {
            int flags = O_WRONLY | O_CREAT | ctx->out_mode;
            int fd = open(ctx->out_file, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "%s: cannot create file\n", ctx->out_file);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Handle stderr redirection
        if (ctx->redirect_err && ctx->error_file) {
            int flags = O_WRONLY | O_CREAT | ctx->err_mode;
            int fd = open(ctx->error_file, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "%s: cannot create file\n", ctx->error_file);
                exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        
        execv(executable_path, ctx->argv);
        fprintf(stderr, "[shell exec] error in execv\n");
        exit(1);
    }

    // PARENT PROCESS
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
    char *cwd = getcwd(current_directory, sizeof(current_directory));
    if (cwd == NULL) {
        fprintf(stderr, "[shell pwd] failed to getcwd\n");
        return;
    }
    
    FILE *output = stdout;
    if (ctx->redirect && ctx->out_file) {
        output = fopen(ctx->out_file, "w");
        if (!output) {
            fprintf(stderr, "pwd: %s: cannot create file\n", ctx->out_file);
            return;
        }
    }
    
    // Handle stderr redirection (create file even if we don't write to it)
    if (ctx->redirect_err && ctx->error_file) {
        FILE *err_file = fopen(ctx->error_file, "w");
        if (err_file) {
            fclose(err_file);
        }
    }
    
    fprintf(output, "%s\n", current_directory);
    
    if (output != stdout) {
        fclose(output);
    }
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

static char *command_generator(const char *text, int state) {
    if (text == NULL) {
        fprintf(stderr, "[command generator] text is NULL\n");
        return NULL;
    }

    static int list_idx, len;
    char *name;

    if (!state) {
        list_idx = 0;
        len = strlen(text);
    }

    while ((name = command_names[list_idx++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

static char **command_completion(const char *text, int start, int end) {
    if (text == NULL) {
        fprintf(stderr, "[command completion] text is NULL\n");
        return NULL;
    }

    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }

    return NULL;
}
