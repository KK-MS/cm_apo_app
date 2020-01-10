
#ifdef _WIN32
# include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "DVA.h"

/*
 * Important note: This module can be used stand-alone.
 *
 * Implementation was done using standard APO functionality;
 * there are no dependencies on ApoClnt stuff.
 */


/** Auxiliary functions *******************************************************/

static short EndianAux = 1;
#define IS_BE_ARCH() (((const char *)&EndianAux)[0] == 0)


static int
put_be_int (char *dst, int value)
{
    const char *src = (const char *)&value;
    if (IS_BE_ARCH()) {
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
    } else {
	dst[0] = src[3];
	dst[1] = src[2];
	dst[2] = src[1];
	dst[3] = src[0];
    }
    return sizeof(value);
}


static int
put_be_float (char *dst, float value)
{
    const char *src = (const char *)&value;
    if (IS_BE_ARCH()) {
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
    } else {
	dst[0] = src[3];
	dst[1] = src[2];
	dst[2] = src[1];
	dst[3] = src[0];
    }
    return sizeof(value);
}


static int
put_be_double (char *dst, double value)
{
    const char *src = (const char *)&value;
    if (IS_BE_ARCH()) {
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
	dst[5] = src[5];
	dst[6] = src[6];
	dst[7] = src[7];
    } else {
	dst[0] = src[7];
	dst[1] = src[6];
	dst[2] = src[5];
	dst[3] = src[4];
	dst[4] = src[3];
	dst[5] = src[2];
	dst[6] = src[1];
	dst[7] = src[0];
    }
    return sizeof(value);
}


/** DVA message handling ******************************************************/

/* Duplicated from CarMaker's DirectVarAccess.h. */
typedef enum {
    DVACmd_Value	= 0,		/* Set absolut Value                        */
    DVACmd_Offset	= 1,		/* Value + Offset                           */
    DVACmd_Factor	= 2,		/* Value * Factor                           */
    DVACmd_FacOff	= 3,		/* Value * Factor + Offset                  */

    DVACmd_ValueRamp	= 4,		/* Ramp (Set absolut Value, n)              */
    DVACmd_OffsetRamp	= 5,		/* Value + Ramp(Offset, n)                  */
    DVACmd_FactorRamp	= 6,		/* Value * Ramp(Factor, n)                  */
    DVACmd_FacOffRamp	= 7,		/* Value * Ramp(Factor,n) + Ramp(Offset, n) */

    DVACmd_Expr		= 8,		/* Realtime Expression                      */

    DVACmd_Release	= 127		/* Remove from list of active jobs          */
} tDVACmd;

typedef struct {
    char	MsgCode[8];
    unsigned	Owner;
    char	Cmds[APO_ADMMAX-8-4];
} tMsg_DVAwr;

static tMsg_DVAwr Msg;
static char *MsgStart = (char *)&Msg, *MsgEnd = (char *)&Msg;


void
DVA_BeginWrite (void)
{
    memcpy(Msg.MsgCode, "DVAWR\0\0\0", 8);
    Msg.Owner = 0;			/* Not really used. */

    MsgEnd = Msg.Cmds;
}


static char *
add_cmd (tDVACmd mode, double duration_ms,
	 const char *name, double value0, double value1, double ramp_ms)
{
    const int namesize = strlen(name) + 1;
    const int cmdsize = 1 + namesize
	+ (mode != DVACmd_Release) * (sizeof(double) + sizeof(double))
	+ (mode == DVACmd_FacOff || mode == DVACmd_FacOffRamp) * sizeof(double)
	+ (mode >= DVACmd_ValueRamp && mode <= DVACmd_FacOffRamp) * sizeof(double);

    if (MsgEnd+cmdsize > MsgStart+APO_ADMMAX)
	return NULL;

    char *p = MsgEnd;
    *p++ = mode;

    strcpy(p, name);
    p += namesize;

    if (mode != DVACmd_Release) {
	p += put_be_double(p, duration_ms);
	p += put_be_double(p, value0);			// Value or Factor
	if (mode == DVACmd_FacOff || mode == DVACmd_FacOffRamp)
	    p += put_be_double(p, value1);		// Offset
	if (mode >= DVACmd_ValueRamp && mode <= DVACmd_FacOffRamp)
	    p += put_be_double(p, ramp_ms);
    }

    return MsgEnd = p;
}


