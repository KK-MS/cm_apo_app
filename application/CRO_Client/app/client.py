# -*- coding: utf-8 -*-
"""
    CRO Ground Truth Server
    16.01.2020
    Version 1.0
    by Dipl.-Ing (FH) Sebastian Keidler
    ADrive LivingLab Kempten
"""

__author__ = "Dipl.-Ing(FH) Sebastian Keidler"
__version__ = "1.0"
__email___ = "sebastian.keidler@hs-kempten.de"
__status__ = "Develop"

#%% Bibliotheken
import zmq
import classes.cCROLayer0 as l0
import classes.cDistance2Line as d2l
import classes.cSegmentAnalyzer as sa
import data_transfer as dt
import pandas as pd


print('Client starts')

oCROLayer0 = l0.cCROLayer0()
oSegment = sa.cSegmentAnalyzer()
oDistance2Line = d2l.cDistance2Line()


sRoadName = 'A7'
sRoadDirection = 'N'
sLayer = '0'
sResolution = '1'
cRefLine = 'P0'



df = oCROLayer0.importRoadData(oCROLayer0.sGrandParentDir,sRoadName,sRoadDirection,sLayer,sResolution)

dfRoadL1 = oSegment.importRoadData(oSegment.sGrandParentDir,sRoadName,sRoadDirection,str(1),sResolution)

#create numpy array for Nearest Neigbour search
afRoadNN = oSegment.createNNRoadArray(df,'Line'+str(cRefLine)+'_lat','Line'+str(cRefLine)+'_lon')


context = zmq.Context()

print("Python Ground Truth APO Bridge Client")

socket = context.socket(zmq.REQ)

socket.connect("tcp://localhost:5555")

request = "Get Data"

    

#f= open("test.txt","a")
#f.write("time,lat,lon,d2l\n")
llat = []
llon = []
ltime = []
ld2l = []

while True:

    print("Sending request to C++ APO Bridge Server", request, "...")

    socket.send_string("Hello")

    message = socket.recv()
    
    #fmessage = float(message)
    print("Server hat GPS-Informationen erhalten! %s" % message)
    
    #print(message,len(message))
    
    
    lat = str(message.decode('UTF-8'))[0:str(message.decode('UTF-8')).find(';')]
    llat.append(float(lat))
    print('CM Position Lat',lat)
    
    
    lon = str(message.decode('UTF-8'))[str(message.decode('UTF-8')).find(';')+1:str(message.decode('UTF-8')).find(',')]
    llon.append(float(lon))    
    print('CM Position Lon',lon)
    

    time = str(message.decode('UTF-8'))[str(message.decode('UTF-8')).find(',')+1:]
    ltime.append(float(time))    
    print('CM Position time',time)

    
    #  Do some 'work'
    sd2l = str(dt.calcD2l(lat,lon,afRoadNN,dfRoadL1))
    ld2l.append(float(sd2l)) 
    #f= open("test.txt","w+")
    #f.write(str(time)+','+str(lat)+','+str(lon)+','+str(sd2l)+'\n')

    
    
    #ld2l.append(d2l)
    print(sd2l)
#f.close()

    



