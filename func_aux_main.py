# -*- coding: utf-8 -*-

import threading,datetime,sys
try:
    import win32file
except:
    print "No estamos en windows, no hay pipes de win32"


def windows_pipe(pipe_name):
	return win32file.CreateFile("\\\\.\\pipe\\"+pipe_name,
	                          win32file.GENERIC_READ | win32file.GENERIC_WRITE,
	                          0, None,
	                          win32file.OPEN_EXISTING,
	                          0, None)

def th_launch(target,args,kw={},daemon=True):
	th=threading.Thread(target=target,args=args,kwargs=kw)
	th.setDaemon(daemon)
	th.start()
	return th

def delta_x():
	qFreq=int(sys.argv[3])
	clockB=int(sys.argv[-2])
	clock=int(sys.argv[-1])
	c = 299792458.
	delta_t = 5e-9;
	n = 1.48143176194
	c_f = c/n
	return qFreq*c_f*delta_t/2

def data_gen_parameters(source_DAS,time_stamp_DAS):

	if source_DAS:
	    pipe_handler = windows_pipe('Pipe')
	    bins_plot=int(sys.argv[1])-int(sys.argv[2])+1
	    dtype_size=4 #tamanio del tipo de dato que viene por el pipe

	    if time_stamp_DAS:
	        now=datetime.datetime.now().strftime("%Y-%M-%d__%H_%M_%S")
	        ts_file=open(now+'.ts','a')
	        ts_size=7*2 #lo que viene por el pipe tiene este tamanio
		return pipe_handler,bins_plot,dtype_size,ts_size,ts_file
	else: #source_random, sin ts
	    pipe_handler=None
	    bins_plot=7000
	    dtype_size=0
	    ts_size=0
	    return pipe_handler,bins_plot,dtype_size,ts_size,None #DECISION POLEMICA-doble return en if\else y None. confuso flow
