#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <apo.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <inttypes.h>

#include<winsock2.h>
//#pragma comment(lib,"ws2_32.lib") //Winsock Library
//
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include<io.h>

#include <errno.h>

//#include "netrx.h"
#include "packet.h"
#include "client_vds_h.h"

#define M_PI 3.14159265358979323846


//---------------------CM_VDS----------------------------------------//


typedef enum {
    SaveFormat_DataNotSaved = 0,
    SaveFormat_Raw,
    SaveFormat_PPM,
    SaveFormat_PGM_byte,
    SaveFormat_PGM_short,
    SaveFormat_PGM_float,
} tSaveFormat;

static struct {
    FILE *EmbeddedDataCollectionFile;
    char *MovieHost;          /* pc on which IPGMovie runs */
    int MoviePort;            /* TCP/IP port for VDS */
    int sock;                 /* TCP/IP CM_Socket */
    int sock_nxp;             /* TCP/IP NXP_Socket */
    char sbuf[64];            /* Buffer for transmitted information */
    int RecvFlags;            /* Receive Flags */
    int Verbose;              /* Logging Output */
    int ConnectionTries;
    tSaveFormat SaveFormat;
    int TerminationRequested;
} VDScfg;

struct Vds_Interface {
    double tFirstDataTime;
    double tStartSim;
    double tEndSim;
    double tLastSimTime;
    unsigned long long int nBytesTotal;
    unsigned long long int nBytesSim;
    unsigned long int nImagesTotal;
    unsigned long int nImagesSim;
    unsigned char nChannels;
} VDSIF;

static void VDS_PrintSimInfo();

// Helpers for VDSIF : VDS information ( stats about current status)
void VDSIF_AddDataToStats(unsigned int len);
void VDSIF_UpdateStats(unsigned int ImgLen, const char *ImgType, int Channel, int ImgWidth, int ImgHeight, float SimTime);
void VDSIF_UpdateEndSimTime();

// misc helpers
//void WriteImgDataToFile(const char* img, unsigned int ImgLen, const char *Img Type, int Channel, int ImgWidth, int ImgHeight, float SimTime);
//void WriteEmbeddedDataToCSVFile(const char* data, unsigned int dataLen, int Channel, float SimTime, const char* AniMode);
//void PrintEmbeddedData (const char* data, unsigned int dataLen);


/*
 ** VDS_RecvHdr
 **
 ** Scan TCP/IP Socket and writes to buffer
 */
static int VDS_RecvHdr(int sock, char *hdr)
{
  const int HdrSize = 64;
  int len = 0;
  int nSkipped = 0;
  int i;

  while (1) {

    if (VDScfg.TerminationRequested)
      return -1;

    for (; len < HdrSize; len += i) {
      if ((i = recv(sock, hdr + len, HdrSize - len, VDScfg.RecvFlags)) <= 0)
{
        if (!VDScfg.TerminationRequested)
          printf ("VDS_RecvHdr Error during recv (received: '%s' (%d))\n", hdr, len);

        return -1;
      }
    }
    if (hdr[0] == '*' && hdr[1] >= 'A' && hdr[1] <= 'Z') {

      /* remove white spaces at end of line */
      while (len > 0 && hdr[len - 1] <= ' ')
        len--;

      hdr[len] = 0;
      if (VDScfg.Verbose == 1 && nSkipped > 0)
        printf("VDS: HDR resync, %d bytes skipped\n", nSkipped);

      return 0;
    }

    for (i = 1; i < len && hdr[i] != '*'; i++)
      ;
    len -= i;
    nSkipped += i;
    memmove(hdr, hdr + i, len);
  }
}


/*
 ** VDS_Connect
 **
 ** Connect over TCP/IP socket
 */
