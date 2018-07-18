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
