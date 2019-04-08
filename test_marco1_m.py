# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt

import collections


import func_opc
import func_alarmas
import funciones_grafico as func_plot
import funciones_data as das_data

from func_aux_main import *

if __name__ == '__main__':


    #Setup flags
    bokeh_switch=False

    source_DAS=True
    time_stamp_DAS=True#regula la creacion de log para los ts reportados por el DAS

    alarmas_switch=True

    OPC_switch=True 
    OPC_alert=False #regula si se notifica automaticamente la alarma

    save_figure=True

    mail_send=True#regula el envio de mails


    #flag para regular envio de mails via opc o timer *not implemented* 


    #Setup values
    
    if source_DAS: #valores que vienen del main para escalar espacialmente datos DAS
        delta_x = delta_x()
        print delta_x, "metros por bin"
    else:
        delta_x=2.02337 #delta_x manual

    
    plots_per_second=0.4 #RENOMBRAR/RECALCULAR indica cuanto acumula en tiempo una fila del waterfall

    #Setup queues
    
    buffer_q=collections.deque() 
    qZscores=None#DECISION quasi-POLEMICA, lo necesita data_gen
    if alarmas_switch:
        qZscores=collections.deque()
        if OPC_switch:
            report_queue=collections.deque()#DECISION POLEMICA-alarmas obliga que alguien consuma report_queue (OPC sv por ej). acoplar si estan acoplados, desacoplar si no
        else:
            report_queue={}
    #Thread Launch
    
    qbokeh=None
    if bokeh_switch:
        import funciones_bokeh as bkh
        bokeh_th=th_launch(bkh.run_server,[])
        qbokeh=bkh.bokeh_queues
        monitor_th=th_launch(bkh.queue_Monitor,[])



    pipe_handler,bins_plot,dtype_size,ts_size,ts_file=data_gen_parameters(source_DAS,time_stamp_DAS)


    if alarmas_switch:

        zonas_opc=18
        factor_zoneo=41
        #zonas=zonas_opc*factor_zoneo#confuso rango de las zonas
        
  
        #intervencion silenciar principio. no respeta tamanio para opc
        zonas=680
        bin_inicio=1872
        ancho_zona=4
        bin_fin=bin_inicio+zonas*ancho_zona

        umbrales={25:0.3,75:0.25}
        ventana_alarma=300
        base_inicio=0
        base_fin=None #tal vez innecsario con el mecanismo de base larga
        print "Alarmas con: ", umbrales, "en ", ventana_alarma, " filas de std", 'bin inicio: ',bin_inicio, ' hasta ',bin_fin

        argumentos_alarma=[bins_plot,ventana_alarma,zonas,umbrales,(bin_fin-bin_inicio)/zonas,'base.std',bin_inicio,bin_fin,base_inicio,base_fin,qZscores,report_queue,factor_zoneo,mail_send,delta_x]
        alarmas_th=th_launch(func_alarmas.alarmas_queue,argumentos_alarma)

    if OPC_switch:
        opc_th=th_launch(func_opc.server_opc,[zonas_opc,report_queue,OPC_alert])


    data_arguments=[pipe_handler,bins_plot*dtype_size,ts_size,source_DAS,time_stamp_DAS,bokeh_switch,ts_file,buffer_q,bins_plot]
    data_th=th_launch(das_data.data_generation,data_arguments,kw={'qZscores':qZscores,'qbokeh':qbokeh})

    plot_anim,ref_slider,ref_button=func_plot.plot_setup(save_figure,buffer_q,bins_plot,delta_x,plots_per_second)
                      

    plt.show()
