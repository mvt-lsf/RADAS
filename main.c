/// Ch0: se�al APD
/// Ch1: se�al monitoreo. Laser+EDFA equipo DAS OLEO 1

#include <main.h>

// renombra algunos par�metros de la placa para usarlos en las funciones. los de
// la derecha significan algo para la placa

#define TIMEBASE WD_IntTimeBase
#define ADTRIGSRC WD_AI_TRGSRC_ExtD
#define ADTRIGMODE WD_AI_TRGMOD_DELAY
#define ADTRIGPOL WD_AI_TrgPositive
#define ADTRIGOUTWDT                                                           \
	WD_OutTrgPWidth_200ns // OTROS VALORES: 50ns 100ns 150ns 200ns 500ns
#define BUFAUTORESET 1

#define CARDNUM 0
#define CHUNKS 40
#define CONSUMIDORES 5

// typedef enum { false, true } bool;

double PCFreq = 0.0;
__int64 CounterStart = 0;

void StartCounter()
{
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		printf("QueryPerformanceFrequency failed!\n");

	PCFreq = ((double)li.QuadPart) / 1000.0;

	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}

double GetCounter()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return ((double)li.QuadPart - CounterStart) / PCFreq;
}

// en la estructura callback estamos juntamos todas las variables externas que
// utiliza la funci�n AI_CallBack
short *chkBuffer;
SYSTEMTIME *tssBuffer; // CAMBIO TIMESTAMP

bool salir = false;
bool salir_raw = false;
bool salir_std = false;
bool salir_monitoreo = false;
bool salir_fft = false;
bool salir_osc = false;

HANDLE pipeSTD;
HANDLE pipeMONITOREO;

struct {
	int nCh;
	int bins;
	int nShotsChk;
	int nSubChk;
	int cnt_SubChk;
	int chkActual;
	bool bufferActual;
	short *ai_buf;
	short *ai_buf2; // este es un puntero short porque los elementos del
			// buffer de la placa son shorts, cuando lo indexemos se
			// va a mover de a un short en memoria, de forma de ir
			// pasando de elemento a elemento.
	bool disponible[CHUNKS];
	bool ovr;
	HANDLE semConsumidor[CONSUMIDORES];
} callback;

void *procesa_FFT_banda(void *n)
{
	struct th_Data *data = (struct th_Data *)n;
	int nCh = data->nCh;
	int bins = data->bins;
	int qFreq = data->qFreq;
	int nShotsChk = data->nShotsChk;
	int chunk_fft_salteo = data->chunk_fft_salteo;
	int chunk_fft_promedio = data->chunk_fft_promedio;
	int window_bin_mean = data->window_bin_mean;
	int bin_mon_edfa_i = data->bin_mon_edfa_i;
	int bin_mon_edfa_f = data->bin_mon_edfa_f;

	// Buffer float para fft
	int n_t = 8;
	fftwf_init_threads();
	fftwf_plan_with_nthreads(
		n_t); /// COMPARAR CONTRA THREADS NORMALES, MKL, IPP, IPP_short

	/// Reservo memoria para salida y entrada del plan
	float *dst_float = malloc(sizeof(float) * bins * nShotsChk);
	fftwf_complex *out = fftwf_malloc(sizeof(fftwf_complex) * bins
					  * (1 + (nShotsChk / 2)));
	float *out_abs = (float *)malloc(sizeof(float) * bins
					 * (1 + (nShotsChk / 2))); // abs fft
	float *out_abs_acc =
		(float *)malloc(sizeof(float) * bins
				* (1 + (nShotsChk / 2))); // abs fft acumulada
	float *fft_0 = malloc(bins * sizeof(float));
	float *fft_0_mean = malloc(bins * sizeof(float));

	int istride = 1 * bins;
	int ostride = 1;
	int idist = 1;
	int odist = 1 + nShotsChk / 2;
	int nn = nShotsChk;
	fftwf_plan p2 = fftwf_plan_many_dft_r2c(
		1, &nn, bins, dst_float, NULL, istride, idist, out, NULL,
		ostride, odist, FFTW_MEASURE | FFTW_PRESERVE_INPUT);

	short *chunk_actual = chkBuffer;

	DWORD leido_paip;
	HANDLE paip = CreateNamedPipe(
		TEXT("\\\\.\\pipe\\PipeFFT"),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE, // FILE_FLAG_FIRST_PIPE_INSTANCE
					      // is not needed but forces
					      // CreateNamedPipe(..) to fail if
					      // the pipe already exists...
		PIPE_WAIT, 1, bins * sizeof(float) * (1 + (nShotsChk / 2)) * 5,
		bins * sizeof(float) * (1 + (nShotsChk / 2)) * 5,
		NMPWAIT_USE_DEFAULT_WAIT, NULL);

	char command[100];
	snprintf(command, 100, "start python pipe_fft_abs.py %d %d %d ", bins,
		 nShotsChk, qFreq);
	printf(command);
	system(command);

	int cnt_salteo = 0;
	int cnt_promedio = 0;
	int chunk_control = 0;
	int i, j;

	float *energia_edfa = malloc(nShotsChk * sizeof(float));
	memset(energia_edfa, 0, nShotsChk * sizeof(float));
	int delta_bin_edfa = bin_mon_edfa_f - bin_mon_edfa_i;
	int off_e_edfa = 0;

	while (salir == 0) { /// wait podr�a estar aca

		WaitForSingleObject(callback.semConsumidor[3], INFINITE);

		for (i = 0; i < nShotsChk; ++i) {
			off_e_edfa =
				2
				* (i * bins
				   + bin_mon_edfa_i); // para posicionar en el
						      // bin correspondiente del
						      // canal 1
			energia_edfa[i] =
				(float)(chunk_actual[off_e_edfa + 1] / 2)
				+ (float)(chunk_actual[off_e_edfa
						       + delta_bin_edfa * nCh
						       + 1]
					  / 2);
			for (j = 1; j < delta_bin_edfa - 1; ++j) {
				energia_edfa[i] +=
					(float)(chunk_actual[off_e_edfa
							     + j * nCh + 1]);
			}
		}

		/// Casteo sin ordenar: la fftw ordena los datos.
		for (i = 0; i < bins * nShotsChk * nCh; i += nCh) {
			dst_float[i / nCh] = (float)chunk_actual[i]
					     / energia_edfa[i / bins / nCh];
		}

		fftwf_execute(p2);

		/// Valor 0 de la fft
		for (i = 0; i < bins; ++i) {
			fft_0[i] = sqrt(
				out[i * (nShotsChk / 2 + 1)][0]
					* out[i * (nShotsChk / 2 + 1)][0]
				+ out[i * (nShotsChk / 2 + 1)][1]
					  * out[i * (nShotsChk / 2 + 1)][1]);
		}

		moving1avg_full(fft_0, bins, window_bin_mean, fft_0_mean);

		/// Calcula el valor absoluto, esto ya es usado por mas de una
		/// funcion, repeticion de codigo.
		for (i = 0; i < ((nShotsChk / 2) + 1) * bins; ++i) {
			out_abs[i] = sqrt(out[i][0] * out[i][0]
					  + out[i][1] * out[i][1])
				     / fft_0_mean[i / (nShotsChk / 2 + 1)]
				     * 2; /// paralelizar
			if (isnan(out_abs[i]))
				out_abs[i] = 0.0f;

			out_abs_acc[i] += out_abs[i];
		}

		WriteFile(paip, &out_abs[0],
			  sizeof(float) * bins * (1 + (nShotsChk / 2)),
			  &leido_paip, NULL);

		chunk_control++;
		if (chunk_control == CHUNKS) { // why??? deberia ser chunks??
			chunk_actual = chkBuffer;
			chunk_control = 0;
		} else {
			chunk_actual +=
				bins * nShotsChk * nCh; // avanzo 2 channels
		}
	}

	CloseHandle(paip);
	free(dst_float);
	free(out_abs);
	free(out_abs_acc);
	salir_fft = true;
	printf("Hilo FFT terminado \n");
	return NULL;
}

void AI_CallBack()
{

	short *bufferPlaca;

	if (callback.bufferActual) {
		bufferPlaca = callback.ai_buf;
	} else {
		bufferPlaca = callback.ai_buf2;
	}

	callback.cnt_SubChk++;

	int indice_chunk = callback.chkActual / callback.nSubChk;

	if (!callback.disponible[indice_chunk]) {
		callback.ovr = true;
		WD_AI_ContBufferReset(CARDNUM);
		WD_Buffer_Free(CARDNUM, callback.ai_buf);
		WD_Buffer_Free(CARDNUM, callback.ai_buf2);
		WD_Release_Card(CARDNUM);
	} else {

		if (callback.cnt_SubChk == callback.nSubChk) {
			callback.disponible[indice_chunk] = false;
		}
		memcpy(chkBuffer
			       + (long long)callback.bins * (callback.nCh)
					 * callback.chkActual
					 * callback.nShotsChk
					 / callback.nSubChk,
		       bufferPlaca,
		       (long long)callback.bins * callback.nCh
			       * callback.nShotsChk / callback.nSubChk
			       * sizeof(short)); /// copia 2 canales
	}

	DWORD semCount;
	if (callback.cnt_SubChk == callback.nSubChk) {
		ReleaseSemaphore(callback.semConsumidor[0], 1,
				 &semCount); /// semaforo para ESCRITURA
		ReleaseSemaphore(callback.semConsumidor[1], 1,
				 &semCount); /// semaforo para STD
		ReleaseSemaphore(callback.semConsumidor[2], 1,
				 &semCount); /// semaforo para MONITOREO
		ReleaseSemaphore(callback.semConsumidor[3], 1,
				 &semCount); /// semaforo para FFT
		ReleaseSemaphore(callback.semConsumidor[4], 1,
				 &semCount); /// semaforo para Osciloscopio
		GetLocalTime(&tssBuffer[indice_chunk]);
		callback.cnt_SubChk = 0;
		printf("Mande un chunk \n");
	}

	callback.chkActual =
		(callback.chkActual + 1) % (CHUNKS * callback.nSubChk);
	callback.bufferActual = !callback.bufferActual;
	return;
}


