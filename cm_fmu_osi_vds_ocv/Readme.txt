
 Release: The demo on the FMU in the CarMaker consisting of 
          FMI, OSI, VDS and OpenCV tool chains.

 Package consists of following files:
   1. CM_OSI_VDS_OCV.zip => CarMaker Project
   2. CM_OSI_VDS_OCV.mp4 => Project demo video
   3. opencv_include_lib_install.zip => OpenCV 320, Win64 libraries
   4. Project_Report_On_Standards_FMI_OSI_Usage.docx => Provides need for project, environment setup and execution results.
   5. Readme.txt => This readme file

 Procedure:
   1. The FMU DLL is compiled in project directory src_OSMPDummySensor
   2. Compilation command is "make". 
   3. Make sure "Makefile" file is modified to correct opencv libraries.
   4. This generates OSMPDummySensor.dll
   5. Copy the *.dll in Plugin
      Command: "cp OSMPDummySensor.dll ../Plugins/OSMPDummySensor/binaries/win64/OSMPDummySensor.dll"
   6. Open CarMaker GUI window, select the project.
   7. Make video stream as "rgb" format in Movie/VDS.cfg and open IPG Movie window
   8. In CarMaker GUI click "Start & Connect" and then "start" button
   9. Output: 
      9.1 IPG Movie should run
      9.2 OpenCV window of Received data
      9.3 OpenCV window of Edge detection
   10. To close, 
      10.1. In CarMaker GUI menu, disconnect the connection
      10.2. Close the IPG Movie

 Notes:
   1. If the closing of windows are not followed, sometimes application can hang.
   2. The FMU from srcOSMPDummySensor folder is used as recommended by IPG.
   3. Win64 build environment is used.
   4. Trial of external FMU generation:
      4.1. The working FMU setup of the previous CarMaker version is resulting in error in 6.0.3 version.
      4.2. Used FMI bench and found not errors, but still CarMaker is resulting in segmentation fault.
   5. The CarMaker 6.0.3 version is required, as it supports both FMI2.0 and OSI
      5.1. CarMaker Package link: https://ipg-automotive.com/support/Vk56mWu4R1Bw/test-installation-packages-603/ 
      5.2. Recommended to install CarMaker package at directory path: "C:\"
      5.3. CarMaker requires license and should be placed at <installed path>/IPG/etc/ 
      5.4. The License file name should be "Licenses" without any extension
   6. VDS: Apply for the VDS license: https://ipg-automotive.com/support/licenses/test-license/
      6.1. Without VDS license, you can still continue with OSI and FMU concepts.
      6.2. The VDS will be helpful for Camera Sensor modelling.
   7. The OpenCV 320 version is used.
      7.1. Prebuilt OpenCV libraries for Win64 is provided in release package 
      7.2. OpenCV prebuilt zip "opencv_include_lib_install.zip"

