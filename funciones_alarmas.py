
# -*- coding: utf-8 -*-
import numpy as np



def z_binning_vect(data,window):
	if data.shape[0]%window==0:
		ultimo_bin=None
	else:
		ultimo_bin=int(data.shape[0]/window)*window

	binned_matrix=data[:ultimo_bin].reshape(-1,data.shape[0]/window,order='F')
	return binned_matrix.mean(0)

def zonear(data,ultima_multiplo,ancho_zona,filas):
	return np.array([z_binning_vect(data[i,:ultima_multiplo],ancho_zona)for i in range (filas)])

def cargar_archivo(archivo,bins,bin_inicio,bin_fin,norm):
    if norm:
        return np.reshape(np.fromfile(archivo,dtype=np.float32),(-1,bins))[:,bin_inicio:bin_fin]/np.reshape(np.fromfile((archivo[:-3]+'avg'),dtype=np.float32),(-1,bins))[:,bin_inicio:bin_fin] 
    else:
        return np.reshape(np.fromfile(archivo,dtype=np.float32),(-1,bins))[:,bin_inicio:bin_fin] 



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