void adquirir(short nCh, int qFreqAdq, int rangoDinCh1, int rangoDinCh2,
	      int bins, int nShotsChk, int nSubChk, int delay)
{
	/* configura la placa pasando el n�mero de canales, el entero por el que
	se divide la freq de adquisici�n m�xima. qFreqAdq 1, 2,3,.......,2^16
	 rangoDin 0,1,2 equivale a +-0.2V +-2V y +-10V
	 Adem�s configura los buffers de la RAM a la que la placa transfire los
	datos aibuf1 y aibuf2. Configura triggres, bufferes de la placa.
	WD_AI_ContBufferSetup y WD_Buffer_Alloc inicia la adquisici�n
	*/

	// variables placa
	I16 Id, Id2,
		card; // son requeridas por funciones de la placa para almacenar
		      // cosas. son de uso exclusivo de las funciones de la
		      // placa
	U16 card_type = PCIe_9852;
	U16 card_num = CARDNUM; // arranca en 0, en esta compu hab�a dos placas
				// la 0 y la 1.

	if ((card = WD_Register_Card(card_type, card_num)) < 0) {
		printf("Register_Card error=%d", card);
		exit(1);
	}

	// WD_AI_AsyncDblBufferMode (card_num, TRUE);
	//----------------------------------
	// ai_buf y ai_buf2
	// configura los buffers de la ram donde se transfieren los datos de la
	// placa
	int puntosChk = nCh * bins * nShotsChk / nSubChk;
	int tamBuffer = puntosChk * sizeof(I16);
	short *ai_buf = WD_Buffer_Alloc(
		card,
		tamBuffer); // direcci�n de memoria del buffer de la ram
			    // utilizado por la placa para descargar los datos.
	// lo tiene que utilizar el callback que vaya a buscar los datos por lo
	// que tine que ser global. Hay un buffer en la ram por cada buffer en
	// la placa.
	if (!ai_buf) {
		printf("buffer1 allocation failed\n");
		exit(1);
	}
	short *ai_buf2 = WD_Buffer_Alloc(card, tamBuffer);
	if (!ai_buf2) {
		printf("buffer22 allocation failed\n");
		WD_Buffer_Free(card, ai_buf);
		exit(1);
	}
	//-----------------------------
	// configura buffer en la placa

	I16 err = WD_AI_ContBufferSetup(card, ai_buf, puntosChk, &Id);
	if (err != 0) {
		printf("P9842_AI_ContBufferSetup 0 error=%d", err);
		WD_Buffer_Free(card, ai_buf);
		WD_Buffer_Free(card, ai_buf2);
		WD_Release_Card(card);
		exit(1);
	}
	err = WD_AI_ContBufferSetup(card, ai_buf2, puntosChk, &Id2);
	if (err != 0) {
		printf("P9842_AI_ContBufferSetup 0 error=%d", err);
		WD_AI_ContBufferReset(card);
		WD_Buffer_Free(card, ai_buf);
		WD_Buffer_Free(card, ai_buf2);
		WD_Release_Card(card);
		exit(1);
	}

	// seteamos impedancia y rango din�mico de los canales
	U16 rango1, rango2;
	switch (rangoDinCh1) {
	case 0:
		rango1 = AD_B_0_2_V; //+-200mV
		break;
	case 1:
		rango1 = AD_B_2_V; //+-2V
		break;
	case 2:
		rango1 = AD_B_10_V; //+-10V
	}
	switch (rangoDinCh2) {
	case 0:
		rango2 = AD_B_0_2_V; //+-200mV
		break;
	case 1:
		rango2 = AD_B_2_V; //+-2V
		break;
	case 2:
		rango2 = AD_B_10_V; //+-10V
	}

	WD_AI_CH_ChangeParam(card, -1, AI_IMPEDANCE,
			     IMPEDANCE_50Ohm); /// card number,channel,channel
					       /// setting,setting value
	WD_AI_CH_ChangeParam(
		card, 0, AI_RANGE,
		rango1); /// card number,channel,setting,setting value
	WD_AI_CH_ChangeParam(card, 1, AI_RANGE, rango2);
	//----------------------------------------------------

	// ver en el manual de la placa esta funci�n
	err = WD_AI_Config(card, TIMEBASE, 1, WD_AI_ADCONVSRC_TimePacer, 0,
			   BUFAUTORESET);
	if (err != 0) {
		printf("WD_AI_Config error=%d", err);
		WD_Buffer_Free(card, ai_buf);
		WD_Buffer_Free(card, ai_buf2);
		WD_Release_Card(card);
		exit(1);
	}
	//------------------------------------------
	// configuraci�n del trigger

	err = WD_AI_Trig_Config(card, ADTRIGMODE, ADTRIGSRC, ADTRIGPOL, 0, 0.0,
				0, 0, delay, 0);
	if (err != 0) {
		printf("WD_AI_Trig_Config error=%d", err);
		WD_Buffer_Free(card, ai_buf);
		WD_Buffer_Free(card, ai_buf2);
		WD_Release_Card(card);
		exit(1);
	}
	err = WD_OutTrig_Config(card, 0, 0, ADTRIGOUTWDT);
	if (err != 0) {
		printf("WD_OutTrig_Config error=%d", err);

		exit(1);
	}
	//----------------------------------------------------
	// Configura el callBack. El evento es que se llen� el buffer de la
	// placa. TrigEvent har�a otra cosa seg�n el manual, pero con este
	// funciona y con que sugiere el manual no
	// el callback es el productor trae los datos del buffer de la ram
	// reservado para la placa a los chunks. Por eso recibe tambi�n los
	// sem�foros de los consumidores
	callback.bins = bins;
	callback.nCh = nCh;
	callback.nShotsChk = nShotsChk;
	callback.nSubChk = nSubChk;
	callback.cnt_SubChk = 0;
	callback.ai_buf = ai_buf;
	callback.ai_buf2 = ai_buf2;
	callback.bufferActual = true;
	callback.chkActual = 0;

	int i;
	for (i = 0; i < CHUNKS; ++i)
		callback.disponible[i] = true;
	//----------------
	// reserva memoria para  los chunks: short* chkBuffer

	// creamos sem�foros
	callback.semConsumidor[0] = CreateSemaphore(NULL, 0, CHUNKS, NULL);
	callback.semConsumidor[1] = CreateSemaphore(NULL, 0, CHUNKS, NULL);
	callback.semConsumidor[2] = CreateSemaphore(NULL, 0, CHUNKS, NULL);
	callback.semConsumidor[3] = CreateSemaphore(NULL, 0, CHUNKS, NULL);
	callback.semConsumidor[4] = CreateSemaphore(NULL, 0, CHUNKS, NULL);

	err = WD_AI_EventCallBack_x64(card, 1, TrigEvent, (U32)AI_CallBack);
	if (err != 0) {
		printf("WD_AI_EventCallBack error=%d", err);
		WD_AI_ContBufferReset(card);
		WD_Buffer_Free(card, ai_buf);
		WD_Buffer_Free(card, ai_buf2);
		WD_Release_Card(card);
		exit(1);
	}

	//--------------------------------------------
	// start adquisition
	err = WD_AI_ContScanChannels(card, nCh - 1, 0, bins, qFreqAdq, qFreqAdq,
				     ASYNCH_OP); // aca puede ser read por scan

	if (err != 0) {
		printf("AI_ContScanChannels error=%d", err);
		WD_AI_ContBufferReset(card);
		WD_Buffer_Free(card, ai_buf);
		WD_Buffer_Free(card, ai_buf2);
		WD_Release_Card(card);
		exit(1);
	}
}

