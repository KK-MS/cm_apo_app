# -*- coding: utf-8 -*-
"""
    checkVehiclePosition
    14.12.2018
    Version 1.0
    by Dipl.-Ing (FH) Sebastian Keidler
    ADrive LivingLab Kempten
"""

__author__ = "Dipl.-Ing(FH) Sebastian Keidler"
__version__ = "1.0"
__email___ = "sebastian.keidler@hs-kempten.de"
__status__ = "Develop"

#%%
#from shapely.geometry import Polygon, Point, MultiPolygon
#import shapefile
import os,sys,inspect
import time
from tqdm import tqdm
import sys
import os
import pandas as pd
import numpy as np
import math
import matplotlib.pyplot as plt
import matplotlib.colors as colors
import matplotlib.cm as cmx
from scipy import interpolate



class cDistance2Line():
    
    
    
    
    def __init__(self):
        # current directory path
        self.sCurrentDir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
        # current parent directory path
        self.sParentDir = os.path.dirname(self.sCurrentDir)
        # current grandparent directory path
        self.sGrandParentDir = os.path.dirname(os.path.dirname(self.sCurrentDir))        
       
    def calcVectorDistanceAngle(self,fLat0,fLon0,fLat1,fLon1):
        """
        create numpy array from pandas DataFrame for Nearest Neigbour Search
        @:param fLat0                      [in]    string road line to calculate distance 2 line 
        @:param fLon0                      [in]    pandas DataFrame of vehicle data
        @:param fLat1                      [in]    pandas DataFrame of road data
        @:param fLon1                      [in]    list of road indexes to vehicle channel length
        @:param fVector                    [out]   list of distance 2 line 
        """
        #Radius Erdmittelpunkt bis Äquator
        r = 6378137.0
        #Exzentrizität
        en = 0.0818191908426
        #Pi, sin
        pi = math.pi
        #Radius Richtung Norden
        rn = (r*(1-en*en))/((1-en*en*math.sin(fLat0)*math.sin(fLat0))**1.5)
        #Radius Richtung Osten
        ro = r/((1-en**2*math.sin(fLat0)**2)**(0.5))
        #Skalierungsfaktor Norden in m/° Abstand in Meter nach Norden, zwischen Breitengraden
        sfn=rn*(pi/180)
        #Skalierungsfaktor Osten in m/° Abstand in Meter nach Osten, zwischen Längengraden
        sfo=ro*(pi/180)*math.cos(fLat0*pi/180)
        #Abstand zwischen den beiden GPS-Punkten in X-Richtung Osten in Meter
        sx = (fLon1-fLon0)*sfo
        #Abstand zwischen den beidn GPS-Punkten in Y-Richtung Norden in Meter
        sy = (fLat1-fLat0)*sfn
        #Distanz zwischen den beiden GPS-Punkten
        s = math.sqrt(sy**2+sx**2)
        #arctan2 - angle in rad 
        angle_rad = math.atan2(sx,sy)

        #angle in degree between -180 / 180
        angle_deg = math.degrees(angle_rad)

        if (angle_deg+360) % 360 >= 355 or (angle_deg+360) % 360 <= 5:             
            #bearing angle between 0-360° 
            bearingangle = (angle_deg+360)
        else:
            #bearing angle between 0-360° 
            bearingangle = (angle_deg+360) % 360

        #s = haversine((lat0,lon0),(lat1,lon1))/1000
        return [float(s),float(bearingangle)]
       
    def calcDistance2MiddleLine(self,dfVehicle,dfRoad,nRoadIdxVehicleLength,nVehicleIndex):
        """
        create numpy array from pandas DataFrame for Nearest Neigbour Search
        @:param dfVehicle                  [in]    pandas DataFrame of vehicle data
        @:param dfRoad                     [in]    pandas DataFrame of road data
        @:param nRoadIdxVehicleLength      [in]    road index to vehicle channel length
        @:param nVehicleIndex              [in]    vehicle index
        @:param fD2L                       [out]   list of distance 2 line 
        """
        #get road bearing
        fRoadBearing = dfRoad.bearing.values[nRoadIdxVehicleLength]
        
        #get road middle line coordinates 
        fLat0 = dfRoad.lat_middle.values[nRoadIdxVehicleLength]
        fLon0 = dfRoad.lon_middle.values[nRoadIdxVehicleLength]
        fLat1 = dfVehicle.lat.values[nVehicleIndex]
        fLon1 = dfVehicle.lon.values[nVehicleIndex]
        
        #calculate vector vehicle to road
        fVector = self.calcVectorDistanceAngle(fLat0,fLon0,fLat1,fLon1)
        
        #distance2line[0] postitives Entfernungsmaß
        if fVector[1] >= 355 and fRoadBearing <= 5:
            fD2L = math.sin(math.radians(360-fVector[1]+fRoadBearing)*abs(fVector[0]))
            
        else:
            fD2L = math.sin(math.radians(fVector[1]-fRoadBearing))*abs(fVector[0])

        return fD2L

        