//static int VDS_Connect(void)
static int VDS_Connect(netrx *ptrCliNet)
{
#ifdef WIN32
  WSADATA WSAdata;

  if (WSAStartup(MAKEWORD(2,2), &WSAdata) != 0) {
    fprintf (stderr, "VDS: WSAStartup ((2,2),0) => %d\n", WSAGetLastError());
    return -1;
  }
#endif

  struct sockaddr_in DestAddr;
  //struct hostent *he;
  int tries = VDScfg.ConnectionTries;
  char *serverIP    = ptrCliNet->serverIP;
  //int port          = ptrCliNet->port;

  printf("Socket for %s\n", serverIP);
  fflush(stdout);

  //if ((he = gethostbyname(VDScfg.MovieHost)) == NULL) {
  //  fprintf(stderr, "VDS: unknown host: %s\n", VDScfg.MovieHost);
  //  return -2;
  //}

  // Socket create
  VDScfg.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  printf("\nsock %d\n", VDScfg.sock);
  fflush(stdout);

  // Address given to socket
  DestAddr.sin_family = AF_INET;
  //DestAddr.sin_port = htons(port);
  DestAddr.sin_port = htons(VDScfg.MoviePort);
  DestAddr.sin_addr.s_addr = inet_addr(serverIP);

  printf("Before connecting to %s\n", serverIP);
  fflush(stdout);

  // Connect to the CM_Server
  if (connect(VDScfg.sock, (struct sockaddr *) &DestAddr, sizeof(DestAddr)) <
 0) {

    fprintf(stderr, "VDS: can't connect '%s:%d'\n", serverIP, VDScfg.MoviePort);
  fflush(stdout);

    if (tries > 1) {
      fprintf(stderr, "\tretrying in 1 second... (%d)\n", --tries);
      fflush(stderr);
      sleep(1);
    } else {
      return -4;
    }
  }
  printf("After connected\n");
  fflush(stdout);

  if (VDS_RecvHdr(VDScfg.sock, VDScfg.sbuf) < 0)
    return -3;

  printf("VDS: Connected: %s\n", VDScfg.sbuf + 1);
  fflush(stdout);

  memset(VDScfg.sbuf, 0, 64);

  return 0;
}

/*
 ** VDS_Connect_NXP
 **
 ** Connect over TCP/IP socket
 */
//static int VDS_Connect(void)
static int VDS_Connect_nxp(netrx *ptrCliNet)
{
#ifdef WIN32
  WSADATA WSAdata;

  if (WSAStartup(MAKEWORD(2,2), &WSAdata) != 0) {
    fprintf (stderr, "VDS: WSAStartup ((2,2),0) => %d\n", WSAGetLastError());
    return -1;
  }
#endif

  struct sockaddr_in DestAddr;
  int tries = VDScfg.ConnectionTries;
  char *serverIP    = ptrCliNet->serverIP_nxp;
  int port          = ptrCliNet->port;

  printf("Socket for %s\n", serverIP);
  fflush(stdout);

  // Socket create
  VDScfg.sock_nxp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  printf("\nsock %d\n", VDScfg.sock);
  fflush(stdout);

  // Address given to socket
  DestAddr.sin_family = AF_INET;
  DestAddr.sin_port = htons(port);
  //DestAddr.sin_port = htons(VDScfg.MoviePort);
  DestAddr.sin_addr.s_addr = inet_addr(serverIP);

  printf("Before connecting to %s\n", serverIP);
  fflush(stdout);

  // Connect to the CM_Server
  if (connect(VDScfg.sock_nxp, (struct sockaddr *) &DestAddr, sizeof(DestAddr
)) < 0) {

    fprintf(stderr, "NXP can't connect '%s:%d'\n", serverIP, port);
    fflush(stdout);

    if (tries > 1) {
      fprintf(stderr, "\tretrying in 1 second... (%d)\n", --tries);
      fflush(stderr);
      sleep(1);
    } else {
      return -4;
    }
  }
  printf("After connected\n");
  fflush(stdout);

  //if (VDS_RecvHdr(VDScfg.sock, VDScfg.sbuf) < 0)
    //return -3;

  printf("NXP: Connected.\n\n");
  fflush(stdout);

  //memset(VDScfg.sbuf, 0, 64);

  return 0;
}

