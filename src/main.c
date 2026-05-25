/* INCLUDE HEADERS */
#include "structs.h"
#include "utilities.h"
#include "builtins.h"
#include "executables.h"
#include "generator.h"
#include "history.h"
#include "dictionary.h"
#include "jobs.h"

int main(void) {
    // Set up readline completion
    rl_attempted_completion_function = command_completion;

    load_history_histfile();
    dictionary = dictionary_new();

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
            .background_job = 0,
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
                if (ctx.background_job) {
                    job_add(0, &ctx);
                } else {
                    shell_exec(&ctx);
                }
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
