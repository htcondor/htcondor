#include "condor_common.h"
#include "_condor_fix_resource.h"
#include <iostream.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>

#include "condor_classad.h"
#include "condor_parser.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "internet.h"
#include "url_condor.h"
#include "fdprintf.h"

#include "condor_attributes.h"
#include "collector_engine.h"

static char *_FileName_ = __FILE__;

extern char *CondorAdministrator;

#if defined(USE_XDR)
extern "C" XDR *xdr_Udp_Init ();
extern "C" int xdr_context (XDR *, CONTEXT *);
static bool backwardCompatibility (int &, ClassAd *&, XDR *);
#endif

static void killHashTable (CollectorHashTable &);
static ClassAd* updateClassAd(CollectorHashTable&,char*,ClassAd*,HashKey&,
							  char*, int &);
int 	engine_clientTimeoutHandler (Service *);
int 	engine_housekeepingHandler  (Service *);
int		email (char *, char * = NULL); 
char	*strStatus (ClassAd *);

// pointer values for representing master states
static	ClassAd	*RECENTLY_DOWN  = (ClassAd *) 0x1;
static	ClassAd *DONE_REPORTING = (ClassAd *) 0x2;
static	ClassAd *LONG_GONE		= (ClassAd *) 0x3;
static	ClassAd *THRESHOLD		= (ClassAd *) 0x4;

CollectorEngine::
CollectorEngine (TimerManager *timerManager) : 
    StartdAds     	(GREATER_TABLE_SIZE, &hashFunction),
	StartdPrivateAds(GREATER_TABLE_SIZE, &hashFunction),
	ScheddAds     	(GREATER_TABLE_SIZE, &hashFunction),
	MasterAds     	(GREATER_TABLE_SIZE, &hashFunction),
	CkptServerAds 	(LESSER_TABLE_SIZE , &hashFunction),
	GatewayAds    	(LESSER_TABLE_SIZE , &hashFunction)
{
	timeToClean = false;
	clientTimeout = 20;
	machineUpdateInterval = 30;
	masterCheckInterval = 10800;
	housekeeperTimerID = -1;
	masterCheckTimerID = -1;
	clientSocket = -1;
	pvtAds = false;
	if (timerManager == NULL)
	{
		EXCEPT ("Require timer manager!");
	}
	timer = timerManager;
}


CollectorEngine::
~CollectorEngine ()
{
	killHashTable (StartdAds);
	killHashTable (StartdPrivateAds);
	killHashTable (ScheddAds);
	killHashTable (MasterAds);
	killHashTable (CkptServerAds);
	killHashTable (GatewayAds);
}


int CollectorEngine::
setClientTimeout (int timeout)
{
	if (timeout <= 0)
		return 0;
	clientTimeout = timeout;
	return 1;
}


int CollectorEngine::
scheduleHousekeeper (int timeout)
{
	// cancel outstanding housekeeping requests
	if (housekeeperTimerID != -1)
	{
		(void) timer->CancelTimer (housekeeperTimerID);
	}

	// reset for new timer
	if (timeout < 0) 
		return 0;

	// set to new timeout interval
	machineUpdateInterval = timeout;

	// if timeout interval was non-zero (i.e., housekeeping required) ...
	if (timeout > 0)
	{
		// schedule housekeeper
		housekeeperTimerID = timer->NewTimer (this, machineUpdateInterval,
									(void *) &engine_housekeepingHandler,
									machineUpdateInterval);
		if (housekeeperTimerID == -1)
			return 0;
	}

	return 1;
}


int CollectorEngine::
scheduleDownMasterCheck (int timeout)
{
	// cancel outstanding check requests
	if (masterCheckTimerID != -1) {
		(void) timer->CancelTimer (masterCheckTimerID);
	}

	// reset for new timer
	if (timeout < 0) {
		return 0;
	}

	// set to new timeout
	masterCheckInterval = timeout;

	// if timeout interval was non-zero (i.e., master checks required) ...
    if (timeout > 0) {
        // schedule master checks
        masterCheckTimerID = timer->NewTimer (this, masterCheckInterval,
                                    (void *) &engine_masterCheckHandler,
                                    masterCheckInterval);
        if (masterCheckTimerID == -1)
            return 0;
    }

    return 1;
}

