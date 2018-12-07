# -*- coding: utf-8 -*-

from funciones_alarmas import *
import time,datetime
import numpy as np
import glob,os
import collections,bisect,re
import subprocess as sp
import cPickle as pickle
#usa z_binning_vect siempre con multiplos


def closest_zone(dict_zona_bin,sub_zone_alarm_list):#asume dict_zona_bin es OrderedDict para bisect, devuelve entre que zonas quedo cada zona de la lista de alarma
    res=[]
    for zone in sub_zone_alarm_list:
        closest_index=bisect.bisect_left(dict_zona_bin.keys(),zone)
        if 0 < closest_index < len(dict_zona_bin.keys()):
            res.append((dict_zona_bin[dict_zona_bin.keys()[closest_index-1]],dict_zona_bin[dict_zona_bin.keys()[closest_index]])) #sintaxis molesta para acceder a los elementos del diccionario
        else:
            if closest_index==len(dict_zona_bin.keys()):#caso fin
                res.append((dict_zona_bin[dict_zona_bin.keys()[closest_index-1]],'fin'))
            else: #caso inicio
                res.append(('inicio', dict_zona_bin[dict_zona_bin.keys()[closest_index]]))
    return list(set(res))#remueve duplicados, pierde orden

def build_zone_dict(bin_dict,sub_zone_size): #pasa de dictionario zone_name->bin a sub_zone -> zona_name. Asume dict llega ordenado
    zone_dict=collections.OrderedDict()# ultima sub zona -> zone_name
    for key,val in bin_dict.iteritems():
        zone_dict[val/sub_zone_size]=key #la ultima subzona que le corresponde a la zona del bin val. asume subzonas suficientemente chicas como para que el margen sea aceptable
    return zone_dict #queda ordenado por zona para recorrerlo con bisect

def texto_zonas_mail(dict_coords,zonas_alarma):#asume que dict_coord tiene definido 'inicio' y 'fin' claves
    texto=':\n'
    #print zonas_alarma
    for st,end in zonas_alarma:
        #print st,end
        tmp='Entre '+st+ ' ( ' + dict_coords[st] + ' ) y ' +end+ ' ( ' + dict_coords[end] + ' ). \n'
        texto+=tmp
    return texto
    


def parse_markers_coord(arch):
    res={}
    pattern='([a-zA-z0-9]+)\s*.\s*(\-*\d+\.*\d+)\s*(\-*\d+\.*\d+)\s*' #match nombre_zona:coord1 coord2
    with open(arch) as markers:
        for l in markers.readlines():
            mark=re.findall(pattern,l)
            if mark:
                res[mark[0][0]]=(mark[0][1])+' ; '+(mark[0][2]) #concatena coordenadas en un solo string
    return res

def parse_markers_bins(arch,bin_inicio): #CODE SMELL codigo repetidisimo
    res=collections.OrderedDict()
    pattern='([a-zA-z0-9]+)\s*.\s*(\d+\.?\d*)\s*'
    with open(arch) as markers:
        for l in markers.readlines():
            mark=re.findall(pattern,l)
            if mark:
                res[mark[0][0]]=int(mark[0][1])-bin_inicio#resto offset para centrar las referencias
    return res

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

def reporte(alarmas,dict_coords,dict_bins,ts):
    texto_reporte=ts+"-> zonas : "
    for zona in alarmas:
        texto_reporte+=str(zona)+' '
    texto_reporte+='\n'
    pares_alarma=closest_zone(dict_bins,alarmas)
    return texto_reporte,texto_zonas_mail(dict_coords,pares_alarma),len(pares_alarma)





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

    while(True):
        if len(alarm_queue)>0: #deberia ser un wait
            fila_nueva,ts=alarm_queue.popleft()
            
            data_report=np.roll(data_report,-1,axis=0)
            data_report[-1,:]=fila_nueva
            
            fila_nueva=fila_nueva[bin_inicio:bin_fin]
            fila_zoneada=z_binning_vect(fila_nueva[:ancho_zona*zonas],ancho_zona)
            alarmas= alarma_fila_nueva(fila_zoneada,umbrales,mean_base,std_base,u_m,u_c,ventana_alarma,zonas)
    


            if np.any(np.where(alarmas>0)) and report_queue!={}:
                report_queue.append(np.unique(np.where(alarmas>0)[0]/factor_zoneo))#VERIFICAR con OPC AUTOMATICO porque no estan equiespaciadas entonces no es solo dividir

            zonas_report=np.append(zonas_report,np.unique(np.where(alarmas>0)[0]))
            ahora=time.time()
            
            if zonas_report.size>0 and mail_send_ok:
                txt_file,txt_mail,alarmas_cant=reporte(np.unique(zonas_report),dict_coords,dict_bins,ts_to_stringDate(ts))
                
                logfile.write(txt_file)#switchear con log_file=True
                
                if mail_send:
                    dict_img={'data':data_report,'delta_x':delta_x,'plots_per_second':plots_per_second,'filename':ts_to_stringDate(ts).replace(':','')[-9:]+'.jpg'}
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
                    dict_img={'data':data_report,'delta_x':delta_x,'plots_per_second':plots_per_second,'filename':ts_to_stringDate(ts).replace(':','')[-9:]+'.jpg'}
                    kargs_mail={'img_data_dict':dict_img,'txt_mail':("No hubo alarma en las ultimas "+str(timer_mail_ok_seg/60.0/60.0)+" horas.")}
                    mail_tmp_file=ts_to_stringDate(ts).replace(':','')[-9:]+'.mail'                    
                    with open(mail_tmp_file,'wb') as f:
                        pickle.dump(kargs_mail,f,pickle.HIGHEST_PROTOCOL)
                    sp.Popen(("python report_mail.py "+mail_tmp_file).split(),stderr=None,stdin=None,stdout=None)#se manda mail
                    
            if len(alarm_queue)>max_pendiente:#deberia haber un monitor global de queues y no estar aca xq confunde
                max_pendiente=len(alarm_queue)
                print 'Nuevo maximo de STD pendientes: ', max_pendiente,' -> ', ts_to_stringDate(ts) 
       
                #se manda mail
                
            #zonas_report=np.append(zonas_report,np.unique(np.where(alarmas>0)[0]/factor_zoneo))
        else:
            time.sleep(0.1)
                #actualizar_histograma(alarmas,ts)
        
