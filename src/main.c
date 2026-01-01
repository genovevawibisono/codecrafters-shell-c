#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 1024

int main(int argc, char *argv[]) {
	while (1) {
		// Flush after every printf
		setbuf(stdout, NULL);

		// TODO: Uncomment the code below to pass the first stage
		fprintf(stdout, "$ ");

		char command[MAX_COMMAND_LENGTH];
		fgets(command, MAX_COMMAND_LENGTH, stdin);
		command[strlen(command) - 1] = '\0';
		fprintf(stdout, "%s: command not found\n", command);
	}

    return 0;
}
