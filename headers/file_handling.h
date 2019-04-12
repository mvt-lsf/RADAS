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

#ifndef STDIO_H
#define STDIO_H
#include <stdio.h>
#endif

#include <string.h>

int write_config();

/*
* Escribe el raw header para los datos guardados.
* Si channel es -1 el campo no se escribe
*/
void write_raw_hdr(char *filename, struct th_Data *data, int channel);
void read_config(char *filename, struct program_config *config);
void parse_th_config(struct th_Data *data, char *line, char *target);
void copy_file(char *source, char *dest);