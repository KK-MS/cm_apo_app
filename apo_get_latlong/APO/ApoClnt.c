
#ifdef _WIN32
# include <winsock2.h>
# include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
#ifdef __linux
# include <unistd.h>
# include <sys/time.h>
# include <netinet/in.h>
#endif

#ifdef _MSC_VER
# define strcasecmp(a, b) _stricmp(a,b)
#endif


#include "ApoClnt.h"


static char *
to_lower (char *str)
{
    char *s;
    for (s=str; *s!='\0'; s++) {
	if ('A'<=*s && *s<='Z')
	    *s += 'a' - 'A';
    }
    return str;
}


/** Making a connection *******************************************************/

enum { QueryTimeout_ms = 1000 };
enum { SleepDelta_ms   = 100  }; /* Long time, used only when disconnected. */


static char *
normalize_hostname (const char *hostname)
{
    const char *newname;
    char buf[256];

    if (strcasecmp(hostname, "localhost") == 0) {
	char *s;
	if (gethostname(buf, sizeof(buf)) != 0)
	    return NULL;
	if ((s = strchr(buf, '.')) != NULL)
	    *s = '\0';		/* Cut off domain part. */
	newname = buf;
    } else {
	newname = hostname;
    }
    return to_lower(strdup(newname));
}


static void
query_for_apps (int timeout_ms, const char *host)
{
    if (strcmp(host, "*") == 0)
	host = NULL;

    ApocQueryServers(timeout_ms, host);
    while (!ApocQueryDone()) {
	if (ApocWaitIO(SleepDelta_ms) == 1)
	    ApocPoll();
    }
}


static int
find_app (const char *appclass, const char *apphost, const char *appuser)
{
    int i, si = -1;
    unsigned long starttime = 0;

    for (i=0; i<ApocGetServerCount(); i++) {
	const tApoServerInfo *sinf = ApocGetServer(i);
	if ((strcmp(appuser,  "*") == 0 || strcmp(appuser,  sinf->Username) == 0)
	 && (strcmp(appclass, "*") == 0 || strcmp(appclass, sinf->AppClass) == 0)) {
	    if (starttime == 0 || starttime < sinf->StartTime) {
		starttime = sinf->StartTime;
		si = i;
	    }
	}
    }

    return si;
}


static tApoSid
connect_to_app (int si, unsigned long channelmask)
{
    tApoSid sid = NULL;
    int res;

    sid = ApocOpenServer(si);
    if ((res = ApocConnect(sid, channelmask)) == ApoErrOk) {
	do {
	    if (ApocWaitIO(SleepDelta_ms) == 1)
		ApocPoll();
	} while ((res = ApocGetStatus(sid, NULL)) == ApoConnPending);
    }
    if (!(ApocGetStatus(sid, NULL) & ApoConnUp)) {
	ApocCloseServer(sid);
	sid = NULL;
    }

    return sid;
}


/*
 * ApoClnt_Connect()
 *
 * Look for an APO server application matching the given criteria and
 * connect to it using the given channel mask.
 *
 * Specifying "*" for the class, the host or the user means "don't care",
 * i.e. any value will match. (Note: Pattern matching is not supported.)
 *
 * Return value:
 *   != NULL		A valid connection handle to the server application.
 *   == NULL		No application found or failed to connect.
 */
tApoSid
ApoClnt_Connect (const char *appclass, const char *apphost,
		 const char *appuser, unsigned long channelmask)
{
    tApoSid sid = NULL;
    char *normhost;
    int si;

    if ((normhost = normalize_hostname(apphost)) == NULL)
	return NULL;

    query_for_apps(QueryTimeout_ms, normhost);
    if ((si = find_app(appclass, normhost, appuser)) >= 0)
	sid = connect_to_app(si, channelmask);

    free(normhost);
    return sid;
}



/** tQuantList datatype *******************************************************/

enum { QuantListInitialSize = 13 };

typedef struct tQuantList {
    int			n, max;
    tApoClnt_Quantity	*Quants;
} tQuantList;


static tQuantList *
quantlist_create (void)
{
    tQuantList *qulist = malloc(sizeof(*qulist));

    qulist->n      = 0;
    qulist->max    = QuantListInitialSize;
    qulist->Quants = malloc(qulist->max * sizeof(*qulist->Quants));

    return qulist;
}


static void
quantlist_destroy (tQuantList *qulist)
{
    int i;
    for (i=0; i<qulist->n; i++)
	free(qulist->Quants[i].Name);
    free(qulist);
}


static tQuantList *
quantlist_clone (const tQuantList *srclist)
{
    tQuantList *dstlist = malloc(sizeof(*dstlist));
    int i;

    dstlist->n      = srclist->n;
    dstlist->max    = srclist->max;
    dstlist->Quants = malloc(srclist->max * sizeof(*dstlist->Quants));

    memcpy(dstlist->Quants, srclist->Quants,
	   srclist->n * sizeof(*dstlist->Quants));
    for (i=0; i<srclist->n; i++)
	dstlist->Quants[i].Name = strdup(srclist->Quants[i].Name);

    return dstlist;
}


