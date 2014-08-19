/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"
#include "condor_threads.h"

#include "clerk_utils.h"
#include "clerk.h"

// set this to true when this deamon is the collector (it turns off reporting to the collector ;)
const bool ImTheCollector = false;

//-------------------------------------------------------------

// define single instance objects at global scope as members of a class app.
// this prevents (among other things) problems with the use of an object called daemon on OSX
struct {
	CmDaemon daemon;
	ForkWork forker;
} app;

CmDaemon::CmDaemon()
	: m_initialized(false)
	, m_idTimerSendUpdates(-1)
		// cached param values
	, m_UPDATE_INTERVAL(15*60)
	, m_CLIENT_TIMEOUT(30)
	, m_QUERY_TIMEOUT(60)
	, m_CLASSAD_LIFETIME(900)
	, m_LOG_UPDATES(false)
{
}

CmDaemon::~CmDaemon()
{
}

typedef struct {
	int id;
	AdTypes atype;
} CmdAdTypePair;

template <typename T>
const T * BinaryLookup (const T aTable[], int cElms, int id)
{
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = aTable[ix].id - id;
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}

// Table to allow quick lookup of adtype from command id.
// THIS MUST BE SORTED BY COMMAND ID!
static const CmdAdTypePair aCommandToAdType[] = {
	{ UPDATE_STARTD_AD,        STARTD_AD },			// 0
	{ UPDATE_SCHEDD_AD,        SCHEDD_AD },			// 1
	{ UPDATE_MASTER_AD,        MASTER_AD },			// 2
	{ UPDATE_CKPT_SRVR_AD,     CKPT_SRVR_AD },		// 4
	{ QUERY_STARTD_ADS,        STARTD_AD },			// 5
	{ QUERY_SCHEDD_ADS,        SCHEDD_AD },			// 6
	{ QUERY_MASTER_ADS,        MASTER_AD },			// 7
	{ QUERY_CKPT_SRVR_ADS,     CKPT_SRVR_AD },		// 9
	{ QUERY_STARTD_PVT_ADS,    STARTD_PVT_AD },		// 10
	{ UPDATE_SUBMITTOR_AD,     SUBMITTOR_AD },		// 11
	{ QUERY_SUBMITTOR_ADS,     SUBMITTOR_AD },		// 12
	{ UPDATE_COLLECTOR_AD,     COLLECTOR_AD },		// 19
	{ QUERY_COLLECTOR_ADS,     COLLECTOR_AD },		// 20
	{ UPDATE_LICENSE_AD,       LICENSE_AD },		// 42
	{ QUERY_LICENSE_ADS,       LICENSE_AD },		// 43
	{ UPDATE_STORAGE_AD,       STORAGE_AD },		// 45
	{ QUERY_STORAGE_ADS,       STORAGE_AD },		// 46
	{ QUERY_ANY_ADS,           ANY_AD },			// 48
	{ UPDATE_NEGOTIATOR_AD,    NEGOTIATOR_AD },		// 49
	{ QUERY_NEGOTIATOR_ADS,    NEGOTIATOR_AD },		// 50
	{ UPDATE_HAD_AD,           HAD_AD },			// 55
	{ QUERY_HAD_ADS,           HAD_AD },			// 56
	{ UPDATE_AD_GENERIC,       GENERIC_AD },		// 58
	{ UPDATE_XFER_SERVICE_AD,  XFER_SERVICE_AD },	// 61
	{ QUERY_XFER_SERVICE_ADS,  XFER_SERVICE_AD },	// 62
	{ UPDATE_LEASE_MANAGER_AD, LEASE_MANAGER_AD },	// 64
	{ QUERY_LEASE_MANAGER_ADS, LEASE_MANAGER_AD },	// 65
	{ UPDATE_GRID_AD,          GRID_AD },			// 70
	{ QUERY_GRID_ADS,          GRID_AD },			// 71
	{ MERGE_STARTD_AD,         STARTD_AD },			// 73
	{ QUERY_GENERIC_ADS,       GENERIC_AD },		// 74

};

AdTypes CollectorCmdToAdType(int cmd) {
	const CmdAdTypePair * ptr = BinaryLookup(aCommandToAdType, NUMELMS(aCommandToAdType), cmd);
	if (ptr) return ptr->atype;
	return (AdTypes)-1;
}

