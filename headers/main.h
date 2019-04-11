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

#include <time.h>

#include <windows.h>
#include <process.h>
#include <conio.h>
#include <math.h>
#include <memory.h>


#include "WD-dask64.h"
#include "fftw3.h"

#include "math_functions.h"
#include "file_handling.h"


double GetCounter();
void StartCounter();
void *procesa_FFT_banda(void *n);
void AI_CallBack();
void adquirir(short nCh, int qFreqAdq, int rangoDinCh1, int rangoDinCh2,
	      int bins, int nShotsChk, int nSubChk, int delay);
void *raw_data_writer(void *n);
void *procesa_osc(void *n);
void ts_fill(SYSTEMTIME ts, unsigned short *ts_pipe);
void *procesa_STD(void *n);
void *procesa_Monitoreo(void *n);
void *procesa_FFT(void *n);