static tApoClnt_Quantity *
quantlist_lookup (tQuantList *qulist, const char *name)
{
    int i;
    for (i=0; i<qulist->n; i++) {
	if (strcmp(qulist->Quants[i].Name, name) == 0)
	    return &qulist->Quants[i];
    }
    return NULL;
}


static tApoClnt_Quantity *
quantlist_add (tQuantList *qulist)
{
    if (qulist->n >= qulist->max) {
	qulist->max *= 2;
	qulist->Quants = realloc(qulist->Quants,
				 qulist->max * sizeof(*qulist->Quants));
    }
    return &qulist->Quants[qulist->n++];
}


static void
quantlist_remove (tQuantList *qulist, const char *name)
{
    tApoClnt_Quantity *qu;

    if ((qu = quantlist_lookup(qulist, name)) != NULL) {
	int qupos = qu - qulist->Quants;
	free(qu->Name);
	memmove(qu, qu+1, (qulist->n-1-qupos)*sizeof(*qulist->Quants));
	--qulist->n;
    }
}


static void
quantlist_remove_all (tQuantList *qulist)
{
    int i;
    for (i=0; i<qulist->n; i++)
	free(qulist->Quants[i].Name);
    qulist->n = 0;
}



/** Subscribing to quantities *************************************************/

static int    ClientBacklog;
static int    ServerBacklog;
static double Frequency_Hz;
static int    UseAppTime;

/* Editable quantity list. */
static tQuantList       *EditQuants = NULL;

/* Current active subscription. */
static tQuantList       *SubsQuants = NULL;
static tApoSubscription *Subs       = NULL;

static unsigned long CurrentVecId = 0;


static void
setup_subscription (int clntbacklog, int servbacklog,
		    double freq_hz, int useapptime)
{
    int i;

    SubsQuants = quantlist_clone(EditQuants);

    Subs = malloc(SubsQuants->n * sizeof(*Subs));
    for (i=0; i<SubsQuants->n; i++)
	Subs[i].Name = SubsQuants->Quants[i].Name;

    ClientBacklog = clntbacklog;
    ServerBacklog = servbacklog;
    Frequency_Hz  = freq_hz;
    UseAppTime    = useapptime;
}


static void
teardown_subscription (void)
{
    free(Subs);
    Subs = NULL;

    quantlist_destroy(SubsQuants);
    SubsQuants = NULL;
}


/*
 * Distribute quantity values to the associated C variables of the
 * current subscription.
 *
 * This function must be called from ApoClnt_PollAndSleep().
 *
 * Return value:
 *   true (1)		Received first quantity vector after a new subscription.
 *			Skip remaining polling activities (thus returning
 *			control to ApoClnt_Subscribe()).
 *   false (0)		Quantity vector was handled normally,
 *			continue with normal polling actitivies.
 */
int
ApoClnt_UpdateQuantVars (unsigned long vecid)
{
    const int wasfirstvec = (CurrentVecId == 0);
    int i;

    CurrentVecId = vecid;

    if (EditQuants == NULL)
	return 0;

    for (i=0; i<SubsQuants->n; i++) {
	const void *src = Subs[i].Ptr;
	void       *dst = SubsQuants->Quants[i].VarPtr;
	double      value;
	if (src != NULL) {
	    switch (Subs[i].Type) {
	      case ApoDouble: value = *(            double *)src; break;
	      case ApoFloat:  value = *(             float *)src; break;
	      case ApoLLong:  value = *(  signed long long *)src; break;
	      case ApoULLong: value = *(unsigned long long *)src; break;
	      case ApoInt:    value = *(  signed       int *)src; break;
	      case ApoUInt:   value = *(unsigned       int *)src; break;
	      case ApoShort:  value = *(  signed     short *)src; break;
	      case ApoUShort: value = *(unsigned     short *)src; break;
	      case ApoChar:   value = *(  signed      char *)src; break;
	      case ApoUChar:  value = *(unsigned      char *)src; break;
	      default:        value = 0.0;                    break;
	    }
	    switch (SubsQuants->Quants[i].VarType) {
	      case ApoDouble: *(            double *)dst =                     value; break;
	      case ApoFloat:  *(             float *)dst = (             float)value; break;
	      case ApoLong:   *(  signed long long *)dst = (  signed long long)value; break;
	      case ApoULong:  *(unsigned long long *)dst = (unsigned long long)value; break;
	      case ApoInt:    *(  signed       int *)dst = (  signed       int)value; break;
	      case ApoUInt:   *(unsigned       int *)dst = (unsigned       int)value; break;
	      case ApoShort:  *(  signed     short *)dst = (  signed     short)value; break;
	      case ApoUShort: *(unsigned     short *)dst = (unsigned     short)value; break;
	      case ApoChar:   *(  signed      char *)dst = (  signed      char)value; break;
	      case ApoUChar:  *(unsigned      char *)dst = (unsigned      char)value; break;
	      default:                                        break;
	    }
	}
    }

    return wasfirstvec;
}