// tables of this struct can be used to register a set of commands id's that share a common handler.
typedef struct {
	int cmd;
	DCpermission perm;
} CmdPermPair;

void RegisterHandlers(const CmdPermPair cmds[], int num_cmds, CommandHandlercpp handler, const char * descrip, Service * serv)
{
	for (int ii = 0; ii < num_cmds; ++ii) {
		const char * name = getCommandString(cmds[ii].cmd);
		daemonCore->Register_CommandWithPayload(cmds[ii].cmd, name, handler, descrip, serv, cmds[ii].perm);
	}
}

void CmDaemon::Init()
{
	dprintf(D_FULLDEBUG, "CmDaemon::Shutdown() called\n");
	stats.Init();

	if (param(m_daemonName, "COLLECTOR_NAME")) {
		if ( ! strchr(m_daemonName.c_str(), '@')) {
			m_daemonName += "@";
			m_daemonName += get_local_fqdn();
		}
	} else {
		m_daemonName = get_local_fqdn();
	}
	if ( ! ImTheCollector) {
		m_daemonName = MY_SUBSYSTEM; m_daemonName.lower_case();
		m_daemonName += "@";
		m_daemonName += get_local_fqdn();
	}

	// setup our daemon ad
	ClassAd * ad = &m_daemonAd;
	ad->Clear();
	SetMyTypeName(*ad, COLLECTOR_ADTYPE);
	SetTargetTypeName(*ad, "");
	ad->Assign(ATTR_NAME, m_daemonName);
	ad->Assign(ATTR_COLLECTOR_IP_ADDR, global_dc_sinful());

	// Publish all DaemonCore-specific attributes, which also handles
	// COLLECTOR_ATTRS for us.
	daemonCore->publish(ad);

	// if this is the first time Init() was called, setup command handlers and timers
	if ( ! m_initialized) {

		static const CmdPermPair querys[] = {
			{ QUERY_STARTD_ADS,        READ },		// 5
			{ QUERY_STARTD_PVT_ADS,    NEGOTIATOR },// 10
			{ QUERY_GENERIC_ADS,       READ },		// 74
			{ QUERY_ANY_ADS,           READ },		// 48
			{ QUERY_SCHEDD_ADS,        READ },		// 6
			{ QUERY_GRID_ADS,          READ },		// 71
			{ QUERY_MASTER_ADS,        READ },		// 7
			{ QUERY_CKPT_SRVR_ADS,     READ },		// 9
			{ QUERY_SUBMITTOR_ADS,     READ },		// 12
			{ QUERY_LICENSE_ADS,       READ },		// 43
			{ QUERY_COLLECTOR_ADS,     READ },		// 20
			{ QUERY_STORAGE_ADS,       READ },		// 46
			{ QUERY_NEGOTIATOR_ADS,    READ },		// 50
			{ QUERY_HAD_ADS,           READ },		// 56
			{ QUERY_XFER_SERVICE_ADS,  READ },		// 62
			{ QUERY_LEASE_MANAGER_ADS, READ },		// 65
		};
		RegisterHandlers(querys, NUMELMS(querys), (CommandHandlercpp)&CmDaemon::receive_query, "receive_query", this);

		static const CmdPermPair invals[] = {
			{ INVALIDATE_STARTD_ADS,        ADVERTISE_STARTD_PERM },	// 13
			{ INVALIDATE_SCHEDD_ADS,        ADVERTISE_SCHEDD_PERM },	// 14
			{ INVALIDATE_SUBMITTOR_ADS,     ADVERTISE_SCHEDD_PERM },	// 18
			{ INVALIDATE_MASTER_ADS,        ADVERTISE_MASTER_PERM },	// 15
			{ INVALIDATE_CKPT_SRVR_ADS,     DAEMON },					// 17
			{ INVALIDATE_LICENSE_ADS,       DAEMON },					// 44
			{ INVALIDATE_COLLECTOR_ADS,     DAEMON },					// 21
			{ INVALIDATE_STORAGE_ADS,       DAEMON },					// 47
			{ INVALIDATE_NEGOTIATOR_ADS,    NEGOTIATOR },				// 51
			{ INVALIDATE_ADS_GENERIC,       DAEMON },					// 59
			{ INVALIDATE_XFER_SERVICE_ADS,  DAEMON },					// 63
			{ INVALIDATE_LEASE_MANAGER_ADS, DAEMON },					// 66
			{ INVALIDATE_GRID_ADS,          DAEMON },					// 72
		};
		RegisterHandlers(invals, NUMELMS(invals), (CommandHandlercpp)&CmDaemon::receive_invalidation,"receive_invalidation", this);

		static const CmdPermPair updates[] = {
			{ UPDATE_STARTD_AD,             ADVERTISE_STARTD_PERM },	// 0
			{ MERGE_STARTD_AD,              NEGOTIATOR },				// 73
			{ UPDATE_AD_GENERIC,            DAEMON },					// 58
			{ UPDATE_SCHEDD_AD,             ADVERTISE_SCHEDD_PERM },	// 1
			{ UPDATE_SUBMITTOR_AD,          ADVERTISE_SCHEDD_PERM },	// 11
			{ UPDATE_MASTER_AD,             ADVERTISE_MASTER_PERM },	// 2
			{ UPDATE_CKPT_SRVR_AD,          DAEMON },					// 4
			{ UPDATE_LICENSE_AD,            DAEMON },					// 42
			{ UPDATE_COLLECTOR_AD,          ALLOW },					// 19
			{ UPDATE_STORAGE_AD,            DAEMON },					// 45
			{ UPDATE_NEGOTIATOR_AD,         NEGOTIATOR },				// 49
			{ UPDATE_HAD_AD,                DAEMON },					// 55
			{ UPDATE_XFER_SERVICE_AD,       DAEMON },					// 61
			{ UPDATE_LEASE_MANAGER_AD,      DAEMON },					// 64
			{ UPDATE_GRID_AD,               DAEMON },					// 70
		};
		RegisterHandlers(updates, NUMELMS(updates), (CommandHandlercpp)&CmDaemon::receive_update,"receive_update", this);

		daemonCore->Register_CommandWithPayload(UPDATE_STARTD_AD_WITH_ACK, getCommandString(UPDATE_STARTD_AD_WITH_ACK),
			(CommandHandlercpp)&CmDaemon::receive_update_expect_ack, "receive_update_expect_ack",
			this, ADVERTISE_STARTD_PERM);

		app.forker.Initialize( );
	}

	m_initialized = true; // been through init at least once.
}