void *raw_data_writer(void *n)
{
	struct th_Data *data = (struct th_Data *)n;
	int bins = data->bins;
	int delay = data->delay;
	int bins_raw = data->bins_raw;
	int delay_raw = data->delay_raw;
	int chunks_save = data->chunks_save;
	int nShotsChk = data->nShotsChk;
	int nCh = data->nCh;
	int qFreq = data->qFreq;
	long long nShots = data->nShots;
	bool infinite_daq = data->infinite_daq;
	char *fname_dir = data->fname_dir;
	char *placa = data->placa;
	bool rawsave_ch0 = data->rawsave_ch0;
	bool rawsave_ch1 = data->rawsave_ch1;
	bool rawsave = false;
	int nCh_copia = 0;

	int caso = 0;
	unsigned long long bin_copia = 0; /// bines que copia por chunk
	if (rawsave_ch0) {
		caso = 1;
		rawsave = true;
		// bin_copia += bins*nShotsChk;
		bin_copia += bins_raw * nShotsChk;
		nCh_copia++;
	}
	if (rawsave_ch1) {
		caso = caso + 2;
		rawsave = true;
		// bin_copia += bins*nShotsChk;
		bin_copia += bins_raw * nShotsChk;
		nCh_copia++;
	}

	if (nCh == 1 && rawsave_ch0) {
		// bin_copia = bins*nShotsChk;
		bin_copia = bins_raw * nShotsChk;
		rawsave = true;
		caso = 3;
		nCh_copia == 1;
	}

	if (nCh == 1 && rawsave_ch1) {
		caso = 0;
		rawsave = false;
		printf("\n No se guarda Raw Data. Con un canal solo se adquiere el canal Ch0. \n");
	}

	printf("caso: %d //", caso);
	short *ch = malloc(nCh_copia * bins_raw * nShotsChk * sizeof(short));

	//    if (nCh == 1 && rawsave_ch1){
	//		caso = 0;
	//		printf("No se guarda Raw Data. Con un canal solo se
	// adquiere el canal Ch0. \n");
	//    }

	char dire_ch0[70];
	char dire_ch0_hdr[70];

	/// Escribe el archivo y header
	DWORD bytesw = 0;
	if (caso == 1) {
		strcpy(dire_ch0, fname_dir);
		strcat(dire_ch0, "/RAW_CH0");
		mkdir(dire_ch0);
		strcpy(dire_ch0_hdr, dire_ch0);
		strcat(dire_ch0_hdr, "/ch0.hdr");

		FILE *f_raw_data_out_ch0_h = fopen(dire_ch0_hdr, "ab");
		fprintf(f_raw_data_out_ch0_h, "Placa: '%s' \n", placa);
		fprintf(f_raw_data_out_ch0_h, "Dato: '%s' \n", "RAW DATA");
		fprintf(f_raw_data_out_ch0_h, "DataType: '%s' \n", "short");
		fprintf(f_raw_data_out_ch0_h, "PythonNpDataType: '%s' \n",
			"np.int16");
		fprintf(f_raw_data_out_ch0_h, "BytesPerDatum: [%d] \n", 2);
		fprintf(f_raw_data_out_ch0_h, "Bins: [%d] \n", bins);
		fprintf(f_raw_data_out_ch0_h, "Delay: [%d] \n", delay);
		fprintf(f_raw_data_out_ch0_h, "Bins_raw: [%d] \n", bins_raw);
		fprintf(f_raw_data_out_ch0_h, "Delay_raw: [%d] \n", delay_raw);
		fprintf(f_raw_data_out_ch0_h, "Cols: [%d] \n", bins_raw);
		fprintf(f_raw_data_out_ch0_h, "NumberOfChannels: [%d] \n", nCh);
		fprintf(f_raw_data_out_ch0_h, "NDatum: [%d] \n", 1);
		fprintf(f_raw_data_out_ch0_h, "Channel: [%d] \n", 0);
		fprintf(f_raw_data_out_ch0_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_raw_data_out_ch0_h, "nShotsChk: [%d] \n", nShotsChk);
		fprintf(f_raw_data_out_ch0_h, "nShots: [%d] \n", nShots);
		fprintf(f_raw_data_out_ch0_h, "Fin header \n");
		fclose(f_raw_data_out_ch0_h);
	}

	char dire_ch1[70];
	char dire_ch1_hdr[70];

	if (caso == 2) {
		strcpy(dire_ch1, fname_dir);
		strcat(dire_ch1, "/RAW_CH1");
		mkdir(dire_ch1);
		strcpy(dire_ch1_hdr, dire_ch1);
		strcat(dire_ch1_hdr, "/ch1.hdr");

		FILE *f_raw_data_out_ch1_h = fopen(dire_ch1_hdr, "ab");
		fprintf(f_raw_data_out_ch1_h, "Placa: '%s' \n", placa);
		fprintf(f_raw_data_out_ch1_h, "Dato: '%s' \n", "RAW DATA");
		fprintf(f_raw_data_out_ch1_h, "DataType: '%s' \n", "short");
		fprintf(f_raw_data_out_ch1_h, "PythonNpDataType: '%s' \n",
			"np.int16");
		fprintf(f_raw_data_out_ch1_h, "BytesPerDatum: [%d] \n", 2);
		fprintf(f_raw_data_out_ch1_h, "Bins: [%d] \n", bins);
		fprintf(f_raw_data_out_ch1_h, "Delay: [%d] \n", delay);
		fprintf(f_raw_data_out_ch1_h, "Bins_raw: [%d] \n", bins_raw);
		fprintf(f_raw_data_out_ch1_h, "Delay_raw: [%d] \n", delay_raw);
		fprintf(f_raw_data_out_ch1_h, "Cols: [%d] \n", bins_raw);
		fprintf(f_raw_data_out_ch1_h, "NumberOfChannels: [%d] \n", nCh);
		fprintf(f_raw_data_out_ch1_h, "NDatum: [%d] \n", 1);
		fprintf(f_raw_data_out_ch1_h, "Channel: [%d] \n", 1);
		fprintf(f_raw_data_out_ch1_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_raw_data_out_ch1_h, "nShotsChk: [%d] \n", nShotsChk);
		fprintf(f_raw_data_out_ch1_h, "nShots: [%d] \n", nShots);
		fprintf(f_raw_data_out_ch1_h, "Fin header \n");
		fclose(f_raw_data_out_ch1_h);
	}

	char dire_ch[70];
	char dire_ch_hdr[70];

	if (caso == 3) {
		strcpy(dire_ch, fname_dir);
		strcat(dire_ch, "/RAW_CH");
		mkdir(dire_ch);
		strcpy(dire_ch_hdr, dire_ch);
		strcat(dire_ch_hdr, "/ch.hdr");

		FILE *f_raw_data_out_h = fopen(dire_ch_hdr, "ab");
		fprintf(f_raw_data_out_h, "Placa: '%s' \n", placa);
		fprintf(f_raw_data_out_h, "Dato: '%s' \n", "RAW DATA");
		fprintf(f_raw_data_out_h, "DataType: '%s' \n", "short");
		fprintf(f_raw_data_out_h, "PythonNpDataType: '%s' \n",
			"np.int16");
		fprintf(f_raw_data_out_h, "BytesPerDatum: [%d] \n", 2);
		fprintf(f_raw_data_out_h, "Bins: [%d] \n", bins);
		fprintf(f_raw_data_out_h, "Delay: [%d] \n", delay);
		fprintf(f_raw_data_out_h, "Bins_raw: [%d] \n", bins_raw);
		fprintf(f_raw_data_out_h, "Delay_raw: [%d] \n", delay_raw);
		fprintf(f_raw_data_out_h, "Cols: [%d] \n", bins_raw);
		fprintf(f_raw_data_out_h, "NumberOfChannels: [%d] \n", nCh);
		fprintf(f_raw_data_out_h, "NDatum: [%d] \n", nCh);
		fprintf(f_raw_data_out_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_raw_data_out_h, "nShotsChk: [%d] \n", nShotsChk);
		fprintf(f_raw_data_out_h, "nShots: [%d] \n", nShots);
		fprintf(f_raw_data_out_h, "Fin header \n");
		fclose(f_raw_data_out_h);
	}

	char dire_time[70];
	char dire_time_hdr[70];

	if (rawsave) {
		strcpy(dire_time, fname_dir);
		strcat(dire_time, "/TIME");
		mkdir(dire_time);
		strcpy(dire_time_hdr, dire_time);
		strcat(dire_time_hdr, "/time.hdr");

		FILE *f_time_out_h = fopen(dire_time_hdr, "ab");
		fprintf(f_time_out_h, "Placa: '%s' \n", placa);
		fprintf(f_time_out_h, "Dato: '%s' \n",
			"Tiempo de escritura a disco");
		fprintf(f_time_out_h, "Comentario: '%s' \n",
			"Tiempo de escritura a disco de un chunk de dato crudo con el fin de monitoreo");
		fprintf(f_time_out_h, "Bins: [%d] \n", bins);
		fprintf(f_time_out_h, "NumberOfChannels: [%d] \n", nCh);
		fprintf(f_time_out_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_time_out_h, "nShotsChk: [%d] \n", nShotsChk);
		fprintf(f_time_out_h, "nShots: [%d] \n", nShots);
		fprintf(f_time_out_h, "Fin header \n");
		fclose(f_time_out_h);
	}

	long long shots_totales = 0;
	int current_chunk =
		0; // current_chunk lleva la iteracion actual mod cant_max
	unsigned long long bin_move = bins * nShotsChk * nCh;
	int it_ovr = 0; /// contador de iteraciones para situacion de overrun
	double velocidad;
	int i, j;
	int cnt_ch = 0;
	int cnt_ch_save = 0;

	char time_str[200];
	double tiempo;

	HANDLE f_raw_data_out_ch0_f;
	HANDLE f_raw_data_out_ch1_f;
	HANDLE f_raw_data_out_f;
	HANDLE f_time_out_f;

	char nombre_ch0[70];
	char nombre_ch1[70];
	char nombre_ch[70];
	char nombre_time[70];
	char num_str[10];

	while (salir == 0) { /// buscar mejor condicion de salida

		WaitForSingleObject(callback.semConsumidor[0],
				    INFINITE); // espero si no hay datos

		if (callback.ovr) { /// OVR CHECK. Si no puedo seguir
				    /// escribiendo, copio todos los datos del
				    /// buffer.
			it_ovr++;
			if (it_ovr == CHUNKS - 1) { /// si ya di toda la vuelta,
						    /// termino
				break;
			}
			printf("Hay overrun \n");
		}

		cnt_ch++;

		StartCounter(); /// tiempo
		if (rawsave) {
			if (caso == 1) {

				for (j = 0; j < nShotsChk; ++j) {
					for (i = 0; i < bins_raw; ++i) {
						ch[j * bins_raw + i] = chkBuffer
							[bin_move
								 * current_chunk
							 + j * bins * nCh
							 + delay_raw * nCh
							 + nCh * i];
					}
				}

				if (cnt_ch % chunks_save == 1) {

					strcpy(nombre_ch0, dire_ch0);
					strcat(nombre_ch0, "/");
					sprintf(num_str, "%06d", cnt_ch_save);
					strcat(nombre_ch0, num_str);
					strcat(nombre_ch0, ".bin");

					f_raw_data_out_ch0_f = CreateFile(
						nombre_ch0, // name of the file
						GENERIC_WRITE, // open for
							       // writing
						0, // sharing mode, none in this
						   // case
						0, // use default security
						   // descriptor
						CREATE_ALWAYS, // overwrite if
							       // exists
						FILE_ATTRIBUTE_NORMAL, 0);
				}

				bytesw = 0;
				WriteFile(f_raw_data_out_ch0_f, &ch[0],
					  (long long)bin_copia * sizeof(short),
					  &bytesw, NULL); ///
				FlushFileBuffers(f_raw_data_out_ch0_f);

				if (cnt_ch % chunks_save == 0) {
					CloseHandle(f_raw_data_out_ch0_f);
				}
			} else if (caso == 2) {
				for (j = 0; j < nShotsChk; ++j) {
					for (i = 0; i < bins_raw; ++i) {
						ch[j * bins_raw + i] = chkBuffer
							[bin_move
								 * current_chunk
							 + j * bins * nCh
							 + delay_raw * nCh
							 + nCh * i + 1];
					}
				}

				if (cnt_ch % chunks_save == 1) {

					strcpy(nombre_ch1, dire_ch1);
					strcat(nombre_ch1, "/");
					sprintf(num_str, "%06d", cnt_ch_save);
					strcat(nombre_ch1, num_str);
					strcat(nombre_ch1, ".bin");

					f_raw_data_out_ch1_f = CreateFile(
						nombre_ch1, // name of the file
						GENERIC_WRITE, // open for
							       // writing
						0, // sharing mode, none in this
						   // case
						0, // use default security
						   // descriptor
						CREATE_ALWAYS, // overwrite if
							       // exists
						FILE_ATTRIBUTE_NORMAL, 0);
				}

				bytesw = 0;
				WriteFile(f_raw_data_out_ch1_f, &ch[0],
					  (long long)bin_copia * sizeof(short),
					  &bytesw, NULL); ///
				FlushFileBuffers(f_raw_data_out_ch1_f);

				if (cnt_ch % chunks_save == 0) {
					CloseHandle(f_raw_data_out_ch1_f);
				}
			} else if (caso == 3) {
				for (j = 0; j < nShotsChk; ++j) {
					for (i = 0; i < bins_raw * nCh; ++i) {
						ch[j * bins_raw * nCh
						   + i] = chkBuffer
							[bin_move
								 * current_chunk
							 + j * bins * nCh
							 + delay_raw * nCh + i];
					}
				}

				if (cnt_ch % chunks_save == 1) {

					strcpy(nombre_ch, dire_ch);
					strcat(nombre_ch, "/");
					sprintf(num_str, "%06d", cnt_ch_save);
					strcat(nombre_ch, num_str);
					strcat(nombre_ch, ".bin");

					f_raw_data_out_f = CreateFile(
						nombre_ch, // name of the file
						GENERIC_WRITE, // open for
							       // writing
						0, // sharing mode, none in this
						   // case
						0, // use default security
						   // descriptor
						CREATE_ALWAYS, // overwrite if
							       // exists
						FILE_ATTRIBUTE_NORMAL, 0);
				}

				bytesw = 0;
				WriteFile(f_raw_data_out_f, &ch[0],
					  (long long)bin_copia * sizeof(short),
					  &bytesw, NULL); ///
				FlushFileBuffers(f_raw_data_out_f);

				if (cnt_ch % chunks_save == 0) {
					CloseHandle(f_raw_data_out_f);
				}
			}

			if (bytesw < ((long long)bin_copia * sizeof(short))) {
				salir_raw = true;
				printf("ERROR ESCRITURA");
				return n;
			}
		}
		tiempo = GetCounter();

		current_chunk++;
		callback.disponible[current_chunk - 1] = true;

		/// Escribo archivo de tiempo
		if (rawsave) {
			snprintf(time_str, sizeof(time_str), "Time: %f \n",
				 tiempo);
			// bytesw=0;

			if (cnt_ch % chunks_save == 1) {

				strcpy(nombre_time, dire_time);
				strcat(nombre_time, "/");
				sprintf(num_str, "%06d", cnt_ch_save);
				strcat(nombre_time, num_str);
				strcat(nombre_time, ".time");

				f_time_out_f = CreateFile(
					nombre_time,   // name of the file
					GENERIC_WRITE, // open for writing
					0, // sharing mode, none in this case
					0, // use default security descriptor
					CREATE_ALWAYS, // overwrite if exists
					FILE_ATTRIBUTE_NORMAL, 0);

				cnt_ch_save++;
			}

			WriteFile(f_time_out_f, time_str, strlen(time_str),
				  &bytesw, NULL);
			velocidad = ((double)bin_copia * sizeof(short) / 1024.0
				     / 1024.0)
				    / (tiempo / 1000.0);
			// printf("velocidad de escritura: %f [Mb/seg]
			// \n",((double)bin_copia*sizeof(short)/1024/1024)/(tiempo/1000));
			printf("velocidad de escritura: %f [Mb/seg] \n",
			       velocidad);
			FlushFileBuffers(f_time_out_f);

			if (cnt_ch % chunks_save == 0) {
				CloseHandle(f_time_out_f);
			}
		}

		// printf("Escribi %d bytes en %.6fseg\n",
		// bin_copia*2,tiempo/1000.0);
		shots_totales += nShotsChk;

		if (!infinite_daq
		    && shots_totales
			       >= nShots) { /// deja de adqurir cuando llego
					    /// hasta aca (puede haber adquirido
					    /// mas shots que shot lim). puede
					    /// guardar m?s datos que shot_lim
					    /// (si no es multiplo de
					    /// shots_per_chunk).
			break;
		}

		if (current_chunk == CHUNKS) { // reinicio current_chunk
			current_chunk = 0;
		}
	}

	if (rawsave) {
		CloseHandle(f_time_out_f);
	}
	if (caso == 1) {
		CloseHandle(f_raw_data_out_ch0_f);
	}
	if (caso == 2) {
		CloseHandle(f_raw_data_out_ch1_f);
	}
	if (caso == 3) {
		CloseHandle(f_raw_data_out_f);
	}
	salir_raw = true;
	salir = true; // por si hay overrun
	printf("Hilo RAW DATA terminado \n");
	return NULL;
}

