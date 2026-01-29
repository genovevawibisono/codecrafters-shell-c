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
    int num_commands;
    char ***all_commands;
    int *all_argc;
    char **all_command_names;
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
static bool is_executable(const char *path);
static char *path_executable_generator(const char *text, int state);
static void shell_exec_pipeline(struct command_context *ctx);
static char *find_executable_in_path(const char *command_name);
static bool is_builtin(const char *command_name);
static command_function get_builtin_function(const char *command_name);
static void execute_builtin_in_fork(const char *command_name, char **argv, int argc, 
                                   int stdin_fd, int stdout_fd);
static void shell_history(struct command_context *ctx);
static void load_history_histfile(void);

/* OTHER HELPERS TO MAKE LIFE EASIER */
struct command commands[] = {
    { "exit", shell_exit },
    { "echo", shell_echo },
	{ "type", shell_type },
    { "pwd", shell_pwd }, 
    { "cd", shell_cd },
    { "history", shell_history },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

char *command_names[] = {
    "exit",
    "echo",
    "type",
    "pwd",
    "cd",
    "history",
    NULL,
};

static int last_history_written = 0;

/* MAIN FUNCTION */

int main(void) {
    // Set up readline completion
    rl_attempted_completion_function = command_completion;

    load_history_histfile();

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
            .num_commands = 0,
            .all_argc = NULL,
            .all_commands = NULL,
            .all_command_names = NULL,
        };

        parse_command_line(line, &ctx);

        // Skip empty commands
        if (ctx.command_name == NULL || ctx.argc == 0) {
            free(line);
            continue;
        }

        // debug_print_context(&ctx);

        if (ctx.num_commands == 0 && (ctx.command_name == NULL || ctx.argc == 0)) {
            free(line);
            continue;
        }

