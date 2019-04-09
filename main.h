#include <stdlib.h>
#include <stdio.h>
#include <time.h>
//#include <windows.h>
//#include <process.h>
//#include <conio.h>
#include <math.h>
#include <memory.h>
#include <WD-dask64.h>
#include <fftw3.h>

struct th_Data
{
    int bins;
    int delay;
    int nShotsChk;
    int nCh;
    int qFreq;
    long long nShots;
    bool infinite_daq;

    // Para RAW
    int bins_raw;
    int delay_raw;

    // Para STD
    int window_time;
    int window_bin;
    int window_bin_mean;

    // Para STD
    int bin_mon_laser_i;
    int bin_mon_laser_f;
    int bin_mon_edfa_i;
    int bin_mon_edfa_f;
    int window_mon_time;

    // Para FFT
    int chunk_fft_salteo;
    int chunk_fft_promedio;

    char *placa;
    char *fname_dir;
    char *name_pozos[10];

    int chunks_save;
    bool rawsave_ch0;
    bool rawsave_ch1;
    bool stdsave;
    bool mon_laser_save;
    bool mon_edfa_save;

    // Para OSC
    int cant_curvas;
    int window_time_osc;

    float cLaser;
    float cEDFA;

};

struct th_Data parse_config(FILE *file);
double GetCounter();
void StartCounter();
float* moving1avg_full(float* vector,unsigned int vector_size ,unsigned int window, float* vector_avg);
void *procesa_FFT_banda(void *n);
float *moving1avg(float *vector, unsigned int vector_size, unsigned int window, float *vector_avg, unsigned int vector_avg_size);
void AI_CallBack();
void adquirir(short nCh, int qFreqAdq, int rangoDinCh1, int rangoDinCh2, int bins, int nShotsChk, int nSubChk, int delay);
void *raw_data_writer(void *n);
void *procesa_osc(void *n);
void ts_fill(SYSTEMTIME ts, unsigned short *ts_pipe);
void *procesa_STD(void *n);
void *procesa_Monitoreo(void *n);
void *procesa_FFT(void *n);