void *procesa_osc(void *n)
{
	struct th_Data *data = (struct th_Data *)n;
	int bins = data->bins;
	int delay = data->delay;
	int nShotsChk = data->nShotsChk;
	int nCh = data->nCh;
	int qFreq = data->qFreq;
	long long nShots = data->nShots;
	bool infinite_daq = data->infinite_daq;
	int window_time_osc = data->window_time_osc;
	char *placa = data->placa;
	int cant_curvas = data->cant_curvas;

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	DWORD dwThreadPri = GetThreadPriority(GetCurrentThread());
	printf("Current thread priority is %d\n", dwThreadPri);

	float *procesado = malloc(bins * cant_curvas * sizeof(float) * nCh);

	/// VER EL TEMA DE LOS PIPES
	// int window=10;
	DWORD leido_paip;
	HANDLE paip = CreateNamedPipe(
		TEXT("\\\\.\\pipe\\Pipe3"),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE, // FILE_FLAG_FIRST_PIPE_INSTANCE
					      // is not needed but forces
					      // CreateNamedPipe(..) to fail if
					      // the pipe already exists...
		PIPE_WAIT, 1, bins * sizeof(float) * cant_curvas * 8,
		bins * sizeof(float) * cant_curvas * 8,
		NMPWAIT_USE_DEFAULT_WAIT, NULL);

	char command[100];
	// snprintf(command,100,"start cmd /k python test_marco1_m.py %d %d
	// %d",perfil_mv_avg_size,window_bin,qFreq);
	snprintf(command, 100, "start cmd /k python osciloscopio.py %d %d %d",
		 bins, cant_curvas, qFreq);
	printf(command);
	system(command);

	short *chunk_actual = chkBuffer;
	int chunk_control = 0;
	int i, k;

	while (salir == 0) {

		WaitForSingleObject(callback.semConsumidor[4], INFINITE);

		memset(procesado, 0, bins * cant_curvas * sizeof(float) * nCh);
		for (i = 0; i < cant_curvas; ++i) {

			for (k = 0; k < bins * window_time_osc; ++k) {
				procesado[i * bins + k % bins] +=
					((float)chunk_actual
						 [bins * nShotsChk / cant_curvas
							  * i
						  + nCh * k])
					/ window_time_osc;
				procesado[cant_curvas * bins + i * bins
					  + k % bins] +=
					((float)chunk_actual
						 [bins * nShotsChk / cant_curvas
							  * i
						  + nCh * k + 1])
					/ window_time_osc;
			}
		}

		/// Pipe para graficar
		WriteFile(paip, &procesado[0],
			  bins * cant_curvas * sizeof(float) * nCh, &leido_paip,
			  NULL);

		chunk_control++;
		if (chunk_control == CHUNKS) { // why??? deberia ser chunks??
			chunk_actual = chkBuffer;
			chunk_control = 0;
		} else {
			chunk_actual +=
				bins * nShotsChk * nCh; // avanzo 2 channels
		}
	}

	CloseHandle(paip);
	free(procesado);
	salir_osc = true;
	printf("Hilo Osciloscopio terminado \n");
	return NULL;
}

void ts_fill(SYSTEMTIME ts, unsigned short *ts_pipe)
{
	ts_pipe[0] = ts.wYear;
	ts_pipe[1] = ts.wMonth;
	ts_pipe[2] = ts.wDay;
	ts_pipe[3] = ts.wHour;
	ts_pipe[4] = ts.wMinute;
	ts_pipe[5] = ts.wSecond;
	ts_pipe[6] = ts.wMilliseconds;
}

