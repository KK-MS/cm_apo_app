# APO Examples

## APO
The default example from the CarMaker folder.

Path in general:

C:\IPG\carmaker\win64-8.0.2\Examples\APO

## Build APO Example of CarMaker

* Open the msys-2017 which is installed along with CarMaker installation.
* In windows: Generally installed in the path C:/msys-2017
* Click msys.bat
* It will open a terminal
* Change directory the application folder.
* Build the application.

## Basic Terminal commands

* "cd"   : command to Change Directory
* "make" : make or build the application.

### Screen shot of building the application.

Building APO application

![Building application](build_make_app.jpg)

## Run the APO standalone client

* Run the command with the options

[LL-NB-09-APO] 38) ./ApoClntDemo.win64.exe -apphost 10.5.11.93 StartStop
Run 15 seconds of Hockenheim
Run 15 seconds of LaneChange_ISO, saving results to file
Simulation results file 'SimOutput/LL-NB-09/20191127/Examples_VehicleDynamics_Handling_LaneChange_ISO_112513.erg'
[LL-NB-09-APO] 39)

* Observations:
** Carmaker Starts and executes Hockenheim and later Lanechange_iso

