#include <file_handling.h>

void read_config(char *filename, struct program_config *config){
		/// CARGO PARAMETROS DE ADQUISICION ARCHIVO config.ini
	FILE *file_config =
		fopen(filename, "r"); /* should check the result */
	
	char line_pozos[200];
	config->th_data = malloc(sizeof(struct th_Data));
	config->calcula_fft = false;
	config->calcula_osc = false;
	config->nro_pozos = 0;
	config->chunksPorPozo;
	config->nSubChk;
	config->window_time_osc;
	config->cant_curvas = 0;

	config->th_data->infinite_daq = true;

	char *line = malloc(sizeof(char) * 200);
	char *target = malloc(sizeof(char) * 200);

	while (fscanf(file_config,"%200s %200s",line, target) != EOF) {

		parse_th_config(config->th_data, line, target);

		if (strstr(line, "NSubChk:")) {
			config->nSubChk = atoi(target);
		}
		for (int i = 0; i < config->nro_pozos; i++) {
			snprintf(line_pozos, sizeof(line_pozos),
				 "Pozo%d:", i + 1);
			if (strstr(line, line_pozos)) {
				config->th_data->name_pozos[i] = target;
			}
		}

		if (strstr(line, "CalculaFFT:")) {
			if (strstr(target, "si")) {
				config->calcula_fft = true;
			}
		}

		if (strstr(line, "CalculaOSC:")) {
			if (strstr(target, "si")) {
				config->calcula_osc = true;
			}
		}
		target = NULL;
	}
	fclose(file_config);
}

void parse_th_config(struct th_Data *data, char *line, char *target){
	if (strstr(line, "Placa:"))
		data->placa = target;
	if (strstr(line, "Bins:"))
		data->bins = atoi(target);
	if (strstr(line, "Delay:"))
		data->delay = atoi(target);
	if (strstr(line, "Bins_raw:"))
		data->bins_raw = atoi(target);
	if (strstr(line, "Delay_raw:"))
		data->delay_raw = atoi(target);
	if (strstr(line, "ChunksSave:"))
		data->chunks_save = atoi(target);
	if (strstr(line, "NCh:"))
		data->nCh = atoi(target);
	if (strstr(line, "NShotsChk:"))
		data->nShotsChk = atoi(target);
	if (strstr(line, "WindowTime:"))
		data->window_time = atoi(target);
	if (strstr(line, "WindowBin:"))
		data->window_bin = atoi(target);
	if (strstr(line, "WindowBinMean:"))
		data->window_bin_mean = atoi(target);
	if (strstr(line, "Bin_mon_laser_i:"))
		data->bin_mon_laser_i = atoi(target);
	if (strstr(line, "Bin_mon_laser_f:"))
		data->bin_mon_laser_f = atoi(target);
	if (strstr(line, "Bin_mon_edfa_i:"))
		data->bin_mon_edfa_i = atoi(target);
	if (strstr(line, "Bin_mon_edfa_f:"))
		data->bin_mon_edfa_f = atoi(target);
	if (strstr(line, "ChunkSalteoFFT:"))
		data->chunk_fft_salteo = atoi(target);
	if (strstr(line, "ChunkPromedioFFT:"))
		data->chunk_fft_promedio = atoi(target);
	if (strstr(line, "nShots:"))
		data->nShots = atoi(target);
	if (strstr(line, "Calibracion_Laser:"))
		data->cLaser = atof(target);
	if (strstr(line, "Calibracion_EDFA:"))
		data->cEDFA = atof(target);
	if (strstr(line, "qFreq:"))
		data->qFreq = atoi(target);
	if (strstr(line, "GuardaRawDataCh0:"))
		if (strstr(target, "si"))
			data->rawsave_ch0 = true;
	if (strstr(line, "GuardaRawDataCh1:"))
		if (strstr(target, "si"))
			data->rawsave_ch1 = true;
	if (strstr(line, "GuardaSTDData:"))
		if (strstr(target, "si"))
			data->stdsave = true;
	if (strstr(line, "GuardaMonLaserData:"))
		if (strstr(target, "si"))
			data->mon_laser_save = true;
	if (strstr(line, "GuardaMonEdfaData:"))
		if (strstr(target, "si"))
			data->mon_edfa_save = true;
	if (strstr(line, "CantCurvasOsc:"))
		data->cant_curvas = atoi(target);
	if (strstr(line, "WindowTimeOsc:"))
		data->window_time_osc = atoi(target);
}