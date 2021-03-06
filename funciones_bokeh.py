# -*- coding: utf-8 -*-

from bokeh.server.server import Server

from functools import partial
import time

import numpy as np
from bokeh.models import ColumnDataSource
from bokeh.plotting import curdoc, figure
from bokeh.models.widgets import Slider
from bokeh.layouts import row, widgetbox,column
from bokeh.models.ranges import Range1d
from bokeh.models.glyphs import Text
from bokeh.palettes import Spectral
from tornado import gen
import datetime
import collections,threading,re


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
def parse_markers(archivo_punzados):
    res={}
    pattern='([a-zA-z0-9]+)\s*.\s*(\d+\.?\d*)\s*'
    with open(archivo_punzados) as markers:
        for l in markers.readlines():
            mark=re.findall(pattern,l)
            if mark:
                res[mark[0][0]]=float(mark[0][1])
    return res
    
bin_inicio=1680
bin_fin=5180
bines=3500 #DECISION POLEMICA: valores de globales deberian ser params de run_server
quot_bines=(bin_fin-bin_inicio)/bines#pasar parametros
dict_mark=parse_markers('zonas_bin.txt')
markers=dict_mark.keys()
markers_x=[]
for mark,pos in dict_mark.iteritems():
	markers_x.append((pos-bin_inicio)/quot_bines)

def funcion_documento(doc):
	source = ColumnDataSource(data=dict(img=[],y=[]))


#	lineas_vista=40
	lineas_vista=30
	vmin=0.02
	vmax=0.17

	p = figure(x_range=(0, bines), y_axis_location="right",toolbar_location="left",plot_width=1000,plot_height=800)
	p.background_fill_alpha=0
	p.border_fill_alpha=0 
#	p = figure(x_range=(0, bines), y_axis_location="right")

	marks_cant = len(markers)
	r_x = markers_x
	rango_y_text = (-3)*np.ones(marks_cant)
	text_c=markers
	text_c_s=markers#copia para stream porquue hace append y modifica

	ss = ColumnDataSource(dict(x=r_x, y=rango_y_text, text=text_c))
	
	glyph = Text(x="x", y="y", text="text", angle=1.3, text_color="black",text_font_size="9pt")
	p.add_glyph(ss, glyph)

	
	imagen=p.image(image='img',x=0, y='y', dw=bines, dh=1 ,palette='Viridis256',source=source,dilate=True)
	
	minColor = Slider(title="vMIN", value=vmin, start=-0.0, end=0.5,step=0.01,height=50)
	maxColor = Slider(title="vMAX", value=vmax, start=-0.0, end=0.5,step=0.01, height=50)
	
	p.yaxis.axis_label = "Tiempo"
	p.xaxis.axis_label = "Puntos"
	p.xaxis.axis_label_text_font_size = "18pt"
	p.yaxis.axis_label_text_font_size = "18pt"
	p.title.text='Datos ducto costero'
	p.title.text_font_size='20pt'
 	p.xgrid.grid_line_color = None
 	p.ygrid.grid_line_color = None

	p.y_range.follow="end"
	p.y_range.follow_interval=41
	
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
#		source.stream(dict(img=[img],y=[t]),25)#magic rollover
 		source.stream(dict(img=[img],y=[t]),lineas_vista)
		ss.stream(dict(x=r_x,y=rango_y_text+t,text=text_c_s),marks_cant)
		imagen.glyph.color_mapper.low = v_min
		imagen.glyph.color_mapper.high = v_max
		p.title.text=ts_to_stringDate(last_ts)

	def worker():
#		block_size=50
		block_size=15
		myQ=collections.deque()
		bokeh_queues[int(time.time()*1000000)]=myQ
		t=0
		end_th=False
		while not(end_th):
			if(len(myQ)>block_size):
				block_vect,last_ts=get_block_q(block_size,myQ)
				data_update=z_binning_vect(np.mean(block_vect,axis=0)[bin_inicio:bin_fin],(bin_fin-bin_inicio)/bines,bines)
				doc.add_next_tick_callback(partial(update_img, img=data_update.reshape(1,bines),t=t,v_min=color_scale['vmin'],v_max=color_scale['vmax'],last_ts=last_ts)) # simula un nuevo trigger del callback
				t=t-1
				try:
					doc.session_context.id
				except:
					end_th=True
			else:
				time.sleep(1)
	blk=threading.Thread(target=worker)	
	blk.start()

# Setting num_procs here means we can't touch the IOLoop before now, we must
# let Server handle that. If you need to explicitly handle IOLoops then you
# will need to use the lower level BaseServer class.
server = Server({'/': funcion_documento},allow_websocket_origin=["*"],keep_alive_milliseconds=3000,check_unused_sessions_milliseconds=60*10*1000,unused_session_lifetime_milliseconds=20*60*1000,websocket_max_message_size=40*1024*1024)

def run_server():
	print('Empieza web')
	server.start()
	server.io_loop.add_callback(server.show, "/")
	server.io_loop.start()

def queue_Monitor():
	sizes={}
	grow_limit=120#cantidad de iteraciones donde la queue no fue consumida
	print "Monitoreando queues"
	while True:
		#print [s.id for s in server.get_sessions()]
		time.sleep(1)
		claves=bokeh_queues.keys()#para que no se rompa si se modifica crea en el medio
		for key in claves:
			if key in sizes:
				tam=len(bokeh_queues[key])
				if 0 < tam >=sizes[key][0]:
					sizes[key]=(tam,sizes[key][1]+1)
				else:
					sizes[key]=(tam,0)#reinicio si se consumio algo de la q
			else:
				sizes[key]=(len(bokeh_queues[key]),0)
			if sizes[key][1]==grow_limit:
				bokeh_queues.pop(key,None)
				print "Id bokeh session ended: ",key

if __name__ == '__main__':
	lalal=threading.Thread(target=run_server)
	lalal.start()
	asd=np.random.uniform(0,1,size=100)
	plt.plot(asd)
	plt.show()
