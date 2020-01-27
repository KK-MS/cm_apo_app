# -*- coding: utf-8 -*-
"""
    Getting Nearest Neigbour Coordinates Vehicle/Road and Distance
    22.01.2020
    Version 1.0
    by Dipl.-Ing (FH) Sebastian Keidler, M. Sc. 
    ADrive LivingLab Kempten
"""

__author__ = "Dipl.-Ing(FH) Sebastian Keidler, M. Sc."
__version__ = "1.0"
__email___ = "sebastian.keidler@hs-kempten.de"
__status__ = "Develop"

#%% Bibliotheks
import numpy as np
import math
import classes.cSegmentAnalyzer as sa
import classes.cDistance2Line as d2l




def get_distance(unit1, unit2):
    phi = abs(unit2-unit1) % 360
    sign = 1
    # used to calculate sign
    if not ((unit1-unit2 >= 0 and unit1-unit2 <= 180) or (
            unit1-unit2 <= -180 and unit1-unit2 >= -360)):
        sign = -1
    if phi > 180:
        result = 360-phi
    else:
        result = phi

    return result*sign 




def getNND2Lcoordinates(dfCRORoadL0, dfCRORoadL1, sRefLine,lat,lon):
    """
    Get the nearest neigbour coordinates from Vehicle position
    @:param dfCRORoadL0            [in]    pandas DataFrame CRO Layer 0  
    @:param dfCRORoadL1            [in]    pandas DataFrame CRO Layer 1
    @:param sRefLine               [in]    Refernce line 
    @:param lat                    [in]    Vehicle latitude
    @:param lon                    [in]    Vehicle longitude
    @:param fLat1                  [out]    Road latitude 
    @:param fLon1                  [out]    Road longitude
    @:param fD2L                   [out]    Distance 2 Line 
    """    

    oSegment = sa.cSegmentAnalyzer()
    oDistance2Line = d2l.cDistance2Line()

    #create numpy array for Nearest Neigbour search
    afRoadNN = oSegment.createNNRoadArray(dfCRORoadL0,
                                          'Line'+str(sRefLine)+'_lat',
                                          'Line'+str(sRefLine)+'_lon')
    

    afVehicleNN = [lat,lon]

    fLat0 = afVehicleNN[0]
    fLon0 = afVehicleNN[1]
    
    # Calculate Nearest Road to Vehicle indexes
    index = np.argmin(np.linalg.norm(afRoadNN-afVehicleNN,axis=1))
    
    # NEAREST NEIGBOUR COORDINATES
    fLat1 = afRoadNN[index][0]
    fLon1 = afRoadNN[index][1]
    
    #calculate vector vehicle to road
    fVector = oDistance2Line.calcVectorDistanceAngle(fLat0,fLon0,fLat1,fLon1)
    
    #Getting Road bearing/azimut by index
    fRoadBearing = dfCRORoadL1.bearing.values[index]
    
    #distance2line[0] postitives Entfernungsmaß
    #if fVector[1] >= 355 and fRoadBearing <= 5:
        #fD2L = math.sin(math.radians(360-fVector[1]+fRoadBearing)*abs(fVector[0]))

    a = get_distance(fVector[1],fRoadBearing)
        
    if a >= 90:
        print(0)
        a = a-(a-90)-min(fVector[1],fRoadBearing)
    elif a <= -90:
        print(1)
        a = -(a+180)  
    else:
        print(2)
        a = a
       
    fD2L = math.sin(math.radians(a))*abs(fVector[0])
    
    print(get_distance(fVector[1],fRoadBearing),a,fVector[1],fRoadBearing)

    return fLat1,fLon1,fD2L
