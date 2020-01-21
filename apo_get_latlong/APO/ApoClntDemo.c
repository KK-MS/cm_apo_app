
#ifdef _WIN32
# include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _MSC_VER
# define strcasecmp(a, b) _stricmp(a,b)
#endif

#include "ApoClnt.h"
#include "GuiCmd.h"
#include "DVA.h"


static void
error_and_exit (const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}


/** Basic APO client infrastructure *******************************************/

/* Connection parameters. */

enum { CmdChannel  = 2             };		/* CarMaker command channel. */
enum { ChannelMask = 1<<CmdChannel };

static const char *AppClass = "CarMaker";
static const char *AppHost  = "*";
static const char *AppUser  = "*";


/* Connection handle to the application (CarMaker in this case). */
static tApoSid Srv;


/* Time to wait between calls to ApoClnt_PollAndSleep().
   The duration must be fine-tuned to the actual subscription parameters used.
   If too short you may hog the CPU, if too long you may loose quantity data. */
static int SleepDelta_ms = 20;


static int
connection_ok (void)
{
    return (ApocGetStatus(Srv, NULL) & ApoConnUp) != 0;
}


/*
 * ApoClnt_PollAndSleep()
 *
 * This function has to be provided by the client application.
 * It will be called from within the ApoClnt code whenever the
 * application needs to wait for something without blocking the
 * rest of the client.
 * 
 * You may adapt the function's code to your needs, provided that
 * the basic APO communication structure is kept as is.
 *
 * Don't assume this function being called with a fixed timing.
 */
void
ApoClnt_PollAndSleep (void)
{
    if (ApocWaitIO(SleepDelta_ms) == 1) {
	ApocPoll();	/* _Always_ poll, even if connection is down. */

	if (connection_ok()) {
	    unsigned long vecid;
	    int msglen, msgch;
	    char msgbuf[APO_ADMMAX];

	    while ((vecid = ApocGetData(Srv)) > 0) {
		if (ApoClnt_UpdateQuantVars(vecid))
		    return;

		/* Put your own quantity data handling code here. */
		/* ... */
	    }
	    if (ApocDictChanged(Srv))
		ApoClnt_Resubscribe(Srv);

	    while (ApocGetAppMsg(Srv, &msgch, msgbuf, &msglen) > 0) {
		if (GuiCmd_HandleMsgs(msgch, msgbuf, msglen))
		    continue;
		if (DVA_HandleMsgs(msgch, msgbuf, msglen))
		    continue;

		/* Put your own message handling code here. */
		/* ... */
	    }
	}
    }

    /* Put your code handling other regular tasks here. */
    /* ... */
}


static void
pause (int milliseconds)
{
    double tstart = ApoGetTime_ms();
    do {
	ApoClnt_PollAndSleep();
    } while (ApoGetTime_ms()-tstart < milliseconds);
}


static void
setup_client (void)
{
    if (ApocInit() != ApoErrOk)
	error_and_exit("Fail to initialize APO library\n");

    Srv = ApoClnt_Connect(AppClass, AppHost, AppUser, ChannelMask);
    if (Srv == NULL)
	error_and_exit("Could not connect\n");

    if (!GuiCmd_IsReady(Srv))
	error_and_exit("GUI not ready\n");
}


static void
teardown_client (void)
{
    ApocCloseServer(Srv);
}


/******************************************************************************/

static void
show_result (void)
{
    const char *result, *s;
    int size, status;

    status = GuiCmd_GetResult(&result, &size);

    switch (status) {
      case GUI_EOK:      s = "GUI_EOK (= no error)";             break;
      case GUI_ETCL:     s = "GUI_ETCL (= tcl error)";           break;
      case GUI_ETIMEOUT: s = "GUI_ETIMEOUT (= timeout expired)"; break;
      case GUI_ECONN:    s = "GUI_ECONN (= connection failed)";  break;
      default:           s = "(unknown result status code)";     break;
    }

    printf("    Result status: %s\n", s);
    printf("    Result size  : %d [characters]\n", size);
    printf("    Result string: '%s'\n", result);
    printf("\n");
}


