#include <file_handling.h>

void read_config(char *filename, struct program_config *config) {
	/// CARGO PARAMETROS DE ADQUISICION ARCHIVO config.ini
	FILE *file_config = fopen(filename, "r"); /* should check the result */

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

	while (fscanf(file_config, "%200s %200s", line, target) != EOF) {

		parse_th_config(config->th_data, line, target);

		if (strstr(line, "NSubChk:"))
			config->nSubChk = atoi(target);

		for (int i = 0; i < config->nro_pozos; i++) {
			snprintf(line_pozos, sizeof(line_pozos),
				 "Pozo%d:", i + 1);
			if (strstr(line, line_pozos))
				config->th_data->name_pozos[i] = target;
		}

		if (strstr(line, "CalculaFFT:"))
			if (strstr(target, "si"))
				config->calcula_fft = true;
		if (strstr(line, "CalculaOSC:"))
			if (strstr(target, "si"))
				config->calcula_osc = true;
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

void copy_file(char *source, char *dest){
	FILE *src_fd;
	FILE *dst_fd;
	src_fd = fopen(source,"r");
	dst_fd = fopen(dest, "w");
	char chr = fgetc(src_fd);
	while (chr != EOF){
		fputc(chr, dst_fd);
		chr = fgetc(src_fd);
	}
	fclose(src_fd);
	fclose(dst_fd);
}

void write_raw_hdr(char *filename, struct th_Data *data, int channel){
		FILE *out_fd = fopen(filename, "ab");
		fprintf(out_fd, "Placa: '%s' \n", data->placa);
		fprintf(out_fd, "Dato: '%s' \n", "RAW DATA");
		fprintf(out_fd, "DataType: '%s' \n", "short");
		fprintf(out_fd, "PythonNpDataType: '%s' \n",
			"np.int16");
		fprintf(out_fd, "BytesPerDatum: [%d] \n", 2);
		fprintf(out_fd, "Bins: [%d] \n", data->bins);
		fprintf(out_fd, "Delay: [%d] \n", data->delay);
		fprintf(out_fd, "Bins_raw: [%d] \n", data->bins_raw);
		fprintf(out_fd, "Delay_raw: [%d] \n", data->delay_raw);
		fprintf(out_fd, "Cols: [%d] \n", data->bins_raw);
		fprintf(out_fd, "NumberOfChannels: [%d] \n", data->nCh);
		fprintf(out_fd, "NDatum: [%d] \n", 1);
		if (channel == -1)
			fprintf(out_fd, "Channel: [%d] \n", channel);
		fprintf(out_fd, "FreqRatio: [%d] \n", data->qFreq);
		fprintf(out_fd, "nShotsChk: [%d] \n", data->nShotsChk);
		fprintf(out_fd, "nShots: [%d] \n", data->nShots);
		fprintf(out_fd, "Fin header \n");
		fclose(out_fd);
}