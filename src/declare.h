#ifndef DECLARE_H
#define DECLARE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct declare {
    char *name;
    char *value;
    char *flag;
    struct declare *next;
} declare_t;

extern declare_t *declare_list;

int declare_new(const char *name, const char *value);
bool declare_search(const char *name);

#endif