static void
demo_Basics (void)
{
    printf("Inquire path of the GUI executable\n");
    GuiCmd_Eval(Srv, "info nameofexecutable");
    show_result();

    printf("Execute a command, don't wait for completion\n");
    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/Racetrack_Hockenheim 1");
    show_result();

    printf("Execute a command yielding a tcl error\n");
    GuiCmd_Eval(Srv, "NoSuchCommand 1 2 3");
    show_result();

    printf("Execute a long running command, triggering a timeout\n");
    GuiCmd_Eval2(Srv, "Sleep 5000; expr {6*7}", 2000);
    show_result();

    printf("Check when executing the command, without retrieving the result\n");
    /* GuiCmd_Eval() returns the same status code as GuiCmd_GetResult(). */
    if (GuiCmd_Eval(Srv, "info hostname") == GUI_EOK)
	printf("    Command succeeded (as expected)\n");
    if (GuiCmd_Eval(Srv, "NoSuchCommand") != GUI_EOK)
	printf("    Command failed (as expected)\n");
    if (GuiCmd_Eval2(Srv, "Sleep 5000; expr {6*7}", 2000) == GUI_ETIMEOUT)
	printf("    Command timeout (as expected)\n");

}


/******************************************************************************/

static void
demo_StartStop (void)
{
    const char *result;

    printf("Run 15 seconds of Hockenheim\n");

    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/Racetrack_Hockenheim 1");
    GuiCmd_Eval(Srv, "StartSim");
    GuiCmd_Eval(Srv, "WaitForStatus running");
    GuiCmd_Eval(Srv, "WaitForTime 15");
    GuiCmd_Eval(Srv, "StopSim");
    GuiCmd_Eval(Srv, "WaitForStatus idle 10000");

    if (!connection_ok())
	error_and_exit("Connection lost!\n");

    printf("Run 15 seconds of LaneChange_ISO, saving results to file\n");

    GuiCmd_Eval(Srv, "SaveMode save_all");

    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/LaneChange_ISO");
    GuiCmd_Eval(Srv, "StartSim");
    GuiCmd_Eval(Srv, "WaitForStatus running");
    GuiCmd_Eval(Srv, "WaitForTime 15");
    GuiCmd_Eval(Srv, "StopSim");
    GuiCmd_Eval(Srv, "WaitForStatus idle 10000");

    GuiCmd_Eval(Srv, "GetLastResultFName");
    GuiCmd_GetResult(&result, NULL);
    printf("Simulation results file '%s'\n", result);

    GuiCmd_Eval(Srv, "SetSaveMode collect");
}


/******************************************************************************/

static void
show_rejected_quantities (void)
{
    int i, nquants;
    const tApoClnt_Quantity *quants = ApoClnt_GetQuants(&nquants);
    for (i=0; i<nquants; i++) {
	if (quants[i].IsRejected)
	    printf("    Cannot subscribe to quantity '%s'\n", quants[i].Name);
    }
}


static void
demo_QuantSubscribe (void)
{
    double Time, Car_v, Car_Distance;
    int DM_GearNo, i;
    float Dummy;

    printf("Run 30 seconds of Hockenheim\n");

    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/Racetrack_Hockenheim 1");
    GuiCmd_Eval(Srv, "StartSim");
    GuiCmd_Eval(Srv, "WaitForStatus running");

    /* Initial subscription. */

    printf("Speed over time:\n");
    ApoClnt_AddQuant("Time",         &Time,         ApoDouble);
    ApoClnt_AddQuant("Car.v",        &Car_v,        ApoDouble);
    ApoClnt_AddQuant("DM.GearNo",    &DM_GearNo,    ApoInt);
    ApoClnt_AddQuant("NoSuchQuant",  &Dummy,        ApoFloat);	/* Rejected! */
    ApoClnt_Subscribe(Srv, 0, 0, 50.0, 1);
    show_rejected_quantities();
    do {
	printf("    %6.3f s: %g m/s, currently gear no. %d\n",
	       Time, Car_v, DM_GearNo);
	pause(1000);
    } while (Time < 10.0);

    /* Changing the subscription on the fly. */

    printf("Distance over time:\n");
    ApoClnt_RemoveQuant("Car.v");
    ApoClnt_RemoveQuant("DM.GearNo");
    ApoClnt_AddQuant("Car.Distance", &Car_Distance, ApoDouble);
    ApoClnt_Subscribe(Srv, 0, 0, 50.0, 1);
    show_rejected_quantities();
    do {
	printf("    %6.3f s: %g m\n", Time, Car_Distance);
	pause(1000);
    } while (Time < 20.0);

    /* Unsubscribing on the fly. */

    printf("Several seconds without subscription to any quantity\n");
    ApoClnt_Unsubscribe(Srv);
    for (i=0; i<5; i++) {
	printf("    %6.3f s: %g m no change as expected\n", Time, Car_Distance);
	pause(1000);
    }

    GuiCmd_Eval(Srv, "StopSim");
    GuiCmd_Eval(Srv, "WaitForStatus idle 10000");
}


