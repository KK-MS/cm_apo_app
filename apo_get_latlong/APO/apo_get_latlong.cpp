#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <apo.h>
//#include <iostream>
#include <math.h>

#define M_PI 3.14159265358979323846


//---------------------DOUBLE-2-STRING----------------------------------//

int main()
{
  const int channel = 1;
  tApoSid sid = NULL;//NULL;
  tApoSubscription subs[4];// Hier steht die Anzahl der aktuellen Abonnements!


  printf(" APOCInit\n");
  ApocInit();

  int    time;
  float vel;
  double lat;
  double lon;

  //-------------------SERVER VERBINDUNGSAUFBAU-----------------------//

  printf(" ApocQueryServers\n");
  ApocQueryServers(2000, NULL);

  while (!ApocQueryDone())
  {
    if (ApocWaitIO(100) == 1)
    {
      ApocPoll();
    }
  }

  printf(" ApocGetServerCount: %d\n", ApocGetServerCount());
  for (int i = 0; i < ApocGetServerCount(); i++)
  {
    const tApoServerInfo *sinf = ApocGetServer(i);
    if (strcmp(sinf->Identity, "CarMaker 8.0.2 - Car_Generic") == 0)
    {
      sid = ApocOpenServer(i);

      break;
    }

  }

  if (sid == NULL)
  {
    exit(1);
  }

  printf(" ApocConnect\n");
  ApocConnect(sid, 1 << channel);
  while (ApocGetStatus(sid, NULL) == ApoConnPending)
  {
    if (ApocWaitIO(100) == 1)
    {
      ApocPoll();
    }
  }

  if (!(ApocGetStatus(sid, NULL) & ApoConnUp))

  {
    exit(2);
  }


  //-----------------------ABONNIEREN DER VARIABLEN----------------------//

  subs[0].Name = "Time";
  subs[1].Name = "Car.Road.GCS.Lat";
  subs[2].Name = "Car.Road.GCS.Long";
  subs[3].Name = "Car.vx";

  printf(" ApocSubscribe\n");

  //ApocSubscribe(sid, 2, subs, 100, 1, 20, 0);
  ApocSubscribe(sid, 4, subs, 100, 1, 50, 0);

  while (true)
  {
    //sleep(10);
    ApocPoll();
    if (!(ApocGetStatus(sid, NULL) &ApoConnUp))
    {
      break;
    }

    if (ApocGetData(sid) > 0)
    {

      time = (*(double*)subs[0].Ptr);

      //lat  = (*(double*)subs[1].Ptr * 180/M_PI);
      lat  = (*(double*)subs[1].Ptr);
          
      //lon  = (*(double*)subs[2].Ptr * 180/M_PI); 
      lon  = (*(double*)subs[2].Ptr); 

      vel  = (*(float*)subs[3].Ptr * 3600) / 1000;

      printf("APO OUTPUT: %d s\t%f\t%f\t%f km/h\n", time, lat, lon, vel);
    }
  }


  return 0;

}

