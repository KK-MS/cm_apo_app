
#ifndef __APOCLNT_H__
#define __APOCLNT_H__

#include <apo.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * In order to keep this header file short and concise, for a description of
 * the functions see both the associated .c file and the demo application!
 */


void    ApoClnt_Sleep (int milliseconds);
double  ApoClnt_GetTime_ms (void);

tApoSid ApoClnt_Connect (const char *appclass, const char *apphost,
			 const char *appuser, unsigned long channelmask);

void    ApoClnt_PollAndSleep (void);  /* Provided by the client application. */



typedef struct {
    char		*Name;
    void		*VarPtr;
    tApoQuantType	 VarType;
    int			IsRejected;
} tApoClnt_Quantity;

void ApoClnt_AddQuant    (const char *name, void *var, tApoQuantType type);
void ApoClnt_RemoveQuant (const char *name);
void ApoClnt_RemoveAllQuants (void);

int  ApoClnt_Subscribe   (tApoSid sid, int clntbacklog, int servbacklog,
			     double freq_hz, int useapptime);
void ApoClnt_Unsubscribe (tApoSid sid);
const tApoClnt_Quantity *ApoClnt_GetQuants (int *pcount);

int  ApoClnt_UpdateQuantVars (unsigned long vecid);
void ApoClnt_Resubscribe (tApoSid sid);


#ifdef __cplusplus
}
#endif

#endif /* !defined(__APOCLNT_H__) */

