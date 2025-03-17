#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
#define 	EXIT_0 	"exit 0"
#define 	ECHO 	"echo "
#define 	TYPE 	"type "
#define 	PWD 	"pwd"
#define 	CD		"cd"
#define		CAT		"cat"

///////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTION HEADERS
static int driver(void);
static void myExit(void);
static void myEcho(char *str);
static void myType(char *str);
static bool myTypeCommandsCheck(char *str, char commands[][16], int commandsSize);
static void myTypeCommands(char *str, char commands[][16], int commandsSize);
static bool myTypeFileCheck(char *str);
static void myTypeFile(char *str);
static void myExec(char *path, int argc, char **argv, char *redirFile, int redirectType);
static bool fileExists(char *str);
static char *getFile(char *str);
static void myPwd(void);
static void myCd(char *dest);
static char *parseSingleQuote(char *str);
static void myCat(int argc, char *argv[], char *redirFile, int redirectType);
static void readFile(const char *str);
static char *parseDoubleQuote(char *str);
static bool singleQuoteCheck(char *str);
static bool doubleQuoteCheck(char *str);
static void noQuoteParse(char *str);
static char *parseStringWithEscapeNoQuotes(char *str);
static void parseStringWithEscapeDoubleQuotes(const char *input);
static bool doubleQuotesHasBackSlash(char *str);
static char *stripQuotesAndEscapes(char *str);
static int redirectStdoutToFile(const char *filename);
static int redirectStderrToFile(const char *filename);
static int appendStdoutToFile(const char *filename);
static int appendStderrToFile(const char *filename);

///////////////////////////////////////////////////////////////////////////////
// MAIN FUNCTION
int main(void) {
	return driver();
}