        // Check if it's a pipeline
        if (ctx.num_commands > 0) {
            // Execute pipeline (works for 2, 3, 4... any number)
            shell_exec_pipeline(&ctx);
        } else {
            // Single command execution
            bool found = false;
            for (size_t i = 0; i < NUM_COMMANDS; i++) {
                if (strcmp(ctx.command_name, commands[i].name) == 0) {
                    commands[i].func(&ctx);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                shell_exec(&ctx);
            }
        }

        // Free allocated memory
        if (ctx.num_commands > 0) {
            // Pipeline
            for (int i = 0; i < ctx.num_commands; i++) {
                for (int j = 0; j < ctx.all_argc[i]; j++) {
                    free(ctx.all_commands[i][j]);
                }
                free(ctx.all_commands[i]);
            }
            free(ctx.all_commands);
            free(ctx.all_argc);
            free(ctx.all_command_names);
        } else {
            // Single command
            for (int i = 0; i < ctx.argc; i++) {
                if (ctx.argv[i]) free(ctx.argv[i]);
            }
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
        ctx->num_commands = 0;
        return;
    }
    
    char token_buffer[MAX_COMMAND_LENGTH] = {0};
    int buffer_pos = 0;
    
    char *p = line;
    char quote_type = '\0';
    
    // === TOKENIZATION LOOP (THIS WAS MISSING!) ===
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
    
    // === CHECK FOR PIPES ===
    int num_pipes = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(ctx->argv[i], "|") == 0) {
            num_pipes++;
        }
    }
    
    if (num_pipes > 0) {
        // Pipeline detected
        ctx->num_commands = num_pipes + 1;
        
        // Allocate arrays
        ctx->all_commands = malloc(ctx->num_commands * sizeof(char **));
        ctx->all_argc = malloc(ctx->num_commands * sizeof(int));
        ctx->all_command_names = malloc(ctx->num_commands * sizeof(char *));
        
        // Split into commands
        int cmd_idx = 0;
        int cmd_start = 0;
        
        for (int i = 0; i <= count; i++) {
            if (i == count || strcmp(ctx->argv[i], "|") == 0) {
                // End of a command
                int cmd_argc = i - cmd_start;
                
                ctx->all_commands[cmd_idx] = malloc((cmd_argc + 1) * sizeof(char *));
                for (int j = 0; j < cmd_argc; j++) {
                    ctx->all_commands[cmd_idx][j] = ctx->argv[cmd_start + j];
                }
                ctx->all_commands[cmd_idx][cmd_argc] = NULL;
                
                ctx->all_argc[cmd_idx] = cmd_argc;
                ctx->all_command_names[cmd_idx] = ctx->all_commands[cmd_idx][0];
                
                cmd_idx++;
                
                if (i < count) {
                    free(ctx->argv[i]); // Free pipe symbol
                }
                
                cmd_start = i + 1;
            }
        }
        
        // Set legacy single-command fields (for debug or compatibility)
        ctx->command_name = ctx->all_command_names[0];
        ctx->argc = ctx->all_argc[0];
        
        return;
    }
    
    // === NO PIPES - SINGLE COMMAND ===
    ctx->num_commands = 0;
    
    // Process redirect operators
    int final_argc = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(ctx->argv[i], ">") == 0 || strcmp(ctx->argv[i], "1>") == 0) {
            if (i + 1 < count) {
                ctx->redirect = true;
                ctx->out_file = strdup(ctx->argv[i + 1]);
                ctx->out_mode = O_TRUNC;
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else if (strcmp(ctx->argv[i], ">>") == 0 || strcmp(ctx->argv[i], "1>>") == 0) {
            if (i + 1 < count) {
                ctx->redirect = true;
                ctx->out_file = strdup(ctx->argv[i + 1]);
                ctx->out_mode = O_APPEND;
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else if (strcmp(ctx->argv[i], "2>") == 0) {
            if (i + 1 < count) {
                ctx->redirect_err = true;
                ctx->error_file = strdup(ctx->argv[i + 1]);
                ctx->err_mode = O_TRUNC;
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else if (strcmp(ctx->argv[i], "2>>") == 0) {
            if (i + 1 < count) {
                ctx->redirect_err = true;
                ctx->error_file = strdup(ctx->argv[i + 1]);
                ctx->err_mode = O_APPEND;
                free(ctx->argv[i]);
                free(ctx->argv[i + 1]);
                i++;
            }
        } else {
            ctx->argv[final_argc++] = ctx->argv[i];
        }
    }
    
    // Set command info
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
        fprintf(output, "%s", ctx->argv[i]);
        // Only add space if not the last arg
        if (i < ctx->argc - 1) {  
            fprintf(output, " ");
        }
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
    static int list_idx;
    static int text_len;
    static bool checking_builtins;
    
    // First call - initialize
    if (!state) {
        list_idx = 0;
        text_len = strlen(text);
        checking_builtins = true;
    }
    
    // First check builtins
    if (checking_builtins) {
        while (command_names[list_idx]) {
            char *name = command_names[list_idx++];
            if (strncmp(name, text, text_len) == 0) {
                return strdup(name);
            }
        }
        // Done with builtins, now check PATH
        checking_builtins = false;
    }
    
    // Now check PATH executables
    return path_executable_generator(text, state);
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

static bool is_executable(const char *path) {
    if (path == NULL) {
        fprintf(stderr, "path is NULL\n");
        return false;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        return false;
    }

    return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
}

static char *path_executable_generator(const char *text, int state) {
    if (text == NULL) {
        fprintf(stderr, "[path executable generator] text is NULL\n");
        return NULL;
    }

    static char **executable_list = NULL;
    static int list_idx, text_len;

    if (!state) {
        list_idx = 0;
        text_len = strlen(text);

        if (executable_list) {
            for (int i = 0; executable_list[i]; i++) {
                free(executable_list[i]);
            }
            free(executable_list);
            executable_list = NULL;
        }

        char *path_env = getenv("PATH");
        if (path_env == NULL) {
            fprintf(stderr, "[path executable generator] path env is NULL\n");
            return NULL;
        }

        int capacity = 1024, count = 0;
        executable_list = malloc(capacity * sizeof(char *));
        if (executable_list == NULL) {
            fprintf(stderr, "[path executable generator] failed to malloc for executable list\n");
            return NULL;
        }

        char *path_copy = strdup(path_env);
        char *token = strtok(path_copy, ":");
        
        while (token) {
            DIR *dir = opendir(token);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    // Skip . and ..
                    if (strcmp(entry->d_name, ".") == 0 || 
                        strcmp(entry->d_name, "..") == 0) {
                        continue;
                    }
                    
                    // Build full path
                    char full_path[MAX_PATH_LENGTH];
                    snprintf(full_path, sizeof(full_path), "%s/%s", 
                            token, entry->d_name);
                    
                    // Check if executable
                    if (is_executable(full_path)) {
                        // Check if we need to grow the list
                        if (count >= capacity - 1) {
                            capacity *= 2;
                            executable_list = realloc(executable_list, 
                                                     capacity * sizeof(char *));
                        }
                        
                        // Add to list (no duplicates check for simplicity)
                        executable_list[count++] = strdup(entry->d_name);
                    }
                }
                closedir(dir);
            }
            token = strtok(NULL, ":");
        }
        
        free(path_copy);
        executable_list[count] = NULL;
    }

    if (executable_list) {
        while (executable_list[list_idx]) {
            char *name = executable_list[list_idx++];
            if (strncmp(name, text, text_len) == 0) {
                return strdup(name);
            }
        }
    }

    return NULL;
}

static void shell_exec_pipeline(struct command_context *ctx) {
    int n = ctx->num_commands;
    
    // Check which commands are builtins and find executables
    bool *is_builtin_arr = malloc(n * sizeof(bool));
    char **exec_paths = malloc(n * sizeof(char *));
    
    for (int i = 0; i < n; i++) {
        is_builtin_arr[i] = is_builtin(ctx->all_command_names[i]);
        
        if (!is_builtin_arr[i]) {
            exec_paths[i] = find_executable_in_path(ctx->all_command_names[i]);
            if (!exec_paths[i]) {
                fprintf(stdout, "%s: command not found\n", ctx->all_command_names[i]);
                // Cleanup what we've allocated so far
                for (int j = 0; j < i; j++) {
                    if (exec_paths[j]) free(exec_paths[j]);
                }
                free(exec_paths);
                free(is_builtin_arr);
                return;
            }
        } else {
            exec_paths[i] = NULL;
        }
    }
    
    // Create pipes (n-1 pipes for n commands)
    int num_pipes = n - 1;
    int (*pipes)[2] = malloc(num_pipes * sizeof(int[2]));
    
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes[i]) == -1) {
            fprintf(stderr, "pipe: failed to create pipe\n");
            // Close pipes we've already created
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            free(pipes);
            for (int j = 0; j < n; j++) {
                if (exec_paths[j]) free(exec_paths[j]);
            }
            free(exec_paths);
            free(is_builtin_arr);
            return;
        }
    }
    
    // Fork for each command
    pid_t *pids = malloc(n * sizeof(pid_t));
    
    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            fprintf(stderr, "fork: failed\n");
            exit(1);
        }
        
        if (pids[i] == 0) {
            // CHILD PROCESS for command i
            
            // Redirect stdin from previous pipe (except first command)
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            
            // Redirect stdout to next pipe (except last command)
            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close ALL pipe file descriptors in child
            for (int j = 0; j < num_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            if (is_builtin_arr[i]) {
                // Builtin command
                command_function func = get_builtin_function(ctx->all_command_names[i]);
                if (!func) {
                    fprintf(stderr, "%s: builtin not found\n", ctx->all_command_names[i]);
                    exit(1);
                }
                
                struct command_context temp_ctx = {
                    .redirect = false,
                    .out_file = NULL,
                    .out_mode = O_TRUNC,
                    .redirect_err = false,
                    .error_file = NULL,
                    .err_mode = O_TRUNC,
                    .command_name = ctx->all_command_names[i],
                    .argc = ctx->all_argc[i],
                    .argv = ctx->all_commands[i],
                    .num_commands = 0,
                    .all_commands = NULL,
                    .all_argc = NULL,
                    .all_command_names = NULL,
                };
                
                func(&temp_ctx);
                exit(0);
            } else {
                // External command
                execv(exec_paths[i], ctx->all_commands[i]);
                fprintf(stderr, "execv: failed to execute %s\n", ctx->all_command_names[i]);
                exit(1);
            }
        }
    }
    
    // PARENT PROCESS
    // Close all pipes in parent
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }
    
    // Cleanup
    free(pipes);
    free(pids);
    for (int i = 0; i < n; i++) {
        if (exec_paths[i]) free(exec_paths[i]);
    }
    free(exec_paths);
    free(is_builtin_arr);
}

