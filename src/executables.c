#include "executables.h"

void shell_exec_pipeline(struct command_context *ctx) {
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
char *find_executable_in_path(const char *command_name) {
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

void execute_builtin_in_fork(const char *command_name, char **argv, int argc, 
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
