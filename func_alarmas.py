# -*- coding: utf-8 -*-

from funciones_alarmas import *
from func_aux_main import th_launch

import time,datetime
import numpy as np
import glob,os
import subprocess as sp
import cPickle as pickle
#usa z_binning_vect siempre con multiplos

def alarm_handler(alarmas,report_queue,factor_zoneo,zonas_report,mail_send_ok,dict_coords,dict_bins,ts,logfile,mail_send,data_report,delta_x,plots_per_second,timer_alarm_seg,timer_mail_ok_seg,last_mail,silence_dict,timer_alarm,timer_mail_ok):
    
    if np.any(np.where(alarmas>0)) and report_queue!={}:
        report_queue.append(np.unique(np.where(alarmas>0)[0]/factor_zoneo))#VERIFICAR con OPC AUTOMATICO porque no estan equiespaciadas entonces no es solo dividir
    
    zonas_report=np.append(zonas_report,np.unique(np.where(alarmas>0)[0]))
    
    ahora=time.time()
   
    if zonas_report.size>0 and mail_send_ok:
        
        zonas_mail,silenciadas=filtrar_y_silenciar(np.unique(zonas_report),last_mail,timer_alarm_seg,silence_dict)
        
        txt_file,txt_mail,alarmas_cant=reporte(zonas_mail,dict_coords,dict_bins,ts_to_stringDate(ts),silenciadas)
        
        logfile.write(txt_file)#switchear con log_file=True
        
        if mail_send and zonas_mail.size>0:#quiza se deberia hacer en el primer if el chequeo y antes el filtro_silenciar
                   
            for z in zonas_mail:
                last_mail[z]=ahora

            dict_img={'data':data_report,'delta_x':delta_x,'plots_per_second':plots_per_second,'filename':filename_mail(ts)}
            kargs_mail={'img_data_dict':dict_img,'alarmas_cant':alarmas_cant,'txt_mail':txt_file+txt_mail}
            mail_tmp_file=ts_to_stringDate(ts).replace(':','')[-9:]+'.mail'                    
            with open(mail_tmp_file,'wb') as f:
                pickle.dump(kargs_mail,f,pickle.HIGHEST_PROTOCOL)
            sp.Popen(("python report_mail.py "+mail_tmp_file).split(),stderr=None,stdin=None,stdout=None)#se manda mail
        
        timer_alarm=time.time()
        timer_mail_ok=time.time()
        mail_send_ok=False
        zonas_report=np.array([])
    
    
    if not(mail_send_ok) and (ahora-timer_alarm>timer_alarm_seg):
        mail_send_ok=True
        
    if(ahora-timer_mail_ok)>timer_mail_ok_seg:
        timer_mail_ok=time.time()
        if mail_send:
            dict_img={'data':data_report,'delta_x':delta_x,'plots_per_second':plots_per_second,'filename':filename_mail(ts)}
            kargs_mail={'img_data_dict':dict_img,'txt_mail':("No hubo alarma en las ultimas "+str(timer_mail_ok_seg/60.0/60.0)+" horas.")}
            mail_tmp_file=ts_to_stringDate(ts).replace(':','')[-9:]+'.mail'                    
            with open(mail_tmp_file,'wb') as f:
                pickle.dump(kargs_mail,f,pickle.HIGHEST_PROTOCOL)
            sp.Popen(("python report_mail.py "+mail_tmp_file).split(),stderr=None,stdin=None,stdout=None)#se manda mail
    return timer_alarm,timer_mail_ok,zonas_report,mail_send_ok