///////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS
static int driver(void) {
	while (true) {
		setbuf(stdout, NULL);
		printf("$ ");

		// Wait for user input
		char input[100];
		fgets(input, 100, stdin);
		int inputLength = strlen(input);
		input[strcspn(input, "\n")] = '\0';

		char *redirStdoutAppend = strstr(input, "1>>");
		char *redirStderrAppend = strstr(input, "2>>");
		char *redirStdout = strstr(input, "1>");
		char *redirStderr = strstr(input, "2>");
		char *append = strstr(input, ">>");

        char *redir = NULL;
		// 0 = no redirection, 1 = stdout, 2 = stderr, 3 = stdout append, 4 = stderr append
        int redirectType = 0;

		if (redirStderrAppend != NULL) {
			memmove(redirStderrAppend, redirStderrAppend + 1, strlen(redirStderrAppend)); 
            redir = redirStderrAppend;
			// Append stderr
            redirectType = 4; 
		} 
		else if (redirStdoutAppend != NULL) { 
            memmove(redirStdoutAppend, redirStdoutAppend + 1, strlen(redirStdoutAppend)); 
            redir = redirStdoutAppend;
			// Append stdout
            redirectType = 3; 
        } 
		else if (redirStderr != NULL) {
            memmove(redirStderr, redirStderr + 1, strlen(redirStderr)); 
            redir = redirStderr;
            redirectType = 2;
        } 
		else if (redirStdout != NULL) {
			// Remove `1`
            memmove(redirStdout, redirStdout + 1, strlen(redirStdout)); 
            redir = redirStdout;
            redirectType = 1;
        } 
		else if (append != NULL) {
            redir = append;
            redirectType = 3; 
        } 
		else {
            redir = strchr(input, '>');
            redirectType = 1; 
        }

        char *filename = NULL;
        if (redir != NULL) {
            *redir = '\0';  
            redir += (redirectType == 3 || redirectType == 4) ? 2 : 1;
            while (isspace(*redir)) redir++; 
            filename = stripQuotesAndEscapes(redir); 
        	char *cleanedInput = stripQuotesAndEscapes(input);

			int savedFd = -1;
			if (filename != NULL) {
				savedFd = (redirectType == 1 || redirectType == 3) ? dup(STDOUT_FILENO) : dup(STDERR_FILENO);
				if (redirectType == 1) {
					if (redirectStdoutToFile(filename) == -1) {
						continue;
					}
				} 
				else if (redirectType == 3) {
					if (appendStdoutToFile(filename) == -1) {
						continue;
					}
				}
				else if (redirectType == 2) {
					if (redirectStderrToFile(filename) == -1) {
						continue;
					}
				else if (redirectType == 4) {
					if (appendStderrToFile(filename) == -1) {
						continue;
					}
				}
            }

            char *token = strtok(cleanedInput, " ");
            if (token == NULL) continue; 

            if (strcmp(token, "echo") == 0) {
                myEcho(cleanedInput + 5);
            } else if (strcmp(token, "type") == 0) {
                myType(cleanedInput + 5);
            } else if (strcmp(token, "pwd") == 0) {
                myPwd();
            } else if (strcmp(token, "cd") == 0) {
                myCd(cleanedInput + 3);
            } else if (strcmp(token, "cat") == 0) {
                char *argv[15];
				int argc = 0;
				char *token = strtok(cleanedInput + 4, " ");
				while (token != NULL && argc < 15) {
					argv[argc++] = token;
					token = strtok(NULL, " ");
				}
				argv[argc] = NULL;
				myCat(argc, argv, filename, redirectType);
            } else {
                char *argv[15];
                int argc = 0;
                while (token != NULL && argc < 15) {
                    argv[argc++] = token;
                    token = strtok(NULL, " ");
                }
                argv[argc] = NULL;
                char *path = getFile(argv[0]);
                if (path != NULL) {
                    myExec(path, argc, argv, filename, redirectType);
                } else {
                    fprintf(stderr, "%s: command not found\n", argv[0]);
                }
            }

            if (filename != NULL) {
            fflush(stdout);
            if (redirectType == 1 || redirectType == 3) {
                dup2(savedFd, STDOUT_FILENO);
            } else if (redirectType == 2 || redirectType == 4) {
                dup2(savedFd, STDERR_FILENO);
            }
            close(savedFd);
        }
        } else {
            if (strcmp(cleanedInput, "exit 0") == 0) {
                free(cleanedInput);
                myExit();
            } else {
                char *argv[15];
                int argc = 0;
                char *token = strtok(cleanedInput, " ");
                while (token != NULL && argc < 15) {
                    argv[argc++] = token;
                    token = strtok(NULL, " ");
                }
                argv[argc] = NULL;
                char *path = getFile(argv[0]);
                if (path != NULL) {
                    myExec(path, argc, argv, filename, redirectType);
                } else {
                    fprintf(stderr, "%s: command not found\n", argv[0]);
                }
            }
        }
        free(cleanedInput);
        }
		else {
			if (strcmp(input, EXIT_0) == 0) {
				myExit();
			}
			else if (strncmp(input, ECHO, 5 * sizeof(char)) == 0) {
				char *buffer = malloc(100 * sizeof(char));
				strcpy(buffer, input + 5);
				if (singleQuoteCheck(buffer) == true) {
					buffer = parseSingleQuote(buffer);
					myEcho(buffer);
					free(buffer);
					continue;
				}
				else if (doubleQuoteCheck(buffer) == true) {
					if (doubleQuotesHasBackSlash(buffer) == true) {
						parseStringWithEscapeDoubleQuotes(buffer);
						free(buffer);
						continue;
					}
					else {
						buffer = parseDoubleQuote(buffer);
						printf("%s\n", buffer);
						free(buffer);
						continue;
					}
				}
				else {
					noQuoteParse(buffer);
					buffer = parseStringWithEscapeNoQuotes(buffer);
					myEcho(buffer);
					free(buffer);
					continue;
				}
			}
			else if (strncmp(input, TYPE, 5 * sizeof(char)) == 0) {
				char *buffer = malloc(100 * sizeof(char));
				strcpy(buffer, input + 5);
				myType(buffer);
				free(buffer);
				continue;
			}
			else if (strcmp(input, PWD) == 0) {
				myPwd();
				continue;
			}
			else if (strncmp(input, CD, 2 * sizeof(char)) == 0) {
				myCd(input + 3);
			}
			else if (strncmp(input, CAT, 3 * sizeof(char)) == 0) {
				char *buffer = malloc(100 * sizeof(char));
				strcpy(buffer, input + 4);
				if (singleQuoteCheck(buffer) == true) {
					char *argv[15];
					int argc = 0;
					char *current = buffer;
					while (*current != '\0' && argc < 15) {
						while (isspace(*current) == true) {
							current++;
						}
						if (*current == '\'') {
							current++;
							argv[argc++] = current;
							while (*current != '\'' && *current != '\0') {
								current++;
							}
							if (*current == '\'') {
								*current++ = '\0';
							}
						} else {
							argv[argc++] = current;
							while (isspace(*current) == false && *current != '\0') {
								current++;
							}
							if (*current != '\0') {
								*current++ = '\0';
							}
						}
					}
					argv[argc] = NULL;
					myCat(argc, argv, filename, redirectType);
					free(buffer);
					continue;
				}
				else if (doubleQuoteCheck(buffer) == true) {
					char *argv[15];
					int argc = 0;
					char *current = buffer;
					while (*current != '\0' && argc < 15) {
						while (isspace(*current) == true) {
							current++;
						}
						if (*current == '\'' || *current == '\"') {
							char quote = *current++; 
							argv[argc++] = current;  
							while (*current != quote && *current != '\0') current++;
							if (*current == quote) {
								*current++ = '\0'; 
							}
						} else {
							argv[argc++] = current;
							while (isspace(*current) == false && *current != '\0') {
								current++;
							}
							if (*current != '\0') {
								*current++ = '\0';
							}  
						}
					}
					argv[argc] = NULL;
					myCat(argc, argv, filename, redirectType);
					free(buffer);
					continue;
				}
				else {
					char *argv[15];
					int argc = 0;
					char *current = buffer;

					while (*current != '\0' && argc < 15) {
						while (isspace(*current)) {
							// Skip leading spaces
							current++;  
						}

						if (*current == '\'' || *current == '\"') {
							// Detect if it's single or double quote
							char quote = *current++;  
							// Save start of filename
							argv[argc++] = current;   
							while (*current != quote && *current != '\0') {
								current++;
							}
							if (*current == quote) {
								// Terminate string
								*current++ = '\0';  
							}
						} else {
							// Save unquoted filename
							argv[argc++] = current;  
							while (*current != '\0' && !isspace(*current)) {
								current++;
							}
							if (*current != '\0') {
								// Terminate filename
								*current++ = '\0';  
							}
						}
					}
					argv[argc] = NULL;
					// Execute cat with parsed filenames
					myCat(argc, argv, filename, redirectType);  
					free(buffer);
    				continue;
				}
			}
			else {
				char *argv[15];
				int argc = 0;
				char *current = input;
				while (*current != '\0' && argc < 15) {
					while (isspace(*current) == true) {
						current++;
					} 
					if (*current == '\'' || *current == '\"') {
						char quote = *current++;
						argv[argc++] = current;
						while (*current != quote && *current != '\0') current++;
						if (*current == quote) *current++ = '\0';
					} else {
						argv[argc++] = current;
						while (isspace(*current) == false && *current != '\0') current++;
						if (*current != '\0') *current++ = '\0';
					}
				}
				argv[argc] = NULL;
				char *path = getFile(argv[0]);
				if (path != NULL) {
					myExec(path, argc, argv, filename, redirectType);
				}
				else {
					printf("%s: command not found\n", argv[0]);
				}
			}
		}
	}
	return 0;
}

