/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
static void purgeHashTable (CollectorHashTable &);

int 	engine_clientTimeoutHandler (Service *);
int 	engine_housekeepingHandler  (Service *);
char	*strStatus (ClassAd *);

CollectorEngine::CollectorEngine (CollectorStats *stats ) : 
	StartdAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
	StartdPrivateAds(GREATER_TABLE_SIZE, &adNameHashFunction),

#if WANT_QUILL
	QuillAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
#endif /* WANT_QUILL */

	ScheddAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
	SubmittorAds  (GREATER_TABLE_SIZE, &adNameHashFunction),
	LicenseAds    (GREATER_TABLE_SIZE, &adNameHashFunction),
	MasterAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
	StorageAds    (GREATER_TABLE_SIZE, &adNameHashFunction),
	CkptServerAds (LESSER_TABLE_SIZE , &adNameHashFunction),
	GatewayAds    (LESSER_TABLE_SIZE , &adNameHashFunction),
	CollectorAds  (LESSER_TABLE_SIZE , &adNameHashFunction),
	NegotiatorAds (LESSER_TABLE_SIZE , &adNameHashFunction),
	HadAds        (LESSER_TABLE_SIZE , &adNameHashFunction)
{
	clientTimeout = 20;
	machineUpdateInterval = 30;
	masterCheckInterval = 10800;
	housekeeperTimerID = -1;
	masterCheckTimerID = -1;

	collectorStats = stats;
}