def alarmas_queue(bins,ventana_alarma,zonas,umbrales,ancho_zona,filename_base,bin_inicio,bin_fin,base_inicio,base_fin,alarm_queue,report_queue,factor_zoneo,mail_send,delta_x,plots_per_second=0.4,rows_plot=400):
    live_base=False
    try:
        #mean_base,std_base=cargar_linea_de_base(filename_base,bins,bin_inicio,bin_fin,ancho_zona,zonas,base_inicio,base_fin)
        ##BASE LARGA
        base_larga=glob.glob('C:/test base larga/1_madrugada/STD/0*.std')
        base_larga.sort(key=os.path.getmtime)#tal vezno hace falta
        
        mean_base,std_base=cargar_base_larga(base_larga,bins,bin_inicio,bin_fin,ancho_zona=ancho_zona)
    
    except Exception as e:
        print e
        print 'No hay ', filename_base
        live_base=True

    now=datetime.datetime.now().strftime("%Y-%m-%d__%H_%M_%S")
    path_alarmas_log='alarmas_log/'
    logfile=open(path_alarmas_log+now+'.alarms','a')
    logfile.write('Alarmas con base larga '+str(umbrales)+' con ventana '+ str(ventana_alarma) + ' desde '+str(bin_inicio)+' hasta '+ str(bin_fin) + ' con ancho zona= '+str(ancho_zona)+'\n')

    dict_coords=parse_markers_coord('zonas_coord.txt')
    dict_coords['inicio']='del ducto'
    dict_coords['fin']='del ducto' #wtf
    
    dict_bins=build_zone_dict(parse_markers_bins('zonas_bin.txt',bin_inicio),ancho_zona)

    window_count=0
    bloque_inicial=np.array([])
    
    ##INTERVENCION RANGO BINES HASTA MEJOR SOLUCION. todos los rangos deben ser multiplo de ancho zona
    data_report=np.zeros((rows_plot,bins))#buffer para la imagen

    while(window_count<ventana_alarma):
        try:#con un if len positiva tal vez basta
            fila_nueva,ts=alarm_queue.popleft()
            window_count+=1   
        except:#Si no hay elementos en la cola
            time.sleep(plots_per_second*0.8)#0.8 para tardar un poco menos que lo tarda en aparecer un nuevo elemento
            continue

        data_report=np.roll(data_report,-1,axis=0)
        data_report[-1,:]=fila_nueva
                

        fila_nueva=fila_nueva[bin_inicio:bin_fin]
        fila_zoneada=z_binning_vect(fila_nueva[:ancho_zona*zonas],ancho_zona)
        bloque_inicial=np.append(bloque_inicial,fila_zoneada)
    print bloque_inicial.shape,zonas    
    bloque_inicial=bloque_inicial.reshape(-1,zonas)
    print "Bloque inicial lleno. Comienza a detectar"

    if live_base:
        mean_base,std_base=np.mean(bloque_inicial,axis=0),np.std(bloque_inicial,axis=0)
    u_m,u_c,_=encontrar_alarmas_live(bloque_inicial,mean_base,std_base,umbrales,ventana_alarma,zonas)

    timer_alarm_seg=60*4
    timer_alarm=time.time()
    
    timer_mail_ok_seg=60*60*4
    timer_mail_ok=time.time()

    zonas_report=np.array([])
    max_pendiente=0#para ver cuantas std hay en la cola para procesar

    mail_send_ok=True
    
    last_mail={}
    silence_dict={}
    tt=th_launch(zone_watcher,[silence_dict])

    while(True):
        if len(alarm_queue)>0: #deberia ser un wait
            fila_nueva,ts=alarm_queue.popleft()
            
            data_report=np.roll(data_report,-1,axis=0)
            data_report[-1,:]=fila_nueva
            
            fila_nueva=fila_nueva[bin_inicio:bin_fin]
            fila_zoneada=z_binning_vect(fila_nueva[:ancho_zona*zonas],ancho_zona)
            alarmas= alarma_fila_nueva(fila_zoneada,umbrales,mean_base,std_base,u_m,u_c,ventana_alarma,zonas)
    
            timer_alarm,timer_mail_ok,zonas_report,mail_send_ok=alarm_handler(alarmas,report_queue,factor_zoneo,zonas_report,mail_send_ok,dict_coords,dict_bins,ts,logfile,mail_send,data_report,delta_x,plots_per_second,timer_alarm_seg,timer_mail_ok_seg,last_mail,silence_dict,timer_alarm,timer_mail_ok)

            if len(alarm_queue)>max_pendiente:#deberia haber un monitor global de queues y no estar aca xq confunde
                max_pendiente=len(alarm_queue)
                print 'Nuevo maximo de STD pendientes: ', max_pendiente,' -> ', ts_to_stringDate(ts) 
       
                #se manda mail
                
            #zonas_report=np.append(zonas_report,np.unique(np.where(alarmas>0)[0]/factor_zoneo))
        else:
            time.sleep(0.1)
                #actualizar_histograma(alarmas,ts)
        
