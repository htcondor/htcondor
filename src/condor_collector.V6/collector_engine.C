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
#include "proc_obj.h"

#include "collector_engine.h"

static char *_FileName_ = __FILE__;

extern "C" XDR *xdr_Udp_Init ();
extern "C" int xdr_context (XDR *, CONTEXT *);

static bool backwardCompatibility (int &, ClassAd *&, XDR *);
static void killHashTable (CollectorHashTable &);
static ClassAd* updateClassAd(CollectorHashTable&,char*,ClassAd*,HashKey&,
							  char*, int &);
int engine_clientTimeoutHandler (Service *);
int engine_housekeepingHandler  (Service *);

CollectorEngine::
CollectorEngine (TimerManager *timerManager) : 
    StartdAds     (GREATER_TABLE_SIZE, &hashFunction),
	ScheddAds     (GREATER_TABLE_SIZE, &hashFunction),
	MasterAds     (GREATER_TABLE_SIZE, &hashFunction),
	CkptServerAds (LESSER_TABLE_SIZE , &hashFunction),
	GatewayAds    (LESSER_TABLE_SIZE , &hashFunction)
{
	timeToClean = false;
	clientTimeout = 20;
	machineUpdateInterval = 30;
	housekeeperTimerID = -1;
	clientSocket = -1;
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


void CollectorEngine::
toggleLogging (void)
{
	log = !log;
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

	// start timer
	timerID = timer->NewTimer(this,clientTimeout,(void *)engine_clientTimeoutHandler);
	if (timerID == -1)
	{
		EXCEPT ("Could not allocate timer");
	}

	// don't need backward compatibility here --- new daemons will use I/O
	clientAd = new ClassAd;
	if (!clientAd) return 0;

	// get the ad
	if (!clientAd->get(*sock))
	{
		dprintf (D_ALWAYS,"Command %d on Sock not follwed by ClassAd\n",
				command);
		delete clientAd;
		return 0;
	}
	
	// get the eom()
	if (!sock->eom())
	{
		dprintf (D_ALWAYS,"Warning: On command %d, ad not followed by eom()\n",
				 command);
	}
	
	// the above includes a timed communication with the client
	(void) timer->CancelTimer (timerID);

	return (collect(command, clientAd, from, insert));
}

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

ClassAd *CollectorEngine::
collect (int command, ClassAd *clientAd, sockaddr_in *from, int &insert)
{
	ClassAd  *retVal;
	HashKey  hk;
	char     hashString [64];
	
	// safer to housekeep now if necessary
 	if (timeToClean)
	{
		timeToClean = false;
		housekeeper ();
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
		sprintf (hashString, "< %s , %s , %d >", hk.name, hk.ip_addr, hk.port);
		retVal=updateClassAd (StartdAds, "StartdAd  ", clientAd, hk, 
							  hashString, insert);
		break;

	  case UPDATE_SCHEDD_AD:
		if (!makeScheddAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		sprintf (hashString, "< %s , %s , %d >", hk.name, hk.ip_addr, hk.port);
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
		sprintf (hashString, "< %s >", hk.name);
		retVal=updateClassAd (CkptServerAds, "CkptSrvrAd", clientAd, hk, 
							  hashString, insert);
		break;

	  case QUERY_STARTD_ADS:
	  case QUERY_SCHEDD_ADS:
	  case QUERY_MASTER_ADS:
	  case QUERY_CKPT_SRVR_ADS:
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
	if (tree = ad->Lookup ("LastHeardFrom"))
	{
		delete tree;
	}	
	(void) time (&now);
	if (now == (time_t) -1)
	{
		EXCEPT ("Error reading system time!");
	}	
	sprintf (buf, "LastHeardFrom = %d", now);
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
		old_ad->ExchangeExpressions (new_ad);
		delete new_ad;

		insert = 0;
		return old_ad;
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
	time_t   timeStamp;
	HashKey  hk;
	double   timeDiff;
	char     hkString [128];

	hashTable.startIterations ();
	while (hashTable.iterate (ad))
	{
		// Read the timestamp of the ad
		if ((tree = ad->Lookup ("LastHeardFrom")) == NULL)
		{
			dprintf (D_ALWAYS, "\t\tError looking up time stamp on ad\n");
			continue;
		}
		timeStamp = (time_t) (((Integer *) tree->RArg())->Value());

		// check if it has expired
		timeDiff = difftime (now, timeStamp);
		if (timeDiff > (double) machineUpdateInterval)
		{
			(*makeKey) (hk, ad, NULL);
			hk.sprint (hkString);
			dprintf (D_ALWAYS,"\t\tDEAD: Removing expired ad: %s\n", hkString);
			if (hashTable.remove (hk) == -1)
			{
				dprintf (D_ALWAYS, "\t\tError while removing ad\n");
			}
		}
	}
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


	  default:
		dprintf (D_ALWAYS, "Received illegal command %d: aborting\n", command);
		return false;
	}

	return true;
}

static void
killHashTable (CollectorHashTable &table)
{
	ClassAd *ad;

	while (table.iterate (ad))
	{
		delete ad;
	}
}
