
#ifndef __DVACLNT_H__
#define __DVACLNT_H__

#include <apo.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * In order to keep this header file short and concise, for a description of
 * the functions see both the associated .c files and the demo application!
 */


enum {
    DVA_EOK		=  0,	/* Command evaluated without error. */
    DVA_ETOOFEW		= -1,	/* Too few requests (empty message). */
    DVA_ETOOMANY	= -1,	/* Too many requests (message overflow). */
    DVA_ECONN		= -2,	/* APO connection failure. */
    DVA_ETIMEOUT	= -3	/* Evaluation of request takes longer than
				   the specified timeout. */
};


int  DVA_WriteAbs              (tApoSid sid, double duration_ms,
				const char *name, double value);
int  DVA_WriteOffset           (tApoSid sid, double duration_ms,
				const char *name, double value);
int  DVA_WriteFactor           (tApoSid sid, double duration_ms,
				const char *name, double value);
int  DVA_WriteFactorOffset     (tApoSid sid, double duration_ms,
				const char *name, double factor, double offset);

int  DVA_WriteAbsRamp          (tApoSid sid, double duration_ms, double ramp_ms,
				const char *name, double value);
int  DVA_WriteOffsetRamp       (tApoSid sid, double duration_ms, double ramp_ms,
				const char *name, double offset);
int  DVA_WriteFactorRamp       (tApoSid sid, double duration_ms, double ramp_ms,
				const char *name, double factor);
int  DVA_WriteFactorOffsetRamp (tApoSid sid, double duration_ms, double ramp_ms,
				const char *name, double factor, double offset);

int  DVA_WriteExpr (tApoSid sid, double duration_ms,
		    const char *name, const char *expr);

int  DVA_Release    (tApoSid sid, const char *name);
int  DVA_ReleaseAll (tApoSid sid);


/* Functions required for synchronized DVA write access, so that handling
   of multiple writes is guaranteed to happen within a single cycle.
   File DVAWrite.c contains an explanation of the different write modes. */
void DVA_BeginWrite (void);
int  DVA_FinishWrite (tApoSid sid);


int  DVA_Read (tApoSid, char *names[], int timeout_ms);
const double *DVA_GetResult (void);

int  DVA_HandleMsgs (int msgch, const char *msgbuf, int msglen);


/******************************************************************************/
/* CarMaker 6.0 compatible DVA write functions (deprecated)                   */
/******************************************************************************/

/* Duplicated from CarMaker's DirectVarAccess.h. */
typedef enum {
    DVA_IO_In          = 0,
    DVA_IO_Out         = 1,
    DVA_DrivMan        = 2,
    DVA_VehicleControl = 3
} tDVALocation;


/* IMPORTANT NOTE: Starting with CarMaker 4.0 the DVA location 'where' will be ignored! */

int  DVA_WriteAbs_CM6              (tApoSid sid, tDVALocation where, int ncycles,
				    const char *name, double value);
int  DVA_WriteOffset_CM6           (tApoSid sid, tDVALocation where, int ncycles,
				    const char *name, double value);
int  DVA_WriteFactor_CM6           (tApoSid sid, tDVALocation where, int ncycles,
				    const char *name, double value);
int  DVA_WriteFactorOffset_CM6     (tApoSid sid, tDVALocation where, int ncycles,
				    const char *name, double factor, double offset);

int  DVA_WriteAbsRamp_CM6          (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
				    const char *name, double value);
int  DVA_WriteOffsetRamp_CM6       (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
				    const char *name, double offset);
int  DVA_WriteFactorRamp_CM6       (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
				    const char *name, double factor);
int  DVA_WriteFactorOffsetRamp_CM6 (tApoSid sid, tDVALocation where, int ncycles, int ncyclesramp,
				    const char *name, double factor, double offset);

int  DVA_ReleaseAll_CM6 (tApoSid sid);


/* Functions required for synchronized DVA write access, so that handling
   of multiple writes is guaranteed to happen within a single cycle.
   File DVAWrite.c contains an explanation of the different write modes. */
void DVA_BeginWrite_CM6 (void);
int  DVA_FinishWrite_CM6 (tApoSid sid);


#ifdef __cplusplus
}
#endif

#endif /* !defined(__DVACLNT_H__) */