static int VDS_GetData(netrx *ptrCliNet)
{
  unsigned int len = 0;
  int res = 0;

  /* Variables for Image Processing */
  char ImgType[32]; //, AniMode[16];
  int ImgWidth, ImgHeight, Channel;
  float SimTime;
  unsigned int ImgLen; //, dataLen;
  //int data[128] = {0x201, 0x61, 0x62};

  if(sscanf(VDScfg.sbuf, "*VDS %d %s %f %dx%d %d", &Channel, ImgType, &SimTime, &ImgWidth, &ImgHeight, &ImgLen) == 6) {

    VDSIF_UpdateStats(ImgLen, ImgType,Channel, ImgWidth, ImgHeight, SimTime);

    if (VDScfg.Verbose == 1)
      printf("\nTEST: %-6.3f : %-2d : %-8s %dx%d %d\n", SimTime, Channel, ImgType, ImgWidth, ImgHeight, ImgLen);

    if (ImgLen > 0) {

      // this is how we get the data
      char *img = (char *) malloc(ImgLen);

      for (len = 0; len < ImgLen; len += res) {

        if ((res = recv(VDScfg.sock, img + len, ImgLen - len, VDScfg.RecvFlags)) < 0) {
          printf("VDS: Socket Reading Failure\n");
          free(img);
          break;
        }
      }

      printf("\nReceive size: %d\n", res);
      // Process the frame
      //process_data(ptrCliNet, img, ImgLen, ImgHeight, ImgWidth);

      // send the FrameData to NXP
      if ((res = send(VDScfg.sock_nxp, (char *) img, ImgLen, VDScfg.RecvFlags)) < 0) {
              printf("VDS: Socket sending Failure\n");
              return -1;
        }
        printf("SENT: %d\n", res);
        fflush(stdout);
    
      // save the data to disc
      //writeImgDataToFile(img, ImgLen, ImgType, Channel, ImgWidth, ImgHeight,SimTime);

      free(img);

      VDSIF_AddDataToStats(len);
    }
  }

  return 0;
}


/*
 ** VDS_Init
 **
 ** Initialize Data Struct
 */
void VDS_Init(void)
{
  //VDScfg.MovieHost = "localhost";
  VDScfg.MoviePort = 2210;
  VDScfg.Verbose = 0;
  VDScfg.SaveFormat = SaveFormat_DataNotSaved;
  VDScfg.EmbeddedDataCollectionFile = NULL;
  VDScfg.RecvFlags = 0;
  VDScfg.ConnectionTries = 5;
  VDScfg.TerminationRequested = 0;

  VDSIF.tFirstDataTime = 0.0;
  VDSIF.tStartSim = 0.0;
  VDSIF.tEndSim = 0.0;
  VDSIF.tLastSimTime = -1.0;
  VDSIF.nImagesSim = 0;
  VDSIF.nImagesTotal = 0;
  VDSIF.nBytesTotal = 0;
  VDSIF.nBytesSim = 0;
  VDSIF.nChannels = 0;
}

static void VDS_PrintSimInfo()
{
  double dtSimReal = VDSIF.tEndSim - VDSIF.tStartSim;
  // at least 1 sec of data is required
  if (dtSimReal > 1.0) {

    printf("\nLast Simulation------------------\n");
    double MiBytes = VDSIF.nBytesSim / (1024.0 * 1024.0);
    printf("Duration: %.3f (real) %.3f (sim) -> x%.2f\n", dtSimReal, VDSIF.tLastSimTime, VDSIF.tLastSimTime / dtSimReal);
    printf("Channels: %d\n", VDSIF.nChannels);
    printf("Images:   %ld (%.3f FPS)\n", VDSIF.nImagesSim, VDSIF.nImagesSim / dtSimReal);
    printf("Bytes:    %.3f MiB (%.3f MiB/s)\n\n", MiBytes, MiBytes / dtSimReal);
  }
  if(VDScfg.EmbeddedDataCollectionFile != NULL)
    fflush(VDScfg.EmbeddedDataCollectionFile);
}

