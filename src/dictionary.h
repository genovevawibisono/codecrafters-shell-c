#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "structs.h"
#include "jobs.h"

// data structure choice: dictionary with separate chaining

#define MAX_CAPACITY 256

typedef struct dictionary {
    job_t **items;
    unsigned int current_capacity;
    unsigned int max_capacity;
} dictionary_t;

extern dictionary_t *dictionary;

dictionary_t *dictionary_new(void);
int dictionary_add(dictionary_t *dictionary, job_t *job);
int dictionary_remove(dictionary_t *dictionary, pid_t job_pid);
void dictionary_display(dictionary_t *dictionary);
void dictionary_jobs(dictionary_t *dictionary);
void dictionary_reap(dictionary_t *dictionary);

#endif