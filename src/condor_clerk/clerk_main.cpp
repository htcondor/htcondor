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
#include "setenv.h"

#include "clerk_utils.h"
#include "clerk.h"

#include "ccb_server.h"

#if !defined(_XFORM_UTILS_H)
// declare enough of the condor_params structure definitions so that we can define submit hashtable defaults
namespace condor_params {
	typedef struct string_value { char * psz; int flags; } string_value;
	struct key_value_pair { const char * key; const string_value * def; };
}
#endif

// set this to true when this deamon is the collector (it turns off reporting to the collector ;)
bool ImTheCollector = false;
bool ImTheViewCollector = false;
class CCBServer * g_ccb_server = NULL;

// this holds varibles the clerk wants to keep track of
static char ClerkString[] = "Clerk";
static char EmptyString[] = "";
static char FalseString[] = "0";
static char TrueString[] = "1";
static condor_params::string_value SubSysVarDef = { ClerkString, 0 };
static condor_params::string_value MyExeVarDef = { EmptyString, 0 };
static condor_params::string_value CmdLineVarDef = { EmptyString, 0 };
static condor_params::string_value IsMainCollVarDef = { FalseString, 0 };
static MACRO_DEF_ITEM ClerkVarDefaults[] = {
	{ "CmdLine", &CmdLineVarDef },
	{ "IsMainCollector", &IsMainCollVarDef },
	{ "MyExecutable", &MyExeVarDef },
	{ "SUBSYS", &SubSysVarDef },
};
MACRO_DEFAULTS ClerkVarDefaultSet = { COUNTOF(ClerkVarDefaults), ClerkVarDefaults, NULL };

static MACRO_SOURCE CmdLineSrc;
MACRO_EVAL_CONTEXT_EX ClerkEvalCtx;

MACRO_SET ClerkVars = {
	0, 0,
	/* CONFIG_OPT_WANT_META | CONFIG_OPT_KEEP_DEFAULT | */ 0,
	0, NULL, NULL,
	ALLOCATION_POOL(),
	std::vector<const char*>(), // for source files
	&ClerkVarDefaultSet,
	NULL
};

void ClearClerkVars()
{
	ClerkVars.sources.clear();
	ClerkVars.apool.clear();
	if (ClerkVars.table) { memset(ClerkVars.table, 0, sizeof(ClerkVars.table[0]) * ClerkVars.allocation_size); }
	if (ClerkVars.metat) { memset(ClerkVars.metat, 0, sizeof(ClerkVars.metat[0]) * ClerkVars.allocation_size); }
	ClerkVars.size = 0;

	// now re-insert the clerk exe into the ClerkVars table
	insert_source(MyExeVarDef.psz, ClerkVars, CmdLineSrc);
}

//-------------------------------------------------------------

// define single instance objects at global scope as members of a class app.
// this prevents (among other things) problems with the use of an object called daemon on OSX
struct {
	CmDaemon daemon;
	ForkWork forker;
} app;

CmDaemon::CmDaemon()
	: m_initialized(false)
	, m_collectorsToUpdate(NULL)
	, m_idTimerSendUpdates(-1)
	, m_idTimerPopulate(-1)
	, m_idTimerHousekeeping(-1)
	, m_popList(NULL)
		// cached param values
	, m_UPDATE_INTERVAL(15*60)
	, m_CLIENT_TIMEOUT(30)
	, m_QUERY_TIMEOUT(60)
	, m_CLASSAD_LIFETIME(900)
	, m_FORWARD_INTERVAL(m_UPDATE_INTERVAL/3)
	, m_FORWARD_FILTERING(false)
	, m_LOG_UPDATES(false)
	, m_IGNORE_INVALIDATE(false)
	, m_EXPIRE_INVALIDATED_ADS(false)
	, m_HOUSEKEEPING_ON_INVALIDATE(true)
{
}

CmDaemon::~CmDaemon()
{
	delete m_collectorsToUpdate;
	m_collectorsToUpdate = NULL;
}

void CmDaemon::CleanupForwarding()
{
	for (std::vector<vc_entry>::iterator e(vc_list.begin());  e != vc_list.end();  ++e) {
		delete e->collector;
		delete e->sock;
	}
	vc_list.clear();
}

void CmDaemon::CleanupAdTypes()
{
	PopulateAdsInfo * pop;
	while ((pop = PopulateAdsInfo::remove_head(m_popList))) {
		pop->Close(-2);
		delete pop;
	}
}

typedef struct {
	int id;
	AdTypes atype;
} CmdAdTypePair;

// Table to allow quick lookup of adtype from command id.
// THIS MUST BE SORTED BY COMMAND ID!
static const CmdAdTypePair aCommandToAdType[] = {
	{ UPDATE_STARTD_AD,          STARTD_AD },		// 0
	{ UPDATE_SCHEDD_AD,          SCHEDD_AD },		// 1
	{ UPDATE_MASTER_AD,          MASTER_AD },		// 2
	{ UPDATE_CKPT_SRVR_AD,       CKPT_SRVR_AD },	// 4
	{ QUERY_STARTD_ADS,          STARTD_AD },		// 5
	{ QUERY_SCHEDD_ADS,          SCHEDD_AD },		// 6
	{ QUERY_MASTER_ADS,          MASTER_AD },		// 7
	{ QUERY_CKPT_SRVR_ADS,       CKPT_SRVR_AD },	// 9
	{ QUERY_STARTD_PVT_ADS,      STARTD_PVT_AD },	// 10
	{ UPDATE_SUBMITTOR_AD,       SUBMITTOR_AD },	// 11
	{ QUERY_SUBMITTOR_ADS,       SUBMITTOR_AD },	// 12
	{ INVALIDATE_STARTD_ADS,     STARTD_AD },		// 13
	{ INVALIDATE_SCHEDD_ADS,     SCHEDD_AD },		// 14
	{ INVALIDATE_MASTER_ADS,     MASTER_AD },		// 15
	{ INVALIDATE_CKPT_SRVR_ADS,  CKPT_SRVR_AD },	// 17
	{ INVALIDATE_SUBMITTOR_ADS,  SUBMITTOR_AD },	// 18
	{ UPDATE_COLLECTOR_AD,       COLLECTOR_AD },	// 19
	{ QUERY_COLLECTOR_ADS,       COLLECTOR_AD },	// 20
	{ INVALIDATE_COLLECTOR_ADS,  COLLECTOR_AD },	// 21
	{ UPDATE_LICENSE_AD,         LICENSE_AD },		// 42
	{ QUERY_LICENSE_ADS,         LICENSE_AD },		// 43
	{ INVALIDATE_LICENSE_ADS,    LICENSE_AD },		// 44
	{ UPDATE_STORAGE_AD,         STORAGE_AD },		// 45
	{ QUERY_STORAGE_ADS,         STORAGE_AD },		// 46
	{ INVALIDATE_STORAGE_ADS,    STORAGE_AD },		// 47
	{ QUERY_ANY_ADS,             ANY_AD },			// 48
	{ UPDATE_NEGOTIATOR_AD,      NEGOTIATOR_AD },	// 49
	{ QUERY_NEGOTIATOR_ADS,      NEGOTIATOR_AD },	// 50
	{ INVALIDATE_NEGOTIATOR_ADS, NEGOTIATOR_AD },	// 51
	{ UPDATE_HAD_AD,             HAD_AD },			// 55
	{ QUERY_HAD_ADS,             HAD_AD },			// 56
	{ UPDATE_AD_GENERIC,         GENERIC_AD },		// 58
	{ INVALIDATE_ADS_GENERIC,    GENERIC_AD },		// 59
	{ UPDATE_XFER_SERVICE_AD,    XFER_SERVICE_AD },	// 61
	{ QUERY_XFER_SERVICE_ADS,    XFER_SERVICE_AD },	// 62
	{ INVALIDATE_XFER_SERVICE_ADS,XFER_SERVICE_AD },// 63
	{ UPDATE_LEASE_MANAGER_AD,   LEASE_MANAGER_AD },// 64
	{ QUERY_LEASE_MANAGER_ADS,   LEASE_MANAGER_AD },// 65
	{ INVALIDATE_LEASE_MANAGER_ADS,LEASE_MANAGER_AD },// 66
	{ UPDATE_GRID_AD,            GRID_AD },			// 70
	{ QUERY_GRID_ADS,            GRID_AD },			// 71
	{ INVALIDATE_GRID_ADS,       GRID_AD },			// 72
	{ MERGE_STARTD_AD,           STARTD_AD },		// 73
	{ QUERY_GENERIC_ADS,         GENERIC_AD },		// 74
	{ UPDATE_ACCOUNTING_AD,      ACCOUNTING_AD },	// 77
	{ QUERY_ACCOUNTING_ADS,      ACCOUNTING_AD },	// 78
	{ INVALIDATE_ACCOUNTING_ADS, ACCOUNTING_AD },	// 79
};

