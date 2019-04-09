# -*- coding: utf-8 -*-

import matplotlib
matplotlib.rcParams.update({'font.size':18})
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.animation as animation
from matplotlib.widgets import Button,Slider
import time,datetime,os
import re
import json

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

def valores_copados(vec,factor_de_copadez=50.0):
    return factor_de_copadez*(np.floor(vec/factor_de_copadez) + np.round((vec/factor_de_copadez)-np.floor(vec/factor_de_copadez)))

def get_block(data_queue,block_size):
    block=[]
    for i in range(block_size):
        vec,ts=data_queue.popleft()
        block.append(vec)
    return np.asarray(block),ts
    

def update_image(image,new_rows):
    prev_data=image.get_array()
    rows_to_update=new_rows.shape[0]
    new_data=np.append(prev_data[rows_to_update:,:],new_rows,axis=0)
    image.set_data(new_data)


def parse_markers(archivo_punzados):
    res={}
    pattern='([a-zA-z0-9]+)\s*.\s*(\d+\.?\d*)\s*'
    with open(archivo_punzados) as markers:
        for l in markers.readlines():
            mark=re.findall(pattern,l)
            if mark:
                res[mark[0][0]]=float(mark[0][1])
    return res

block_size=20 #DECISION POLEMICA--global para animation y switch
old_block_size=block_size
tInicial=time.time()
slider_switch=False
plot_marks=[]

new_txt=0     
path_web='\\\Sscrdcrapl04\\Y-TEC\\WF\\web\\'
web_update_timer=time.time()
web_update_seg=15

folder_creation_timer=time.time()
folder_creation_limit_seg=60*60*24

path_Guardado='\\\Sscrdcrapl04\\Y-TEC\\WF\\'

def new_folder_date(path):
    fecha=datetime.datetime.now().strftime("%y-%m-%d-%H-%M-%S")
    res=path+fecha+'\\'
    try:
        os.mkdir(res)
    except:
        print "error creacion de carpeta", res
    return res



def plot_setup(save_figure,data_queue,bins_plot,delta_x,plots_per_second):
    global plot_marks,img_dst
    block_size_original=block_size
    if save_figure:
        segundosEsperaCap=100
        img_dst=new_folder_date(path_Guardado)


    fig,ax=plt.subplots(figsize=(20, 10))
    fig.canvas.set_window_title('Waterfall costero')

    # Adjust the subplots region to leave some space for the sliders and buttons
    fig.subplots_adjust(left=0.25, bottom=0.25)

    rows_plot=400

    plot_image=ax.imshow(np.zeros((rows_plot,bins_plot)),cmap='jet',aspect='auto',vmax=0.2)#,interpolation= 'none')
    cbar=fig.colorbar(plot_image)
    tx=ax.set_title('Datos ducto costero')

    try:
        markers=parse_markers('punzados.txt')
    except:
        markers={}
        print "sin markers"
        

    for label,position in markers.iteritems():
        mark=ax.axvline(x=position/delta_x,color='lightcoral')
        t=ax.text(position/delta_x,rows_plot/2,label,rotation=90,color='hotpink',weight='bold',fontsize=12)
        plot_marks.append(mark)
        plot_marks.append(t)


    def show_lines(event):
        global slider_switch,plot_marks
        for marker in plot_marks:
            marker.set_visible(slider_switch)
        slider_switch=not(slider_switch)

    def update_clim_max(val):
        plot_image.set_clim(0,val)


    amp_slider_ax  = fig.add_axes([0.7, 0.025, 0.05, 0.03])
    amp_slider_max = Slider(amp_slider_ax, 'Color max', 0.02, 0.5, valinit=0.2)
    amp_slider_max.on_changed(update_clim_max)


    button_ax = fig.add_axes([0.85, 0.025, 0.1, 0.04])
    b_lines=Button(button_ax,'Switch marcadores')
    b_lines.on_clicked(show_lines)

    labels_copados=valores_copados(np.arange(0,bins_plot*delta_x,1000*delta_x))
    ax.set_xticks(labels_copados/delta_x)
    ax.set_xticklabels(labels_copados)
    ax.set_yticks(np.arange(0,rows_plot+1,50))
    ax.set_yticklabels(map(str,(np.arange(0,rows_plot+1,50)*plots_per_second)[::-1]*-1))
    ax.set_ylabel('Tiempo[seg]')
    ax.set_xlabel('Posicion[m]')

    ax.fmt_xdata=lambda x: str(x*delta_x)[0:6]+'m - bin:'+str(int(x))
    ax.fmt_ydata=lambda y: str((y-rows_plot)*plots_per_second)[0:5]+'s  STD:'
    fig.tight_layout()

    zoom_inicial_m=3400
    zoom_final_m=10300
    ax.set_xlim(zoom_inicial_m/delta_x,zoom_final_m/delta_x)


    def update_plot(frame):
        global block_size,tInicial,old_block_size,new_txt,folder_creation_timer,web_update_timer,img_dst #lo necesita global para cambiarlo
        
        if len(data_queue)>=block_size:
            new_block, last_ts=get_block(data_queue,block_size)
            line_prom.set_ydata(np.mean(new_block,axis=0))
            ax_prom_hist.figure.canvas.draw()
            update_image(plot_image,new_block)
            #tx.set_text(ts_to_stringDate(last_ts))
            labels = [item.get_text() for item in ax.get_yticklabels()]
            labels[-1]=ts_to_stringDate(last_ts)[5:-3]
            ax.set_yticklabels(labels)
            ax.set_title(ts_to_stringDate(last_ts)[5:-3])
            #print 'Perfiles en espera: ', len(data_queue)
            if len(data_queue)>10*block_size:
                block_size=min(block_size*2,100)
            else: 
                block_size=max(block_size_original,block_size/4)
                #print 'Recalculado block size a ',block_size
            if block_size!=old_block_size:
                print "Cambia actualizacion waterfall a ", block_size, " filas por vez"
                old_block_size=block_size
            if save_figure:
                tActual=time.time()
                if (tActual-tInicial)>segundosEsperaCap:
                    tInicial=time.time()
                    filename=ts_to_stringDate(last_ts).replace(" ","_").replace(":","_").replace("/","_")
                    new_img=img_dst
                    fig.savefig(new_img+filename+'.jpg',dpi=100,transparent=True)

                if(tActual- folder_creation_timer )>folder_creation_limit_seg:
                    folder_creation_timer=time.time()
                    img_dst=new_folder_date(path_Guardado)

                if (tActual-web_update_timer)>web_update_seg:
                    try:
                        fig.savefig(path_web+'imagen_actual.jpg',dpi=100,transparent=True)
                        with open(path_web+"lala.json", "r") as jsonFile:
                            data = json.load(jsonFile)
                        data["lastfile"] = str(new_txt)
                        new_txt=(new_txt+1)%9 #que escriba solo un caracter, es para que actualice la imagen   
                        with open(path_web+"lala.json", "w") as jsonFile:
                            json.dump(data, jsonFile)
                        web_update_timer=time.time()
                    except:
                        print "Falla imagen web", datetime.datetime.now()




    plot_anim=animation.FuncAnimation(fig, update_plot, interval=100)



    fig_prom_hist,ax_prom_hist=plt.subplots(figsize=(20, 10))
    fig_prom_hist.canvas.set_window_title('Ultima std y promedio')

    line_prom,=ax_prom_hist.plot(np.zeros(bins_plot),label='Ultimo std')
    ax_prom_hist.legend()

    return plot_anim,amp_slider_max,b_lines #devuelve referencia a la animacion y los botones para que nos sea garbagecollected (documentado en mpl)