void *procesa_STD(void *n) // Sucede lo mismo que con procesa_monitoreo
{
	struct th_Data *data = (struct th_Data *)n;
	int bins = data->bins;
	int delay = data->delay;
	int nShotsChk = data->nShotsChk;
	int chunks_save = data->chunks_save;
	int nCh = data->nCh;
	int qFreq = data->qFreq;
	long long nShots = data->nShots;
	bool infinite_daq = data->infinite_daq;
	int window_time = data->window_time;
	int window_bin = data->window_bin;
	int window_bin_mean = data->window_bin_mean;
	char *fname_dir = data->fname_dir;
	char *placa = data->placa;
	bool stdsave = data->stdsave;

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	DWORD dwThreadPri = GetThreadPriority(GetCurrentThread());
	printf("Current thread priority is %d\n", dwThreadPri);

	short *chunk_actual = chkBuffer; // arranco corrido 2 canales
	int i, k;
	// float *raw=malloc(bin*shots_per_chunk*sizeof(float));//falta free
	unsigned int perfil_mv_avg_size = bins - window_bin_mean + 1;
	float *mean_w = malloc(bins * nShotsChk * sizeof(float)); // falta free
	float *mean = malloc(bins * sizeof(float));		  // falta free
	float *perfil = malloc(bins * sizeof(float));		  // falta free
	float *perfil_mv_avg =
		malloc(perfil_mv_avg_size * sizeof(float)); // falta free
	float *std_mv_avg =
		malloc(perfil_mv_avg_size * sizeof(float)); // falta free
	memset(mean, 0, bins * sizeof(float));
	memset(mean_w, 0, bins * nShotsChk * sizeof(float));

	/// Es para hacer promedio en bins
	unsigned int vector_avg_size = perfil_mv_avg_size - window_bin + 1;
	float *vector_avg = malloc(sizeof(float) * vector_avg_size);

	/// Para guardar std y avg por separado (06/02/2018)
	float *vector_std_g = malloc(sizeof(float) * vector_avg_size);
	float *vector_avg_g = malloc(sizeof(float) * vector_avg_size);

	/// Escribe a archivo
	char dire_std[70];
	char dire_std_hdr[70];
	char dire_avg[70];
	char dire_avg_hdr[70];

	DWORD bytesw = 0;
	if (stdsave) {
		strcpy(dire_std, fname_dir);
		strcat(dire_std, "/STD");
		mkdir(dire_std);
		strcpy(dire_std_hdr, dire_std);
		strcat(dire_std_hdr, "/std.hdr");

		FILE *f_std_out_h = fopen(dire_std_hdr, "ab");
		fprintf(f_std_out_h, "Placa: '%s' \n", placa);
		fprintf(f_std_out_h, "Dato: '%s' \n", "STD");
		fprintf(f_std_out_h, "DataType: '%s' \n", "float");
		fprintf(f_std_out_h, "PythonNpDataType: '%s' \n", "np.float32");
		fprintf(f_std_out_h, "BytesPerDatum: [%d] \n", 4);
		fprintf(f_std_out_h, "Delay: [%d] \n", delay);
		fprintf(f_std_out_h, "Bins: [%d] \n", bins);
		fprintf(f_std_out_h, "Cols: [%d] \n", vector_avg_size);
		fprintf(f_std_out_h, "NDatum: [%d] \n", 1);
		fprintf(f_std_out_h, "NumberOfChannels: [%d] \n", nCh);
		fprintf(f_std_out_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_std_out_h, "nShotsChk: [%d] \n", nShotsChk);
		fprintf(f_std_out_h, "nShots: [%d] \n", nShots);
		fprintf(f_std_out_h, "WindowTime: [%d] \n", window_time);
		fprintf(f_std_out_h, "WindowBin: [%d] \n", window_bin);
		fprintf(f_std_out_h, "WindowBinMean: [%d] \n", window_bin_mean);
		fprintf(f_std_out_h, "Fin header \n");
		fclose(f_std_out_h);

		strcpy(dire_avg, fname_dir);
		strcat(dire_avg, "/AVG");
		mkdir(dire_avg);
		strcpy(dire_avg_hdr, dire_avg);
		strcat(dire_avg_hdr, "/avg.hdr");

		FILE *f_avg_out_h = fopen(dire_avg_hdr, "ab");
		fprintf(f_avg_out_h, "Placa: '%s' \n", placa);
		fprintf(f_avg_out_h, "Dato: '%s' \n", "AVG");
		fprintf(f_avg_out_h, "DataType: '%s' \n", "float");
		fprintf(f_avg_out_h, "PythonNpDataType: '%s' \n", "np.float32");
		fprintf(f_avg_out_h, "BytesPerDatum: [%d] \n", 4);
		fprintf(f_avg_out_h, "Delay: [%d] \n", delay);
		fprintf(f_avg_out_h, "Bins: [%d] \n", bins);
		fprintf(f_avg_out_h, "Cols: [%d] \n", vector_avg_size);
		fprintf(f_avg_out_h, "NDatum: [%d] \n", 1);
		fprintf(f_avg_out_h, "NumberOfChannels: [%d] \n", nCh);
		fprintf(f_avg_out_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_avg_out_h, "nShotsChk: [%d] \n", nShotsChk);
		fprintf(f_avg_out_h, "nShots: [%d] \n", nShots);
		fprintf(f_avg_out_h, "WindowTime: [%d] \n", window_time);
		fprintf(f_avg_out_h, "WindowBin: [%d] \n", window_bin);
		fprintf(f_avg_out_h, "WindowBinMean: [%d] \n", window_bin_mean);
		fprintf(f_avg_out_h, "Fin header \n");
		fclose(f_avg_out_h);
	}

	/// VER EL TEMA DE LOS PIPES
	// int window=10;
	DWORD leido_paip;
	HANDLE paip = CreateNamedPipe(
		TEXT("\\\\.\\pipe\\Pipe"),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE, // FILE_FLAG_FIRST_PIPE_INSTANCE
					      // is not needed but forces
					      // CreateNamedPipe(..) to fail if
					      // the pipe already exists...
		PIPE_WAIT, 1, bins * sizeof(double) * CHUNKS,
		bins * sizeof(double) * CHUNKS, NMPWAIT_USE_DEFAULT_WAIT, NULL);

	char command[100];
	// snprintf(command,100,"start cmd /k python test_marco1_m.py %d %d
	// %d",perfil_mv_avg_size,window_bin,qFreq);
	snprintf(command, 100, "start cmd /k python test_marco1_m.py %d %d %d",
		 perfil_mv_avg_size, window_bin, qFreq);
	printf(command);
	system(command);

	int cnt_ch = 0;
	int cnt_ch_save = 0;

	double ttime = 0.0;
	int chunk_control = 0;

	HANDLE f_std_out_f;
	HANDLE f_avg_out_f;
	char nombre_std[70];
	char nombre_avg[70];
	char num_str[10];
	float *mean_t = malloc(bins * sizeof(float));
	unsigned short *ts_pipe = malloc(7 * sizeof(unsigned short));

	while (salir == 0) { /// wait podr�a estar aca

		cnt_ch++;

		WaitForSingleObject(callback.semConsumidor[1], INFINITE);
		// StartCounter();
		//    printf("HOLA\n");
		// StartCounter();
		memset(mean, 0, bins * sizeof(float));
		memset(mean_w, 0, bins * nShotsChk * sizeof(float));

		for (i = 0; i < bins * (window_time - 1);
		     ++i) { /// calcula ventana-1
			mean_w[i % bins] += (float)(chunk_actual[i * nCh]);
		}

		for (i = bins * (window_time - 1); i < bins * window_time;
		     ++i) { /// termina ventana
			mean_w[i % bins] = (mean_w[i % bins]
					    + (float)(chunk_actual[i * nCh]))
					   / window_time;
			mean[i % bins] += mean_w[i % bins];
		}
		//    printf("AndaPrimeraWindow\n");

		for (i = bins * window_time; i < bins * nShotsChk;
		     ++i) { /// resta y suma el apropiado a la ventana calculada

			mean_w[(i - bins * (window_time - 1))] =
				mean_w[(i - bins * (window_time))]
				+ ((float)(chunk_actual[i * nCh])
				   - (float)(chunk_actual
						     [nCh
						      * (i
							 - bins * (window_time))]))
					  / window_time;
			mean[i % bins] +=
				mean_w[(i - bins * (window_time - 1))];
		} /// falta restar la se�al con el filtro mean_w
		//    printf("AndaFiltroMedia\n");

		for (i = 0; i < bins; ++i) { /// STD Cambio en 29/09/2017
			mean_t[i] = mean[i] / (nShotsChk - window_time);
			//            //mean[i]=mean_t;
			mean[i] = 0;
		}
		for (k = 0; k < (nShotsChk - window_time) * bins; ++k) {
			mean[k % bins] += (mean_w[k] - mean_t[k % bins])
					  * (mean_w[k] - mean_t[k % bins]);
		}
		for (i = 0; i < bins; ++i) {
			mean[i] = sqrt(mean[i] / (nShotsChk - window_time - 1));
			perfil[i] = mean_t[i];
			//        //mean[i]=sqrt(mean[i]/(shots_per_chunk-window-1));
		}

		/// Agrega Marco 18/07/2017 (version 2.8.2). Ahora hace cociente
		/// entre la desviaci�n est�ndar y el promedio con moving
		/// average.
		moving1avg(perfil, bins, window_bin_mean, perfil_mv_avg,
			   perfil_mv_avg_size);

		/// Cambio 06/02/2018
		for (i = 0; i < perfil_mv_avg_size; ++i) {
			std_mv_avg[i] = mean[i] / perfil_mv_avg[i];
		}

		/// Moving STD PIPE average en bines
		moving1avg(std_mv_avg, perfil_mv_avg_size, window_bin,
			   vector_avg, vector_avg_size);

		/// Moving STD average en bines
		moving1avg(mean, perfil_mv_avg_size, window_bin, vector_std_g,
			   vector_avg_size);

		/// Moving AVG average en bines
		moving1avg(perfil_mv_avg, perfil_mv_avg_size, window_bin,
			   vector_avg_g, vector_avg_size);

		/// Pipe para graficar
		WriteFile(paip, &vector_avg[0], sizeof(float) * vector_avg_size,
			  &leido_paip, NULL);
		SYSTEMTIME ts = tssBuffer[chunk_control];
		ts_fill(ts, ts_pipe);
		WriteFile(paip, &ts_pipe[0], sizeof(unsigned short) * 7,
			  &leido_paip, NULL);

		// 20170111 agregamos esto para guardar a disco el vector
		// procesado el archivo se abre y cierrra en main
		if (stdsave) {

			if (cnt_ch % chunks_save == 1) {

				/// Guarda STD
				strcpy(nombre_std, dire_std);
				strcat(nombre_std, "/");
				sprintf(num_str, "%06d", cnt_ch_save);
				strcat(nombre_std, num_str);
				strcat(nombre_std, ".std");

				f_std_out_f = CreateFile(
					nombre_std,    // name of the file
					GENERIC_WRITE, // open for writing
					0, // sharing mode, none in this case
					0, // use default security descriptor
					CREATE_ALWAYS, // overwrite if exists
					FILE_ATTRIBUTE_NORMAL, 0);

				/// Guarda MEAN
				strcpy(nombre_avg, dire_avg);
				strcat(nombre_avg, "/");
				sprintf(num_str, "%06d", cnt_ch_save);
				strcat(nombre_avg, num_str);
				strcat(nombre_avg, ".avg");

				f_avg_out_f = CreateFile(
					nombre_avg,    // name of the file
					GENERIC_WRITE, // open for writing
					0, // sharing mode, none in this case
					0, // use default security descriptor
					CREATE_ALWAYS, // overwrite if exists
					FILE_ATTRIBUTE_NORMAL, 0);

				cnt_ch_save++;
			}

			bytesw = 0;
			/// Guarda STD
			WriteFile(f_std_out_f, &vector_std_g[0],
				  vector_avg_size * sizeof(float), &bytesw,
				  NULL);
			FlushFileBuffers(f_std_out_f);

			bytesw = 0;
			/// Guarda el AVG
			WriteFile(f_avg_out_f, &vector_avg_g[0],
				  vector_avg_size * sizeof(float), &bytesw,
				  NULL);
			FlushFileBuffers(f_avg_out_f);

			if (cnt_ch % chunks_save == 0) {
				CloseHandle(f_std_out_f);
				CloseHandle(f_avg_out_f);
			}
		}

		// printf("tiempo: %.5f \n",GetCounter()/1000.0);
		// printf("Termine STD \n");

		chunk_control++;
		if (chunk_control == CHUNKS) { // why??? deberia ser chunks??
			chunk_actual = chkBuffer;
			chunk_control = 0;
		} else {
			chunk_actual +=
				bins * nShotsChk * nCh; // avanzo 2 channels
		}
	}

	if (stdsave) {
		CloseHandle(f_std_out_f);
		CloseHandle(f_avg_out_f);
	}
	CloseHandle(paip);
	salir_std = true;
	printf("Hilo STD terminado \n");
	return NULL;
}

