#ifndef JOBS_H
#define JOBS_H

#include "structs.h"

#define MAX_JOBS 256

typedef struct job {
    pid_t pid;
    unsigned int job_id;
    struct command_context *ctx;
    bool is_running;

    // for dictionary separate chaining
    struct job *next;
} job_t;

extern int job_count;

job_t *job_new(struct command_context *ctx);
int job_add(pid_t pid, struct command_context *ctx);
int job_remove(pid_t pid);
void job_display(job_t *job);

#endif