/******************************************************************************/

static void
demo_DVAWrite (void)
{
    /*
     * - The example code below is probably not very self explaining...
     * - The code below requires a simulation program that
     *   + in User.c:User_DeclQuants() defines the UserOut_* quanties
     *     using DVA access point DVA_VC,
     *   + executes the following line in User.c:User_In() in each
     *     simulation cycle:
     *       UserOut[0] = UserOut[1] = 0.5;
     * - Furthermore, when simulating with CarMaker/Office, simulation speed
     *   ("Perf.") should be set to "realtime".
     */

    printf("Load and start testrun 'Hockenheim'\n");

    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/Racetrack_Hockenheim 1");
    GuiCmd_Eval(Srv, "StartSim");
    GuiCmd_Eval(Srv, "WaitForStatus running");
    pause(2000);

    printf("DVA_WriteAbs(\"UserOut_00\", 1.0) --> 1.0\n");
    pause(2000);
    DVA_WriteAbs(Srv, 1000, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteFactor(\"UserOut_00\", 0.5) --> 0.25\n");
    pause(2000);
    DVA_WriteFactor(Srv, 1000, "UserOut_00", 0.5);
    pause(2000);

    printf("DVA_WriteOffset(\"UserOut_00\", 1.0) --> 1.5\n");
    pause(2000);
    DVA_WriteOffset(Srv, 1000, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteFactorOffset(\"UserOut_00\", 2.0, 0.5) --> 1.5\n");
    pause(2000);
    DVA_WriteFactorOffset(Srv, 1000, "UserOut_00", 2.0, 0.5);
    pause(2000);

    printf("DVA_WriteAbsRamp(\"UserOut_00\", 1.0, 500) --> 0.5 ... 1.0\n");
    pause(2000);
    DVA_WriteAbsRamp(Srv, 1000, 500, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteOffsetRamp(\"UserOut_00\", 1.0, 500) --> 0.5 ... 1.5\n");
    pause(2000);
    DVA_WriteOffsetRamp(Srv, 1000, 500, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteFactorRamp(\"UserOut_00\", 1.0, 500) --> 0.5 ... 1.0\n");
    pause(2000);
    DVA_WriteFactorRamp(Srv, 1000, 500, "UserOut_00", 2.0);
    pause(2000);

    printf("DVA_WriteFactorOffsetRamp(\"UserOut_00\", 1.0, 500) --> 0.5 ... 2.5\n");
    pause(2000);
    DVA_WriteFactorOffsetRamp(Srv, 1000, 500, "UserOut_00", 3.0, 1.0);
    pause(2000);

    printf("DVA_WriteAbs(\"UserOut_00\", 1.0, 5000) --> will be released after 1000\n");
    printf("DVA_ReleaseAll()\n");
    pause(2000);
    DVA_WriteAbs(Srv, 5000, "UserOut_00", 1.0);
    pause(1000);
    DVA_ReleaseAll(Srv);
    pause(2000);

    printf("Synchronized DVA writes:\n");
    printf("  DVA_WriteAbs(\"UserOut_00\",  1.0) --> 1.0, will stop earlier\n");
    printf("  DVA_WriteAbs(\"UserOut_01\", -1.0) --> cos(Qu::Time)\n");
    pause(2000);

    DVA_BeginWrite();
    DVA_WriteAbs(NULL,   500, "UserOut_00",  1.0);
    DVA_WriteExpr(NULL, 1000, "UserOut_01",  "cos(Qu::Time)");
    DVA_FinishWrite(Srv);
    pause(2000);

    GuiCmd_Eval(Srv, "StopSim");
    GuiCmd_Eval(Srv, "WaitForStatus idle 10000");
}


/******************************************************************************/

/* This code and the use of DVA_*_CM6() functions is deprecated. */

static void
demo_DVAWrite_CM6 (void)
{
    /*
     * - The example code below is probably not very self explaining...
     * - The code below requires a simulation program that
     *   + in User.c:User_DeclQuants() defines the UserOut_* quanties
     *     using DVA access point DVA_VC,
     *   + executes the following line in User.c:User_In() in each
     *     simulation cycle:
     *       UserOut[0] = UserOut[1] = 0.5;
     * - Furthermore, when simulating with CarMaker/Office, simulation speed
     *   ("Perf.") should be set to "realtime".
     */

    printf("Load and start testrun 'Hockenheim'\n");

    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/Racetrack_Hockenheim 1");
    GuiCmd_Eval(Srv, "StartSim");
    GuiCmd_Eval(Srv, "WaitForStatus running");
    pause(2000);

    printf("DVA_WriteAbs_CM6(\"UserOut_00\", 1.0) --> 1.0\n");
    pause(2000);
    DVA_WriteAbs_CM6(Srv, DVA_DrivMan, 1000, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteFactor_CM6(\"UserOut_00\", 0.5) --> 0.25\n");
    pause(2000);
    DVA_WriteFactor_CM6(Srv, DVA_DrivMan, 1000, "UserOut_00", 0.5);
    pause(2000);

    printf("DVA_WriteOffset_CM6(\"UserOut_00\", 1.0) --> 1.5\n");
    pause(2000);
    DVA_WriteOffset_CM6(Srv, DVA_DrivMan, 1000, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteFactorOffset_CM6(\"UserOut_00\", 2.0, 0.5) --> 1.5\n");
    pause(2000);
    DVA_WriteFactorOffset_CM6(Srv, DVA_DrivMan, 1000, "UserOut_00", 2.0, 0.5);
    pause(2000);

    printf("DVA_WriteAbsRamp_CM6(\"UserOut_00\", 1.0, 500) --> 0.5 ... 1.0\n");
    pause(2000);
    DVA_WriteAbsRamp_CM6(Srv, DVA_DrivMan, 1000, 500, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteOffsetRamp_CM6(\"UserOut_00\", 1.0, 500) --> 0.5 ... 1.5\n");
    pause(2000);
    DVA_WriteOffsetRamp_CM6(Srv, DVA_DrivMan, 1000, 500, "UserOut_00", 1.0);
    pause(2000);

    printf("DVA_WriteFactorRamp_CM6(\"UserOut_00\", 1.0, 500) --> 0.5 ... 1.0\n");
    pause(2000);
    DVA_WriteFactorRamp_CM6(Srv, DVA_DrivMan, 1000, 500, "UserOut_00", 2.0);
    pause(2000);

    printf("DVA_WriteFactorOffsetRamp_CM6(\"UserOut_00\", 1.0, 500) --> 0.5 ... 2.5\n");
    pause(2000);
    DVA_WriteFactorOffsetRamp_CM6(Srv, DVA_DrivMan, 1000, 500, "UserOut_00", 3.0, 1.0);
    pause(2000);

    printf("DVA_WriteAbs_CM6(\"UserOut_00\", 1.0, 5000) --> will be released after 1000\n");
    printf("DVA_ReleaseAll_CM6()\n");
    pause(2000);
    DVA_WriteAbs_CM6(Srv, DVA_DrivMan, 5000, "UserOut_00", 1.0);
    pause(1000);
    DVA_ReleaseAll_CM6(Srv);
    pause(2000);

    printf("Synchronized DVA writes:\n");
    printf("  DVA_WriteAbs_CM6(\"UserOut_00\",  1.0) -->  1.0, will stop earlier\n");
    printf("  DVA_WriteAbs_CM6(\"UserOut_01\", -1.0) --> -1.0\n");
    pause(2000);
    DVA_BeginWrite_CM6();
    DVA_WriteAbs_CM6(NULL, DVA_DrivMan,  500, "UserOut_00",  1.0);
    DVA_WriteAbs_CM6(NULL, DVA_DrivMan, 1000, "UserOut_01", -1.0);
    DVA_FinishWrite_CM6(Srv);
    pause(2000);

    GuiCmd_Eval(Srv, "StopSim");
    GuiCmd_Eval(Srv, "WaitForStatus idle 10000");
}


/******************************************************************************/

static void
demo_DVARead (void)
{
    int i;

    printf("Load and start testrun 'Hockenheim'\n");

    GuiCmd_Eval(Srv, "LoadTestRun Examples/VehicleDynamics/Handling/Racetrack_Hockenheim 1");
    GuiCmd_Eval(Srv, "StartSim");
    GuiCmd_Eval(Srv, "WaitForStatus running");
    pause(2000);

    printf("Trying a number of DVA reads...\n");

    for (i=0; i<10; i++) {
	static char *names[] = { "Time", "Vhcl.Distance", NULL };

	if (DVA_Read(Srv, names, 1000) == DVA_EOK) {
	    const double *p = DVA_GetResult();
	    printf("\t%s %g\t%s %g\n", names[0], p[0], names[1], p[1]);
	} else {
	    printf("DVA read error\n");
	}

	pause(1000);
    }

    GuiCmd_Eval(Srv, "StopSim");
    GuiCmd_Eval(Srv, "WaitForStatus idle 10000");
}


/******************************************************************************/

static void
usage_and_exit (void)
{
    printf("\n");
    printf("Usage: ApoClntDemo [options] example\n");
    printf("\n");
    printf("Available options:\n");
    printf("  -apphost h    Connect to application on host h\n");
    printf("  -appuser u    Connect to application start by user u\n");
    printf("  -apclass c    Connect to application of class c\n");
    printf("\n");
    printf("Available examples:\n");
    printf("  Basics\n");
    printf("  StartStop\n");
    printf("  QuantSubscribe\n");
    printf("  DVARead\n");
    printf("  DVAWrite\n");
    printf("  DVAWrite_CM6\n");
    printf("\n");

    exit(EXIT_SUCCESS);
}


int
main (int ac, char **av)
{
    const char *example = "";

    while (*++av != NULL) {
	if (       strcmp(*av, "-appclass") == 0 && *(av+1) != NULL) {
	    AppClass = *++av;
	} else if (strcmp(*av, "-apphost")  == 0 && *(av+1) != NULL) {
	    AppHost = *++av;
	} else if (strcmp(*av, "-appuser")  == 0 && *(av+1) != NULL) {
	    AppUser = *++av;
	} else if (strcmp(*av, "-help") == 0 || strcmp(*av, "-h") == 0) {
	    usage_and_exit();
	} else if (**av == '-') {
	    printf("Unknown option: '%s'\n", *av);
	    printf("Run with '-help' to see the usage.\n");
	    exit(EXIT_FAILURE);
	} else {
	    example = *av;
	    break;
	}
    }
    if (strcmp(AppHost, "*") == 0) {
	printf("Please specify at least a host to connect to.\n");
	printf("Run with '-help' to see a list of available options.\n");
	exit(EXIT_FAILURE);
    }

    setup_client();

    if        (strcasecmp(example, "Basics") == 0) {
	demo_Basics();
    } else if (strcasecmp(example, "StartStop") == 0) {
	demo_StartStop();
    } else if (strcasecmp(example, "QuantSubscribe") == 0) {
	demo_QuantSubscribe();
    } else if (strcasecmp(example, "DVAWrite") == 0) {
	demo_DVAWrite();
    } else if (strcasecmp(example, "DVAWrite_CM6") == 0) {
	demo_DVAWrite_CM6();
    } else if (strcasecmp(example, "DVARead") == 0) {
	demo_DVARead();
    } else {
	printf("Invalid example specified.\n");
	printf("Run with '-help' to see a list of available examples.\n");
    }

    teardown_client();

    return EXIT_SUCCESS;
}