void CmDaemon::Config()
{
	dprintf(D_FULLDEBUG, "CmDaemon::Config() called\n");
	if ( ! m_initialized) Init();

	// handle params for upstream Collector updates
	if (m_idTimerSendUpdates >= 0) {
		daemonCore->Cancel_Timer(m_idTimerSendUpdates);
		m_idTimerSendUpdates = -1;
	}

	m_UPDATE_INTERVAL = param_integer("COLLECTOR_UPDATE_INTERVAL",15*60); // default 15 min
	m_CLIENT_TIMEOUT = param_integer ("CLIENT_TIMEOUT",30);
	m_QUERY_TIMEOUT = param_integer ("QUERY_TIMEOUT",60);
	m_CLASSAD_LIFETIME = param_integer ("CLASSAD_LIFETIME",900);
	m_LOG_UPDATES = param_boolean("LOG_UPDATES", false);

	MyString upstreamCollector;
	if ( ! ImTheCollector) { // just send updates to the collector like any other deamon
		m_idTimerSendUpdates = daemonCore->Register_Timer(1, m_UPDATE_INTERVAL,
			(TimerHandlercpp)&CmDaemon::update_collector, "update_collector",
			this);
	} else if (param (upstreamCollector, "CONDOR_DEVELOPERS_COLLECTOR")) {
		m_idTimerSendUpdates = daemonCore->Register_Timer(1, m_UPDATE_INTERVAL,
			(TimerHandlercpp)&CmDaemon::send_collector_ad, "send_collector_ad",
			this);
	}

	int num = param_integer ("COLLECTOR_QUERY_WORKERS", 2);
	app.forker.setMaxWorkers(num);

}

void CmDaemon::Shutdown(bool fast)
{
	dprintf(D_FULLDEBUG, "CmDaemon::Shutdown(%d) called\n", fast);
	if ( ! fast) {
		// send a final update?
		// let forked work complete?
	}

	// Clean up any workers that have exited but haven't been reaped yet.
	// This can occur if the collector receives a query followed
	// immediately by a shutdown command.  The worker will exit but
	// not be reaped because the SIGTERM from the shutdown command will
	// be processed before the SIGCHLD from the worker process exit.
	// Allowing the stack to clean up worker processes is problematic
	// because the collector will be shutdown and the daemonCore
	// object deleted by the time the worker cleanup is attempted.
	app.forker.DeleteAll( );

	if (m_idTimerSendUpdates >= 0) {
		daemonCore->Cancel_Timer(m_idTimerSendUpdates);
		m_idTimerSendUpdates = -1;
	}
	return;
}

