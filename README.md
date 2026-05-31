# Building a POSIX Shell in C

[![progress-banner](https://backend.codecrafters.io/progress/shell/7caf40a5-e333-46de-a753-d730edff8c51)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)

A from-scratch POSIX-compliant shell written in C, built as part of the [CodeCrafters "Build Your Own Shell"](https://app.codecrafters.io/courses/shell/overview) challenge. The project goes well beyond a toy implementation — it covers process management, job control, programmatic tab completion, parameter expansion, and a vtable-driven command dispatch architecture.

---

## Architecture

### Vtable-Driven Builtin Dispatch

Builtin commands are registered in a static dispatch table — a pattern borrowed directly from kernel and firmware design:

```c
struct command {
    const char *name;
    command_function func;   // typedef void (*command_function)(struct command_context *)
};

struct command commands[] = {
    { "exit",     shell_exit     },
    { "echo",     shell_echo     },
    { "cd",       shell_cd       },
    { "pwd",      shell_pwd      },
    { "jobs",     shell_jobs     },
    { "complete", shell_complete },
    { "declare",  shell_declare  },
    // ...
};
```

The main loop performs a linear scan over this table and invokes the matching function pointer — the same pattern used by Linux's `file_operations` struct and VFS layer. Adding a new builtin requires only a new row in the table and its implementation; no branching logic elsewhere changes.

### Command Context Passing

All parsed state is carried through a single `command_context` struct passed by pointer:

```c
struct command_context {
    char         *command_name;
    int           argc;
    char        **argv;
    bool          redirect;
    char         *out_file;
    int           out_mode;       // O_TRUNC or O_APPEND
    bool          redirect_err;
    char         *error_file;
    int           err_mode;
    int           num_commands;   // > 0 for pipelines
    char       ***all_commands;
    int          *all_argc;
    char        **all_command_names;
    int           background_job;
};
```

This mirrors the kernel's approach of threading a context object through a call chain rather than relying on global state or per-call argument explosion.

---

## Process Management & Job Control

### fork/exec Model

External commands follow the standard Unix process creation model: `fork(2)` duplicates the process, the child sets up I/O redirections via `dup2(2)`, then replaces itself with `execv(2)`. The parent either blocks with `waitpid(2)` for foreground jobs or returns immediately for background jobs.

### Background Jobs & Zombie Reaping

Background jobs (`cmd &`) are tracked in a hash table keyed by PID. The shell uses `waitpid(pid, &status, WNOHANG)` with `WIFEXITED` to non-blockingly poll for process completion:

- Before each prompt, `dictionary_reap` scans all tracked jobs for exits and prints `Done` notifications — the same deferred-reap pattern used by real shells to avoid blocking the REPL on child state changes.
- The `jobs` builtin displays live status, recomputes `+`/`-` recency markers from the current job table, and removes completed entries in the same pass.

### Job Number Recycling

Completed job IDs are enqueued into a FIFO free-list (`job_number_list`). New background jobs consume recycled IDs before allocating new sequential ones — matching bash's behavior where `[1]` reappears after job 1 completes.

---

## Data Structures

### Hash Table for Job Tracking

The job table is a separate-chaining hash table (`dictionary_t`) with a load-factor-based resize policy (threshold: 0.75). The hash function is `pid % capacity`. On resize, all entries are rehashed into a new allocation — the same open-addressing pattern used in kernel radix trees and inode caches.

### Linked-List Stores

Shell variables (`var_list`), completion specs (`complete_list`), and recycled job IDs (`job_number_list`) are all singly-linked lists with upsert semantics — insert at head for O(1) prepend, linear scan for lookup. Appropriate for small, infrequently-accessed tables where cache locality of a flat array isn't needed.

---

## I/O Redirection & Pipelines

Redirections (`>`, `>>`, `2>`, `2>>`) are parsed into the command context and applied in the child process via `open(2)` + `dup2(2)` before `execv`. Pipelines of arbitrary depth are constructed with `pipe(2)` and a loop over child processes — each stage's stdout wired to the next stage's stdin.

---

## Programmatic Tab Completion

The shell integrates with GNU Readline to support completion specs registered via `complete -C script cmd`. On TAB:

1. The command name is looked up in the completion spec table.
2. If a `-C` spec exists, the shell forks and runs the registered script via `popen`, passing `COMP_LINE` and `COMP_POINT` as environment variables (matching bash's completion protocol).
3. The script's stdout lines become the candidate list fed to `rl_completion_matches`.
4. Readline handles bell-on-ambiguity and double-TAB candidate display natively.

---

## Parameter Expansion

Shell variables declared with `declare NAME=VALUE` are expanded during tokenization in the parser. The tokenizer recognizes both `$VAR` and `${VAR}` forms outside single quotes, performing lookup and inline substitution before the token is finalized — matching the POSIX-specified expansion order.

---

## Building

```sh
cmake -S . -B build && cmake --build build
./your_program.sh
```

Requires `readline` (`brew install readline` on macOS).
