# -*- coding: utf-8 -*-

from bokeh.server.server import Server

from functools import partial
from random import random
from threading import Thread
import time

import numpy as np
from bokeh.models import ColumnDataSource
from bokeh.plotting import curdoc, figure
from bokeh.models.widgets import Slider
from bokeh.layouts import row, widgetbox,column
from bokeh.models.ranges import Range1d

from tornado import gen
import datetime
import collections,threading


bokeh_queues={}
qLock=threading.Lock()

def get_block_q(block_size,queue):
	block=[]
	for i in range(block_size):
		vec,ts=queue.popleft()
		block.append(vec)
	return np.asarray(block),ts

def z_binning_vect(data,window,win_cant):
	binned_matrix=data[:win_cant*window].reshape(-1,win_cant,order='F')
	return binned_matrix.mean(0)

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

def funcion_documento(doc):
	source = ColumnDataSource(data=dict(img=[],y=[]))

	bin_inicio=1680
	bin_fin=5180
	bines=493
	lineas_vista=60
	vmin=0.01
	vmax=0.2

	p = figure(x_range=(0, bines), y_axis_location="right",toolbar_location="left",active_scroll="wheel_zoom",plot_width=1000,plot_height=800)
	
	imagen=p.image(image='img',x=0, y='y', dw=bines, dh=1 ,palette="Plasma256",source=source)
	
	minColor = Slider(title="vMIN", value=vmin, start=-0.0, end=0.5,step=0.01,height=50)
	maxColor = Slider(title="vMAX", value=vmax, start=-0.0, end=0.5,step=0.01, height=50)
	
	p.yaxis.axis_label = "Tiempo"
	p.xaxis.axis_label = "Puntos"
	p.xaxis.axis_label_text_font_size = "18pt"
	p.yaxis.axis_label_text_font_size = "18pt"
	p.title.text='Random Data(Click en reset (a la izquierda) para seguir los datos)'
	p.title.text_font_size='20pt'

	p.y_range.follow="start"
	p.y_range.follow_interval=lineas_vista
	
	color_scale={'vmin':vmin,'vmax':vmax}
	def vminChange(attr,old,new):
		color_scale['vmin']=minColor.value
	minColor.on_change('value',vminChange)
	def vmaxChange(attr,old,new):
		color_scale['vmax']=maxColor.value
	maxColor.on_change('value',vmaxChange)
	
	inputs = widgetbox(minColor,maxColor)

	doc.add_root(column(p,inputs))# inicializa el grafico

	@gen.coroutine # if your next tick callback is a Tornado coroutine it wont run the callback again until all the async work is done [bokeh issue 3702].
	def update_img(img,t,v_min,v_max,last_ts):
		source.stream(dict(img=[img],y=[t]),25)#magic rollover
		imagen.glyph.color_mapper.low = v_min
		imagen.glyph.color_mapper.high = v_max
		p.title.text=ts_to_stringDate(last_ts)

	def worker():
		block_size=10
		myQ=collections.deque()
		qLock.acquire()
		bokeh_queues[int(time.time()*1000000)]=myQ
		qLock.release()
		t=0
		while True:
			if(len(myQ)>block_size):
				block_vect,last_ts=get_block_q(block_size,myQ)
				data_update=z_binning_vect(np.mean(block_vect,axis=0)[bin_inicio:bin_fin],(bin_fin-bin_inicio)/bines,bines)
				doc.add_next_tick_callback(partial(update_img, img=data_update.reshape(1,bines),t=t,v_min=color_scale['vmin'],v_max=color_scale['vmax'],last_ts=last_ts)) # simula un nuevo trigger del callback
				t=t-1
			else:
				time.sleep(1)
		print "termina th: "
	blk=threading.Thread(target=worker)	
	blk.start()

# Setting num_procs here means we can't touch the IOLoop before now, we must
# let Server handle that. If you need to explicitly handle IOLoops then you
# will need to use the lower level BaseServer class.
server = Server({'/': funcion_documento},allow_websocket_origin=["*"])
server.start()

def run_server():
    print('Empieza web')
    server.io_loop.add_callback(server.show, "/")
    server.io_loop.start()

if __name__ == '__main__':
    lalal=threading.Thread(target=run_server)
    lalal.start()
    asd=np.random.uniform(0,1,size=100)
    plt.plot(asd)
    plt.show()