static char *
add_expr (double duration_ms, const char *name, const char *expr)
{
    const int namesize = strlen(name) + 1;
    const int exprsize = strlen(expr) + 1;
    const int cmdsize = 1 + namesize + sizeof(double) + exprsize;

    if (MsgEnd+cmdsize > MsgStart+APO_ADMMAX)
	return NULL;

    char *p = MsgEnd;
    *p++ = DVACmd_Expr;

    strcpy(p, name);
    p += namesize;

    p += put_be_double(p, duration_ms);

    strcpy(p, expr);
    p += exprsize;

    return MsgEnd = p;
}


int
DVA_FinishWrite (tApoSid sid)
{
    enum { CmdChannel = 2 };

    int msgsize = MsgEnd-MsgStart;

    assert(sizeof(tMsg_DVAwr) == APO_ADMMAX);

    MsgEnd = MsgStart;

    if (!(ApocGetStatus(sid, NULL) & ApoConnUp))
	return -1;
    if (ApocSendAppMsg(sid, CmdChannel, MsgStart, msgsize) != 0)
	return -1;
    return 0;
}


/** DVA write functions (CarMaker 7 and later) ********************************/

/*
 * Starting with CarMaker 4.0 DVA write access on quantities changed slightly:
 * - DVA writes on quantities, that don't have the ApoPropWritable property
 *   set (i.e. DVAPlace==DVA_None), will be ignored.
 * - Quantities do 'know' their DVALocation, so the 'where'-parameter in
 *   DVA write messages will be ignored. The parameter was kept, however, for
 *   compatibility reasons e.g. when communicating with an older CarMaker.
 *
 *
 * DVA write commands can be issued in two different modes:
 *
 * 1) Unsynchronized
 *    Each DVA command will be sent in an APO message of its own.
 *    Evaluation of subsequent DVA commands in CarMaker will be done individually
 *    for each DVA command and possibly in different simulation cycles.
 *
 *    Example:
 *      DVA_WriteXXX(sid, ..., "quant1", ...);
 *      DVA_WriteXXX(sid, ..., "quant2", ...);
 *          ...
 *      DVA_WriteXXX(sid, ..., "quantn", ...);
 *
 *    Please note that each function requires a valid APO server id.
 *
 * 2) Synchronized
 *    DVA commands can be grouped together in a single APO message.
 *    Evaluation of all commands in a single message is guaranteed to take
 *    place simultaneously and within the same simulation cycle in CarMaker.
 *
 *    Example:
 *      DVA_BeginWrite();
 *      DVA_WriteXXX(NULL, ..., "quant1", ...);
 *      DVA_WriteXXX(NULL, ..., "quant2", ...);
 *          ...
 *      DVA_WriteXXX(NULL, ..., "quantn", ...);
 *      DVA_FinishWrite(sid);
 *
 *    Please note the NULL passed to each command function as the APO server id,
 *    which in this context means something like "collect, but do not send".
 *    Only the final DVA_FinishBlock() requires a valid APO server id as the
 *    DVA message will be sent in this function.
 *
 * Important exception:
 *    DVA_ReleaseAll() always immediately sends a "release all" message.
 *    Existing message buffer contents (from previous DVA_WriteXXX() calls)
 *    are discarded.
 *
 * Return values of all DVA command functions implemented below:
 *   DVA_EOK      No error.
 *   DVA_ECONN    Sending the DVA message with ApocSendAppMsg() failed.
 *   DVA_ETOOMANY The maximum number of DVA commands that can be grouped into
 *                a single message was exceeded.
 */


