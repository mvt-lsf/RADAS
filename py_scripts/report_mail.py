# -*- coding: utf-8 -*-
"""
Created on Tue Nov 27 08:48:58 2018

@author: LSF
"""

import matplotlib
matplotlib.use('Agg')
matplotlib.rcParams.update({'font.size':18})
import matplotlib.pyplot as plt
import re,os
import numpy as np

import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText
from email.mime.image import MIMEImage

import datetime

def valores_copados(vec,factor_de_copadez=50.0):
    return factor_de_copadez*(np.floor(vec/factor_de_copadez) + np.round((vec/factor_de_copadez)-np.floor(vec/factor_de_copadez)))

def parse_markers(archivo_punzados):
    res={}
    pattern='([a-zA-z0-9]+)\s*.\s*(\d+\.?\d*)\s*'
    with open(archivo_punzados) as markers:
        for l in markers.readlines():
            mark=re.findall(pattern,l)
            if mark:
                res[mark[0][0]]=float(mark[0][1])
    return res



def generate_waterfall(img_dict):
    data=img_dict['data']
    delta_x=img_dict['delta_x']
    plots_per_second=img_dict['plots_per_second']
    filename=img_dict['filename']
    
    bins_plot=data.shape[1]
    rows_plot=data.shape[0]
    
    fig,ax=plt.subplots(figsize=(20, 10))
    plot_image=ax.imshow(data,cmap='jet',aspect='auto',vmax=0.2)#,interpolation= 'none')
    cbar=fig.colorbar(plot_image)
    tx=ax.set_title('Datos ducto costero')
    try:
        markers=parse_markers('punzados.txt')
    except:
        markers={}
        print "sin markers"
    
    for label,position in markers.iteritems():
        mark=ax.axvline(x=position/delta_x,color='lightcoral')
        t=ax.text(position/delta_x,rows_plot/2,label,rotation=90,color='hotpink',weight='bold')


    labels_copados=valores_copados(np.arange(0,bins_plot*delta_x,1000*delta_x))
    ax.set_xticks(labels_copados/delta_x)
    ax.set_xticklabels(labels_copados)
    ax.set_yticks(np.arange(0,rows_plot+1,50))
    ax.set_yticklabels(map(str,(np.arange(0,rows_plot+1,50)*plots_per_second)[::-1]*-1))

    labels = [item.get_text() for item in ax.get_yticklabels()]
    labels[-1]=filename
    ax.set_yticklabels(labels)
    
    ax.set_ylabel('Tiempo[seg]')
    ax.set_xlabel('Posicion[m]')

    zoom_inicial_m=3300#param
    zoom_final_m=10800#param
    ax.set_xlim(zoom_inicial_m/delta_x,zoom_final_m/delta_x)
    fig.savefig(filename,dpi=100,trasparent=True)


    
def mail_alerta(body,alarmas_cant=0,img_data_dict=None):   
    to="matias.t.vargas@set.ypf.com,kunik.dario@set.ypf.com,pablo.j.orte@set.ypf.com"
    smtp_server="smtp-app-int.grupo.ypf.com"
    to_list=to.split(',')
    msg=MIMEMultipart()
    msg['From']="matias.t.vargas@set.ypf.com"
    msg['To']=to
    subject='alertaDuctoCostero'
    if alarmas_cant>0:
        subject=subject+' ['+str(alarmas_cant)+']'
    msg['Subject']=subject
    msg.attach(MIMEText(body,'plain'))
    if img_data_dict:
        generate_waterfall(img_data_dict)#datos de img_data_dict
        img_data = open(img_data_dict['filename'], 'rb').read()
        image = MIMEImage(img_data, name=img_data_dict['filename'])
        msg.attach(image)
    try:
        sv=smtplib.SMTP(smtp_server,25,timeout=30)
        sv.sendmail('',to_list,msg.as_string())
        print body, datetime.datetime.now()       
        sv.quit()
    except:
        print "Falla mail"
    if img_data_dict:
        os.remove(img_data_dict['filename'])
        
        
import sys,time,glob
import cPickle as pickle
fname=sys.argv[1]

if ' '+fname in glob.glob('*.mail'):#corrige problema espacio. arreglar en func_alarmas
    fname=' '+fname

tries=5
while tries>0:
    time.sleep(1)
    tries=tries-1
    try:
        
        with open(fname,'rb') as f:
            params=pickle.load(f)
        if 'alarmas_cant'  in params.keys():
            mail_alerta(params['txt_mail'],alarmas_cant=params['alarmas_cant'],img_data_dict=params['img_data_dict'])
        else:
            mail_alerta(params['txt_mail'],img_data_dict=params['img_data_dict'])
            
        os.remove(fname)
        break
    except:
        print "Falla lectura archivo .mail", tries
# #alarma_queue nuevo
# #########    
# if __name__ == "__main__":
#     import matplotlib
#     matplotlib.use('Agg')
#     matplotlib.rcParams.update({'font.size':18})
#     import matplotlib.pyplot as plt
#     import re,os
#     import numpy as np
    
#     import smtplib
#     from email.MIMEMultipart import MIMEMultipart
#     from email.MIMEText import MIMEText
#     from email.mime.image import MIMEImage    
#     import time
#     def mail_alerta_gmail(body,alarmas_cant=0,img_data_dict=None):
#         gmail_user = 'alarmatodoestabien'  
#         gmail_password = 'milanesa'
#         print 'conectando'
#         try:  
#             server = smtplib.SMTP_SSL('smtp.gmail.com', 465,timeout=30)
#             #server.ehlo()
#             server.login(gmail_user, gmail_password)
#         except:  
#             print 'Something went wrong...'   
#             return
#         print 'conectad2'
#         to="mvargastelles@undav.edu.ar"
#         to_list=to.split(',')
#         msg=MIMEMultipart()
#         msg['From']="lol"
#         msg['To']=to
#         subject='alertaDuctoCostero'
#         if alarmas_cant>0:
#             subject=subject+' ['+str(alarmas_cant)+']'
#         msg['Subject']=subject
#         msg.attach(MIMEText(body,'plain'))
#         if img_data_dict:
#             print 'hay imagen'
#             st=time.time()
#             generate_waterfall(img_data_dict)#datos de img_data_dict
#             print time.time()-st
#             img_data = open(img_data_dict['filename'], 'rb').read()
#             image = MIMEImage(img_data, name=img_data_dict['filename'])
#             msg.attach(image)
#         try:
#             server.sendmail('',to_list,msg.as_string())
#             print body        
#             server.quit()
#         except:
#             print "Falla mail"
#         if img_data_dict:
#             os.remove(img_data_dict['filename'])

#     data=np.random.uniform(size=(400,7000))
#     print 'hay imagen'
#     delta_x=2.03
#     plots_per_second=0.4
#     filename='saraza.png'
#     dict_img={'data':data,'delta_x':delta_x,'plots_per_second':plots_per_second,'filename':filename}
#     print 'va correo'
#     mail_alerta_gmail('hola',alarmas_cant=1,img_data_dict=dict_img)

