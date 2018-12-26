
# -*- coding: utf-8 -*-
import numpy as np

import collections,bisect,glob,time,os,re

def nombre_avg_2(archivo):
    return archivo[:-3]+'avg'

def nombre_avg(archivo,same=False):
    if same:
        return nombre_avg_2(archivo)
    return '../AVG/'+archivo[:-3]+'avg'


def cargar_base_larga(archivos_std,bins,bin_inicio=0,bin_fin=None,norm=True,ancho_zona=4):
    imagen_std_actual=cargar_archivo(archivos_std[0],bins,bin_inicio,bin_fin,norm,same=True)#cargo el primero
    filas=imagen_std_actual.shape[0]    
    dato_z=zonear(imagen_std_actual,None,ancho_zona,filas)#asume multiplo de ancho_Zona por eso none    
    media_z=np.mean(dato_z,axis=0)
    varianza_z=np.var(dato_z,axis=0,ddof=1)    
    for archivo in archivos_std[1:]:#cargo los restantes
        dato_z=zonear(cargar_archivo(archivo,bins,bin_inicio,bin_fin,norm,same=True),None,ancho_zona,filas)        
        media_z+=np.mean(dato_z,axis=0)
        varianza_z+=np.var(dato_z,axis=0,ddof=1)    
    return media_z/(len(archivos_std)),np.sqrt(varianza_z)/np.sqrt(len(archivos_std))

def cargar_archivo(archivo,bins,bin_inicio,bin_fin,norm,same=False):
    if norm:
        wat=np.reshape(np.fromfile(archivo,dtype=np.float32),(-1,bins))[:,bin_inicio:bin_fin]/np.reshape(np.fromfile(nombre_avg(archivo,same),dtype=np.float32),(-1,bins))[:,bin_inicio:bin_fin]
        return wat
    else:
        return np.reshape(np.fromfile(archivo,dtype=np.float32),(-1,bins))[:,bin_inicio:bin_fin]
def z_binning_vect(data,window):
	if data.shape[0]%window==0:
		ultimo_bin=None
	else:
		ultimo_bin=int(data.shape[0]/window)*window

	binned_matrix=data[:ultimo_bin].reshape(-1,data.shape[0]/window,order='F')
	return binned_matrix.mean(0)

def zonear(data,ultima_multiplo,ancho_zona,filas):
	return np.array([z_binning_vect(data[i,:ultima_multiplo],ancho_zona)for i in range (filas)])

def cargar_linea_de_base(filename_base,bins,bin_inicio,bin_fin,ancho_zona,zonas,base_inicio=0,base_fin=None,norm=True):
	linea_de_base=cargar_archivo(filename_base,bins,bin_inicio,bin_fin,norm)[base_inicio:base_fin,:]
	ultima_multiplo=ancho_zona*zonas
	base_zoneada=zonear(linea_de_base,ultima_multiplo,ancho_zona,linea_de_base.shape[0])
	mean_base=np.mean(base_zoneada,axis=0)
	std_base=np.std(base_zoneada,axis=0)
	return mean_base,std_base



def encontrar_alarmas_live(block_alarma_live,avg,std,umbrales_porcentaje,ventana_alarma,zonas):

	z_score_block=(block_alarma_live-avg)/std #calculo los z_scores
	#filtro con broadcast bool
	
	umbrales_matriz={}
	umbrales_cuenta={}
	matriz_umbrales=np.array([])
	
	for u in umbrales_porcentaje:
	
		umbrales_matriz[u]=np.where(z_score_block>u,1,0)#Matriz de 1 donde supera el umbral, 0 donde no
	
		umbrales_cuenta[u]=np.sum(umbrales_matriz[u],axis=0)#sumo por columna
	
		matriz_umbrales=np.append(matriz_umbrales,np.where(umbrales_cuenta[u]>(umbrales_porcentaje[u]*ventana_alarma),u,0)) #construye matriz con todas las filas por umbral para buscar maximo por filas
		
	matriz_umbrales=matriz_umbrales.reshape((len(umbrales_porcentaje.keys()),zonas))
	primera_fila_alarma=np.max(matriz_umbrales,axis=0)#inicializo la primera fila
	
	return umbrales_matriz,umbrales_cuenta,primera_fila_alarma