void CollectorEngine::
toggleLogging (void)
{
	log = !log;
}


void CollectorEngine::
wantStartdPrivateAds (bool flag)
{
	pvtAds = flag;
}


int CollectorEngine::
walkHashTable (AdTypes adType, int (*scanFunction)(ClassAd *))
{
	CollectorHashTable *table;
	int retval = 1;

	ClassAd *ad;

	switch (adType)
	{
	  case STARTD_AD:
		table = &StartdAds;
		break;

	  case SCHEDD_AD:
		table = &ScheddAds;
		break;

	  case MASTER_AD:
		table = &MasterAds;
		break;

	  case CKPT_SRVR_AD:
		table = &CkptServerAds;
		break;

	  case STARTD_PVT_AD:
		table = &StartdPrivateAds;	
		break;

	  default:
		dprintf (D_ALWAYS, "Unknown type %d\n", adType);
		return 0;
	}

	// walk the hash table
	table->startIterations ();
	while (table->iterate (ad))
	{
		// call scan function for each ad
		if (!scanFunction (ad))
		{
			retval = 0;
			break;
		}
	}

	return 1;
}

ClassAd *CollectorEngine::
collect (int command, Sock *sock, sockaddr_in *from, int &insert)
{
	ClassAd	*clientAd;
	int		timerID;
	ClassAd *rval;

	// start timer
	timerID=timer->NewTimer(this,clientTimeout,(void*)
								engine_clientTimeoutHandler);
	if (timerID == -1)
	{
		EXCEPT ("Could not allocate timer");
	}

	// don't need backward compatibility here --- new daemons will use I/O
	if (!(clientAd = new ClassAd)) 
	{
		EXCEPT ("Memory error!");
	}

	// get the ad
	if (!clientAd->get(*sock))
	{
		dprintf (D_ALWAYS,"Command %d on Sock not follwed by ClassAd\n",
				command);
		delete clientAd;
		sock->end_of_message();
		return 0;
	}
	
	// the above includes a timed communication with the client
	(void) timer->CancelTimer (timerID);

	rval = collect(command, clientAd, from, insert, sock);

	// get the end_of_message()
	if (!sock->end_of_message())
	{
		dprintf(D_FULLDEBUG,"Warning: Command %d; maybe shedding data on eom\n",
				 command);
	}
	
	return rval;
}

#if defined(USE_XDR)
ClassAd *CollectorEngine::
collect (int command, XDR *xdrs, sockaddr_in *from, int &insert)
{
	ClassAd  *clientAd;
	int      timerID;

	// start timer
	timerID = timer->NewTimer(this,clientTimeout,(void *)engine_clientTimeoutHandler);
	if (timerID == -1)
	{
		EXCEPT ("Could not allocate timer");
	}

	// so that old startds and schedds can talk to the new collector
	if (!backwardCompatibility (command, clientAd, xdrs)) return NULL;

	// the above includes a timed xdr communication with the client
	(void) timer->CancelTimer (timerID);

	return (collect(command, clientAd, from, insert));
}
#endif

