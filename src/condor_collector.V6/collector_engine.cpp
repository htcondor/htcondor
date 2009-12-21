/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
#include "condor_daemon_core.h"
#include "file_sql.h"
#include "classad_merge.h"

extern FILESQL *FILEObj;

//-------------------------------------------------------------

#include "collector_engine.h"

static void killHashTable (CollectorHashTable &);
static int killGenericHashTable(CollectorHashTable *);
static void purgeHashTable (CollectorHashTable &);

int 	engine_clientTimeoutHandler (Service *);
int 	engine_housekeepingHandler  (Service *);

CollectorEngine::CollectorEngine (CollectorStats *stats ) :
	StartdAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
	StartdPrivateAds(GREATER_TABLE_SIZE, &adNameHashFunction),

#ifdef HAVE_EXT_POSTGRESQL
	QuillAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
#endif /* HAVE_EXT_POSTGRESQL */

	ScheddAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
	SubmittorAds  (GREATER_TABLE_SIZE, &adNameHashFunction),
	LicenseAds    (GREATER_TABLE_SIZE, &adNameHashFunction),
	MasterAds     (GREATER_TABLE_SIZE, &adNameHashFunction),
	StorageAds    (GREATER_TABLE_SIZE, &adNameHashFunction),
	XferServiceAds(GREATER_TABLE_SIZE, &adNameHashFunction),
	CkptServerAds (LESSER_TABLE_SIZE , &adNameHashFunction),
	GatewayAds    (LESSER_TABLE_SIZE , &adNameHashFunction),
	CollectorAds  (LESSER_TABLE_SIZE , &adNameHashFunction),
	NegotiatorAds (LESSER_TABLE_SIZE , &adNameHashFunction),
	HadAds        (LESSER_TABLE_SIZE , &adNameHashFunction),
	LeaseManagerAds(LESSER_TABLE_SIZE , &adNameHashFunction),
	GridAds       (LESSER_TABLE_SIZE , &adNameHashFunction),
	GenericAds    (LESSER_TABLE_SIZE , &stringHashFunction)
{
	clientTimeout = 20;
	machineUpdateInterval = 30;
	housekeeperTimerID = -1;

	collectorStats = stats;
	m_collector_requirements = NULL;
}


CollectorEngine::
~CollectorEngine ()
{
#ifdef HAVE_EXT_POSTGRESQL
	killHashTable (QuillAds);
#endif /* HAVE_EXT_POSTGRESQL */

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
	killHashTable (XferServiceAds);
	killHashTable (LeaseManagerAds);
	killHashTable (GridAds);
	GenericAds.walk(killGenericHashTable);

	if(m_collector_requirements) {
		delete m_collector_requirements;
		m_collector_requirements = NULL;
	}
}


int CollectorEngine::
setClientTimeout (int timeout)
{
	if (timeout <= 0)
		return 0;
	clientTimeout = timeout;
	return 1;
}

bool
CollectorEngine::setCollectorRequirements( char const *str, MyString &error_desc )
{
	if( m_collector_requirements ) {
		delete m_collector_requirements;
		m_collector_requirements = NULL;
	}

	if( !str ) {
		return true;
	}

	m_collector_requirements = new ClassAd();
	if( !m_collector_requirements->AssignExpr(COLLECTOR_REQUIREMENTS, str) ) {
		error_desc += "failed to parse expression";
		if( m_collector_requirements ) {
			delete m_collector_requirements;
			m_collector_requirements = NULL;
		}
		return false;
	}
	return true;
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

#ifdef HAVE_EXT_POSTGRESQL
		case QUILL_AD:
			cleanHashTable (QuillAds, now, makeQuillAdHashKey);
			break;
#endif /* HAVE_EXT_POSTGRESQL */

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
            
		case GRID_AD:
			cleanHashTable (GridAds, now, makeGridAdHashKey);
			break;

		case GENERIC_AD:
			CollectorHashTable *cht;
			GenericAds.startIterations();
			while (GenericAds.iterate(cht)) {
				cleanHashTable (*cht, now, makeGenericAdHashKey);
			}
			break;

		case XFER_SERVICE_AD:
			cleanHashTable (XferServiceAds, now, makeXferServiceAdHashKey);
			break;

		case LEASE_MANAGER_AD:
			cleanHashTable (LeaseManagerAds, now, makeLeaseManagerAdHashKey);
			break;

		default:
			return 0;
	}

	return 1;
}


