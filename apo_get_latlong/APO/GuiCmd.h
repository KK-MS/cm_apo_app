
#ifndef __GUICMD_H__
#define __GUICMD_H__

#include <apo.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * In order to keep this header file short and concise, for a description of
 * the functions see both the associated .c file and the demo application!
 */

/* Please note: GuiCmd is functionality only available with CarMaker >= 2.1.9. */

enum {
    GUI_NOWAIT		=  0,	/* Don't wait for result. */
    GUI_WAIT		= -1	/* Wait for result (possibly forever). */
 /* timeout [ms] > 0 */		/* Wait for result or until timeout expires. */
};

enum {
    GUI_EOK		=  0,	/* Command evaluated without error. */
    GUI_ETCL		= -1,	/* Command led to a Tcl error. */
    GUI_ECONN		= -2,	/* APO connection failure. */
    GUI_ETIMEOUT	= -3	/* Evaluation of command takes longer than
				   the specified timeout. */
};

int  GuiCmd_IsReady (tApoSid sid);
int  GuiCmd_Eval    (tApoSid sid, const char *cmd);
int  GuiCmd_Eval2   (tApoSid sid, const char *cmd, int timeout_ms);
int  GuiCmd_GetResult (const char **presult, int *psize);

int  GuiCmd_HandleMsgs (int msgch, const char *msgbuf, int msglen);


#ifdef __cplusplus
}
#endif

#endif /* !defined(__GUICMD_H__) */
