# -*- coding: utf-8 -*-

import win32file
import sys
import numpy as np
import time,datetime
import collections,threading

import matplotlib
matplotlib.rcParams.update({'font.size':16})

import matplotlib.pyplot as plt
#from matplotlib.widgets import TextBox
import matplotlib.animation as animation

from functools import partial

bandas_actuales=[1,20,70,140]
ultimas_graficadas=bandas_actuales[:]#[:]copia array
ancho_banda=10
banda_max_valida=2500-ancho_banda
fft_q=collections.deque()
block_size=3 
rows_plot=120

fake_data=False

if not fake_data:
    bins=int(sys.argv[1])
    nShotChk=int(sys.argv[2])
    bin_inicio=1620
    bin_fin=5320
else:
    bins=3500
    bin_inicio=0
    bin_fin=3500


fft_save=True
if fft_save:
    freq_to_save=200
    fft_save_q=collections.deque()



def filename_now(path,ext):
    timeStamp=datetime.datetime.fromtimestamp(time.time()).strftime('%Y_%m_%d__%H_%M_%S')
    nombreArchivo=path+timeStamp+ext
    return nombreArchivo

def init_file(filename):
    fid=open(filename,'wb')
    return fid

def fft_saver():

    saved_fft=0
    fft_per_file=1000
    fft_file=init_file(filename_now('fft/','.fft'))

    while True:
        if len(fft_save_q)>0:
            data_to_save=fft_save_q.popleft()
            data_to_save.tofile(fft_file)
            saved_fft=saved_fft+1
            if saved_fft==fft_per_file:
                saved_fft=0
                fft_file.close()
                fft_file=init_file(filename_now('fft/','.fft'))

        else:
            time.sleep(0.1)

def fft_por_bandas(fft_data,fft_queue): #fft_Data viene reshapeada
    res={}
    for b in bandas_actuales:#b es una banda válida verificada antes de ser banda actual
        res[b]=np.sum(fft_data[b:b+ancho_banda,bin_inicio:bin_fin],axis=0)
    fft_queue.append(res)

def fake_data_gen(fft_queue): #fft_Data viene reshapeada
    res={}
    for b in bandas_actuales:#b es una banda válida verificada antes de ser banda actual
        res[b]=np.random.uniform(high=100,size=bins)
    fft_queue.append(res)


def windows_pipe(pipe_name):
    return win32file.CreateFile("\\\\.\\pipe\\"+pipe_name,
                              win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                              0, None,
                              win32file.OPEN_EXISTING,
                              0, None)

def fft_pipe_thread():
    global fft_q



    if not fake_data:
        tam=bins*(nShotChk/2+1)*4
        pipe_FFT=windows_pipe('PipeFFT')

    while True:
        if not fake_data:
            error,vec=win32file.ReadFile(pipe_FFT,tam)
            vec=np.frombuffer(vec,dtype=np.float32)
            fft_data=vec.reshape(nShotChk/2+1,-1,order='F')#ACA está la matriz con la fft
            fft_por_bandas(fft_data,fft_q)#cargo bandas
            if fft_save:
                fft_save_q.append(fft_data[:freq_to_save,bin_inicio:bin_fin])
        else:
            fake_data_gen(fft_q)
            print "nuevo dato", len(fft_q)
            time.sleep(1)

def get_multi_block(fft_queue,block_size):
    data_b=fft_queue.popleft()
    for _ in range(block_size-1):#el primero lo levante arriba
        tmp_b=fft_queue.popleft()
        for key in tmp_b:
            if key in data_b: #si no esta, lo defino
                data_b[key]=np.vstack([data_b[key],tmp_b[key]])
            else:
                data_b[key]=tmp_b[key]
    return data_b
    
def update_multi_image(images,data_block):
    
    global ultimas_graficadas

    mask=[False]*len(bandas_actuales)
    
    for i,(b1,b2) in enumerate(zip(bandas_actuales,ultimas_graficadas)):
        if b1!=b2:
            mask[i]=True
            ultimas_graficadas[i]=b1
    
    for image,band,mask in zip(images,ultimas_graficadas,mask):
        if mask:
            image.set_data(np.zeros((rows_plot,bin_fin-bin_inicio)))
        if band in data_block:
            prev_data=image.get_array()
            rows_to_update=data_block[band].shape[0]
            new_data=np.append(prev_data[rows_to_update:,:],data_block[band],axis=0)
            image.set_data(new_data)


def random_mat(n=10,m=10):
    return np.abs(np.random.uniform(high=0.4,size=(n,m)))

