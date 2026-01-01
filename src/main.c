#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exit.h"

#define MAX_COMMAND_LENGTH 1024
#define DEFAULT_EXIT_STATUS 0

int main(int argc, char *argv[]) {
	while (1) {
		// Flush after every printf
		setbuf(stdout, NULL);

		// Uncomment the code below to pass the first stage
		fprintf(stdout, "$ ");

		char command[MAX_COMMAND_LENGTH];
		fgets(command, MAX_COMMAND_LENGTH, stdin);
		command[strlen(command) - 1] = '\0';

		if (strncmp(command,  "exit", strlen(command)) == 0) {
			shell_exit(DEFAULT_EXIT_STATUS);
		} else {
			fprintf(stdout, "%s: command not found\n", command);
		}
	}

    return 0;
}
