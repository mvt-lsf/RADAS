# -*- coding: utf-8 -*-
"""
Created on Tue Nov 27 08:48:58 2018

@author: LSF
"""

import re,os,glob
import numpy as np

import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText
from email.mime.image import MIMEImage


    
def mail_alerta(body,image_filename,alarmas_cant=0):   
    to="matias.t.vargas@set.ypf.com"
    smtp_server="smtp-app-int.grupo.ypf.com"
    to_list=to.split(',')
    msg=MIMEMultipart()
    msg['From']="matias.t.vargas@set.ypf.com"
    msg['To']=to
    subject='waterfall'
    if alarmas_cant>0:
        subject=subject+' ['+str(alarmas_cant)+']'
    msg['Subject']=subject
    msg.attach(MIMEText(body,'plain'))
    img_data = open(image_filename, 'rb').read()
    image = MIMEImage(img_data, name=image_filename)
    msg.attach(image)
    try:
        sv=smtplib.SMTP(smtp_server,25,timeout=30)
        sv.sendmail('',to_list,msg.as_string())
        print body        
        sv.quit()
    except:
        print "Falla mail"
        
        
import time

start=time.time()
segundos_funcionamiento=6*60*60
longitud_anterior=0
while time.time()-start<segundos_funcionamiento:
    nueva_lista=glob.glob('*.jpg')
    if len(nueva_lista)>longitud_anterior:
        try:
            mail_alerta(" ",nueva_lista[-1])
        except:
            time.sleep(10)
            print "Fallamail"
            continue
        longitud_anterior=len(nueva_lista)        
    time.sleep(60)