// Helper to find executable in PATH
static char *find_executable_in_path(const char *command_name) {
    char *path_env = getenv("PATH");
    if (!path_env) {
        return NULL;
    }
    
    char *path_copy = strdup(path_env);
    char *token = strtok(path_copy, ":");
    char *executable_path = NULL;
    
    while (token) {
        DIR *dir = opendir(token);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, command_name) == 0) {
                    char full_path[MAX_PATH_LENGTH];
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
    return executable_path;
}

// Helper to check if a command is a builtin
static bool is_builtin(const char *command_name) {
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(commands[i].name, command_name) == 0) {
            return true;
        }
    }
    return false;
}

// Helper to get builtin function
static command_function get_builtin_function(const char *command_name) {
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(commands[i].name, command_name) == 0) {
            return commands[i].func;
        }
    }
    return NULL;
}

static void execute_builtin_in_fork(const char *command_name, char **argv, int argc, 
                                   int stdin_fd, int stdout_fd) {
    command_function func = get_builtin_function(command_name);
    if (!func) {
        fprintf(stderr, "%s: builtin not found\n", command_name);
        exit(1);
    }
    
    // Redirect stdin if needed
    if (stdin_fd != STDIN_FILENO) {
        dup2(stdin_fd, STDIN_FILENO);
        close(stdin_fd);
    }
    
    // Redirect stdout if needed
    if (stdout_fd != STDOUT_FILENO) {
        dup2(stdout_fd, STDOUT_FILENO);
        close(stdout_fd);
    }
    
    // Create a temporary context for the builtin
    struct command_context temp_ctx = {
        .redirect = false,
        .out_file = NULL,
        .out_mode = O_TRUNC,
        .redirect_err = false,
        .error_file = NULL,
        .err_mode = O_TRUNC,
        .command_name = (char *)command_name,
        .argc = argc,
        .argv = argv,
        .num_commands = 0,           
        .all_commands = NULL,        
        .all_argc = NULL,            
        .all_command_names = NULL,
    };
    
    func(&temp_ctx);
    exit(0);
}