CollectorEngine::
~CollectorEngine ()
{
#if WANT_QUILL
	killHashTable (QuillAds);
#endif /* WANT_QUILL */

	killHashTable (StartdAds);
	killHashTable (StartdPrivateAds);
	killHashTable (ScheddAds);
	killHashTable (SubmittorAds);
	killHashTable (LicenseAds);
	killHashTable (MasterAds);
	killHashTable (StorageAds);
	killHashTable (CkptServerAds);
	killHashTable (GatewayAds);
	killHashTable (NegotiatorAds);
	killHashTable (HadAds);
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

#if WANT_QUILL
		case QUILL_AD: 
			cleanHashTable (QuillAds, now, makeQuillAdHashKey);
			break;
#endif /* WANT_QUILL */

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

		case NEGOTIATOR_AD:
			cleanHashTable (NegotiatorAds, now, makeStorageAdHashKey);
			break;

		case HAD_AD:
			cleanHashTable (HadAds, now, makeHadAdHashKey);
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

#if WANT_QUILL
	  case QUILL_AD:
		table = &QuillAds;
		break;
#endif /* WANT_QUILL */

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

	  case NEGOTIATOR_AD:
		table = &NegotiatorAds;
		break;

	  case HAD_AD:
		table = &HadAds;
		break;

	  case ANY_AD:
		return
			StorageAds.walk(scanFunction) &&
			CkptServerAds.walk(scanFunction) &&
			LicenseAds.walk(scanFunction) &&
			StartdAds.walk(scanFunction) &&
			ScheddAds.walk(scanFunction) &&
			MasterAds.walk(scanFunction) &&
			SubmittorAds.walk(scanFunction) &&
			NegotiatorAds.walk(scanFunction) &&
			HadAds.walk(scanFunction);

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

	// Don't leak the ad on error!
	if ( ! rval ) {
		delete clientAd;
	}

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
	ClassAd		*retVal;
	ClassAd		*pvtAd;
	int		insPvt;
	AdNameHashKey		hk;
	HashString	hashString;
	static int repeatStartdAds = -1;		// for debugging
	ClassAd		*clientAdToRepeat = NULL;

	if (repeatStartdAds == -1) {
		repeatStartdAds = param_integer("COLLECTOR_REPEAT_STARTD_ADS",0);
	}
	
	// mux on command
	switch (command)
	{
	  case UPDATE_STARTD_AD:
		if ( repeatStartdAds > 0 ) {
			clientAdToRepeat = new ClassAd(*clientAd);
		}
		if (!makeStartdAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		hashString.Build( hk );
		retVal=updateClassAd (StartdAds, "StartdAd     ", "Start",
							  clientAd, hk, hashString, insert, from );

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
			(void) updateClassAd (StartdPrivateAds, "StartdPvtAd  ",
								  "StartdPvt", pvtAd, hk, hashString, insPvt,
								  from );
		}

		// create fake duplicates of this ad, each with a different name, if 
		// we are told to do so.  this feature exists for developer 
		// scalability testing.
		if ( repeatStartdAds > 0 && clientAdToRepeat ) {
			ClassAd *fakeAd;
			int n;
			char newname[150],oldname[130];
			oldname[0] = '\0';
			clientAdToRepeat->LookupString("Name",oldname,sizeof(oldname));
			for (n=0;n<repeatStartdAds;n++) {
				fakeAd = new ClassAd(*clientAdToRepeat);
				snprintf(newname,sizeof(newname),
						 "Name=\"fake%d-%s\"",n,oldname);
				fakeAd->InsertOrUpdate(newname);
				makeStartdAdHashKey (hk, fakeAd, from);
				hashString.Build( hk );
				if (! updateClassAd (StartdAds, "StartdAd     ", "Start",
							  fakeAd, hk, hashString, insert, from ) )
				{
					// don't leak memory if there is some failure
					delete fakeAd;
				}
			}
			delete clientAdToRepeat;
			clientAdToRepeat = NULL;
		}
		break;

#if WANT_QUILL
	  case UPDATE_QUILL_AD:
		if (!makeQuillAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		hashString.Build( hk );
		retVal=updateClassAd (QuillAds, "QuillAd     ", "Quill",
							  clientAd, hk, hashString, insert, from );
		break;
#endif /* WANT_QUILL */

	  case UPDATE_SCHEDD_AD:
		if (!makeScheddAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		hashString.Build( hk );
		retVal=updateClassAd (ScheddAds, "ScheddAd     ", "Schedd",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_SUBMITTOR_AD:
		// use the same hashkey function as a schedd ad
		if (!makeScheddAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		// since submittor ads always follow a schedd ad, and a master check is
		// performed for schedd ads, we don't need a master check in here
		hashString.Build( hk );
		retVal=updateClassAd (SubmittorAds, "SubmittorAd  ", "Submittor",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_LICENSE_AD:
		// use the same hashkey function as a schedd ad
		if (!makeLicenseAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		// since submittor ads always follow a schedd ad, and a master check is
		// performed for schedd ads, we don't need a master check in here
		hashString.Build( hk );
		retVal=updateClassAd (LicenseAds, "LicenseAd  ", "License",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_MASTER_AD:
		if (!makeMasterAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (MasterAds, "MasterAd     ", "Master",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_CKPT_SRVR_AD:
		if (!makeCkptSrvrAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		checkMasterStatus (clientAd);
		hashString.Build( hk );
		retVal=updateClassAd (CkptServerAds, "CkptSrvrAd   ", "CkptSrvr",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_COLLECTOR_AD:
		if (!makeCollectorAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (CollectorAds, "CollectorAd  ", "Collector",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_STORAGE_AD:
		if (!makeStorageAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (StorageAds, "StorageAd  ", "Storage",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_NEGOTIATOR_AD:
		if (!makeNegotiatorAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
			// first, purge all the existing negotiator ads, since we
			// want to enforce that *ONLY* 1 negotiator is in the
			// collector any given time.
		purgeHashTable( NegotiatorAds );
		retVal=updateClassAd (NegotiatorAds, "NegotiatorAd  ", "Negotiator",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_HAD_AD:
		  if (!makeHadAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (HadAds, "HadAd  ", "HAD",
							  clientAd, hk, hashString, insert, from );
		break;

	  case QUERY_STARTD_ADS:
	  case QUERY_SCHEDD_ADS:
	  case QUERY_MASTER_ADS:
	  case QUERY_GATEWAY_ADS:
	  case QUERY_SUBMITTOR_ADS:
	  case QUERY_CKPT_SRVR_ADS:
	  case QUERY_STARTD_PVT_ADS:
	  case QUERY_COLLECTOR_ADS:
  	  case QUERY_NEGOTIATOR_ADS:
  	  case QUERY_HAD_ADS:
	  case INVALIDATE_STARTD_ADS:
	  case INVALIDATE_SCHEDD_ADS:
	  case INVALIDATE_MASTER_ADS:
	  case INVALIDATE_GATEWAY_ADS:
	  case INVALIDATE_CKPT_SRVR_ADS:
	  case INVALIDATE_SUBMITTOR_ADS:
	  case INVALIDATE_COLLECTOR_ADS:
	  case INVALIDATE_NEGOTIATOR_ADS:
	  case INVALIDATE_HAD_ADS:
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
lookup (AdTypes adType, AdNameHashKey &hk)
{
	ClassAd *val;

	switch (adType)
	{
		case STARTD_AD:
			if (StartdAds.lookup (hk, val) == -1)
				return 0;
			break;

#if WANT_QUILL
		case QUILL_AD:
			if (QuillAds.lookup (hk, val) == -1)
				return 0;
			break;
#endif /* WANT_QUILL */

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

		case NEGOTIATOR_AD:
			if (NegotiatorAds.lookup (hk, val) == -1)
				return 0;
			break;

		case HAD_AD:
			if (HadAds.lookup (hk, val) == -1)
				return 0;
			break;

		default:
			val = 0;
	}

	return val;
}


int CollectorEngine::
remove (AdTypes adType, AdNameHashKey &hk)
{
	switch (adType)
	{
		case STARTD_AD:
			return !StartdAds.remove (hk);

#if WANT_QUILL
		case QUILL_AD:
			return !QuillAds.remove (hk);
#endif /* WANT_QUILL */

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

		case NEGOTIATOR_AD:
			return !NegotiatorAds.remove (hk);

		case HAD_AD:
			return !HadAds.remove (hk);

		default:
			return 0;
	}
}
	
				
ClassAd * CollectorEngine::
updateClassAd (CollectorHashTable &hashTable, 
			   const char *adType,
			   const char *label,
			   ClassAd *ad, 
			   AdNameHashKey &hk,
			   const MyString &hashString,
			   int  &insert,
			   const sockaddr_in *from )
{
	ClassAd		*old_ad, *new_ad;
	MyString	buf;
	time_t		now;

	(void) time (&now);
	if (now == (time_t) -1)
	{
		EXCEPT ("Error reading system time!");
	}	
	buf.sprintf( "%s = %d", ATTR_LAST_HEARD_FROM, (int)now);
	ad->Insert ( buf.GetCStr() );

	// this time stamped ad is the new ad
	new_ad = ad;

	// check if it already exists in the hash table ...
	if ( hashTable.lookup (hk, old_ad) == -1)
    {	 	
		// no ... new ad
		dprintf (D_ALWAYS, "%s: Inserting ** \"%s\"\n", adType, hashString.GetCStr() );

		// Update statistics
		collectorStats->update( label, NULL, new_ad );

		// Now, store it away
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
		dprintf (D_FULLDEBUG, "%s: Updating ... \"%s\"\n", adType, hashString.GetCStr() );

		// check if it has special status (master ads)
		if (old_ad < CollectorEngine::THRESHOLD)
		{
			dprintf (D_ALWAYS, "** Master %s rejuvenated from %s\n",
					 hashString.GetCStr(), strStatus(old_ad));
			if (hashTable.remove (hk)==-1 || hashTable.insert (hk, new_ad)==-1)
			{
				EXCEPT ("Error updating ad (probably out of memory)");
			}

			// Update statistics
			collectorStats->update( label, NULL, new_ad );
		}
		else
		{
			// Update statistics
			collectorStats->update( label, old_ad, new_ad );

			// Now, finally, store the new ClassAd
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
	AdNameHashKey 	hk;
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
		dprintf(D_ALWAYS,"WARNING:  No master ad for < %s >\n", hk.name.GetCStr() );
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

#if WANT_QUILL
	dprintf (D_ALWAYS, "\tCleaning QuillAds ...\n");
	cleanHashTable (QuillAds, now, makeQuillAdHashKey);
#endif /* WANT_QUILL */

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

	dprintf (D_ALWAYS, "\tCleaning NegotiatorAds ...\n");
	cleanHashTable (NegotiatorAds, now, makeNegotiatorAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning HadAds ...\n");
	cleanHashTable (HadAds, now, makeHadAdHashKey);

	// add other ad types here ...


	// cron manager
	event_mgr();

	dprintf (D_ALWAYS, "Housekeeper:  Done cleaning\n");
	return TRUE;
}

void CollectorEngine::
cleanHashTable (CollectorHashTable &hashTable, time_t now, 
				bool (*makeKey) (AdNameHashKey &, ClassAd *, sockaddr_in *) )
{
	ClassAd  *ad;
	int   	 timeStamp;
	int		 updateInterval;
	AdNameHashKey  hk;
	double   timeDiff;
	MyString	hkString;

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
			hk.sprint( hkString );
			dprintf (D_ALWAYS,"\t\t**** Removing stale ad: \"%s\"\n", hkString.GetCStr() );
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


static void
purgeHashTable( CollectorHashTable &table )
{
	ClassAd* ad;
	AdNameHashKey hk;
	table.startIterations();
	while( table.iterate(hk,ad) ) {
		if( table.remove(hk) == -1 ) {
			dprintf( D_ALWAYS, "\t\tError while removing ad\n" );
		}		
		if( ad > CollectorEngine::THRESHOLD ) {
			delete ad;
		}
	}
}


int CollectorEngine::
masterCheck ()
{
	ClassAd		*ad;
	ClassAd		*nextStatus;
	AdNameHashKey		hk;
	MyString	hkString;
	MyString	buffer;
	FILE*		mailer = NULL;
	int		more;

	char *tmp = param("COLLECTOR_PERFORM_MASTERCHECK");
	bool do_check = true;	// default should be to do the check
	if ( tmp ) {
		if ( tmp[0] == 'F' || tmp[0] == 'f' ) {
			do_check = false;
		}
		free(tmp);
	}
	if ( do_check == false ) {
		// User explicitly asked for no checking... all done.
		return TRUE;
	}

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
			dprintf (D_ALWAYS,"\tMaster %s: purging dead entry\n", hkString.GetCStr() );
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
				buffer.sprintf( "Collector (%s): Dead condor_masters", 
								my_full_hostname() );
				if ((mailer = email_admin_open(buffer.GetCStr())) == NULL) {
					dprintf (D_ALWAYS, "Error sending email --- aborting\n");
					return FALSE;
				}
				fprintf (mailer, "The following masters are dead, leaving"
							" orphaned daemons\n\n");
			}
			fprintf (mailer, "\t\t%s\n", hkString.GetCStr() );
			dprintf (D_ALWAYS, "\tMaster %s: recently went down\n", hkString.GetCStr() );
			nextStatus = DONE_REPORTING;
		}	
		else 
		// the master is dead, but we've already sent mail about it
		if (ad == DONE_REPORTING) {
			dprintf(D_ALWAYS,"\tMaster %s: death already reported\n",hkString.GetCStr() );
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
