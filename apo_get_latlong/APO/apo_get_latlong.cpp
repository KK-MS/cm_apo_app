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
  tApoSubscription subs[2];// Hier steht die Anzahl der aktuellen Abonnements!


  printf(" APOCInit\n");
  ApocInit();

  //string latstring = "";
  //string lonstring = "";
  //string all = "";
  double lat = 0.0;
  double lon = 0.0;
  //int lat = 0.0;
  //int lon = 0.0;

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

  subs[0].Name = "Car.Road.GCS.Lat";
  subs[1].Name = "Car.Road.GCS.Long";

  printf(" ApocSubscribe\n");
  ApocSubscribe(sid, 2, subs, 100, 1, 20, 0);

  //cout << "Connecting to hello world server" << endl;

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

      lat = ((*(double*)subs[0].Ptr * 180/M_PI) );

      //cout.precision(10);

      //cout << lat << endl;

      lon = ((*(double*)subs[1].Ptr * 180/M_PI)); 

      //cout << "Sending" << endl;
      printf(" %d, %d\n", lat, lon);
    }
  }


  return 0;

}

