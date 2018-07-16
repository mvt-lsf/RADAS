# -*- coding: utf-8 -*-

from funciones_alarmas import *
import time,datetime
import numpy as np

import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText

#usa z_binning_vect siempre con multiplos

def mail_alerta(body):
    to='dario.kunik@ypftecnologia.com'
    smtp_server="smtp-app-int.grupo.ypf.com"
    
    msg=MIMEMultipart()
    msg['From']=to
    msg['To']=to
    msg['Subject']='alertaDuctoCostero'
    msg.attach(MIMEText(body,'plain'))
    try:
        sv=smtplib.SMTP(smtp_server,25)
        sv.sendmail('',to,msg.as_string())
        print "Enviado mail"
        sv.quit()
    except:
        print "Falla mail"

def reportar(alarmas,ts,logfile=None,mail_alert=False):
    texto_reporte=ts+"-> Hay alarma en las zonas : "
    for zona in alarmas:
        texto_reporte+=str(zona)+' '
    texto_reporte+='\n'
    print texto_reporte
    if logfile:
        logfile.write(texto_reporte)
    if mail_alert:
        mail_alerta(texto_reporte)


def ts_to_stringDate(ts):
    res=""
    for num in map(str,ts[0:3]):
        if len(num)==1:
            res+="0"
        res+=num+"/"
    res=res[:-1]
    res+=" "
    for num in map(str,ts[3:]):
        if len(num)==1:
            res+="0"
        res+=num+":"
    return res[:-1]

def alarmas_queue(bins,ventana_alarma,zonas,umbrales,ancho_zona,filename_base,bin_inicio,bin_fin,alarm_queue,report_queue,factor_zoneo,mail_send,plots_per_second=0.4):
    live_base=False
    try:
        mean_base,std_base=cargar_linea_de_base(filename_base,bins,bin_inicio,bin_fin,ancho_zona,zonas)
    except Exception as e:
        print e
        print 'No hay ', filename_base
        live_base=True

    now=datetime.datetime.now().strftime("%Y-%M-%d__%H_%M_%S")
    logfile=open(now+'.alarms','a')

    window_count=0
    bloque_inicial=np.array([])

    while(window_count<ventana_alarma):
        try:
            fila_nueva,ts=alarm_queue.popleft()
            window_count+=1   
        except:#Si no hay elementos en la cola
            #print e
            print "Llenando bloque inicial de alarma..."
            time.sleep(plots_per_second*0.8)#0.8 para tardar un poco menos que lo tarda en aparecer un nuevo elemento
            continue
        fila_nueva=fila_nueva[bin_inicio:bin_fin]
        fila_zoneada=z_binning_vect(fila_nueva[:ancho_zona*zonas],ancho_zona)
        bloque_inicial=np.append(bloque_inicial,fila_zoneada)
    bloque_inicial=bloque_inicial.reshape(-1,zonas)
    if live_base:
        mean_base,std_base=np.mean(bloque_inicial,axis=0),np.std(bloque_inicial,axis=0)
    u_m,u_c,_=encontrar_alarmas_live(bloque_inicial,mean_base,std_base,umbrales,ventana_alarma,zonas)

    timer_alarm_seg=60
    timer_alarm=time.time()

    zonas_report=np.array([])

    while(True):
        try:
            fila_nueva,ts=alarm_queue.popleft()
        except:
            time.sleep(plots_per_second*0.8)
            continue
        fila_nueva=fila_nueva[bin_inicio:bin_fin]
        fila_zoneada=z_binning_vect(fila_nueva[:ancho_zona*zonas],ancho_zona)
        alarmas= alarma_fila_nueva(fila_zoneada,umbrales,mean_base,std_base,u_m,u_c,ventana_alarma,zonas)
        if np.any(np.where(alarmas>0)):
            report_queue.append(np.unique(np.where(alarmas>0)[0]/factor_zoneo))
            #DECISION POLEMICA-alarmas obliga que alguien consuma report_queue (OPC sv por ej)

        ahora=time.time()
        if (ahora-timer_alarm)>timer_alarm_seg:#reporta cuando se cumple timer_mail que regula tambien cuando escribe el log :S
            if zonas_report.size>0:
                reportar(np.unique(zonas_report),ts_to_stringDate(ts),logfile,mail_send)
                zonas_report=np.array([])
                timer_alarm=time.time()
        zonas_report=np.append(zonas_report,np.unique(np.where(alarmas>0)[0]/factor_zoneo))
            #actualizar_histograma(alarmas,ts)
        print 'alarmas pendientes: ', len(alarm_queue), 'zonas alarmas: ',zonas_report.size
