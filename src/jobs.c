#include "jobs.h"
#include "dictionary.h"
#include "utilities.h"

static unsigned int job_id_counter = 0;
int job_count = 0;
job_number_list_t *job_number_list = NULL;

static job_number_node_t *job_number_node_new(unsigned int job_id);
static int job_number_list_enqueue(unsigned int job_id);
static unsigned int job_number_list_peek(void);
static void job_number_list_remove_head(void);

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

    // build command string once, while ctx is still valid
    char cmd[MAX_COMMAND_LENGTH] = {0};
    for (int i = 0; i < ctx->argc; i++) {
        if (i > 0) strcat(cmd, " ");
        strcat(cmd, ctx->argv[i]);
    }
    new->pid = pid;
    
    if (job_number_list->head == NULL) {
        new->job_id = ++job_count;
    } else {
        new->job_id = job_number_list_peek();
        job_number_list_remove_head();
    }

    new->cmd = strdup(cmd);
    new->is_running = true;
    new->most_recent = true;
    new->second_most_recent = false;
    new->next = NULL;

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
    // check if process is still alive without blocking
    int wstatus;
    pid_t result = waitpid(job->pid, &wstatus, WNOHANG);
    if (result > 0 && WIFEXITED(wstatus)) {
        job->is_running = false;
    }

    char marker;
    if (job->most_recent) marker = '+';
    else if (job->second_most_recent) marker = '-';
    else marker = ' ';

    if (job->is_running) {
        fprintf(stdout, "[%d]%c  %-24s%s &\n", job->job_id, marker, "Running", job->cmd);
    } else {
        fprintf(stdout, "[%d]%c  %-24s%s\n", job->job_id, marker, "Done", job->cmd);
    }
}

static job_number_node_t *job_number_node_new(unsigned int job_id) {
    job_number_node_t *new = malloc(sizeof(job_number_node_t));
    if (new == NULL) {
        fprintf(stderr, "[job number node new] failed to malloc for job number node\n");
        return NULL;
    }

    new->job_id = job_id;
    new->next = NULL;

    return new;
}

int job_number_list_new(void) {
    job_number_list = malloc(sizeof(*job_number_list));
    if (job_number_list == NULL) {
        fprintf(stderr, "[job number list new] failed to create job number list\n");
        return -1;
    }

    job_number_list->head = NULL;
    job_number_list->size = 0;

    return 0;
}

static int job_number_list_enqueue(unsigned int job_id) {
    if (job_number_list == NULL) {
        fprintf(stderr, "[job number list enqueue] job number list does not exist yet\n");
        return -1;
    }

    job_number_node_t *node = job_number_node_new(job_id);
    if (node == NULL) {
        fprintf(stderr, "[job number list enqueue] failed to create a new job number node\n");
        return -1;
    }

    if (job_number_list->head == NULL) {
        job_number_list->head = node;
        job_number_list->size++;

        return 0;
    }

    job_number_node_t *curr = job_number_list->head;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = node;
    job_number_list->size++;

    return 0;
}

static unsigned int job_number_list_peek(void) {
    return job_number_list->head->job_id;
}

static void job_number_list_remove_head(void) {
    if (job_number_list->head == NULL) {
        fprintf(stderr, "[job number list remove head] list is empty\n");
        return;
    }

    job_number_node_t *tmp = job_number_list->head;
    job_number_list->head = job_number_list->head->next;
    free(tmp);

    return;
}

void job_id_recycle(unsigned int job_id) {
    job_number_list_enqueue(job_id);
}
