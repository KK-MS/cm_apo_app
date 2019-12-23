"""@package docstring
Class to evaluate ADAS with KPI's
"""

import pandas as pd
import numpy as np
import os,sys,inspect
import h5py
import matplotlib.pyplot as plt
import seaborn as sns
import matplotlib.gridspec as gridspec
import math
import warnings
from scipy.signal import filtfilt, butter
import glob
import os 
import warnings
from scipy.signal import filtfilt, butter


class cCROLayer1:
    
    def __init__(self):
        # current directory path
        self.sCurrentDir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
        # current parent directory path
        self.sParentDir = os.path.dirname(self.sCurrentDir)
        # current grandparent directory path
        self.sGrandParentDir = os.path.dirname(os.path.dirname(self.sCurrentDir))
        # all available CRO roads
        #self.oRoadslist = ['A7','A8','B19','B295','L1180']



    def calcLowPassFilter(self,data,order,frequenzy):
        print('Tiefpassfilterung')
        with warnings.catch_warnings():

          
            # Generate a noisy signal to be filtered.
            x = data
            # Create an order 2 lowpass butterworth filter.
            b, a = butter(order, frequenzy)
            # Use filtfilt to apply the filter.
            y = filtfilt(b, a, x)
          
        return y
        
        
        
        
    def calcMiddleline(self,dfroad,order,frequenzy,resolution):
        
        #find the Layer 0 Lines
            # 1 Referenzline
        LineM0_lat = dfroad.LineM0_lat.values
        LineM0_lon = dfroad.LineM0_lon.values
        LineM0_ele = dfroad.LineM0_ele.values
            # 2 last Linie       
        LinePX_lat = dfroad.iloc[:,-3]
        LinePX_lon = dfroad.iloc[:,-2]
        LinePX_ele = dfroad.iloc[:,-1]
        
        #numpy add
        LineP1P2_lat = np.add(LineM0_lat,LinePX_lat)/2
        LineP1P2_lon = np.add(LineM0_lon,LinePX_lon)/2
        LineP1P2_ele = np.add(LineM0_ele,LinePX_ele)/2
        
        LineP1P2 = pd.DataFrame({'lat_rightBorder':LineM0_lat,
                           'lon_rightBorder':LineM0_lon,
                           'ele_rightBorder':LineM0_ele,
                           'lat_middle':LineP1P2_lat,
                           'lon_middle':LineP1P2_lon,
                           'ele_middle':LineP1P2_ele,
                           'lat_leftBorder':LinePX_lat,
                           'lon_leftBorder':LinePX_lon,
                           'ele_leftBorder':LinePX_ele}, columns=['lat_rightBorder',
        'lon_rightBorder','ele_rightBorder','lat_middle','lon_middle','ele_middle',
        'lat_leftBorder','lon_leftBorder','ele_leftBorder'])
        
        
        #output transfomlist
        lsx = []
        lsy = []
        lsz = []
        ls = []
        lssum = []
        lbear= []
        ssum = 0


        for i in range(0,len(LineP1P2.lat_middle.values)-1,1):
            #Radius Erdmittelpunkt bis Äquator
            r = 6378137.0
            #Exzentrizität
            en = 0.0818191908426
            #Pi, sin
            pi = math.pi
            #Radius Richtung Norden
            rn = (r*(1-en*en))/((1-en*en*math.sin(LineP1P2.lat_middle.values[i])*math.sin(LineP1P2.lat_middle.values[i]))**1.5)
            #Radius Richtung Osten
            ro = r/((1-en**2*math.sin(LineP1P2.lat_middle.values[i])**2)**(0.5))
            #Skalierungsfaktor Norden in m/° Abstand in Meter nach Norden, zwischen Breitengraden
            sfn=rn*(pi/180)
            #Skalierungsfaktor Osten in m/° Abstand in Meter nach Osten, zwischen Längengraden
            sfo=ro*(pi/180)*math.cos(LineP1P2.lat_middle.values[i]*pi/180)

            #Abstand zwischen den beiden GPS-Punkten in X-Richtung Osten in Meter
            sx = (LineP1P2.lon_middle.values[i+1]-LineP1P2.lon_middle.values[i])*sfo
            lsx.append(sx)

            #Abstand zwischen den beidn GPS-Punkten in Y-Richtung Norden in Meter
            sy = (LineP1P2.lat_middle.values[i+1]-LineP1P2.lat_middle.values[i])*sfn
            lsy.append(sy)
            
            #Abstand zwischen den beidn GPS-Punkten in Z-Richtung Osten in Meter
            sz = (LineP1P2.ele_middle.values[i+1]-LineP1P2.ele_middle.values[i])
            lsz.append(sz)

            #Distanz zwischen den beiden GPS-Punkten
            s = math.sqrt(sy**2+sx**2)
            #Liste
            ls.append(s)
            #Berechnung der Summe aus S
            ssum = s + ssum
            lssum.append(ssum)

            #arctan2 - angle in rad 
            angle_rad = math.atan2(sx,sy)

            #angle in degree between -180 / 180
            angle_deg = math.degrees(angle_rad)

            #bearing angle between 0-360° 
            bearingangle = (angle_deg+360) % 360
            lbear.append(bearingangle)
       

        lsx.append(sx)     
        lsy.append(sy)  
        lsz.append(sz)
        ls.append(s)
        lssum.append(ssum)
        lbear.append(bearingangle)
        


        #Tiefpassfilter Bearingangle
        lbear = self.calcLowPassFilter(lbear,order,frequenzy)  
        
        
        df = pd.DataFrame({'bearing':lbear})
        bearingdiff = df.bearing.diff()
        
        #Liste plausibilisierte Ableitung Kurvenradien
        bearing_plaus = []
        
        i = 0
        
        for value in bearingdiff.values:
            i = i + 1
         
            if value <= 5 and value >= -5:
                bearing_plaus.append(bearingdiff.values[i-1])     
                
            else:
                bearing_plaus.append(0)

                
                
                
        df_plaus = pd.DataFrame({'bearnbing_plaus':bearing_plaus})    
        
        lhead= list(df_plaus.bearnbing_plaus.values)
        
        
        lhead = self.calcLowPassFilter(df_plaus.bearnbing_plaus.values,order,frequenzy)    

    #    lhead = []
    #    for i in range(0,len(bearfilter)-1,1):
    #        head = bearfilter[i+1]-bearfilter[i]
    #        lhead.append(head)
    #        
    #    lhead.append(head)
        #calc heading angle
        #abear= np.array(lbear)
        #aheading = np.gradient(abear)

        #calc curvature
        ars= np.array(ls)

        ahead= np.array(lhead)
        ahead = np.radians(ahead)
        acurv = np.divide(ahead,resolution)
        
        
        #print(len(lssum),len(ls),len(lbear),len(lhead),len(acurv),len(lsx),len(lsy),
        #      len(lsz),len(LineP1P2.lat_middle.values),len(LineP1P2.lon_middle.values),len(LineP1P2.ele_middle.values))
        
        LineP1P2 = pd.DataFrame({'ssum':  list(map(float,lssum)),
                               's' : list(map(float,ls)),
                               'bearing':list(map(float,lbear)),
                               'heading':lhead,
                               'curv':acurv,
                               'x' : list(map(float,lsx)),
                               'y': list(map(float,lsy)),  
                               'z' : list(map(float,lsz)),
                               'lat_middle':LineP1P2.lat_middle.values,
                           'lon_middle':LineP1P2.lon_middle.values,
                           'ele_middle':LineP1P2.ele_middle.values,
                           'lat_rightBorder':LineP1P2.lat_rightBorder.values,
                           'lon_rightBorder':LineP1P2.lon_rightBorder.values,
                           'ele_rightBorder':LineP1P2.ele_rightBorder.values,
                           'lat_leftBorder':LineP1P2.lat_leftBorder.values,
                           'lon_leftBorder':LineP1P2.lon_leftBorder.values,
                           'ele_leftBorder':LineP1P2.ele_leftBorder.values}, 
        columns=['ssum','s','bearing','heading','curv','x','y','z','lat_middle','lon_middle','ele_middle',
                 'lat_rightBorder','lon_rightBorder','ele_rightBorder','lat_leftBorder','lon_leftBorder','ele_leftBorder',])
        


        return LineP1P2
 
    def importRoadDataHeader(self,sParentPath,sRoadName,sRoadDirection,sLayer,sResolution):
        """
        import the cro road data as pandas DataFrame
        @:param sParentPath          [in]    road path
        @:param sRoadName            [in]    road name
        @:param sRoadDirection       [in]    road direction
        @:param sLayer               [in]    road layer
        @:param sResolution          [in]    road resolution
        @:param df                   [out]   road data as pandas DataFrame
        """
        #read data with path
        f = open(sParentPath+'\\data\\cro\\'+sRoadName +'.'+sRoadDirection+'.L'+sLayer+'.R'+sResolution+'.ASCII.cro','r', encoding="latin-1")
        data = f.readlines()[0:40]
       #get header line in list
        mystring = data[:1][0]
        mylist = mystring.split(",") # split string 
        len(mylist) # len of list of column names
        return data

        
    def importRoadData(self,sParentPath,sRoadName,sRoadDirection,sLayer,sResolution):
        """
        import the cro road data as pandas DataFrame
        @:param sParentPath          [in]    road path
        @:param sRoadName            [in]    road name
        @:param sRoadDirection       [in]    road direction
        @:param sLayer               [in]    road layer
        @:param sResolution          [in]    road resolution
        @:param df                   [out]   road data as pandas DataFrame
        """
        return pd.read_csv(sParentPath+'\\data\\cro\\'+sRoadName +'.'+sRoadDirection+'.L'+sLayer+'.R'+sResolution+'.ASCII.cro',
                           sep=',', skiprows = 40, engine='python')        
        

                           
    def createNNRoadArray(self,df,sRoadLat,sRoadLon):
        """
        create numpy array from pandas DataFrame for Nearest Neigbour Search
        @:param df                   [in]    road data as pandas DataFrame
        @:param sRoadLat             [in]    lat channel name of road data as pandas DataFrame
        @:param sRoadLon             [in]    lon channel name of road data as pandas DataFrame
        @:param bNNarray             [out]   numpy array for NearestNeigbourSearch array([[lat0,lon0],[lat1,lon1],..)
        """
        #Array for nearest Neigbour Search
        return np.array([ [df[sRoadLat].values[i],df[sRoadLon].values[i]] for i in range(len(df)) ])

    def convertCSV2CRO(self,path):
        #Layer 0 DataFRAME
        dfLayer0 = pd.DataFrame()
        print(dfLayer0)
        #get all txt_files of path
        text_files = [f for f in os.listdir(path) if f.endswith('.csv')]
        print(text_files)
        #interate over all files and save as pandasDataframe
        for file in text_files:
            dfFile = pd.read_csv(path+file,sep=',')
            print(dfFile)
            #rename columns for Layer0
            dfLayer0[file[0:-4]+'_'+dfFile.columns[0]] = dfFile.lat.values
            dfLayer0[file[0:-4]+'_'+dfFile.columns[1]] = dfFile.lon.values
            dfLayer0[file[0:-4]+'_'+dfFile.columns[2]] = dfFile.ele.values
            print(file)
        return dfLayer0
    
    
    def calcNNindex(self,afRefChannel,afSyncChannel):
        """
        calculate list of indexes of linear NearestNeigbours
        @:param afRefChannel         [in]    float array of reference channel with channel length
        @:param afSyncChannel        [in]    float array of syncronized channel with channel length
        @:param lIdxs                [out]   list of synchrone indexes between road and vehicle
        """

        return [np.argmin(np.linalg.norm(afSyncChannel-afRefChannel[i],axis=1)) for i in range(0,len(afRefChannel),1)] 
        

        
    def calcVectorBetween2DGPSPoints(self,afNNRefLine,afNNRelLine):
        """
        calculate distance s, distance sx, distance sy, azimutangle between 2 DGPS-points
        @:param afNNRefLine          [in]    float array of reference channel with channel length
        @:param afNNRelLine          [in]    float array of syncronized channel with channel length
        @:param vector               [out]   road data as pandas DataFrame
        """
        #Radius Erdmittelpunkt bis Äquator
        r = 6378137.0
        #Exzentrizität
        en = 0.0818191908426
        #Pi, sin
        pi = math.pi

        lat0 = afNNRefLine[0]
        lon0 = afNNRefLine[1]
        lat1 = afNNRelLine[0]
        lon1 = afNNRelLine[1]
        
        #Radius Richtung Norden
        rn = (r*(1-en*en))/((1-en*en*math.sin(lat0)*math.sin(lat0))**1.5)
        #Radius Richtung Osten
        ro = r/((1-en**2*math.sin(lat0)**2)**(0.5))
        #Skalierungsfaktor Norden in m/° Abstand in Meter nach Norden, zwischen Breitengraden
        sfn=rn*(pi/180)
        #Skalierungsfaktor Osten in m/° Abstand in Meter nach Osten, zwischen Längengraden
        sfo=ro*(pi/180)*math.cos(lat0*pi/180)
        #Abstand zwischen den beiden GPS-Punkten in X-Richtung Osten in Meter
        sx = (lon1-lon0)*sfo
        #Abstand zwischen den beidn GPS-Punkten in Y-Richtung Norden in Meter
        sy = (lat1-lat0)*sfn

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
        return [float(s),float(sx),float(sy),float(bearingangle)]
        

        
        
    def calcLayer3asPandas(self,dfsigns,lindex,vector):
        """
        Get all ways from osm strored in pandas DataFrame
        @:param dfsigns                 [in]    minidom object
        @:param df                  [out]   pandas DataFrame with ways information
        """ 
        li = [] #indexes layer 0 anzahl der zeilen
        lsx = []
        lsy = []
        ls = []
        lazim = []
        lrsid = []

        checkliste = []

        for i in range(0,len(dfsigns)-1,1):
            
            
            if dfsigns.way.values[i] not in checkliste:
                checkliste.append(dfsigns.way.values[i])
            
            

                li.append(lindex[i])
                lsx.append(vector[i][0])
                lsy.append(vector[i][1])
                ls.append(vector[i][2])
                lazim.append(vector[i][3])
                lrsid.append(dfsigns.roadsign.values[i])



        dflayer3 = pd.DataFrame({'id':li,
                                    'sx':lsx,
                                    'sy':lsy,
                                    's':ls,
                                    'azimut':lazim,
                                'signid':lrsid},columns=['id','sx','sy','s','azimut','signid'])
                                
        return dflayer3

        
        
        
    def calcLayer3DataBlock(self,dflayer3,dfroadL1):
        """
        Get all ways from osm strored in pandas DataFrame
        @:param dfsigns                 [in]    minidom object
        @:param df                  [out]   pandas DataFrame with ways information
        """ 
        
        lsignid = []
        lsxsign = []
        lsysign = []
        lazimutsign = []

        zaeler = 0

        for i in range(0,len(dfroadL1),1):    
            
            if i in list(dflayer3.id.values):
                lsignid.append(dflayer3.signid.values[zaeler])
                lsxsign.append(dflayer3.sx.values[zaeler])
                lsysign.append(dflayer3.sy.values[zaeler])
                lazimutsign.append(dflayer3.azimut.values[zaeler])
                zaeler = zaeler + 1
                
            else:
                lsignid.append(0)
                lsxsign.append(0)
                lsysign.append(0)
                lazimutsign.append(0)
                
        #    if dfsign.way.values[i] not in checkliste:
        #        checkliste.append(dfsign.way.values[i])

        dflayer3 = pd.DataFrame({'ssum':dfroadL1.ssum.values,
                                's':dfroadL1.s.values,
                                'sign':lsignid,
                                'sx':lsxsign,
                                'sy':lsysign,
                                'azimut':lazimutsign},columns=['ssum','s','sign','sx','sy','azimut'])
                                
        return dflayer3        
   


   
    def getPandasDataFrameFromOSM(self,dnodes,dways):
        """
        calulate pandas DataFrame from OSM data with all information
        @:param dnodes               [in]    pandas DataFrame with nodes information 
        @:param dways                [in]    pandas DataFrame with ways information 
        @:param df                   [out]   pandas DataFrame with all osm information
        """
        #Get List of ways
        lcWayIds = list(dways.columns.values)

        #Get all points with all Information in one pandas DataFrame
        lcGeomType = []
        lcEle = []
        lcLat = []
        lcLon = []
        lcId = []
        lcWay = []
        lcRoadType = []

        for way in lcWayIds:
            cway = str(way)
            cGeomType = dways[way].iloc[0] #geomType
            cNodeIds = dways[way].iloc[4] # ref
            cRoadSign = dways[way].iloc[7] # roadsign:type
            #print(cNodesIds) 
            for node in cNodeIds:
                node = str(node)
                cEle = dnodes[node].iloc[0]
                lcEle.append(cEle)
                cLat = dnodes[node].iloc[1]
                lcLat.append(cLat)
                cLon = dnodes[node].iloc[2]
                lcLon.append(cLon)      
                lcId.append(node)   
                lcWay.append(cway)
                lcGeomType.append(cGeomType)
                lcRoadType.append(cRoadSign)

        #create new pandas Dataframe 
        dfdata = pd.DataFrame({'lon' : list(map(float,lcLon)),
                               'lat': list(map(float,lcLat)),  
                              'ele' : list(map(float,lcEle)),
                              'id': list(map(int,lcId)),
                               'way': list(map(int,lcWay)),
                            'geometry': list(map(str,lcGeomType)),
                              'roadsign':list(map(str,lcRoadType))}, columns=['lon','lat','ele','id','way','geometry','roadsign'])


        return dfdata
        
        
        
        
    def extractRoadSigns(self,dways,dnodes,dfdata):
        """
        claculate pandas DataFrame from .osm with all road signs
        @:param dways                   [in]    pandas DataFrame with ways information
        @:param dnodes                  [in]    pandas DataFrame with nodes information
        @:param dfdata                  [in]    pandas DataFrame with all osm information
        @:param dfsign                  [out]   pandas DataFrame with all road signs
        """
        # all Ways with roation clockwise
        WayIds = []
        RoadSigns = []


        #Get List of ways
        lcWayIds = list(dways.columns.values)

        #Get all points with all Information in one pandas DataFrame
        lcGeomType = []
        lcEle = []
        lcLat = []
        lcLon = []
        lcId = []
        lcWay = []
        lcRoadType = []

        #getting list of wayID's
        for i in range(0,len(dfdata.way.values),1):

            if dfdata.way.values[i] not in WayIds and dfdata.roadsign.values[i] != 'nan':
                WayIds.append(dfdata.way.values[i])
                RoadSigns.append(dfdata.roadsign.values[i])
                
                for way in WayIds:
                    cway = str(way)
                    cGeomType = dways[str(way)].iloc[0] #geomType
                    cNodeIds = dways[str(way)].iloc[4] # ref
                    cRoadSign = dways[str(way)].iloc[7] # roadsign:type
                    #print(cNodesIds) 
                    for node in cNodeIds:
                        node = str(node)
                        cEle = dnodes[node].iloc[0]
                        lcEle.append(cEle)
                        cLat = dnodes[node].iloc[1]
                        lcLat.append(cLat)
                        cLon = dnodes[node].iloc[2]
                        lcLon.append(cLon)      
                        lcId.append(node)   
                        lcWay.append(cway)
                        lcGeomType.append(cGeomType)
                        lcRoadType.append(cRoadSign)

                #create new pandas Dataframe 
                dfsign = pd.DataFrame({'lon' : list(map(float,lcLon)),
                                       'lat': list(map(float,lcLat)),  
                                      'ele' : list(map(float,lcEle)),
                                      'id': list(map(int,lcId)),
                                       'way': list(map(int,lcWay)),
                                    'geometry': list(map(str,lcGeomType)),
                                      'roadsign':list(map(str,lcRoadType))}, columns=['lon','lat','ele','id','way','geometry','roadsign'])
           
        #pandas dataframe with road signs
        return dfsign
        
        
        
    def saveHeader(self,path,lHeader):
        """
        Save Header infromation from other layer 1 with layer 3 infos
        """
        with open(self.sGrandParentDir+path, 'w+') as cro_file:

            for line in lHeader:
                
                if line == 'layer,1\n':
                    line = 'layer,3\n'
                
                cro_file.write(line)     
                
                
    def combineHeaderData(self,street,direction,layer,resolution):
        """
        Save CRO File with header & data infromation 
        """        
        read_files = glob.glob(os.path.join(self.sGrandParentDir+'/data/temp/', '*.cro'))

        with open(self.sGrandParentDir+'\data\\cro\\'+str(street) +'.'+str(direction)+'.L'+str(layer)+'.R'+str(resolution)+'.ASCII.cro', "wb") as outfile:
            for f in read_files:
                with open(f, "rb") as infile:
                    outfile.write(infile.read())

            outfile.close()           