AdTypes CollectorCmdToAdType(int cmd) {
	const CmdAdTypePair * ptr = BinaryLookup(aCommandToAdType, COUNTOF(aCommandToAdType), cmd);
	if (ptr) return ptr->atype;
	return (AdTypes)-1;
}

int CollectorAdTypeToCmd(AdTypes typ, int op) {
	const char * type_str = NameOf(typ);
	if ( ! type_str) return -1;
	const char * fmt = (op < 0) ? "QUERY_%sS" : ((op & 2) ? "INVALIDATE_%sS" : "UPDATE_%s");
	char cmd_name[128];
	sprintf(cmd_name, fmt, type_str);
	return getCommandNum(cmd_name);
}


// tables of this struct can be used to register a set of commands id's that share a common handler.
typedef struct {
	int cmd;
	DCpermission perm;
} CmdPermPair;

void RegisterHandlers(const CmdPermPair cmds[], size_t num_cmds, CommandHandlercpp handler, const char * descrip, Service * serv)
{
	for (size_t ii = 0; ii < num_cmds; ++ii) {
		const char * name = getCommandString(cmds[ii].cmd);
		daemonCore->Register_CommandWithPayload(cmds[ii].cmd, name, handler, descrip, serv, cmds[ii].perm);
	}
}

void CmDaemon::Init()
{
	dprintf(D_FULLDEBUG, "CmDaemon::Init() called\n");

	dprintf(D_ALWAYS, "CmdLine: %s\n", lookup_macro("CmdLine", ClerkVars, ClerkEvalCtx));
	auto_free_ptr port_id(param("SHARED_PORT_DEFAULT_ID"));
	dprintf(D_ALWAYS, "SHARED_PORT_DEFAULT_ID=%s\n", port_id ? port_id.ptr() : "");

	stats.Init();

	const char * local_name = get_mySubSystem()->getLocalName();
	if (param(m_daemonName, "COLLECTOR_NAME")) {
		if ( ! strchr(m_daemonName.c_str(), '@')) {
			m_daemonName += "@";
			m_daemonName += get_local_fqdn();
		}
	} else {
		m_daemonName = get_local_fqdn();
	}
	if ( ! ImTheCollector) {
		m_daemonName = local_name ? local_name : MY_SUBSYSTEM;
		m_daemonName.lower_case();
		m_daemonName += "@";
		m_daemonName += get_local_fqdn();
	}

	// setup our daemon ad
	ClassAd * ad = &m_daemonAd;
	ad->Clear();
	SetMyTypeName(*ad, COLLECTOR_ADTYPE);
	SetTargetTypeName(*ad, "");
	ad->Assign(ATTR_NAME, m_daemonName);
	if (ImTheCollector || ImTheViewCollector) {
		ad->Assign(ATTR_COLLECTOR_IP_ADDR, global_dc_sinful());
	}

	// Publish all DaemonCore-specific attributes, which also handles
	// COLLECTOR_ATTRS for us.
	daemonCore->publish(ad);
	collect_self(ad);

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
			{ QUERY_ACCOUNTING_ADS,    READ },		// 78;
			{ QUERY_NEGOTIATOR_ADS,    READ },		// 50
			{ QUERY_HAD_ADS,           READ },		// 56
			{ QUERY_XFER_SERVICE_ADS,  READ },		// 62
			{ QUERY_LEASE_MANAGER_ADS, READ },		// 65
		};
		RegisterHandlers(querys, COUNTOF(querys), (CommandHandlercpp)&CmDaemon::receive_query, "receive_query", this);

		static const CmdPermPair invals[] = {
			{ INVALIDATE_STARTD_ADS,        ADVERTISE_STARTD_PERM },	// 13
			{ INVALIDATE_SCHEDD_ADS,        ADVERTISE_SCHEDD_PERM },	// 14
			{ INVALIDATE_SUBMITTOR_ADS,     ADVERTISE_SCHEDD_PERM },	// 18
			{ INVALIDATE_MASTER_ADS,        ADVERTISE_MASTER_PERM },	// 15
			{ INVALIDATE_CKPT_SRVR_ADS,     DAEMON },					// 17
			{ INVALIDATE_LICENSE_ADS,       DAEMON },					// 44
			{ INVALIDATE_COLLECTOR_ADS,     DAEMON },					// 21
			{ INVALIDATE_STORAGE_ADS,       DAEMON },					// 47
			{ INVALIDATE_ACCOUNTING_ADS,    NEGOTIATOR },				// 79
			{ INVALIDATE_NEGOTIATOR_ADS,    NEGOTIATOR },				// 51
			{ INVALIDATE_ADS_GENERIC,       DAEMON },					// 59
			{ INVALIDATE_XFER_SERVICE_ADS,  DAEMON },					// 63
			{ INVALIDATE_LEASE_MANAGER_ADS, DAEMON },					// 66
			{ INVALIDATE_GRID_ADS,          DAEMON },					// 72
		};
		RegisterHandlers(invals, COUNTOF(invals), (CommandHandlercpp)&CmDaemon::receive_invalidation,"receive_invalidation", this);

		static const CmdPermPair updates[] = {
			{ UPDATE_STARTD_AD,             ADVERTISE_STARTD_PERM },	// 0
			{ UPDATE_STARTD_AD_WITH_ACK,    ADVERTISE_STARTD_PERM },	// 60
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
			{ UPDATE_ACCOUNTING_AD,         NEGOTIATOR },				// 77
		};
		RegisterHandlers(updates, COUNTOF(updates), (CommandHandlercpp)&CmDaemon::receive_update,"receive_update", this);

		#if 0 // this now handled by the normal receive_update handler
		daemonCore->Register_CommandWithPayload(UPDATE_STARTD_AD_WITH_ACK, getCommandString(UPDATE_STARTD_AD_WITH_ACK),
			(CommandHandlercpp)&CmDaemon::receive_update_expect_ack, "receive_update_expect_ack",
			this, ADVERTISE_STARTD_PERM);
		#endif

		// load the persistent ads
		//
		if ( ! ImTheViewCollector) {
			auto_free_ptr offline_log(param(ImTheCollector ? "COLLECTOR_PERSISTENT_AD_LOG" : "CLERK_OFFLINE_AD_LOG"));
			// if not found, try depreciated name OFFLINE_LOG
			if ( ! offline_log && ImTheCollector) { offline_log.set(param("OFFLINE_LOG")); }
			if (offline_log) {
				// reload the offline ads. The readloading will happen right here, before we do anything else
				// note that this does NOT populate the collector ads tables, we can't do that until
				// after we config.
				offline_ads_load(offline_log);
			}
		}

		app.forker.Initialize( );
	}

	m_initialized = true; // been through init at least once.
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// load configuration for AdType customization
// customization is
//   CLERK_DYNAMIC_AD_NAMES = <MyType1> <MyType2> [...]
//   CLERK_DYNAMIC_AD_<MyType1> @=end
//     File = <filename>            # MyType1 ads will be populated by parsing this file
//       or
//     Command = <command-line>     # MyType1 ads will be populated by parsing the stdout of this command
//
//     # These variables influence the above file or command reading
//     Filter = <filter-expression> # Only MyType1 ads that match this expression will be added
//     Replace = true | false       # Replace or Merge ads with ads from the file or command
//     DeleteIf = <expr>            # before populating, delete all MyType1 ads that match this expression
//
//     # these variable control storage and query behavior
//     ReKey = <attr1> <attr2>      # Use these attrs as the keys for indexing MyType1 ads
//     Lifetime = <seconds>         # Use <seconds> as the default lifetime for MyType1 ads if the ad has no lifetime attribute
//     Fork = true | false          # fork before replying to queries of this adtype - set to true of there are lots of MyType1 ads
//   @end
//
// If MyType1 is one of the known types, the the pre-defined AdTypes integer value will be used
// If it is not one of the known types, a new AdType integer will be allocated and used
// for the lifetime of the Clerk.
//
void CmDaemon::SetupAdTypes()
{
	CleanupAdTypes();

	// load the dynamic AD table configuration
	StringList tags;
	if ( ! param_and_insert_unique_items ("CLERK_DYNAMIC_AD_NAMES", tags))
		return;

	for (const char * tag = tags.first(); tag; tag = tags.next()) {
		std::string param_name("CLERK_DYNAMIC_AD_"); param_name += tag;
		NOCASE_STRING_MAP smap;
		if ( ! param_and_populate_smap(param_name.c_str(), smap)) {
			dprintf(D_ALWAYS, "dynamic AD info for '%s' was empty\n", tag);
			continue;
		}

		// get config variables that control populating from a file
		//
		AdTypes adtype = AdTypeFromString(tag);
		if (adtype == NO_AD) {
			// Got dynamic info for a new ad type, so we have to register a new ad type
			adtype = collect_register_adtype(tag);
			dprintf(D_FULLDEBUG, "Creating dynamic AD configuration for adtype '%s' (%d)\n", tag, adtype);
		} else {
			dprintf(D_FULLDEBUG, "Loading dynamic AD configuration for adtype '%s' (%d)\n", tag, adtype);
		}

		// config variables that set/change the way the table operates
		//
		bool def_replace = true;
		unsigned int flags = 0;
		if (smap_string("Fork", smap)) {
			flags |= COLL_SET_FORK;
			if (smap_bool("Fork", smap, IsBigTable(adtype))) { flags |= COLL_F_FORK; }
		}
		if (smap_string("Forward", smap)) {
			flags |=  COLL_SET_FORWARD;
			if (smap_bool("Forward", smap, false)) { flags |= COLL_F_FORWARD; }
		}
		if (smap_string("InjectAuth", smap)) {
			flags |= COLL_SET_INJECT_AUTH;
			if (smap_bool("InjectAuth", smap, true)) { flags |= COLL_F_INJECT_AUTH; }
		}
		if (smap_string("AlwaysMerge", smap)) {
			flags |= COLL_SET_ALWAYS_MERGE;
			if (smap_bool("AlwaysMerge", smap, false)) { flags |= COLL_F_ALWAYS_MERGE; }
			def_replace = ! (flags & COLL_F_ALWAYS_MERGE);
		}

		// override the key
		const char * re_key = smap_string("ReKey", smap);
			
		// override the default lifetime
		double lifetime = 1;
		if (smap_number("Lifetime", smap, lifetime)) { flags |= COLL_SET_LIFETIME; }

		// set a persist log
		const char * persist = smap_string("Persist", smap);

		// These control populating the table by reading a file at startup
		//
		const char * filter = NULL;
		const char * file_or_script = NULL;
		bool is_script = false;
		int collect_op = COLLECT_OP_MERGE;
		file_or_script = smap_string("FILE", smap);
		if (file_or_script) {
			is_script = false;
		} else {
			file_or_script = smap_string("Command", smap);
			if (file_or_script) is_script = true;
		}
		filter = smap_string("Filter", smap);
		bool replace_it = smap_bool("Replace", smap, def_replace);
		collect_op = replace_it ? COLLECT_OP_REPLACE : COLLECT_OP_MERGE;

		// set or override settings by ad type
		if (adtype != NO_AD && adtype != ANY_AD) {
			collect_set_adtype(adtype, re_key, lifetime, flags, persist);
		}

		if ( ! smap["DeleteIf"].empty()) {
			ConstraintHolder delete_if(strdup(smap["DeleteIf"].c_str()));
			if ( ! delete_if.Expr()) {
				dprintf(D_ALWAYS, "Invalid DeleteIf expression: %s\n", delete_if.c_str());
			} else {
				int cRemoved = collect_delete(COLLECT_OP_DELETE, adtype, delete_if);
				dprintf(D_ALWAYS, "Removed %d '%s' ads because of DeleteIf expression\n", cRemoved, tag);
			}
		}

		if (file_or_script && adtype != NO_AD) {
			dprintf(D_FULLDEBUG, "Adding PopulateAd %s %s\n\t%s: %s\n\tFilter: %s\n",
				CollectOpName(collect_op), NameOf(adtype),
				is_script ? "Command" : "File", file_or_script,
				filter ? filter : "");
			append_populate(new PopulateAdsInfo(collect_op, adtype, file_or_script, is_script, filter));
		}
	}
}