void CollectorEngine::
toggleLogging (void)
{
	log = !log;
}


int (*CollectorEngine::genericTableScanFunction)(ClassAd *) = NULL;

int CollectorEngine::
genericTableWalker(CollectorHashTable *cht)
{
	ASSERT(genericTableScanFunction != NULL);
	return cht->walk(genericTableScanFunction);
}

int CollectorEngine::
walkGenericTables(int (*scanFunction)(ClassAd *))
{
	int ret;
	genericTableScanFunction = scanFunction;
	ret = GenericAds.walk(genericTableWalker);
	genericTableScanFunction = NULL;
	return ret;
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

#ifdef HAVE_EXT_POSTGRESQL
	  case QUILL_AD:
		table = &QuillAds;
		break;
#endif /* HAVE_EXT_POSTGRESQL */

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

      case GRID_AD:
        table = &GridAds;
		break;

	  case XFER_SERVICE_AD:
		table = &XferServiceAds;
		break;

	  case LEASE_MANAGER_AD:
		table = &LeaseManagerAds;
		break;

	  case GENERIC_AD:
		return walkGenericTables(scanFunction);

	  case ANY_AD:
		return
			StorageAds.walk(scanFunction) &&
			CkptServerAds.walk(scanFunction) &&
			LicenseAds.walk(scanFunction) &&
			CollectorAds.walk(scanFunction) &&
			StartdAds.walk(scanFunction) &&
			ScheddAds.walk(scanFunction) &&
			MasterAds.walk(scanFunction) &&
			SubmittorAds.walk(scanFunction) &&
			NegotiatorAds.walk(scanFunction) &&
#ifdef HAVE_EXT_POSTGRESQL
			QuillAds.walk(scanFunction) &&
#endif
			HadAds.walk(scanFunction) &&
			GridAds.walk(scanFunction) &&
			XferServiceAds.walk(scanFunction) &&
			LeaseManagerAds.walk(scanFunction) &&

			walkGenericTables(scanFunction);
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

CollectorHashTable *CollectorEngine::findOrCreateTable(MyString &type)
{
	CollectorHashTable *table;
	if (GenericAds.lookup(type, table) == -1) {
		dprintf(D_ALWAYS, "creating new table for type %s\n", type.Value());
		table = new CollectorHashTable(LESSER_TABLE_SIZE , &adNameHashFunction);
		if (GenericAds.insert(type, table) == -1) {
			dprintf(D_ALWAYS,  "error adding new generic hash table\n");
			delete table;
			return NULL;
		}
	}

	return table;
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

	// insert the authenticated user into the ad itself
	const char* authn_user = sock->getFullyQualifiedUser();
	if (authn_user) {
		clientAd->Assign("AuthenticatedIdentity", authn_user);
	} else {
		// remove it from the ad if it's not authenticated.
		clientAd->Delete("AuthenticatedIdentity");
	}

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

bool CollectorEngine::ValidateClassAd(int command,ClassAd *clientAd,Sock *sock)
{

	if( !m_collector_requirements ) {
			// no need to do any of the following checks if the admin has
			// not configured any COLLECTOR_REQUIREMENTS
		return true;
	}


	char const *ipattr = NULL;
	switch( command ) {
	  case MERGE_STARTD_AD:
	  case UPDATE_STARTD_AD:
	  case UPDATE_STARTD_AD_WITH_ACK:
		  ipattr = ATTR_STARTD_IP_ADDR;
		  break;
	  case UPDATE_SCHEDD_AD:
	  case UPDATE_SUBMITTOR_AD:
		  ipattr = ATTR_SCHEDD_IP_ADDR;
		  break;
	  case UPDATE_MASTER_AD:
		  ipattr = ATTR_MASTER_IP_ADDR;
		  break;
	  case UPDATE_NEGOTIATOR_AD:
		  ipattr = ATTR_NEGOTIATOR_IP_ADDR;
		  break;
	  case UPDATE_QUILL_AD:
		  ipattr = ATTR_QUILL_DB_IP_ADDR;
		  break;
	  case UPDATE_COLLECTOR_AD:
		  ipattr = ATTR_COLLECTOR_IP_ADDR;
		  break;
	  case UPDATE_LICENSE_AD:
	  case UPDATE_CKPT_SRVR_AD:
	  case UPDATE_STORAGE_AD:
	  case UPDATE_HAD_AD:
	  case UPDATE_AD_GENERIC:
      case UPDATE_GRID_AD:
		  break;
	default:
		dprintf(D_ALWAYS,
				"ERROR: Unexpected command %d from %s in ValidateClassAd()\n",
				command,
				sock->get_sinful_peer());
		return false;
	}

	if(ipattr) {
		MyString my_address;
		MyString subsys_ipaddr;

			// Some ClassAds contain two copies of the IP address,
			// one named "MyAddress" and one named "<SUBSYS>IpAddr".
			// If the latter exists, then it _must_ match the former,
			// because people may be filtering in COLLECTOR_REQUIREMENTS
			// on MyAddress, and we don't want them to have to worry
			// about filtering on the older cruftier <SUBSYS>IpAddr.

		if( clientAd->LookupString( ipattr, subsys_ipaddr ) ) {
			clientAd->LookupString( ATTR_MY_ADDRESS, my_address );
			if( my_address != subsys_ipaddr ) {
				dprintf(D_ALWAYS,
				        "%s VIOLATION: ClassAd from %s advertises inconsistent"
				        " IP addresses: %s=%s, %s=%s\n",
				        COLLECTOR_REQUIREMENTS,
				        (sock ? sock->get_sinful_peer() : "(NULL)"),
				        ipattr, subsys_ipaddr.Value(),
				        ATTR_MY_ADDRESS, my_address.Value());
				return false;
			}
		}
	}


		// Now verify COLLECTOR_REQUIREMENTS
	int collector_req_result = 0;
	if( !m_collector_requirements->EvalBool(COLLECTOR_REQUIREMENTS,clientAd,collector_req_result) ) {
		dprintf(D_ALWAYS,"WARNING: %s did not evaluate to a boolean result.\n",COLLECTOR_REQUIREMENTS);
		collector_req_result = 0;
	}
	if( !collector_req_result ) {
		static int details_shown=0;
		bool show_details = (details_shown<10) || (DebugFlags & D_FULLDEBUG);
		dprintf(D_ALWAYS,"%s VIOLATION: requirements do not match ad from %s.%s\n",
				COLLECTOR_REQUIREMENTS,
				sock ? sock->get_sinful_peer() : "(null)",
				show_details ? " Contents of the ClassAd:" : " (turn on D_FULLDEBUG to see details)");
		if( show_details ) {
			details_shown += 1;
			clientAd->dPrint(D_ALWAYS);
		}

		return false;
	}

	return true;
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

	if( !ValidateClassAd(command,clientAd,sock) ) {
		return NULL;
	}

	// mux on command
	switch (command)
	{
	  case UPDATE_STARTD_AD:
	  case UPDATE_STARTD_AD_WITH_ACK:
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

				// Fix up some stuff in the private ad that we depend on.
				// We started doing this in 7.2.0, so once we no longer
				// care about compatibility with stuff from before then,
				// the startd could stop bothering to send these attributes.

				// Queries of private ads depend on the following:
			pvtAd->SetMyTypeName( STARTD_ADTYPE );

				// Negotiator matches up private ad with public ad by
				// using the following.
			if( retVal ) {
				pvtAd->CopyAttribute( ATTR_MY_ADDRESS, retVal );
				pvtAd->CopyAttribute( ATTR_NAME, retVal );
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

	  case MERGE_STARTD_AD:
		if (!makeStartdAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=mergeClassAd (StartdAds, "StartdAd     ", "Start",
							  clientAd, hk, hashString, insert, from );
		break;

#ifdef HAVE_EXT_POSTGRESQL
	  case UPDATE_QUILL_AD:
		if (!makeQuillAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (QuillAds, "QuillAd     ", "Quill",
							  clientAd, hk, hashString, insert, from );
		break;
#endif /* HAVE_EXT_POSTGRESQL */

	  case UPDATE_SCHEDD_AD:
		if (!makeScheddAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
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
		// CRUFT: Before 7.3.2, submitter ads had a MyType of
		//   "Scheduler". The only way to tell the difference
		//   was that submitter ads didn't have ATTR_NUM_USERS.
		//   Coerce MyStype to "Submitter" for ads coming from
		//   these older schedds.
		clientAd->SetMyTypeName( SUBMITTER_ADTYPE );
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

	  case UPDATE_GRID_AD:
		if (!makeGridAdHashKey(hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (GridAds, "GridAd  ", "Grid",
							  clientAd, hk, hashString, insert, from );
          break;

	  case UPDATE_AD_GENERIC:
	  {
		  const char *type_str = clientAd->GetMyTypeName();
		  if (type_str == NULL) {
			  dprintf(D_ALWAYS, "collect: UPDATE_AD_GENERIC: ad has no type\n");
			  insert = -3;
			  retVal = 0;
			  break;
		  }
		  MyString type(type_str);
		  CollectorHashTable *cht = findOrCreateTable(type);
		  if (cht == NULL) {
			  dprintf(D_ALWAYS, "collect: findOrCreateTable failed\n");
			  insert = -3;
			  retVal = 0;
			  break;
		  }
		  if (!makeGenericAdHashKey (hk, clientAd, from))
		  {
			  dprintf(D_ALWAYS, "Could not make haskey --- ignoring ad\n");
			  insert = -3;
			  retVal = 0;
			  break;
		  }
		  hashString.Build(hk);
		  retVal = updateClassAd(*cht, type_str, type_str, clientAd,
					 hk, hashString, insert, from);
		  break;
	  }

	  case UPDATE_XFER_SERVICE_AD:
		if (!makeXferServiceAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (XferServiceAds, "XferServiceAd  ",
							  "XferService",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_LEASE_MANAGER_AD:
		if (!makeLeaseManagerAdHashKey (hk, clientAd, from))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
			// first, purge all the existing LeaseManager ads, since we
			// want to enforce that *ONLY* 1 manager is in the
			// collector any given time.
		purgeHashTable( LeaseManagerAds );
		retVal=updateClassAd (LeaseManagerAds, "LeaseManagerAd  ",
							  "LeaseManager",
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
  	  case QUERY_XFER_SERVICE_ADS:
  	  case QUERY_LEASE_MANAGER_ADS:
	  case QUERY_GENERIC_ADS:
	  case INVALIDATE_STARTD_ADS:
	  case INVALIDATE_SCHEDD_ADS:
	  case INVALIDATE_MASTER_ADS:
	  case INVALIDATE_GATEWAY_ADS:
	  case INVALIDATE_CKPT_SRVR_ADS:
	  case INVALIDATE_SUBMITTOR_ADS:
	  case INVALIDATE_COLLECTOR_ADS:
	  case INVALIDATE_NEGOTIATOR_ADS:
	  case INVALIDATE_HAD_ADS:
	  case INVALIDATE_XFER_SERVICE_ADS:
	  case INVALIDATE_LEASE_MANAGER_ADS:
	  case INVALIDATE_ADS_GENERIC:
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

#ifdef HAVE_EXT_POSTGRESQL
		case QUILL_AD:
			if (QuillAds.lookup (hk, val) == -1)
				return 0;
			break;
#endif /* HAVE_EXT_POSTGRESQL */

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

        case GRID_AD:
            if (GridAds.lookup (hk, val) == -1)
                return 0;
			break;

		case XFER_SERVICE_AD:
			if (XferServiceAds.lookup (hk, val) == -1)
				return 0;
			break;

		case LEASE_MANAGER_AD:
			if (LeaseManagerAds.lookup (hk, val) == -1)
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

#ifdef HAVE_EXT_POSTGRESQL
		case QUILL_AD:
			return !QuillAds.remove (hk);
#endif /* HAVE_EXT_POSTGRESQL */

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

        case GRID_AD:
			return !GridAds.remove (hk);

		case XFER_SERVICE_AD:
			return !XferServiceAds.remove (hk);

		case LEASE_MANAGER_AD:
			return !LeaseManagerAds.remove (hk);

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
			   const sockaddr_in * /*from*/ )
{
	ClassAd		*old_ad, *new_ad;
	MyString	buf;
	time_t		now;

		// NOTE: LastHeardFrom will already be in ad if we are loading
		// adds from the offline classad collection, so don't mess with
		// it if it is already there
	if( !ad->LookupExpr(ATTR_LAST_HEARD_FROM) ) {
		(void) time (&now);
		if (now == (time_t) -1)
		{
			EXCEPT ("Error reading system time!");
		}	
		buf.sprintf( "%s = %d", ATTR_LAST_HEARD_FROM, (int)now);
		ad->Insert ( buf.Value() );
	}

	// this time stamped ad is the new ad
	new_ad = ad;

	// check if it already exists in the hash table ...
	if ( hashTable.lookup (hk, old_ad) == -1)
    {	 	
		// no ... new ad
		dprintf (D_ALWAYS, "%s: Inserting ** \"%s\"\n", adType, hashString.Value() );

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
		dprintf (D_FULLDEBUG, "%s: Updating ... \"%s\"\n", adType, hashString.Value() );

		// Update statistics
		collectorStats->update( label, old_ad, new_ad );

		// Now, finally, store the new ClassAd
		if (hashTable.remove(hk) == -1) {
			EXCEPT( "Error removing ad" );
		}
		if (hashTable.insert(hk, new_ad) == -1) {
			EXCEPT( "Error inserting ad" );
		}

		delete old_ad;

		insert = 0;
		return new_ad;
	}
}

ClassAd * CollectorEngine::
mergeClassAd (CollectorHashTable &hashTable,
			   const char *adType,
			   const char *label,
			   ClassAd *new_ad,
			   AdNameHashKey &hk,
			   const MyString &hashString,
			   int  &insert,
			   const sockaddr_in * /*from*/ )
{
	ClassAd		*old_ad = NULL;

	insert = 0;

	// check if it already exists in the hash table ...
	if ( hashTable.lookup (hk, old_ad) == -1)
    {	 	
		dprintf (D_ALWAYS, "%s: Failed to merge update for ** \"%s\" because "
				 "no existing ad matches.\n", adType, hashString.Value() );
	}
	else
    {
		// yes ... old ad must be updated
		dprintf (D_FULLDEBUG, "%s: Merging update for ... \"%s\"\n",
				 adType, hashString.Value() );

		// Now, finally, merge the new ClassAd into the old one
		MergeClassAds(old_ad,new_ad,true);
	}
	delete new_ad;
	return old_ad;
}

#if 0
void
CollectorEngine::updateAd ( ClassAd *ad ) {



    updateClassAd ( StartdAds)

    const char      *label = "Start";
    AdNameHashKey   hashKey;
    MyString	    key;
    ClassAd		    *old_ad;
    time_t		    now;

    /* get the current time */
    time ( &now );

    if ( (time_t) -1 == now ) {
        EXCEPT ("Error reading system time!");
    }

    buf.sprintf( "%s = %d", ATTR_LAST_HEARD_FROM, (int) now );
	ad->Insert ( buf.Value () );

    /* update statistics */
    collectorStats->update ( label, NULL, ad );

    /* make a hash key for the ad */
    if ( !makeStartdAdHashKey ( hashKey, (ClassAd*) &ad, NULL ) ) {

        dprintf (
            D_FULLDEBUG,
            "OfflineCollectorPlugin::update: "
            "failed to hash class ad. Ignoring.\n" );

        return;

    }

    /* store it the ad */
    hashKey.sprint ( key );
    if ( -1 == StartdAds.insert ( key, ad ) ) {
        EXCEPT ( "Error inserting ad (out of memory)" );
	}

}
#endif

void
CollectorEngine::
housekeeper()
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

	dprintf (D_ALWAYS, "\tCleaning StartdPrivateAds ...\n");
	cleanHashTable (StartdPrivateAds, now, makeStartdAdHashKey);

#ifdef HAVE_EXT_POSTGRESQL
	dprintf (D_ALWAYS, "\tCleaning QuillAds ...\n");
	cleanHashTable (QuillAds, now, makeQuillAdHashKey);
#endif /* HAVE_EXT_POSTGRESQL */

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

    dprintf (D_ALWAYS, "\tCleaning GridAds ...\n");
	cleanHashTable (GridAds, now, makeGridAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning XferServiceAds ...\n");
	cleanHashTable (XferServiceAds, now, makeXferServiceAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning LeaseManagerAds ...\n");
	cleanHashTable (LeaseManagerAds, now, makeLeaseManagerAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning Generic Ads ...\n");
	CollectorHashTable *cht;
	GenericAds.startIterations();
	while (GenericAds.iterate(cht)) {
		cleanHashTable (*cht, now, makeGenericAdHashKey);
	}

	// cron manager
	event_mgr();

	dprintf (D_ALWAYS, "Housekeeper:  Done cleaning\n");
}

void CollectorEngine::
cleanHashTable (CollectorHashTable &hashTable, time_t now,
				bool (*makeKey) (AdNameHashKey &, ClassAd *, sockaddr_in *) )
{
	ClassAd  *ad;
	int   	 timeStamp;
	int		 max_lifetime;
	AdNameHashKey  hk;
	double   timeDiff;
	MyString	hkString;

	hashTable.startIterations ();
	while (hashTable.iterate (ad))
	{
		// Read the timestamp of the ad
		if (!ad->LookupInteger (ATTR_LAST_HEARD_FROM, timeStamp)) {
			dprintf (D_ALWAYS, "\t\tError looking up time stamp on ad\n");
			continue;
		}

		// how long has it been since the last update?
		timeDiff = difftime( now, timeStamp );

		if( !ad->LookupInteger( ATTR_CLASSAD_LIFETIME, max_lifetime ) ) {
			max_lifetime = machineUpdateInterval;
		}

		// check if it has expired
		if ( timeDiff > (double) max_lifetime )
		{
			// then remove it from the segregated table
			(*makeKey) (hk, ad, NULL);
			hk.sprint( hkString );
			dprintf (D_ALWAYS,"\t\t**** Removing stale ad: \"%s\"\n", hkString.Value() );
			if (hashTable.remove (hk) == -1)
			{
				dprintf (D_ALWAYS, "\t\tError while removing ad\n");
			}
			delete ad;
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
		delete ad;
	}
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

static int
killGenericHashTable(CollectorHashTable *table)
{
	ASSERT(table != NULL);
	killHashTable(*table);
	delete table;
	return 0;
}