static void VDS_PrintClosingInfo()
{
  // from the very first image to the very last
  double dtSession = VDSIF.tEndSim - VDSIF.tFirstDataTime;
  printf("\n-> Closing VDS-Client...\n");

  // at least 1 sec of data is required
  if (dtSession > 1.0) {

    VDS_PrintSimInfo();
    printf("Session--------------------------\n");
    double MiBytes = VDSIF.nBytesTotal / (1024.0 * 1024.0);
    printf("Duration: %g seconds\n", dtSession);
    printf("Images:   %ld (%.3f FPS)\n", VDSIF.nImagesTotal, VDSIF.nImagesTotal / dtSession);
    printf("Bytes:    %.3f MiB (%.3f MiB per second)\n", MiBytes, MiBytes/ dtSession);
  }
  fflush(stdout);

  if (VDScfg.EmbeddedDataCollectionFile != NULL)
    fclose(VDScfg.EmbeddedDataCollectionFile);
}

// on a system with properly configured timers, calling this function should need less then 0.1us
static inline double GetTime()  // in seconds
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}

void VDSIF_AddDataToStats(unsigned int len)
{
  VDSIF.nImagesTotal++;
  VDSIF.nBytesTotal += len;
  VDSIF.nImagesSim++;
  VDSIF.nBytesSim += len;
}

void VDSIF_UpdateStats(unsigned int ImgLen, const char *ImgType, int Channel, int ImgWidth, int ImgHeight, float SimTime)
{
  if(VDSIF.tFirstDataTime == 0.0)
    VDSIF.tFirstDataTime = GetTime();

  if(SimTime < 0.005 || VDSIF.tLastSimTime < 0) {

    if (Channel == 0) {
      if (VDSIF.tLastSimTime > 0)
        VDS_PrintSimInfo();

      printf("-> Simulation started... (@ %.3f)\n", SimTime);
      VDSIF.tStartSim = GetTime();
      VDSIF.nBytesSim = 0;
      VDSIF.nImagesSim = 0;
      VDSIF.nChannels = 1;
    }

    // this text will appear only for the first img of each channel
    if(VDScfg.Verbose == 2)
      printf("%-6.3f : %-2d : %-8s %dx%d %d\n", SimTime, Channel, ImgType, ImgWidth, ImgHeight, ImgLen);
  }
  if(Channel == 0)
    VDSIF.tLastSimTime = SimTime;

  if(Channel >= VDSIF.nChannels)
    VDSIF.nChannels = Channel + 1;
}

void VDSIF_UpdateEndSimTime()
{
  VDSIF.tEndSim = GetTime();
}

void termination_handler(int signum)
{
  VDScfg.TerminationRequested = 1;
  VDS_PrintClosingInfo();

#ifdef WIN32
  WSACleanup();
#endif

  exit(0);
}


//---------------------DOUBLE-2-STRING-------------------------------//


