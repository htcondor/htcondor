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
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "classad_merge.h"

//-------------------------------------------------------------

#include "collector.h"
#include "collector_engine.h"

static void killHashTable (CollectorHashTable &);
static int killGenericHashTable(CollectorHashTable *);
static void purgeHashTable (CollectorHashTable &);

int 	engine_clientTimeoutHandler (Service *);
int 	engine_housekeepingHandler  (Service *);

CollectorEngine::CollectorEngine (CollectorStats *stats ) :
	StartdAds     (&adNameHashFunction),
	StartdPrivateAds(&adNameHashFunction),
	ScheddAds     (&adNameHashFunction),
	SubmittorAds  (&adNameHashFunction),
	LicenseAds    (&adNameHashFunction),
	MasterAds     (&adNameHashFunction),
	StorageAds    (&adNameHashFunction),
	AccountingAds (&adNameHashFunction),
	CkptServerAds (&adNameHashFunction),
	GatewayAds    (&adNameHashFunction),
	CollectorAds  (&adNameHashFunction),
	NegotiatorAds (&adNameHashFunction),
	HadAds        (&adNameHashFunction),
	GridAds       (&adNameHashFunction),
	GenericAds    (&hashFunction),
	__self_ad__(0)
{
	clientTimeout = 20;
	machineUpdateInterval = 30;
	m_forwardInterval = machineUpdateInterval / 3;
	m_forwardFilteringEnabled = false;
	housekeeperTimerID = -1;

	m_allowOnlyOneNegotiator = param_boolean("COLLECTOR_ALLOW_ONLY_ONE_NEGOTIATOR", false);

	collectorStats = stats;
	m_collector_requirements = NULL;
	m_get_ad_options = 0;
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
	killHashTable (AccountingAds);
	killHashTable (CkptServerAds);
	killHashTable (GatewayAds);
	killHashTable (CollectorAds);
	killHashTable (NegotiatorAds);
	killHashTable (HadAds);
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
CollectorEngine::setCollectorRequirements( char const *str, std::string &error_desc )
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
	// Are we filtering updates that we forward to the view collector?
	std::string watch_list;
	param(watch_list,"COLLECTOR_FORWARD_WATCH_LIST", "State,Cpus,Memory,IdleJobs,ClaimId,Capability,ClaimIdList,ChildClaimIds");
	m_forwardWatchList.clearAll();
	m_forwardWatchList.initializeFromString(watch_list.c_str());

	m_forwardFilteringEnabled = param_boolean( "COLLECTOR_FORWARD_FILTERING", false );

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

	m_forwardInterval = param_integer("COLLECTOR_FORWARD_INTERVAL", machineUpdateInterval / 3, 0);

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
invokeHousekeeper (AdTypes adType)
{
	time_t now;

	(void) time (&now);
	if (now == (time_t) -1)
	{
		dprintf (D_ALWAYS, "Error in reading system time --- aborting\n");
		return 0;
	}

	CollectorHashTable *table=0;
	CollectorEngine::HashFunc func;
	if (LookupByAdType(adType, table, func)) {
		cleanHashTable(*table, now, func);
	} else {
		if (GENERIC_AD == adType) {
			CollectorHashTable *cht=0;
			GenericAds.startIterations();
			while (GenericAds.iterate(cht)) {
				cleanHashTable (*cht, now, makeGenericAdHashKey);
			}
		} else {
			return 0;
		}
	}

	return 1;
}

int
CollectorEngine::invalidateAds(AdTypes adType, ClassAd &query)
{
	CollectorHashTable *table=0;
	CollectorEngine::HashFunc func;
	if (!LookupByAdType(adType, table, func)) {
		dprintf (D_ALWAYS, "Unknown type %d\n", adType);
		return 0;
	}

	int count = 0;
	ClassAd  *ad;
	AdNameHashKey  hk;
	std::string hkString;
	(*table).startIterations();
	while ((*table).iterate (ad)) {
		if (IsAHalfMatch(&query, ad)) {
			(*table).getCurrentKey(hk);
			hk.sprint(hkString);
			if ((*table).remove(hk) == -1) {
				dprintf(D_ALWAYS,
						"\t\tError while removing ad: \"%s\"\n",
						hkString.c_str());
			} else {
				dprintf(D_ALWAYS,
						"\t\t**** Invalidating ad: \"%s\"\n",
						hkString.c_str());
				delete ad;
				count++;
			}
		}
	}

	return count;
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
	//int retval = 1;

	if (GENERIC_AD == adType) {
		return walkGenericTables(scanFunction);
	} else if (ANY_AD == adType) {
		return
			AccountingAds.walk(scanFunction) &&
			StorageAds.walk(scanFunction) &&
			CkptServerAds.walk(scanFunction) &&
			LicenseAds.walk(scanFunction) &&
			CollectorAds.walk(scanFunction) &&
			StartdAds.walk(scanFunction) &&
			ScheddAds.walk(scanFunction) &&
			MasterAds.walk(scanFunction) &&
			SubmittorAds.walk(scanFunction) &&
			NegotiatorAds.walk(scanFunction) &&
			HadAds.walk(scanFunction) &&
			GridAds.walk(scanFunction) &&
			walkGenericTables(scanFunction);
	}

	CollectorHashTable *table;
	CollectorEngine::HashFunc func;
	if (!LookupByAdType(adType, table, func)) {
		dprintf (D_ALWAYS, "Unknown type %d\n", adType);
		return 0;
	}

	// walk the hash table
	ClassAd *ad;
	(*table).startIterations ();
	while ((*table).iterate (ad))
	{
		// call scan function for each ad
		if (!scanFunction (ad))
		{
			//retval = 0;
			break;
		}
	}

	return 1;
}


CollectorHashTable *CollectorEngine::findOrCreateTable(const std::string &type)
{
	CollectorHashTable *table=0;
	if (GenericAds.lookup(type, table) == -1) {
		dprintf(D_ALWAYS, "creating new table for type %s\n", type.c_str());
		table = new CollectorHashTable(&adNameHashFunction);
		if (GenericAds.insert(type, table) == -1) {
			dprintf(D_ALWAYS,  "error adding new generic hash table\n");
			delete table;
			return NULL;
		}
	}

	return table;
}

#ifdef PROFILE_RECEIVE_UPDATE
collector_runtime_probe CollectorEngine_ruc_runtime;
collector_runtime_probe CollectorEngine_ruc_getAd_runtime;
collector_runtime_probe CollectorEngine_ruc_authid_runtime;
collector_runtime_probe CollectorEngine_ruc_collect_runtime;
collector_runtime_probe CollectorEngine_rucc_runtime;
collector_runtime_probe CollectorEngine_rucc_validateAd_runtime;
collector_runtime_probe CollectorEngine_rucc_makeHashKey_runtime;
collector_runtime_probe CollectorEngine_rucc_insertAd_runtime;
collector_runtime_probe CollectorEngine_rucc_updateAd_runtime;
collector_runtime_probe CollectorEngine_rucc_getPvtAd_runtime;
collector_runtime_probe CollectorEngine_rucc_insertPvtAd_runtime;
collector_runtime_probe CollectorEngine_rucc_updatePvtAd_runtime;
collector_runtime_probe CollectorEngine_rucc_repeatAd_runtime;
collector_runtime_probe CollectorEngine_rucc_other_runtime;
#endif


ClassAd *CollectorEngine::
collect (int command, Sock *sock, const condor_sockaddr& from, int &insert)
{
	ClassAd	*clientAd;
	ClassAd	*rval;

#ifdef PROFILE_RECEIVE_UPDATE
	_condor_auto_accum_runtime<collector_runtime_probe> rt(CollectorEngine_ruc_runtime);
	double rt_last = rt.begin;
#endif

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	// get the ad
	clientAd = new ClassAd;
	if (!clientAd) return 0;

	if( !getClassAdEx(sock, *clientAd, m_get_ad_options) )
	{
		dprintf (D_ALWAYS,"Command %d on Sock not followed by ClassAd (or timeout occured)\n",
				command);
		delete clientAd;
		sock->end_of_message();
		return 0;
	}

#ifdef PROFILE_RECEIVE_UPDATE
	double delta_time = rt.tick(rt_last);
	CollectorEngine_ruc_getAd_runtime.Add(delta_time);
#endif

	// insert the authenticated user into the ad itself
	const char* authn_user = sock->getFullyQualifiedUser();
	if (authn_user) {
		clientAd->Assign(ATTR_AUTHENTICATED_IDENTITY, authn_user);
		clientAd->Assign(ATTR_AUTHENTICATION_METHOD, sock->getAuthenticationMethodUsed());
	} else {
		// remove it from the ad if it's not authenticated.
		clientAd->Delete(ATTR_AUTHENTICATED_IDENTITY);
		clientAd->Delete(ATTR_AUTHENTICATION_METHOD);
	}

#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_ruc_authid_runtime.Add(rt.tick(rt_last));
#endif

	rval = collect(command, clientAd, from, insert, sock);
#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_ruc_collect_runtime.Add(rt.tick(rt_last));
#endif

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

	if( !m_collector_requirements && (command != UPDATE_OWN_SUBMITTOR_AD)) {
			// no need to do any of the following checks if the admin has
			// not configured any COLLECTOR_REQUIREMENTS and there aren't
			// any mandatory checks.
		return true;
	}


	char const *ipattr = NULL;
	char const *check_owner = nullptr;
	switch( command ) {
	  case MERGE_STARTD_AD:
	  case UPDATE_STARTD_AD:
	  case UPDATE_STARTD_AD_WITH_ACK:
		  ipattr = ATTR_STARTD_IP_ADDR;
		  break;
	  case UPDATE_OWN_SUBMITTOR_AD:
		check_owner = ATTR_NAME;
		// fallthrough
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
	  case UPDATE_COLLECTOR_AD:
		  ipattr = ATTR_COLLECTOR_IP_ADDR;
		  break;
	  case UPDATE_LICENSE_AD:
	  case UPDATE_CKPT_SRVR_AD:
	  case UPDATE_STORAGE_AD:
	  case UPDATE_HAD_AD:
	  case UPDATE_AD_GENERIC:
      case UPDATE_GRID_AD:
	  case UPDATE_ACCOUNTING_AD:
	  default:
		  break;
	}

	if(ipattr) {
		std::string my_address;
		std::string subsys_ipaddr;

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
				        ipattr, subsys_ipaddr.c_str(),
				        ATTR_MY_ADDRESS, my_address.c_str());
				return false;
			}
		}
	}
		// Verify the owner matches the value in the specified attribute.
	if (check_owner) {
		const char *sock_owner = sock->getOwner();
		if (!sock_owner || !*sock_owner || !strcmp(sock_owner, "unmapped")) {
			return false;
		}
		std::string ad_owner;
		if (!clientAd->EvaluateAttrString(check_owner, ad_owner)) {
			return false;
		}
		auto at_sign = ad_owner.find("@");
		if (at_sign != std::string::npos) {
			ad_owner = ad_owner.substr(0, at_sign);
		}
		auto last_dot = ad_owner.find_last_of(".");
		if (last_dot != std::string::npos) {
			ad_owner = ad_owner.substr(last_dot+1);
		}
		if (strcmp(sock_owner, ad_owner.c_str())) {
			return false;
		}
	}


		// Now verify COLLECTOR_REQUIREMENTS
	if( !m_collector_requirements ) {
		return true;
	}
	bool collector_req_result = false;
	if( !EvalBool(COLLECTOR_REQUIREMENTS,m_collector_requirements,clientAd,collector_req_result) ) {
		dprintf(D_ALWAYS,"WARNING: %s did not evaluate to a boolean result.\n",COLLECTOR_REQUIREMENTS);
		collector_req_result = false;
	}
	if( !collector_req_result ) {
		if( IsDebugLevel(D_MACHINE) ) { // Is this still an optimization?
			dprintf( D_MACHINE,
				"%s VIOLATION: requirements do not match ad from %s.\n",
				COLLECTOR_REQUIREMENTS,
				sock ? sock->get_sinful_peer() : "(null)" );
			}
		if( IsDebugVerbose( D_MACHINE ) ) { // Is this still an optimization?
			dPrintAd( D_MACHINE | D_VERBOSE, * clientAd );
		}
		return false;
	}

	return true;
}

