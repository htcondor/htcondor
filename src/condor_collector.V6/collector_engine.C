/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"

extern "C" void event_mgr (void);

//-------------------------------------------------------------

#include "condor_classad.h"
#include "condor_parser.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_email.h"

#include "condor_attributes.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

//-------------------------------------------------------------

#include "collector_engine.h"


// pointer values for representing master states
ClassAd* CollectorEngine::RECENTLY_DOWN  = (ClassAd *) 0x1;
ClassAd* CollectorEngine::DONE_REPORTING = (ClassAd *) 0x2;
ClassAd* CollectorEngine::LONG_GONE	  = (ClassAd *) 0x3;
ClassAd* CollectorEngine::THRESHOLD	  = (ClassAd *) 0x4;

static void killHashTable (CollectorHashTable &);

int 	engine_clientTimeoutHandler (Service *);
int 	engine_housekeepingHandler  (Service *);
char	*strStatus (ClassAd *);

CollectorEngine::
CollectorEngine () : 
	StartdAds     (GREATER_TABLE_SIZE, &hashFunction),
	StartdPrivateAds(GREATER_TABLE_SIZE, &hashFunction),
	ScheddAds     (GREATER_TABLE_SIZE, &hashFunction),
	SubmittorAds  (GREATER_TABLE_SIZE, &hashFunction),
	LicenseAds    (GREATER_TABLE_SIZE, &hashFunction),
	MasterAds     (GREATER_TABLE_SIZE, &hashFunction),
	StorageAds       (GREATER_TABLE_SIZE, &hashFunction),
	CkptServerAds (LESSER_TABLE_SIZE , &hashFunction),
	GatewayAds    (LESSER_TABLE_SIZE , &hashFunction),
	CollectorAds  (LESSER_TABLE_SIZE , &hashFunction)
{
	clientTimeout = 20;
	machineUpdateInterval = 30;
	masterCheckInterval = 10800;
	housekeeperTimerID = -1;
	masterCheckTimerID = -1;
}


CollectorEngine::
~CollectorEngine ()
{
	killHashTable (StartdAds);
	killHashTable (StartdPrivateAds);
	killHashTable (ScheddAds);
	killHashTable (SubmittorAds);
	killHashTable (LicenseAds);
	killHashTable (MasterAds);
	killHashTable (StorageAds);
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
		(void) daemonCore->Cancel_Timer(housekeeperTimerID);
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
		housekeeperTimerID = daemonCore->Register_Timer(machineUpdateInterval,
						machineUpdateInterval,
						(TimerHandlercpp)&CollectorEngine::housekeeper,
						"CollectorEngine::housekeeper",this);
		if (housekeeperTimerID == -1)
			return 0;
	}

	return 1;
}


int CollectorEngine::
invokeHousekeeper (AdTypes adtype)
{
	time_t now;
   
	(void) time (&now);
	if (now == (time_t) -1)
	{
		dprintf (D_ALWAYS, "Error in reading system time --- aborting\n");
		return FALSE;
	}

	switch (adtype)
	{
		case STARTD_AD: 
			cleanHashTable (StartdAds, now, makeStartdAdHashKey);
			break;

		case SCHEDD_AD:
			cleanHashTable (ScheddAds, now, makeScheddAdHashKey);
			break;

		case MASTER_AD:
			cleanHashTable (MasterAds, now, makeMasterAdHashKey);
			break;

		case STARTD_PVT_AD:
			cleanHashTable (StartdPrivateAds, now, makeStartdAdHashKey);
			break;

		case SUBMITTOR_AD:
			cleanHashTable (SubmittorAds, now, makeScheddAdHashKey);
			break;

		case LICENSE_AD:
			cleanHashTable (LicenseAds, now, makeLicenseAdHashKey);
			break;

		case STORAGE_AD:
			cleanHashTable (StorageAds, now, makeStorageAdHashKey);
			break;

		default:
			return 0;
	}

	return 1;
}


