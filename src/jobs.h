#ifndef JOBS_H
#define JOBS_H

#include "structs.h"

#define MAX_JOBS 256

typedef struct job {
    pid_t pid;
    unsigned int job_id;
    char *cmd;
    bool is_running;
    bool most_recent;
    bool second_most_recent;

    // for dictionary separate chaining
    struct job *next;
} job_t;

extern int job_count;

job_t *job_new(struct command_context *ctx);
int job_add(pid_t pid, struct command_context *ctx);
int job_remove(pid_t pid);
void job_display(job_t *job);

#endif