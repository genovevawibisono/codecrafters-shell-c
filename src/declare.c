#include "declare.h"

declare_t *declare_list = NULL;

int declare_new(const char *name, const char *value) {
    if (name == NULL) {
        fprintf(stderr, "[declare new] name is NULL\n");
        return -1;
    }

    declare_t *new = malloc(sizeof(declare_t));
    if (new == NULL) {
        fprintf(stderr, "[declare new] failed to malloc for new declare\n");
        return -1;
    }

    new->name = strdup(name);
    new->value = value;
    new->next = declare_list;
    declare_list = new;

    return 0;
}

bool declare_search(const char *name) {
    if (declare_list == NULL) {
        fprintf(stderr, "[declare search] list is NULL\n");
        return -1;
    }

    if (name == NULL) {
        fprintf(stderr, "[declare search] name is NULL\n");
        return -1;
    }

    declare_t *node = declare_list;
    while (node != NULL) {
        if (strcmp(node->name, name) == 0) {
            return true;
        }

        node = node->next;
    }

    return false;
}

