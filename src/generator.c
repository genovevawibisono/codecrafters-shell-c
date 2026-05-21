#include "generator.h"

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

char **command_completion(const char *text, int start, int end) {
    if (text == NULL) {
        fprintf(stderr, "[command completion] text is NULL\n");
        return NULL;
    }

    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }

    char command[MAX_COMMAND_LENGTH] = {0};
    sscanf(rl_line_buffer, "%s", command);
    if (strcmp(command, "cd") == 0) {
        rl_completion_append_character = '\0';
        return rl_completion_matches(text, directory_generator);
    }

    /* For arguments (start != 0) provide filename completion from CWD */
    rl_filename_completion_desired = 1;
    return rl_completion_matches(text, rl_filename_completion_function);
}

/* Filename completion for arguments: search current directory for entries
 * starting with the given text and return them with a trailing space.
 */
static char *filename_generator(const char *text, int state) {
    static DIR *dir = NULL;
    static int text_len = 0;
    struct dirent *entry;

    if (text == NULL) {
        return NULL;
    }

    if (!state) {
        if (dir) {
            closedir(dir);
            dir = NULL;
        }

        dir = opendir(".");
        if (!dir) {
            return NULL;
        }
        text_len = strlen(text);
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip current/parent entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (strncmp(entry->d_name, text, text_len) == 0) {
            struct stat st;
            if (stat(entry->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
                char *result = malloc(strlen(entry->d_name) + 2);
                sprintf(result, "%s/", entry->d_name);
                return result;
            }
            return strdup(entry->d_name);
        }
    }

    if (dir) {
        closedir(dir);
        dir = NULL;
    }

    return NULL;
}

static char *directory_generator(const char *text, int state) {
    static DIR *dir = NULL;
    static int text_len = 0;
    struct dirent *entry;

    if (text == NULL) {
        return NULL;
    }

    if (!state) {
        if (dir) {
            closedir(dir);
            dir = NULL;
        }

        dir = opendir(".");
        if (!dir) {
            return NULL;
        }
        text_len = strlen(text);
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip current/parent entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if it's a directory
        struct stat st;
        if (stat(entry->d_name, &st) == -1) {
            continue;
        }
        if (!S_ISDIR(st.st_mode)) {
            continue;
        }

        if (strncmp(entry->d_name, text, text_len) == 0) {
            char *result = malloc(strlen(entry->d_name) + 2);
            sprintf(result, "%s/", entry->d_name);
            return result;
        }
    }

    if (dir) {
        closedir(dir);
        dir = NULL;
    }

    return NULL;
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
