# -*- coding: utf-8 -*-
"""
Created on Tue Aug 13 15:00:30 2019

@author: ll_stsekeid
"""

import json
import numpy as np
import math
import classes.cCROLayer0 as l0
import classes.cSegmentAnalyzer as sa
import classes.cDistance2Line as d2l


def calcD2l(lat,lon,afRoadNN,dfRoadL1):
    
    afVehicleNN = [float(lat),float(lon)]
    #print(afVehicleNN)
    
    fLat0 = afVehicleNN[0]
    fLon0 = afVehicleNN[1]
    
    # Calculate Nearest Road to Vehicle indexes
    index = np.argmin(np.linalg.norm(afRoadNN-afVehicleNN,axis=1))
    
    fLat1 = afRoadNN[index][0]
    fLon1 = afRoadNN[index][1]
    
    oDistance2Line = d2l.cDistance2Line()
    #calculate vector vehicle to road
    fVector = oDistance2Line.calcVectorDistanceAngle(fLat0,fLon0,fLat1,fLon1)
    
    fRoadBearing = dfRoadL1.bearing.values[index]
    
    
    #distance2line[0] postitives Entfernungsmaß
    if fVector[1] >= 355 and fRoadBearing <= 5:
        fD2L = math.sin(math.radians(360-fVector[1]+fRoadBearing)*abs(fVector[0]))
        
    else:
        fD2L = math.sin(math.radians(fVector[1]-fRoadBearing))*abs(fVector[0])
        
    #print(fD2L)
    return fD2L