bool   last_updateClassAd_was_insert;

ClassAd *CollectorEngine::
collect (int command,ClassAd *clientAd,const condor_sockaddr& from,int &insert,Sock *sock)
{
	ClassAd		*retVal;
	ClassAd		*pvtAd;
	int		insPvt;
	AdNameHashKey		hk;
	HashString	hashString;
	static int repeatStartdAds = -1;		// for debugging
	ClassAd		*clientAdToRepeat = NULL;
#ifdef PROFILE_RECEIVE_UPDATE
	_condor_auto_accum_runtime<collector_runtime_probe> rt(CollectorEngine_rucc_runtime);
	double rt_last = rt.begin;
#endif

	if (repeatStartdAds == -1) {
		repeatStartdAds = param_integer("COLLECTOR_REPEAT_STARTD_ADS",0);
	}

	if( !ValidateClassAd(command,clientAd,sock) ) {
	    insert = -4;
		return NULL;
	}

#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_rucc_validateAd_runtime.Add(rt.tick(rt_last));
#endif

	// mux on command
	switch (command)
	{
	  case UPDATE_STARTD_AD:
	  case UPDATE_STARTD_AD_WITH_ACK:
		if ( repeatStartdAds > 0 ) {
			clientAdToRepeat = new ClassAd(*clientAd);
		}
		if (!makeStartdAdHashKey (hk, clientAd))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );

#ifdef PROFILE_RECEIVE_UPDATE
		CollectorEngine_rucc_makeHashKey_runtime.Add(rt.tick(rt_last));
#endif

		retVal=updateClassAd (StartdAds, "StartdAd     ", "Start",
							  clientAd, hk, hashString, insert, from );

#ifdef PROFILE_RECEIVE_UPDATE
		if (last_updateClassAd_was_insert) { CollectorEngine_rucc_insertAd_runtime.Add(rt.tick(rt_last));
		} else { CollectorEngine_rucc_updateAd_runtime.Add(rt.tick(rt_last)); }
#endif

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
			if( !getClassAdEx(sock, *pvtAd, m_get_ad_options) )
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
			SetMyTypeName( *pvtAd, STARTD_ADTYPE );

				// Negotiator matches up private ad with public ad by
				// using the following.
			if( retVal ) {
				CopyAttribute( ATTR_MY_ADDRESS, *pvtAd, *retVal );
				CopyAttribute( ATTR_NAME, *pvtAd, *retVal );
			}

#ifdef PROFILE_RECEIVE_UPDATE
			CollectorEngine_rucc_getPvtAd_runtime.Add(rt.tick(rt_last));
#endif

			// insert the private ad into its hashtable --- use the same
			// hash key as the public ad
			(void) updateClassAd (StartdPrivateAds, "StartdPvtAd  ",
								  "StartdPvt", pvtAd, hk, hashString, insPvt,
								  from );
#ifdef PROFILE_RECEIVE_UPDATE
			if (last_updateClassAd_was_insert) { CollectorEngine_rucc_insertPvtAd_runtime.Add(rt.tick(rt_last));
			} else { CollectorEngine_rucc_updatePvtAd_runtime.Add(rt.tick(rt_last)); }
#endif
		}

		// create fake duplicates of this ad, each with a different name, if
		// we are told to do so.  this feature exists for developer
		// scalability testing.
		if ( repeatStartdAds > 0 && clientAdToRepeat ) {
			ClassAd *fakeAd;
			int n;
			char newname[150],oldname[130];
			oldname[0] = '\0';
			clientAdToRepeat->LookupString(ATTR_NAME,oldname,sizeof(oldname));
			for (n=0;n<repeatStartdAds;n++) {
				fakeAd = new ClassAd(*clientAdToRepeat);
				snprintf(newname,sizeof(newname),
						 "fake%d-%s",n,oldname);
				fakeAd->Assign(ATTR_NAME, newname);
				makeStartdAdHashKey (hk, fakeAd);
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
#ifdef PROFILE_RECEIVE_UPDATE
			CollectorEngine_rucc_repeatAd_runtime.Add(rt.tick(rt_last));
#endif
		}
		break;

	  case MERGE_STARTD_AD:
		if (!makeStartdAdHashKey (hk, clientAd))
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

	  case UPDATE_SCHEDD_AD:
		if (!makeScheddAdHashKey (hk, clientAd))
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

	  case UPDATE_OWN_SUBMITTOR_AD: // fallthrough
	  case UPDATE_SUBMITTOR_AD:
		// use the same hashkey function as a schedd ad
		if (!makeScheddAdHashKey (hk, clientAd))
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
		if (!makeLicenseAdHashKey (hk, clientAd))
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
		if (!makeMasterAdHashKey (hk, clientAd))
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
		if (!makeCkptSrvrAdHashKey (hk, clientAd))
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
		if (!makeCollectorAdHashKey (hk, clientAd))
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
		if (!makeStorageAdHashKey (hk, clientAd))
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

	  case UPDATE_ACCOUNTING_AD:
		if (!makeAccountingAdHashKey (hk, clientAd))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		retVal=updateClassAd (AccountingAds, "AccountingAd  ", "Accouting",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_NEGOTIATOR_AD:
		if (!makeNegotiatorAdHashKey (hk, clientAd))
		{
			dprintf (D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			insert = -3;
			retVal = 0;
			break;
		}
		hashString.Build( hk );
		if (m_allowOnlyOneNegotiator) {
			// first, purge all the existing negotiator ads, since we
			// want to enforce that *ONLY* 1 negotiator is in the
			// collector any given time.
			purgeHashTable( NegotiatorAds );
		}
		retVal=updateClassAd (NegotiatorAds, "NegotiatorAd  ", "Negotiator",
							  clientAd, hk, hashString, insert, from );
		break;

	  case UPDATE_HAD_AD:
		if (!makeHadAdHashKey (hk, clientAd))
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
		if (!makeGridAdHashKey(hk, clientAd))
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
		  const char *type_str = GetMyTypeName(*clientAd);
		  if (type_str == NULL) {
			  dprintf(D_ALWAYS, "collect: UPDATE_AD_GENERIC: ad has no type\n");
			  insert = -3;
			  retVal = 0;
			  break;
		  }
		  std::string type(type_str);
		  CollectorHashTable *cht = findOrCreateTable(type);
		  if (cht == NULL) {
			  dprintf(D_ALWAYS, "collect: findOrCreateTable failed\n");
			  insert = -3;
			  retVal = 0;
			  break;
		  }
		  if (!makeGenericAdHashKey (hk, clientAd))
		  {
			  dprintf(D_ALWAYS, "Could not make hashkey --- ignoring ad\n");
			  insert = -3;
			  retVal = 0;
			  break;
		  }
		  hashString.Build(hk);
		  retVal = updateClassAd(*cht, type_str, type_str, clientAd,
					 hk, hashString, insert, from);
		  break;
	  }

	  case QUERY_STARTD_ADS:
	  case QUERY_SCHEDD_ADS:
	  case QUERY_MASTER_ADS:
	  case QUERY_SUBMITTOR_ADS:
	  case QUERY_CKPT_SRVR_ADS:
	  case QUERY_STARTD_PVT_ADS:
	  case QUERY_COLLECTOR_ADS:
  	  case QUERY_NEGOTIATOR_ADS:
  	  case QUERY_HAD_ADS:
	  case QUERY_GENERIC_ADS:
	  case INVALIDATE_STARTD_ADS:
	  case INVALIDATE_SCHEDD_ADS:
	  case INVALIDATE_MASTER_ADS:
	  case INVALIDATE_CKPT_SRVR_ADS:
	  case INVALIDATE_SUBMITTOR_ADS:
	  case INVALIDATE_COLLECTOR_ADS:
	  case INVALIDATE_NEGOTIATOR_ADS:
	  case INVALIDATE_HAD_ADS:
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

#ifdef PROFILE_RECEIVE_UPDATE
	if (command != UPDATE_STARTD_AD && command != UPDATE_STARTD_AD_WITH_ACK) {
		CollectorEngine_rucc_other_runtime.Add(rt.tick(rt_last));
	}
#endif


	// return the updated ad
	return retVal;
}

ClassAd *CollectorEngine::
lookup (AdTypes adType, AdNameHashKey &hk)
{
	ClassAd *val;

	CollectorHashTable *table;
	CollectorEngine::HashFunc func;
	if (!LookupByAdType(adType, table, func)) {
		return 0;
	}
	if (table->lookup(hk, val) == -1) {
		return 0;
	}

	return val;
}

int CollectorEngine::remove (AdTypes t_AddType, const ClassAd & c_query, bool *query_contains_hash_key)
{
	int iRet = 0;
	AdNameHashKey hk;
	CollectorHashTable * table;
	HashFunc makeKey;
	std::string hkString;

	if( query_contains_hash_key ) {
		*query_contains_hash_key = false;
	}

	// making it generic so any would be invalid query can contain these params.
	if ( LookupByAdType (t_AddType, table, makeKey) )
	{
		ClassAd * pAd=0;
		// try to create a hk from the query ad if it is possible.
		if ( (*makeKey) (hk, &c_query) ) {
			if( query_contains_hash_key ) {
				*query_contains_hash_key = true;
			}
			if( table->lookup(hk, pAd) != -1 )
			{
				hk.sprint( hkString );
				iRet = !table->remove(hk);
				dprintf (D_ALWAYS,"\t\t**** Removed(%d) ad(s): \"%s\"\n", iRet, hkString.c_str() );
				delete pAd;
			}
		}
	}

	return ( iRet );
}

int CollectorEngine::expire( AdTypes adType, const ClassAd & query, bool * queryContainsHashKey ) {
    int rVal = 0;
    if( queryContainsHashKey ) { * queryContainsHashKey = false; }

    HashFunc hFunc;
    CollectorHashTable * hTable;
    if( LookupByAdType( adType, hTable, hFunc ) ) {
        AdNameHashKey hKey;
        if( (* hFunc)( hKey, & query ) ) {
            if( queryContainsHashKey ) { * queryContainsHashKey = true; }

            ClassAd * cAd = NULL;
            if( hTable->lookup( hKey, cAd ) != -1 ) {
                cAd->Assign( ATTR_LAST_HEARD_FROM, 1 );
                
                if( CollectorDaemon::offline_plugin_.expire( * cAd ) == true ) {
                    return rVal;
                }

                rVal = hTable->remove( hKey );
                if( rVal == -1 ) {
                    dprintf( D_ALWAYS, "\t\t Error removing ad\n" );
                    return 0;
                }
                rVal = (! rVal);

                std::string hkString;
                hKey.sprint( hkString );
                dprintf( D_ALWAYS, "\t\t**** Removed(%d) stale ad(s): \"%s\"\n", rVal, hkString.c_str() );

                delete cAd;
            }
        }
    }

	return rVal;
}

int CollectorEngine::
remove (AdTypes adType, AdNameHashKey &hk)
{
	CollectorHashTable *table;
	CollectorEngine::HashFunc func;
	if (!LookupByAdType(adType, table, func)) {
		return 0;
	}
	return !table->remove(hk);
}

void CollectorEngine::
identifySelfAd(ClassAd * ad)
{
	__self_ad__ = (void*)ad;
}

extern bool   last_updateClassAd_was_insert;

ClassAd * CollectorEngine::
updateClassAd (CollectorHashTable &hashTable,
			   const char *adType,
			   const char *label,
			   ClassAd *ad,
			   AdNameHashKey &hk,
			   const std::string &hashString,
			   int  &insert,
			   const condor_sockaddr& /*from*/ )
{
	ClassAd		*old_ad, *new_ad;
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
		ad->Assign(ATTR_LAST_HEARD_FROM, (int)now);
	}

	// this time stamped ad is the new ad
	new_ad = ad;
	last_updateClassAd_was_insert = false;

	// check if it already exists in the hash table ...
	if ( hashTable.lookup (hk, old_ad) == -1)
	{
		// no ... new ad
		last_updateClassAd_was_insert = true;
		dprintf (D_ALWAYS, "%s: Inserting ** \"%s\"\n", adType, hashString.c_str() );

		// Update statistics, but not for private ads we can't see
		if (strcmp(label, "StartdPvt") != 0) {
			collectorStats->update( label, NULL, new_ad );
		}

		// Now, store it away
		if (hashTable.insert (hk, new_ad) == -1)
		{
			EXCEPT ("Error inserting ad (out of memory)");
		}

		insert = 1;

		if ( m_forwardFilteringEnabled && ( strcmp( label, "Start" ) == 0 || strcmp( label, "StartdPvt" ) == 0 || strcmp( label, "Submittor" ) == 0 ) ) {
			new_ad->Assign( ATTR_LAST_FORWARDED, (int)time(NULL) );
		}

		return new_ad;
	}
	else
    {
		// yes ... old ad must be updated
		dprintf (D_FULLDEBUG, "%s: Updating ... \"%s\"\n", adType, hashString.c_str() );

		// Update statistics
		if (strcmp(label, "StartdPvt") != 0) {
			collectorStats->update( label, old_ad, new_ad );
		}

		// Now, finally, store the new ClassAd
		if (hashTable.remove(hk) == -1) {
			EXCEPT( "Error removing ad" );
		}
		if (hashTable.insert(hk, new_ad) == -1) {
			EXCEPT( "Error inserting ad" );
		}

		if ( m_forwardFilteringEnabled && ( strcmp( label, "Start" ) == 0 || strcmp( label, "StartdPvt" ) == 0 || strcmp( label, "Submittor" ) == 0 ) ) {
			bool forward = false;
			int last_forwarded = 0;
			old_ad->LookupInteger( "LastForwarded", last_forwarded );
			if ( last_forwarded + m_forwardInterval < time(NULL) ) {
				forward = true;
			} else {
				classad::Value old_val;
				classad::Value new_val;
				const char *attr;
				m_forwardWatchList.rewind();
				while ( (attr = m_forwardWatchList.next()) ) {
					// This treats attribute-not-present and
					// attribute-evaluates-to-UNDEFINED as equivalent.
					if ( old_ad->EvaluateAttr( attr, old_val ) &&
						 new_ad->EvaluateAttr( attr, new_val ) &&
						 !new_val.SameAs( old_val ) )
					{
						forward = true;
						break;
					}
				}
			}
			new_ad->Assign( ATTR_SHOULD_FORWARD, forward );
			new_ad->Assign( ATTR_LAST_FORWARDED, forward ? (int)time(NULL) : last_forwarded );
		}

		if (isSelfAd(old_ad)) { __self_ad__ = new_ad; }

		delete old_ad;

		insert = 0;
		return new_ad;
	}
}

