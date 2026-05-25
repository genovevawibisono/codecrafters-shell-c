#include "jobs.h"
#include "dictionary.h"
#include "utilities.h"

static unsigned int job_id_counter = 0;
int job_count = 0;

job_t *job_new(struct command_context *ctx) {
    char *exe = resolve_executable(ctx->command_name);
    if (!exe) {
        fprintf(stderr, "[job new] command not found: %s\n", ctx->command_name);
        return NULL;
    }

    job_t *new = malloc(sizeof(job_t));
    if (new == NULL) {
        fprintf(stderr, "[job new] failed to malloc for a new job\n");
        free(exe);
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "[job new] failed to fork\n");
        free(exe);
        free(new);
        return NULL;
    }

    if (pid == 0) {
        execv(exe, ctx->argv);
        exit(1);
    }

    free(exe);

    new->pid = pid;
    new->job_id = ++job_id_counter;
    new->ctx = ctx;
    new->is_running = true;
    new->next = NULL;

    job_count++;

    fprintf(stdout, "[%d] %d\n", new->job_id, new->pid);

    return new;
}

int job_add(pid_t pid, struct command_context *ctx) {
    if (ctx == NULL) {
        fprintf(stderr, "[job add] command context is NULL\n");
        return -1;
    }

    job_t *new_job = job_new(ctx);
    if (new_job == NULL) {
        fprintf(stderr, "[job add] failed to malloc for new job\n");
        return -1;
    }

    // add job to data structure
    int res = dictionary_add(dictionary, new_job);
    if (res == -1) {
        fprintf(stderr, "[job add] failed to add job to dictionary\n");
        return -1;
    }

    return 0;
}

int job_remove(pid_t pid) {
    return dictionary_remove(dictionary, pid);
}

void job_display(job_t *job) {
    // just printing out the pid first in the mean time
    // refine later
    fprintf(stdout, "[%d] %d\n", job->job_id, job->pid);
}