void CmDaemon::SetupForwarding()
{
	// if we're not the View Collector, let's set something up to forward
	// all of our ads to the view collector.
	CleanupForwarding();

	if (ImTheViewCollector)
		return;

	const char * forwardHostsParamName = ImTheCollector ? "CONDOR_VIEW_HOST" : "CLERK_FORWARD_TO_LIST";
	const char * forwardAdtypesParamName = ImTheCollector ? "CONDOR_VIEW_CLASSAD_TYPES" : "CLERK_FORWARD_CLASSAD_TYPES";

	StringList cvh;
	if (param_and_insert_unique_items(forwardHostsParamName, cvh)) {
		for (const char * vhost = cvh.first(); vhost; vhost = cvh.next()) {
			DCCollector* vhd = new DCCollector(vhost, DCCollector::CONFIG_VIEW);
			Sinful view_addr( vhd->addr() );
			Sinful my_addr( daemonCore->publicNetworkIpAddr() );

			if (my_addr.addressPointsToMe(view_addr)) {
				dprintf(D_ALWAYS, "Not forwarding to View Server %s - self referential\n", vhost);
				delete vhd;
				continue;
			}
			dprintf(D_ALWAYS, "Will forward ads on to View Server %s\n", vhost);

			Sock* vhsock = NULL;
			if (vhd->useTCPForUpdates()) {
				vhsock = new ReliSock();
			} else {
				vhsock = new SafeSock();
			}

			vc_list.push_back(vc_entry());
			vc_list.back().name = ClerkVars.apool.insert(vhost);
			vc_list.back().collector = vhd;
			vc_list.back().sock = vhsock;
		}
	}

	if ( ! vc_list.empty()) {
		// protect against frequent time-consuming reconnect attempts
		view_sock_timeslice.setTimeslice(0.05);
		view_sock_timeslice.setMaxInterval(1200);

		classad::References viewAdTypes;
		if (param_and_insert_attrs(forwardAdtypesParamName, viewAdTypes)) {
			std::string str; str.reserve(100); str = " ";
			for (classad::References::iterator it = viewAdTypes.begin(); it != viewAdTypes.end(); ++it) {
				str += *it;
				str += " ";
			}
			dprintf(D_ALWAYS, "%s configured, will forward ad types: %s\n", forwardAdtypesParamName, str.c_str());
			collect_set_forwarding_adtypes(viewAdTypes);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void CmDaemon::Config()
{
	dprintf(D_FULLDEBUG, "CmDaemon::Config() called\n");
	bool first_time_config = ! m_initialized;
	if ( ! m_initialized) Init();
	ClearClerkVars();

	// handle params for upstream Collector updates
	if (m_idTimerSendUpdates >= 0)  { daemonCore->Cancel_Timer(m_idTimerSendUpdates);  m_idTimerSendUpdates = -1; }
	if (m_idTimerPopulate >= 0)     { daemonCore->Cancel_Timer(m_idTimerPopulate);     m_idTimerPopulate = -1; }
	if (m_idTimerHousekeeping >= 0) { daemonCore->Cancel_Timer(m_idTimerHousekeeping); m_idTimerHousekeeping = -1; }

	m_UPDATE_INTERVAL = param_integer("COLLECTOR_UPDATE_INTERVAL",15*60); // default 15 min
	m_CLIENT_TIMEOUT = param_integer ("CLIENT_TIMEOUT",30);
	m_QUERY_TIMEOUT = param_integer ("QUERY_TIMEOUT",60);
	m_CLASSAD_LIFETIME = param_integer ("CLASSAD_LIFETIME",15*60);
	m_FORWARD_INTERVAL = param_integer("COLLECTOR_FORWARD_INTERVAL", m_CLASSAD_LIFETIME/3);
	m_FORWARD_FILTERING = param_boolean("COLLECTOR_FORWARD_FILTERING", false);
	m_LOG_UPDATES = param_boolean("LOG_UPDATES", false);
	m_IGNORE_INVALIDATE = param_boolean("IGNORE_INVALIDATE", false);
	m_EXPIRE_INVALIDATED_ADS = param_boolean( "EXPIRE_INVALIDATED_ADS", false);
	m_HOUSEKEEPING_ON_INVALIDATE = param_boolean("HOUSEKEEPING_ON_INVALIDATE", true);

	collect_config(param_integer("CLERK_MAX_ADTYPES", NUM_AD_TYPES + 100));

	// we want to preserve the ad sequence counters even as we create a new collector list
	// so that we don't end up thinking we have missed updates.
	DCCollectorAdSequences * adSeq = NULL;
	if (m_collectorsToUpdate) {
		adSeq = m_collectorsToUpdate->detachAdSequences();
		delete m_collectorsToUpdate;
		m_collectorsToUpdate = NULL;
	}

	m_collectorsToUpdate = CollectorList::create(NULL, adSeq);

	const char * lsubsys = get_mySubSystem()->getName();
	if (ImTheCollector || ImTheViewCollector) {
		// gotta worry about maybe sending updates to myself.
		// remove ourselves from the list of collectors to update, since update of ourself
		// through shared port doesn't work anyway (and does cause us to hang until the attempt times out)
		remove_self_from_collector_list(m_collectorsToUpdate, true, ImTheCollector);
		if (ImTheViewCollector) {
			dprintf(D_ALWAYS, "ImTheViewCollector - will not send a collector ad to anyone since I seem to hang when I try.\n");
		} else {
			dprintf(D_ALWAYS, "ImTheCollector=%d, ImTheViewCollector=%d, SUBSYSTEM=%s %d choosing send_collector_ad\n",
				ImTheCollector, ImTheViewCollector, lsubsys, YourString("COLLECTOR")==lsubsys);

			m_idTimerSendUpdates = daemonCore->Register_Timer(1, m_UPDATE_INTERVAL,
				(TimerHandlercpp)&CmDaemon::send_collector_ad, "send_collector_ad",
				this);
		}
	} else {
		dprintf(D_ALWAYS, "Im a Clerk - SUBSYSTEM=%s choosing update_collector\n", lsubsys);
		// just send updates to the collector like any other deamon
		m_idTimerSendUpdates = daemonCore->Register_Timer(1, m_UPDATE_INTERVAL,
			(TimerHandlercpp)&CmDaemon::update_collector, "update_collector",
			this);
	}

	m_idTimerHousekeeping = daemonCore->Register_Timer(m_CLASSAD_LIFETIME, m_CLASSAD_LIFETIME,
			(TimerHandlercpp)&CmDaemon::housekeeping, "housekeeping",
			this);

	// configure/customize ad types
	SetupAdTypes();

	// setup forward to view collector and parent collectors in the tree.
	SetupForwarding();

	// finalize (and log) the collector tables configuration
	collect_finalize_ad_tables();

	// now that we have configured the adtype tables, we can populate the absent ads
	if (first_time_config) {
		offline_ads_populate();
	}

	if (m_popList) {
		int pop_delay = 5;
		int pop_interval = m_UPDATE_INTERVAL;
		m_idTimerPopulate = daemonCore->Register_Timer(pop_delay, pop_interval,
				(TimerHandlercpp)&CmDaemon::populate_collector, "populate_collector",
				this);
	}

	if (ImTheCollector) {
		if (param_boolean("ENABLE_CCB_SERVER",true)) {
			if ( ! g_ccb_server) {
				dprintf(D_ALWAYS, "Enabling CCB Server.\n");
				g_ccb_server = new CCBServer;
			}
			g_ccb_server->InitAndReconfig();
		}
		else if (g_ccb_server) {
			dprintf(D_ALWAYS, "Disabling CCB Server.\n");
			delete g_ccb_server;
			g_ccb_server = NULL;
		}
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
		"Got %s adtype=%s\n", getCommandString(command), NameOf(CollectorCmdToAdType(command)));

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	bool ack_requested = (command == UPDATE_STARTD_AD_WITH_ACK);
	int  collect_op = (command == MERGE_STARTD_AD) ? COLLECT_OP_MERGE : COLLECT_OP_REPLACE;

	// process the given command
	ClassAd *ad = getClassAd(sock);
	if ( ! ad) {
		stats.UpdatesFailed += 1;

		dprintf (D_ALWAYS,"Command %d on Sock not follwed by ClassAd (or timeout occured)\n", command);
		sock->end_of_message();
		return FALSE;
	}

	// for these commands, there will also be a private ad.
	ClassAd * adPvt = NULL;
	if (command == UPDATE_STARTD_AD || command == UPDATE_STARTD_AD_WITH_ACK) {
		adPvt = getClassAd(sock);
		if ( ! adPvt) {
			dprintf(D_FULLDEBUG,"\t(Could not get startd's private ad)\n");
		}
	}

	// get the end_of_message()
	if ( ! sock->end_of_message()) {
		dprintf(D_FULLDEBUG,"Warning: Command %d; maybe shedding data on eom\n", command);
	}

	CollectStatus status;
	AdTypes whichAds = CollectorCmdToAdType(command);

	// GENERIC OR ANY ads may involve adding new adtypes on the fly
	if (whichAds == GENERIC_AD || whichAds == ANY_AD) {
		std::string mytype;
		if (ad->EvaluateAttrString(ATTR_MY_TYPE, mytype)) {
			AdTypes adt = collect_lookup_mytype(mytype.c_str());
			if (adt != NO_AD) whichAds = adt;
		}
	}

	// insert the authenticated user from the socket into the ad itself if the adtype wants it.
	const char* username = sock->getFullyQualifiedUser();
	if (username && ShouldInjectAuthId(whichAds)) {
		ad->Assign(ATTR_AUTHENTICATED_IDENTITY, username);
	} else {
		// remove it from the ad if it's not authenticated.
		ad->Delete(ATTR_AUTHENTICATED_IDENTITY);
	}

	int rval = collect(collect_op, whichAds, ad, adPvt, status);
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

	// send acknowledgement if requested.
	if (ack_requested) {
		sock->encode();
		sock->timeout(5);
		int ok = 1;
		if ( ! sock->code(ok)) {
			dprintf(D_ALWAYS, "failed to send ACK for %s to host %s\n", getCommandString(command), sock->peer_ip_str());
		}
		if ( ! sock->code(ok)) {
			dprintf(D_ALWAYS, "failed to send EOM for %s to host %s\n", getCommandString(command), sock->peer_ip_str());
		}
	}


	if (status.should_forward) {
		time_t now = time(NULL);
		if ( ! status.entry || status.entry->ForwardBeforeTime(now) < now) {
			dprintf(D_FULLDEBUG, "Forwarding ad: type=%s command=%s\n", NameOf(whichAds), getCommandString(command));
			if (send_classad_to_sock(command, ad, adPvt) >= 1) {
				if (status.entry) { status.entry->Forwarded(now); }
			}
		}
	}

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		return this->StashSocket( (ReliSock *)sock );
	}

	// let daemon core clean up the socket
	return TRUE;
}

// forward update commands to parent collectors or view collector
// returns the number of collectors we forwarded to, or 1 if there are no upstream collectors
int CmDaemon::send_classad_to_sock(int command, ClassAd * ad, ClassAd * adPvt)
{
	if ( ! ad) return 0;
	if (vc_list.empty()) return 1;

	int cSent = 0;
	for (std::vector<vc_entry>::iterator e(vc_list.begin());  e != vc_list.end();  ++e) {
		DCCollector* view_coll = e->collector;
		Sock* view_sock = e->sock;
		const char* view_name = e->name;

		bool raw_command = false;
		if (!view_sock->is_connected()) {
			// We must have gotten disconnected.  (Or this is the 1st time.)
			// In case we keep getting disconnected or fail to connect,
			// and each connection attempt takes a long time, restrict
			// what fraction of our time we spend trying to reconnect.
			if (view_sock_timeslice.isTimeToRun()) {
				dprintf(D_ALWAYS,"Connecting to Forward HOST %s\n", view_name);

				// Only run timeslice timer for TCP, since connect on UDP 
				// is instantaneous.
				if (view_sock->type() == Stream::reli_sock) {
					view_sock_timeslice.setStartTimeNow();
				}
				view_coll->connectSock(view_sock,20);
				if (view_sock->type() == Stream::reli_sock) {
					view_sock_timeslice.setFinishTimeNow();
				}

				if (!view_sock->is_connected()) {
					dprintf(D_ALWAYS,"Failed to connect to HOST %s so not forwarding ad.\n", view_name);
					continue;
				}
			} else {
				dprintf(D_FULLDEBUG,"Skipping forwarding of ad to HOST %s, because reconnect is delayed for %us.\n", view_name, view_sock_timeslice.getTimeToNextRun());
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
			view_coll->startCommand(command, view_sock, 20, NULL, NULL, raw_command);
		if ( raw_command == false ) {
			view_sock_timeslice.setFinishTimeNow();
		}

		if (! start_command_result ) {
			dprintf( D_ALWAYS, "Can't send command %d to View Collector %s\n", 
				command, view_name);
			view_sock->end_of_message();
			view_sock->close();
			continue;
		}

		if (ad) {
			if (!putClassAd(view_sock, *ad)) {
				dprintf( D_ALWAYS, "Can't forward classad to View Collector %s\n", view_name);
				view_sock->end_of_message();
				view_sock->close();
				continue;
			}
		}

		// If there's a private startd ad, send that as well.
		if (adPvt) {
			if (!putClassAd(view_sock, *adPvt)) {
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

		++cSent;
	}

	return cSent;
}

#if 0
int CmDaemon::receive_update_expect_ack(int, Stream*)
{
	int rval = 0;
	return rval;
}
#endif

int CmDaemon::receive_invalidation(int command, Stream* stream)
{
	Sock * sock = (Sock*)stream;
	//condor_sockaddr from = sock->peer_addr();

	AdTypes whichAds = CollectorCmdToAdType(command);

	stats.UpdatesReceived += 1;
	dprintf(D_ALWAYS | (m_LOG_UPDATES ? 0 : D_VERBOSE),
		"Got %s adtype=%s\n", getCommandString(command), NameOf(whichAds));

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	// process the given command
	ClassAd *ad = getClassAd(sock);
	if ( ! ad || ! sock->end_of_message()) {
		stats.UpdatesFailed += 1;

		dprintf (D_ALWAYS,"Command %d on Sock not follwed by ClassAd (or timeout occured)\n", command);
		return FALSE;
	}

	// cancel timeout
	sock->timeout(0);

	CollectStatus status;
	if (m_IGNORE_INVALIDATE) {
		dprintf(D_ALWAYS, "Ignoring invalidate (IGNORE_INVALIDATE=TRUE)\n");
	} else {
		int rval = collect(m_EXPIRE_INVALIDATED_ADS ? COLLECT_OP_EXPIRE : COLLECT_OP_DELETE, whichAds, ad, NULL, status);
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
			return FALSE;
		}

		int num_invalidate = rval;
		dprintf (D_ALWAYS, "(Invalidated %d ads)\n", num_invalidate);

			// Suppose lots of ads are getting invalidated and we have no clue
			// why.  That is what the following block of code tries to solve.
		if (num_invalidate > 2 || (num_invalidate > 1 && has_private_adtype(whichAds) == NO_AD)) {
			dprintf(D_ALWAYS, "The invalidation query was this:\n");
			dPrintAd(D_ALWAYS, *ad);
		}
	}

#if 0 // ad forwarding code copied from 8.5.8 collector
	if (viewCollectorTypes) {
		forward_classad_to_view_collector(command,
			ATTR_MY_TYPE,
			cad);
	} else if ((command == UPDATE_STARTD_AD) || (command == UPDATE_SUBMITTOR_AD)) {
		send_classad_to_sock(command, cad);
	}
#endif
	if (status.should_forward) {
		dprintf(D_FULLDEBUG, "Forwarding ad: type=%s command=%s\n", NameOf(whichAds), getCommandString(command));
		send_classad_to_sock(command, ad, NULL);
	}

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		return this->StashSocket( (ReliSock *)sock );
	}

	// let daemon core clean up the socket
	return TRUE;
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
			if( ! ad->GetExprReferences(attr.Value(), &expanded_projection, &externals) ) {
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
							// PRAGMA_REMIND("need a version of GetInternalReferences that understands that my.foo is an internal ref.")
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

	int putver = param_integer("CLERK_PUT_VERSION", 3);
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
	Joinery join(whichAds);
	// for Generic or Any queries, there can be a target type in the request
	if (whichAds == GENERIC_AD || whichAds == ANY_AD) {
		dprintf(D_FULLDEBUG, "Got %s adtype=%s getting target adtype from query:\n", getCommandString(command), NameOf(whichAds));
		dPrintAd(D_FULLDEBUG, query, false);
		std::string target;
		if (query.LookupString(ATTR_TARGET_TYPE, target)) {
			whichAds = collect_lookup_mytype(target.c_str());
			join.targetAds = whichAds;
		}
		if (query.LookupString("LookupType", join.lookupType)) {
			whichAds = collect_lookup_mytype(join.lookupType.c_str());
		}

		const classad::ClassAd* usingAd = ExprTreeIsClassAd(query.Lookup("Using"));
		if (usingAd) {
		#if 1
			join.join_key_ad = new ClassAd();
			ExprTree * jax = join.join_key_ad;
			join.join_parent.Insert(join.lookupType, jax, false);
			ClassAd * jad = join.join_key_ad;

			// join.join_ad.Update(*usingAd);
			bool has_each = false;
			for (classad::ClassAd::const_iterator itr = usingAd->begin(); itr != usingAd->end(); ++itr) {
				classad::ExprTree* tree = SkipExprEnvelope(itr->second);
				std::string fnname;
				std::vector<classad::ExprTree*> fnargs;
				bool is_each = false;
				if (ExprTreeIsFnCall(tree, fnname, fnargs) && (fnname == "each") && fnargs.size() == 1) {
					has_each = is_each = true;
					tree = fnargs[0];
				}
				std::string attr, scope;
				if (ExprTreeIsScopedAttrRef(tree, attr, scope)) {
					//if (scope.empty() || AttrMatch(scope, join.lookupType))
					if (is_each) {
						std::string oneof(attr); oneof += "[ix]";
						jad->AssignExpr(itr->first.c_str(), oneof.c_str());
						MyString foreach;
						foreach.formatstr("size(%s)", attr.c_str());
						join.foreach_size.set(foreach.StrDup());
					} else if ( ! AttrMatch(itr->first, attr)) {
						jad->AssignExpr(itr->first.c_str(), attr.c_str());
					}
				} else {
					// has_key_exprs = true;
					classad::ExprTree * tr = itr->second->Copy();
					jad->Insert(itr->first, tr, false);
				}
			}

			dprintf(D_ALWAYS, "join %susing: %s\n", has_each ? "each " : "", join.foreach_size.c_str());
			if (join.join_key_ad) { dPrintAd(D_ALWAYS, *join.join_key_ad); }
		#else
			std::string keys;
			bool has_each = false;
			bool has_key_exprs = false;
			bool has_odd_key_scope = false;
			for (classad::ClassAd::const_iterator itr = usingAd->begin(); itr != usingAd->end(); ++itr) {
				classad::ExprTree* tree = SkipExprEnvelope(itr->second);
				std::string fnname;
				std::vector<classad::ExprTree*> fnargs;
				if (ExprTreeIsFnCall(tree, fnname, fnargs) && (fnname == "each") && fnargs.size() == 1) {
					has_each = true;
					tree = fnargs[0];
				}
				std::string attr, scope;
				if (ExprTreeIsScopedAttrRef(tree, attr, scope)) {
					if (has_each && join.foreach_size_attr.empty()) {
						join.foreach_size_attr = attr;
					}
					if (scope.empty() || AttrMatch(scope, join.lookupType)) {
						if ( ! keys.empty()) keys += ",";
						keys += attr;
					} else {
						has_odd_key_scope = true;
					}
				} else {
					has_key_exprs = true;
				}
			}
			dprintf(D_ALWAYS, "join using %s'%s'%s\n", has_each ? "each " : "", keys.c_str(), has_key_exprs ? " exprs" : "");
		#endif
		}
	}
	dprintf(D_FULLDEBUG, "Got %s adtype=%s lookup=%s\n", getCommandString(command), NameOf(join.targetAds), NameOf(whichAds));

	// when collector ads are queried, put the latest stats into the self ad before replying.
	ClassAd * self_stats_ad = NULL;
	if (join.targetAds == COLLECTOR_AD) {
		daemonCore->monitor_data.ExportData(&m_daemonAd);

		// update stats in the collector ad before we return it, if an override for the stats
		// was passed, then we update a copy rather than the real one.
		MyString stats_config;
		query.LookupString("STATISTICS_TO_PUBLISH",stats_config);
		if (stats_config.empty()) {
			// use the existing stats config, but refresh the values.
			daemonCore->dc_stats.Publish(m_daemonAd, stats_config.Value());
			stats.Publish(m_daemonAd, stats_config.Value());
		} else if (stats_config != "stored") {
			// populate a temporary ad with the stats override
			self_stats_ad = new ClassAd(m_daemonAd);
			daemonCore->dc_stats.Publish(*self_stats_ad, stats_config.Value());
			stats.Publish(*self_stats_ad, stats_config.Value());
		}
	}

	UtcTime begin(true);

	// Perform the query
	int cTotalAds = 0;
	ConstraintHolder fetch_if;
	List<ClassAd> ads;
	ForkStatus	fork_status = FORK_FAILED;
	if (whichAds != (AdTypes)-1) {
		// only for to handle the query for the "big" tables
		if (IsBigTable(whichAds)) {
			fork_status = app.forker.NewJob();
			if (FORK_PARENT == fork_status) {
				return 1; // we are the parent - let the child do the work.
			}
		}

		ExprTree * filter = query.LookupExpr(ATTR_REQUIREMENTS);
		ConstraintHolder fetch_if(filter ? filter->Copy() : NULL);
		collect_add_absent_clause(fetch_if);

		// small table query, We are the child or or the fork failed
		// either way we want to actually perform the query now.
		FetchAds (whichAds, fetch_if, join, ads, self_stats_ad, &cTotalAds);
	}

	UtcTime end_write, end_query(true);

	// send the resultant ads via cedar
	sock->encode();
	ads.Rewind();
	int more = 1;

	// declare this here so we can print it out later
	int cAdsMatched = ads.Length();
	int cAdsSkipped = cTotalAds - cAdsMatched;

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
		rval = 0;
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
			 (whichAds>=0) ? NameOf(whichAds) : "-1",
			 fetch_if.c_str(),
			 sock->peer_description(),
			 projection.c_str());

	// all done; let daemon core will clean up connection
FINIS:
	delete self_stats_ad; // don't need this anymore
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
// we do this if we are acting as the collector (ImTheCollector == true)
//
int CmDaemon::send_collector_ad()
{
	int rval = 0;

	dprintf(D_FULLDEBUG, "Daemon::send_collector_ad() called\n");

	ClassAd * ad = &m_daemonAd;

	// scan all of the ads and determine the collection totals
	struct CollectionCounters tot;
	calc_collection_totals(tot);

	ad->InsertAttr(ATTR_RUNNING_JOBS,       tot.submitterRunningJobs);
	ad->InsertAttr(ATTR_IDLE_JOBS,          tot.submitterIdleJobs);
	ad->InsertAttr(ATTR_NUM_HOSTS_TOTAL,    tot.machinesTotal);
	ad->InsertAttr(ATTR_NUM_HOSTS_CLAIMED,  tot.machinesClaimed);
	ad->InsertAttr(ATTR_NUM_HOSTS_UNCLAIMED,tot.machinesUnclaimed);
	ad->InsertAttr(ATTR_NUM_HOSTS_OWNER,    tot.machinesOwner);
	ad->InsertAttr("MachineAds", tot.startdNumAds);
	ad->InsertAttr("SubmitterAds", tot.submitterNumAds);

	PRAGMA_REMIND("add the usage stats") // ustatsAccum.publish(), ustatsMonthly.publish()

	daemonCore->publish(ad);
	daemonCore->dc_stats.Publish(*ad);
	daemonCore->monitor_data.ExportData(ad);
	stats.Publish(*ad);
	collect_self(ad);

	int num_updated = m_collectorsToUpdate->sendUpdates(UPDATE_COLLECTOR_AD, ad, NULL, false);
	if (num_updated < m_collectorsToUpdate->number()) {
		dprintf(D_ALWAYS, "Unable to send UPDATE_COLLECTOR_AD to all configured collectors\n");
	}

	//PRAGMA_REMIND("add code to update the world collector")

		// Reset the timer so we don't do another period update until 
	daemonCore->Reset_Timer(m_idTimerSendUpdates, m_UPDATE_INTERVAL, m_UPDATE_INTERVAL);

	return rval;
}

// housekeeping timer callback. expire ads
//
int CmDaemon::housekeeping()
{
	int rval = 0;
	dprintf(D_FULLDEBUG, "Daemon::housekeeping() called\n");

	time_t now = time(NULL);
	if (now == (time_t) -1) {
		dprintf (D_ALWAYS,
				 "Housekeeper:  Error in reading system time --- aborting\n");
		return 0;
	}

	rval = collect_clean(now);

	return rval;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////

int CmDaemon::populate_collector()
{
	int rval = 0;

	dprintf(D_FULLDEBUG, "Daemon::populate_collector() called\n");

	if (m_popList) {
		int cItems = 0;
		CollectStatus status;
		AdTypes whichAds = m_popList->WhichAds();
		dprintf(D_FULLDEBUG, "Populating %s Ads\n", NameOf(whichAds));
		const double max_reading_time = 10.0; // 10 sec max reading time before we go back to the pump.
		double begin_time = _condor_debug_get_time_double();
		double now = begin_time;
		for (;;) {
			ClassAd * ad = m_popList->Next();
			if ( ! ad) {
				break;
			}
			rval = m_popList->collect(ad, status);
			if (rval < 0) {
				if (rval == -3) {
					dprintf (D_ALWAYS, "Ignoring malformed %s ad from %s\n", NameOf(whichAds), m_popList->Source());
				}
				delete ad;
			}
			++cItems;
			now = _condor_debug_get_time_double();
			if ((now - begin_time) > max_reading_time) {
				break;
			}
		}
		if (m_popList->Failed()) {
			dprintf(D_ALWAYS, "Failed to read %s ads from %s\n", NameOf(whichAds), m_popList->Source());
		}
		if (m_popList->Done()) {
			delete PopulateAdsInfo::remove_head(m_popList);
		}
		dprintf(D_FULLDEBUG, "Populated %d %s ads in %.3f seconds (so far), will resume in %d sec\n",
			cItems, NameOf(whichAds), now - begin_time, 2);
		rval = 0;
	}

	if ( ! m_popList) {
		dprintf(D_ALWAYS, "Populating of Clerk from files is complete\n");
		daemonCore->Cancel_Timer(m_idTimerPopulate);
		m_idTimerPopulate = -1;
	} else {
		daemonCore->Reset_Timer(m_idTimerPopulate, 2, 120);
	}

	return rval;
}


int CmDaemon::FetchAds (AdTypes whichAds, ConstraintHolder & fetch_if, Joinery & join, List<ClassAd>& ads, ClassAd * self_stats_ad, int * pcTotalAds)
{
	int rval = 0;
	dprintf(D_FULLDEBUG, "Daemon::FetchAds(%d,...) called\n", whichAds);

	int cTotalAds = 0;
	CollectionIterator<ClassAd*>* c_ads = collect_get_iter(whichAds);
	if ( ! c_ads) {
		return 0;
	}

	ExprTree * filter = fetch_if.Expr();

	// for COLLECTOR or ANY querys, inject a fresh version of our own daemon ad.
	if (whichAds == COLLECTOR_AD || whichAds == ANY_AD || whichAds == NO_AD) {
		++cTotalAds;
		bool insert_self = true;

		ClassAd * ad = self_stats_ad ? self_stats_ad : &m_daemonAd;
		if (filter && ! EvalBool(ad, filter)) {
			insert_self = false;
		}
		if (insert_self) {
			if (join.targetAds != whichAds) {
				ClassAd * ad2 = collect_lookup(join.targetAds, ad);
				if (ad2) ads.Append(ad2);
			} else {
				ads.Append(ad);
			}
		}
	}

	ClassAd * ad;
	c_ads->Rewind();
	while ((ad = c_ads->Next())) {
		++cTotalAds;
		if (filter && ! EvalBool(ad, filter)) {
			continue; // no match - don't include this ad.
		}
		if (join.targetAds != whichAds) {
		#if 1
			std::vector<std::string> keys;
			if (collect_make_keys(join.targetAds, ad, join, keys)) {
				for (std::vector<std::string>::iterator it = keys.begin(); it != keys.end(); ++it) {
					ClassAd * ad2 = collect_lookup(*it);
					if (ad2) ads.Append(ad2);
				}
			}
		#else
			ClassAd * ad2 = collect_lookup(join.targetAds, ad);
			if (ad2) ads.Append(ad2);
		#endif
		} else {
			ads.Append(ad);
		}
	}

	collect_free_iter(c_ads);
	if (pcTotalAds) { *pcTotalAds = cTotalAds; }

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

// one time init that happend before daemon core
// THIS HAPPENDS BEFORE PARAMS ARE LOADED.
void early_init(int argc, const char **argv)
{
	ImTheCollector = false;

	// Figure out whether my subsys should be CLERK or COLLECTOR based on commandline 
	bool got_itc_arg = false;
	bool verbose = false;
	for (int ii = 0; ii < argc; ++ii) {
		if (MATCH == strcmp(argv[ii],"-v")) { verbose = true; }
		if (MATCH == strcmp(argv[ii],"-CLERK")) { ImTheCollector = false; got_itc_arg = true; break; }
		if (MATCH == strcmp(argv[ii],"-COLLECTOR")) { ImTheCollector = true; got_itc_arg = true; break; }
		if (MATCH == strcmp(argv[ii],"-local-name") && argv[ii+1]) {
			StringList viewNames("VIEW_COLLECTOR CONDOR_VIEW VIEW_SERVER");
			if (viewNames.contains_anycase(argv[ii+1])) { ImTheViewCollector = true; got_itc_arg = true; break; }
		}
	}

	// If no command line, figure out whether my subsys should be CLERK or COLLECTOR
	// based on whether the master passed me a SHARED_PORT_DEFAULT_ID
	// that starts with 'collector_' or not.
	if ( ! got_itc_arg) {
		MyString port_id;
		if (GetEnv("_condor_SHARED_PORT_DEFAULT_ID", port_id)) {
			if (MATCH == strcasecmp(port_id.Value(),"collector") || starts_with_ignore_case(port_id.Value(), "collector_")) {
				ImTheCollector = true;
			}
		}
	}
	const char * my_subsys = "CLERK";
	if (ImTheCollector || ImTheViewCollector) { my_subsys = "COLLECTOR"; }
	set_mySubSystem(my_subsys, SUBSYSTEM_TYPE_COLLECTOR);
	ClerkEvalCtx.init(my_subsys);
	if (verbose) { printf("ImTheCollector = %d, ImTheViewCollector=%d, my_subsys = %s\n", ImTheCollector, ImTheViewCollector, my_subsys); }

	// setup the permanent ClerkVar defaults
	SubSysVarDef.psz = const_cast<char*>(my_subsys);
	if (ImTheCollector) { IsMainCollVarDef.psz = TrueString; }
	MyExeVarDef.psz = const_cast<char*>(argv[0]);
	insert_source(argv[0], ClerkVars, CmdLineSrc);

	// setup configuration tables for standard collector ad types
	config_standard_ads(NUM_AD_TYPES + 100);

	// re-construct the command line and insert it into ClerkVar defaults
	int cch = 0;
	for (int ii = 0; ii < argc; ++ii) { cch += strlen(argv[ii]) + 3; }
	char *ptr = (char*)malloc(cch);
	CmdLineVarDef.psz = ptr;
	for (int ii = 0; ii < argc; ++ii) { strcpy(ptr, argv[ii]); ptr += strlen(ptr); *ptr++ = ' '; }
	*ptr = 0;
}

int
main( int argc, const char **argv )
{
	// do one-time init that must happen before daemon core (and before params)
	early_init(argc, argv); 

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, const_cast<char**>(argv) );
}