void *procesa_Monitoreo(void *n) // Se encarga de manejar la memoria, procesar y
				 // guardar la configuracion ademas de escribir
				 // los datos... esta va a la guillotina
{
	struct th_Data *data = (struct th_Data *)n;
	int bins = data->bins;
	int delay = data->delay;
	int nShotsChk = data->nShotsChk;
	int chunks_save = data->chunks_save;
	int nCh = data->nCh;
	int qFreq = data->qFreq;
	long long nShots = data->nShots;
	bool infinite_daq = data->infinite_daq;
	int window_time = data->window_time;
	int window_bin = data->window_bin;
	int bin_mon_laser_i = data->bin_mon_laser_i;
	int bin_mon_laser_f = data->bin_mon_laser_f;
	int bin_mon_edfa_i = data->bin_mon_edfa_i;
	int bin_mon_edfa_f = data->bin_mon_edfa_f;
	int window_mon_time = data->window_mon_time;
	char *fname_dir = data->fname_dir;
	char *placa = data->placa;
	bool mon_edfa_save = data->mon_edfa_save;
	bool mon_laser_save = data->mon_laser_save;
	float cLaser = data->cLaser;
	float cEDFA = data->cEDFA;

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	DWORD dwThreadPri = GetThreadPriority(GetCurrentThread());
	printf("Current thread priority is %d\n", dwThreadPri);

	short *chunk_actual = chkBuffer; // arranco corrido 2 canales
	int i, j, k;
	// float *raw=malloc(bin*shots_per_chunk*sizeof(float));//falta free
	float *mean_dbl = malloc(bins * sizeof(double)); // falta free
	float *mean_flt = malloc(bins * sizeof(float));  // falta free

	// AGREGA MARCO PARA VER ESTABILIDAD DE LA ENERG�A DE LOS PULSOS
	// AMPLIFICADOS 11/01/2017 Histograma Laser
	int delta_bin_laser = bin_mon_laser_f - bin_mon_laser_i;
	int off_e_laser = 0;
	int energia_avg_size = nShotsChk - window_mon_time + 1;
	int energia_avg_size_r = nShotsChk / window_mon_time;
	float *energia_laser = malloc(nShotsChk * sizeof(float));
	float *energia_avg_laser = malloc(energia_avg_size * sizeof(float));
	float *energia_avg_r_laser = malloc(energia_avg_size_r * sizeof(float));

	memset(energia_laser, 0, nShotsChk * sizeof(float));
	memset(energia_avg_laser, 0, energia_avg_size * sizeof(float));
	memset(energia_avg_r_laser, 0, energia_avg_size_r * sizeof(float));

	// Edfa
	int delta_bin_edfa = bin_mon_edfa_f - bin_mon_edfa_i;
	int off_e_edfa = 0;
	float *energia_edfa = malloc(nShotsChk * sizeof(float));
	float *energia_avg_edfa = malloc(energia_avg_size * sizeof(float));
	float *energia_avg_r_edfa = malloc(energia_avg_size_r * sizeof(float));

	memset(energia_edfa, 0, nShotsChk * sizeof(float));
	memset(energia_avg_edfa, 0, energia_avg_size * sizeof(float));
	memset(energia_avg_r_edfa, 0, energia_avg_size_r * sizeof(float));

	int super_vector_size = bins + 2 * energia_avg_size_r;
	float *super_vector = malloc(super_vector_size * sizeof(float));
	memset(super_vector, 0, super_vector_size * sizeof(float));
	//

	/// Escribe a archivo
	char dire_mon_laser[70];
	char dire_mon_laser_hdr[70];

	DWORD bytesw = 0;
	if (mon_laser_save) {
		strcpy(dire_mon_laser, fname_dir);
		strcat(dire_mon_laser, "/MON_LASER");
		mkdir(dire_mon_laser);
		strcpy(dire_mon_laser_hdr, dire_mon_laser);
		strcat(dire_mon_laser_hdr, "/laser.mon.hdr");

		FILE *f_monitoreo_out_laser_h = fopen(dire_mon_laser_hdr, "ab");
		fprintf(f_monitoreo_out_laser_h, "Placa: '%s' \n", placa);
		fprintf(f_monitoreo_out_laser_h, "Dato: '%s' \n",
			"Energia Pulsos LASER para Monitoreo");
		fprintf(f_monitoreo_out_laser_h, "Comentario: '%s' \n",
			"Calcula la energia del pulso (laser o EDFA) como la integral de la senial del fotodiodo entre BinMonIni y BinMonFin");
		fprintf(f_monitoreo_out_laser_h, "DataType: '%s' \n", "float");
		fprintf(f_monitoreo_out_laser_h, "PythonNpDataType: '%s' \n",
			"np.float32");
		fprintf(f_monitoreo_out_laser_h, "BytesPerDatum: [%d] \n", 4);
		fprintf(f_monitoreo_out_laser_h, "Delay: [%d] \n", delay);
		fprintf(f_monitoreo_out_laser_h, "Bins: [%d] \n", bins);
		fprintf(f_monitoreo_out_laser_h, "Cols: [%d] \n",
			energia_avg_size_r);
		fprintf(f_monitoreo_out_laser_h, "NDatum: [%d] \n", 1);
		fprintf(f_monitoreo_out_laser_h, "BinMonIni: [%d] \n",
			bin_mon_laser_i);
		fprintf(f_monitoreo_out_laser_h, "BinMonFin: [%d] \n",
			bin_mon_laser_f);
		fprintf(f_monitoreo_out_laser_h, "NumberOfChannels: [%d] \n",
			nCh);
		fprintf(f_monitoreo_out_laser_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_monitoreo_out_laser_h, "nShotsChk: [%d] \n",
			nShotsChk);
		fprintf(f_monitoreo_out_laser_h, "nShots: [%d] \n", nShots);
		fprintf(f_monitoreo_out_laser_h, "WindowTime: [%d] \n",
			window_time);
		fprintf(f_monitoreo_out_laser_h, "WindowBin: [%d] \n",
			window_bin);
		fprintf(f_monitoreo_out_laser_h, "Fin header \n");
		fclose(f_monitoreo_out_laser_h);
	}

	char dire_mon_edfa[70];
	char dire_mon_edfa_hdr[70];

	if (mon_edfa_save) {
		strcpy(dire_mon_edfa, fname_dir);
		strcat(dire_mon_edfa, "/MON_EDFA");
		mkdir(dire_mon_edfa);
		strcpy(dire_mon_edfa_hdr, dire_mon_edfa);
		strcat(dire_mon_edfa_hdr, "/laser.mon.hdr");

		FILE *f_monitoreo_out_edfa_h = fopen(dire_mon_edfa_hdr, "ab");
		fprintf(f_monitoreo_out_edfa_h, "Placa: '%s' \n", placa);
		fprintf(f_monitoreo_out_edfa_h, "Dato: '%s' \n",
			"Energia Pulsos EDFA para Monitoreo");
		fprintf(f_monitoreo_out_edfa_h, "Comentario: '%s' \n",
			"Calcula la energia del pulso (laser o EDFA) como la integral de la senial del fotodiodo entre BinMonIni y BinMonFin");
		fprintf(f_monitoreo_out_edfa_h, "DataType: '%s' \n", "float");
		fprintf(f_monitoreo_out_edfa_h, "PythonNpDataType: '%s' \n",
			"np.float32");
		fprintf(f_monitoreo_out_edfa_h, "BytesPerDatum: [%d] \n", 4);
		fprintf(f_monitoreo_out_edfa_h, "Delay: [%d] \n", delay);
		fprintf(f_monitoreo_out_edfa_h, "Bins: [%d] \n", bins);
		fprintf(f_monitoreo_out_edfa_h, "Cols: [%d] \n",
			energia_avg_size_r);
		fprintf(f_monitoreo_out_edfa_h, "NDatum: [%d] \n", 1);
		fprintf(f_monitoreo_out_edfa_h, "BinMonIni: [%d] \n",
			bin_mon_edfa_i);
		fprintf(f_monitoreo_out_edfa_h, "BinMonFin: [%d] \n",
			bin_mon_edfa_f);
		fprintf(f_monitoreo_out_edfa_h, "NumberOfChannels: [%d] \n",
			nCh);
		fprintf(f_monitoreo_out_edfa_h, "FreqRatio: [%d] \n", qFreq);
		fprintf(f_monitoreo_out_edfa_h, "nShotsChk: [%d] \n",
			nShotsChk);
		fprintf(f_monitoreo_out_edfa_h, "nShots: [%d] \n", nShots);
		fprintf(f_monitoreo_out_edfa_h, "WindowTime: [%d] \n",
			window_time);
		fprintf(f_monitoreo_out_edfa_h, "WindowBin: [%d] \n",
			window_bin);
		fprintf(f_monitoreo_out_edfa_h, "Fin header \n");
		fclose(f_monitoreo_out_edfa_h);
	}

	///

	/// Pipe
	DWORD leido_paip;
	HANDLE paip = CreateNamedPipe(
		TEXT("\\\\.\\pipe\\Pipe2"),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE, // FILE_FLAG_FIRST_PIPE_INSTANCE
					      // is not needed but forces
					      // CreateNamedPipe(..) to fail if
					      // the pipe already exists...
		PIPE_WAIT, 1,
		//                            bin*sizeof(double)*2,
		//                            bin*sizeof(double)*2,
		super_vector_size * sizeof(double) * 20,
		super_vector_size * sizeof(double) * 20,
		NMPWAIT_USE_DEFAULT_WAIT, NULL);

	char command[100];
	snprintf(command, 100,
		 "start python impar.py %d %d %d %d %d %d %d %f %f", bins,
		 energia_avg_size_r, energia_avg_size_r, bin_mon_laser_i,
		 bin_mon_laser_f, bin_mon_edfa_i, bin_mon_edfa_f, cLaser,
		 cEDFA);
	printf(command);
	system(command);

	int cnt_ch = 0;
	int cnt_ch_save_l = 0;
	int cnt_ch_save_e = 0;

	int chunk_control = 0;

	HANDLE f_monitoreo_out_laser_f;
	HANDLE f_monitoreo_out_edfa_f;
	char nombre_mon_laser[70];
	char nombre_mon_edfa[70];
	char num_str[10];

	while (salir == 0) { /// wait podr�a estar aca

		cnt_ch++;

		WaitForSingleObject(callback.semConsumidor[2], INFINITE);

		// StartCounter();
		memset(mean_dbl, 0, bins * sizeof(double));
		memset(mean_flt, 0, bins * sizeof(float));
		// win_actual=0;
		//   w_index=0;

		for (i = 0; i < bins * nShotsChk; ++i) { /// acumula
			mean_dbl[(i) % bins] +=
				(double)(chunk_actual[i * nCh
						      + 1]); // magia para no
							     // hacer 2 for
		}

		for (i = 0; i < bins; ++i) { /// divide
			mean_dbl[i] = mean_dbl[i] / (double)nShotsChk;
			mean_flt[i] = (float)(mean_dbl[i]);
			super_vector[i] = mean_flt[i]; // agrega Marco para
						       // estabilidad de pulsos
		}

		// AGREGA MARCO PARA VER ESTABILIDAD DE LA ENERG�A DE LOS PULSOS
		// AMPLIFICADOS Sumo la senal con la regla de los trapecios.
		// Monitoreo LASER
		for (i = 0; i < nShotsChk; ++i) {
			off_e_laser =
				nCh
				* (i * bins
				   + bin_mon_laser_i); // para posicionar en el
						       // bin correspondiente
						       // del canal 1
			energia_laser[i] =
				(float)(chunk_actual[off_e_laser + 1] / 2)
				+ (float)(chunk_actual[off_e_laser
						       + delta_bin_laser * nCh
						       + 1]
					  / 2);
			for (j = 1; j < delta_bin_laser - 1; ++j) {
				energia_laser[i] +=
					(float)(chunk_actual[off_e_laser
							     + j * nCh + 1]);
			}

			off_e_edfa =
				2
				* (i * bins
				   + bin_mon_edfa_i); // para posicionar en el
						      // bin correspondiente del
						      // canal 1
			energia_edfa[i] =
				(float)(chunk_actual[off_e_edfa + 1] / 2)
				+ (float)(chunk_actual[off_e_edfa
						       + delta_bin_edfa * nCh
						       + 1]
					  / 2);
			for (j = 1; j < delta_bin_edfa - 1; ++j) {
				energia_edfa[i] +=
					(float)(chunk_actual[off_e_edfa
							     + j * nCh + 1]);
			}
		}

		// Aplico moving average del tamano de la ventana window
		moving1avg(energia_laser, nShotsChk, window_mon_time,
			   energia_avg_laser, energia_avg_size);
		moving1avg(energia_edfa, nShotsChk, window_mon_time,
			   energia_avg_edfa, energia_avg_size);

		//
		for (i = 0; i < energia_avg_size_r; ++i) {
			energia_avg_r_laser[i] =
				energia_avg_laser[i * window_mon_time];
			energia_avg_r_edfa[i] =
				energia_avg_edfa[i * window_mon_time];
		}

		for (i = 0; i < energia_avg_size_r; ++i) {
			super_vector[bins + i] = energia_avg_r_laser[i];
			super_vector[bins + energia_avg_size_r + i] =
				energia_avg_r_edfa[i];
		}

		//
		if (mon_laser_save) {
			bytesw = 0;

			if (cnt_ch % chunks_save == 1) {

				strcpy(nombre_mon_laser, dire_mon_laser);
				strcat(nombre_mon_laser, "/");
				sprintf(num_str, "%06d", cnt_ch_save_l);
				strcat(nombre_mon_laser, num_str);
				strcat(nombre_mon_laser, ".mon");

				f_monitoreo_out_laser_f = CreateFile(
					nombre_mon_laser, // name of the file
					GENERIC_WRITE,    // open for writing
					0, // sharing mode, none in this case
					0, // use default security descriptor
					CREATE_ALWAYS, // overwrite if exists
					FILE_ATTRIBUTE_NORMAL, 0);

				cnt_ch_save_l++;
			}

			WriteFile(f_monitoreo_out_laser_f,
				  &energia_avg_r_laser[0],
				  energia_avg_size_r * sizeof(float), &bytesw,
				  NULL);
			FlushFileBuffers(f_monitoreo_out_laser_f);

			if (cnt_ch % chunks_save == 0) {
				CloseHandle(f_monitoreo_out_laser_f);
			}
		}
		if (mon_edfa_save) {
			bytesw = 0;

			if (cnt_ch % chunks_save == 1) {

				strcpy(nombre_mon_edfa, dire_mon_edfa);
				strcat(nombre_mon_edfa, "/");
				sprintf(num_str, "%06d", cnt_ch_save_e);
				strcat(nombre_mon_edfa, num_str);
				strcat(nombre_mon_edfa, ".mon");

				f_monitoreo_out_edfa_f = CreateFile(
					nombre_mon_edfa, // name of the file
					GENERIC_WRITE,   // open for writing
					0, // sharing mode, none in this case
					0, // use default security descriptor
					CREATE_ALWAYS, // overwrite if exists
					FILE_ATTRIBUTE_NORMAL, 0);

				cnt_ch_save_e++;
			}

			WriteFile(f_monitoreo_out_edfa_f,
				  &energia_avg_r_edfa[0],
				  energia_avg_size_r * sizeof(float), &bytesw,
				  NULL);
			FlushFileBuffers(f_monitoreo_out_edfa_f);

			if (cnt_ch % chunks_save == 0) {
				CloseHandle(f_monitoreo_out_edfa_f);
			}
		}

		WriteFile(paip, &super_vector[0],
			  sizeof(float) * super_vector_size, &leido_paip, NULL);

		chunk_control++;
		if (chunk_control == CHUNKS) { // why??? deberia ser chunks??
			chunk_actual = chkBuffer;
			chunk_control = 0;
		} else {
			chunk_actual +=
				bins * nShotsChk * 2; // avanzo 2 channels
		}
	}

	if (mon_laser_save) {
		CloseHandle(f_monitoreo_out_laser_f);
	}
	if (mon_edfa_save) {
		CloseHandle(f_monitoreo_out_edfa_f);
	}
	CloseHandle(paip);
	free(super_vector);
	free(energia_avg_r_laser);
	free(energia_avg_r_edfa);
	free(energia_laser);
	free(energia_edfa);
	free(mean_flt);
	free(mean_dbl);

	salir_monitoreo = true;
	printf("Hilo Monitoreo terminado \n");
	return NULL;
}