int CollectorEngine::
scheduleDownMasterCheck (int timeout)
{
	// cancel outstanding check requests
	if (masterCheckTimerID != -1) {
		(void) daemonCore->Cancel_Timer(masterCheckTimerID);
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
		masterCheckTimerID = daemonCore->Register_Timer(masterCheckInterval,
						masterCheckInterval,
						(TimerHandlercpp)&CollectorEngine::masterCheck,
						"CollectorEngine::masterCheck",this);
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

	  case SUBMITTOR_AD:
		table = &SubmittorAds;
		break;

	  case LICENSE_AD:
		table = &LicenseAds;
		break;

	  case COLLECTOR_AD:
		table = &CollectorAds;
		break;

	  case STORAGE_AD:
		table = &StorageAds;
		break;

	  case ANY_AD:
		return
			StorageAds.walk(scanFunction) &&
			CkptServerAds.walk(scanFunction) &&
			LicenseAds.walk(scanFunction) &&
			StartdAds.walk(scanFunction) &&
			ScheddAds.walk(scanFunction) &&
			MasterAds.walk(scanFunction) &&
			SubmittorAds.walk(scanFunction);
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

/*
static bool printAdToFile(ClassAd& ad, char* JobHistoryFileName) {
  FILE* LogFile=fopen(JobHistoryFileName,"a");
  if ( !LogFile ) {
    dprintf(D_ALWAYS,"ERROR saving to history file; cannot open %s\n",JobHistoryFileName);
    return false;
  }
  if (!ad.fPrint(LogFile)) {
    dprintf(D_ALWAYS, "ERROR in Scheduler::LogMatchEnd - failed to write clas ad to log file %s\n",JobHistoryFileName);
    fclose(LogFile);
    return false;
  }
  fprintf(LogFile,"***\n");   // separator
  fclose(LogFile);
  return true;
}
*/

ClassAd *CollectorEngine::
collect (int command, Sock *sock, sockaddr_in *from, int &insert)
{
	ClassAd	*clientAd;
	ClassAd	*rval;

	// use a timeout
	sock->timeout(clientTimeout);

	clientAd = new ClassAd;
	if (!clientAd) return 0;

	// get the ad
	if( !clientAd->initFromStream(*sock) )
	{
		dprintf (D_ALWAYS,"Command %d on Sock not follwed by ClassAd (or timeout occured)\n",
				command);
		delete clientAd;
		sock->end_of_message();
		return 0;
	}
	
	// the above includes a timed communication with the client
	sock->timeout(0);

	rval = collect(command, clientAd, from, insert, sock);

	// get the end_of_message()
	if (!sock->end_of_message())
	{
		dprintf(D_FULLDEBUG,"Warning: Command %d; maybe shedding data on eom\n",
				 command);
	}
	
	return rval;
}

ClassAd *CollectorEngine::
collect (int command,ClassAd *clientAd,sockaddr_in *from,int &insert,Sock *sock)
{
	ClassAd	*retVal;
	ClassAd	*pvtAd;
	int		insPvt;
	HashKey hk;
	char    hashString [64];
	
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
		retVal=updateClassAd (StartdAds, "StartdAd     ", clientAd, hk, 
							  hashString, insert);

		// if we want to store private ads
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
			if( !pvtAd->initFromStream(*sock) )
			{
				dprintf(D_FULLDEBUG,"\t(Could not get startd's private ad)\n");
				delete pvtAd;
				break;
			}
			
			// insert the private ad into its hashtable --- use the same
			// hash key as the public ad
			(void) updateClassAd (StartdPrivateAds, "StartdPvtAd  ", pvtAd,
									hk, hashString, insPvt);
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
		retVal=updateClassAd (ScheddAds, "ScheddAd     ", clientAd, hk,
							  hashString, insert);
		break;

	  case UPDATE_SUBMITTOR_AD:
		// use the same hashkey function as a schedd ad
		if (!makeScheddAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		// since submittor ads always follow a schedd ad, and a master check is
		// performed for schedd ads, we don't need a master check in here
		sprintf (hashString, "< %s , %s >", hk.name, hk.ip_addr);
		retVal=updateClassAd (SubmittorAds, "SubmittorAd  ", clientAd, hk,
							  hashString, insert);
		break;

	  case UPDATE_LICENSE_AD:
		// use the same hashkey function as a schedd ad
		if (!makeLicenseAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		// since submittor ads always follow a schedd ad, and a master check is
		// performed for schedd ads, we don't need a master check in here
		sprintf (hashString, "< %s , %s >", hk.name, hk.ip_addr);
		retVal=updateClassAd (LicenseAds, "LicenseAd  ", clientAd, hk,
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
		retVal=updateClassAd (MasterAds, "MasterAd     ", clientAd, hk, 
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
		retVal=updateClassAd (CkptServerAds, "CkptSrvrAd   ", clientAd, hk, 
							  hashString, insert);
		break;

	  case UPDATE_COLLECTOR_AD:
		if (!makeCollectorAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		sprintf (hashString, "< %s >", hk.name);
		retVal=updateClassAd (CollectorAds, "CollectorAd  ", clientAd, hk, 
							  hashString, insert);
		break;

	  case UPDATE_STORAGE_AD:
		if (!makeStorageAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			retVal = 0;
			break;
		}
		sprintf (hashString, "< %s >", hk.name);
		retVal=updateClassAd (StorageAds, "StorageAd  ", clientAd, hk, 
							  hashString, insert);
		break;

	  case QUERY_STARTD_ADS:
	  case QUERY_SCHEDD_ADS:
	  case QUERY_MASTER_ADS:
	  case QUERY_GATEWAY_ADS:
	  case QUERY_SUBMITTOR_ADS:
	  case QUERY_CKPT_SRVR_ADS:
	  case QUERY_STARTD_PVT_ADS:
	  case QUERY_COLLECTOR_ADS:
	  case INVALIDATE_STARTD_ADS:
	  case INVALIDATE_SCHEDD_ADS:
	  case INVALIDATE_MASTER_ADS:
	  case INVALIDATE_GATEWAY_ADS:
	  case INVALIDATE_CKPT_SRVR_ADS:
	  case INVALIDATE_SUBMITTOR_ADS:
	  case INVALIDATE_COLLECTOR_ADS:
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

		case SUBMITTOR_AD:
			if (SubmittorAds.lookup (hk, val) == -1)
				return 0;
			break;

		case LICENSE_AD:
			if (LicenseAds.lookup (hk, val) == -1)
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

		case COLLECTOR_AD:
			if (CollectorAds.lookup (hk, val) == -1)
				return 0;
			break;

		case STARTD_PVT_AD:
			if (StartdPrivateAds.lookup (hk, val) == -1)
				return 0;
			break;

		case STORAGE_AD:
			if (StorageAds.lookup (hk, val) == -1)
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

		case SUBMITTOR_AD:
			return !SubmittorAds.remove (hk);

		case LICENSE_AD:
			return !LicenseAds.remove (hk);

		case MASTER_AD:
			return !MasterAds.remove (hk);

		case CKPT_SRVR_AD:
			return !CkptServerAds.remove (hk);

		case STARTD_PVT_AD:
			return !StartdPrivateAds.remove (hk);

		case COLLECTOR_AD:
			return !CollectorAds.remove (hk);

		case STORAGE_AD:
			return !StorageAds.remove (hk);

		default:
			return 0;
	}
}
	
				
ClassAd * CollectorEngine::
updateClassAd (CollectorHashTable &hashTable, 
			   char *adType, 
			   ClassAd *ad, 
			   HashKey &hk,
			   char *hashString,
			   int  &insert )
{
	ClassAd  *old_ad, *new_ad;
	char     buf [40];
	time_t   now;

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
		dprintf (D_FULLDEBUG, "%s: Updating ... %s\n", adType, hashString);

		// check if it has special status (master ads)
		if (old_ad < CollectorEngine::THRESHOLD)
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

int 
CollectorEngine::
housekeeper()
{
	time_t now;
   
	(void) time (&now);
	if (now == (time_t) -1)
	{
		dprintf (D_ALWAYS, 
				 "Housekeeper:  Error in reading system time --- aborting\n");
		return FALSE;
	}

	dprintf (D_ALWAYS, "Housekeeper:  Ready to clean old ads\n");

	dprintf (D_ALWAYS, "\tCleaning StartdAds ...\n");
	cleanHashTable (StartdAds, now, makeStartdAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning StartdPrivateAds ...\n");
	cleanHashTable (StartdPrivateAds, now, makeStartdAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning ScheddAds ...\n");
	cleanHashTable (ScheddAds, now, makeScheddAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning SubmittorAds ...\n");
	cleanHashTable (SubmittorAds, now, makeScheddAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning LicenseAds ...\n");
	cleanHashTable (LicenseAds, now, makeLicenseAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning MasterAds ...\n");
	cleanHashTable (MasterAds, now, makeMasterAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning CkptServerAds ...\n");
	cleanHashTable (CkptServerAds, now, makeCkptSrvrAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning CollectorAds ...\n");
	cleanHashTable (CollectorAds, now, makeCollectorAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning StorageAds ...\n");
	cleanHashTable (StorageAds, now, makeStorageAdHashKey);

	// add other ad types here ...


	// cron manager
	event_mgr();

	dprintf (D_ALWAYS, "Housekeeper:  Done cleaning\n");
	return TRUE;
}

void CollectorEngine::
cleanHashTable (CollectorHashTable &hashTable, time_t now, 
				bool (*makeKey) (HashKey &, ClassAd *, sockaddr_in *) )
{
	ClassAd  *ad;
	int   	 timeStamp;
	int		 updateInterval;
	HashKey  hk;
	double   timeDiff;
	char     hkString [128];

	hashTable.startIterations ();
	while (hashTable.iterate (ad))
	{
		// ignore ads less than threshold
		if (ad < THRESHOLD) continue;

		// Read the timestamp of the ad
		if (!ad->LookupInteger (ATTR_LAST_HEARD_FROM, timeStamp)) {
			dprintf (D_ALWAYS, "\t\tError looking up time stamp on ad\n");
			continue;
		}

		// how long has it been since the last update?
		timeDiff = difftime( now, timeStamp );

		if( !ad->LookupInteger( ATTR_UPDATE_INTERVAL, updateInterval ) ) {
			updateInterval = machineUpdateInterval;
		}

		// check if it has expired
		if (timeDiff > (double) updateInterval )
		{
			// then remove it from the segregated table
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

int CollectorEngine::
masterCheck ()
{
	ClassAd  *ad;
	ClassAd	 *nextStatus;
	HashKey  hk;
	char     hkString [128];
	char	 buffer [128];
	FILE* 	 mailer = NULL;
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
			if (mailer == NULL) {
				sprintf( buffer, "Collector (%s): Dead condor_masters", 
						 my_full_hostname() );
				if ((mailer = email_admin_open(buffer)) == NULL) {
					dprintf (D_ALWAYS, "Error sending email --- aborting\n");
					return FALSE;
				}
				fprintf (mailer, "The following masters are dead, leaving"
							" orphaned daemons\n\n");
			}
			fprintf (mailer, "\t\t%s\n", hkString);
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
	if (mailer) email_close(mailer);

	dprintf (D_ALWAYS, "MasterCheck:  Done checking for down masters\n");
	return TRUE;
}

static void
killHashTable (CollectorHashTable &table)
{
	ClassAd *ad;

	while (table.iterate (ad))
	{
		if (ad > CollectorEngine::THRESHOLD) delete ad;
	}
}

char *
strStatus (ClassAd *ad)
{
	if (ad == CollectorEngine::RECENTLY_DOWN) return "recently down";
	if (ad == CollectorEngine::DONE_REPORTING) return "done reporting";
	if (ad == CollectorEngine::LONG_GONE) return "long gone";

	return "(unknown master state)";
}
