
#ifdef _WIN32
# include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __linux
# include <netinet/in.h>
#endif

#include "ApoClnt.h"
#include "GuiCmd.h"


/* Please note: GuiCmd functionality is only available with CarMaker >= 2.1.9. */


typedef struct tMsg_GuiCmd {
    char	MsgCode[8];
    unsigned	CmdId;
    char	Cmd[APO_ADMMAX-8-4];
} tMsg_GuiCmd;

typedef struct tMsg_GuiRes {
    char	MsgCode[8];
    unsigned	CmdId;
    unsigned	ResultFlags;
    char	Result[APO_ADMMAX-8-4-4];
} tMsg_GuiRes;

static unsigned CmdId = 0;
static int  ResultReceived = 1;

static int  EvalResultStatus = GUI_EOK;
static int  EvalResultSize = 0;
static char EvalResult[APO_ADMMAX+1] = ""; /* Extra space for term. NUL-char. */


static int
reset_result (int status)
{
    const char *result;

    switch (status) {
      case GUI_EOK:
	result = "";
	break;
      case GUI_ETIMEOUT:
	result = "timeout";
	break;
      case GUI_ECONN:
	result = "connection failed";
	break;
      case GUI_ETCL:
	/*FALLTHROUGH*/
      default:
	result = "(reset_result(): unexpected status code)";
	break;
    }

    strcpy(EvalResult, result);
    EvalResultSize = strlen(EvalResult);
    EvalResultStatus = status;

    return status;
}


static int
send_command (tApoSid sid, unsigned cmdid, const char *cmd)
{
    enum { CmdChannel = 2 };

    tMsg_GuiCmd msg;
    int cmdsize, msgsize;

    assert(sizeof(tMsg_GuiCmd) == APO_ADMMAX);
    assert(sizeof(tMsg_GuiRes) == APO_ADMMAX);

    memcpy(msg.MsgCode, "guicmd\0\0", 8);

    msg.CmdId = htonl(cmdid);

    cmdsize = strlen(cmd);
    if (cmdsize > sizeof(msg.Cmd))
	cmdsize = sizeof(msg.Cmd);		/* Truncate silently. */
    memcpy(msg.Cmd, cmd, cmdsize);

    msgsize = &msg.Cmd[cmdsize] - (char *)&msg;
    if (ApocSendAppMsg(sid, CmdChannel, (char *)&msg, msgsize) != 0)
	return -1;

    return 0;
}


static void
store_result (const tMsg_GuiRes *msg, int msglen)
{
    EvalResultStatus = (ntohl(msg->ResultFlags) & 0x1) ? GUI_ETCL : GUI_EOK;

    EvalResultSize = msglen - (msg->Result - (char *)msg);
    memcpy(EvalResult, msg->Result, EvalResultSize);

    /* Guarantee NUL-terminated result for easier string processing. */
    EvalResult[EvalResultSize] = '\0';
}


/*
 * Filter incoming application defined messages when waiting for the
 * result of a Tcl command just issued.
 *
 * This function must be called from ApoClnt_PollAndSleep().
 *
 * Return value:
 *   true (1)		Message carried a Tcl result and was handled by the
 *			function. Proceed with next message.
 *   false (0)		Message did not contain the result of a Tcl command
 *			and should be processed somewhere else.
 */
int
GuiCmd_HandleMsgs (int msgch, const char *msgbuf, int msglen)
{
    if (msglen >= 8 && memcmp(msgbuf, "guires", 7) == 0) {
	const tMsg_GuiRes *msg = (const tMsg_GuiRes *)msgbuf;
	if (!ResultReceived && ntohl(msg->CmdId) == CmdId) {
	    store_result(msg, msglen);
	    ResultReceived = 1;
	} else {
	    /* Throw away result messages arriving too late (timeout!). */
	}
	return 1;
    }
    return 0;
}


static int
wait_for_result (tApoSid sid, int timeout_ms)
{
    double tstart = ApoGetTime_ms();

    do {
	ApoClnt_PollAndSleep();
	if (!(ApocGetStatus(sid, NULL) & ApoConnUp))
	    return -1;			/* Connection error. */
	if (ResultReceived)
	    return 0;			/* Result received. */
    } while (timeout_ms < 0 || ApoGetTime_ms()-tstart < timeout_ms);

    return 1;				/* Timeout. */
}


/*
 * GuiCmd_Eval2()
 *
 * Send a Tcl command to the CarMaker GUI for execution.
 *
 * The function takes a timeout parameter specifying how long to wait for
 * the result of the command. Specify either GUI_WAIT, GUI_NOWAIT or a
 * positive integer value.
 *
 * After the function returns any previous result is lost.
 *
 * Return value:
 *   The status GUI_Exxx describing success or failure of the Tcl command.
 */
int
GuiCmd_Eval2 (tApoSid sid, const char *cmd, int timeout_ms)
{
    int res;

    if (!(ApocGetStatus(sid, NULL) & ApoConnUp))
	return reset_result(GUI_ECONN);

    ++CmdId;
    if (send_command(sid, CmdId, cmd))
	return reset_result(GUI_ECONN);

    reset_result(GUI_EOK);
 
    if (timeout_ms != GUI_NOWAIT) {
	ResultReceived = 0;
	res = wait_for_result(sid, timeout_ms);
	ResultReceived = 1;

	/* ResultReceived!=0 is important for later calls to GuiCmd_HandleMsgs()
	   from outside wait_for_result(), so that late tcl command result
	   messages will be recognized but dropped during normal polling. */
	assert(ResultReceived != 0);

	if (res < 0) {
	    return reset_result(GUI_ECONN);
	} else if (res > 0) {
	    return reset_result(GUI_ETIMEOUT);
	} else {
	    /* EvalResult was set properly in store_result(). */
	}
    }

    return EvalResultStatus;
}


/*
 * GuiCmd_Eval()
 *
 * Like GuiCmd_Eval2(), but waits until the result is sent back.
 */
int
GuiCmd_Eval (tApoSid sid, const char *cmd)
{
    return GuiCmd_Eval2(sid, cmd, GUI_WAIT);
}


/*
 * GuiCmd_GetResult()
 *
 * Retrieve the result returned by the last command issued for evaluation.
 *
 * The function actually returns three values:
 * - A status GUI_Exxx describing success or failure of the Tcl command,
 *   returned as the function's value and identical to the value
 *   by the most recent call to GuiCmd_Eval() or GuiCmd_Eval2().
 * - A pointer to the result string itself,
 *   optionally returned in a variable specified by the caller.
 * - The size of the result,
 *   optionally returned in a variable specified by the caller.
 */
int
GuiCmd_GetResult (const char **presult, int *psize)
{
    if (presult != NULL)
	*presult = EvalResult;
    if (psize != NULL)
	*psize = EvalResultSize;
    return EvalResultStatus;
}


/*
 * GuiCmd_IsReady()
 *
 * A simple smoke test to see if the CarMaker GUI is ready,
 * so that subsequent commands sent for evaluation have a chance to succeed.
 *
 * Return value:
 *   true (1)		If the CarMaker GUI is ready.
 *   false (0)		If the CarMaker GUI is not ready.
 */
int
GuiCmd_IsReady (tApoSid sid)
{
    return GuiCmd_Eval2(sid, "info tclversion", 1000) == GUI_EOK;
}

