#ifndef HISTORY_H
#define HISTORY_H

#include "structs.h"
#include <readline/history.h>

static int last_history_written = 0;

void load_history_histfile(void);
void write_history_histfile(void);

#endif