#ifndef BOOLS
#define BOOLS
#include <stdbool.h>
#endif

struct th_Data {
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