ClassAd * CollectorEngine::
mergeClassAd (CollectorHashTable &hashTable,
			   const char *adType,
			   const char * /*label*/,
			   ClassAd *new_ad,
			   AdNameHashKey &hk,
			   const std::string &hashString,
			   int  &insert,
			   const condor_sockaddr& /*from*/ )
{
	ClassAd		*old_ad = NULL;

	insert = 0;

	// check if it already exists in the hash table ...
	if ( hashTable.lookup (hk, old_ad) == -1)
    {
		dprintf (D_ALWAYS, "%s: Failed to merge update for ** \"%s\" because "
				 "no existing ad matches.\n", adType, hashString.c_str() );
			// We should _NOT_ delete new_ad if we return NULL
			// because our caller will delete it in that case.
		return NULL;
	}
	else
    {
		// yes ... old ad must be updated
		dprintf (D_FULLDEBUG, "%s: Merging update for ... \"%s\"\n",
				 adType, hashString.c_str() );

			// Do not allow changes to some attributes
		ClassAd new_ad_copy(*new_ad);
		new_ad_copy.Delete(ATTR_AUTHENTICATED_IDENTITY);
		new_ad_copy.Delete(ATTR_MY_TYPE);
		new_ad_copy.Delete(ATTR_TARGET_TYPE);

		// Now, finally, merge the new ClassAd into the old one
		MergeClassAds(old_ad,&new_ad_copy,true);
	}
	delete new_ad;
	return old_ad;
}


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

	dprintf (D_ALWAYS, "\tCleaning AccountingAds ...\n");
	cleanHashTable (AccountingAds, now, makeAccountingAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning NegotiatorAds ...\n");
	cleanHashTable (NegotiatorAds, now, makeNegotiatorAdHashKey);

	dprintf (D_ALWAYS, "\tCleaning HadAds ...\n");
	cleanHashTable (HadAds, now, makeHadAdHashKey);

    dprintf (D_ALWAYS, "\tCleaning GridAds ...\n");
	cleanHashTable (GridAds, now, makeGridAdHashKey);

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
cleanHashTable (CollectorHashTable &hashTable, time_t now, HashFunc makeKey) const
{
	ClassAd         *ad;
	int             timeStamp;
	int             max_lifetime;
	AdNameHashKey   hk;
	double          timeDiff;
	std::string     hkString;

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
			(*makeKey) (hk, ad);
			hk.sprint( hkString );
			if( timeStamp == 0 ) {
				dprintf (D_ALWAYS,"\t\t**** Removing invalidated ad: \"%s\"\n", hkString.c_str() );
			}
			else {
				dprintf (D_ALWAYS,"\t\t**** Removing stale ad: \"%s\"\n", hkString.c_str() );
			    /* let the off-line plug-in know we are about to expire this ad, so it can
				   potentially mark the ad absent. if expire() returns false, then delete
				   the ad as planned; if it return true, it was likely marked as absent,
				   so then this ad should NOT be deleted. */
				if ( CollectorDaemon::offline_plugin_.expire( *ad ) == true ) {
					// plugin say to not delete this ad, so continue
					continue;
				} else {
					dprintf (D_ALWAYS,"\t\t**** Removing stale ad: \"%s\"\n", hkString.c_str() );
				}
			}
			if (hashTable.remove (hk) == -1)
			{
				dprintf (D_ALWAYS, "\t\tError while removing ad\n");
			}
			delete ad;
		}
	}
}