static int
subscribe (tApoSid sid, int awaitfirstvec)
{
    int res;

    res = ApocSubscribe(sid, SubsQuants->n, Subs,
			ClientBacklog, ServerBacklog, Frequency_Hz, UseAppTime);
    if (res != ApoErrOk)
	return -1;

    if (awaitfirstvec) {
	enum { timeout_ms = 1000 };
	double tstart = ApoGetTime_ms();
	CurrentVecId = 0;
	do {
	    ApoClnt_PollAndSleep();
	} while (CurrentVecId == 0 && ApoGetTime_ms()-tstart < timeout_ms);
    }

    return 0;
}


/*
 * ApoClnt_Subscribe()
 *
 * Duplicate the editable quantity list and subscribe its quantities,
 * making it the new active subscription.
 *
 * A number of additional parameters need to be specified for the
 * subscription; see ApocSubscribe() for details.
 *
 * Return value:
 *    0			Subscription successful.
 *   -1			Subscription failed.
 */
int
ApoClnt_Subscribe (tApoSid sid, int clntbacklog, int servbacklog,
		   double freq_hz, int useapptime)
{
    int i;

    if (EditQuants == NULL)
	return 0;
    if (Subs != NULL)
	teardown_subscription();

    if (!(ApocGetStatus(sid, NULL) & ApoConnUp))
	return -1;

    setup_subscription(clntbacklog, servbacklog, freq_hz, useapptime);

    if (subscribe(sid, 1) != 0) {
	teardown_subscription();
	return -1;
    }

    for (i=0; i<SubsQuants->n; i++) {
	int b = (Subs[i].Ptr == NULL);
	EditQuants->Quants[i].IsRejected = SubsQuants->Quants[i].IsRejected = b;
    }

    return 0;
}


/*
 * ApoClnt_GetQuants()
 *
 * Return the current active subscription and its number of elements.
 *
 * In case no current active subscription exists (e.g. after calling
 * ApoClnt_Unsubscribe() or after an unsuccessful ApoClnt_Resubscribe())
 * NULL is returned.
 *
 * Call this function immediately after ApoClnt_Subscribe() in order to
 * identify quantities that were rejected by the server (i.e. quantities
 * that could not be subscribed to).
 */
const tApoClnt_Quantity *
ApoClnt_GetQuants (int *pcount)
{
    if (SubsQuants == NULL)
	return NULL;

    *pcount = SubsQuants->n;
    return SubsQuants->Quants;
}


/*
 * Renew the current active subscription in reaction to a dictionary change
 * on the server side.
 *
 * This function must be called from ApoClnt_PollAndSleep().
 */
void
ApoClnt_Resubscribe (tApoSid sid)
{
    if (Subs == NULL)
	return;

    if (subscribe(sid, 0) != 0)
	teardown_subscription();
}


/*
 * ApoClnt_Unsubscribe().
 *
 * Cancel the current active subscription immediately.
 */
void
ApoClnt_Unsubscribe (tApoSid sid)
{
    if (Subs != NULL)
	teardown_subscription();

    if ((ApocGetStatus(sid, NULL) & ApoConnUp))
	ApocSubscribe(sid, 0, NULL, 0, 0, 0, 0);
}


/*
 * ApoClnt_AddQuant()
 *
 * Add the given quantity to the editable quantity list.
 *
 * In addition to the quantity name, the address and data type of
 * the associated C variable must be specified.
 *
 * This change will not affect the current active subscription
 * until the next call to ApoClnt_Subscribe().
 */
void
ApoClnt_AddQuant (const char *name, void *var, tApoQuantType type)
{
    tApoClnt_Quantity *qu;

    if (EditQuants == NULL)
	EditQuants = quantlist_create();

    qu = quantlist_add(EditQuants);
    qu->Name       = strdup(name);
    qu->VarPtr     = var;
    qu->VarType    = type;
    qu->IsRejected = 0;
}


/*
 * ApoClnt_RemoveQuant()
 *
 * Remove the given quantity from the editable quantity list.
 *
 * This change will not affect the current active subscription
 * until the next call to ApoClnt_Subscribe().
 */
void
ApoClnt_RemoveQuant (const char *name)
{
    if (EditQuants == NULL)
	return;

    quantlist_remove(EditQuants, name);
}


/*
 * ApoClnt_RemoveAllQuants()
 *
 * Remove all quantities from the editable quantity list.
 *
 * This change will not affect the current active subscription
 * until the next call to ApoClnt_Subscribe().
 */
void
ApoClnt_RemoveAllQuants (void)
{
    if (EditQuants == NULL)
	return;

    quantlist_remove_all(EditQuants);
}