int main(int argc, char *argv[]) 
{
  
  //-------------------Init & NXP_SERVER CONNECT --------------------//

  const int channel = 1;
  tApoSid sid = NULL;//NULL;
  tApoSubscription subs[6];// Hier steht die Anzahl der aktuellen Abonnements!

 printf(" APOCInit\n");
 ApocInit();

 //unsigned int  time;
 double    time;

 float vel;
 //double lat;
 //double lon;
 float lat;
 float lon;


 // Request type
 PACKET stPacket;

 PACKET *ptr_metadata = (PACKET *) &(stPacket);

 int i;
 char *serverIP;
 char *serverIP_nxp;
 int port;

 netrx cliNet; // TODO use malloc to create netrx object.

 serverIP                      = argv[1];
 serverIP_nxp                  = argv[2];
 port                          = atoi(argv[3]);

 cliNet.serverIP                 = serverIP;
 cliNet.serverIP_nxp           = serverIP_nxp;
 cliNet.port                     = port;

#if 1
  /* Connect to NXP Server*/
 if((i = VDS_Connect_nxp(&cliNet)) != 0) {
   fprintf(stderr, "Can't initialise vds client (returns %d, %s)\n", i, i == -4 ? "No server": strerror(errno));
 }
#endif

#if 1
 //-------------------VDS -> CM_SERVER-----------------------//

 char *appname = argv[0];

  // For add the logo to main Frame to display

printf("VDS_Init\n");
 fflush(stdout);
 VDS_Init();

 printf("entering loop ..\n");
 fflush(stdout);

 // get the options
 while (*++argv != NULL) {

   if(strcmp(*argv, "-v") == 0) {
     VDScfg.Verbose = 1;

   }
 }

 // handle Ctrl-C
if(signal(SIGINT, termination_handler) == SIG_IGN) 
	signal(SIGINT, SIG_IGN);

 printf("connecting ... \n");
 fflush(stdout);
   
 /* Connect to VDS Server */
 if((i = VDS_Connect(&cliNet)) != 0) {
   fprintf(stderr, "Can't initialise vds client (returns %d, %s)\n", i, i == -4 ? "No server": strerror(errno));
 }

#endif 

#if 1
  //-------------------APO -> CM_SERVER-----------------------//

  printf("\nApocQueryServers\n");
  ApocQueryServers(200, NULL);

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
#endif

  //-----------------------ABONNIEREN DER VARIABLEN------------------//

#if 1
  subs[0].Name = "Time";
  subs[1].Name = "Car.Road.GCS.Lat";
  subs[2].Name = "Car.Road.GCS.Long";
  subs[3].Name = "Car.vx";
  subs[4].Name = "Car.Road.Lane.Act.Width";
  subs[5].Name = "Car.Road.Path.DevDist";

  printf(" ApocSubscribe\n");

  //ApocSubscribe(sid, 2, subs, 100, 1, 20, 0);
  ApocSubscribe(sid, 6, subs, 100, 1, 25, 0);

 float width_lane;
 float devdist;
 float dist;

#endif
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
      ptr_metadata->u4_timestampL = time;

      lat  = (*(double*)subs[1].Ptr * 180/M_PI);
      //lat  = (*(double*)subs[1].Ptr);
      ptr_metadata->u4_ins_latitude = lat;
          
      lon  = (*(double*)subs[2].Ptr * 180/M_PI); 
      //lon  = (*(double*)subs[2].Ptr); 
      ptr_metadata->u4_ins_longitude = lon; 

      vel  = (*(float*)subs[3].Ptr * 3600) / 1000;

      width_lane  = (*(float*)subs[4].Ptr); 
     
      devdist  = (*(float*)subs[5].Ptr); 

      dist = (width_lane/2) + devdist - 0.811;
      ptr_metadata->u4_ins_cm_d2l = dist; 


     printf("\nAPO OUTPUT: %f s\t%f\t%f\t%f km/h\n", time, lat, lon, vel);
     printf("APO OUTPUT: %f \t%f\t%f\n", width_lane, devdist, dist) ;

#if 1
      int send_size = send(VDScfg.sock_nxp, (char *)ptr_metadata, sizeof(PACKET), 0);
      if(send_size == -1){
	      printf("\n***Error in sending request for Lane information: [%d] : %s, [%d]\n", errno, strerror(errno), send_size);
              return -1;
      }

      printf("send Size: %d\n", send_size);
     
    /* Read from TCP/IP-Port and process the image */
    if (VDS_RecvHdr(VDScfg.sock, VDScfg.sbuf) == 0) {

	if(VDScfg.TerminationRequested)
	break;

        VDS_GetData(&cliNet);
        fflush(stdout); 
    }
#endif
    }


  }

  return 0;
}