static void shell_history(struct command_context *ctx) {
    FILE *output = stdout;
    
    if (ctx->redirect && ctx->out_file) {
        const char *mode = (ctx->out_mode == O_APPEND) ? "a" : "w";
        output = fopen(ctx->out_file, mode);
        if (!output) {
            fprintf(stderr, "[shell history] history: %s: cannot create file\n", ctx->out_file);
            return;
        }
    }
    
    // Check for -r flag (read from file)
    if (ctx->argc >= 3 && strcmp(ctx->argv[1], "-r") == 0) {
        const char *filepath = ctx->argv[2];
        
        FILE *history_file = fopen(filepath, "r");
        if (!history_file) {
            fprintf(stderr, "[shell history] history: %s: cannot open file\n", filepath);
            if (output != stdout) {
                fclose(output);
            }
            return;
        }
        
        char line[MAX_COMMAND_LENGTH];
        while (fgets(line, sizeof(line), history_file)) {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }
            
            if (strlen(line) > 0) {
                add_history(line);
            }
        }
        
        fclose(history_file);
        
        if (output != stdout) {
            fclose(output);
        }
        return;
    }
    
    // Check for -w flag (write to file)
    if (ctx->argc >= 3 && strcmp(ctx->argv[1], "-w") == 0) {
        const char *filepath = ctx->argv[2];
        
        FILE *history_file = fopen(filepath, "w");
        if (!history_file) {
            fprintf(stderr, "[shell history] history: %s: cannot create file\n", filepath);
            if (output != stdout) {
                fclose(output);
            }
            return;
        }
        
        // Get history list
        HIST_ENTRY **hist_list = history_list();
        
        if (hist_list) {
            // Write each history entry to file
            for (int i = 0; i < history_length; i++) {
                if (hist_list[i]) {
                    fprintf(history_file, "%s\n", hist_list[i]->line);
                }
            }
        }

        last_history_written = history_length;
        
        fclose(history_file);
        
        if (output != stdout) {
            fclose(output);
        }
        return;
    }

    if (ctx->argc >= 3 && strcmp(ctx->argv[1], "-a") == 0) {
        const char *filepath = ctx->argv[2];
        
        // Open in APPEND mode
        FILE *history_file = fopen(filepath, "a");
        if (!history_file) {
            fprintf(stderr, "history: %s: cannot open file\n", filepath);
            if (output != stdout) {
                fclose(output);
            }
            return;
        }
        
        HIST_ENTRY **hist_list = history_list();
        
        if (hist_list) {
            // Only write NEW commands since last_history_written
            for (int i = last_history_written; i < history_length; i++) {
                if (hist_list[i]) {
                    fprintf(history_file, "%s\n", hist_list[i]->line);
                }
            }
        }
        
        // Update the last written position
        last_history_written = history_length;
        
        fclose(history_file);
        
        if (output != stdout) {
            fclose(output);
        }
        return;
    }
    
    // Normal history display 
    HIST_ENTRY **hist_list = history_list();
    
    if (!hist_list) {
        if (output != stdout) {
            fclose(output);
        }
        return;
    }
    
    int start_index = 0;
    int total_entries = history_length;
    
    if (ctx->argc >= 2) {
        int n = atoi(ctx->argv[1]);
        if (n > 0 && n < total_entries) {
            start_index = total_entries - n;
        }
    }
    
    for (int i = start_index; i < total_entries; i++) {
        if (hist_list[i]) {
            fprintf(output, "%5d  %s\n", i + history_base, hist_list[i]->line);
        }
    }
    
    if (output != stdout) {
        fclose(output);
    }
}

static void load_history_histfile(void) {
    char *histfile = getenv("HISTFILE");
    if (histfile) {
        FILE *file = fopen(histfile, "r");
        if (file) {
            char line[MAX_COMMAND_LENGTH];
            while (fgets(line, sizeof(line), file)) {
                // Remove trailing newline
                size_t len = strlen(line);
                if (len > 0 && line[len - 1] == '\n') {
                    line[len - 1] = '\0';
                }
                
                // Skip empty lines
                if (strlen(line) > 0) {
                    add_history(line);
                }
            }
            fclose(file);
        }
    }
}