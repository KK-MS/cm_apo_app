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


def calcD2l(lat,lon):
    
    oCROLayer0 = l0.cCROLayer0()
    oSegment = sa.cSegmentAnalyzer()
    oDistance2Line = d2l.cDistance2Line()
    
    
    sRoadName = 'A7'
    sRoadDirection = 'S'
    sLayer = '0'
    sResolution = '1'
    cRefLine = 'P0'
    
    
    df = oCROLayer0.importRoadData(oCROLayer0.sGrandParentDir,sRoadName,sRoadDirection,sLayer,sResolution)
    # Import Road data Layer 1
    dfRoadL1 = oSegment.importRoadData(oSegment.sGrandParentDir,sRoadName,sRoadDirection,str(1),sResolution)
    
    
    
    #create numpy array for Nearest Neigbour search
    afRoadNN = oSegment.createNNRoadArray(df,'Line'+str(cRefLine)+'_lat','Line'+str(cRefLine)+'_lon')
    

    afVehicleNN = [float(lat),float(lon)]
    #print(afVehicleNN)
    
    fLat0 = afVehicleNN[0]
    fLon0 = afVehicleNN[1]
    
    # Calculate Nearest Road to Vehicle indexes
    index = np.argmin(np.linalg.norm(afRoadNN-afVehicleNN,axis=1))
    
    
    fLat1 = afRoadNN[index][0]
    fLon1 = afRoadNN[index][1]
    
    
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