int CmDaemon::StashSocket(ReliSock* sock)
{
	if (daemonCore->SocketIsRegistered(sock)) {
		return KEEP_STREAM;
	}

	const char * peer = sock->peer_description();

	MyString msg;
	if (daemonCore->TooManyRegisteredSockets(sock->get_file_desc(),&msg)) {
		dprintf(D_ALWAYS, "WARNING: cannot register TCP update socket from %s: %s\n", peer, msg.c_str());
		return FALSE;
	}

		// Register this socket w/ DaemonCore so we wake up if
		// there's more data to read...
	int rval = daemonCore->Register_Command_Socket(sock, "Update Socket");
	if (rval < 0) {
		dprintf(D_ALWAYS, "Failed to register TCP socket from %s for updates: error %d.\n", peer, rval);
		return FALSE;
	}

	dprintf (D_FULLDEBUG, "Registered TCP socket from %s for updates.\n", peer);
	return KEEP_STREAM;
}


int CmDaemon::receive_update(int command, Stream* stream)
{
	Sock * sock = (Sock*)stream;
	//condor_sockaddr from = sock->peer_addr();

	stats.UpdatesReceived += 1;
	dprintf(D_ALWAYS | (m_LOG_UPDATES ? 0 : D_VERBOSE),
		"Got %s adtype=%s\n", getCommandString(command), AdTypeToString(CollectorCmdToAdType(command)));

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	// process the given command
	ClassAd *ad = getClassAd(sock);
	if ( ! ad) {
		stats.UpdatesFailed += 1;

		dprintf (D_ALWAYS,"Command %d on Sock not follwed by ClassAd (or timeout occured)\n", command);
		sock->end_of_message();
		return FALSE;
	}

	// insert the authenticated user into the ad itself
	const char* username = sock->getFullyQualifiedUser();
	if (username) {
		ad->Assign("AuthenticatedIdentity", username);
	} else {
		// remove it from the ad if it's not authenticated.
		ad->Delete("AuthenticatedIdentity");
	}

	// for these commands, there will also be a private ad.
	ClassAd * adPvt = NULL;
	if (command == UPDATE_STARTD_AD || command == UPDATE_STARTD_AD_WITH_ACK) {
		adPvt = getClassAd(sock);
		if ( ! adPvt) {
			dprintf(D_FULLDEBUG,"\t(Could not get startd's private ad)\n");
		}
	}

	int rval = collect(command, ad, adPvt);
	if (rval < 0) {
		if (rval == -3)
		{
			/* this happens when we get a classad for which a hash key could
				not been made. This occurs when certain attributes are needed
				for the particular catagory the ad is destined for, but they
				are not present in the ad. */
			dprintf (D_ALWAYS, "Received malformed ad from command (%d). Ignoring.\n", command);
		}

		// don't leak the ad(s)!
		delete ad;
		delete adPvt;
		return FALSE;
	}

	// get the end_of_message()
	if ( ! sock->end_of_message()) {
		dprintf(D_FULLDEBUG,"Warning: Command %d; maybe shedding data on eom\n", command);
	}


	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		return this->StashSocket( (ReliSock *)sock );
	}

	// let daemon core clean up the socket
	return TRUE;
}

int CmDaemon::receive_update_expect_ack(int, Stream*)
{
	int rval = 0;
	return rval;
}

int CmDaemon::receive_invalidation(int, Stream*)
{
	int rval = 0;
	return rval;
}

//#define ENABLE_V0_PUT_CLASSAD

