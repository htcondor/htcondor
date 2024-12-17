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
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_email.h"
#include "condor_daemon_core.h"
#include "collector_engine.h"
#include "hashkey.h"

#include "condor_adtypes.h"
#include "condor_universe.h"
#include "ipv6_hostname.h"
#include "condor_threads.h"
#include "classad_helpers.h"

#include "condor_claimid_parser.h"
#include "authentication.h"

#include "collector.h"

#if defined(UNIX) && !defined(DARWIN)
#include "CollectorPlugin.h"
#endif

#include "ccb_server.h"

#ifdef TRACK_QUERIES_BY_SUBSYS
#include "subsystem_info.h" // so we can track query by client subsys
#endif

#include "dc_schedd.h"

using std::vector;
using std::string;

//----------------------------------------------------------------

extern "C" char* CondorVersion( void );
extern "C" char* CondorPlatform( void );

CollectorStats CollectorDaemon::collectorStats( false, 0 );
CollectorEngine CollectorDaemon::collector( &collectorStats );
int CollectorDaemon::HandleQueryInProcPolicy = HandleQueryInProcSmallTableAndQuery;
int CollectorDaemon::ClientTimeout;
int CollectorDaemon::QueryTimeout;
char* CollectorDaemon::CollectorName = NULL;
Timeslice CollectorDaemon::view_sock_timeslice;
vector<CollectorDaemon::vc_entry> CollectorDaemon::vc_list;
ConstraintHolder CollectorDaemon::vc_projection;

#if 0
ClassAd* CollectorDaemon::__query__;
int CollectorDaemon::__numAds__;
int CollectorDaemon::__resultLimit__;
int CollectorDaemon::__failed__;
List<CollectorRecord>* CollectorDaemon::__ClassAdResultList__;
std::string CollectorDaemon::__adType__;
ExprTree *CollectorDaemon::__filter__;

TrackTotals* CollectorDaemon::normalTotals = NULL;
int CollectorDaemon::submittorRunningJobs;
int CollectorDaemon::submittorIdleJobs;
int CollectorDaemon::submittorNumAds;

CollectorUniverseStats CollectorDaemon::ustatsAccum;

int CollectorDaemon::machinesTotal;
int CollectorDaemon::machinesUnclaimed;
int CollectorDaemon::machinesClaimed;
int CollectorDaemon::machinesOwner;
int CollectorDaemon::startdNumAds;
#endif
CollectorUniverseStats CollectorDaemon::ustatsMonthly;

ClassAd* CollectorDaemon::ad = NULL;
CollectorList* CollectorDaemon::collectorsToUpdate = NULL;
int CollectorDaemon::UpdateTimerId;

OfflineCollectorPlugin CollectorDaemon::offline_plugin_;

std::vector<std::string> viewCollectorTypes;

CCBServer *CollectorDaemon::m_ccb_server;
bool CollectorDaemon::filterAbsentAds;
bool CollectorDaemon::forwardClaimedPrivateAds = true;

std::queue<CollectorDaemon::pending_query_entry_t *> CollectorDaemon::query_queue_high_prio;
std::queue<CollectorDaemon::pending_query_entry_t *> CollectorDaemon::query_queue_low_prio;
int CollectorDaemon::ReaperId = -1;
int CollectorDaemon::max_query_workers = 4;
int CollectorDaemon::reserved_for_highprio_query_workers = 1;
int CollectorDaemon::max_pending_query_workers = 50;
int CollectorDaemon::max_query_worktime = 0;
int CollectorDaemon::active_query_workers = 0;
int CollectorDaemon::pending_query_workers = 0;

#ifdef TRACK_QUERIES_BY_SUBSYS
bool CollectorDaemon::want_track_queries_by_subsys = false;
#endif

AdTransforms CollectorDaemon::m_forward_ad_xfm;

//---------------------------------------------------------


struct TokenRequestContinuation {
	std::unique_ptr<DCSchedd> m_schedd;
	std::string m_peer_location;
	ReliSock *m_requester;

	static
	void finish(bool success, const std::string &token, const CondorError &err, void *misc_data)
	{
		auto continuation_ptr = static_cast<TokenRequestContinuation*>(misc_data);
		std::unique_ptr<TokenRequestContinuation> continuation(continuation_ptr);
		classad::ClassAd result_ad;
		if (!success) {
			result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
			result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
			if (!putClassAd(continuation->m_requester, result_ad) ||
				!continuation->m_requester->end_of_message())
			{
				dprintf(D_FULLDEBUG, "schedd_token_request: failed to send error"
					" response ad to client.\n");
			}
			return;
		}

		result_ad.InsertAttr(ATTR_SEC_TOKEN, token);
		dprintf(D_ALWAYS, "Token issued for user %s from %s for schedd %s.\n",
			continuation->m_requester->getFullyQualifiedUser(),
			continuation->m_peer_location.c_str(),
			continuation->m_schedd->addr());

		if (!putClassAd(continuation->m_requester, result_ad) ||
			!continuation->m_requester->end_of_message())
		{
			dprintf(D_FULLDEBUG, "schedd_token_request: failed to send token back "
				"to peer %s.\n",
				continuation->m_peer_location.c_str());
			return;
		}
		return;
	}
};


int
CollectorDaemon::schedd_token_request(int, Stream *stream)
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "schedd_token_request: failed to read input from client\n");
		return false;
	}

	int error_code = 0;
	std::string error_string;

	const char *fqu = static_cast<Sock*>(stream)->getFullyQualifiedUser();
	if (!fqu || !strlen(fqu)) {
		error_code = 1;
		error_string = "Missing requester identity.";
	}

	std::string authz_list_str;
	ad.EvaluateAttrString(ATTR_SEC_LIMIT_AUTHORIZATION, authz_list_str);
	std::vector<std::string> authz_bounding_set = split(authz_list_str);
	int requested_lifetime = -1;
	if (!ad.EvaluateAttrInt(ATTR_SEC_TOKEN_LIFETIME, requested_lifetime)) {
		requested_lifetime = -1;
	}

	if (!stream->get_encryption()) {
		error_code = 3;
		error_string = "Request to server was not encrypted.";
	}

		// Lookup schedd ad
	std::string schedd_name;
	if (!ad.EvaluateAttrString(ATTR_NAME, schedd_name)) {
		error_code = 4;
		error_string = "No schedd target specified.";
	}
	std::string capability, schedd_addr;
	if (!error_code && !collector.walkConcreteTable(SCHEDD_AD, [&](CollectorRecord *record) -> int {
			std::string local_schedd_name;
			if (!record || !record->m_publicAd ||
				!record->m_publicAd->EvaluateAttrString(ATTR_NAME, local_schedd_name) ||
				(schedd_name != local_schedd_name) ||
				!record->m_publicAd->EvaluateAttrString(ATTR_CAPABILITY, capability) ||
				!record->m_publicAd->EvaluateAttrString(ATTR_MY_ADDRESS, schedd_addr))
			{
				return 1;
			}
			return 0;
		}))
	{
		error_code = 4;
		error_string = "Failed to walk the schedd table.";
	}
	if (!error_code && schedd_addr.empty()) {
		error_code = 5;
		formatstr(error_string, "Schedd %s is not known to the collector.",
			schedd_name.c_str());
	}

	auto peer_location = static_cast<Sock*>(stream)->peer_ip_str();

	classad::ClassAd result_ad;
	classad::ClassAd request_ad;
	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
		result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
		// Bail out early if we had an error.
		if (!putClassAd(stream, result_ad) ||
			!stream->end_of_message())
		{
			dprintf(D_FULLDEBUG, "schedd_token_request: failed to send response ad to client.\n");
			return false;
		}
		return true;
	}

		// Install the capability for this session.
	ClaimIdParser cidp(capability.c_str());
	auto secman = daemonCore->getSecMan();
	secman->CreateNonNegotiatedSecuritySession(
		CLIENT_PERM,
		cidp.secSessionId(),
		cidp.secSessionKey(),
		cidp.secSessionInfo(),
		AUTH_METHOD_MATCH,
		SUBMIT_SIDE_MATCHSESSION_FQU,
		schedd_addr.c_str(),
		1200,
		nullptr, false
	);


	std::unique_ptr<DCSchedd> schedd(new DCSchedd(schedd_addr.c_str()));
	std::unique_ptr<TokenRequestContinuation> continuation(new TokenRequestContinuation());
	continuation->m_schedd = std::move(schedd);
	continuation->m_peer_location = peer_location;
	continuation->m_requester = static_cast<ReliSock*>(stream);

	CondorError err;
	if (!continuation->m_schedd->requestImpersonationTokenAsync(fqu, authz_bounding_set,
		requested_lifetime,
		static_cast<ImpersonationTokenCallbackType*>(&TokenRequestContinuation::finish),
		continuation.get(), err))
	{
		result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
		result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
		if (!putClassAd(stream, result_ad) || !stream->end_of_message())
		{
			dprintf(D_FULLDEBUG, "schedd_token_request: failed to send error response"
				" ad to client.\n");
		}
		return false;
	}
	continuation.release();
	return KEEP_STREAM;
}
 
//----------------------------------------------------------------

