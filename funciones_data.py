# -*- coding: utf-8 -*-

import numpy as np
import time,datetime
try:
    import win32file
except:
    print "No estamos en windows, no hay pipes de win32"


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


def ts_now():
    now=(datetime.datetime.now().strftime("%y-%m-%d-%H-%M-%S")).split('-')
    res=map(int,now)
    res.append(10)#10 ms para respetar el formato de 7 elementos en la lista
    return res 

def data_generation(pipe_handler,vec_size,ts_size,source_DAS,time_stamp_DAS,bokeh_switch,ts_file,data_queue,bins_plot,qZscores=None,qbokeh=None):
    if not(source_DAS):
        vec_count=0
        pert_size=80/2
        pert_count=0
    while(True):#flow confuso del while
        if source_DAS:
            error,vec=win32file.ReadFile(pipe_handler,vec_size)
            vec=np.frombuffer(vec,dtype=np.float32)
        else:
            vec_count+=1
            vec=np.random.normal(0.1,0.001,size=bins_plot)
            if vec_count>60:#deberia ser ventana_alarma pero desacoplar
                if pert_count<pert_size:
                    pert_count+=1
                    vec+=0.01*(pert_count)
                else:
                    pert_count=0
            time.sleep(0.4)
        if time_stamp_DAS:
                error,ts=win32file.ReadFile(pipe_handler,ts_size)
                ts=np.frombuffer(ts,dtype=np.uint16).tolist()
                ts_file.write(ts_to_stringDate(ts)[5:]+'\n')
        try:
            if time_stamp_DAS:
                new_data=(vec,ts)
            else:
                new_data=(vec,ts_now())
            data_queue.append(new_data)
            if qZscores!=None:
                qZscores.append(new_data)
            if bokeh_switch:
                for q in qbokeh.keys():
                    qbokeh[q].append(new_data)
                #print "queues bokeh activas: ", len(qbokeh.keys())
        except e:
            print e
            pass