// put_ad_v1 is the code used by the condor_collector in V8.2.0
// the projection is a StringList and is expanded into a StringList
//
int CmDaemon::put_ad_v1(ClassAd *ad, Stream* sock, ClassAd & query, bool v0)
{
	StringList expanded_projection;
	StringList *attr_whitelist=NULL;
	StringList externals; // shouldn't have any

	std::string projection = "";
	query.EvalString(ATTR_PROJECTION, ad, projection);

	SimpleList<MyString> projectionList;
	::split_args(projection.c_str(), &projectionList);

	if (projectionList.Number() > 0) {
		projectionList.Rewind();
		MyString attr;
		while (projectionList.Next(attr)) {
			// Get the indirect attributes
			if( ! ad->GetExprReferences(attr.Value(), expanded_projection, externals) ) {
				dprintf(D_FULLDEBUG, "computeProjection failed to parse requested ClassAd expression: %s\n",attr.Value());
			}
		}
		attr_whitelist = &expanded_projection;
	}
	int more=1;
	if ( ! sock->code(more)) {
		return 0;
	}
	if (v0) {
		#ifdef ENABLE_V0_PUT_CLASSAD
		extern int _putClassAd_v0( Stream *sock, classad::ClassAd& ad, bool excludeTypes, bool exclude_private, StringList *attr_whitelist );
		if ( ! _putClassAd_v0(sock, *ad, false, false, attr_whitelist)) {
			return 0;
		}
		#endif
	} else
	{
		classad::References attrs;
		classad::References * whitelist = NULL;
		if (attr_whitelist) {
			const char * attr;
			attr_whitelist->rewind();
			while ((attr = attr_whitelist->next())) { attrs.insert(attr); }
			whitelist = &attrs;
		}
		if ( ! putClassAd(sock, *ad, PUT_CLASSAD_NO_EXPAND_WHITELIST, whitelist)) {
			return 0;
		}
	}
	return 1;
}

int CmDaemon::put_ad_v2(ClassAd &ad, Stream* sock, ClassAd & query)
{
	classad::References * whitelist = NULL;
	classad::References attrs;
	ExprTree * proj_tree = query.Lookup(ATTR_PROJECTION);
	if (proj_tree) {
		classad::Value val;
		if (proj_tree->GetKind() == ExprTree::LITERAL_NODE) {
			classad::Value::NumberFactor factor;
			((classad::Literal*)proj_tree)->GetComponents(val, factor);
		} else {
			if (query.EvalAttr(ATTR_PROJECTION, &ad, val)) {
			}
		}
		const char * projection = NULL;
		if (val.IsStringValue(projection)) {
			StringList proj_list(projection);
			if ( ! proj_list.isEmpty()) {
				proj_list.rewind();
				const char * attr;
				while ((attr = proj_list.next())) {
					ExprTree * tree = ad.Lookup(attr);
					if (tree) {
						attrs.insert(attr); // the node exists, so add it to the final whitelist
						if (tree->GetKind() != ExprTree::LITERAL_NODE) {
							ad.GetInternalReferences(tree, attrs, false);
							PRAGMA_REMIND("need a version of GetInternalReferences that understands that my.foo is an internal ref.")
						}
					}
				}
				whitelist = &attrs;
			}
		}
	}

	int more = 1;
	if ( ! sock->code(more)) {
		return 0;
	}

	int options = PUT_CLASSAD_NO_EXPAND_WHITELIST; //PUT_CLASSAD_NO_PRIVATE | PUT_CLASSAD_NO_TYPES | PUT_CLASSAD_NO_EXPAND_WHITELIST | PUT_CLASSAD_NON_BLOCKING;
	if ( ! putClassAd(sock, ad, options, whitelist)) { return 0; }

	return 1;
}

int CmDaemon::put_ad_v3(ClassAd &ad, Stream* sock, const classad::References * whitelist)
{
	int more = 1;
	if ( ! sock->code(more)) {
		return 0;
	}
	int options = 0;
	if ( ! putClassAd(sock, ad, options, whitelist)) {
		return 0;
	}
	return 1;
}

