#ifndef DECLARE_H
#define DECLARE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct declare {
    char *name;
    char *flag;
    struct declare *next;
} declare_t;

#endif