# -*- coding: utf-8 -*-

#manejar estado control
#silenciar zonas
from opcua import ua, Server

import time
import numpy as np

def server_opc(zonas_cant,qAlarmas,OPC_alert):
    server = Server()
    server.set_endpoint("opc.tcp://0.0.0.0:4840/Ducto_costero")
    
    
    server.set_server_name("Servidor alarmas ducto costero")
    # setup our own namespace, not really necessary but should as spec
    uri = "Alarmas ducto RA"
    
    idx = server.register_namespace(uri)
    # get Objects node, this is where we should put our nodes
    objects = server.get_objects_node()
    
    # populating our address space
    myobj = objects.add_object(idx, "Alarmas_RA")
    zonass=[]
    for i in range(zonas_cant):
        new_zone=myobj.add_variable(idx, "Zona_"+str(i+1), False)
        new_zone.set_writable()
        zonass.append(new_zone)

    zonass_silence=[]
    for i in range(zonas_cant):
        new_zone=myobj.add_variable(idx, "Silenciar_zona_"+str(i+1), False)
        new_zone.set_writable()
        zonass_silence.append(new_zone)


    
    estados=[]
    for i in range(5):
#        new_st=myobj.add_variable(idx, "Estado_"+str(i+1), int(0))
        new_st=myobj.add_variable(idx, "Estado_"+str(i+1), float(0))
        new_st.set_writable()
        estados.append(new_st)
        
    new_st=myobj.add_variable(idx, "Cantidad_de_zonas_activadas", float(0))
    new_st.set_writable()
    estados.append(new_st)
        
    
    zonass=np.array(zonass)#DUDOSO USO DE np
    zonass_silence=np.array(zonass_silence)
    
    variable_mail=myobj.add_variable(idx,"Notificar_mail",True)
    variable_mail.set_writable()
    
    # starting!
    server.start()
    try:
        while True:
            if len(qAlarmas)>0:
                proxima_alarma=qAlarmas.popleft()
                #print proxima_alarma
                if OPC_alert:
                    for zona in zonass[proxima_alarma]:
                        zona.set_value(True)
            else:
                time.sleep(1)
    finally:
    #close connection, remove subcsriptions, etc
        server.stop()
