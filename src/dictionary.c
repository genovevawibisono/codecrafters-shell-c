#include "dictionary.h"

// Static function headers
static int dictionary_expand(dictionary_t *dictionary);
static unsigned int hash(pid_t job_pid, unsigned int max_capacity);
static void dictionary_maintain(dictionary_t *dictionary);

dictionary_t *dictionary = NULL;

dictionary_t *dictionary_new(void) {
    dictionary_t *new = malloc(sizeof(dictionary_t));
    if (new == NULL) {
        fprintf(stderr, "[dictionary new] failed to malloc to create dictionary\n");
        return NULL;
    }

    new->items = calloc(MAX_CAPACITY, sizeof(job_t *));
    if (new->items == NULL) {
        fprintf(stderr, "[dictionary new] failed to calloc for items\n");
        free(new);
        return NULL;
    }
    new->current_capacity = 0;
    new->max_capacity = MAX_CAPACITY;

    return new;
}

int dictionary_add(dictionary_t *dictionary, job_t *job) {
    if (job == NULL) {
        fprintf(stderr, "[dictionary add] job is NULL\n");
        return -1;
    }

    int err;
    // check if dictionary is almost full, realloc if needed
    float load = (float)dictionary->current_capacity / dictionary->max_capacity;
    if (load > 0.75) {
        err = dictionary_expand(dictionary);
        if (err == -1) {
            fprintf(stderr, "[dictionary add] failed to expand dictionary\n");
            return -1;
        }
    }

    dictionary_maintain(dictionary);

    unsigned int index = hash(job->pid, dictionary->max_capacity);
    job->next = dictionary->items[index];
    dictionary->items[index] = job;

    dictionary->current_capacity++;

    return 0;
}

static int dictionary_expand(dictionary_t *dictionary) {
    if (dictionary == NULL) {
        fprintf(stderr, "[dictionary expand] dictionary is NULL\n");
        return -1;
    }

    unsigned int new_capacity = dictionary->max_capacity * 2;
    job_t **new_items = calloc(new_capacity, sizeof(job_t *));
    if (new_items == NULL) {
        fprintf(stderr, "[dictionary expand] failed to calloc for new items\n");
    }

    for (unsigned int i = 0; i < dictionary->max_capacity; i++) {
        job_t *node = dictionary->items[i];
        while (node != NULL) {
            job_t *next_node = node->next;
            unsigned int hash_index = hash(node->pid, new_capacity);
            node->next = new_items[hash_index];
            new_items[hash_index] = node;

            node = next_node;
        }
    }

    free(dictionary->items);

    dictionary->items = new_items;
    dictionary->max_capacity = new_capacity;

    return 0;
}

static unsigned int hash(pid_t job_pid, unsigned int max_capacity) {
    return (unsigned int)job_pid % max_capacity;
}

int dictionary_remove(dictionary_t *dictionary, pid_t job_pid) {
    if (dictionary == NULL) {
        fprintf(stderr, "[dictionary remove] dictionary is NULL\n");
        return -1;
    }

    if (job_pid < 0) {
        fprintf(stderr, "[dictionary remove] invalid job pid\n");
        return -1;
    }

    unsigned int index = hash(job_pid, dictionary->max_capacity);
    job_t *node = dictionary->items[index];
    if (node == NULL) {
        fprintf(stderr, "[dictionary remove] node is NULL\n");
        return -1;
    }

    // removal at the beginning of the linked list
    if (node->pid == job_pid) {
        dictionary->items[index] = node->next;
        free(node->cmd);
        free(node);
        return 0;
    }

    job_t *prev;
    job_t *temp = node;
    while (temp->next != NULL && temp->pid != job_pid) {
        job_t *prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        fprintf(stderr, "[dictionary remove] job with associated pid doesn\'t exist\n");
        return -1;
    }

    prev->next = temp->next;
    free(temp->cmd);
    free(temp);

    return 0;
}

void dictionary_display(dictionary_t *dictionary) {
    for (unsigned int i = 0; i < dictionary->max_capacity; i++) {
        fprintf(stdout, "======================================\n");
        fprintf(stdout, "[dictionary display] bucket %d\n", i);

        job_t *curr = dictionary->items[i];
        while (curr != NULL) {
            // print curr job
            job_display(curr);
            curr = curr->next;
        }
    }
}

static void dictionary_maintain(dictionary_t *dictionary) {
    for (int i = 0; i < dictionary->max_capacity; i++) {
        job_t *curr = dictionary->items[i];
        while (curr != NULL) {
            if (curr->most_recent == true) {
                curr->second_most_recent = true;
            } else {
                curr->second_most_recent = false;
            }
            curr->most_recent = false;
            curr = curr->next;
        }
    } 
}

static int compare_job_id(const void *a, const void *b) {
    job_t *ja = *(job_t **)a;
    job_t *jb = *(job_t **)b;
    return (int)ja->job_id - (int)jb->job_id;
}

void dictionary_jobs(dictionary_t *dictionary) {
    // collect all jobs into a flat array so we can sort by job_id
    job_t **jobs = malloc(dictionary->current_capacity * sizeof(job_t *));
    if (!jobs) return;
    int count = 0;

    for (int i = 0; i < (int)dictionary->max_capacity; i++) {
        job_t *curr = dictionary->items[i];
        while (curr != NULL) {
            jobs[count++] = curr;
            curr = curr->next;
        }
    }

    qsort(jobs, count, sizeof(job_t *), compare_job_id);

    // recompute markers based on surviving jobs ordered by job_id
    for (int i = 0; i < count; i++) {
        jobs[i]->most_recent = false;
        jobs[i]->second_most_recent = false;
    }
    if (count >= 1) jobs[count - 1]->most_recent = true;
    if (count >= 2) jobs[count - 2]->second_most_recent = true;

    for (int i = 0; i < count; i++) {
        job_display(jobs[i]);
    }

    free(jobs);

    // remove done jobs
    for (int i = 0; i < (int)dictionary->max_capacity; i++) {
        job_t *prev = NULL;
        job_t *curr = dictionary->items[i];
        while (curr != NULL) {
            job_t *next = curr->next;
            if (!curr->is_running) {
                if (prev == NULL) {
                    dictionary->items[i] = next;
                } else {
                    prev->next = next;
                }
                free(curr->cmd);
                free(curr);
                dictionary->current_capacity--;
            } else {
                prev = curr;
            }
            curr = next;
        }
    }
}