int CmDaemon::receive_query(int command, Stream* sock)
{
	int rval = 0;
	ClassAd query;  // the query ad

	stats.QueriesReceived += 1;

	int putver = param_integer(MY_SUBSYSTEM "_PUT_VERSION", 0);
	dprintf(D_FULLDEBUG, "Daemon::receive_query(%d,...) called (v%d)\n", command, putver);

	sock->decode();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	bool ep = CondorThreads::enable_parallel(true);
	bool res = ! getClassAd(sock, query) || ! sock->end_of_message();
	CondorThreads::enable_parallel(ep);
	if (res) {
		dprintf(D_ALWAYS,"Failed to receive query on TCP: aborting\n");
		return 0;
	}

		// here we set up a network timeout of a longer duration
	sock->timeout(m_CLIENT_TIMEOUT);

	// Initial query handler
	AdTypes whichAds = CollectorCmdToAdType(command);
	dprintf(D_FULLDEBUG, "Got %s adtype=%s\n", getCommandString(command), AdTypeToString(whichAds));

	UtcTime begin(true);

	// Perform the query
	int cTotalAds = 0;
	List<ClassAd> ads;
	ForkStatus	fork_status = FORK_FAILED;
	if (whichAds != (AdTypes)-1) {
		fork_status = app.forker.NewJob();
		if (FORK_PARENT == fork_status) {
			return 1; // we are the parent - let the child do the work.
		} else {
			// We are the child, or the fork failed, either way we want
			// to actually perform the query now.
			FetchAds (whichAds, query, ads, &cTotalAds);
		}
	}

	UtcTime end_write, end_query(true);

	// send the resultant ads via cedar
	sock->encode();
	ads.Rewind();
	int more = 1;

	// declare this her so we can print it out later
	int cAdsMatched = ads.Length();
	int cAdsSkipped = cTotalAds - cAdsMatched;
	ExprTree * filter_tree = query.Lookup(ATTR_REQUIREMENTS);

	std::string projection;
	bool evaluate_projection = false;
	if ( ! query.LookupString(ATTR_PROJECTION, projection)) {
		projection = "";
		if (query.Lookup(ATTR_PROJECTION)) { evaluate_projection = true; }
	}

	classad::References * whitelist = NULL;
	classad::References attrs;
	if (putver == 3) {
		if ( ! projection.empty()) {
			StringTokenIterator list(projection);
			const std::string * attr;
			while ((attr = list.next_string())) { attrs.insert(*attr); }
			if ( ! attrs.empty()) { whitelist = &attrs; }
		}
	}


		// See if query ad asks for server-side projection
	ClassAd *ad = NULL;
	while ((ad = ads.Next())) {
		int rval = 0;
		if (putver == 3) {
			if (evaluate_projection) {
				projection.clear();
				attrs.clear();
				whitelist = NULL;
				if (query.EvalString(ATTR_PROJECTION, ad, projection) && ! projection.empty()) {
					StringTokenIterator list(projection);
					const std::string * attr;
					while ((attr = list.next_string())) { attrs.insert(*attr); }
					if ( ! attrs.empty()) { whitelist = &attrs; }
				}
			}
			rval = put_ad_v3(*ad, sock, whitelist);
		} else if (putver == 2) {
			rval = put_ad_v2(*ad, sock, query);
		} else {
			rval = put_ad_v1(ad, sock, query, !putver);
		}
		if ( ! rval)
		{
			dprintf (D_ALWAYS, "Error sending query result to client -- aborting\n");
			rval = 0;
			goto FINIS;
		}
	}

	// end of query response ...
	more = 0;
	if ( ! sock->code(more))
	{
		dprintf (D_ALWAYS, "Error sending EndOfResponse (0) to client\n");
	}

	// flush the output
	if ( ! sock->end_of_message())
	{
		dprintf (D_ALWAYS, "Error flushing CEDAR socket\n");
	}
	rval = 1;

	end_write.getTime();

	dprintf (D_ALWAYS,
			 "Query info%d: matched=%d; skipped=%d; query_time=%f; send_time=%f; type=%s; requirements={%s}; peer=%s; projection={%s}\n",
			 putver,
			 cAdsMatched,
			 cAdsSkipped,
			 end_query.difference(begin),
			 end_write.difference(end_query),
			 AdTypeToString(whichAds),
			 ExprTreeToString(filter_tree),
			 sock->peer_description(),
			 projection.c_str());

	// all done; let daemon core will clean up connection
FINIS:
	if (FORK_CHILD == fork_status) {
		app.forker.WorkerDone(); // Never returns
	}
	return rval;
}

// update our daemon ad to the registered collectors
// we do this if we are just a normal daemon (ImTheCollector == false)
//
int CmDaemon::update_collector()
{
	int rval = 0;

	dprintf(D_FULLDEBUG, "Daemon::update_collector() called\n");

	ClassAd * ad = &m_daemonAd;

    daemonCore->publish(ad);
	daemonCore->dc_stats.Publish(*ad);
	daemonCore->monitor_data.ExportData(ad);
	stats.Publish(*ad);

		// Send the ad
	daemonCore->sendUpdates(UPDATE_COLLECTOR_AD, ad, NULL, false);

		// Reset the timer so we don't do another period update until 
	daemonCore->Reset_Timer(m_idTimerSendUpdates, m_UPDATE_INTERVAL, m_UPDATE_INTERVAL);

	return rval;
}

