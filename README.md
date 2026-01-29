# Toy Shell 

A feature-rich command-line shell built from scratch in C, demonstrating systems programming concepts including process management, inter-process communication, and I/O handling.

## Overview

This project implements a POSIX-compliant shell that supports command execution, pipelines, I/O redirection, command history, and interactive features like tab completion. Built as a learning exercise to understand how shells like bash and zsh work under the hood.

## Features

### Command Execution
- **Built-in Commands**: `exit`, `echo`, `type`, `pwd`, `cd`, `history`
- **External Programs**: Executes any executable found in the `PATH` environment variable
- **I/O Redirection**: Supports output (`>`, `>>`), and error redirection (`2>`, `2>>`)
- **Command Pipelines**: Chain unlimited commands together with the `|` operator

### History System
- **Persistent History**: Automatically loads command history from a file on startup
- **History Display**: View all commands or limit to the most recent N entries
- **File Operations**: Read from, write to, or append to history files
- **Intelligent Appending**: Tracks which commands have been written to avoid duplicates

### Interactive Features
- **Tab Completion**: Press TAB to auto-complete command names (both built-ins and executables)
- **Command Navigation**: Use arrow keys to browse through previous commands
- **Quote Support**: Handle both single and double quotes with proper escape sequences

## Design Decisions

### 1. Command Dispatch Table (Function Pointers)

**Problem**: How do you avoid a giant if-else chain or switch statement in main for every builtin command?

**Solution**: A dispatch table using function pointers.
```c
typedef void (*command_function)(struct command_context *);

struct command {
    const char *name;
    command_function func;
};

struct command commands[] = {
    { "exit", shell_exit },
    { "echo", shell_echo },
    { "type", shell_type },
    { "pwd", shell_pwd },
    { "cd", shell_cd },
    { "history", shell_history },
};
```

**Execution**:
```c
// Instead of this mess:
if (strcmp(cmd, "exit") == 0) {
    shell_exit(&ctx);
} else if (strcmp(cmd, "echo") == 0) {
    shell_echo(&ctx);
} else if (strcmp(cmd, "type") == 0) {
    shell_type(&ctx);
}
// ... and so on

// We do this:
for (size_t i = 0; i < NUM_COMMANDS; i++) {
    if (strcmp(command_name, commands[i].name) == 0) {
        commands[i].func(&ctx);  // Call via function pointer
        break;
    }
}
```

**Why this matters**:
- **Cleaner main()**: The main function stays lean - just lookup and dispatch
- **Easy to extend**: Adding a new builtin is just one line in the table
- **Single responsibility**: Each command implementation is isolated in its own function
- **Type safety**: Compiler ensures all command functions have the same signature

**Performance**: O(N) lookup where N = number of builtins (currently 6). For better performance with many builtins, this could be upgraded to a hash table, but linear search is fine for small N.

**The alternative**: Without this pattern, main() would be bloated with hundreds of lines of if-else logic, which makes the code not as unreadable and clean. 

### 2. Unified Command Context

**Problem**: How do you handle both single commands and complex pipelines with the same code?

**Solution**: A single data structure that adapts to both cases:
```c
struct command_context {
    // For single commands
    char *command_name;
    char **argv;
    int argc;
    
    // For pipelines (when num_commands > 0)
    int num_commands;
    char ***all_commands;    // Array of command arrays
    int *all_argc;
    char **all_command_names;
}
```

When `num_commands = 0`, it's a single command. When `num_commands > 0`, it's a pipeline with N commands. This eliminates code duplication and makes the pipeline logic naturally scalable.

**Why this matters**: Instead of having separate code paths for 2-command pipelines vs 3-command pipelines, we have one implementation that scales to any number. Adding support for `cat file | head | grep pattern | wc` required zero additional pipeline code.

### 3. Scalable Pipeline Architecture

**Problem**: How do you connect an arbitrary number of commands together?

**Solution**: Dynamic pipe array with index-based connections.

For N commands, we create N-1 pipes:
```
Command 0  →  [pipe 0]  →  Command 1  →  [pipe 1]  →  Command 2
```

Each command knows its position (index `i`) and connects accordingly:
- First command (i=0): Reads from terminal, writes to pipe[0]
- Middle commands: Reads from pipe[i-1], writes to pipe[i]
- Last command: Reads from pipe[N-2], writes to terminal

**Performance consideration**: All pipe file descriptors are closed immediately after forking. This is critical because:
1. Prevents file descriptor leaks (each pipe uses 2 FDs)
2. Allows commands to detect when input is closed (EOF)
3. Avoids hitting the OS limit on open files

### 4. Builtin Commands in Pipelines

**Problem**: Builtin commands run in the shell's process. How do you put them in a pipeline without affecting the parent shell?

**Solution**: Fork before executing builtins when they're in a pipeline.
```
Normal builtin:  shell_echo() runs directly in main process
Pipeline builtin: fork() → child runs shell_echo() → child exits
```

**Why fork for builtins?** 
- Redirecting stdout in the main process would break the shell's ability to print prompts
- If a builtin crashes, it shouldn't take down the entire shell
- Consistent behavior: all pipeline stages run in isolated processes

**Trade-off**: Slight overhead from forking, but ensures correctness and isolation.

### 5. Smart History Appending

**Problem**: When using `history -a` repeatedly, how do you avoid writing the same commands multiple times?

**Solution**: Position tracking with a global bookmark.
```c
static int last_history_written = 0;

// On history -a:
for (int i = last_history_written; i < history_length; i++) {
    write_to_file(history[i]);
}
last_history_written = history_length;
```

