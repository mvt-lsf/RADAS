#ifndef BOOLS
#define BOOLS
#include <stdbool.h>
#endif

#ifndef STRUCTS
#define STRUCTS
#include "structs.h"
#endif

#ifndef STDLIB_H
#define STDLIB_H
#include <stdlib.h>
#endif

#include <string.h>

int write_config();
struct th_Data read_config();
void parse_th_config(struct th_Data *data, char *line, char *target);