void *
procesa_FFT(void *n) // acepta solamente un th_Data, se podria mover de lugar
{
	struct th_Data *data = (struct th_Data *)n;
	int nCh = data->nCh;
	int bins = data->bins;
	int qFreq = data->qFreq;
	int nShotsChk = data->nShotsChk;
	int chunk_fft_salteo = data->chunk_fft_salteo;
	int chunk_fft_promedio = data->chunk_fft_promedio;

	// Buffer float para fft
	int n_t = 8;
	fftwf_init_threads();
	fftwf_plan_with_nthreads(
		n_t); /// COMPARAR CONTRA THREADS NORMALES, MKL, IPP, IPP_short

	/// Reservo memoria para salida y entrada del plan
	float *dst_float = malloc(sizeof(float) * bins * nShotsChk);
	fftwf_complex *out = fftwf_malloc(sizeof(fftwf_complex) * bins
					  * (1 + (nShotsChk / 2)));
	float *out_abs = (float *)malloc(sizeof(float) * bins
					 * (1 + (nShotsChk / 2))); // abs fft
	float *out_abs_acc =
		(float *)malloc(sizeof(float) * bins
				* (1 + (nShotsChk / 2))); // abs fft acumulada

	/// Plan p2 fft: datos sin ordenar, luego fftw ordena
	int istride = 1 * bins;
	int ostride = 1;
	int idist = 1;
	int odist = 1 + nShotsChk / 2;
	int nn = nShotsChk;
	fftwf_plan p2 = fftwf_plan_many_dft_r2c(
		1, &nn, bins, dst_float, NULL, istride, idist, out, NULL,
		ostride, odist, FFTW_MEASURE | FFTW_PRESERVE_INPUT);

	short *chunk_actual = chkBuffer;

	DWORD leido_paip;
	HANDLE paip = CreateNamedPipe(
		TEXT("\\\\.\\pipe\\PipeFFT"),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE, // FILE_FLAG_FIRST_PIPE_INSTANCE
					      // is not needed but forces
					      // CreateNamedPipe(..) to fail if
					      // the pipe already exists...
		PIPE_WAIT, 1, bins * sizeof(float) * (1 + (nShotsChk / 2)) * 4,
		bins * sizeof(float) * (1 + (nShotsChk / 2)) * 4,
		NMPWAIT_USE_DEFAULT_WAIT, NULL);

	char command[100];
	snprintf(command, 100, "start python test_fft_1.py %d %d %d ", bins,
		 nShotsChk, qFreq);
	printf(command);
	system(command);

	int cnt_salteo = 0;
	int cnt_promedio = 0;
	int chunk_control = 0;
	while (salir == 0) { /// wait podr�a estar aca

		WaitForSingleObject(callback.semConsumidor[3], INFINITE);

		cnt_salteo++;

		if (cnt_salteo % (chunk_fft_salteo + 1) != 0) {
			continue;
		}
		cnt_salteo = 0;

		/// Casteo sin ordenar: la fftw ordena los datos.
		int i;
		for (i = 0; i < bins * nShotsChk * nCh; i += nCh) {
			dst_float[i / nCh] = (float)chunk_actual[i];
		}

		fftwf_execute(p2);

		// printf("FFT: %f, %f, %f
		// \n",out[2000*10+1][0],out[2000*10+2][0],out[2000*10+3][0]);

		/// Calcula el valor absoluto, se podria pasar a su propia
		/// funcion
		for (i = 0; i < ((nShotsChk / 2) + 1) * bins; ++i) {
			out_abs[i] = out[i][0] * out[i][0]
				     + out[i][1] * out[i][1]; /// paralelizar
			if (isnan(out_abs[i]))
				out_abs[i] = 0.0f;

			out_abs_acc[i] += out_abs[i] / (float)nShotsChk;
		}

		cnt_promedio++;
		if (cnt_promedio % chunk_fft_promedio == 0) {

			for (i = 0; i < ((nShotsChk / 2) + 1) * bins; ++i) {
				out_abs_acc[i] /= chunk_fft_promedio;
			}

			// Poner el pipe
			WriteFile(paip, &out_abs_acc[0],
				  sizeof(float) * bins * (1 + (nShotsChk / 2)),
				  &leido_paip, NULL);

			memset(out_abs_acc, 0,
			       sizeof(float) * bins * (1 + (nShotsChk / 2)));
			cnt_promedio = 0;
		}
		printf("Termine FFT \n");

		chunk_control++;
		if (chunk_control == CHUNKS) { // why??? deberia ser chunks??
			chunk_actual = chkBuffer;
			chunk_control = 0;
		} else {
			chunk_actual +=
				bins * nShotsChk * nCh; // avanzo 2 channels
		}
	}

	CloseHandle(paip);
	free(dst_float);
	free(out_abs);
	free(out_abs_acc);
	salir_fft = true;
	printf("Hilo FFT terminado \n");
	return NULL;
}