static void myExit(void) {
	exit(EXIT_SUCCESS);
}

static void myEcho(char *input) {
	printf("%s\n", input);
}

static void myType(char *str) {
	char commands[][16] = {"echo", "exit", "type", "pwd", "cd"};
	int commandsSize = sizeof(commands) / sizeof(commands[0]);
	if (myTypeCommandsCheck(str, commands, commandsSize) == true) {
		myTypeCommands(str, commands, commandsSize);
	}
	else if (myTypeFileCheck(str) == true) {
		myTypeFile(str);
	}
	else {
		printf("%s: not found\n", str);
	}
}

static bool myTypeCommandsCheck(char *str, char commands[][16], int commandsSize) {
	for (int i = 0; i < commandsSize; i++) {
		if (strcmp(str, commands[i]) == 0) {
			return true;
		}
	}
	return false;
}

static void myTypeCommands(char *str, char commands[][16], int commandsSize) {
	for (int i = 0; i < commandsSize; i++) {
		if (strcmp(str, commands[i]) == 0) {
			printf("%s is a shell builtin\n", str);
			return;
		}
	}
}

static bool myTypeFileCheck(char *str) {
	char *path = getenv("PATH");
	int pathLength = strlen(path);
	char *envPath = malloc(pathLength * sizeof(char));
	strcpy(envPath, path);
	char *dirpath = strtok(envPath, ":");
	while (dirpath != NULL) {
		DIR *directory = opendir(dirpath);
		if (directory == NULL) {
			dirpath = strtok(NULL, ":");
			continue;
		}
		struct dirent *file;
		while ((file = readdir(directory)) != NULL) {
			if (strcmp(file->d_name, str) == 0) {
				free(envPath);
				closedir(directory);
				return true;
			}
		}
		dirpath = strtok(NULL, ":");
		closedir(directory);
	}
	free(envPath);
	return false;
}

