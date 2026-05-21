#include "history.h"

void load_history_histfile(void) {
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

void write_history_histfile(void) {
    char *histfile = getenv("HISTFILE");

    if (histfile) {
        FILE *file = fopen(histfile, "w");
        if (file) {
            // Get history list
            HIST_ENTRY **hist_list = history_list();
            
            if (hist_list) {
                // Write each history entry to file
                for (int i = 0; i < history_length; i++) {
                    if (hist_list[i]) {
                        fprintf(file, "%s\n", hist_list[i]->line);
                    }
                }
            }
            fclose(file);
        }
    }
}