ClassAd *CollectorEngine::
collect (int command,ClassAd *clientAd,sockaddr_in *from,int &insert,Sock *sock)
{
	ClassAd	*retVal;
	ClassAd	*pvtAd;
	int		insPvt;
	HashKey hk;
	char    hashString [64];
	
	// safer to housekeep now if necessary
 	if (timeToClean)
	{
		timeToClean = false;
		housekeeper ();
	}

	// check for down masters
	if (timeToCheckMasters)
	{
		timeToCheckMasters = false;
		masterCheck ();
	}

	// mux on command
	switch (command)
	{
	  case UPDATE_STARTD_AD:
		if (!makeStartdAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		sprintf (hashString, "< %s , %s >", hk.name, hk.ip_addr);
		retVal=updateClassAd (StartdAds, "StartdAd  ", clientAd, hk, 
							  hashString, insert);

		// if we want to store private ads
		if (pvtAds)
		{
			if (!sock)
			{
				dprintf (D_ALWAYS, "Want private ads, but no socket given!\n");
				break;
			}
			else
			{
				if (!(pvtAd = new ClassAd))
				{
					EXCEPT ("Memory error!");
				}
				if (!pvtAd->get(*sock))
				{
					dprintf(D_ALWAYS,"\t(Could not get startd's private ad)\n");
					break;
				}
				
				// insert the private ad into its hashtable --- use the same
				// hash key as the public ad
				(void) updateClassAd (StartdPrivateAds, "StartdPvt ", pvtAd,
										hk, hashString, insPvt);
			}
		}
		break;

	  case UPDATE_SCHEDD_AD:
		if (!makeScheddAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		sprintf (hashString, "< %s , %s >", hk.name, hk.ip_addr);
		retVal=updateClassAd (ScheddAds, "ScheddAd  ", clientAd, hk,
							  hashString, insert);
		break;

	  case UPDATE_MASTER_AD:
		if (!makeMasterAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		sprintf (hashString, "< %s >", hk.name);
		retVal=updateClassAd (MasterAds, "MasterAd  ", clientAd, hk, 
							  hashString, insert);
		break;

	  case UPDATE_CKPT_SRVR_AD:
		if (!makeCkptSrvrAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		sprintf (hashString, "< %s >", hk.name);
		retVal=updateClassAd (CkptServerAds, "CkptSrvrAd", clientAd, hk, 
							  hashString, insert);
		break;

	  case QUERY_STARTD_ADS:
	  case QUERY_SCHEDD_ADS:
	  case QUERY_MASTER_ADS:
	  case QUERY_CKPT_SRVR_ADS:
	  case QUERY_STARTD_PVT_ADS:
		// these are not implemented in the engine, but we allow another
		// daemon to detect that these commands have been given
	    insert = -2;
		retVal = 0;
	    break;

	  default:
		dprintf (D_ALWAYS, "Received illegal command: %d\n", command);
		insert = -1;
		retVal = 0;
	}

	// return the updated ad
	return retVal;
}

ClassAd *CollectorEngine::
lookup (AdTypes adType, HashKey &hk)
{
	ClassAd *val;

	switch (adType)
	{
		case STARTD_AD:
			if (StartdAds.lookup (hk, val) == -1)
				return 0;
			break;

		case SCHEDD_AD:
			if (ScheddAds.lookup (hk, val) == -1)
				return 0;
			break;

		case MASTER_AD:
			if (MasterAds.lookup (hk, val) == -1)
				return 0;
			break;

		case CKPT_SRVR_AD:
			if (CkptServerAds.lookup (hk, val) == -1)
				return 0;
			break;

		case STARTD_PVT_AD:
			if (StartdPrivateAds.lookup (hk, val) == -1)
				return 0;
		default:
			val = 0;
	}

	return val;
}


int CollectorEngine::
remove (AdTypes adType, HashKey &hk)
{
	switch (adType)
	{
		case STARTD_AD:
			return !StartdAds.remove (hk);

		case SCHEDD_AD:
			return !ScheddAds.remove (hk);

		case MASTER_AD:
			return !MasterAds.remove (hk);

		case CKPT_SRVR_AD:
			return !CkptServerAds.remove (hk);

		case STARTD_PVT_AD:
			return !StartdPrivateAds.remove (hk);

		default:
			return 0;
	}
}
	
				
static ClassAd *
updateClassAd (CollectorHashTable &hashTable, 
			   char *adType, 
			   ClassAd *ad, 
			   HashKey &hk,
			   char *hashString,
			   int  &insert)
{
	ClassAd  *old_ad, *new_ad;
	ExprTree *tree;
	char     buf [40];
	time_t   now;

	// timestamp the ad
	if (tree = ad->Lookup (ATTR_LAST_HEARD_FROM))
	{
		delete tree;
	}	
	(void) time (&now);
	if (now == (time_t) -1)
	{
		EXCEPT ("Error reading system time!");
	}	
	sprintf (buf, "%s = %d", ATTR_LAST_HEARD_FROM, now);
	ad->Insert (buf);

	// this time stamped ad is the new ad
	new_ad = ad;

	// check if it already exists in the hash table ...
	if (hashTable.lookup (hk, old_ad) == -1)
    {	 	
		// no ... new ad
		dprintf (D_ALWAYS, "%s: Inserting ** %s\n", adType, hashString);
		if (hashTable.insert (hk, new_ad) == -1)
		{
			EXCEPT ("Error inserting ad (out of memory)");
		}
		
		insert = 1;
		return new_ad;
	}
	else
    {
		// yes ... old ad must be updated
		dprintf (D_ALWAYS, "%s: Updating ... %s\n", adType, hashString);

		// check if it has special status (master ads)
		if (old_ad < THRESHOLD)
		{
			dprintf (D_ALWAYS, "** Master %s rejuvenated from %s\n", hashString,
								strStatus(old_ad));
			if (hashTable.remove (hk)==-1 || hashTable.insert (hk, new_ad)==-1)
			{
				EXCEPT ("Error updating ad (probably out of memory)");
			}
		}
		else
		{
			old_ad->ExchangeExpressions (new_ad);
			delete new_ad;
		}

		insert = 0;
		return old_ad;
	}
}		


void CollectorEngine::
checkMasterStatus (ClassAd *ad)
{
	HashKey 	hk;
	ClassAd		*old;
	ClassAd		*newStatus;

	// make the master ad's hashkey
	if (!makeMasterAdHashKey (hk, ad, NULL))
	{
		dprintf (D_ALWAYS, "Aborting check on master status\n");
		return;
	}

	// check if the ad is absent
	if (MasterAds.lookup (hk, old) == -1)
	{
		// ad was not there ... enter status as RECENTLY_DOWN
		dprintf(D_ALWAYS,"WARNING:  No master ad for < %s >\n",hk.name);
		if (MasterAds.insert (hk, RECENTLY_DOWN) == -1)
		{
			EXCEPT ("Out of memory");
		}
	}
	else
	if (old == LONG_GONE)
	{
		if (MasterAds.remove(hk)==-1 || MasterAds.insert(hk,DONE_REPORTING)==-1)
		{
			EXCEPT ("Out of memory");
		}
	}
}


void CollectorEngine::
housekeeper (void)
{
	time_t now;
   
	(void) time (&now);
	if (now == (time_t) -1)
	{
		dprintf (D_ALWAYS, 
				 "Housekeeper:  Error in reading system time --- aborting\n");
		return;
	}

	dprintf (D_ALWAYS, "Housekeeper:  Ready to clean old ads\n");

	dprintf (D_ALWAYS, "\tCleaning StartdAds ...\n");
	cleanHashTable (StartdAds, now, makeStartdAdHashKey);

	if (pvtAds)
	{
		dprintf (D_ALWAYS, "\tCleaning StartdPrivateAds ...\n");
		cleanHashTable (StartdAds, now, makeStartdAdHashKey);
	}

	dprintf (D_ALWAYS, "\tCleaning ScheddAds ...\n");
	cleanHashTable (ScheddAds, now, makeScheddAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning MasterAds ...\n");
	cleanHashTable (MasterAds, now, makeMasterAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning CkptServerAds ...\n");
	cleanHashTable (CkptServerAds, now, makeCkptSrvrAdHashKey);

	// add other ad types here ...
	dprintf (D_ALWAYS, "Housekeeper:  Done cleaning\n");
}


void CollectorEngine::
cleanHashTable (CollectorHashTable &hashTable, time_t now, 
				bool (*makeKey) (HashKey &, ClassAd *, sockaddr_in *))
{
	ClassAd  *ad;
	ExprTree *tree;
	int   	 timeStamp;
	HashKey  hk;
	double   timeDiff;
	char     hkString [128];

	hashTable.startIterations ();
	while (hashTable.iterate (ad))
	{
		// ignore ads less than threshold
		if (ad < THRESHOLD) continue;

		// Read the timestamp of the ad
		if (!ad->LookupInteger (ATTR_LAST_HEARD_FROM, timeStamp))
		{
			dprintf (D_ALWAYS, "\t\tError looking up time stamp on ad\n");
			continue;
		}

		// check if it has expired
		timeDiff = difftime (now, timeStamp);
		if (timeDiff > (double) machineUpdateInterval)
		{
			(*makeKey) (hk, ad, NULL);
			hk.sprint (hkString);
			dprintf (D_ALWAYS,"\t\t**** Removing stale ad: %s\n", hkString);
			if (hashTable.remove (hk) == -1)
			{
				dprintf (D_ALWAYS, "\t\tError while removing ad\n");
			}
			if (ad > THRESHOLD)
			{
				delete ad;
			}
		}
	}
}


void CollectorEngine::
masterCheck ()
{
	ClassAd  *ad;
	ClassAd	 *nextStatus;
	double	 timeDiff;
	HashKey  hk;
	char     hkString [128];
	char	 whoami [128];
	char	 buffer [128];
	int 	 mailer = -1;
	int		 more;

	dprintf (D_ALWAYS, "MasterCheck:  Checking for down masters ...\n");

	MasterAds.startIterations ();
	more = MasterAds.iterate (ad);
	while (more) 
	{
		// process the current ad
		if (MasterAds.getCurrentKey (hk) == -1) 
		{
			dprintf (D_ALWAYS, "Master entry exists, but could not get key");
			more = MasterAds.iterate (ad);
			continue;
		}
		hk.sprint (hkString);

		nextStatus = NULL;

		// don't need to process regular ads
		if (ad > THRESHOLD) {
			more = MasterAds.iterate (ad);
			continue;
		}

		// if the master has been dead for a while and children are dead ...
		if (ad == LONG_GONE) {
			dprintf (D_ALWAYS,"\tMaster %s: purging dead entry\n", hkString);
			if (MasterAds.remove (hk) == -1) 
			{
				dprintf (D_ALWAYS, "\tError while removing LONG_GONE ad\n");
			}
			more = MasterAds.iterate (ad);
			continue;
		} 

		// need to report for recently down masters (children still alive)
		if (ad == RECENTLY_DOWN) {
			// if the mailer is not open, open it now
			if (mailer < 0) {
				if (get_machine_name (whoami) == -1) {
					dprintf (D_ALWAYS, "Failed get_machine_name() call\n");
					whoami[0] = '\0';
				}
				sprintf (buffer, "Condor Collector (%s): Dead condor_masters", 
							whoami);
				if ((mailer = email (buffer)) < 0) {
					dprintf (D_ALWAYS, "Error sending email --- aborting\n");
					return;
				}
				fdprintf (mailer, "The following masters are dead, leaving"
							" orphaned daemons\n\n");
			}
			fdprintf (mailer, "\t\t%s\n", hkString);
			dprintf (D_ALWAYS, "\tMaster %s: recently went down\n", hkString);
			nextStatus = DONE_REPORTING;
		}	
		else 
		// the master is dead, but we've already sent mail about it
		if (ad == DONE_REPORTING) {
			dprintf(D_ALWAYS,"\tMaster %s: death already reported\n",hkString);
			nextStatus = LONG_GONE;
		}

		// We will soon modify the data structure that we are iterating
		// over.  This causes the same "entry" to be traversed more than once
		// Therefore, we first perform the reinitialization of the iteration
		// before modifying the hashtable
		more = MasterAds.iterate (ad);

		// need to change the status of the ad
		if (nextStatus != NULL) {
			if (MasterAds.remove(hk)==-1||MasterAds.insert(hk,nextStatus)==-1) {
				EXCEPT ("Could not change status of master ad");
			}
		}
	}

	// close the mailer if necessary
	if (mailer > -1) close (mailer);

	dprintf (D_ALWAYS, "MasterCheck:  Done checking for down masters\n");
}


int  
engine_clientTimeoutHandler (Service *x)
{
    if (((CollectorEngine *)x)->clientSocket == -1)
	{
		dprintf (D_ALWAYS, "Alarm: Got alarm, but have no client socket\n");
		return 0;
	}
    
    dprintf (D_ALWAYS, 
			 "Alarm: Client took too long (over %d secs) -- aborting\n",
			 ((CollectorEngine *)x)->clientTimeout);

	(void) close (((CollectorEngine *)x)->clientSocket);
    ((CollectorEngine *)x)->clientSocket = -1;

	return 0;
}


int
engine_housekeepingHandler (Service *x)
{
	dprintf (D_ALWAYS, "Alarm: Ready to clean out old ads ...\n");
	((CollectorEngine *)x)->timeToClean = true;

	return 0;
}


int
engine_masterCheckHandler (Service *x)
{
	dprintf (D_ALWAYS, "Alarm: Ready to check master status ...\n");
	((CollectorEngine *)x)->timeToCheckMasters = true;

	return 0;
}


int
email (char *subject, char *address)
{
	int 	mailer;
	char	mailtoURL[128];

	if (address == NULL) {
		address = CondorAdministrator;
	}

	sprintf (mailtoURL, "mailto:%s", address);
	if ((mailer = open_url (mailtoURL, O_WRONLY, 0)) < 0)
		dprintf (D_ALWAYS, "Unable to send mail though open_url\n");
	else
		fdprintf (mailer, "Subject:  %s\n\n", subject);

	return mailer;
}

	
#if defined(USE_XDR)
static bool 
backwardCompatibility (int &command, ClassAd *&clientAd, XDR *xdrs)
{
	CONTEXT *context;

	switch (command)
	{
	  // Old: these commands are supplied by old Startds and Schedds
	  case STARTD_INFO:
	  case SCHEDD_INFO:
		if (command == STARTD_INFO)
		{
			dprintf (D_ALWAYS, "Got STARTD_INFO command;"
					 "upgrading STARTD_INFO -> UPDATE_STARTD_AD\n");
			command = UPDATE_STARTD_AD;
		}
		else
		{
			dprintf (D_ALWAYS, "Got SCHEDD_INFO command;"
					 "upgrading SCHEDD_INFO -> UPDATE_SCHEDD_AD\n");
			command = UPDATE_SCHEDD_AD;
		}
		context = create_context ();
		if (!xdr_context (xdrs, context))
		{
			dprintf (D_ALWAYS, "Command %d not followed by CONTEXT\n");
			return false;
		}
		clientAd = new ClassAd (context);
		clientAd->SetMyTypeName ("Machine");
		clientAd->SetTargetTypeName ("Job");
		free_context (context);
		break;

#if 0
	  // these are the new commands
	  case UPDATE_STARTD_AD:
	  case UPDATE_SCHEDD_AD:
	  case UPDATE_MASTER_AD:
	  case UPDATE_GATEWAY_AD:
	  case UPDATE_CKPT_SRVR_AD:
	  case QUERY_STARTD_ADS:
	  case QUERY_SCHEDD_ADS:
	  case QUERY_MASTER_ADS:
	  case QUERY_GATEWAY_ADS:
	  case QUERY_CKPT_SRVR_ADS:
		clientAd = new ClassAd ();
		if (!clientAd->get (xdrs))
		{
			dprintf (D_ALWAYS, "Command %d not followed by ClassAd\n",command);
			return false;
		}
		if (!xdrrec_skiprecord (xdrs))
		{
			dprintf (D_ALWAYS, "Could not skiprecord\n");
			return false;
		}
		break;
#endif

	  default:
		dprintf (D_ALWAYS, "Received illegal command %d: aborting\n", command);
		return false;
	}

	return true;
}
#endif

static void
killHashTable (CollectorHashTable &table)
{
	ClassAd *ad;

	while (table.iterate (ad))
	{
		if (ad > THRESHOLD) delete ad;
	}
}

char *
strStatus (ClassAd *ad)
{
	if (ad == RECENTLY_DOWN) return "recently down";
	if (ad == DONE_REPORTING) return "done reporting";
	if (ad == LONG_GONE) return "long gone";

	return "(unknown master state)";
}
