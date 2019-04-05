# RADAS

## Estructura de archivos Hasta contar con mejores nombres (.py implícito):

__test_marco1_m__ -> main

__func_aux_main__ -> definiciones auxiliares para main

__funciones_data__ -> Script para recibir y formatear datos del equipo DAS

__funciones_grafico__ -> Script para mostrar waterfall (y otros) localmente

__func_alarmas__ -> Script para calcular alarmas con z_scores

__funciones_alarmas__ -> definiciones auxiliares usadas por func_alarmas

__funciones_bokeh__ -> SCript para publicar waterfall web

__func_opc__ -> Script para servidor OPC

## Flags main:

En los _switch_ (bokeh, OPC, alarmas) activan o desactivan estas funciones. Source_DAS y timestamp_DAS pueden ponerse en False para testear otra parte del código con datos sintéticos. OPC_alert en True indica que las variables OPC se prenden automáticamente. save_figure regula el guardado de las figuras y mail_send el envío de correos.

## Muevo para aca los logs

/// (2.8.2) Cambios respecto a version 2.8.1:
/// En Thread STD: agrego la posibilidad de hacer la STD Normalizada respecto al moving average del perfil (agrego parametro de entrada window_bin_mean)
/// En Thread FFT: si se adquiere un solo canal la FFT no explota (estaba hecha para cuando se adquirian dos canales). Se explicita nCh
/// En Thread Raw_Data: Se imprime la velocidad de escritura para monitoreo
/// En Thread Monitoreo: Se explicita el nCh en la cuenta (medio al dope porqe si nCh = 1 el thread no se ejecuta)
/// En Main: Si nCh = 1, el thread monitoreo no se ejecuta.
/// Cambio funcion moving1avg: caso window = 1
/// La apertura de los datos en python se realiza con las funciones especificas puestas en funciones2.py

/// (2.9.2) Cambios respecto a version 2.8.2:
/// Se puede grabar independientemente los dos canales Ch0 y Ch1

/// (2.9.4) Cambios respecto a version 2.9.3:
/// El copiado del buffer se hace con la funcion memcpy_s que avisa cuando hay error en el copiado.

/// (2.9.5) Cambios respecto a version 2.9.4:
/// Divide la FFT por nShotsChk. El pipe de visualizacion llama fft_test_1 en vez de fft_test. fft_test_1 es gui.

/// (2.9.6) Cambios respecto a version 2.9.5:
/// Escribe los archivos de salida en archivos de menor tamano para evitar perder los datos en caso de corte imprevisto de energia durante la adquisicion.
/// Se agrega parametro de guardado ChunksSave en config.ini: cantidad de chunks que se guardan por archivo.

/// (2.9.7) Cambios respecto a version 2.9.6:
/// Escribe los archivos de salida partidos en tamanos chicos y los pone en distintas carpetas segun el tipo de dato. Por cada carpeta (o tipo de dato)
/// escribe un header que contiene la informacion necesaria para abrir los archivos.
/// La apertura de los datos en python se realiza con las funciones especificas puestas en funciones3.py (las tengo que escribir todavia)
/// Carpetas:
/// -RAW_CH0: dato crudo del canal 0
/// -RAW_CH1: dato crudo del canal 1
/// -RAW_CH: dato crudo de los canales 0 y 1 en un mismo archivo
/// -STD: dato procesado
/// -MON_LASER: monitoreo de la estabilidad del laser
/// -MON_EDFA: monitoreo de la estabilidad del pulso amplificado por el edfa
/// -TIME: tiempo de guardado de cada chunk en el HD
/// Agrego el par�metro Delay al header (me lo habia olvidado)
///
/// (3.0) Cambios respecto a version 2.9.7:
/// 17/08/2017: Se castea el chkBuffer como long long. En las versiones anteriores se verificaba error cuando el chkBuffer superaba el tama�o de un long (4 bytes). Ahora el chkBuffer puede superar ese valor.
/// 17/08/2017: Se verifica error de la placa cuando el tama�o del buffer de la placa supera los 128MS. NO RESUELTO.
///
/// (3.1) Cambios respecto a version 3.0:
/// 26/08/2017: Se agrega otro parametro a la adquisici�n nSubChunk para resolver las limitaciones en el tama�o del buffer de la placa (ver ultimo punto de la version anterior). Este par�metro es un numero entero
///             que divide el chunk de nShotsChk en particiones peque�as de tama�o nShotsChk/nSubChunk. Cada chunk ahora esta formado de nSubChunk pedazos. El semaforo se activa cuando se llena todo un chunk (es decir todos los nSubChunk).
///             Se modifican el callback y la funci�n adquirir, que recibe un nueva variable de entrada (nSubChunk).
///
/// (3.2) Cambios respecto a version 3.1:
/// 27/08/2017: Se agrega la posibilidad de guardar dato crudo en un rango de bines. Para esto se agregan dos nuevos par�metros.
///             - Bins_raw: bins que se escriben de dato crudo
///             - Delay_raw: delay para el dato crudo. El Delay_raw es respecto de la adquisici�n. Es decir el delay total de la escritura del dato crudo es Delay + Delay_raw
///
/// (4.0) Cambios respecto a version 3.2:
/// 29/09/2017: Rehace la cuenta para calcular la STD de manera mas eficiente, la calcula a medida que recorre el chunk en orden. De esta manera el proceso STD no se atrasa cuando se adquiere gran cantidad de bins
///             Esto se debe cuantificar.