int
DVA_WriteAbs (tApoSid sid, double duration_ms,
	      const char *name, double value)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_Value, duration_ms, name, value, 0.0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteOffset (tApoSid sid, double duration_ms,
		 const char *name, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_Offset, duration_ms, name, offset, 0.0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactor (tApoSid sid, double duration_ms,
		 const char *name, double factor)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_Factor, duration_ms, name, factor, 0.0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactorOffset (tApoSid sid, double duration_ms,
		       const char *name, double factor, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_FacOff, duration_ms, name, factor, offset, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteAbsRamp (tApoSid sid, double duration_ms, double ramp_ms,
		  const char *name, double value)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_ValueRamp, duration_ms, name, value, 0.0, ramp_ms) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteOffsetRamp (tApoSid sid, double duration_ms, double ramp_ms,
		  const char *name, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_OffsetRamp, duration_ms, name, offset, 0.0, ramp_ms) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactorRamp (tApoSid sid, double duration_ms, double ramp_ms,
		     const char *name, double factor)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_FactorRamp, duration_ms, name, factor, 0.0, ramp_ms) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactorOffsetRamp (tApoSid sid, double duration_ms, double ramp_ms,
			   const char *name, double factor, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_FacOffRamp, duration_ms, name, factor, offset, ramp_ms) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteExpr (tApoSid sid, double duration_ms,
		 const char *name, const char *expr)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_expr(duration_ms, name, expr) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_Release (tApoSid sid, const char *name)
{
    if (sid != NULL)
	DVA_BeginWrite();

    if (add_cmd(DVACmd_Release, 0, name, 0.0, 0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_ReleaseAll (tApoSid sid)
{
    return DVA_Release(sid, "*");
}


/******************************************************************************/
/* CarMaker 6.0 compatible DVA write functions (deprecated)                   */
/******************************************************************************/

void
DVA_BeginWrite_CM6 (void)
{
    memcpy(Msg.MsgCode, "DVAwr\0\0\0", 8);
    Msg.Owner = 0;			/* Not really used. */

    MsgEnd = Msg.Cmds;
}


static char *
add_cmd_cm6 (tDVALocation where, tDVACmd mode, int ncycles,
	     const char *name, double value0, double value1, int ncyclesramp)
{
    const int namesize = strlen(name) + 1;
    const int cmdsize = 3 + sizeof(int) + namesize
	+ sizeof(float)
	+ (mode == DVACmd_FacOff || mode == DVACmd_FacOffRamp) * sizeof(float)
	+ (mode >= DVACmd_ValueRamp) * sizeof(int);
    char *p;

    if (MsgEnd+cmdsize > MsgStart+APO_ADMMAX)
	return NULL;

    p = MsgEnd;
    *p++ = 'f';				/* Set floating point value. */
    *p++ = where;
    *p++ = mode;

    p += put_be_int(p, ncycles);

    strcpy(p, name);
    p += namesize;

    p += put_be_float(p, (float)value0);
    if (mode == DVACmd_FacOff || mode == DVACmd_FacOffRamp)
	p += put_be_float(p, (float)value1);
    if (mode >= DVACmd_ValueRamp)
	p += put_be_int(p, ncyclesramp);

    return MsgEnd = p;
}


int
DVA_FinishWrite_CM6 (tApoSid sid)
{
    enum { CmdChannel = 2 };

    int msgsize = MsgEnd-MsgStart;

    assert(sizeof(tMsg_DVAwr) == APO_ADMMAX);

    MsgEnd = MsgStart;

    if (!(ApocGetStatus(sid, NULL) & ApoConnUp))
	return -1;
    if (ApocSendAppMsg(sid, CmdChannel, MsgStart, msgsize) != 0)
	return -1;
    return 0;
}


/** DVA write functions (CarMaker 6 and older) (deprecated) *******************/

/*
 * Starting with CarMaker 4.0 DVA write access on quantities changed slightly:
 * - DVA writes on quantities, that don't have the ApoPropWritable property
 *   set (i.e. DVAPlace==DVA_None), will be ignored.
 * - Quantities do 'know' their DVALocation, so the 'where'-parameter in
 *   DVA write messages will be ignored. The parameter was kept, however, for
 *   compatibility reasons e.g. when communicating with an older CarMaker.
 *
 *
 * DVA write commands can be issued in two different modes:
 *
 * 1) Unsynchronized
 *    Each DVA command will be sent in an APO message of its own.
 *    Evaluation of subsequent DVA commands in CarMaker will be done individually
 *    for each DVA command and possibly in different simulation cycles.
 *
 *    Example:
 *      DVA_WriteXXX_CM6(sid, ..., "quant1", ...);
 *      DVA_WriteXXX_CM6(sid, ..., "quant2", ...);
 *          ...
 *      DVA_WriteXXX_CM6(sid, ..., "quantn", ...);
 *
 *    Please note that each function requires a valid APO server id.
 *
 * 2) Synchronized
 *    DVA commands can be grouped together in a single APO message.
 *    Evaluation of all commands in a single message is guaranteed to take
 *    place simultaneously and within the same simulation cycle in CarMaker.
 *
 *    Example:
 *      DVA_BeginWrite_CM6();
 *      DVA_WriteXXX_CM6(NULL, ..., "quant1", ...);
 *      DVA_WriteXXX_CM6(NULL, ..., "quant2", ...);
 *          ...
 *      DVA_WriteXXX_CM6(NULL, ..., "quantn", ...);
 *      DVA_FinishWrite_CM6(sid);
 *
 *    Please note the NULL passed to each command function as the APO server id,
 *    which in this context means something like "collect, but do not send".
 *    Only the final DVA_FinishBlock_CM6() requires a valid APO server id as the
 *    DVA message will be sent in this function.
 *
 * Important exception:
 *    DVA_ReleaseAll_CM6() always immediately sends a "release all" message.
 *    Existing message buffer contents (from previous DVA_WriteXXX_CM6() calls)
 *    are discarded.
 *
 * Return values of all DVA command functions implemented below:
 *   DVA_EOK      No error.
 *   DVA_ECONN    Sending the DVA message with ApocSendAppMsg() failed.
 *   DVA_ETOOMANY The maximum number of DVA commands that can be grouped into
 *                a single message was exceeded.
 */


int
DVA_WriteAbs_CM6 (tApoSid sid, tDVALocation where, int ncycles,
		  const char *name, double value)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_Value, ncycles, name, value, 0.0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteOffset_CM6 (tApoSid sid, tDVALocation where, int ncycles,
		     const char *name, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_Offset, ncycles, name, offset, 0.0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactor_CM6 (tApoSid sid, tDVALocation where, int ncycles,
		     const char *name, double factor)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_Factor, ncycles, name, factor, 0.0, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactorOffset_CM6 (tApoSid sid, tDVALocation where, int ncycles,
			   const char *name, double factor, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_FacOff, ncycles, name, factor, offset, -1) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteAbsRamp_CM6 (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
		      const char *name, double value)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_ValueRamp, ncycles, name, value, 0.0, ncyclesramp) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteOffsetRamp_CM6 (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
			 const char *name, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_OffsetRamp, ncycles, name, offset, 0.0, ncyclesramp) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactorRamp_CM6 (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
			 const char *name, double factor)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_FactorRamp, ncycles, name, factor, 0.0, ncyclesramp) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_WriteFactorOffsetRamp_CM6 (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
			       const char *name, double factor, double offset)
{
    if (sid != NULL)
	DVA_BeginWrite_CM6();

    if (add_cmd_cm6(where, DVACmd_FacOffRamp, ncycles, name, factor, offset, ncyclesramp) == NULL)
	return DVA_ETOOMANY;

    if (sid != NULL && DVA_FinishWrite_CM6(sid)!=0)
	return DVA_ECONN;
    return DVA_EOK;
}


int
DVA_ReleaseAll_CM6 (tApoSid sid)
{
    DVA_BeginWrite_CM6();

    add_cmd_cm6(DVA_IO_In, DVACmd_Value, 0, "*", 0.0, 0, -1);

    if (DVA_FinishWrite_CM6(sid) != 0)
	return DVA_ECONN;
    return DVA_EOK;
}

