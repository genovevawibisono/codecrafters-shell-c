#ifndef COMPLETE_H
#define COMPLETE_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct complete_node {
    char *command;
    char *flag;
    char *value;
    struct complete_node *next;
} complete_node_t;

int complete_add(char *flag, char *value, char *command);
int complete_print(const char *command);

extern complete_node_t *complete_list;

#endif