static void myTypeFile(char *str) {
	char *path = getenv("PATH");
	int pathLength = strlen(path);
	char *envPath = malloc(pathLength * sizeof(char));
	strcpy(envPath, path);
	char *dirpath = strtok(envPath, ":");
	while (dirpath != NULL) {
		DIR *directory = opendir(dirpath);
		if (directory == NULL) {
			dirpath = strtok(NULL, ":");
			continue;
		}
		struct dirent *file;
		while ((file = readdir(directory)) != NULL) {
			if (strcmp(file->d_name, str) == 0) {
				printf("%s is %s/%s\n", str, dirpath, file->d_name);
				free(envPath);
				closedir(directory);
				return;
			}
		}
		dirpath = strtok(NULL, ":");
		closedir(directory);
	}
	free(envPath);
	printf("%s not found\n", str);
	return;
}

static void myExec(char *path, int argc, char **argv, char *redirFile, int redirectType) {
    pid_t pid = fork();
    if (pid == 0) { // Child process
        if (redirFile != NULL && redirectType != 0) {
            if (redirectType == 1) {
                if (redirectStdoutToFile(redirFile) == -1) exit(1);
            } else if (redirectType == 2) {
                if (redirectStderrToFile(redirFile) == -1) exit(1);
            } else if (redirectType == 3) {
                if (appendStdoutToFile(redirFile) == -1) exit(1);
            } else if (redirectType == 4) {
                if (appendStderrToFile(redirFile) == -1) exit(1);
            }
        }
        execv(path, argv);
        perror("execv");
        exit(1);
    } else if (pid < 0) {
        perror("fork");
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

static bool fileExists(char *str) {
	char *path = getenv("PATH");
	int pathLength = strlen(path);
	char *filePath = malloc(pathLength * sizeof(char));
	strcpy(filePath, path);
	char *dirpath = strtok(filePath, ":");
	while (dirpath != NULL) {
		DIR *d = opendir(dirpath);
		if (d == NULL) {
			dirpath = strtok(NULL, ":");
			continue;
		}
		struct dirent *file;
		while ((file = readdir(d)) != NULL) {
			if (strcmp(str, file->d_name) == 0) {
				free(filePath);
				closedir(d);
				return true;
			}
		}
		dirpath = strtok(NULL, ":");
		closedir(d);
	}
	free(filePath);
	return false;
}

static char *getFile(char *str) {
	char *cleaned = stripQuotesAndEscapes(str);
	char *path = getenv("PATH");
	int pathLength = strlen(path);
	char *filePath = malloc(pathLength * sizeof(char));
	strcpy(filePath, path);
	char *dirPath = strtok(filePath, ":");
	static char fullPath[1024];
	while (dirPath != NULL) {
		snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, str);
		if (access(fullPath, X_OK) == 0) {
			free(filePath);
			return fullPath;
		}
		dirPath = strtok(NULL, ":");
	}
	free(filePath);
	return NULL;
}

static void myPwd(void) {
	char *buffer = malloc(MAXPATHLEN * sizeof(char));
	if (getcwd(buffer, MAXPATHLEN) != NULL) {
		printf("%s\n", buffer);
	}
	else {
		fprintf(stderr, "An error occurred.\n");
	}
}

static void myCd(char *dest) {
	if (strcmp(dest, "~") == 0) {
		char *home = getenv("HOME");
		dest = home;
	}
	if (chdir(dest) < 0) {
		fprintf(stderr, "cd: %s: No such file or directory\n", dest);
	}
}

static char *parseSingleQuote(char *str) {
    int n = strlen(str), idx = 0;
    char *parsed = malloc((n + 1) * sizeof(char));
    bool inQuotes = false;
    for (int i = 0; i < n; i++) {
        if (str[i] == '\'') {
            inQuotes = !inQuotes;
        } else if (inQuotes == true) {
            parsed[idx++] = str[i];
        } else if (isspace(str[i]) == false|| (idx > 0 && isspace(parsed[idx - 1]) == false)) {
            parsed[idx++] = str[i];
        }
    }
    if (idx > 0 && (isspace(parsed[idx - 1]) == true)) {
        idx--;
    }
    parsed[idx] = '\0';
    return parsed;
}

static void myCat(int argc, char *argv[], char *redirFile, int redirectType) {
    pid_t pid = fork();
    if (pid == 0) {
        if (redirFile != NULL && redirectType != 0) {
            if (redirectType == 1 && redirectStdoutToFile(redirFile) == -1) exit(1);
            else if (redirectType == 2 && redirectStderrToFile(redirFile) == -1) exit(1);
            else if (redirectType == 3 && appendStdoutToFile(redirFile) == -1) exit(1);
            else if (redirectType == 4 && appendStderrToFile(redirFile) == -1) exit(1);
        }
        for (int i = 0; i < argc; i++) {
            readFile(argv[i]);
        }
        fflush(stdout);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork");
    }
}

static void readFile(const char *str) {
    char *cleanPath = strdup(str);
    if (cleanPath[0] == '\'' && cleanPath[strlen(cleanPath) - 1] == '\'') {
        cleanPath[strlen(cleanPath) - 1] = '\0';
        cleanPath++;
    }
    FILE *file = fopen(cleanPath, "r");
    if (file == NULL) {
        fprintf(stderr, "cat: %s: No such file or directory\n", cleanPath);
        return;
    }
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        fputc(ch, stdout);
    }
    fclose(file);
}

