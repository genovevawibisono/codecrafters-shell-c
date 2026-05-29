#ifndef VARIABLES_H
#define VARIABLES_H

typedef struct var_node {
    char *name;
    char *value;
    struct var_node *next;
} var_node_t;

extern var_node_t *var_list;

void        var_set(const char *name, const char *value);
const char *var_get(const char *name);

#endif
