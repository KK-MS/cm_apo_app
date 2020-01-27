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

class cSegmentAnalyzer:
    
    def __init__(self):
        # current directory path
        self.sCurrentDir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
        # current parent directory path
        self.sParentDir = os.path.dirname(self.sCurrentDir)
        # current grandparent directory path
        self.sGrandParentDir = os.path.dirname(os.path.dirname(self.sCurrentDir))
        # all available CRO roads
        #self.oRoadslist = ['A7','A8','B19','B295','L1180']
        
        
    def importVehicleData(self,sParentPath,sFileName,sFileFormat):
        """
        Import the vehicle data from different file formats
        @:param sPath               [in]    file path
        @:param sFileName           [in]    file name
        @:param sFileFormat         [in]    file format
        @:param df                  [out]   vehicle data as pandas DataFrame
        """
        df = pd.DataFrame()
        
        if sFileFormat == '.csv':
            df = pd.read_csv(sParentPath+sFileName+'.csv',sep=',')
        elif sFileFormat == '.hdf5':
            hf = h5py.File(sParentPath+sFileName+'.hdf5', 'r')
            for column in list(hf.keys()):
                df[column] = hf.get(column).value
            
        return df
        
    def changeHeaderDewesoftCSVExport(self,lColumnNames):
        """
        import the cro road data as pandas DataFrame
        @:param lColumnNames          [in]    liste with all column names from Dewesoft
        @:param lColumnNamesNew       [ou]    new list with all column names
        """   
        newString = []
        for name in liste:
            #detect index of space in string
            num = name.index(" ")
            #delete everything after space
            name = name[0:num]
            newString.append(name)   
            
        newString2 = []
        for name in newString:
            if name != "":
                newString2.append(name)
            else:
                pass
              
        return newString2
         
    
    
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
                           sep=',', skiprows = 47, engine='python')
        
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
    
    
    def calcStartEndIndex(self,lIdxVehicleRoad,lIdxs):
        """
        calculate start and end point to analyse
        @:param lIdxVehicleRoad      [in]    list of vehicle channel and corresponding road indexes
        @:param lIdxs                [in]    list of road channel and corresponding vehicle indexes
        @:param nStartIdx            [out]   start of the segment
        @:param nEndIdx              [out]   end of the segment
        """
        
        start = 0
        end = len(lIdxs)
        
        for i in range(len(lIdxVehicleRoad)):
            if lIdxVehicleRoad[i] == 0:
                continue
            elif lIdxVehicleRoad[i] > 0:
                start = lIdxVehicleRoad[i]
                break
                
        for i in range(len(lIdxVehicleRoad)):
            if lIdxVehicleRoad[i] < len(lIdxs):
                end = lIdxVehicleRoad[i]
            #elif lIdxVehicleRoad[i] >= len(lIdxs):
               # end = lIdxs[-1]
                
        return start,end
        
    
    def calcNNindex(self,afRefChannel,afSyncChannel):
        """
        calculate list of indexes of linear NearestNeigbours
        @:param afRefChannel         [in]    float array of reference channel with channel length
        @:param afSyncChannel        [in]    float array of syncronized channel with channel length
        @:param lIdxs                [out]   list of synchrone indexes between road and vehicle
        """

        return [np.argmin(np.linalg.norm(afSyncChannel-afRefChannel[i],axis=1)) for i in range(0,len(afRefChannel),1)] 
        
    def calcCurvatureDataFrame(self,lIdxs,dfRefChannel,lRefChannelNames,dfSyncChannel,lSyncChannelNames):
        """
        calculate defined synchronized curvature DataFrame
        @:param lIdxs                [in]    list of synchrone indexes between road and vehicle
        @:param dfRefChannel         [in]    pandas DataFrame of reference channel
        @:param lRefChannelNames     [in]    list of vehicle channel names
        @:param dfSyncChannel        [in]    pandas DataFrame of syncronized channel
        @:param lSyncChannelNames    [in]    list of road channel names
        @:param dfcurv               [out]   pandas DataFrame with synchronized curvatures
        """ 
        dfcurv = pd.DataFrame()
        
        for channel in lSyncChannelNames:
            dfcurv[channel] = [dfSyncChannel[channel].values[lIdxs[i]] for i in range(len(lIdxs)) ]
            
        for channel in lRefChannelNames:
            dfcurv[channel] = dfRefChannel[channel].values
            
        return dfcurv

    def calcAbsErrThreshold(self,afSignal1,afSignal2,fThreshold):
        """
        calculate absolute error with threhold to eliminate straight road segments
        @:param afSignal1           [in]    float array first signal
        @:param afSignal2           [in]    float array second signal
        @:param fThreshold          [in]    float of Threshold
        @:param dfcurv              [out]   pandas DataFrame with absolute Error
        """ 
        abserror = []
        for i in range(len(afSignal1)):
            if abs(afSignal1[i]) >= fThreshold:
                error = np.abs(afSignal1[i]-afSignal2[i])
                if error <= fThreshold:
                    abserror.append(error)
                else:
                    abserror.append(0)
            else:
                abserror.append(0)
        return abserror
    
    def calcRelErrThreshold(self,afSignal1,afSignal2,fThreshold1,fThreshold2):
        """
        calculate relative error with threhold to eliminate straight road segments
        @:param afSignal1           [in]    float array first signal
        @:param afSignal2           [in]    float array second signal
        @:param fThreshold1         [in]    float of Threshold relative measure limit to avoid zero division
        @:param fThreshold2         [in]    float of Threshold percentage limit  
        @:param dfcurv              [out]   pandas DataFrame with relative Error
        """ 
        relerror = []
        for i in range(len(afSignal1)):
            #
            if abs(afSignal1[i]) >= fThreshold1:
                error = np.abs(afSignal1[i]-afSignal2[i])/np.abs(afSignal1[i])*100
                if error <= fThreshold2:
                    relerror.append(error)
                else:
                    relerror.append(0)
            else:
                relerror.append(0)
        return relerror
    
    def cumulateChannel(self,afSignal,nWindow):
        """
        cumulate a channel with a difined window size
        @:param afSignal           [in]    float array signal
        @:param nWindow            [in]    int window size 
        @:param afCum              [out]   float array of cumulated channel 
        """ 
        nCounter = nWindow
        nSumSignal = 0
        nCumSignal = []
        for i in range(len(afSignal)):
            if i <= nWindow:
                nSumSignal = afSignal[i] + nSumSignal
            else:
                nWindow = nWindow + nCounter
                for i in range(nCounter):
                    nCumSignal.append(np.mean(nSumSignal)/nCounter)
                nSumSignal = 0 
        nAppend = len(afSignal)-len(nCumSignal)
        for i in range(nAppend):
            nCumSignal.append(np.mean(nSumSignal)/nCounter)
        return nCumSignal
    
    
    def appendPandasDataFrames(self,df1,df2):
        """
        cumulate a channel with a difined window size
        @:param df1                 [in]    pandas DataFrame to start
        @:param df2                 [in]    pandas DataFrame to append
        @:param df                  [out]   pandas DataFrame appended 
        """
        appended = []
        appended.append(df1)
        appended.append(df2)
        df = pd.concat(appended)
        return df
    
    def calcMeanValuesIgnoringZeros(self,afArray):
        """
        calculating the mean value of an array ignoring all zeros 
        @:param afArray              [in]    pandas DataFrame to start
        @:param fMean                [out]   pandas DataFrame appended 
        """
        afArray[afArray == 0] = np.nan
        fMean = np.nanmean(afArray)
        return fMean
    
        
    def plotPandasDataFrame(self,df,sChannelNames):
        #%matplotlib nbagg
        """
        plot the pandas DataFrame
        @:param df                  [in]    data as pandas DataFrame
        @:param sChannelNames       [in]    channel names of data 
        @:param plot                [out]   plot of pandas DataFrame
        """
        for channel in sChannelNames:
            df[channel].plot()
            
    def plotCurvatureKPI(self,afroadCurvature,vehicleCurvature,absCurvError,relCurvError,road,direction,layer,resolution,window,threshold1,threshold2,threshold3):
        """
        plot curvrature KPI and signal
        @:param afroadCurvature       [in]    float array of road curvrature
        @:param sChannelNames       [in]    channel names of data 
        @:param plot                [out]   plot of pandas DataFrame
        """
        #%matplotlib nbagg
        plt.rcParams.update({
            "font.family": "serif",
            "font.serif": [],                    # use latex default serif font
            "font.sans-serif": ["DejaVu Sans"],  # use a specific sans-serif font
        })
        # Plot figure with subplots of different sizes
        fig = plt.figure(1)

        # set up subplot grid
        gridspec.GridSpec(2,2)

        # large subplot
        plt.subplot2grid((2,2), (0,0), colspan=2, rowspan=1)
        plt.locator_params(axis='x', nbins=5)
        plt.locator_params(axis='y', nbins=5)
        #plt.title('LKAS vs. CRO Curvatures')
        plt.suptitle('road '+str(road)+' direction '+str(direction)+ ' layer '+str(layer)+' resolution '+str(resolution)+
                     ' window '+str(window)+' threshold1 '+str(threshold1)+' threshold2 '+str(threshold2)+' threshold3 '+
                     str(threshold3), fontsize=8)        
        plt.xlabel('gridcells')
        plt.ylabel('curvature in 1/m')
        absCurvError[absCurvError == 0] = np.nan
        abserror = np.nanmean(absCurvError)
        plt.annotate('mean abs error: '+str(round(abserror,5))+' 1/m', xy=(0, 0.95), xytext=(6, -6), va='top',
             xycoords='axes fraction', textcoords='offset points', color="black", size=12)
        sns.set_style('darkgrid')        
        plt.plot(vehicleCurvature, color="red")
        plt.plot(afroadCurvature, color="blue")
        a = np.isnan(absCurvError)
        absCurvError[a] = 0
        plt.plot(absCurvError, color="black")
        plt.legend()
        #plt.hist(dist_norm, bins=30, color='0.30')


        # small subplot 2
        plt.subplot2grid((2,2), (1,0))
        plt.locator_params(axis='x', nbins=5)
        plt.locator_params(axis='y', nbins=5)
        plt.title('relative Error')
        plt.xlabel('gridcells')
        plt.ylabel('relative curvrature error in %')
        a = np.isnan(relCurvError)
        relCurvError[a] = 0
        plt.plot(relCurvError, color="green")


        # small subplot 3
        plt.subplot2grid((2,2), (1,1))
        plt.locator_params(axis='x', nbins=5)
        plt.locator_params(axis='y', nbins=5)
        plt.title('relative Error Histogramm')
        plt.xlabel('Frequency')
        plt.ylabel('relative curvrature error in %')
        relCurvError[relCurvError == 0] = np.nan
        relerror = np.nanmean(relCurvError)
        plt.annotate('mean rel error: '+str(round(relerror,2))+' %', xy=(0, 1), xytext=(12, -12), va='top',
             xycoords='axes fraction', textcoords='offset points', color="green", size=12)
        sns.set_style('darkgrid')
        sns.distplot([value for value in relCurvError if not math.isnan(np.float(value))], color="green", vertical=True)


        # fit subplots and save fig
        fig.tight_layout()
        fig.set_size_inches(w=9,h=9)
        return fig