def alarma_fila_nueva(fila_nueva_zoneada,umbrales_porcentaje,avg,std,umbrales_matriz,umbrales_cuenta,ventana_alarma,zonas):
	
	z_score_fila=(fila_nueva_zoneada-avg)/std #calculo los z_scores
	#filtro con broadcast 1,0
	
	matriz_umbrales=np.array([])
	
	for u in umbrales_porcentaje:
		fila_nueva_umbrales=np.where(z_score_fila>u,1,0)
		
		fila_a_borrar=umbrales_matriz[u][0,:]
				
		umbrales_matriz[u]=np.vstack([umbrales_matriz[u][1:,:],fila_nueva_umbrales]) #actualizo matriz
	
		umbrales_cuenta[u]=umbrales_cuenta[u]-fila_a_borrar+fila_nueva_umbrales#actualizo cuenta
	
		matriz_umbrales=np.append(matriz_umbrales,np.where(umbrales_cuenta[u]>(umbrales_porcentaje[u]*ventana_alarma),u,0)) #construye matriz con todas las filas por umbral para buscar maximo por filas
		
	matriz_umbrales=matriz_umbrales.reshape((len(umbrales_porcentaje.keys()),zonas))
	return np.max(matriz_umbrales,axis=0)


def is_zone_silenced(zone,silence_dict):
    return any([z in silence_dict for z in [zone-1,zone,zone+1]])#para ver si la zona está silenciada veo si hay una zona aledaña silenciada


def load_silence_file(fname,silence_dict):
    with open(fname,"r") as f:
        zones,silence_hrs=f.readline().split(" ")#elem 0 =zonas, elem 1=horas a silenciar
    silence_hrs=min(24,float(silence_hrs))#maximo 24 hrs de silence
    if '-' in zones:#caso rango
        rango=zones.split('-')
        start=int(rango[0])
        end=int(rango[1])
        print "silenciando desde ",start, " hasta ", end
        for z in range(start,end+1):
            silence_zone(z,silence_dict,time_ext_seg=silence_hrs*60*60)
    if ',' in zones:#caso enumeracion
        zones_to_silence=zones.split(',')
        print "silenciando ", zones_to_silence
        for z in zones_to_silence:
            silence_zone(int(z),silence_dict,time_ext_seg=silence_hrs*60*60)
    os.remove(fname)

        

def silence_zone(zone,silence_dict,time_ext_seg=30*60):#30*60 es lo minimo que _tiene sentido_ silenciar
    silence_dict[zone]=time.time()+time_ext_seg

def zone_watcher(silence_dict,check_freq=30):
    while(True):
        time.sleep(check_freq)
        for zone,deadline in silence_dict.items():
            now=time.time()
            if now>deadline:
                del silence_dict[zone] #se agoto el deadline
        
        archivos_a_silenciar=glob.glob("//Sscrdcrapl04/silenciarRA/*.silence")
        if archivos_a_silenciar!=[]:
            for f in archivos_a_silenciar:
                try:
                    load_silence_file(f,silence_dict)
                except:
                    print "error carga ", f, ". Sera eliminado"
                    os.remove(f)

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


def filename_mail(ts):
    return ts_to_stringDate(ts).replace(':','')[-9:].replace(" ","")+'.jpg'

def filtrar_y_silenciar(zonas_report,last_mail,timer_mail,silence_dict):
    zonas_a_filtrar=[]
    silenciadas=[]
    for i,z in enumerate(zonas_report):

        if is_zone_silenced(z,silence_dict):#si está silenciada, la saco de la lista para mandar mail            
            zonas_a_filtrar.append(i)
        
        if z in last_mail:
            if time.time()-last_mail[z]<1.5*timer_mail:
                silenciadas.append(z)
                silence_zone(z,silence_dict)#si mande mail hace menos de tanto tiempo, la silencio pero no la saco para que mande el 2do mail

    return np.delete(zonas_report,zonas_a_filtrar),silenciadas

def reporte(alarmas,dict_coords,dict_bins,ts,silenciadas):
    texto_reporte=ts+' :'
    if len(silenciadas)>0:
        texto_reporte+='\nSe silenciaron las siguientes zonas: '
        for z in silenciadas:
            texto_reporte+=str(z)+' '
    texto_reporte+="\nZonas con alarma: "
    for zona in alarmas:
        texto_reporte+=str(zona)+' '
    texto_reporte+='\n'
    pares_alarma=closest_zone(dict_bins,alarmas)
    return texto_reporte,texto_zonas_mail(dict_coords,pares_alarma),len(pares_alarma)


