#include "utilities.h"

static void trim_newline(char *s);
static void debug_print_context(struct command_context *ctx);

static void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}

void parse_command_line(char *line, struct command_context *ctx) {
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

    if (count > 0 && strcmp(ctx->argv[count - 1], "&") == 0) {
        ctx->background_job = 1;
        free(ctx->argv[--count]);
        ctx->argv[count] = NULL;
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

char *resolve_executable(const char *name) {
    char *path_env = getenv("PATH");
    if (!path_env) return NULL;

    char *path_copy = strdup(path_env);
    char *token = strtok(path_copy, ":");
    char *result = NULL;

    while (token) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", token, name);
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
            result = strdup(full_path);
            break;
        }
        token = strtok(NULL, ":");
    }

    free(path_copy);
    return result;
}

bool is_executable(const char *path) {
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