int main()
{

	/// CARGO PARAMETROS DE ADQUISICION ARCHIVO config.ini
	FILE *file_config =
		fopen("config.ini", "r"); /* should check the result */
	char line[200];
	char line_pozos[200];

	const char *P1_s = "'";
	const char *P2_s = "'";
	const char *P1_i = "[";
	const char *P2_i = "]";

	char *target = NULL;
	char *start, *end;

	struct th_Data datos_thread;
	bool calcula_fft = false;
	bool calcula_osc = false;
	int nro_pozos = 0;
	int chunksPorPozo;
	short nSubChk;
	int window_time_osc;
	int cant_curvas = 0;

	datos_thread.infinite_daq = true;

	while (fgets(line, sizeof(line), file_config)) {
		printf("%s", line);

		/// Busco string
		if (start = strstr(line, P1_s)) {
			start += strlen(P1_s);
			if (end = strstr(start, P2_s)) {
				target = (char *)malloc(end - start + 1);
				memcpy(target, start, end - start);
				target[end - start] = '\0';
			}
		}

		if (start = strstr(line, P1_i)) {
			start += strlen(P1_i);
			if (end = strstr(start, P2_i)) {
				target = (char *)malloc(end - start + 1);
				memcpy(target, start, end - start);
				target[end - start] = '\0';
			}
		}

		parse_th_config(&datos_thread, line, target);

		if (strstr(line, "NSubChk:")) {
			nSubChk = atoi(target);
		}
		for (int i = 0; i < nro_pozos; i++) {
			snprintf(line_pozos, sizeof(line_pozos),
				 "Pozo%d:", i + 1);
			if (strstr(line, line_pozos)) {
				datos_thread.name_pozos[i] = target;
			}
		}

		if (strstr(line, "CalculaFFT:")) {
			if (strstr(target, "si")) {
				calcula_fft = true;
			}
		}

		if (strstr(line, "CalculaOSC:")) {
			if (strstr(target, "si")) {
				calcula_osc = true;
			}
		}
		target = NULL;
	}
	fclose(file_config);

	if (datos_thread.nShots > 0) {
		datos_thread.infinite_daq = false;
	}
	/// FIN DE LA CARGA
	if (datos_thread.nShotsChk % nSubChk != 0) {
		printf("Error: NShotsChk/NSubChk debe ser un numero entero");
		salir = true;
	}

	/// Para los nombre de los archivos de salida
	char fname_dir[40];
	time_t rawtime;
	time(&rawtime);
	struct tm *timeinfo = localtime(&rawtime);
	strftime(fname_dir, 50, "%y_%d_%m_%H_%M_%S", timeinfo);
	mkdir(fname_dir);

	/// Copio archivo de configuraci�n en la carpeta de salida
	char config_file_dir[100];
	char fname_config;
	FILE *source, *dest;
	strcpy(config_file_dir, fname_dir);
	strcat(config_file_dir, "/config.ini");
	source = fopen("config.ini", "r");
	dest = fopen(config_file_dir, "w");
	while ((fname_config = fgetc(source)) != EOF)
		fputc(fname_config, dest);

	fclose(source);
	fclose(dest);

	/// Window time de monitoreo
	datos_thread.window_mon_time = datos_thread.window_time; // Th_Data

	/// Aloca memoria del buffer
	chkBuffer = malloc((long long)datos_thread.bins * datos_thread.nShotsChk
			   * datos_thread.nCh * CHUNKS * sizeof(short));
	// long tamanio=5L*1024L*1024L*1024L;procesa_FFT
	printf("Puntero chunks reservado: %p.\n", chkBuffer);

	tssBuffer = malloc(CHUNKS * sizeof(SYSTEMTIME));
	// long tamanio=5L*1024L*1024L*1024L;
	printf("Puntero tss reservado: %p.\n", chkBuffer);

	/// Struct con parametros de los threads
	struct th_Data datos_thread; // crea una estructura del tipo th_data

	/*llama la funci�n adquirir desde donde se lanzan el productor y se
	 * setea el callback*/
	adquirir(datos_thread.nCh, datos_thread.qFreq, 1, 1, datos_thread.bins,
		 datos_thread.nShotsChk, nSubChk, datos_thread.delay);
	/*lanza el proceso procesaDTS_01 (el consumidor)*/
	DWORD rawID, stdID, monID, fftID, oscID;
	_beginthreadex(NULL, 0,
		       (unsigned int(__stdcall *)(void *))raw_data_writer,
		       (void *)&datos_thread, 0, (unsigned int *)&rawID);
	_beginthreadex(NULL, 0, (unsigned int(__stdcall *)(void *))procesa_STD,
		       (void *)&datos_thread, 0, (unsigned int *)&stdID);
	if (datos_thread.nCh > 1) {
		_beginthreadex(
			NULL, 0,
			(unsigned int(__stdcall *)(void *))procesa_Monitoreo,
			(void *)&datos_thread, 0, (unsigned int *)&monID);
	}
	if (calcula_fft) {
		_beginthreadex(
			NULL, 0,
			(unsigned int(__stdcall *)(void *))procesa_FFT_banda,
			(void *)&datos_thread, 0, (unsigned int *)&fftID);
	}
	if (calcula_osc) {
		_beginthreadex(
			NULL, 0, (unsigned int(__stdcall *)(void *))procesa_osc,
			(void *)&datos_thread, 0, (unsigned int *)&oscID);
	}

	/*--------------------------*/

	while (!kbhit() || getch() != 's')
		sleep(2);

	salir = true;

	while (salir_raw == 0)
		Sleep(1);
	while (salir_std == 0)
		Sleep(1);
	if (datos_thread.nCh > 1) {
		while (salir_monitoreo == 0)
			Sleep(1);
	}
	if (calcula_fft) {
		while (salir_fft == 0)
			Sleep(1);
	}
	if (calcula_osc) {
		while (salir_osc == 0)
			Sleep(1);
	}

	CloseHandle(rawID);
	CloseHandle(stdID);
	if (datos_thread.nCh > 1) {
		CloseHandle(monID);
	}
	CloseHandle(fftID);
	CloseHandle(oscID);

	free(chkBuffer);
	free(tssBuffer);
	WD_Release_Card(0);

	return 0;
}