void CollectorDaemon::Init()
{
	dprintf(D_ALWAYS, "In CollectorDaemon::Init()\n");

	// read in various parameters from condor_config
	CollectorName=NULL;
	ad=NULL;
	UpdateTimerId=-1;
	collectorsToUpdate = NULL;
	Config();

	// install command handlers for queries
	daemonCore->Register_CommandWithPayload(QUERY_STARTD_ADS,"QUERY_STARTD_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_STARTD_PVT_ADS,"QUERY_STARTD_PVT_ADS",
		receive_query_cedar,"receive_query_cedar",NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_MASTER_ADS,"QUERY_MASTER_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_CKPT_SRVR_ADS,"QUERY_CKPT_SRVR_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_SUBMITTOR_ADS,"QUERY_SUBMITTOR_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_LICENSE_ADS,"QUERY_LICENSE_ADS",
	receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_COLLECTOR_ADS,"QUERY_COLLECTOR_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_STORAGE_ADS,"QUERY_STORAGE_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_ACCOUNTING_ADS,"QUERY_ACCOUNTING_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_NEGOTIATOR_ADS,"QUERY_NEGOTIATOR_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_HAD_ADS,"QUERY_HAD_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_ANY_ADS,"QUERY_ANY_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
    daemonCore->Register_CommandWithPayload(QUERY_GRID_ADS,"QUERY_GRID_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_GENERIC_ADS,"QUERY_GENERIC_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_MULTIPLE_ADS,"QUERY_MULTIPLE_ADS",
		receive_query_cedar,"receive_query_cedar",READ);
	daemonCore->Register_CommandWithPayload(QUERY_MULTIPLE_PVT_ADS,"QUERY_MULTIPLE_PVT_ADS",
		receive_query_cedar,"receive_query_cedar",NEGOTIATOR);

	// install command handlers for invalidations
	daemonCore->Register_CommandWithPayload(INVALIDATE_STARTD_ADS,"INVALIDATE_STARTD_ADS",
		receive_invalidation,"receive_invalidation",ADVERTISE_STARTD_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_SCHEDD_ADS,"INVALIDATE_SCHEDD_ADS",
		receive_invalidation,"receive_invalidation",ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_MASTER_ADS,"INVALIDATE_MASTER_ADS",
		receive_invalidation,"receive_invalidation",ADVERTISE_MASTER_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_CKPT_SRVR_ADS,
		"INVALIDATE_CKPT_SRVR_ADS", receive_invalidation,
		"receive_invalidation",DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_SUBMITTOR_ADS,
		"INVALIDATE_SUBMITTOR_ADS", receive_invalidation,
		"receive_invalidation",ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_LICENSE_ADS,
		"INVALIDATE_LICENSE_ADS", receive_invalidation,
		"receive_invalidation",DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_COLLECTOR_ADS,
		"INVALIDATE_COLLECTOR_ADS", receive_invalidation,
		"receive_invalidation",DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_STORAGE_ADS,
		"INVALIDATE_STORAGE_ADS", receive_invalidation,
		"receive_invalidation",DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_ACCOUNTING_ADS,
		"INVALIDATE_ACCOUNTING_ADS", receive_invalidation,
		"receive_invalidation",NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(INVALIDATE_NEGOTIATOR_ADS,
		"INVALIDATE_NEGOTIATOR_ADS", receive_invalidation,
		"receive_invalidation",NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(INVALIDATE_HAD_ADS,
		"INVALIDATE_HAD_ADS", receive_invalidation,
		"receive_invalidation",DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_ADS_GENERIC,
		"INVALIDATE_ADS_GENERIC", receive_invalidation,
		"receive_invalidation",DAEMON);
    daemonCore->Register_CommandWithPayload(INVALIDATE_GRID_ADS,
        "INVALIDATE_GRID_ADS", receive_invalidation,
		"receive_invalidation",DAEMON);

	// install command handlers for updates
	daemonCore->Register_CommandWithPayload(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
		receive_update,"receive_update",ADVERTISE_STARTD_PERM);
	daemonCore->Register_CommandWithPayload(MERGE_STARTD_AD,"MERGE_STARTD_AD",
		receive_update,"receive_update",NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(UPDATE_SCHEDD_AD,"UPDATE_SCHEDD_AD",
		receive_update,"receive_update",ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(UPDATE_SUBMITTOR_AD,"UPDATE_SUBMITTOR_AD",
		receive_update,"receive_update",ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(UPDATE_LICENSE_AD,"UPDATE_LICENSE_AD",
		receive_update,"receive_update",DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_MASTER_AD,"UPDATE_MASTER_AD",
		receive_update,"receive_update",ADVERTISE_MASTER_PERM);
	daemonCore->Register_CommandWithPayload(UPDATE_CKPT_SRVR_AD,"UPDATE_CKPT_SRVR_AD",
		receive_update,"receive_update",DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_COLLECTOR_AD,"UPDATE_COLLECTOR_AD",
		receive_update,"receive_update",DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_STORAGE_AD,"UPDATE_STORAGE_AD",
		receive_update,"receive_update",DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_NEGOTIATOR_AD,"UPDATE_NEGOTIATOR_AD",
		receive_update,"receive_update",NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(UPDATE_HAD_AD,"UPDATE_HAD_AD",
		receive_update,"receive_update",DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_AD_GENERIC, "UPDATE_AD_GENERIC",
		receive_update,"receive_update", DAEMON);
    daemonCore->Register_CommandWithPayload(UPDATE_GRID_AD,"UPDATE_GRID_AD",
		receive_update,"receive_update",DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_ACCOUNTING_AD,"UPDATE_ACCOUNTING_AD",
		receive_update,"receive_update",NEGOTIATOR);
	std::vector<DCpermission> allow_perms{ALLOW};
		// Users may advertise their own submitter ads.  If they do, there are additional
		// restrictions to their contents (such as the user must be authenticated, not
		// unmapped, and must match the Owner attribute).
	daemonCore->Register_CommandWithPayload(UPDATE_OWN_SUBMITTOR_AD,"UPDATE_OWN_SUBMITTOR_AD",
		receive_update,"receive_update", DAEMON, false,
		0, &allow_perms);
		//
	daemonCore->Register_CommandWithPayload(IMPERSONATION_TOKEN_REQUEST, "IMPERSONATION_TOKEN_REQUEST",
		schedd_token_request, "schedd_token_request", DAEMON,
		true, 0, &allow_perms);

    // install command handlers for updates with acknowledgement

    daemonCore->Register_CommandWithPayload(
		UPDATE_STARTD_AD_WITH_ACK,
		"UPDATE_STARTD_AD_WITH_ACK",
		receive_update_expect_ack,
		"receive_update_expect_ack",ADVERTISE_STARTD_PERM);

    // add all persisted ads back into the collector's store
    // process the given command
    int     insert = -3;
    ClassAd *tmpad;
    offline_plugin_.rewind ();
    while ( offline_plugin_.iterate ( tmpad ) ) {
		tmpad = new ClassAd(*tmpad);
	    if ( !collector.collect (UPDATE_STARTD_AD, tmpad, condor_sockaddr::null,
								 insert ) ) {
		    
            if ( -3 == insert ) {

                /* this happens when we get a classad for which a hash 
                key could not been made. This occurs when certain 
                attributes are needed for the particular category the
                ad is destined for, but they are not present in the 
                ad itself. */
			    dprintf (
                    D_ALWAYS,
				    "Received malformed ad. Ignoring.\n" );

	        }
			delete tmpad;
	    }

    }

	// add an exponential moving average counter of updates received.
	daemonCore->dc_stats.NewProbe("Collector", "UpdatesReceived", AS_COUNT | IS_CLS_SUM_EMA_RATE | IF_BASICPUB);

	// add a reaper for our query threads spawned off via Create_Thread
	if ( ReaperId == -1 ) {
		ReaperId = daemonCore->Register_Reaper("CollectorDaemon::QueryReaper",
						&CollectorDaemon::QueryReaper,
						"CollectorDaemon::QueryReaper()");
	}	
}

collector_runtime_probe HandleQuery_runtime;
collector_runtime_probe HandleLocate_runtime;
collector_runtime_probe HandleQueryForked_runtime;
collector_runtime_probe HandleQueryMissedFork_runtime;
collector_runtime_probe HandleLocateForked_runtime;
collector_runtime_probe HandleLocateMissedFork_runtime;


template <typename T>
class _condor_variable_auto_accum_runtime : public _condor_runtime
{
public:
	_condor_variable_auto_accum_runtime(T * store) : runtime(store) { }; // remember where to save result
	~_condor_variable_auto_accum_runtime() { (*runtime) += elapsed_runtime(); };
	T * runtime;
};

ExprTree * CollectorDaemon::get_query_filter(ClassAd* query, const std::string & attr, bool & skip_absent)
{
	// If ABSENT_REQUIREMENTS is defined, rewrite filter to filter-out absent ads
	// if ATTR_ABSENT is not alrady referenced in the query.
	// TODO: change this to get an attribute check in the scan function
	ExprTree * constraint = query->LookupExpr(attr);
	skip_absent = false;
	if ( filterAbsentAds ) {	// filterAbsentAds is true if ABSENT_REQUIREMENTS defined
		skip_absent = true;
		bool checks_absent = false;
		if (constraint) {
			classad::References machine_refs;  // machine attrs referenced by requirements
			GetReferences(attr.c_str(),*query,NULL,&machine_refs);
			checks_absent = machine_refs.count( ATTR_ABSENT );
		}
		if (checks_absent) {
			skip_absent = false;
		} else {
		#if 0 // this is now handled first class in the query scan callback
			std::string modified_filter;
			if (constraint) {
				formatstr(modified_filter, "(%s) && (%s =!= True)",
					ExprTreeToString(constraint),ATTR_ABSENT);
			} else {
				modified_filter = ATTR_ABSENT;
				modified_filter += " =!= True";
			}
			query->AssignExpr(attr,modified_filter.c_str());
			constraint = query->LookupExpr(attr);
			dprintf(D_FULLDEBUG,"Query after modification: *%s*\n",modified_filter.c_str());
		#endif
		}
	}
	return constraint;
}

CollectorDaemon::pending_query_entry_t *  CollectorDaemon::make_query_entry(
	AdTypes whichAds,
	ClassAd * query,
	bool allow_pvt)
{
	pending_query_entry_t *query_entry = nullptr;
	const char * label = AdTypeToString(whichAds);
	std::string target; // value of ATTR_TARGET_TYPE, which can be a list if the command is QUERY_MULTIPLE_ADS
	int num_adtypes = 1;
	size_t cbq, cbt;
	char * tagp;
	bool is_multi_query = false;

	if (whichAds == BOGUS_AD) {
		// This is a QUERY_MULTIPLE_ADS or QUERY_MULTIPLE_PVT_ADS query. so we look at ATTR_TARGET_TYPE
		// to determine the actual target type.  For this query type and ONLY this query, the target type can be a list
		if ( ! query->LookupString(ATTR_TARGET_TYPE, target) || target.empty()) {
			dprintf(D_ALWAYS,"Failed to find " ATTR_TARGET_TYPE " attribute in query ad of QUERY_MULTIPLE.\n");
			return nullptr;
		}
		label = "Multi";
		is_multi_query = true;
		StringTokenIterator it(target);
		const char * tag = it.first();
		if (tag) { whichAds = AdTypeStringToWhichAds(tag); }
		if (whichAds == NO_AD || whichAds == BOGUS_AD || ! collector.getHashTable(whichAds)) {
			dprintf(D_ALWAYS,"Invalid " ATTR_TARGET_TYPE "=\"%s\" in QUERY_MULTIPLE.\n", target.c_str());
			return nullptr;
		}
		num_adtypes = (int) std::distance(it.begin(), it.end());
	} else if (whichAds == GENERIC_AD) {
		if ( ! query->LookupString(ATTR_TARGET_TYPE, target) || target.empty()) {
			dprintf(D_ALWAYS,"Failed to find " ATTR_TARGET_TYPE " attribute in query ad of QUERY_GENERIC_ADS.\n");
			return nullptr;
		}
	} else if (whichAds == STARTD_AD) {
		// for QUERY_STARTD_ADS, we look at the target type to figure which startd ad table to use
		// if no target type, assume the slot ad table
		if (query->LookupString(ATTR_TARGET_TYPE, target)) {
			whichAds = get_realish_startd_adtype(target.c_str());
			//?? label = AdTypeToString(whichAds);
		}
	} else if (whichAds == STARTD_PVT_AD && ! allow_pvt) {
		dprintf(D_ALWAYS,"QUERY_STARTD_PVT_ADS not authorized.\n");
		return nullptr;
	}

	bool is_locate = query->Lookup(ATTR_LOCATION_QUERY) != NULL;
	if (is_locate) {
		// do startd ad locates in the daemon ad table
		if (whichAds == STARTD_AD) {
			whichAds = STARTDAEMON_AD;
			label = STARTD_DAEMON_ADTYPE;
		}
	}

	// malloc a query_entry struct.  we must use malloc here, not new, since
	// DaemonCore::Create_Thread requires a buffer created with malloc(), as it
	// will insist on calling free().
	// the pending query will consist of a pending_query_entry_t with an array of size num_adtypes
	// followed by a char buffer for adtype name strings
	cbq = pending_query_entry_t::size(num_adtypes);
	cbt = target.size() + 1;
	query_entry = (pending_query_entry_t *) malloc(cbq + cbt);
	tagp = ((char *) query_entry) + cbq;

	ASSERT(query_entry);
	memset(query_entry, 0, cbq+cbt);

	query_entry->label = label;
	query_entry->cad = query;
	query_entry->is_locate = is_locate;
	query_entry->is_multi = is_multi_query;

	bool skip_absent = false;
	ExprTree * constraint = get_query_filter(query, ATTR_REQUIREMENTS, skip_absent);

	int limit = INT_MAX; // no limit
	if ( ! query->LookupInteger(ATTR_LIMIT_RESULTS, limit) || limit <= 0) {
		limit = INT_MAX; // no limit
	}
	query_entry->limit = limit;

	// for the MULTI query, we allow a separate limit, contraint (and projection)
	// for each adtype.
	if (is_multi_query) {
		query_entry->num_adtypes = 0;
		for (auto & tag : StringTokenIterator(target)) {
			int ix = query_entry->num_adtypes;
			AdTypes adtype = AdTypeStringToWhichAds(tag.c_str());
			if (adtype == STARTD_PVT_AD) {
				if ( ! allow_pvt) {
					dprintf(D_ALWAYS,"Query of STARTD_PVT_ADS not authorized.\n");
					free(query_entry);
					return nullptr;
				}
			#if 1 // future
			#else
				if (ix > 0 && (query_entry->adt[ix].whichAds == STARTD_AD ||
					           query_entry->adt[ix].whichAds == SLOT_AD)) {
					// if this is a multi-query and the previous ad was a slot ad
					// promote that have the private attributes and don't
					// make an entry in the query for this adtype
					query_entry->adt[ix].pvt_ad = allow_pvt;
					continue;
				}
			#endif
			}

			// store the tag in the tags buffer, and set the pointer to it.
			query_entry->adt[ix].tag = tagp;
			strncpy(tagp, tag.c_str(), cbt);
			tagp += tag.size()+1;
			cbt -= tag.size()+1;

			if (adtype == NO_AD || ! collector.getHashTable(adtype)) {
				// assume generic if we don't recognise the adtype
				query_entry->adt[ix].whichAds = GENERIC_AD;
			} else {
				query_entry->adt[ix].whichAds = adtype;
			}
			query_entry->adt[ix].limit = limit;
			query_entry->adt[ix].constraint = constraint;
			query_entry->adt[ix].skip_absent = skip_absent;

			// check for <AdType>Limit
			std::string attr(tag); attr += ATTR_LIMIT_RESULTS;
			int taglimit = -1;
			if (query->LookupInteger(attr, taglimit) && taglimit > 0) {
				query_entry->adt[ix].limit = taglimit;
			}

			// check for <AdType>Requirements
			attr = tag; attr += ATTR_REQUIREMENTS;
			if (query->Lookup(attr)) {
				bool tag_skip_absent = skip_absent;
				query_entry->adt[ix].constraint = get_query_filter(query, attr, tag_skip_absent);
				query_entry->adt[ix].skip_absent = tag_skip_absent;
			}
			query_entry->num_adtypes += 1;
		}

	} else {
		// regular (non-multi) query
		query_entry->num_adtypes = 1;
		query_entry->adt[0].whichAds = whichAds;
		query_entry->adt[0].limit = limit;
		query_entry->adt[0].constraint = constraint;
		query_entry->adt[0].skip_absent = skip_absent;
		query_entry->adt[0].tag = label;
		if ( ! target.empty()) {
			// this needs to be the target when the table is GENERIC
			query_entry->adt[0].tag = tagp;
			strncpy(tagp, target.c_str(), cbt);
		}
	}

	return query_entry;
}


int CollectorDaemon::receive_query_cedar(int command,
										 Stream* sock)
{
	int return_status = TRUE;
	pending_query_entry_t *query_entry = NULL;
	bool handle_in_proc;
	bool is_locate;
	bool negotiate_auth = false;
	const unsigned int BIG_TABLE_MASK = (1<<GENERIC_AD) | (1<<ANY_AD) | (1<<MASTER_AD) | (1<<STARTDAEMON_AD)
	                                  | (1<<STARTD_AD) | (1<<STARTD_PVT_AD) | (1<<SLOT_AD) | (1<<BOGUS_AD);
	KnownSubsystemId clientSubsys = SUBSYSTEM_ID_UNKNOWN;
	AdTypes whichAds;
	ClassAd *cad = new ClassAd();
	ASSERT(cad);

	_condor_variable_auto_accum_runtime<collector_runtime_probe> rt(&HandleQuery_runtime);
	//double rt_last = rt.begin;

	sock->decode();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	bool ep = CondorThreads::enable_parallel(true);
	bool res = !getClassAd(sock, *cad) || !sock->end_of_message();
	CondorThreads::enable_parallel(ep);
    if( res )
    {
        dprintf(D_ALWAYS,"Failed to receive query on TCP: aborting\n");
		return_status = FALSE;
		goto END;
    }

	// Initial query handler
	whichAds = receive_query_public( command );
	negotiate_auth = (command == QUERY_STARTD_PVT_ADS || command == QUERY_MULTIPLE_PVT_ADS);
	query_entry = make_query_entry(whichAds, cad, negotiate_auth);
	if ( ! query_entry) {
		// expect make_query_entry to log the reason for the failure
		return_status = FALSE;
		goto END;
	}
	query_entry->sock = sock;
	is_locate = query_entry->is_locate;
	if (is_locate) { rt.runtime = &HandleLocate_runtime; }

	// Figure out whether to handle the query inline or to fork.
	handle_in_proc = false;
	if (HandleQueryInProcPolicy == HandleQueryInProcAlways) {
		handle_in_proc = true;
	} else if (HandleQueryInProcPolicy == HandleQueryInProcNever) {
		handle_in_proc = false;
	} else {
		bool is_bigtable = ((1<<whichAds) & BIG_TABLE_MASK) != 0;
		if (HandleQueryInProcPolicy == HandleQueryInProcSmallTable) {
			handle_in_proc = !is_bigtable;
		} else {
			bool small_query = is_locate;
			if ( ! is_locate) {
				long long result_limit = 0;
				bool has_limit = cad->EvaluateAttrInt(ATTR_LIMIT_RESULTS, result_limit);
				bool has_projection = cad->Lookup(ATTR_PROJECTION);
				small_query = has_projection && has_limit && (result_limit < 10);
			}
			switch (HandleQueryInProcPolicy) {
				case HandleQueryInProcSmallQuery: handle_in_proc = small_query; break;
				case HandleQueryInProcSmallTableAndQuery: handle_in_proc = !is_bigtable && small_query; break;
				case HandleQueryInProcSmallTableOrQuery: handle_in_proc = !is_bigtable || small_query; break;
			}
		}
	}
	// If we are not allowed any forked query workers, i guess we are going in-proc
	if ( max_query_workers < 1 ) {
		handle_in_proc = true;
	}

	// Set a deadline on the query socket if the admin specified one in the config,
	// but if the socket came to us with a previous (shorter) deadline, honor it.
	if ( max_query_worktime > 0 ) {   // max_query_worktime came from config file
		time_t new_deadline = time(NULL) + max_query_worktime;
		time_t sock_deadline = sock->get_deadline();
		if ( sock_deadline > 0 ) {
			new_deadline = MIN(new_deadline,sock_deadline);
		}
		sock->set_deadline(new_deadline);
		// dprintf(D_FULLDEBUG,"QueryWorker old sock_deadline = %d, now new_deadline = %d\n",sock_deadline,new_deadline);
	}

#ifdef TRACK_QUERIES_BY_SUBSYS
	if ( want_track_queries_by_subsys ) {
		std::string subsys;
		const std::string &sess_id = static_cast<Sock *>(sock)->getSessionID();
		daemonCore->getSecMan()->getSessionStringAttribute(sess_id.c_str(),ATTR_SEC_SUBSYSTEM,subsys);
		if ( ! subsys.empty()) {
			clientSubsys = getKnownSubsysNum(subsys.c_str());
			strncpy(query_entry->subsys, subsys.c_str(), COUNTOF(query_entry->subsys));
			query_entry->subsys[COUNTOF(query_entry->subsys)-1] = 0;
		}
	}
#endif

	// Now we are ready to either invoke a worker thread directly to handle the query,
	// or enqueue a request to run the worker thread later.
	if ( handle_in_proc ) {
		// We want to immediately handle the query inline in this process.
		// So in this case, we simply directly invoke our worker thread function.
		dprintf(D_FULLDEBUG,"QueryWorker: about to handle query in-process\n");
		return_status = receive_query_cedar_worker_thread((void *)query_entry,sock);
	} else {
		// Enqueue the query to ultimately run in a forked process created created with
		// DaemonCore::Create_Thread().  
		// We add the pending query entry to a queue, and then directly invoke the worker 
		// thread reaper, as the reaper handles popping entries off the queue and invoking
		// Create_Thread once where are enough worker slots.
		// Note in this case, we must return KEEP_STEAM so the socket to our client is not
		// closed before we handle the request; it will be closed by the reaper ( in parent process)
		// and/or by daemoncore (in child process).  Either way, don't close it upon exit from this
		// command handler.
		int did_we_fork = FALSE;

		// We want to add a query request into the queue.
		// Decide if it should go into the high priority or low priorirty queue
		// based upon the Subsystem attribute in the session for this connection;
		// if the request is from the NEGOTIATOR, it is high priority.
		// Also high priority if command is from the superuser (i.e. via condor_sos).
		bool high_prio_query = false;
		if ( clientSubsys == SUBSYSTEM_ID_UNKNOWN ) {
			// If we have not yet determined the subsystem, we must do that now.
			std::string subsys;
			const std::string &sess_id = static_cast<Sock *>(sock)->getSessionID();
			daemonCore->getSecMan()->getSessionStringAttribute(sess_id.c_str(),ATTR_SEC_SUBSYSTEM,subsys);
			if ( ! subsys.empty()) {
				clientSubsys = getKnownSubsysNum(subsys.c_str());
				strncpy(query_entry->subsys, subsys.c_str(), COUNTOF(query_entry->subsys));
				query_entry->subsys[COUNTOF(query_entry->subsys)-1] = 0;
			}
		}
		// TODO Giving high-priority to queries from a collector is a hack
		//   to work around the fact that queries from the negotiator get
		//   flagged is coming from a collector when the family security
		//   session is used.
		//   Better identification of the peer when a non-negotiated
		//   security session is used is the real solution.
		if ( clientSubsys == SUBSYSTEM_ID_NEGOTIATOR || clientSubsys == SUBSYSTEM_ID_COLLECTOR || daemonCore->Is_Command_From_SuperUser(sock) )
		{
			high_prio_query = true;
		}

		// Now that we know if the incoming query is high priority or not,
		// place it into the proper queue if we don't already have too many pending.
		if ( ((high_prio_query==false) &&
			  (active_query_workers + pending_query_workers <  max_query_workers + max_pending_query_workers - reserved_for_highprio_query_workers))
			 ||
			 ((high_prio_query==true) &&
			  (active_query_workers - reserved_for_highprio_query_workers + (int)query_queue_high_prio.size() <  max_query_workers + max_pending_query_workers))
		   )
		{
			if ( high_prio_query ) {
				query_queue_high_prio.push( query_entry );
			} else {
				query_queue_low_prio.push( query_entry );
			}
			did_we_fork = QueryReaper(-1, -1);
			cad = NULL; // set this to NULL so we won't delete it below; our reaper will remove it
			query_entry = NULL; // set this to NULL so we won't free it below; daemoncore will remove it
			return_status = KEEP_STREAM; // tell daemoncore to not mess with socket when we return
		} else {
			dprintf( D_ALWAYS, 
				"QueryWorker: dropping %s priority query request due to max pending workers of %d ( max %d reserved %d active %d pending %d )\n", 
				high_prio_query ? "high" : "low",
				max_pending_query_workers, max_query_workers, reserved_for_highprio_query_workers, active_query_workers, pending_query_workers );
			collectorStats.global.DroppedQueries += 1;
		}

		// Update a few statistics
		if ( !daemonCore->DoFakeCreateThread() ) {  // if we are configured to really fork()...
			if (did_we_fork == TRUE) {
				// A new worker was forked off
				if (is_locate) { rt.runtime = &HandleLocateForked_runtime; } else { rt.runtime = &HandleQueryForked_runtime; }
			} else {
				// Did not yet fork off a new worker cuz we are at a limit
				if (is_locate) { rt.runtime = &HandleLocateMissedFork_runtime; } else { rt.runtime = &HandleQueryMissedFork_runtime; }
			}
		}
	}

#ifdef TRACK_QUERIES_BY_SUBSYS
	if ( want_track_queries_by_subsys ) {
		// count the number of queries for each client subsystem.
		if (clientSubsys >= 0 && clientSubsys < SUBSYSTEM_ID_COUNT) {
			if (handle_in_proc) {
				collectorStats.global.InProcQueriesFrom[clientSubsys] += 1;
			} else {
				collectorStats.global.ForkQueriesFrom[clientSubsys] += 1;
			}
		}
	}
#endif

END:
    // all done
	delete cad;
	free(query_entry);
	return return_status;
}


// Return 1 if forked a worker, 0 if not, and -1 upon an error.
int CollectorDaemon::QueryReaper(int pid, int /* exit_status */ )
{
	if ( pid >= 0 ) {
		dprintf(D_FULLDEBUG,
			"QueryWorker: Child %d done\n", pid);
		if (active_query_workers > 0 ) {
			active_query_workers--;
		}
		collectorStats.global.ActiveQueryWorkers = active_query_workers;
	}

	// Grab a queue_entry to service, ignoring "stale" (old) entries.
	bool high_prio_query;
	pending_query_entry_t * query_entry = NULL;	
	while ( query_entry == NULL ) {
		// Pull of an entry from our high_prio queue; if nothing there, grab
		// one from our low prio queue.  Ignore "stale" (old) requests.

		high_prio_query = query_queue_high_prio.size() > 0;

		// Dequeue a high priority entry if worker slots available.
		// If high priority queue is empty, dequeue a low priority entry
		// if a worker slot (minus those reserved only for high prioirty) is available.
		if ( active_query_workers < max_query_workers ) {
			if ( !query_queue_high_prio.empty() ) {
				query_entry = query_queue_high_prio.front();
				query_queue_high_prio.pop();
			} else if ( !query_queue_low_prio.empty() && active_query_workers < (max_query_workers - reserved_for_highprio_query_workers) ) {
				query_entry = query_queue_low_prio.front();
				query_queue_low_prio.pop();
			}
		}

		// Update our pending stats counters.  Note we need to do this regardless
		// of if query_entry==NULL, since we may be here because something was either
		// recently added into the queue, or recently removed from the queue.
		pending_query_workers = query_queue_high_prio.size() + query_queue_low_prio.size();
		collectorStats.global.PendingQueries = pending_query_workers;

		// If query_entry==NULL, we are not forking anything now, so we're done for now
		if ( query_entry == NULL ) {
			// If we are not forking because we hit fork limits, dprintf
			if ( pending_query_workers > 0  ) {
				dprintf( D_ALWAYS,
					"QueryWorker: delay forking %s priority query because too many workers ( max %d reserved %d active %d pending %d ) \n",
					high_prio_query ? "high" : "low",
					max_query_workers, reserved_for_highprio_query_workers, active_query_workers, pending_query_workers );
			}
			return 0;
		}

		// If we are here because a fork worker just exited (pid >= 0), or
		// if there are still more pending queries in the queue, then it is possible
		// that the query_entry we are about to service has been sitting around
		// in the queue for some time.  So we need to check if it is "stale"
		// before we spend time forking.
		if ( pid >= 0 || pending_query_workers > 0 ) {
			// Consider a query_request to be stale if
			//   a) our deadline on the socket has expired, or
			//   b) the client has closed the TCP socket.
			// Note that we can figure out (b) by asking CEDAR if
			// the socket is "readReady()", which will return true if
			// either the socket is closed or the socket has data waiting to be read.
			// Since we know there should be nothing more to read on this socket (the
			// client should now be awaiting our reponse), if readReady() return true
			// we can safely assume that TCP thinks the socket has been closed.
			if ( query_entry->sock->deadline_expired() ||
				static_cast<Sock *>(query_entry->sock)->readReady() )
			{
				dprintf( D_ALWAYS, 
					"QueryWorker: dropping stale query request because %s ( max %d active %d pending %d )\n", 
					query_entry->sock->deadline_expired() ? "max worktime expired" : "client gone",
					max_query_workers, active_query_workers, pending_query_workers );
				collectorStats.global.DroppedQueries += 1;				
				// now deallocate everything with this query_entry
				delete query_entry->sock;
				delete query_entry->cad;
				free(query_entry); 
				query_entry = NULL;  // so we will loop and dequeue another entry
			}
		}
	}  // end of while queue_entry == NULL

	// If we have made it here, we are allowed to fork another worker
	// to handle the query represented by query_entry. Fork one!
	// First stash a copy of query_entry->sock and query_entry->cad so 
	// we can deallocate the memory associated with these after a succesfull 
	// call to Create_Thread - we need to stash them away because DaemonCore will
	// free the query_entry struct itself.
	Stream *sock = query_entry->sock;
	query_entry->sock = NULL;
	ClassAd *query_classad = query_entry->cad;
	int tid = daemonCore->
		Create_Thread((ThreadStartFunc)&CollectorDaemon::receive_query_cedar_worker_thread,
		    (void *)query_entry, sock, ReaperId);
	if (tid == FALSE) {
		dprintf(D_ALWAYS,
				"ERROR: Create_Thread failed trying to fork a QueryWorker!\n");
		free(query_entry); // daemoncore won't free this if Create_Thread fails
		delete sock;
		delete query_classad;
		return -1;
	}

	// If we made it here, we forked off another worker. 

	// Increment our count of active workers
	active_query_workers++;
	collectorStats.global.ActiveQueryWorkers = active_query_workers;

	// Also close query_entry->sock since DaemonCore
	// will have cloned this socket for the child, and we have no need to write anything
	// out here in the parent once the child finishes.
	delete sock;
	sock = NULL;

	// Also deallocate the memory associated with the stashed in query_classad.
	// It is safe to do this now, because if Create_Thread already forked the child
	// now has a copy, or the Create_Thread ran in-proc it has already completed.
	delete query_classad;
	query_classad = NULL;

	dprintf(D_ALWAYS,
			"QueryWorker: forked new %sworker with id %d ( max %d active %d pending %d )\n",
			high_prio_query ? "high priority " : "", tid,
			max_query_workers, active_query_workers, pending_query_workers);

	return 1;
}


int CollectorDaemon::receive_query_cedar_worker_thread(void *in_query_entry, Stream* sock)
{
	int return_status = TRUE;
	_condor_runtime runtime;
	double tick_time = runtime.begin;
	double query_time = 0;
	double send_time = 0;

	// Pull out relavent state from query_entry
	pending_query_entry_t *query_entry = (pending_query_entry_t *) in_query_entry;
	ClassAd *query = query_entry->cad;
	bool is_locate = query_entry->is_locate;
	bool wants_pvt_attrs = false;
	int num_adtypes = (query_entry->num_adtypes > 0) ? query_entry->num_adtypes : 1;
	std::deque<CollectorRecord*> results;

	query->LookupBool(ATTR_SEND_PRIVATE_ATTRIBUTES, wants_pvt_attrs);

		// If our peer is at least 8.9.3 and has NEGOTIATOR authz, then we'll
		// trust it to handle our capabilities.
		// Starting with 10.0.0, send the private attributes only if the
		// client requests them.
	bool filter_private_attrs = true;
	auto *verinfo = sock->get_peer_version();
	if (verinfo && verinfo->built_since_version(8, 9, 3) &&
		(USER_AUTH_SUCCESS == daemonCore->Verify("send private attrs", NEGOTIATOR, *static_cast<ReliSock*>(sock), D_SECURITY|D_FULLDEBUG)))
	{
		if (verinfo->built_since_version(10, 0, 0)) {
			filter_private_attrs = !wants_pvt_attrs;
		} else {
			filter_private_attrs = false;
		}
	}

		// If our peer has ADMINISTRATOR authz and explicitly asks for
		// private attributes, then we'll trust it to handle our capabilities
	if (wants_pvt_attrs &&
		(USER_AUTH_SUCCESS == daemonCore->Verify("send private attrs", ADMINISTRATOR, *static_cast<ReliSock*>(sock), D_SECURITY|D_FULLDEBUG)))
	{
		dprintf(D_SECURITY|D_FULLDEBUG, "Administrator requesting private attributes - will not filter.\n");
		filter_private_attrs = false;
	}

	// See if query ad asks for server-side projection
	std::string projection;
	std::string attr_projection(ATTR_PROJECTION);
	// turn projection string into a set of attributes
	classad::References proj, tagproj;
	bool proj_is_expr = false;
	if (query->LookupString(attr_projection, projection) && ! projection.empty()) {
		StringTokenIterator list(projection);
		const std::string * attr;
		while ((attr = list.next_string())) { proj.insert(*attr); }
	} else if (query->Lookup(attr_projection)) {
		// if projection is not a simple string, then assume that evaluating it as a string in the context of the ad will work better
		// (the negotiator sends this sort of projection)
		proj_is_expr = true;
	}

	// Perform the query
	CollectorDaemon::collect_op op;
	bool sending = false;
	int more = 1;

	for (int ix = 0; ix < num_adtypes; ++ix) {
		const AdTypes whichAds = (AdTypes) query_entry->adt[ix].whichAds;

		op.__filter__ = query_entry->adt[ix].constraint;
		op.__skip_absent__ = query_entry->adt[ix].skip_absent;
		op.__mytype__ = (query_entry->adt[ix].match_mytype) ? query_entry->adt[ix].tag : nullptr;
		op.__resultLimit__ = MIN(query_entry->limit, query_entry->adt[ix].limit) - op.__numAds__;
		op.__results__ = &results;
		results.clear();

		CollectorHashTable * table = nullptr;
		if (whichAds == GENERIC_AD) {
			table = collector.getGenericHashTable(query_entry->adt[ix].tag);
		} else if (whichAds != ANY_AD) {
			table = collector.getHashTable(whichAds);
		}
		if (table) {
			collector.walkHashTable (*table,
				[&op](CollectorRecord*cr){
					return op.query_scanFunc(cr);
				});
		} else if (ix==0 && whichAds == ANY_AD) {
			std::vector<CollectorHashTable *> tables = collector.getAnyHashTables(op.__mytype__);
			for (auto table : tables) {
				collector.walkHashTable (*table,
					[&op](CollectorRecord*cr){
						return op.query_scanFunc(cr);
					});
				if (op.__numAds__ >= op.__resultLimit__)
					break;
			}
			num_adtypes = 1; // don't allow Any as part of a multi-table scan.
		} else {
			dprintf (D_ALWAYS, "Error no collector table for %s\n", query_entry->adt[ix].tag);
			continue;
		}

		query_time += runtime.tick(tick_time);

		if (results.empty())
			continue;

		// do first time initialization for sending ads
		if ( ! sending) {
			// send the results via cedar
			sock->timeout(QueryTimeout); // set up a network timeout of a longer duration
			sock->encode();
			sending = true;
		}
		if (whichAds == STARTD_PVT_AD) { filter_private_attrs = false; }

		attr_projection = ATTR_PROJECTION;
		bool evaluate_projection = proj_is_expr;
		classad::References * active_proj = &proj;
		classad::References * whitelist = proj.empty() ? nullptr : &proj;
		if (query_entry->is_multi) {
			std::string tagattr(query_entry->adt[ix].tag); tagattr += ATTR_PROJECTION;
			if (query->LookupString(tagattr, projection)) {
				tagproj.clear();
				StringTokenIterator list(projection);
				const std::string * attr;
				while ((attr = list.next_string())) { tagproj.insert(*attr); }
				whitelist = tagproj.empty() ? nullptr : &tagproj;
				evaluate_projection = false;
			} else if (query->Lookup(tagattr)) {
				attr_projection = tagattr;
				active_proj = &tagproj;
				evaluate_projection = true;
			}
		}

		for (CollectorRecord* curr_rec : results)
		{
			ClassAd* ad_to_send = filter_private_attrs ? curr_rec->m_publicAd : curr_rec->m_pvtAd;
			// if querying collector ads, and the collectors own ad appears in this list.
			// then we want to shove in current statistics. we do this by chaining a
			// temporary stats ad into the ad to be returned, and publishing updated
			// statistics into the stats ad.  we do this because if the verbosity level
			// is increased we do NOT want to put the high-verbosity attributes into
			// our persistent collector ad.
			ClassAd * stats_ad = NULL;
			if ((whichAds == COLLECTOR_AD) && collector.isSelfAd(curr_rec)) {
				dprintf(D_ALWAYS,"Query includes collector's self ad\n");
				// update stats in the collector ad before we return it.
				std::string stats_config;
				query->LookupString("STATISTICS_TO_PUBLISH",stats_config);
				if (stats_config != "stored") {
					dprintf(D_ALWAYS,"Updating collector stats using a chained ad and config=%s\n", stats_config.c_str());
					stats_ad = new ClassAd();
					if (!filter_private_attrs) {
						stats_ad->CopyFrom(*curr_rec->m_pvtAd);
					}
					daemonCore->dc_stats.Publish(*stats_ad, stats_config.c_str());
					daemonCore->monitor_data.ExportData(stats_ad, true);
					collectorStats.publishGlobal(stats_ad, stats_config.c_str());
					stats_ad->ChainToAd(curr_rec->m_publicAd);
					ad_to_send = stats_ad; // send the stats ad instead of the self ad.
				}
			}

			if (evaluate_projection) {
				active_proj->clear();
				projection.clear();
				if (EvalString(attr_projection.c_str(), query, curr_rec->m_publicAd, projection) && ! projection.empty()) {
					StringTokenIterator list(projection);
					const std::string * attr;
					while ((attr = list.next_string())) { active_proj->insert(*attr); }
				}
				whitelist = active_proj->empty() ? nullptr : active_proj;
			}

			bool send_failed = (!sock->code(more) || !putClassAd(sock, *ad_to_send, 0, whitelist));

			if (stats_ad) {
				stats_ad->Unchain();
				delete stats_ad;
			}

			if (send_failed)
			{
				dprintf (D_ALWAYS,
						"Error sending query result to client -- aborting\n");
				return_status = 0;
				goto END;
			}

			if (sock->deadline_expired()) {
				dprintf( D_ALWAYS,
					"QueryWorker: max_worktime expired while sending query result to client -- aborting\n");
				return_status = 0;
				goto END;
			}

		} // end of for : results

		send_time += runtime.tick(tick_time);
		results.clear();
	}

	if ( ! sending) {
		sock->encode();
		sending = true;
	}

	// end of query response ...
	more = 0;
	if (!sock->code(more))
	{
		dprintf (D_ALWAYS, "Error sending EndOfResponse (0) to client\n");
	}

	// flush the output
	if (!sock->end_of_message())
	{
		dprintf (D_ALWAYS, "Error flushing CEDAR socket\n");
	}

	send_time += runtime.tick(tick_time);

	dprintf (D_ALWAYS,
			 "Query info: matched=%d; skipped=%d; query_time=%f; send_time=%f; type=%s; requirements={%s}; locate=%d; limit=%d; from=%s; peer=%s; projection={%s}; filter_private_attrs=%d\n",
			 op.__numAds__,
			 op.__failed__ + op.__absent__,
			 query_time,
			 send_time,
			 query_entry->label ? query_entry->label : "?",
			 op.__filter__ ? ExprTreeToString(op.__filter__) : "",
			 is_locate,
			 (op.__resultLimit__ == INT_MAX) ? 0 : op.__resultLimit__,
			 query_entry->subsys,
			 sock->peer_description(),
			 projection.c_str(),
			 filter_private_attrs);
END:
	
	// All done.  Deallocate memory allocated in this method.  Note that DaemonCore 
	// will supposedly free() the query_entry struct itself and also delete sock.

	return return_status;
}

AdTypes
CollectorDaemon::receive_query_public( int command )
{
	AdTypes whichAds;

    switch (command)
    {
	  case QUERY_STARTD_ADS:
		dprintf (D_ALWAYS, "Got QUERY_STARTD_ADS\n");
		whichAds = STARTD_AD;
		break;
		
	  case QUERY_SCHEDD_ADS:
		dprintf (D_ALWAYS, "Got QUERY_SCHEDD_ADS\n");
		whichAds = SCHEDD_AD;
		break;

	  case QUERY_SUBMITTOR_ADS:
		dprintf (D_ALWAYS, "Got QUERY_SUBMITTOR_ADS\n");
		whichAds = SUBMITTOR_AD;
		break;

	  case QUERY_LICENSE_ADS:
		dprintf (D_ALWAYS, "Got QUERY_LICENSE_ADS\n");
		whichAds = LICENSE_AD;
		break;

	  case QUERY_MASTER_ADS:
		dprintf (D_ALWAYS, "Got QUERY_MASTER_ADS\n");
		whichAds = MASTER_AD;
		break;
		
	  case QUERY_CKPT_SRVR_ADS:
		dprintf (D_ALWAYS, "Got QUERY_CKPT_SRVR_ADS\n");
		whichAds = CKPT_SRVR_AD;	
		break;
		
	  case QUERY_STARTD_PVT_ADS:
		dprintf (D_ALWAYS, "Got QUERY_STARTD_PVT_ADS\n");
		whichAds = STARTD_PVT_AD;
		break;

	  case QUERY_COLLECTOR_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_COLLECTOR_ADS\n");
		whichAds = COLLECTOR_AD;
		break;

	  case QUERY_STORAGE_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_STORAGE_ADS\n");
		whichAds = STORAGE_AD;
		break;

	  case QUERY_ACCOUNTING_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_ACCOUNTING_ADS\n");
		whichAds = ACCOUNTING_AD;
		break;

	  case QUERY_NEGOTIATOR_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_NEGOTIATOR_ADS\n");
		whichAds = NEGOTIATOR_AD;
		break;

	  case QUERY_MULTIPLE_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_MULTIPLE_ADS\n");
		whichAds = BOGUS_AD;
		break;

	  case QUERY_MULTIPLE_PVT_ADS:
		dprintf (D_ALWAYS,"Got QUERY_MULTIPLE_PVT_ADS\n");
		whichAds = BOGUS_AD;
		break;

	  case QUERY_HAD_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_HAD_ADS\n");
		whichAds = HAD_AD;
		break;

	  case QUERY_GENERIC_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_GENERIC_ADS\n");
		whichAds = GENERIC_AD;
		break;

	  case QUERY_ANY_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_ANY_ADS\n");
		whichAds = ANY_AD;
		break;
      
      case QUERY_GRID_ADS:
        dprintf (D_FULLDEBUG,"Got QUERY_GRID_ADS\n");
        whichAds = GRID_AD;
		break;

	  default:
		dprintf(D_ALWAYS,
				"Unknown command %d in receive_query_public()\n",
				command);
		whichAds = (AdTypes) -1;
    }

	return whichAds;
}

int CollectorDaemon::receive_invalidation(int command,
										  Stream* sock)
{
	AdTypes whichAds;
	ClassAd cad;

	sock->decode();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

    if( !getClassAd(sock, cad) || !sock->end_of_message() )
    {
        dprintf( D_ALWAYS,
				 "Failed to receive invalidation on %s: aborting\n",
				 sock->type() == Stream::reli_sock ? "TCP" : "UDP" );
        return FALSE;
    }

    // cancel timeout --- collector engine sets up its own timeout for
    // collecting further information
    sock->timeout(0);

    switch (command)
    {
	  case INVALIDATE_STARTD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_STARTD_ADS\n");
		whichAds = STARTD_AD;
		break;
		
	  case INVALIDATE_SCHEDD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_SCHEDD_ADS\n");
		whichAds = SCHEDD_AD;
		break;
		
	  case INVALIDATE_SUBMITTOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_SUBMITTOR_ADS\n");
		whichAds = SUBMITTOR_AD;
		break;

	  case INVALIDATE_LICENSE_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_LICENSE_ADS\n");
		whichAds = LICENSE_AD;
		break;

	  case INVALIDATE_MASTER_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_MASTER_ADS\n");
		whichAds = MASTER_AD;
		break;
		
	  case INVALIDATE_CKPT_SRVR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_CKPT_SRVR_ADS\n");
		whichAds = CKPT_SRVR_AD;	
		break;
		
	  case INVALIDATE_COLLECTOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_COLLECTOR_ADS\n");
		whichAds = COLLECTOR_AD;
		break;

	  case INVALIDATE_NEGOTIATOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_NEGOTIATOR_ADS\n");
		whichAds = NEGOTIATOR_AD;
		break;

	  case INVALIDATE_HAD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_HAD_ADS\n");
		whichAds = HAD_AD;
		break;

	  case INVALIDATE_STORAGE_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_STORAGE_ADS\n");
		whichAds = STORAGE_AD;
		break;

	  case INVALIDATE_ACCOUNTING_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_ACCOUNTING_ADS\n");
		whichAds = ACCOUNTING_AD;
		break;

	  case INVALIDATE_ADS_GENERIC:
		dprintf(D_ALWAYS, "Got INVALIDATE_ADS_GENERIC\n");
		whichAds = GENERIC_AD;
		break;

      case INVALIDATE_GRID_ADS:
        dprintf(D_ALWAYS, "Got INVALIDATE_GRID_ADS\n");
        whichAds = GRID_AD;
		break;

	  default:
		dprintf(D_ALWAYS,"Unknown command %d in receive_invalidation()\n",
			command);
		whichAds = (AdTypes) -1;
    }

	collect_op op;
    if (whichAds != (AdTypes) -1)
		op.process_invalidation (whichAds, cad, sock);

    /* let the off-line plug-in invalidate the given ad */
    offline_plugin_.invalidate ( command, cad );

#if defined(UNIX) && !defined(DARWIN)
	CollectorPluginManager::Invalidate(command, cad);
#endif

	if (!viewCollectorTypes.empty() || command == INVALIDATE_STARTD_ADS || command == INVALIDATE_SUBMITTOR_ADS) {
		forward_classad_to_view_collector(command,
										  ATTR_TARGET_TYPE,
										  &cad);
    }

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		return stashSocket( (ReliSock *)sock );
	}

    // all done; let daemon core will clean up connection
	return TRUE;
}


collector_runtime_probe CollectorEngine_receive_update_runtime;
#ifdef PROFILE_RECEIVE_UPDATE
collector_runtime_probe CollectorEngine_ru_pre_collect_runtime;
collector_runtime_probe CollectorEngine_ru_collect_runtime;
collector_runtime_probe CollectorEngine_ru_plugins_runtime;
collector_runtime_probe CollectorEngine_ru_forward_runtime;
collector_runtime_probe CollectorEngine_ru_stash_socket_runtime;
#endif

int CollectorDaemon::receive_update(int command, Stream* sock)
{
    int	insert;
	CollectorRecord *record;
	_condor_auto_accum_runtime<collector_runtime_probe> rt(CollectorEngine_receive_update_runtime);
#ifdef PROFILE_RECEIVE_UPDATE
	double rt_last = rt.begin;
#endif

	daemonCore->dc_stats.AddToAnyProbe("UpdatesReceived", 1);

	/* assume the ad is malformed... other functions set this value */
	insert = -3;

	// get endpoint
	condor_sockaddr from = ((Sock*)sock)->peer_addr();

#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_ru_pre_collect_runtime += rt.tick(rt_last);
#endif
    // process the given command
	if (!(record = collector.collect (command,(Sock*)sock,from,insert)))
	{
		if (insert == -2)
		{
			// this should never happen assuming we never register QUERY
			// commands with daemon core, but it cannot hurt to check...
			dprintf (D_ALWAYS,"Got QUERY command (%d); not supported for UDP\n",
						command);
		}

		if (insert == -3)
		{
			/* this happens when we get a classad for which a hash key could
				not been made. This occurs when certain attributes are needed
				for the particular catagory the ad is destined for, but they
				are not present in the ad. */
			dprintf (D_ALWAYS,
				"Received malformed ad from command (%d). Ignoring.\n",
				command);
		}

		if (insert == -4)
		{
			// Rejected by COLLECTOR_REQUIREMENTS in validateClassad(),
			// which already does all the necessary logging.
		}

		return FALSE;

	}
#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_ru_collect_runtime += rt.tick(rt_last);
#endif

	/* let the off-line plug-in have at it */
	offline_plugin_.update ( command, *record->m_publicAd );

#if defined(UNIX) && !defined(DARWIN)
	// JEF TODO Should we use the private ad here?
	CollectorPluginManager::Update(command, *record->m_publicAd);
#endif

#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_ru_plugins_runtime += rt.tick(rt_last);
#endif

	if (!viewCollectorTypes.empty() || command == UPDATE_STARTD_AD || command == UPDATE_SUBMITTOR_AD) {
		forward_classad_to_view_collector(command,
										  ATTR_MY_TYPE,
										  record->m_pvtAd);
	}

#ifdef PROFILE_RECEIVE_UPDATE
	CollectorEngine_ru_forward_runtime += rt.tick(rt_last);
#endif

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		int rv = stashSocket( (ReliSock *)sock );
#ifdef PROFILE_RECEIVE_UPDATE
		CollectorEngine_ru_stash_socket_runtime += rt.tick(rt_last);
#endif
		return rv;
	}

	// let daemon core clean up the socket
	return TRUE;
}

int CollectorDaemon::receive_update_expect_ack(int command,
												Stream *stream )
{

    if ( NULL == stream ) {
        return FALSE;
    }

    Sock        *socket = (Sock*) stream;
    ClassAd     *updateAd = new ClassAd;
    const int   timeout = 5;
    int         ok      = 1;
    
    socket->decode ();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	socket->timeout(1);

    if ( !getClassAd ( socket, *updateAd ) ) {

        dprintf ( 
            D_ALWAYS,
            "receive_update_expect_ack: "
            "Failed to read class ad off the wire, aborting\n" );

        return FALSE;

    }

    /* assume the ad is malformed */
    int insert = -3;
    
    /* get peer's IP/port */
	condor_sockaddr from = socket->peer_addr();

    /* "collect" the ad */
    CollectorRecord *record = collector.collect ( 
        command,
        updateAd,
        from,
        insert );

    if ( !record ) {

        /* attempting to "collect" a QUERY or INVALIDATE command?!? */
        if ( -2 == insert ) {

	        dprintf (
                D_ALWAYS,
                "receive_update_expect_ack: "
                "Got QUERY or INVALIDATE command (%d); these are "
                "not supported.\n",
                command );

        }

        /* this happens when we get a classad for which a hash key 
        could not been made. This occurs when certain attributes are 
        needed for the particular catagory the ad is destined for, 
        but they are not present in the ad. */
	    if ( -3 == insert ) {
			
	        dprintf (
                D_ALWAYS,
                "receive_update_expect_ack: "
		        "Received malformed ad from command (%d).\n",
                command );

	    }

    } else {

        socket->encode ();
        socket->timeout ( timeout );

        /* send an acknowledgment that we received the ad */
        if ( !socket->code ( ok ) ) {
        
            dprintf ( 
                D_ALWAYS,
                "receive_update_expect_ack: "
                "Failed to send acknowledgement to host %s, "
                "aborting\n",
                socket->peer_ip_str () );
        
            /* it's ok if we fail here, since we won't drop the ad,
            it's only on the client side that any error should be
            treated as fatal */

        }

        if ( !socket->end_of_message () ) {
        
            dprintf ( 
                D_FULLDEBUG, 
                "receive_update_expect_ack: "
                "Failed to send update EOM to host %s.\n", 
                socket->peer_ip_str () );
            
	    }   
        
    }

	if(record) {
		offline_plugin_.update ( command, *record->m_publicAd );

#if defined(UNIX) && !defined(DARWIN)
		// JEF TODO Should we use the private ad here?
		CollectorPluginManager::Update ( command, *record->m_publicAd );
#endif

		if (!viewCollectorTypes.empty() || UPDATE_STARTD_AD_WITH_ACK == command) {
			forward_classad_to_view_collector(command,
										  ATTR_MY_TYPE,
										  record->m_pvtAd);
		}
	}

	// let daemon core clean up the socket
	return TRUE;
}


int
CollectorDaemon::stashSocket( ReliSock* sock )
{
	if( daemonCore->SocketIsRegistered( sock ) ) {
		return KEEP_STREAM;
	}

	std::string msg;
	if( daemonCore->TooManyRegisteredSockets(sock->get_file_desc(),&msg) ) {
		dprintf(D_ALWAYS,
				"WARNING: cannot register TCP update socket from %s: %s\n",
				sock->peer_description(), msg.c_str());
		return FALSE;
	}

		// Register this socket w/ DaemonCore so we wake up if
		// there's more data to read...
	int rc = daemonCore->Register_Command_Socket(
		sock, "Update Socket" );

	if( rc < 0 ) {
		dprintf(D_ALWAYS,
				"Failed to register TCP socket from %s for updates: error %d.\n",
				sock->peer_description(),rc);
		return FALSE;
	}

	dprintf( D_FULLDEBUG,
			 "Registered TCP socket from %s for updates.\n",
			 sock->peer_description() );

	return KEEP_STREAM;
}

int CollectorDaemon::collect_op::query_scanFunc (CollectorRecord *record)
{
	ClassAd* cad = record->m_publicAd;

	if (__mytype__ && MATCH != strcasecmp(__mytype__, GetMyTypeName(*cad))) {
		return 1;
	}

	int rc = 1;
	classad::Value result;
	bool val;
	if ( ! __filter__ ||
		(EvalExprToBool( __filter__, cad, NULL, result ) &&
		 result.IsBooleanValueEquiv(val) && val)) {
		bool absent = false;
		if (__skip_absent__ && cad->LookupBool(ATTR_ABSENT, absent) && absent) {
			__absent__++;
		} else {
			// Found a match
			__numAds__++;
			__results__->push_back(record);
			if (__numAds__ >= __resultLimit__) {
				rc = 0; // tell it to stop iterating, we have all the results we want
			}
		}
	} else {
		__failed__++;
	}

    return rc;
}

#if 0

void CollectorDaemon::process_query_public (AdTypes whichAds,
	ClassAd *query,
	List<CollectorRecord>* results)
{
	// set up for hashtable scan
	__query__ = query;
	__numAds__ = 0;
	__failed__ = 0;
	__ClassAdResultList__ = results;
	// An empty adType means don't check the MyType of the ads.
	// This means either the command indicates we're only checking one
	// type of ad, or the query's TargetType is "Any" (match all ad types).
	__adType__ = "";
	if ( whichAds == GENERIC_AD || whichAds == ANY_AD ) {
		query->LookupString( ATTR_TARGET_TYPE, __adType__ );
		if ( strcasecmp( __adType__.c_str(), "any" ) == 0 ) {
			__adType__ = "";
		}
	}

	__filter__ = query->LookupExpr( ATTR_REQUIREMENTS );
	if ( __filter__ == NULL ) {
		dprintf (D_ALWAYS, "Query missing %s\n", ATTR_REQUIREMENTS );
		return;
	}

	__resultLimit__ = INT_MAX; // no limit
	if ( ! query->LookupInteger(ATTR_LIMIT_RESULTS, __resultLimit__) || __resultLimit__ <= 0) {
		__resultLimit__ = INT_MAX; // no limit
	}

	// If ABSENT_REQUIREMENTS is defined, rewrite filter to filter-out absent ads 
	// if ATTR_ABSENT is not alrady referenced in the query.
	if ( filterAbsentAds ) {	// filterAbsentAds is true if ABSENT_REQUIREMENTS defined
		classad::References machine_refs;  // machine attrs referenced by requirements
		bool checks_absent = false;

		GetReferences(ATTR_REQUIREMENTS,*query,NULL,&machine_refs);
		checks_absent = machine_refs.count( ATTR_ABSENT );
		if (!checks_absent) {
			std::string modified_filter;
			formatstr(modified_filter, "(%s) && (%s =!= True)",
				ExprTreeToString(__filter__),ATTR_ABSENT);
			query->AssignExpr(ATTR_REQUIREMENTS,modified_filter.c_str());
			__filter__ = query->LookupExpr(ATTR_REQUIREMENTS);
			if ( __filter__ == NULL ) {
				dprintf (D_ALWAYS, "Failed to parse modified filter: %s\n", 
					modified_filter.c_str());
				return;
			}
			dprintf(D_FULLDEBUG,"Query after modification: *%s*\n",modified_filter.c_str());
		}
	}

	if (!collector.walkHashTable (whichAds, query_scanFunc))
	{
		dprintf (D_ALWAYS, "Error sending query response\n");
	}

	dprintf (D_ALWAYS, "(Sending %d ads in response to query)\n", __numAds__);
}
#endif

//
// Setting ATTR_LAST_HEARD_FROM to 0 causes the housekeeper to invalidate
// the ad.  Since we don't want that -- we just want the ad to expire --
// set the time to the next-smallest legal value, instead.  Expiring
// invalidated ads allows the offline plugin to decide if they should go
// absent, instead.
//
int CollectorDaemon::collect_op::expiration_scanFunc (CollectorRecord *record)
{
    return setAttrLastHeardFrom( record->m_publicAd, 1 );
}

int CollectorDaemon::collect_op::invalidation_scanFunc (CollectorRecord *record)
{
    return setAttrLastHeardFrom( record->m_publicAd, 0 );
}

int CollectorDaemon::collect_op::setAttrLastHeardFrom (ClassAd* cad, unsigned long time)
{
	if (__mytype__) {
		std::string type = "";
		cad->LookupString( ATTR_MY_TYPE, type );
		if (MATCH != strcasecmp( type.c_str(), __mytype__)) {
			return 1;
		}
	}

	classad::Value result;
	bool val;
	if ( EvalExprToBool( __filter__, cad, NULL, result ) &&
		 result.IsBooleanValueEquiv(val) && val ) {

		cad->Assign( ATTR_LAST_HEARD_FROM, time );
        __numAds__++;
    }

    return 1;
}

void CollectorDaemon::collect_op::process_invalidation (AdTypes whichAds, ClassAd &query, Stream *sock)
{
	if (param_boolean("IGNORE_INVALIDATE", false)) {
		dprintf(D_ALWAYS, "Ignoring invalidate (IGNORE_INVALIDATE=TRUE)\n");
		return;
	}

	// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	CollectorEngine::HashFunc makeKey = nullptr;
	CollectorHashTable * hTable = nullptr;
	CollectorHashTable * hTablePvt = nullptr;
	int num_pvt = 0;
	bool expireInvalidatedAds = param_boolean( "EXPIRE_INVALIDATED_ADS", false );

	//std::string adbuf;
	//dprintf(D_ZKM, "process_invalidate(%d) : %s\n", whichAds, formatAd(adbuf, query, "\t", nullptr, false));

	// For commands that specify the ad table type indirectly via the command int
	// it is not necessary to specify the target type of the ad(s) to be invalidated.
	// But when the command doesn't indicate a specific table, use the targettype
	// to figure out the table.
	std::string target_type;
	if (whichAds == GENERIC_AD || whichAds == ANY_AD || whichAds == BOGUS_AD) {
		if (query.LookupString(ATTR_TARGET_TYPE, target_type)) {
			__mytype__ = target_type.c_str(); // in case we want to constrain an invalidate
			hTable = collector.getGenericHashTable(target_type.c_str());
			if (hTable) { makeKey = makeGenericAdHashKey; }
			else if (whichAds != GENERIC_AD) {
				// maybe a standard table after all?
				AdTypes adtype = AdTypeStringToWhichAds(__mytype__);
				if (adtype != NO_AD) {
					collector.LookupByAdType(adtype, hTable, makeKey);
				}
			}
		} else {
			dprintf(D_ALWAYS, "Invalidate %s failed - no target MyType specified\n", AdTypeToString(whichAds));
			return;
		}
	} else if (whichAds == STARTD_AD) {
		// INVALIDATE_STARTD_AD is used to invalidate the slot and the daemon ads
		if (query.LookupString(ATTR_TARGET_TYPE, target_type) && YourStringNoCase(STARTD_DAEMON_ADTYPE) == target_type) {
			whichAds = STARTDAEMON_AD;
		}
		collector.LookupByAdType(whichAds, hTable, makeKey);
		if ( ! hTable) { target_type = AdTypeToString(whichAds); } // for the error message
		else if (whichAds == STARTD_AD) { 
			CollectorEngine::HashFunc dummy;
			collector.LookupByAdType(STARTD_PVT_AD, hTablePvt, dummy);
		}

	} else {
		collector.LookupByAdType(whichAds, hTable, makeKey);
		if ( ! hTable) { target_type = AdTypeToString(whichAds); } // for the error message
	}
	if ( ! hTable) {
		dprintf(D_ALWAYS, "Invalidate failed - %s has no table\n", target_type.c_str());
		return;
	}

	AdNameHashKey hKey;
	if (makeKey(hKey, &query)) {

		if( expireInvalidatedAds ) {
			__numAds__ = collector.expire(hTable, hKey);
			if (hTablePvt) {
				num_pvt = collector.expire(hTablePvt, hKey);
				if (num_pvt > __numAds__) { dprintf(D_ALWAYS, "WARNING: expired %d private ads for %d ads\n", num_pvt, __numAds__); }
			}
		} else {
			__numAds__ = collector.remove(hTable, hKey, whichAds);
			if (hTablePvt) {
				num_pvt = collector.remove(hTablePvt, hKey, STARTD_PVT_AD);
				if (num_pvt > __numAds__) { dprintf(D_ALWAYS, "WARNING: removed %d private ads for %d ads\n", num_pvt, __numAds__); }
			}
		}

	} else {
		dprintf ( D_ALWAYS, "Walking tables to invalidate... O(n)\n" );

		// set up for hashtable scan
		__query__ = &query;
		__mytype__ = nullptr;
		__filter__ = query.LookupExpr( ATTR_REQUIREMENTS );
		if ( ! __filter__) {
			dprintf (D_ALWAYS, "Invalidation missing %s\n", ATTR_REQUIREMENTS );
			return;
		}

		if (expireInvalidatedAds) {
			collector.walkHashTable (*hTable, [this](CollectorRecord*cr){return this->expiration_scanFunc(cr);});
			if (hTablePvt) { collector.walkHashTable (*hTablePvt, [this](CollectorRecord*cr){return this->expiration_scanFunc(cr);}); }
			collector.invokeHousekeeper (whichAds);
		} else if (param_boolean("HOUSEKEEPING_ON_INVALIDATE", true))  {
			// first set all the "LastHeardFrom" attributes to low values ...
			collector.walkHashTable (*hTable, [this](CollectorRecord*cr){return this->invalidation_scanFunc(cr);});
			if (hTablePvt) { collector.walkHashTable (*hTablePvt, [this](CollectorRecord*cr){return this->invalidation_scanFunc(cr);}); }
			// ... then invoke the housekeeper
			collector.invokeHousekeeper (whichAds);
		} else {
			__numAds__ = collector.invalidateAds(hTable, __mytype__, query);
			if (hTablePvt) {
				num_pvt = collector.invalidateAds(hTablePvt, __mytype__, query);
				if (num_pvt > __numAds__) { dprintf(D_ALWAYS, "WARNING: invalidated %d private ads for %d ads\n", num_pvt, __numAds__); }
			}
		}
	}

	if (hTablePvt) {
		dprintf (D_ALWAYS, "(Invalidated %d ads) + %d private ads\n", __numAds__, num_pvt );
	} else {
		dprintf (D_ALWAYS, "(Invalidated %d ads)\n", __numAds__ );
	}

		// Suppose lots of ads are getting invalidated and we have no clue
		// why.  That is what the following block of code tries to solve.
	if( __numAds__ > 1 ) {
		std::string buf;
		dprintf(D_ALWAYS, "The invalidation query was this: %s\n", formatAd(buf, query, "\t", nullptr, false));
	}
}	


#if 1
#else

int CollectorDaemon::reportStartdScanFunc( CollectorRecord *record )
{
	return normalTotals->update( record->m_publicAd );
}

int CollectorDaemon::reportSubmittorScanFunc( CollectorRecord *record )
{
	++submittorNumAds;

	int tmp1, tmp2;
	if( !record->m_publicAd->LookupInteger( ATTR_RUNNING_JOBS , tmp1 ) ||
		!record->m_publicAd->LookupInteger( ATTR_IDLE_JOBS, tmp2 ) )
			return 0;
	submittorRunningJobs += tmp1;
	submittorIdleJobs	 += tmp2;

	return 1;
}

int CollectorDaemon::reportMiniStartdScanFunc( CollectorRecord *record )
{
    char buf[80];
	int iRet = 0;

	++startdNumAds;

	if ( record->m_publicAd->LookupString( ATTR_STATE, buf, sizeof(buf) ) )
	{
		machinesTotal++;
		switch ( buf[0] )
		{
			case 'C':
				machinesClaimed++;
				break;
			case 'U':
				machinesUnclaimed++;
				break;
			case 'O':
				machinesOwner++;
				break;
		}

		// Count the number of jobs in each universe
		int universe;
		if ( record->m_publicAd->LookupInteger( ATTR_JOB_UNIVERSE, universe ) )
		{
			ustatsAccum.accumulate( universe );
		}
		iRet = 1;
	}

    return iRet;
}

#endif

void CollectorDaemon::Config()
{
	dprintf(D_ALWAYS, "In CollectorDaemon::Config()\n");

    char	*tmp;
    int     ClassadLifetime;

    ClientTimeout = param_integer ("CLIENT_TIMEOUT",30);
    QueryTimeout = param_integer ("QUERY_TIMEOUT",60);
    ClassadLifetime = param_integer ("CLASSAD_LIFETIME",900);

    if (CollectorName) free (CollectorName);
    CollectorName = param("COLLECTOR_NAME");

	HandleQueryInProcPolicy = HandleQueryInProcSmallTableOrQuery;
	auto_free_ptr policy(param("HANDLE_QUERY_IN_PROC_POLICY"));
	if (policy) {
		bool boolval;
		if (string_is_boolean_param(policy, boolval)) {
			if( boolval ) {
				HandleQueryInProcPolicy = HandleQueryInProcAlways;
			} else {
				HandleQueryInProcPolicy = HandleQueryInProcNever;
			}
		} else if (YourStringNoCase("always") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcAlways;
		} else if (YourStringNoCase("never") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcNever;
		} else if (YourStringNoCase("small_table") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallTable;
		} else if (YourStringNoCase("small_query") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallQuery;
		} else if (YourStringNoCase("small_table_or_query") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallTableOrQuery;
		} else if (YourStringNoCase("small_table_and_query") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallTableAndQuery;
		} else {
			dprintf(D_ALWAYS, "Unknown value for HANDLE_QUERY_IN_PROC_POLICY, using default of SMALL_TABLE_OR_QUERY\n");
		}
	}

	// handle params for Collector updates
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}

	DCCollectorAdSequences * adSeq = NULL;
	if( collectorsToUpdate ) {
		adSeq = collectorsToUpdate->detachAdSequences();
		delete collectorsToUpdate;
		collectorsToUpdate = NULL;
	}
	collectorsToUpdate = CollectorList::create(NULL, adSeq);

	//
	// If we don't use the network to update ourselves, we could allow
	// [UPDATE|INVALIDATE]_COLLECTOR_AD[S] to use TCP (and therefore
	// shared port).  This would also bypass the need for us to have a
	// UDP command socket (which we currently won't, if we're using
	// shared port without an assigned port number).
	//

	const char * myself = daemonCore->InfoCommandSinfulString();
	if( myself == NULL ) {
		EXCEPT( "Unable to determine my own address, aborting rather than hang.  You may need to make sure the shared port daemon is running first." );
	}
	Sinful mySinful( myself );
	std::erase_if(collectorsToUpdate->getList(),
			[&](DCCollector* curr) {
				return curr->addr() && mySinful.addressPointsToMe(Sinful(curr->addr()));
			} );

	int i = param_integer("COLLECTOR_UPDATE_INTERVAL",900); // default 15 min
	if( UpdateTimerId < 0 ) {
		UpdateTimerId = daemonCore->
			Register_Timer( 1, i, sendCollectorAd,
							"sendCollectorAd" );
	}

	collector.m_allowOnlyOneNegotiator = param_boolean("COLLECTOR_ALLOW_ONLY_ONE_NEGOTIATOR", false);
	// This it temporary (for 8.7.0) just in case we need to turn off the new getClassAdEx options
	collector.m_get_ad_options = param_integer("COLLECTOR_GETAD_OPTIONS", GET_CLASSAD_FAST | GET_CLASSAD_LAZY_PARSE);
	collector.m_get_ad_options &= (GET_CLASSAD_LAZY_PARSE | GET_CLASSAD_FAST | GET_CLASSAD_NO_CACHE);
	std::string opts;
	if (collector.m_get_ad_options & GET_CLASSAD_FAST) { opts += "fast "; }
	if (collector.m_get_ad_options & GET_CLASSAD_NO_CACHE) { opts += "no-cache "; }
	else if (collector.m_get_ad_options & GET_CLASSAD_LAZY_PARSE) { opts += "lazy-parse "; }
	if (opts.empty()) { opts = "none "; }
	dprintf(D_ALWAYS, "COLLECTOR_GETAD_OPTIONS set to %s(0x%x)\n", opts.c_str(), collector.m_get_ad_options);

	tmp = param(COLLECTOR_REQUIREMENTS);
	std::string collector_req_err;
	if( !collector.setCollectorRequirements( tmp, collector_req_err ) ) {
		EXCEPT("Handling of '%s=%s' failed: %s",
			   COLLECTOR_REQUIREMENTS,
			   tmp ? tmp : "(null)",
			   collector_req_err.c_str());
	}
	if( tmp ) {
		free( tmp );
		tmp = NULL;
	}

	init_classad(i);

    // set the appropriate parameters in the collector engine
    collector.setClientTimeout( ClientTimeout );
    collector.scheduleHousekeeper( ClassadLifetime );

    offline_plugin_.configure ();

    vc_projection.clear();

    // if we're not the View Collector, let's set something up to forward
    // all of our ads to the view collector.
    for (auto e(vc_list.begin());  e != vc_list.end();  ++e) {
        delete e->collector;
        delete e->sock;
    }
    vc_list.clear();

    tmp = param("CONDOR_VIEW_HOST");
    if (tmp) {
        for (auto& vhost: StringTokenIterator(tmp)) {
            DCCollector* vhd = new DCCollector(vhost.c_str(), DCCollector::CONFIG_VIEW);
            Sinful view_addr( vhd->addr() );
            Sinful my_addr( daemonCore->publicNetworkIpAddr() );

            if (my_addr.addressPointsToMe(view_addr)) {
                dprintf(D_ALWAYS, "Not forwarding to View Server %s - self referential\n", vhost.c_str());
                delete vhd;
                continue;
            }
            dprintf(D_ALWAYS, "Will forward ads on to View Server %s\n", vhost.c_str());

            Sock* vhsock = NULL;
            if (vhd->useTCPForUpdates()) {
                vhsock = new ReliSock();
            } else {
                vhsock = new SafeSock();
            }

            vc_list.emplace_back();
            vc_list.back().name = vhost;
            vc_list.back().collector = vhd;
            vc_list.back().sock = vhsock;
        }
        free(tmp);
    }

    if (!vc_list.empty()) {
        // protect against frequent time-consuming reconnect attempts
        view_sock_timeslice.setTimeslice(0.05);
        view_sock_timeslice.setMaxInterval(1200);
    }

	viewCollectorTypes.clear();
	if (!vc_list.empty()) {
		std::string tmp;
		if (param(tmp, "CONDOR_VIEW_CLASSAD_TYPES")) {
			viewCollectorTypes = split(tmp);
			if (contains_anycase(viewCollectorTypes, STARTD_OLD_ADTYPE) &&
				! contains_anycase(viewCollectorTypes, STARTD_SLOT_ADTYPE)) {
				viewCollectorTypes.emplace_back(STARTD_SLOT_ADTYPE);
			}
			std::string types = join(viewCollectorTypes, ",");
			dprintf(D_ALWAYS, "CONDOR_VIEW_CLASSAD_TYPES configured, will forward ad types: %s\n",
				types.c_str());
		}

		vc_projection.set(param("COLLECTOR_FORWARD_PROJECTION"));
		if ( ! vc_projection.empty()) {
			if ( ! vc_projection.Expr()) {
				dprintf(D_ALWAYS, "ignoring COLLECTOR_FORWARD_PROJECTION, it is not a valid expression: %s\n", vc_projection.c_str());
			} else {
				dprintf(D_ALWAYS, "COLLECTOR_FORWARD_PROJECTION configured, will forward: %s\n", vc_projection.c_str());
			}
		}
	}

	int history_size = 1024;

	bool collector_daemon_stats = param_boolean ("COLLECTOR_DAEMON_STATS",true);
	collectorStats.setDaemonStats( collector_daemon_stats );
	if (param_boolean("RESET_DC_STATS_ON_RECONFIG",false)) {
		daemonCore->dc_stats.Clear();
		collectorStats.global.Clear();
	}

	history_size = param_integer ("COLLECTOR_DAEMON_HISTORY_SIZE",128);
	collectorStats.setDaemonHistorySize( history_size );

	time_t garbage_interval = param_integer( "COLLECTOR_STATS_SWEEP", DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL );
	collectorStats.setGarbageCollectionInterval( garbage_interval );

    max_query_workers = param_integer ("COLLECTOR_QUERY_WORKERS", 4, 0);
	max_pending_query_workers = param_integer ("COLLECTOR_QUERY_WORKERS_PENDING", 50, 0);
	max_query_worktime = param_integer("COLLECTOR_QUERY_MAX_WORKTIME",0,0);
	reserved_for_highprio_query_workers = param_integer("COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO",1,0);

	// max_query_workers had better be at least one greater than reserved_for_highprio_query_workers,
	// or condor_status queries will never be answered.
	// note we do allow max_query_workers to be zero, which means do all queries in-proc - this
	// is useful for developers profiling the code, since profiling forked workers is a pain.
	if ( max_query_workers - reserved_for_highprio_query_workers < 1) {
		reserved_for_highprio_query_workers = MAX(0, (max_query_workers - 1) );
		dprintf(D_ALWAYS,
				"Warning: Resetting COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO to %d\n",
				reserved_for_highprio_query_workers);
	}

#ifdef TRACK_QUERIES_BY_SUBSYS
	want_track_queries_by_subsys = param_boolean("COLLECTOR_TRACK_QUERY_BY_SUBSYS",true);
#endif

	bool ccb_server_enabled = param_boolean("ENABLE_CCB_SERVER",true);
	if( ccb_server_enabled ) {
		if( !m_ccb_server ) {
			dprintf(D_ALWAYS, "Enabling CCB Server.\n");
			m_ccb_server = new CCBServer;
		}
		m_ccb_server->InitAndReconfig();
	}
	else if( m_ccb_server ) {
		dprintf(D_ALWAYS, "Disabling CCB Server.\n");
		delete m_ccb_server;
		m_ccb_server = NULL;
	}

	if ( (tmp=param("ABSENT_REQUIREMENTS")) ) {
		filterAbsentAds = true;
		free(tmp);
	} else {
		filterAbsentAds = false;
	}

	forwardClaimedPrivateAds = param_boolean("COLLECTOR_FORWARD_CLAIMED_PRIVATE_ADS", true);

	// load FORWARD_AD_TRANSFORM_NAMES and FORWARD_AD_TRANSFORM_*
	m_forward_ad_xfm.config("FORWARD_AD");

	return;
}

void CollectorDaemon::Exit()
{
	// Clean up any workers that have exited but haven't been reaped yet.
	// This can occur if the collector receives a query followed
	// immediately by a shutdown command.  The worker will exit but
	// not be reaped because the SIGTERM from the shutdown command will
	// be processed before the SIGCHLD from the worker process exit.
	// Allowing the stack to clean up worker processes is problematic
	// because the collector will be shutdown and the daemonCore
	// object deleted by the time the worker cleanup is attempted.
	// forkQuery.DeleteAll( );
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	free( CollectorName );
	delete ad;
	delete collectorsToUpdate;
	delete m_ccb_server;
	return;
}

void CollectorDaemon::Shutdown()
{
	// Clean up any workers that have exited but haven't been reaped yet.
	// This can occur if the collector receives a query followed
	// immediately by a shutdown command.  The worker will exit but
	// not be reaped because the SIGTERM from the shutdown command will
	// be processed before the SIGCHLD from the worker process exit.
	// Allowing the stack to clean up worker processes is problematic
	// because the collector will be shutdown and the daemonCore
	// object deleted by the time the worker cleanup is attempted.
	// forkQuery.DeleteAll( );
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	free( CollectorName );
	delete ad;
	delete collectorsToUpdate;
	delete m_ccb_server;
	return;
}

void CollectorDaemon::sendCollectorAd(int /* tid */)
{

    struct {
        // compute submitted jobs information
        int submittorRunningJobs = 0;
        int submittorIdleJobs = 0;
        int submittorNumAds = 0;
        // compute machine information
        int machinesTotal = 0;
        int machinesUnclaimed = 0;
        int machinesClaimed = 0;
        int machinesOwner = 0;
        int startdNumAds = 0;
        CollectorUniverseStats ustatsAccum;
        char buf[16];
    } tstat;

    if( !collector.walkHashTable(*collector.getHashTable(SUBMITTOR_AD),
        [&tstat](CollectorRecord*record){
			tstat.submittorNumAds += 1;

			int tmp1, tmp2;
			if( !record->m_publicAd->LookupInteger( ATTR_RUNNING_JOBS , tmp1 ) ||
				!record->m_publicAd->LookupInteger( ATTR_IDLE_JOBS, tmp2 ) )
				return 0;

			tstat.submittorRunningJobs += tmp1;
			tstat.submittorIdleJobs	 += tmp2;

			return 1;
        })) {
         dprintf( D_ALWAYS, "Error making collector ad (submittor scan)\n" );
    }
    collectorStats.global.SubmitterAds = tstat.submittorNumAds;

	tstat.ustatsAccum.Reset( );
    if (!collector.walkHashTable (*collector.getHashTable(STARTD_AD),
        [&tstat](CollectorRecord*record){
			tstat.startdNumAds += 1;

			if ( record->m_publicAd->LookupString( ATTR_STATE, tstat.buf, sizeof(tstat.buf) ) )
			{
				tstat.machinesTotal++;
				switch ( tstat.buf[0] )
				{
				case 'C':
					tstat.machinesClaimed++;
					break;
				case 'U':
					tstat.machinesUnclaimed++;
					break;
				case 'O':
					tstat.machinesOwner++;
					break;
				}

				// Count the number of jobs in each universe
				int universe;
				if ( record->m_publicAd->LookupInteger( ATTR_JOB_UNIVERSE, universe ) )
				{
					tstat.ustatsAccum.accumulate( universe );
				}
				return 1;
			}

			return 0;
        })) {
            dprintf (D_ALWAYS, "Error making collector ad (startd scan) \n");
    }
    collectorStats.global.MachineAds = tstat.startdNumAds;

    // insert values into the ad
    ad->InsertAttr(ATTR_RUNNING_JOBS,       tstat.submittorRunningJobs);
    ad->InsertAttr(ATTR_IDLE_JOBS,          tstat.submittorIdleJobs);
    ad->InsertAttr(ATTR_NUM_HOSTS_TOTAL,    tstat.machinesTotal);
    ad->InsertAttr(ATTR_NUM_HOSTS_CLAIMED,  tstat.machinesClaimed);
    ad->InsertAttr(ATTR_NUM_HOSTS_UNCLAIMED,tstat.machinesUnclaimed);
    ad->InsertAttr(ATTR_NUM_HOSTS_OWNER,    tstat.machinesOwner);

	// Accumulate for the monthly
	ustatsMonthly.setMax( tstat.ustatsAccum );

	// If we've got any universe reports, find the maxes
	tstat.ustatsAccum.publish( ATTR_CURRENT_JOBS_RUNNING, ad );
	ustatsMonthly.publish( ATTR_MAX_JOBS_RUNNING, ad );

	// Collector engine stats, too
	collectorStats.publishGlobal( ad, NULL );
    daemonCore->dc_stats.Publish(*ad);
    daemonCore->monitor_data.ExportData(ad);

	//
	// Make sure that our own ad is in the collector hashtable table, even if send-updates won't put it there
	// note that sendUpdates might soon _overwrite_ this ad with a differnt copy, but that's ok once we have
	// identified the selfAd with the collector engine, it's supposed to keep the self ad pointer updated
	// as it replaces the collector ad in that hashtable slot.
	//
	int error = 0;
	ClassAd * selfAd = new ClassAd(*ad);

		// Administrative security sessions are added directly in the daemon core code; since we
		// are bypassing DC and invoking collector.collect on the selfAd, invoke it here as well.
	std::string capability;
	if (daemonCore->SetupAdministratorSession(1800, capability)) {
		selfAd->InsertAttr(ATTR_REMOTE_ADMIN_CAPABILITY, capability);
	}

	CollectorRecord* self_rec = nullptr;
	if( ! (self_rec = collector.collect( UPDATE_COLLECTOR_AD, selfAd, condor_sockaddr::null, error )) ) {
		dprintf( D_ERROR, "Failed to add my own ad to myself (%d).\n", error );
		delete selfAd;
	}

	if (self_rec) {
		collector.identifySelfAd(self_rec);

		// inserting the selfAd into the collector hashtable will stomp the update counters
		// so clear out the per-daemon Updates* stats to avoid confusion with the global stats
		// and re-publish the global collector stats.
		//PRAGMA_REMIND("tj: remove this code once the collector generates it's ad when queried.")
		selfAd->Delete(ATTR_UPDATESTATS_HISTORY);
		selfAd->Delete(ATTR_UPDATESTATS_SEQUENCED);
		collectorStats.publishGlobal(selfAd, NULL);
	}

	// Send the ad
	int num_updated = collectorsToUpdate->sendUpdates(UPDATE_COLLECTOR_AD, ad, NULL, false);
	if ( num_updated != (int)collectorsToUpdate->getList().size() ) {
		dprintf( D_ALWAYS, "Unable to send UPDATE_COLLECTOR_AD to all configured collectors\n");
	}

}

void CollectorDaemon::init_classad(int interval)
{
	if( ad ) delete( ad );
	ad = new ClassAd();

	SetMyTypeName(*ad, COLLECTOR_ADTYPE);

	char *tmp;
	tmp = param( "CONDOR_ADMIN" );
	if( tmp ) {
		ad->Assign( ATTR_CONDOR_ADMIN, tmp );
		free( tmp );
	}

	std::string id;
	if( CollectorName ) {
		if (strchr(CollectorName, '@')) {
			id = CollectorName;
		} else {
			id = CollectorName;
			id += '@';
			id += get_local_fqdn();
		}
	} else if (get_mySubSystem()->getLocalName()) {
		id = get_mySubSystem()->getLocalName();
	} else {
		id = get_local_fqdn();
	}
	ad->Assign( ATTR_NAME, id );

	ad->Assign( ATTR_COLLECTOR_IP_ADDR, global_dc_sinful() );

	if ( interval > 0 ) {
		ad->Assign( ATTR_UPDATE_INTERVAL, 24*interval );
	}

	// Publish all DaemonCore-specific attributes, which also handles
	// COLLECTOR_ATTRS for us.
	daemonCore->publish(ad);
}

void
CollectorDaemon::forward_classad_to_view_collector(int cmd,
										   const char *filterAttr,
										   ClassAd *theAd)
{
	if (vc_list.empty()) return;

	if (!theAd) {
		dprintf(D_ALWAYS, "Trying to forward ad on, but ad is NULL\n");
		return;
	}

	if (filterAttr && !viewCollectorTypes.empty()) {
		std::string type;
		if (!theAd->EvaluateAttrString(std::string(filterAttr), type)) {
			dprintf(D_ALWAYS, "Failed to lookup %s on ad, not forwarding\n", filterAttr);
			return;
		}

		if (!contains_anycase(viewCollectorTypes, type)) {
			return;
		}
		dprintf(D_ALWAYS, "Forwarding ad: type=%s command=%s\n",
				type.c_str(), getCommandString(cmd));
	}

	ClassAd *pvtAd = nullptr;
	if (cmd == UPDATE_STARTD_AD) {
		// Forward the startd private ad as well.  This allows the
		// target collector to act as an aggregator for multiple collectors
		// that balance the load of authenticating connections from
		// the rest of the pool.
		AdNameHashKey hk;
		ASSERT( makeStartdAdHashKey (hk, theAd) );
		CollectorRecord* pvt_rec = collector.lookup(STARTD_PVT_AD,hk);
		pvtAd = pvt_rec ? pvt_rec->m_pvtAd : nullptr;
		if (pvt_rec && !forwardClaimedPrivateAds){
			std::string state;
			if (theAd->LookupString(ATTR_STATE, state) && state == "Claimed") {
				pvtAd = nullptr;
			}
		}
	}

	bool should_forward = true;
	theAd->LookupBool( ATTR_SHOULD_FORWARD, should_forward );
	if ( !should_forward && pvtAd ) {
		pvtAd->LookupBool( ATTR_SHOULD_FORWARD, should_forward );
	}
	if ( !should_forward ) {
		// TODO Should we remove the ShouldForward attribute?
		dprintf( D_FULLDEBUG, "Trying to forward ad on, but %s=False\n", ATTR_SHOULD_FORWARD );
		return;
	}

	// Transform ad
	//
	// If we're forwarding an update, then theAd points to the private
	// ad in our collection. We want to transform the public ad that it's
	// chained to.
	// If we're forwarding an invalidate, then there is no chained parent.
	ClassAd* xfm_ad = theAd->GetChainedParentAd();
	if (!xfm_ad) {
		xfm_ad = theAd;
	}
	m_forward_ad_xfm.transform(xfm_ad, nullptr);

    for (auto e(vc_list.begin());  e != vc_list.end();  ++e) {
        DCCollector* view_coll = e->collector;
        Sock* view_sock = e->sock;
        const char* view_name = e->name.c_str();

        bool raw_command = false;
        if (!view_sock->is_connected()) {
            // We must have gotten disconnected.  (Or this is the 1st time.)
            // In case we keep getting disconnected or fail to connect,
            // and each connection attempt takes a long time, restrict
            // what fraction of our time we spend trying to reconnect.
            if (view_sock_timeslice.isTimeToRun()) {
                dprintf(D_ALWAYS,"Connecting to CONDOR_VIEW_HOST %s\n", view_name);

                // Only run timeslice timer for TCP, since connect on UDP 
                // is instantaneous.
                if (view_sock->type() == Stream::reli_sock) {
	                view_sock_timeslice.setStartTimeNow();
                }
                int r = view_coll->connectSock(view_sock,20);
                if (view_sock->type() == Stream::reli_sock) {
	                view_sock_timeslice.setFinishTimeNow();
                }

                if (!view_sock->is_connected() || (!r)) {
                    dprintf(D_ALWAYS,"Failed to connect to CONDOR_VIEW_HOST %s so not forwarding ad.\n", view_name);
                    continue;
                }
            } else {
                dprintf(D_FULLDEBUG,"Skipping forwarding of ad to CONDOR_VIEW_HOST %s, because reconnect is delayed for %us.\n", view_name, view_sock_timeslice.getTimeToNextRun());
                continue;
            }
        } else if (view_sock->type() == Stream::reli_sock) {
            // we already did the security handshake the last time
            // we sent a command on this socket, so just send a
            // raw command this time to avoid reauthenticating
            raw_command = true;
        }

        // Run timeslice timer if raw_command is false, since this means 
        // startCommand() may need to initiate an authentication round trip 
        // and thus could block if the remote view collector is unresponsive.
        if ( raw_command == false ) {
	        view_sock_timeslice.setStartTimeNow();
        }
        bool start_command_result = 
			view_coll->startCommand(cmd, view_sock, 20, NULL, NULL, raw_command);
        if ( raw_command == false ) {
	        view_sock_timeslice.setFinishTimeNow();
        }

        if (! start_command_result ) {
            dprintf( D_ALWAYS, "Can't send command %d to View Collector %s\n", 
					cmd, view_name);
            view_sock->end_of_message();
            view_sock->close();
            continue;
        }

        if (theAd) {
            // if there is a COLLECTOR_FORWARD_PROJECTION expression, evaluate it against the ad we plan to send
            // and send only the projected attributes if the projection is non-empty
            classad::References proj, * whitelist = nullptr;
            classad::ExprTree * proj_expr = vc_projection.Expr(); // we already reported errors on config load
            if (proj_expr) {
                classad::Value val;
                const char * proj_str = NULL;
                if (EvalExprToString(proj_expr, theAd, NULL, val) && val.IsStringValue(proj_str)) {
                    add_attrs_from_string_tokens(proj, proj_str);
                }
                if ( ! proj.empty()) { whitelist = &proj; }
            }

            if (!putClassAd(view_sock, *theAd, 0, whitelist)) {
                dprintf( D_ALWAYS, "Can't forward classad to View Collector %s\n", view_name);
                view_sock->end_of_message();
                view_sock->close();
                continue;
            }
        }

        // If there's a private startd ad, send that as well.
        if (pvtAd) {
            if (!putClassAd(view_sock, *pvtAd)) {
                dprintf( D_ALWAYS, "Can't forward startd private classad to View Collector %s\n", view_name);
                view_sock->end_of_message();
                view_sock->close();
                continue;
            }
        }

        if (!view_sock->end_of_message()) {
            dprintf(D_ALWAYS, "Can't send end_of_message to View Collector %s\n", view_name);
            view_sock->close();
            continue;
        }
    }
}

//  Collector stats on universes
CollectorUniverseStats::CollectorUniverseStats( void )
{
	Reset( );
}

//  Collector stats on universes
CollectorUniverseStats::CollectorUniverseStats( CollectorUniverseStats &ref )
{
	int		univ;

	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		perUniverse[univ] = ref.perUniverse[univ];
	}
	count = ref.count;
}

CollectorUniverseStats::~CollectorUniverseStats( void )
{
}

void
CollectorUniverseStats::Reset( void )
{
	int		univ;

	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		perUniverse[univ] = 0;
	}
	count = 0;
}

void
CollectorUniverseStats::accumulate(int univ )
{
	if (  ( univ >= 0 ) && ( univ < CONDOR_UNIVERSE_MAX ) ) {
		perUniverse[univ]++;
		count++;
	}
}

int
CollectorUniverseStats::getValue (int univ )
{
	if (  ( univ >= 0 ) && ( univ < CONDOR_UNIVERSE_MAX ) ) {
		return perUniverse[univ];
	} else {
		return -1;
	}
}

int
CollectorUniverseStats::getCount ( void ) const
{
	return count;
}

int
CollectorUniverseStats::setMax( CollectorUniverseStats &ref )
{
	int		univ;

	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		if ( ref.perUniverse[univ] > perUniverse[univ] ) {
			perUniverse[univ] = ref.perUniverse[univ];
		}
		if ( ref.count > count ) {
			count = ref.count;
		}
	}
	return 0;
}

const char *
CollectorUniverseStats::getName( int univ )
{
	return CondorUniverseNameUcFirst( univ );
}

int
CollectorUniverseStats::publish( const char *label, ClassAd *ad )
{
	int	univ;
	std::string attrn;

	// Loop through, publish all universes with a name
	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		const char *name = getName( univ );
		if ( name ) {
			attrn = label;
			attrn += name;
			ad->Assign(attrn, getValue(univ));
		}
	}
	attrn = label;
	attrn += "All";
	ad->Assign(attrn, count);

	return 0;
}