static char *parseDoubleQuote(char *str) {
    int n = strlen(str), idx = 0;
    char *parsed = malloc((n + 1) * sizeof(char));
    bool inQuotes = false;
    for (int i = 0; i < n; i++) {
        if (str[i] == '\"') {
            inQuotes = !inQuotes;
        } else if (inQuotes == true || isspace(str[i]) == false) {
            parsed[idx++] = str[i];
        } else if (idx > 0 && isspace(parsed[idx - 1]) == false) {
            parsed[idx++] = ' ';
        }
    }
    if (idx > 0 && isspace(parsed[idx - 1]) == true) {
        idx--;
    }
    parsed[idx] = '\0';
    return parsed;
}

static bool singleQuoteCheck(char *str) {
	int len = strlen(str);
	return str[0] == '\'' && str[len - 1] == '\'';
}

static bool doubleQuoteCheck(char *str) {
	int len = strlen(str);
	return str[0] == '\"' && str[len - 1] == '\"';
}

static void noQuoteParse(char *str) {
	int i = 0, j = 0;
    bool inWord = false;
    while (str[i] != '\0') {
        if (isspace(str[i]) == false) {
            if (inWord == false && j > 0) {
                str[j++] = ' ';
            }
            str[j++] = str[i];
            inWord = true;
        } else {
            inWord = false;
        }
        i++;
    }
    str[j] = '\0';
}