// update our daemon ad to the registered collectors
// we do this if we are just a normal daemon (ImTheCollector == false)
//
int CmDaemon::send_collector_ad()
{
	int rval = 0;

	dprintf(D_FULLDEBUG, "Daemon::send_collector_ad() called\n");

	ClassAd * ad = &m_daemonAd;

    daemonCore->publish(ad);
	daemonCore->dc_stats.Publish(*ad);
	daemonCore->monitor_data.ExportData(ad);
	stats.Publish(*ad);

	/* // this is stolen from collector.cpp...
	// Send the ad
	int num_updated = updateCollectors->sendUpdates(UPDATE_COLLECTOR_AD, ad, NULL, false);
	if ( num_updated != updateCollectors->number() ) {
		dprintf( D_ALWAYS, "Unable to send UPDATE_COLLECTOR_AD to all configured collectors\n");
	}
	*/

	return rval;
}

PRAGMA_REMIND("get rid of this hack variable.")
static ClassAdList s_ads;

int CmDaemon::FetchAds (AdTypes whichAds, ClassAd & query, List<ClassAd>& ads, int * pcTotalAds)
{
	int rval = 0;
	dprintf(D_FULLDEBUG, "Daemon::FetchAds(%d,...) called\n", whichAds);

#if 1
	int cTotalAds = 0;
	bool collector_iter = true;
	CollectionIterator<ClassAd*>* c_ads = collect_get_iter(whichAds);
	if (c_ads) {
		if (c_ads->IsEmpty()) {
			collect_free_iter(c_ads);
			collector_iter = false;

			// read ads from a file to populate the collector list
			if (s_ads.Length() <= 0) {
				MyString filename;
				if (param(filename, MY_SUBSYSTEM "_ADS_FILE")) {
					read_classad_file(filename.c_str(), s_ads, NULL);
				}
			}
			c_ads = new CollectionIterFromClassadList(&s_ads);
		}
	}

	ExprTree * constraint = query.LookupExpr(ATTR_REQUIREMENTS);

	ClassAd * ad;
	c_ads->Rewind();
	while ((ad = c_ads->Next())) {
		++cTotalAds;
		if (constraint) {
			bool val;
			classad::Value result;
			if ( ! EvalExprTree(constraint, ad, NULL, result) || ! result.IsBooleanValueEquiv(val) || ! val) {
				continue; // no match - don't include this ad.
			}
		}
		ads.Append(ad);
	}

	if (collector_iter) {
		collect_free_iter(c_ads);
	} else {
		delete c_ads;
	}
	if (pcTotalAds) { *pcTotalAds = cTotalAds; }

#else
	// for now, read ads from a file to populate the collector list
	if (s_ads.Length() <= 0) {
		MyString filename;
		if (param(filename, MY_SUBSYSTEM "_ADS_FILE")) {
			read_classad_file(filename.c_str(), s_ads, NULL);
		}
	}


	ExprTree * constraint = query.LookupExpr(ATTR_REQUIREMENTS);

	ClassAd * ad;
	s_ads.Rewind();
	while ((ad = s_ads.Next())) {
		if (constraint) {
			bool val;
			classad::Value result;
			if ( ! EvalExprTree(constraint, ad, NULL, result) || ! result.IsBooleanValueEquiv(val) || ! val) {
				continue; // no match - don't include this ad.
			}
		}
		ads.Append(ad);
	}
#endif
	return rval;
}


//-------------------------------------------------------------

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-p <port>]\n",name );
	exit( 1 );
}

//-------------------------------------------------------------

void main_init(int argc, char *argv[])
{
	// handle collector-specific command line args
	if(argc > 2)
	{
		usage(argv[0]);
	}
	
	app.daemon.Init();
	app.daemon.Config();
}

//-------------------------------------------------------------

void main_config()
{
	app.daemon.Config();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	app.daemon.Shutdown(true);
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	app.daemon.Shutdown(false);
	DC_Exit(0);
}

int
main( int argc, const char **argv )
{
	set_mySubSystem(MY_SUBSYSTEM, SUBSYSTEM_TYPE_COLLECTOR);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, const_cast<char**>(argv) );
}
