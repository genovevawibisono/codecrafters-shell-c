#include "builtins.h"
#include "dictionary.h"

struct command commands[] = {
    { "exit", shell_exit },
    { "echo", shell_echo },
    { "type", shell_type },
    { "pwd", shell_pwd },
    { "cd", shell_cd },
    { "history", shell_history },
    { "jobs", shell_jobs },
    { "complete", shell_complete },
    { "declare", shell_declare },
};

char *command_names[] = {
    "exit",
    "echo",
    "type",
    "pwd",
    "cd",
    "history",
    "jobs",
    "complete",
    "declare",
    NULL,
};

static void shell_exit(struct command_context *ctx) {
	(void) ctx;

    write_history_histfile();

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

void shell_exec(struct command_context *ctx) {
    char *executable_path = resolve_executable(ctx->command_name);

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

// Helper to check if a command is a builtin
bool is_builtin(const char *command_name) {
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(commands[i].name, command_name) == 0) {
            return true;
        }
    }
    return false;
}

// Helper to get builtin function
command_function get_builtin_function(const char *command_name) {
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(commands[i].name, command_name) == 0) {
            return commands[i].func;
        }
    }
    return NULL;
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

static void shell_jobs(struct command_context *ctx) {
    if (ctx == NULL) {
        fprintf(stderr, "[shell jobs] command context is NULL\n");
        return;
    }

    dictionary_jobs(dictionary);
}

static void shell_complete(struct command_context *ctx) {
    if (ctx->argc < 2) {
        fprintf(stderr, "complete: usage: complete -p [command] | complete <flag> <value> <command>\n");
        return;
    }

    if (strcmp(ctx->argv[1], "-p") == 0) {
        const char *command = (ctx->argc >= 3) ? ctx->argv[2] : NULL;
        int found = complete_print(command);
        if (!found && command != NULL) {
            fprintf(stderr, "complete: %s: no completion specification\n", command);
        }
        return;
    }

    if (ctx->argc >= 3 && strcmp(ctx->argv[1], "-r") == 0) {
        complete_remove(ctx->argv[2]);
        return;
    }

    // complete <flag> <value> <command>  e.g. -F, -C, -W ...
    if (ctx->argc >= 4 && ctx->argv[1][0] == '-') {
        complete_add(ctx->argv[1], ctx->argv[2], ctx->argv[3]);
        return;
    }

    fprintf(stderr, "complete: usage: complete -p [command] | complete <flag> <value> <command>\n");
}

static void shell_declare(struct command_context *ctx) {
    if (ctx->argc < 2) return;

    if (strcmp(ctx->argv[1], "-p") == 0) {
        if (ctx->argc < 3) return;
        const char *name = ctx->argv[2];
        const char *value = var_get(name);
        if (value == NULL) {
            fprintf(stderr, "declare: %s: not found\n", name);
        } else {
            fprintf(stdout, "declare -- %s=\"%s\"\n", name, value);
        }
        return;
    }

    // declare NAME=VALUE
    char *eq = strchr(ctx->argv[1], '=');
    if (eq == NULL) return;
    *eq = '\0';
    var_set(ctx->argv[1], eq + 1);
    *eq = '=';
}