static char *parseStringWithEscapeNoQuotes(char *str) {
    int n = strlen(str), idx = 0;
    char *parsed = malloc((n + 1) * sizeof(char));
    bool inQuotes = false;
    char quoteChar = '\0';
    for (int i = 0; i < n; i++) {
        if (str[i] == '\\') { 
            if (i + 1 < n) {
                parsed[idx++] = str[++i];
            }
        } else if (str[i] == '\'' || str[i] == '\"') { 
            if (inQuotes == false) {
                inQuotes = true;
                quoteChar = str[i];
            } else if (quoteChar == str[i]) {
                inQuotes = false;
                quoteChar = '\0';
            }
        } else if (inQuotes == true || isspace(str[i]) == false || (idx > 0 && isspace(parsed[idx - 1]) == false)) {
            parsed[idx++] = isspace(str[i]) && !inQuotes ? ' ' : str[i];
        }
    }
    if (idx > 0 && isspace(parsed[idx - 1]) == true) {
        idx--; 
    }
    parsed[idx] = '\0';
    return parsed;
}

static void parseStringWithEscapeDoubleQuotes(const char *input) {
	int i = 0;
    int in_single_quotes = 0;
    int in_double_quotes = 0;

    while (input[i] != '\0') {
        if (input[i] == '\'' && !in_double_quotes) {
			// Toggle single quotes
            in_single_quotes = !in_single_quotes; 
        } else if (input[i] == '"' && !in_single_quotes) {
			// Toggle double quotes
            in_double_quotes = !in_double_quotes; 
        } else if (input[i] == '\\' && input[i + 1] != '\0') {
            // Handle escape sequences
            i++;
            if (input[i] == 'n') {
                putchar('\n');
            } else if (input[i] == 't') {
                putchar('\t');
            } else {
				// Print the escaped character
                putchar(input[i]); 
            }
        } else {
			// Print normal characters
            putchar(input[i]); 
        }
        i++;
    }
    putchar('\n');
}

static bool doubleQuotesHasBackSlash(char *str) {
	int n = strlen(str);
	for (int i = 0; i < n; i++) {
		if (str[i] == '\\') {
			return true;
		}
	}
	return false;
}

static char *stripQuotesAndEscapes(char *str) {
    size_t len = strlen(str);
    char *result = malloc((len + 1) * sizeof(char));
    size_t idx = 0;

    bool inQuotes = false;
    char quoteType = '\0';
    for (size_t i = 0; i < len; ++i) {
        if ((str[i] == '\'' || str[i] == '"') && !inQuotes) {
            inQuotes = true;
            quoteType = str[i];
        } else if (str[i] == quoteType && inQuotes) {
            inQuotes = false;
        } else if (str[i] == '\\' && i + 1 < len) {
			// Add escaped character
            result[idx++] = str[++i];  
        } else {
            result[idx++] = str[i];
        }
    }
    result[idx] = '\0';
    return result;
}

static int redirectStdoutToFile(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

static int redirectStderrToFile(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    if (dup2(fd, STDERR_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int appendStdoutToFile(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644); 
    if (fd == -1) {
        perror("open");
        return -1;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int appendStderrToFile(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644); 
    if (fd == -1) {
        perror("open");
        return -1;
    }

    if (dup2(fd, STDERR_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return -1;
    }

	fflush(stderr);
    close(fd);
    return 0;
}
