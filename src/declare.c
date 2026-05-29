#include "declare.h"

declare_t *declare_list = NULL;

static int declare_new(const char *name) {
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
    new->next = declare_list;
    declare_list = new;

    return 0;
}

