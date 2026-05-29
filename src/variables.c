#include "variables.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

var_node_t *var_list = NULL;

void var_set(const char *name, const char *value) {
    // update existing entry
    var_node_t *curr = var_list;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = strdup(value);
            return;
        }
        curr = curr->next;
    }

    // insert new
    var_node_t *node = malloc(sizeof(var_node_t));
    node->name  = strdup(name);
    node->value = strdup(value);
    node->next  = var_list;
    var_list    = node;
}

const char *var_get(const char *name) {
    var_node_t *curr = var_list;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) return curr->value;
        curr = curr->next;
    }
    return NULL;
}
