#ifndef main
#define main
#include <main.h>
#include <stddef.h>
#include <stdbool.h>
#endif

int write_config();
struct th_Data read_config();
void read_string(struct th_Data *data, char *line, char *target);