def create_subplots(bins_plot,rows):
    fig, axs = plt.subplots(2, 2)#, sharex=True, sharey=True)
    #fig.subplots_adjust(left=0.1, bottom=0.01,right=0.99,top=0.99,hspace=0.05,wspace=0.05)
    fig.canvas.set_window_title('FFT por bandas')

    im1=axs[0,0].matshow(random_mat(rows,bins_plot),aspect='auto')
    im2=axs[0,1].matshow(random_mat(rows,bins_plot),aspect='auto')
    im3=axs[1,0].matshow(random_mat(rows,bins_plot),aspect='auto')
    im4=axs[1,1].matshow(random_mat(rows,bins_plot),aspect='auto')

    #ACA CLIM



    imgs_band=[im1,im2,im3,im4]

    def cambiar_freq(banda,text):
        global bandas_actuales
        try:
            freq=int(text)
        except:
            return
        if freq>=banda_max_valida:
            print "mala freq"
            #return
        else:
            bandas_actuales[banda]=freq
    
    widgets=[]


#    axbox = plt.axes([0.05, 0.8, 0.06, 0.04])
#    text_box = TextBox(axbox, 'Banda1', initial=str(bandas_actuales[0]))
#    text_box.on_submit(partial(cambiar_freq,0))
#
#    axbox2 = plt.axes([0.05, 0.6, 0.06, 0.04])
#    text_box2 = TextBox(axbox2, 'Banda2', initial=str(bandas_actuales[1]))
#    text_box2.on_submit(partial(cambiar_freq,1))
#
#    axbox3 = plt.axes([0.05, 0.4, 0.06, 0.04])
#    text_box3 = TextBox(axbox3, 'Banda3', initial=str(bandas_actuales[2]))
#    text_box3.on_submit(partial(cambiar_freq,2))
#
#    axbox4 = plt.axes([0.05, 0.2, 0.06, 0.04])
#    text_box4 = TextBox(axbox4, 'Banda4', initial=str(bandas_actuales[3]))
#    text_box4.on_submit(partial(cambiar_freq,3))
#    widgets=[text_box,text_box2,text_box3,text_box4]
    return imgs_band,widgets,fig



def plot_setup_fft(data_queue,bins_plot):
    
    block_size_original=block_size



    # Adjust the subplots region to leave some space for the sliders and buttons


    imgs_fft,widgets,fig=create_subplots(bins_plot,rows_plot)#resultado de imshows (img_bands)

        #    plot_image=ax.imshow(np.zeros((rows_plot,bins_plot)),cmap='jet',aspect='auto',vmax=0.2)#,interpolation= 'none')
        #   cbar=fig.colorbar(plot_image)
        #   tx=ax.set_title('Datos ducto costero')

                # def update_clim_max(val):
                #     plot_image.set_clim(0,val)


                # amp_slider_ax  = fig.add_axes([0.7, 0.025, 0.05, 0.03])
                # amp_slider_max = Slider(amp_slider_ax, 'Color max', 0.02, 0.5, valinit=0.2)
                # amp_slider_max.on_changed(update_clim_max)


    def update_plot(frame):
        global block_size,fft_q #lo necesita global para cambiarlo
        if len(fft_q)>=block_size:
            new_block=get_multi_block(fft_q,block_size)
            update_multi_image(imgs_fft,new_block)
    #            ax.set_title(ts_to_stringDate(last_ts)[5:-3])
            if len(fft_q)>10*block_size:
                block_size=min(block_size*2,rows_plot/2)
                print 'Recalculado block size a ',block_size
            else: 
                block_size=max(block_size_original,block_size/4)
                #print 'Recalculado block size a ',block_size
                # if save_figure:
                #     tActual=time.time()
                #     if (tActual-tInicial)>segundosEsperaCap:
                #         tInicial=time.time()
                #         filename=ts_to_stringDate(last_ts).replace(" ","_").replace(":","_").replace("/","_")
                #         fig.savefig(path_Guardado+filename+'.jpg',dpi=100,transparent=True)




    plot_anim=animation.FuncAnimation(fig, update_plot, interval=50)



    return widgets,plot_anim#,amp_slider_max #devuelve referencia a la animacion y los botones para que nos sea garbagecollected (documentado en mpl)


if fft_save:
    fft_save_thread=threading.Thread(target=fft_saver)
    fft_save_thread.setDaemon(True)
    fft_save_thread.start()


fft_thread=threading.Thread(target=fft_pipe_thread)
fft_thread.setDaemon(True)
fft_thread.start()


widgets,plot_anim=plot_setup_fft(fft_q,bin_fin-bin_inicio)
plt.show()