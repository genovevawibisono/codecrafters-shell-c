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