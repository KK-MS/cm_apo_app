
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
#include "DVA.h"


/** Auxiliary functions *******************************************************/

static double
get_float (const char *src)
{
    union {
	int i;
	float f;
    } tmp;
    char *dst = (char *)&tmp.i;

    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;

    tmp.i = htonl(tmp.i);
    return tmp.f;
}


/** DVA read functions ********************************************************/

typedef struct {
    char	MsgCode[8];
    int		RecvChannel;
    unsigned	SeqNo;
    char	Names[APO_ADMMAX-8-4-4];
} tMsg_DVArd;

typedef struct {
    char	MsgCode[8];
    unsigned	SeqNo;
    char	Values[APO_ADMMAX-8-4];
} tMsg_DVArdr;

static unsigned SeqNo = 4242;
static int ResultReceived = 1;

enum { MaxReadValues = 20 };			/* $$$ Rather arbitrary limit! */
static int nReadValues = -1;
static double ReadValues[MaxReadValues];


static int
send_request (tApoSid sid, char *names[])
{
    enum { CmdChannel = 2 };

    tMsg_DVArd msg;
    int msgsize;
    char *p, **pname;

    assert(sizeof(tMsg_DVArd)  == APO_ADMMAX);
    assert(sizeof(tMsg_DVArdr) == APO_ADMMAX);

    if (!(ApocGetStatus(sid, NULL) & ApoConnUp))
	return DVA_ECONN;

    memcpy(msg.MsgCode, "DVArd\0\0\0", 8);
    msg.RecvChannel = htonl(CmdChannel);
    msg.SeqNo = htonl(SeqNo);

    nReadValues = 0;
    p = msg.Names;
    for (pname=names; *pname!=NULL; pname++) {
	const int namesize = strlen(*pname) + 1;

	if (p+1+namesize>(char *)&msg+APO_ADMMAX || nReadValues>=MaxReadValues)
	    return DVA_ETOOMANY;

	*p++ = 'f';		/* Read floating point value. */

	strcpy(p, *pname);
	p += namesize;

	ReadValues[nReadValues++] = 0.0;
    }
    if (nReadValues == 0)
	return DVA_ETOOFEW;

    msgsize = p - (char *)&msg;
    if (ApocSendAppMsg(sid, CmdChannel, &msg, msgsize) != 0)
	return DVA_ECONN;

    return DVA_EOK;
}


static void
store_result (const tMsg_DVArdr *msg, int msglen)
{
    const char *p = msg->Values;
    int i;

    for (i=0; i<nReadValues; i++) {
	ReadValues[i] = get_float(p);
	p += sizeof(float);
    }
}


/*
 * Filter incoming application defined messages when waiting for the
 * result of a DVA read message just issued.
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
DVA_HandleMsgs (int msgch, const char *msgbuf, int msglen)
{
    if (msglen >= 12 && memcmp(msgbuf, "DVArdr", 7) == 0) {
	const tMsg_DVArdr *msg = (const tMsg_DVArdr *)msgbuf;

	if (!ResultReceived && ntohl(msg->SeqNo) == SeqNo) {
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
	    return DVA_ECONN;
	if (ResultReceived)
	    return DVA_EOK;
    } while (timeout_ms < 0 || ApoGetTime_ms()-tstart < timeout_ms);

    return DVA_ETIMEOUT;
}


/*
 * DVA_Read()
 *
 * Read one or more quantity values from CarMaker.
 *
 * The function takes a NULL-terminated list of quantity names and a
 * timeout parameter specifying how long to wait for the result of the
 * read request. Specify a positive integer value for the timeout.
 *
 * After the function returns any previously received values are lost.
 *
 * Return value:
 *   The status DVA_Exxx describing success or failure of the request.
 */
int
DVA_Read (tApoSid sid, char *names[], int timeout_ms)
{
    int res;

    ++SeqNo;
    if ((res = send_request(sid, names)) != DVA_EOK) {
	nReadValues = -1;
	return res;
    }

    ResultReceived = 0;
    res = wait_for_result(sid, timeout_ms);
    ResultReceived = 1;

    /* ResultReceived!=0 is important for later calls to DVA_HandleMsgs()
       from outside wait_for_result(), so that late DVA read result
       messages will be recognized but dropped during normal polling. */
    assert(ResultReceived != 0);

    if (res != DVA_EOK) {
	nReadValues = -1;
	return res;
    } 

    /* Received values were fetched in store_result(). */

    return DVA_EOK;
}


/*
 * DVA_GetResult()
 *
 * Retrieve the result returned by the last DVA read request.
 *
 * The function returns a pointer to an array containing the read values
 * in the same order as issued in the read request. If an error occured
 * during DVA_Read(), NULL is returned.
 */
const double *
DVA_GetResult (void)
{
    return nReadValues>=0 ? ReadValues : NULL;
}

