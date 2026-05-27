#include "complete.h"
#include <string.h>

complete_node_t *complete_list = NULL;

static int complete_node_new(char *command, char *flag, char *value) {
    if (command == NULL) {
        fprintf(stderr, "[complete node new] command is NULL\n");
        return -1;
    }

    if (flag == NULL) {
        fprintf(stderr, "[complete node new] no flags\n");
        return -1;
    }

    if (value == NULL) {
        fprintf(stderr, "[complete node new] value is NULL\n");
        return -1;
    }

    complete_node_t *node = malloc(sizeof(complete_node_t));
    if (node == NULL) {
        fprintf(stderr, "[complete node new] failed to malloc\n");
        return -1;
    }

    node->command = strdup(command);
    node->flag = strdup(flag);
    node->value = strdup(value);
    node->next = complete_list;
    complete_list = node;

    return 0;
}

int complete_add(char *flag, char *value, char *command) {
    // update existing entry if command already has a spec
    complete_node_t *curr = complete_list;
    while (curr != NULL) {
        if (strcmp(curr->command, command) == 0) {
            free(curr->flag);
            free(curr->value);
            curr->flag = strdup(flag);
            curr->value = strdup(value);
            return 0;
        }
        curr = curr->next;
    }
    return complete_node_new(command, flag, value);
}

int complete_print(const char *command) {
    int found = 0;
    complete_node_t *curr = complete_list;
    while (curr != NULL) {
        if (command == NULL || strcmp(curr->command, command) == 0) {
            fprintf(stdout, "complete %s '%s' %s\n", curr->flag, curr->value, curr->command);
            found = 1;
        }
        curr = curr->next;
    }
    return found;
}