bool
CollectorEngine::LookupByAdType(AdTypes adType,
								CollectorHashTable *&table,
								CollectorEngine::HashFunc &func)
{
	switch (adType)
	{
		case STARTD_AD:
			table = &StartdAds;
			func = makeStartdAdHashKey;
			break;
		case SCHEDD_AD:
			table = &ScheddAds;
			func = makeScheddAdHashKey;
			break;
		case SUBMITTOR_AD:
			table = &SubmittorAds;
			func = makeScheddAdHashKey;
			break;
		case LICENSE_AD:
			table = &LicenseAds;
			func = makeLicenseAdHashKey;
			break;
		case MASTER_AD:
			table = &MasterAds;
			func = makeMasterAdHashKey;
			break;
		case CKPT_SRVR_AD:
			table = &CkptServerAds;
			func = makeCkptSrvrAdHashKey;
			break;
		case STARTD_PVT_AD:
			table = &StartdPrivateAds;
			func = makeStartdAdHashKey;
			break;
		case COLLECTOR_AD:
			table = &CollectorAds;
			func = makeCollectorAdHashKey;
			break;
		case STORAGE_AD:
			table = &StorageAds;
			func = makeStorageAdHashKey;
			break;
		case ACCOUNTING_AD:
			table = &AccountingAds;
			func = makeAccountingAdHashKey;
			break;
		case NEGOTIATOR_AD:
			table = &NegotiatorAds;
			func = makeStorageAdHashKey; // XXX
			break;
		case HAD_AD:
			table = &HadAds;
			func = makeHadAdHashKey;
			break;
        case GRID_AD:
			table = &GridAds;
			func = makeGridAdHashKey;
			break;
		default:
			return false;
	}

	return true;
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

	table.startIterations();
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
	return 1;
}