Think of it like a bookmark in a book - we remember where we stopped reading and continue from there next time.

**Why this approach?**: The alternative would be reading the entire file, comparing each line, and only writing new ones. This is O(N×M) complexity. Our approach is O(N) - we just track an integer.

### 6. Two-Phase Tab Completion

**Problem**: Tab completion needs to search both builtins and potentially thousands of executables in PATH. How do you make it fast?

**Solution**: Prioritized two-phase search.

**Phase 1**: Check builtin commands (instant - just an array lookup)
**Phase 2**: Scan PATH directories (slower - requires disk I/O)
```c
if (checking_builtins) {
    return next_builtin_match();
}
// Only scan PATH if no builtin matches
return scan_path_directories();
```

**Why this matters**: If you type `ec<TAB>`, the shell returns "echo" immediately without ever touching the filesystem. Only if there's no builtin match does it scan PATH.

**Optimization**: During PATH scanning, we build the complete list once and cache it. Subsequent matches reuse the cached list until completion finishes.

### 7. Memory Management Strategy

**Problem**: Complex data structures with nested allocations - how do you prevent memory leaks?

**Solution**: Clear ownership rules with comprehensive cleanup.

**Rules**:
1. Parser allocates tokens with `strdup()` - caller must free
2. Pipeline splitter creates structure arrays - main loop frees based on `num_commands`
3. Each allocation has exactly one corresponding free

**Pattern**:
```c
// Allocation
char **argv = malloc(n * sizeof(char *));
for (i = 0; i < n; i++) {
    argv[i] = strdup(token);  // Each token allocated
}

// Cleanup
for (i = 0; i < n; i++) {
    free(argv[i]);  // Free each token
}
free(argv);  // Free array
```

**Testing**: Verified zero leaks using valgrind during development.

### 8. Quote and Escape Handling

**Design**: Different quote types have different rules.

- **Single quotes** (`'...'`): Everything is literal, no escapes processed
- **Double quotes** (`"..."`): Process escape sequences for `\"`, `\\`, `\$`, `` \` ``
- **No quotes**: Backslash escapes the next character

**Example**:
```bash
echo 'hello\nworld'  → prints: hello\nworld (literal)
echo "hello\nworld"  → prints: hello\nworld (literal, but \ processed)
echo hello\ world    → prints: hello world (space escaped)
```

This matches POSIX shell behavior and allows users to control exactly how their input is interpreted.

## Performance Characteristics

### Time Complexity
- **Command execution**: O(1) for builtins (table lookup), O(P) for PATH lookup (P = number of PATH directories)
- **Pipeline setup**: O(N) where N = number of commands
- **History operations**: O(H) where H = history length
- **Tab completion**: O(B + E) where B = builtins, E = executables in PATH

### Space Complexity
- **Command storage**: O(T) where T = total tokens in command line
- **Pipeline**: O(N×T) for N commands with T average tokens each
- **History**: O(H×L) where H = entries, L = average command length

### Optimizations
1. **Lazy PATH scanning**: Only scan when needed for tab completion
2. **Immediate pipe cleanup**: Close unused file descriptors ASAP
3. **Single-pass parsing**: Tokenize and analyze in one pass
4. **Cached completions**: Build executable list once per completion session
5. **Function pointer dispatch**: Direct function calls instead of string comparison chains

## Technical Implementation

### Process Flow
```
1. Read user input (readline library)
2. Add to history
3. Parse into tokens
4. Detect pipes and split into commands
5. Execute:
   - Builtin: lookup in dispatch table → call function pointer
   - External: fork and exec
   - Pipeline: create pipes, fork N times, connect appropriately
6. Wait for all child processes
7. Clean up memory
```

### Key System Calls
- `fork()`: Create new process
- `pipe()`: Create IPC channel between processes
- `dup2()`: Redirect file descriptors
- `execv()`: Replace process with new program
- `wait()`: Wait for child process completion

### Libraries Used
- **readline**: Provides line editing, history, and tab completion framework
- **POSIX standard library**: Process and file management APIs

## Build Instructions
```bash
# Compile
gcc -o shell main.c -lreadline

# Run
./shell

# Run with persistent history
HISTFILE=~/.my_history ./shell
```

## Testing Coverage

Tested scenarios include:
- ✅ Single commands and multi-stage pipelines
- ✅ Mixed builtin/external commands in pipes
- ✅ All I/O redirection operators
- ✅ Quote handling and escape sequences
- ✅ History persistence and append operations
- ✅ Tab completion for builtins and PATH executables
- ✅ Edge cases: empty input, missing files, invalid commands

## Limitations & Future Work

**Current Limitations**:
- No background job control (`command &`)
- No signal handling for Ctrl+C interruption
- No variable expansion (`$VAR`)
- No command substitution (`` `command` `` or `$(command)`)
- No wildcard globbing (`*.txt`)

**Potential Enhancements**:
- Job control system for background processes
- Signal handlers for graceful interrupt handling
- Environment variable expansion
- Subshell and command substitution
- Glob pattern matching
- Scripting support (read and execute files)
- Hash table for builtin lookup (if many more builtins are added)

## Lessons

This project deepened my understanding of:
- How shells manage processes and inter-process communication
- The relationship between file descriptors, pipes, and process isolation
- Why forking is essential for process management
- Memory management in complex C programs
- The design decisions that make interactive tools responsive
- How function pointers enable clean, extensible architectures

## Acknowledgments

Built as a systems programming learning project. Inspired by the architecture of bash and zsh, implemented from first principles to understand the underlying mechanisms.

---

**Lines of Code**: ~1200 🚀
