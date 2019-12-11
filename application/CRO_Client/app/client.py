import zmq
import time
import classes.cCROLayer0 as l0
import classes.cDistance2Line as d2l
import data_transfer as dt
import json

print('Server starts')

oCROLayer0 = l0.cCROLayer0()

sRoadName = 'B308'
sRoadDirection = 'W'
sLayer = '0'
sResolution = '1'


df = oCROLayer0.importRoadData(oCROLayer0.sGrandParentDir,sRoadName,sRoadDirection,sLayer,sResolution)
df.head()


context = zmq.Context()

print("Connecting to hello world server...")

socket = context.socket(zmq.REQ)

socket.connect("tcp://localhost:1234")

request = "Get Data"


ld2l = []
llat = []
llon = []


while True:

    print("Sending request ", request, "...")

    socket.send_string("Hello")

    message = socket.recv()
    
    #fmessage = float(message)
    print("Server hat GPS-Informationen erhalten! %s" % message)
    print(message,len(message))
    
    lat = message[0:13]
    llat.append(float(lat))
    lon = message[13:26]
    llon.append(float(lon))
    
    #  Do some 'work'
    d2l = dt.calcD2l(lat,lon)
    #d2l = d2l*-1
    
    sd2l = str(d2l)
    
    ld2l.append(d2l)
    print(sd2l)