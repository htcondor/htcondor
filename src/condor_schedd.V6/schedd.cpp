/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "proc.h"
#include "exit.h"
#include "scheduler.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "classad_helpers.h"
#include "condor_adtypes.h"
#include "condor_string.h"
#include "condor_email.h"
#include "condor_uid.h"
#include "get_daemon_name.h"
#include "write_user_log.h"
#include "access.h"
#include "internet.h"
#include "spooled_job_files.h"
#include "generic_query.h"
#include "condor_query.h"
#include "directory.h"
#include "condor_ver_info.h"
#include "grid_universe.h"
#include "globus_utils.h"
#include "env.h"
#include "dc_schedd.h"  // for JobActionResults class and enums we use  
#include "dc_startd.h"
#include "dc_collector.h"
#include "dc_starter.h"
#include "nullfile.h"
#include "store_cred.h"
#include "file_transfer.h"
#include "basename.h"
#include "user_job_policy.h"
#include "condor_holdcodes.h"
#include "sig_name.h"
#include "../condor_procapi/procapi.h"
#include "condor_distribution.h"
#include "util_lib_proto.h"
#include "status_string.h"
#include "condor_id.h"
#include "named_classad_list.h"
#include "schedd_cron_job_mgr.h"
#include "misc_utils.h"  // for startdClaimFile()
#include "condor_crontab.h"
#include "fs_util.h"
#include "condor_mkstemp.h"
#include "utc_time.h"
#include "condor_getcwd.h"
#include "set_user_priv_from_ad.h"
#include "classad_visa.h"
#include "subsystem_info.h"
#include "authentication.h"
#include "setenv.h"
#include "classadHistory.h"
#include "forkwork.h"
#include "condor_open.h"
#include "schedd_negotiate.h"
#include "filename_tools.h"
#include "ipv6_hostname.h"
#ifdef UNIX
#include "ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#endif
#include <algorithm>
#include "pccc.h"
#include "shared_port_endpoint.h"
#include "condor_auth_passwd.h"
#include "condor_secman.h"
#include "token_utils.h"
#include "jobsets.h"
#include "classad_collection.h"
#include "../condor_sysapi/sysapi.h"

#if defined(WINDOWS) && !defined(MAXINT)
	#define MAXINT INT_MAX
#endif

#define DEFAULT_SHADOW_SIZE 800
#define DEFAULT_JOB_START_COUNT 1

#define SUCCESS 1
#define CANT_RUN 0

char const * const HOME_POOL_SUBMITTER_TAG = "";

extern GridUniverseLogic* _gridlogic;

#include "qmgmt.h"
#include "condor_qmgr.h"
#include "condor_vm_universe_types.h"
#include "enum_utils.h"
#include "credmon_interface.h"

#ifdef WIN32
#define DIR_DELIM_STR "\\"
#else
#define DIR_DELIM_STR "/"
#endif

extern char* Spool;
extern char * Name;
extern char * JobHistoryFileName;
extern char * PerJobHistoryDir;

extern bool        DoHistoryRotation; 
extern bool        DoDailyHistoryRotation; 
extern bool        DoMonthlyHistoryRotation; 
extern filesize_t  MaxHistoryFileSize;
extern int         NumberBackupHistoryFiles;

extern FILE *DebugFP;
extern char *DebugFile;
extern char *DebugLock;

extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;


// the attribute name we use for the "owner" of the job, historically ATTR_OWNER 
// but switching to ATTR_USER as we move to using fully qualified usernames
std::string ownerinfo_attr_name;
const std::string & attr_JobUser = ownerinfo_attr_name;
bool user_is_the_new_owner = false;
bool ignore_domain_mismatch_when_setting_owner = false; // obsolete user_is_the_new_owner knob

// these knobs give fine grained control for choosing between using fully-qualified
// names for ownership checks, or the old behavior of using unqualifed names (i.e. User vs Owner) 
// The ideal future will have ignore=false, warn=true and must_be=false
// But that will require that we be able to run jobs as Nobody for users from a foreign UID_DOMAIN
// Until have have that, the most we can do is ignore=false, warn=false, and must_be=true
// while the most backward compatible mode would be ignore=true, warn=false must_be=false
bool ignore_domain_for_OwnerCheck = true; // do UserCheck2 ignoring the domain part of the socket
bool warn_domain_for_OwnerCheck = true;   // dprintf if UserCheck2 succeeds, but would fail if we weren't ignoring the domain
bool job_owner_must_be_UidDomain = false; // don't allow jobs to be placed by a socket with domain != UID_DOMAIN
bool allow_submit_from_known_users_only = false; // if false, create UseRec for new users when they submit

#ifdef USE_JOB_QUEUE_USERREC
JobQueueUserRec CondorUserRec(CONDOR_USERREC_ID, USERREC_NAME_IS_FULLY_QUALIFIED ? "condor@family" : "condor", "", true);
JobQueueUserRec * get_condor_userrec() { return &CondorUserRec; }

// examine the socket, and if the real owner of the socket is determined to be "condor"
// return the CondorUserRec
JobQueueUserRec * real_owner_is_condor(const Sock * sock) {
	if (sock && USERREC_NAME_IS_FULLY_QUALIFIED) {
		static bool personal_condor = ! is_root();
		const char* real_user = sock->getFullyQualifiedUser();
		const char* owner_part = sock->getOwner();
		#ifdef WIN32
		CompareUsersOpt opt = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN | CASELESS_USER);
		#else
		CompareUsersOpt opt = (CompareUsersOpt)(COMPARE_DOMAIN_FULL | ASSUME_UID_DOMAIN);
		#endif
		if (YourString(CondorUserRec.Name()) == real_user || YourString("condor@child") == real_user ||
			YourString("condor@password") == real_user ||
			YourString("condor_pool@") == real_user ||
		#ifdef WIN32
			YourStringNoCase("LOCAL_SYSTEM") == owner_part || YourStringNoCase("SYSTEM") == owner_part
		#else
			YourString("root") == owner_part ||
			( ! personal_condor && is_same_user(get_condor_username(),real_user,opt,scheduler.uidDomain()) )
		#endif
			) {
			return get_condor_userrec();
		}
	} else
	if (sock) {
		// TODO: check for family session??
		// TODO: will Windows ever see "root" intended to be a super-user?
		static bool personal_condor = ! is_root();
		const char* real_owner = sock->getOwner();
		if (YourString(CondorUserRec.Name()) == real_owner ||
		#ifdef WIN32
			YourStringNoCase("LOCAL_SYSTEM") == real_owner || YourStringNoCase("SYSTEM") == real_owner
		#else
			YourString("root") == real_owner ||
			( ! personal_condor && YourString(get_condor_username()) == real_owner)
		#endif
			) {
			return get_condor_userrec();
		}
	}
	return nullptr;
}
inline const OwnerInfo * EffectiveUserRec(const Sock * sock) {
	if ( ! sock) return &CondorUserRec;	 // commands from inside the schedd
	const char * owner = nullptr;
	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		owner = sock->getFullyQualifiedUser();
	} else {
		owner = sock->getOwner();
	}
	const OwnerInfo * owni = scheduler.lookup_owner_const(owner);
	if (owni) return owni;
	return real_owner_is_condor(sock);
}
inline const char * EffectiveUserName(const Sock * sock) {
	if ( ! sock) return "";
	return sock->getOwner();
}

int init_user_ids(const OwnerInfo * user) {
	if ( ! user) { 
		return 0;
	}
	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		std::string buf;
		const char * owner = name_of_user(user->Name(), buf);
		return init_user_ids(owner, user->NTDomain());
	} else {
		return init_user_ids(user->Name(), user->NTDomain());
	}
}
#else
inline const char * EffectiveUser(const Sock * sock) {
	if (!sock) return "";
#if 0 // def USE_JOB_QUEUE_USERREC - disabling for now, it's ok to use Owner here in 10.6.x
	return sock->getFullyQualifiedUser();
#else
	if (user_is_the_new_owner) {
		return sock->getFullyQualifiedUser();
	} else {
		return sock->getOwner();
	}
#endif
	return "";
}
#endif

// priority records
extern prio_rec *PrioRec;
extern int N_PrioRecs;
extern int grow_prio_recs(int);

bool ReadProxyFileIntoAd( const char *file, const char *owner, ClassAd &x509_attrs );

void cleanup_ckpt_files(int , int , char*);
void send_vacate(match_rec*, int);
void mark_job_stopped(PROC_ID*);
void mark_job_running(PROC_ID*);
void mark_serial_job_running( PROC_ID *job_id );
//int fixAttrUser(JobQueueJob *job, const JOB_ID_KEY & /*jid*/, void *);
bool service_this_universe(int, ClassAd*);
bool jobIsSandboxed( ClassAd* ad );
bool jobPrepNeedsThread( int cluster, int proc );
bool jobCleanupNeedsThread( int cluster, int proc );
int  count_a_job( JobQueueBase *job, const JOB_ID_KEY& jid, void* user);
void mark_jobs_idle();
void load_job_factories();
static void WriteCompletionVisa(ClassAd* ad);

schedd_runtime_probe WalkJobQ_check_for_spool_zombies_runtime;
schedd_runtime_probe WalkJobQ_count_a_job_runtime;
schedd_runtime_probe WalkJobQ_PeriodicExprEval_runtime;
schedd_runtime_probe WalkJobQ_clear_autocluster_id_runtime;
schedd_runtime_probe WalkJobQ_add_runnable_local_jobs_runtime;
schedd_runtime_probe WalkJobQ_fixAttrUser_runtime;
schedd_runtime_probe WalkJobQ_updateSchedDInterval_runtime;

int	WallClockCkptInterval = 0;
int STARTD_CONTACT_TIMEOUT = 45;  // how long to potentially block

void UpdateJobProxyAttrs( PROC_ID job_id, const ClassAd &proxy_attrs )
{
	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd(true, true);

	for (ClassAd::const_iterator attr_it = proxy_attrs.begin(); attr_it != proxy_attrs.end(); ++attr_it)
	{
		std::string attr_value_buf;
		unparse.Unparse(attr_value_buf, attr_it->second);
		SetSecureAttribute(job_id.cluster, job_id.proc, attr_it->first.c_str(), attr_value_buf.c_str());
	}
}

void AuditLogNewConnection( int cmd, Sock &sock, bool failure )
{
	// Quickly determine if we're writing to the audit log and if this
	// is a command we care about for the audit log.
	if ( !IsDebugCategory( D_AUDIT ) ) {
		return;
	}
	switch( cmd ) {
	case ACT_ON_JOBS:
	case SPOOL_JOB_FILES:
	case SPOOL_JOB_FILES_WITH_PERMS:
	case TRANSFER_DATA:
	case TRANSFER_DATA_WITH_PERMS:
	case UPDATE_GSI_CRED:
	case DELEGATE_GSI_CRED_SCHEDD:
	case QMGMT_WRITE_CMD:
	case GET_JOB_CONNECT_INFO:
	case DC_EXCHANGE_SCITOKEN:
	case DC_GET_SESSION_TOKEN:
	case DC_START_TOKEN_REQUEST:
	case DC_FINISH_TOKEN_REQUEST:
	case DC_LIST_TOKEN_REQUEST:
	case DC_APPROVE_TOKEN_REQUEST:
	case DC_AUTO_APPROVE_TOKEN_REQUEST:
	case COLLECTOR_TOKEN_REQUEST:
		break;
	default:
		return;
	}

	const char *schedd_uid_domain = scheduler.uidDomain();
	const char *uid_domain = schedd_uid_domain ? schedd_uid_domain : "";
	if ( (!strcmp( get_condor_username(), sock.getOwner() ) && !strcmp(uid_domain, sock.getDomain())) ||
	     !strcmp( CONDOR_CHILD_FQU, sock.getFullyQualifiedUser() ) ) {
		return;
	}

	const char *cmd_name = getCommandString( cmd );
	const char *sinful = sock.get_sinful_peer();
	const char *method = sock.getAuthenticationMethodUsed();
	const char *unmapped = sock.getAuthenticatedName();
	const char *mapped = sock.getFullyQualifiedUser();
	dprintf( D_AUDIT, sock, "Command=%s, peer=%s\n",
			 cmd_name ? cmd_name : "(null)",
			 sinful ? sinful : "(null)" );

	if ( failure && unmapped == NULL ) {
		const char *methods_tried = sock.getAuthenticationMethodsTried();
		dprintf( D_AUDIT, sock, "Authentication Failed, MethodsTried=%s\n",
				 methods_tried ? methods_tried : "(null)" );
		return;
	}

	dprintf( D_AUDIT, sock, "AuthMethod=%s, AuthId=%s, CondorId=%s\n",
			 method ? method : "(null)",
			 unmapped ? unmapped : "(null)",
			 mapped ? mapped : "(null)" );
	/* Currently, all audited commands require authentication.
	dprintf( D_AUDIT, sock,
			 "triedAuthentication=%s, isAuthenticated=%s, isMappedFQU=%s\n",
			 sock.triedAuthentication() ? "true" : "false",
			 sock.isAuthenticated() ? "true" : "false",
			 sock.isMappedFQU() ? "true" : "false" );
	*/

	if ( failure ) {
		dprintf( D_AUDIT, sock, "Authorization Denied\n" );
	}
}

void AuditLogJobProxy( const Sock &sock, PROC_ID job_id, const char *proxy_file )
{
	dprintf( D_AUDIT, sock, "Received proxy for job %d.%d\n",
			 job_id.cluster, job_id.proc );
	dprintf( D_AUDIT, sock, "proxy path: %s\n", proxy_file );

#if !defined(WIN32)
	X509Credential* proxy_handle = x509_proxy_read( proxy_file );

	if ( proxy_handle == NULL ) {
		dprintf( D_AUDIT|D_ERROR_ALSO, sock, "Failed to read job proxy: %s\n",
				 x509_error_string() );
		return;
	}

	time_t expire_time = x509_proxy_expiration_time( proxy_handle );
	char *proxy_subject = x509_proxy_subject_name( proxy_handle );
	char *proxy_identity = x509_proxy_identity_name( proxy_handle );
	char *proxy_email = x509_proxy_email( proxy_handle );
	char *voname = NULL;
	char *firstfqan = NULL;
	char *fullfqan = NULL;
	extract_VOMS_info( proxy_handle, 0, &voname, &firstfqan, &fullfqan );

	delete proxy_handle;

	dprintf( D_AUDIT, sock, "proxy expiration: %lld\n", (long long)expire_time );
	dprintf( D_AUDIT, sock, "proxy identity: %s\n", proxy_identity );
	dprintf( D_AUDIT, sock, "proxy subject: %s\n", proxy_subject );
	if ( proxy_email ) {
		dprintf( D_AUDIT, sock, "proxy email: %s\n", proxy_email );
	}
	if ( voname ) {
		dprintf( D_AUDIT, sock, "proxy vo name: %s\n", voname );
	}
	if ( firstfqan ) {
		dprintf( D_AUDIT, sock, "proxy first fqan: %s\n", firstfqan );
	}
	if ( fullfqan ) {
		dprintf( D_AUDIT, sock, "proxy full fqan: %s\n", fullfqan );
	}

	free( proxy_subject );
	free( proxy_identity );
	free( proxy_email );
	free( voname );
	free( firstfqan );
	free( fullfqan );
#endif
}

void AuditLogJobProxy( const Sock &sock, ClassAd *job_ad )
{
	PROC_ID job_id;
	std::string iwd;
	std::string proxy_file;
	std::string buff;

	ASSERT( job_ad );

	if ( !job_ad->LookupString( ATTR_X509_USER_PROXY, buff ) || buff.empty() ) {
		return;
	}
	if ( buff[0] == DIR_DELIM_CHAR ) {
		proxy_file = buff;
	} else {
		job_ad->LookupString( ATTR_JOB_IWD, iwd );
		formatstr( proxy_file, "%s%c%s", iwd.c_str(), DIR_DELIM_CHAR, buff.c_str() );
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, job_id.cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, job_id.proc );

	AuditLogJobProxy( sock, job_id, proxy_file.c_str() );
}

size_t UserIdentity::HashFcn(const UserIdentity & index)
{
	return hashFunction(index.m_username) + hashFunction(index.m_auxid);
}

//PRAGMA_REMIND("Owner/user change to take fully qualified user as a single string, remove separate domain string")
UserIdentity::UserIdentity(JobQueueJob& job_ad):
	m_username(job_ad.ownerinfo->Name()),
	m_auxid("")
{
	std::string tmp;
	m_osname = name_of_user(m_username.c_str(), tmp);
	if (job_ad.ownerinfo->NTDomain()) {
		m_osname += '@';
		m_osname += job_ad.ownerinfo->NTDomain();
	}
	ExprTree *tree = const_cast<ExprTree *>(scheduler.getGridParsedSelectionExpr());
	classad::Value val;
	const char *str = NULL;
	if ( tree &&
		 EvalExprToString(tree,&job_ad,NULL,val) && val.IsStringValue(str) )
	{
		m_auxid = str;
	}
}

struct job_data_transfer_t {
	int mode;
	std::vector<PROC_ID> *jobs;
	char peer_version[1]; // We'll malloc enough extra space for this
};

match_rec::match_rec( char const* the_claim_id, char const* p, PROC_ID* job_id,
					  const ClassAd *match, char const *the_user, char const *my_pool,
					  bool is_dedicated_arg ):
	use_sec_session(false),
	claim_id(the_claim_id)
{
	peer = strdup( p );
	origcluster = cluster = job_id->cluster;
	proc = job_id->proc;
	status = M_UNCLAIMED;
	entered_current_status = time(nullptr);
	shadowRec = NULL;
	num_exceptions = 0;
	if( match ) {
		my_match_ad = new ClassAd( *match );
		if( IsDebugLevel(D_MACHINE) ) {
			std::string buf; buf.reserve(1000);
			dprintf( D_MACHINE, "*** ClassAd of Matched Resource ***\n%s", formatAd(buf, *my_match_ad, "\t"));
		}
	} else {
		my_match_ad = NULL;
	}
	user = strdup( the_user );
	if( my_pool ) {
		m_pool = std::string(my_pool);
	}
	this->is_dedicated = is_dedicated_arg;
	allocated = false;
	scheduled = false;
	needs_release_claim = false;
	claim_requester = NULL;
	auth_hole_id = NULL;
	m_startd_sends_alives = false;
	m_claim_pslot = false;

	makeDescription();

	// use_sec_session defaults to false. This means we won't try to do
	// anything with the claimid security session unless we successfully
	// add it to our cache below. Most importantly, we will not try to
	// delete it when this match rec is destroyed. (If we fail to
	// create the session, that may because it already exists, and this
	// is a duplicate match record that will soon be thrown out.)
	if( param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true) ) {
		if( claim_id.secSessionInfo()[0] == '\0' ) {
			dprintf(D_FULLDEBUG,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: did not create security session from claim id, because claim id does not contain session information: %s\n",claim_id.publicClaimId());
		}
		else {
			bool rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
				DAEMON,
				claim_id.secSessionId(),
				claim_id.secSessionKey(),
				claim_id.secSessionInfo(),
				AUTH_METHOD_MATCH,
				EXECUTE_SIDE_MATCHSESSION_FQU,
				peer,
				0,
				nullptr, false );

			if( rc ) {
					// we're good to go; use the claimid security session
				use_sec_session = true;
			}
			if( !rc ) {
				dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create security session for %s, so will try to obtain a new security session\n",claim_id.publicClaimId());
			}
		}
	}

	std::string value;
	param( value, "STARTD_SENDS_ALIVES", "peer" );
	if ( strcasecmp( value.c_str(), "false" ) == 0 ) {
		m_startd_sends_alives = false;
	} else if ( strcasecmp( value.c_str(), "true" ) == 0 ) {
		m_startd_sends_alives = true;
	} else if ( my_match_ad &&
				my_match_ad->LookupString( ATTR_VERSION, value ) ) {
		CondorVersionInfo ver( value.c_str() );
		if ( ver.built_since_version( 7, 5, 4 ) ) {
			m_startd_sends_alives = true;
		} else {
			m_startd_sends_alives = false;
		}
	} else {
		// Don't know the version of the startd, assume false
		// unless STARTER_HANDLES_ALIVES is true.  If STARTER_HANDLES_ALIVES
		// is true, we want to assume startd will send alives otherwise
		// on a schedd restart the schedd will keep sending alives to all
		// reconnected jobs because it will never actually receive an alive
		// from the startd.  The downside of this logic is a v8.3.2+ schedd can
		// no longer successfully reconnect to a startd older than v7.5.5
		// assuming STARTER_HANDLES_ALIVES is set to the default of true.
		// I think this is a reasonable compromise.  -Todd Tannenbaum 10/2014
		m_startd_sends_alives = false;
		if ( param_boolean("STARTER_HANDLES_ALIVES",true) ) {
			m_startd_sends_alives = true;
		}
	}

	keep_while_idle = 0;
	idle_timer_deadline = 0;
}

void
match_rec::makeDescription() {
	m_description = "";
	if( my_match_ad ) {
		my_match_ad->LookupString(ATTR_NAME,m_description);
	}

	if( m_description.length() ) {
		m_description += " ";
	}
	if( IsFulldebug(D_FULLDEBUG) ) {
		m_description += claim_id.publicClaimId();
	}
	else if( peer ) {
		m_description += peer;
	}
	if( user ) {
		m_description += " for ";
		m_description += user;
	}
}

match_rec::~match_rec()
{
	if( peer ) {
		free( peer );
	}
	if( my_match_ad ) {
		delete my_match_ad;
	}
	if( user ) {
		free(user);
	}
	delete auth_hole_id;

		// If we are shuting down, the daemonCore instance will be null
		// and any use of it will cause a core dump.  At best.
	if (!daemonCore) {
		return;
	}

	if( claim_requester.get() ) {
			// misc_data points to this object, so NULL it out, just to be safe
		claim_requester->setMiscDataPtr( NULL );
		claim_requester->cancelMessage();
		claim_requester = NULL;
	}

	if(use_sec_session) {
			// Expire the session after enough time to let the final
			// RELEASE_CLAIM command finish, in case it is still in
			// progress.  This also allows us to more gracefully
			// handle any final communication from the startd that may
			// still be in flight.
		daemonCore->getSecMan()->SetSessionExpiration(claim_id.secSessionId(),time(NULL)+600);
			// In case we get the same claim id again before the slop time
			// expires, mark this session as "lingering" so we know it can
			// be replaced.
		daemonCore->getSecMan()->SetSessionLingerFlag(claim_id.secSessionId());
	}
}


void
match_rec::setStatus( int stat )
{
	if ( stat != status ) {
		entered_current_status = time(0);
	}
	status = stat;
	if( status == M_CLAIMED ) {
			// We have successfully claimed this startd, so we need to
			// release it later.
		needs_release_claim = true;
	}
		// We do NOT send RELEASE_CLAIM while in M_STARTD_CONTACT_LIMBO,
		// because then we could destroy a claim that some other schedd
		// has a prior claim to (e.g. if the negotiator has a stale view
		// of the world and hands out the same claim id to different schedds
		// in different negotiation cycles).  When we are in limbo and the
		// claim object is deleted, cleanup should happen automatically
		// on the startd side anyway, because it will see our REQUEST_CLAIM
		// socket disconnect.
}


ContactStartdArgs::ContactStartdArgs( char const* the_claim_id, char const *extra_claims, const char* sinfulstr, bool is_dedicated ) 
{
	csa_claim_id = strdup( the_claim_id );
	csa_extra_claims = strdup( extra_claims );
	csa_sinful = strdup( sinfulstr );
	csa_is_dedicated = is_dedicated;
}


ContactStartdArgs::~ContactStartdArgs()
{
	free( csa_claim_id );
	free( csa_extra_claims );
	free( csa_sinful );
}

Scheduler::Scheduler() :
	OtherPoolStats(stats),
	m_scheduler_startup(time(NULL)),
    m_adSchedd(NULL),
    m_adBase(NULL),
	GridJobOwners(UserIdentity::HashFcn),
	stop_job_queue( "stop_job_queue" ),
	act_on_job_myself_queue( "act_on_job_myself_queue" ),
	job_is_finished_queue( "job_is_finished_queue", 1 ),
	slotWeightOfJob(0),
	slotWeightGuessAd(0),
	m_use_slot_weights(false),
	m_local_startd_pid(-1),
	m_matchPasswordEnabled(false),
	m_token_requester(&Scheduler::token_request_callback, this)
{
	MyShadowSockName = NULL;
	shadowCommandrsock = NULL;
	shadowCommandssock = NULL;
	QueueCleanInterval = 0; JobStartDelay = 0;
	JobStopDelay = 0;
	JobStopCount = 1;
	RequestClaimTimeout = 0;
	MaxRunningSchedulerJobsPerOwner = INT_MAX;
	MaxJobsRunning = 0;
	AllowLateMaterialize = false;
	NonDurableLateMaterialize = false;
	EnablePersistentOwnerInfo = true;
	EnableJobQueueTimestamps = false;
	MaxMaterializedJobsPerCluster = INT_MAX;
	MaxJobsSubmitted = INT_MAX;
	MaxJobsPerOwner = INT_MAX;
	MaxJobsPerSubmission = INT_MAX;
	NegotiateAllJobsInCluster = false;
	JobsStarted = 0;
	JobsIdle = 0;
	JobsRunning = 0;
	JobsHeld = 0;
	JobsTotalAds = 0;
	JobsFlocked = 0;
	JobsRemoved = 0;
	SchedUniverseJobsIdle = 0;
	SchedUniverseJobsRunning = 0;
	LocalUniverseJobsIdle = 0;
	LocalUniverseJobsRunning = 0;
	LocalUnivExecuteDir = NULL;
	ReservedSwap = 0;
	SwapSpace = 0;
	RecentlyWarnedMaxJobsRunning = true;
	m_need_reschedule = false;
	m_send_reschedule_timer = -1;

	stats.InitMain();

		//
		// ClassAd attribute for evaluating whether to start
		// a local universe job
		// 
	StartLocalUniverse = NULL;

		//
		// ClassAd attribute for evaluating whether to start
		// a scheduler universe job
		// 
	StartSchedulerUniverse = NULL;

		// variables for START_VANILLA_UNIVERSE
	vanilla_start_expr.clear();

	ShadowSizeEstimate = 0;

	NumSubmitters = 0;
	NegotiationRequestTime = 0;

		//gotiator = NULL;
	CondorAdministrator = NULL;
	alive_interval = 0;
	leaseAliveInterval = 500000;	// init to a nice big number
	aliveid = -1;
	ExitWhenDone = FALSE;
	matches = NULL;
	matchesByJobID = NULL;

	numMatches = 0;
	numShadows = 0;
	MinFlockLevel = 0;
	MaxFlockLevel = 0;
	FlockLevel = 0;
	StartJobTimer=-1;
	timeoutid = -1;
	startjobsid = -1;
	periodicid = -1;

	checkContactQueue_tid = -1;
	checkReconnectQueue_tid = -1;
	num_pending_startd_contacts = 0;
	max_pending_startd_contacts = 0;

	act_on_job_myself_queue.
		registerHandlercpp( (ServiceDataHandlercpp)
							&Scheduler::actOnJobMyselfHandler, this );

	stop_job_queue.
		registerHandlercpp( (ServiceDataHandlercpp)
							&Scheduler::actOnJobMyselfHandler, this );

	job_is_finished_queue.
		registerHandlercpp( (ServiceDataHandlercpp)
							&Scheduler::jobIsFinishedHandler, this );

	sent_shadow_failure_email = FALSE;
	_gridlogic = NULL;
	m_parsed_gridman_selection_expr = NULL;
	m_unparsed_gridman_selection_expr = NULL;

	CronJobMgr = NULL;

    m_userlog_file_cache_max = 0;
    m_userlog_file_cache_clear_last = time(NULL);
    m_userlog_file_cache_clear_interval = 60;

	jobThrottleNextJobDelay = 0;

	JobStartCount = 0;
	MaxNextJobDelay = 0;
	JobsThisBurst = -1;

	SwapSpaceExhausted = false;
	MaxShadowsForSwap = 0;

	BadCluster = BadProc = -1;
	NumUniqueOwners = 0;
	NextOwnerId = -1;  // -1 means not yet initialized
	shadowReaperId = -1;
	m_have_xfer_queue_contact = false;
	AccountantName = 0;
	UidDomain = 0;
	From.sin_port =-1;
	From.sin_family = 0;
	From.sin_addr.s_addr = 0;

	m_use_startd_for_local = false;
	cronTabs = 0;
	MaxExceptions = 0;
	m_job_machine_attrs_history_length = 0;

	jobSets = nullptr;
}


Scheduler::~Scheduler()
{
	delete jobSets;
	jobSets = nullptr;

	delete m_adSchedd;
    delete m_adBase;
	if (MyShadowSockName)
		free(MyShadowSockName);
	if( LocalUnivExecuteDir ) {
		free( LocalUnivExecuteDir );
	}
	if ( this->StartLocalUniverse ) {
		free ( this->StartLocalUniverse );
	}
	if ( this->StartSchedulerUniverse ) {
		free ( this->StartSchedulerUniverse );
	}

	if ( CronJobMgr ) {
		delete CronJobMgr;
		CronJobMgr = NULL;
	}

	if( shadowCommandrsock ) {
		if( daemonCore ) {
			daemonCore->Cancel_Socket( shadowCommandrsock );
		}
		delete shadowCommandrsock;
		shadowCommandrsock = NULL;
	}
	if( shadowCommandssock ) {
		if( daemonCore ) {
			daemonCore->Cancel_Socket( shadowCommandssock );
		}
		delete shadowCommandssock;
		shadowCommandssock = NULL;
	}

	if (CondorAdministrator)
		free(CondorAdministrator);
	if (matches) {
		matches->startIterations();
		match_rec *rec;
		std::string id;
		while (matches->iterate(id, rec) == 1) {
			delete rec;
		}
		delete matches;
	}
	if (matchesByJobID) {
		delete matchesByJobID;
	}
	for (const auto &[pid, rec]: shadowsByPid) {
		delete rec;
	}
	for (const auto &[pid, rec]: spoolJobFileWorkers) {
		delete rec;
	}
	if ( checkContactQueue_tid != -1 && daemonCore ) {
		daemonCore->Cancel_Timer(checkContactQueue_tid);
	}

	Submitters.clear();

	if (_gridlogic) {
		delete _gridlogic;
	}
	if ( m_parsed_gridman_selection_expr ) {
		delete m_parsed_gridman_selection_expr;
	}
	if ( m_unparsed_gridman_selection_expr ) {
		free(m_unparsed_gridman_selection_expr);
	}

    userlog_file_cache_clear(true);

		//
		// Delete CronTab objects
		//
	if ( this->cronTabs ) {
		this->cronTabs->startIterations();
		CronTab *current;
		while ( this->cronTabs->iterate( current ) >= 1 ) {
			if ( current ) delete current;
		}
		delete this->cronTabs;
	}

	delete slotWeightOfJob;
	delete slotWeightGuessAd;
}


bool
Scheduler::JobCanFlock(classad::ClassAd &job_ad, const std::string &pool) {
		// We always trust the home pool...
	if (pool == HOME_POOL_SUBMITTER_TAG) {return true;}

		// Determine whether we should flock this cluster with this pool.
	if (m_include_default_flock_param) {
		for (const auto &flock_entry : FlockPools) {
			if (flock_entry == pool) {
				return true;
			}
		}
	}

	std::string flock_targets;
	if (job_ad.EvaluateAttrString(ATTR_FLOCK_TO, flock_targets)) {
		for (auto& flock_entry: StringTokenIterator(flock_targets)) {
			if (!strcasecmp(pool.c_str(), flock_entry.c_str())) {
				return true;
			}
		}
	}
	return false;
}


bool
Scheduler::SetupNegotiatorSession(unsigned duration, const std::string &pool, std::string &capability)
{
	if (!m_matchPasswordEnabled) {
		return false;
	}

	// Internally, the negotiator session is serialized as a ClaimID.
	// Obviously, there is no Claim here -- but it's simpler to keep
	// this aligned with the rest of the infrastructure.

	// ClaimId string is of the form:
	// (keep this in sync with condor_claimid_parser)
	// "<ip:port>#schedd_bday#sequence_num#cookie"

	m_negotiator_seq++;

	std::string id;

	formatstr( id, "%s#%ld#%lu", daemonCore->publicNetworkIpAddr(),
	           m_scheduler_startup, (long unsigned)m_negotiator_seq);

	auto keybuf = std::unique_ptr<char, decltype(free)*>{
		Condor_Crypt_Base::randomHexKey(SEC_SESSION_KEY_LENGTH_V9),
		free
	};
	if (!keybuf.get()) {
		return false;
	}

	const char *session_info = "[Encryption=\"YES\";Integrity=\"YES\";ValidCommands=\"416\"]";
	std::string exported_session_info;
	classad::ClassAd policy_ad;
	policy_ad.InsertAttr(ATTR_REMOTE_POOL, pool);

	bool retval = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
		NEGOTIATOR,
		id.c_str(),
		keybuf.get(),
		session_info,
		AUTH_METHOD_MATCH,
		NEGOTIATOR_SIDE_MATCHSESSION_FQU,
		nullptr,
		duration,
		&policy_ad, true
	);
	if ( retval ) {
		retval = daemonCore->getSecMan()->ExportSecSessionInfo(id.c_str(), exported_session_info);
	}
	if ( retval ) {
		ClaimIdParser cid(id.c_str(), exported_session_info.c_str(), keybuf.get());
		capability = cid.claimId();
	}
	return retval;
}

	// Largely, this is the same as the negotiator session logic.
bool
Scheduler::SetupCollectorSession(unsigned duration, std::string &capability)
{
	if (!m_matchPasswordEnabled) {return false;}
		// We reuse the negotiator sequence -- this way we don't have
		// overlaps in the session names.
	m_negotiator_seq++;

	std::string id;
	formatstr( id, "%s#%ld#%lu", daemonCore->publicNetworkIpAddr(),
	           m_scheduler_startup, static_cast<long unsigned>(m_negotiator_seq));

	auto keybuf = std::unique_ptr<char, decltype(free)*>{
		Condor_Crypt_Base::randomHexKey(SEC_SESSION_KEY_LENGTH_V9),
		free
	};
	if (!keybuf.get()) {
		return false;
	}

	auto session_info = "[Encryption=\"YES\";Integrity=\"YES\";ValidCommands=\"523\"]";
	std::string exported_session_info;
	classad::ClassAd policy_ad;
	policy_ad.InsertAttr("ScheddSession", true);
	bool retval = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
		ALLOW,
		id.c_str(),
		keybuf.get(),
		session_info,
		AUTH_METHOD_MATCH,
		COLLECTOR_SIDE_MATCHSESSION_FQU,
		nullptr,
		duration,
		&policy_ad, true
	);
	if ( retval ) {
		retval = daemonCore->getSecMan()->ExportSecSessionInfo(id.c_str(), exported_session_info);
	}
	if ( retval ) {
		ClaimIdParser cid(id.c_str(), exported_session_info.c_str(), keybuf.get());
		capability = cid.claimId();
	}
	return retval;
}


// If a job has been spooling for 12 hours,
// It may well be that the remote condor_submit died
// So we kill this job
int check_for_spool_zombies(JobQueueJob *job, const JOB_ID_KEY & /*jid*/, void *)
{
	int cluster = job->jid.cluster;
	int proc = job->jid.proc;

	//PRAGMA_REMIND("tj asks: is it really ok that this function will look inside an uncommitted transaction to get the job status?")

	int hold_status = 0;
	if( GetAttributeInt(cluster,proc,ATTR_JOB_STATUS,&hold_status) >= 0 ) {
		if(hold_status == HELD) {
			int hold_reason_code = 0;
			if( GetAttributeInt(cluster,proc,ATTR_HOLD_REASON_CODE,
					&hold_reason_code) >= 0) {
				if(hold_reason_code == CONDOR_HOLD_CODE::SpoolingInput) {
					dprintf( D_FULLDEBUG, "Job %d.%d held for spooling. "
						"Checking how long...\n",cluster,proc);
					time_t stage_in_start = 0;
					int ret = GetAttributeInt(cluster,proc,ATTR_STAGE_IN_START,
							&stage_in_start);
					if(ret >= 0) {
						time_t now = time(nullptr);
						time_t diff = now - stage_in_start;
						dprintf( D_FULLDEBUG, "Job %d.%d on hold for %lld seconds.\n",
							cluster,proc,(long long)diff);
						if(diff > 60*60*12) { // 12 hours is sufficient?
							dprintf( D_FULLDEBUG, "Aborting job %d.%d\n",
								cluster,proc);
							abortJob(cluster,proc,
								"Spooling is taking too long",true);
						}
					}
					if(ret < 0) {
						dprintf( D_FULLDEBUG, "Attribute %s not set in %d.%d. "
							"Set it.\n", ATTR_STAGE_IN_START,cluster,proc);
						time_t now = time(0);
						SetAttributeInt(cluster,proc,ATTR_STAGE_IN_START,now);
					}
				}
			}
		}
	}
	return 0;
}

void
Scheduler::timeout( int /* timerID */ )
{
	static bool min_interval_timer_set = false;
	static bool walk_job_queue_timer_set = false;

		// If we are called too frequently, delay.
	SchedDInterval.expediteNextRun();
	unsigned int time_to_next_run = SchedDInterval.getTimeToNextRun();

	//dprintf_on_function_exit on_exit(true, D_FULLDEBUG, "Scheduler::timeout() InWalk=%d time_to_next_run = %u\n", InWalkJobQueue(), time_to_next_run );

	if ( time_to_next_run > 0 ) {
		if (!min_interval_timer_set) {
			dprintf(D_FULLDEBUG,"Setting delay until next queue scan to %u seconds\n",time_to_next_run);

			daemonCore->Reset_Timer(timeoutid,time_to_next_run,1);
			min_interval_timer_set = true;
		}
		return;
	}

	if( InWalkJobQueue() ) {
			// WalkJobQueue is not reentrant.  We must be getting called from
			// inside something else that is iterating through the queue,
			// such as PeriodicExprHandler().
		if( !walk_job_queue_timer_set ) {
			dprintf(D_ALWAYS,"Delaying next queue scan until current operation finishes.\n");
			daemonCore->Reset_Timer(timeoutid,0,1);
			walk_job_queue_timer_set = true;
		}
		return;
	}

	walk_job_queue_timer_set = false;
	min_interval_timer_set = false;
	SchedDInterval.setStartTimeNow();

    // keep the log file cache from accumulating defunct user log file mappings
    userlog_file_cache_clear();

	count_jobs();

	clean_shadow_recs();

		// Spooling should not take too long
	WalkJobQueue(check_for_spool_zombies);

	/* Call preempt() if we are running more than max jobs; however, do not
	 * call preempt() here if we are shutting down.  When shutting down, we have
	 * a timer which is progressively preempting just one job at a time.
	 */
	if( (numShadows > MaxJobsRunning) && (!ExitWhenDone) ) {
		dprintf( D_ALWAYS, 
				 "Preempting %d jobs due to MAX_JOBS_RUNNING change\n",
				 (numShadows - MaxJobsRunning) );
		preempt( numShadows - MaxJobsRunning );
		m_need_reschedule = false;
	}

	if( LocalUniverseJobsIdle > 0 || SchedUniverseJobsIdle > 0 ) {
		this->calculateCronTabSchedules();
		AddRunnableLocalJobs();
	}

	/* Call function that will start using our local startd if it
	   appears we have lost contact with the pool negotiator
	 */
	if ( claimLocalStartd() ) {
		dprintf(D_ALWAYS,"Negotiator gone, trying to use our local startd\n");
	}

		// In case we were not able to drain the startd contact queue
		// because of too many registered sockets or something, give it
		// a spin.
	scheduler.rescheduleContactQueue();

	SchedDInterval.setFinishTimeNow();

	if( m_need_reschedule ) {
		sendReschedule();
	}

	/* Reset our timer */
	time_to_next_run = SchedDInterval.getTimeToNextRun();
	daemonCore->Reset_Timer(timeoutid,time_to_next_run,1);
}

void
Scheduler::check_claim_request_timeouts()
{
	if(RequestClaimTimeout > 0) {
		matches->startIterations();
		match_rec *rec;
		while(matches->iterate(rec) == 1) {
			if(rec->status == M_STARTD_CONTACT_LIMBO) {
				time_t time_left = rec->entered_current_status + \
				                 RequestClaimTimeout - time(NULL);
				if(time_left < 0) {
					dprintf(D_ALWAYS,"Timed out requesting claim %s after REQUEST_CLAIM_TIMEOUT=%d seconds.\n",rec->description(),RequestClaimTimeout);
						// We could just do send_vacate() here and
						// wait for the startd contact socket to call
						// us back when the connection closes.
						// However, this means that if send_vacate()
						// fails (e.g. times out setting up security
						// session), we keep calling it over and over,
						// timing out each time, without ever
						// removing the match.
					DelMrec(rec);
				}
			}
		}
	}
}

/*
  Helper method to create a submitter ad, called by Scheduler::count_jobs().
  Given an index owner_num in the Owners array and a flock_level, insert a common
  set of submitter ad attributes into pAd.
  Return true if attributes filled in, false if not (because this submitter should
  no longer flock and/or be advertised).
*/
bool
Scheduler::fill_submitter_ad(ClassAd & pAd, const SubmitterData & Owner, const std::string &pool_name, int flock_level)
{
	const bool publish_stats_to_flockers = false;
	const SubmitterCounters & Counters = Owner.num;

	int jobs_idle = pool_name.empty() ? Counters.JobsIdle : 0;
	if ((Owner.FlockLevel >= flock_level) || (flock_level == INT_MAX)) {
		int weighted_jobs_idle = pool_name.empty() ? Counters.WeightedJobsIdle : 0;
		const auto iter = Owner.flock.find(pool_name);
		if (iter != Owner.flock.end()) {
			jobs_idle = iter->second.JobsIdle;
			weighted_jobs_idle = iter->second.WeightedJobsIdle;
		}
		pAd.Assign(ATTR_IDLE_JOBS, jobs_idle);
		pAd.Assign(ATTR_WEIGHTED_IDLE_JOBS, weighted_jobs_idle);
	} else if (Owner.OldFlockLevel >= flock_level ||
				Counters.JobsRunning > 0) {
		pAd.Assign(ATTR_IDLE_JOBS, (int)0);
		pAd.Assign(ATTR_WEIGHTED_IDLE_JOBS, (int)0);
	} else {
		// if we're no longer flocking with this pool and
		// we're not running jobs in the pool, then don't send
		// an update
		return false;
	}

	int JobsRunningHere = Counters.JobsRunning;
	double WeightedJobsRunningHere = Counters.WeightedJobsRunning;
	int JobsRunningElsewhere = Counters.JobsFlocked;

	if (flock_level > 0) {
		const auto &iter = Owner.flock.find(pool_name);
		if (iter != Owner.flock.end()) {
			JobsRunningHere = iter->second.JobsRunning;
			WeightedJobsRunningHere = iter->second.WeightedJobsRunning;
		} else {
			JobsRunningHere = 0;
			WeightedJobsRunningHere = 0;
		}
		JobsRunningElsewhere = (Counters.JobsRunning + Counters.JobsFlocked) - JobsRunningHere;

			// For flock targets, if we have neither idle nor running jobs for that pool,
			// simply do not advertise.
		if ((JobsRunningHere == 0) && (jobs_idle == 0)) {
			return false;
		}
	}

	pAd.Assign(ATTR_RUNNING_JOBS, JobsRunningHere);

	pAd.Assign(ATTR_WEIGHTED_RUNNING_JOBS, WeightedJobsRunningHere);

	pAd.Assign(ATTR_RUNNING_LOCAL_JOBS, Counters.LocalJobsIdle);

	pAd.Assign(ATTR_IDLE_LOCAL_JOBS, Counters.LocalJobsRunning);

	pAd.Assign(ATTR_RUNNING_SCHEDULER_JOBS, Counters.SchedulerJobsRunning);

	pAd.Assign(ATTR_IDLE_SCHEDULER_JOBS, Counters.SchedulerJobsIdle);

	pAd.Assign(ATTR_HELD_JOBS, Counters.JobsHeld);

	pAd.Assign(ATTR_FLOCKED_JOBS, JobsRunningElsewhere);

	if (publish_stats_to_flockers || flock_level < 1) {
		//PRAGMA_REMIND("tj: move OwnerStats into OwnerData objects.")
		ScheddOtherStats * OwnerStats = OtherPoolStats.Lookup("Owner");
		bool owner_stats_set = false;
		if (OwnerStats && ! OwnerStats->sets.empty()) {
			std::map<std::string, ScheddOtherStats *>::iterator fnd = OwnerStats->sets.find(Owner.Name());
			if (fnd != OwnerStats->sets.end()) {
				ScheddOtherStats * po2 = fnd->second;
				po2->stats.Pool.Publish(pAd, IF_VERBOSEPUB | IF_RECENTPUB | IF_NONZERO);
				owner_stats_set = true;
			}
		}
		// if we didn't successfully set new owner-stats, then we have to clear them
		// because the input pAd is re-used for muliple Submitters.
		if (OwnerStats && ! owner_stats_set) {
			OwnerStats->stats.Pool.Unpublish(pAd);
		}

		// Publish per-user transfer stats if they exist, remove the relevant attributes if they do not.
		//PRAGMA_REMIND("TJ: this only works if TRANSFER_QUEUE_USER_EXPR is strcat(\"Owner_\",Owner)")
		m_xfer_queue_mgr.publish_user_stats(&pAd, Owner.Name(), IF_VERBOSEPUB | IF_RECENTPUB);
	}

	std::string str;
	if ( param_boolean("USE_GLOBAL_JOB_PRIOS",false) ) {
		int max_entries = param_integer("MAX_GLOBAL_JOB_PRIOS",500);
		int num_prios = (int)Owner.PrioSet.size();
		if (num_prios > max_entries) {
			pAd.Assign(ATTR_JOB_PRIO_ARRAY_OVERFLOW, num_prios);
		} else {
			// if no overflow, do not advertise ATTR_JOB_PRIO_ARRAY_OVERFLOW
			pAd.Delete(ATTR_JOB_PRIO_ARRAY_OVERFLOW);
		}
		// reverse iterator to go high to low prio
		std::set<int>::const_reverse_iterator rit;
		int num_entries = 0;
		for (rit = Owner.PrioSet.rbegin();
			 rit != Owner.PrioSet.rend() && num_entries < max_entries;
			 ++rit)
		{
			if ( !str.empty() ) {
				str += ",";
			}
			str += std::to_string( *rit );
			num_entries++;
		}
		pAd.Assign(ATTR_JOB_PRIO_ARRAY, str);
	}

	if (user_is_the_new_owner) {
		pAd.Assign(ATTR_NAME, Owner.Name());
	} else {
		formatstr(str, "%s@%s", Owner.Name(), AccountingDomain.c_str());
		pAd.Assign(ATTR_NAME, str);
	}

	return true;
}

void Scheduler::userlog_file_cache_clear(bool force) {
    time_t t = time(NULL);
    if (!force && ((t - m_userlog_file_cache_clear_last) < m_userlog_file_cache_clear_interval)) return;

    dprintf(D_FULLDEBUG, "Clearing userlog file cache\n");

    for (WriteUserLog::log_file_cache_map_t::iterator e(m_userlog_file_cache.begin());  e != m_userlog_file_cache.end();  ++e) {
        delete e->second;
    }
    m_userlog_file_cache.clear();

    m_userlog_file_cache_clear_last = t;
}


void Scheduler::userlog_file_cache_erase(const int& cluster, const int& proc) {
    // only if caching is turned on
    if (m_userlog_file_cache_max <= 0) return;

	ClassAd* ad = GetJobAd(cluster, proc);
    if (NULL == ad) return;

    std::string userlog_name;
    std::string dagman_log_name;
    std::vector<char const*> log_names;

    // possible userlog file names associated with this job
    if (getPathToUserLog(ad, userlog_name)) log_names.push_back(userlog_name.c_str());
    if (getPathToUserLog(ad, dagman_log_name, ATTR_DAGMAN_WORKFLOW_LOG)) log_names.push_back(dagman_log_name.c_str());

    for (std::vector<char const*>::iterator j(log_names.begin());  j != log_names.end();  ++j) {

        // look for file name in the cache
        WriteUserLog::log_file_cache_map_t::iterator f(m_userlog_file_cache.find(*j));
        if (f == m_userlog_file_cache.end()) continue;

        // remove this job from the reference set:
        f->second->refset.erase(std::make_pair(cluster, proc));
        if (f->second->refset.empty()) {
            // if that was the last job referring to this log file, remove it from the cache
            dprintf(D_FULLDEBUG, "Erasing entry for %s from userlog file cache\n", *j);
            delete f->second;
            m_userlog_file_cache.erase(f);
        }
    }
}


void
Scheduler::sumAllSubmitterData(SubmitterData &all) {
	// hack to allow this to work with unmodified negotiator
	all.name = "AllSubmittersAt"; 
	all.isOwnerName = false;
	all.FlockLevel = -1;
	all.num.clear_job_counters();

	// Cons up a fake submitter ad representing all demand
	for (auto it = Submitters.begin(); it != Submitters.end(); ++it) {
		SubmitterData &sd = it->second;
		all.num.JobsRunning 			+= sd.num.JobsRunning;
		all.num.JobsIdle				+= sd.num.JobsIdle;
		all.num.WeightedJobsRunning		+= sd.num.WeightedJobsRunning;
		all.num.WeightedJobsIdle		+= sd.num.WeightedJobsIdle;
		all.num.JobsHeld				+= sd.num.JobsHeld;
		all.num.JobsFlocked				+= sd.num.JobsFlocked;
		all.num.SchedulerJobsRunning	+= sd.num.SchedulerJobsRunning;
		all.num.SchedulerJobsIdle		+= sd.num.SchedulerJobsIdle;
		all.num.LocalJobsRunning		+= sd.num.LocalJobsRunning;
		all.num.LocalJobsIdle			+= sd.num.LocalJobsIdle;;
		all.num.Hits					+= sd.num.Hits;
	}
}

void
Scheduler::updateSubmitterAd(SubmitterData &SubDat, ClassAd &pAd, DCCollector *col, int flock_level, time_t time_now) {
		const char * owner_name = SubDat.Name();
		// only flocked collectors get names
		const char * col_name = col ? col->name() : "";

		if (SubDat.num.Hits == 0 && SubDat.absentUpdateSent) {
			dprintf( D_FULLDEBUG, "Skipping send ad to collectors for %s Hit=%d Tot=%d Idle=%d Run=%d\n",
				owner_name, SubDat.num.Hits, SubDat.num.JobsCounted, SubDat.num.JobsIdle, SubDat.num.JobsRunning );
			return;
		}
		if (!fill_submitter_ad(pAd, SubDat, col_name, flock_level)) return;

#ifdef UNIX
		if (col == nullptr) {
			// only fire plugin for main collector updates
			ScheddPluginManager::Update(UPDATE_SUBMITTOR_AD, &pAd);
		}
#endif

		if (user_is_the_new_owner) {
			SubmitterMap.AddSubmitter(col_name, SubDat.name, time_now);
		} else {
			// Note the submitter; the negotiator uses user@uid_domain when
			// referring the submitter when it requests to negotiate.
			SubmitterMap.AddSubmitter(col_name, SubDat.name + "@" + AccountingDomain, time_now);
		}
		// Update non-flock collectors
		int num_updates = 0;
		if (col) {
			DCCollectorAdSequences & adSeq = daemonCore->getUpdateAdSeq();
			num_updates = col->sendUpdate( UPDATE_SUBMITTOR_AD, &pAd, adSeq, NULL, true );
		} else {
			num_updates = daemonCore->sendUpdates(UPDATE_SUBMITTOR_AD, &pAd, NULL, true);
			SubDat.lastUpdateTime = time_now;
			SubDat.absentUpdateSent = (SubDat.num.Hits == 0);
		}
		dprintf( D_FULLDEBUG, "Sent ad to %d collectors for %s Hit=%d Tot=%d Idle=%d Run=%d\n",
			num_updates, owner_name, SubDat.num.Hits, SubDat.num.JobsCounted, SubDat.num.JobsIdle, SubDat.num.JobsRunning );
}


/*
** Examine the job queue to determine how many CONDOR jobs we currently have
** running, and how many individual users own them.
*/
int
Scheduler::count_jobs()
{
	ClassAd * cad = m_adSchedd;

	//dprintf_on_function_exit on_exit(true, D_FULLDEBUG, "count_jobs()\n");

	 // copy owner data to old-owners table
	time_t AbsentSubmitterLifetime = param_integer("ABSENT_SUBMITTER_LIFETIME", 60*60*24*7); // 1 week.
	time_t AbsentSubmitterUpdateRate = param_integer("ABSENT_SUBMITTER_UPDATE_RATE", 60*5); // 5 min
	time_t AbsentOwnerLifetime = param_integer("ABSENT_OWNER_LIFETIME", 60*5);

	JobsRunning = 0;
	JobsIdle = 0;
	JobsHeld = 0;
	JobsTotalAds = 0;
	JobsFlocked = 0;
	JobsRemoved = 0;
	SchedUniverseJobsIdle = 0;
	SchedUniverseJobsRunning = 0;
	LocalUniverseJobsIdle = 0;
	LocalUniverseJobsRunning = 0;
	stats.JobsRunning = 0;
	stats.JobsRunningRuntimes = 0;
	stats.JobsRunningSizes = 0;
	stats.JobsUnmaterialized = 0;
	scheduler.OtherPoolStats.ResetJobsRunning();

	time_t current_time = time(0);

	for (OwnerInfoMap::iterator it = OwnersInfo.begin(); it != OwnersInfo.end(); ++it) {
	#ifdef USE_JOB_QUEUE_USERREC
		OwnerInfo & Owner = *(it->second);
	#else
		OwnerInfo & Owner = it->second;
	#endif
		Owner.num.clear_counters();	// clear the jobs counters 
	}
	for (OwnerInfo * owni : zombieOwners) {
		owni->num.clear_counters(); // clear refcounts for zombies also
	}

	FlockPools.clear();
	if (FlockCollectors.size()) {
		std::vector<std::string> effectFlockList;
		int currLevel = 0;
		for (auto& col : FlockCollectors) {
			FlockPools.insert(col.name());
			const char* col_addr = col.addr();
			if (!col_addr) { continue; }
			if (currLevel < FlockLevel)
				effectFlockList.emplace_back(col_addr);
			++currLevel;
		}

		if (!effectFlockList.empty()) {
			cad->Assign(ATTR_EFFECTIVE_FLOCK_LIST, join(effectFlockList, ","));
		} else { cad->Delete(ATTR_EFFECTIVE_FLOCK_LIST); }
	}

	for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
		SubmitterData & SubDat = it->second;
		SubDat.num.clear_job_counters();	// clear the jobs counters 
		SubDat.PrioSet.clear();
		SubDat.flock.clear();
		for (const auto &entry : FlockPools) {
			SubDat.flock[entry] = SubmitterFlockCounters();
		}
	}
	SubmitterMap.Cleanup(time(NULL));

	GridJobOwners.clear();

		// Clear out the DedicatedScheduler's list of idle dedicated
		// job cluster ids, since we're about to re-create it.
	dedicated_scheduler.clearDedicatedClusters();

		// inserts/finds an entry in Owners for each job
		// updates SubmitterCounters: Hits, JobsIdle, WeightedJobsIdle & JobsHeld
		// 10/8/2021 TJ - count_a_job now also sees cluster and jobset ads so it will update Owner records.
		//    For job factories that have no materialized jobs it will potentially trigger new materialization
	WalkJobQueueWith(WJQ_WITH_CLUSTERS | WJQ_WITH_JOBSETS, count_a_job, nullptr);

	if (JobsSeenOnQueueWalk >= 0) {
		TotalJobsCount = JobsSeenOnQueueWalk;
	}

	if( dedicated_scheduler.hasDedicatedClusters() ) {
			// We found some dedicated clusters to service.  Wake up
			// the DedicatedScheduler class when we return to deal
			// with them.
		dedicated_scheduler.handleDedicatedJobTimer( 0 );
	}

		// set JobsRunning/JobsFlocked for owners
	matches->startIterations();
	match_rec *rec;
	while(matches->iterate(rec) == 1) {
		SubmitterData * SubDat;
		if (user_is_the_new_owner) {
			SubDat = insert_submitter(rec->user);
		} else {
			char *at_sign = strchr(rec->user, '@');
			if ( ! at_sign) {
				SubDat = insert_submitter(rec->user);
			} else {
				// TJ, I don't think we ever get here but just in case, we preserve the old (pre 8.9) behavior
				*at_sign = '\0';
				SubDat = insert_submitter(rec->user);
				*at_sign = '@';
			}
		}
		SubDat->num.Hits += 1;
		SubDat->LastHitTime = current_time;
		if (rec->shadowRec && rec->getPool().empty()) {
				// Sum up the # of cpus claimed by this user and advertise it as
				// WeightedJobsRunning. 

			double job_weight = calcSlotWeight(rec);
			
			SubDat->num.WeightedJobsRunning += job_weight;
			SubDat->num.JobsRunning++;
		} else if (!rec->getPool().empty()) { // non-empty pool name indicates a  remote pool, so add to Flocked count
			auto iter = SubDat->flock.insert({rec->getPool(), SubmitterFlockCounters()});
			iter.first->second.JobsRunning++;
			iter.first->second.WeightedJobsRunning += calcSlotWeight(rec);
			JobsFlocked++;
		}
	}

	// count the number of unique owners that have jobs in the queue.
	NumUniqueOwners = 0;
	for (OwnerInfoMap::iterator it = OwnersInfo.begin(); it != OwnersInfo.end(); ++it) {
	#ifdef USE_JOB_QUEUE_USERREC
		const OwnerInfo & Owner = *(it->second);
	#else
		const OwnerInfo & Owner = it->second;
	#endif
		if (Owner.num.Hits > 0) ++NumUniqueOwners;
	}

	// count the submitters that have non-zero Hits count.  These are the submitters that
	// currently have jobs in the queue.
	NumSubmitters = 0;
	for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
		const SubmitterData & SubDat = it->second;
		if (SubDat.num.Hits > 0) ++NumSubmitters;
	}

	// we may need to create a mark file if we have expired users
	auto_free_ptr cred_dir_krb(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	auto_free_ptr cred_dir_oauth(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));

	// Look for owners with zero jobs and purge them
	for (OwnerInfoMap::iterator it = OwnersInfo.begin(); it != OwnersInfo.end(); ++it) {
	#ifdef USE_JOB_QUEUE_USERREC
		OwnerInfo & owner_info = *(it->second);
	#else
		OwnerInfo & owner_info = it->second;
	#endif
		// If this Owner has any jobs in the queue or match records,
		// we don't want to remove the entry.
		if (owner_info.num.Hits > 0) {
			// CRUFT: Remove these calls once we have proper handling of
			//   users in different domains
			if (cred_dir_krb) {
				credmon_clear_mark(cred_dir_krb, owner_info.Name());
			}
			if (cred_dir_oauth) {
				credmon_clear_mark(cred_dir_oauth, owner_info.Name());
			}
			continue;
		}

		// expire and mark for removal Owners that have not had any hits (i.e jobs in the queue)
		if ( ! owner_info.LastHitTime) {
			// this is unxpected, we really should never get here with LastHitTime of 0, but in case
			// we do. start the decay timer now.
			owner_info.LastHitTime = current_time;
		} else if ( current_time - owner_info.LastHitTime > AbsentOwnerLifetime ) {
			// mark user creds for sweeping.
			if (cred_dir_krb) {
				credmon_mark_creds_for_sweeping(cred_dir_krb, owner_info.Name(), credmon_type_KRB);
			}
			if (cred_dir_oauth) {
				credmon_mark_creds_for_sweeping(cred_dir_oauth, owner_info.Name(), credmon_type_OAUTH);
			}
			// Now that we've finished using Owner.Name, we can
			// free it.  this marks the entry as unused
		#ifdef USE_JOB_QUEUE_USERREC
			//PRAGMA_REMIND("tj mark OwnerRec for deletion here...")
		#else
			owner_info.name.clear();
		#endif
		}
	}
	purgeZombieOwners();

	// set FlockLevel for owners
	if (MaxFlockLevel) {
		int flock_increment = param_integer("FLOCK_INCREMENT",1,1);

		for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
			SubmitterData & SubDat = it->second;

			// set negotiation timestamp to current time for owners that have no idle jobs.
			if ( ! SubDat.num.JobsIdle) {
				//PRAGMA_REMIND("tj: do we even need to do this?")
				SubDat.NegotiationTimestamp = current_time;
				continue;
			}

			// if this owner hasn't received a negotiation in a long time,
			// then we should flock -- we need this case if the negotiator
			// is down or simply ignoring us because our priority is so low
			if ((current_time - SubDat.NegotiationTimestamp >
				 SchedDInterval.getDefaultInterval()*2) && (SubDat.FlockLevel < MaxFlockLevel)) {
				int old_flocklevel = SubDat.FlockLevel;
				int new_flock_level = MIN(old_flocklevel + flock_increment, MaxFlockLevel);
				SubDat.FlockLevel = new_flock_level;
				SubDat.NegotiationTimestamp = current_time;
				dprintf(D_ALWAYS,
						"Increasing flock level for %s to %d from %d. (Due to lack of activity from negotiator)\n",
						SubDat.Name(), new_flock_level, old_flocklevel);
			}
			FlockLevel = MAX(SubDat.FlockLevel, FlockLevel);
		}
	}

	dprintf( D_FULLDEBUG, "JobsRunning = %d\n", JobsRunning );
	dprintf( D_FULLDEBUG, "JobsIdle = %d\n", JobsIdle );
	dprintf( D_FULLDEBUG, "JobsHeld = %d\n", JobsHeld );
	dprintf( D_FULLDEBUG, "JobsRemoved = %d\n", JobsRemoved );
	dprintf( D_FULLDEBUG, "LocalUniverseJobsRunning = %d\n",
			LocalUniverseJobsRunning );
	dprintf( D_FULLDEBUG, "LocalUniverseJobsIdle = %d\n",
			LocalUniverseJobsIdle );
	dprintf( D_FULLDEBUG, "SchedUniverseJobsRunning = %d\n",
			SchedUniverseJobsRunning );
	dprintf( D_FULLDEBUG, "SchedUniverseJobsIdle = %d\n",
			SchedUniverseJobsIdle );
	dprintf( D_FULLDEBUG, "NumSubmitters = %d\n", NumSubmitters );
	dprintf( D_FULLDEBUG, "MaxJobsRunning = %d\n", MaxJobsRunning );
	dprintf( D_FULLDEBUG, "MaxRunningSchedulerJobsPerOwner = %d\n", MaxRunningSchedulerJobsPerOwner );

	// later when we compute job priorities, we will need PrioRec
	// to have as many elements as there are jobs in the queue.  since
	// we just counted the jobs, lets make certain that PrioRec is 
	// large enough.  this keeps us from guessing to small and constantly
	// growing PrioRec... Add 5 just to be sure... :^) -Todd 8/97
	grow_prio_recs( JobsRunning + JobsIdle + 5 );
	
	cad->Assign(ATTR_NUM_USERS, NumSubmitters);
	cad->Assign(ATTR_NUM_OWNERS, NumUniqueOwners);
	cad->Assign(ATTR_MAX_JOBS_RUNNING, MaxJobsRunning);
	if (MaxRunningSchedulerJobsPerOwner >= 0 && MaxRunningSchedulerJobsPerOwner < MaxJobsPerOwner) {
		cad->Assign( "MaxRunningSchedulerJobsPerOwner", MaxRunningSchedulerJobsPerOwner );
	}

	cad->AssignExpr(ATTR_START_LOCAL_UNIVERSE, this->StartLocalUniverse);
	// m_adBase->AssignExpr(ATTR_START_LOCAL_UNIVERSE, this->StartLocalUniverse);
	cad->AssignExpr(ATTR_START_SCHEDULER_UNIVERSE, this->StartSchedulerUniverse);
	// m_adBase->AssignExpr(ATTR_START_SCHEDULER_UNIVERSE, this->StartSchedulerUniverse);
	
	cad->Assign(ATTR_NAME, Name);
	cad->Assign(ATTR_SCHEDD_IP_ADDR, daemonCore->publicNetworkIpAddr() );
	cad->Assign(ATTR_VIRTUAL_MEMORY, SwapSpace );
	 
	cad->Assign(ATTR_TOTAL_IDLE_JOBS, JobsIdle);
	cad->Assign(ATTR_TOTAL_RUNNING_JOBS, JobsRunning);
	cad->Assign(ATTR_TOTAL_JOB_ADS, JobsTotalAds);
	cad->Assign(ATTR_TOTAL_HELD_JOBS, JobsHeld);
	cad->Assign(ATTR_TOTAL_FLOCKED_JOBS, JobsFlocked);
	cad->Assign(ATTR_TOTAL_REMOVED_JOBS, JobsRemoved);

	cad->Assign(ATTR_TOTAL_LOCAL_IDLE_JOBS, LocalUniverseJobsIdle);
	cad->Assign(ATTR_TOTAL_LOCAL_RUNNING_JOBS, LocalUniverseJobsRunning);
	
	cad->Assign(ATTR_TOTAL_SCHEDULER_IDLE_JOBS, SchedUniverseJobsIdle);
	cad->Assign(ATTR_TOTAL_SCHEDULER_RUNNING_JOBS, SchedUniverseJobsRunning);

	cad->Assign(ATTR_SCHEDD_SWAP_EXHAUSTED, (bool)SwapSpaceExhausted);

	cad->Assign(ATTR_NUM_JOB_STARTS_DELAYED, RunnableJobQueue.size());
	cad->Assign(ATTR_NUM_PENDING_CLAIMS, startdContactQueue.size() + num_pending_startd_contacts);

	m_xfer_queue_mgr.publish(cad);

	// one last check for any newly-queued jobs
	// this count is cumulative within the qmgmt package
	// TJ: get daemon-core message-time here and pass into stats.Tick()
	time_t now = stats.Tick();
	stats.JobsSubmitted = GetJobQueuedCount();
	stats.Autoclusters = autocluster.getNumAutoclusters();

	OtherPoolStats.Tick(now);
	// because cad is really m_adSchedd which is persistent, we have to 
	// actively delete expired statistics atributes.
	OtherPoolStats.UnpublishDisabled(*cad);
	OtherPoolStats.RemoveDisabled();

	// As of 8.3.6. we don't show Owner stats in schedd ad.  we put them into the Submitter ads now.
	// but since the stats themselves are in the OtherPoolStats collection, we need to temporarily
	// disable them before we call Publish, then re-enable them after.
	ScheddOtherStats * OwnerStats = OtherPoolStats.Lookup("Owner");
	if (OwnerStats) {
		if ( ! OwnerStats->by) {
			OwnerStats = NULL; // these are not per-owner stats.
		} else {
			OwnerStats->enabled = false;
		}
	}
	OtherPoolStats.Publish(*cad, stats.PublishFlags);

	// re-enable owner stats after we call publish.
	if (OwnerStats) { OwnerStats->enabled = true; }

	// publish scheduler generic statistics in the Schedd ad.
	stats.Publish(*cad);

	daemonCore->publish(cad);
	daemonCore->dc_stats.Publish(*cad);
	daemonCore->monitor_data.ExportData(cad);
	extra_ads.Publish( cad );

	// can't do this at init time, the job_queue_log doesn't exist at that time.
	time_t job_queue_birthdate = GetOriginalJobQueueBirthdate();
	cad->Assign(ATTR_JOB_QUEUE_BIRTHDATE, job_queue_birthdate);
	m_adBase->Assign(ATTR_JOB_QUEUE_BIRTHDATE, job_queue_birthdate);

	daemonCore->UpdateLocalAd(cad);

#ifdef UNIX
	ScheddPluginManager::Update(UPDATE_SCHEDD_AD, cad);
#endif

		// This is called at most every 5 seconds, meaning this can cause
		// up to 300 / 5 * 2 = 120 sessions to be opened at a time per
		// collector.
	unsigned duration = 2*param_integer( "SCHEDD_INTERVAL", 300 );
	std::string capability;
	if (param_boolean("SEC_ENABLE_IMPERSONATION_TOKENS", false) && SetupCollectorSession(duration, capability)) {
		cad->InsertAttr(ATTR_CAPABILITY, capability);
	}

	int num_updates = daemonCore->sendUpdates(UPDATE_SCHEDD_AD, cad, NULL, true,
		&m_token_requester, DCTokenRequester::default_identity, "ADVERTISE_SCHEDD");
	dprintf( D_FULLDEBUG,
			 "Sent HEART BEAT ad to %d collectors. Number of active submittors=%d\n",
			 num_updates, NumSubmitters );

	cad->Delete(ATTR_CAPABILITY);
	cad->Delete(ATTR_EFFECTIVE_FLOCK_LIST);

	// send the schedd ad to our flock collectors too, so we will
	// appear in condor_q -global and condor_status -schedd
	if( FlockCollectors.size() ) {
		DCCollectorAdSequences & adSeq = daemonCore->getUpdateAdSeq();
		for (auto& col : FlockCollectors) {
			auto data = col.name() ?
					m_token_requester.createCallbackData(col.name(),
					DCTokenRequester::default_identity, "ADVERTISE_SCHEDD")
				: nullptr;
			col.sendUpdate( UPDATE_SCHEDD_AD, cad, adSeq, NULL, true, DCTokenRequester::daemonUpdateCallback, data );
		}
	}

	// --------------------------------------------------------------------------------------
	// begin publishing submitter ADs
	//

	// Set attributes common to all submitter Ads
	// 
	SetMyTypeName(*m_adBase, SUBMITTER_ADTYPE);
	m_adBase->Assign(ATTR_SCHEDD_NAME, Name);
	m_adBase->Assign(ATTR_SCHEDD_IP_ADDR, daemonCore->publicNetworkIpAddr() );
	daemonCore->publish(m_adBase);
	extra_ads.Publish(m_adBase);

	// Create a new add for the per-submitter attribs 
	// and chain it to the base ad.

	time_t update_time = time(0); // time at which submitter ads are updated

	ClassAd pAd;
	pAd.ChainToAd(m_adBase);
	pAd.Assign(ATTR_SUBMITTER_TAG,HOME_POOL_SUBMITTER_TAG);

	if (SetupNegotiatorSession(duration, HOME_POOL_SUBMITTER_TAG, capability)) {
		pAd.InsertAttr(ATTR_CAPABILITY, capability);
	}

	time_t time_now = time(nullptr);

	if (param_boolean("SCHEDDS_ARE_SUBMITTERS", false) == false) {
		// The usual case -- send one submitter ad per submitter
		for (auto it = Submitters.begin(); it != Submitters.end(); ++it) {
			updateSubmitterAd(it->second, pAd, nullptr, -1, time_now);
		}
	} else {
		// The case where we send one ad for the sum of all demand
		SubmitterData all;
	
		sumAllSubmitterData(all);	

		// and send it to our collectors
		pAd.Assign(ATTR_SCHEDDS_ARE_SUBMITTERS, true);
		updateSubmitterAd(all, pAd, nullptr, -1, time_now);
	}


	// update collector of any pools we flock to 
	DCCollectorAdSequences & adSeq = daemonCore->getUpdateAdSeq();
	DCCollector* flock_col;
	if( FlockCollectors.size() && FlockNegotiators.size() ) {

		for( int flock_level = 1;
			 flock_level <= MaxFlockLevel; flock_level++) {
			if (flock_level > (int)FlockCollectors.size() || flock_level > (int)FlockNegotiators.size()) {
				break;
			}
			flock_col = &FlockCollectors[flock_level-1];

				// Same comment about potentially creating hundreds of sessions applies
				// here as above for the primary collector...
			unsigned duration = 2*param_integer( "SCHEDD_INTERVAL", 300 );
			std::string capability;
			SetupNegotiatorSession(duration, flock_col->name(), capability);

			// update submitter ad in this pool for each owner
			if (param_boolean("SCHEDDS_ARE_SUBMITTERS_FOR_FLOCKERS", false) == false) {
				for (auto it = Submitters.begin(); it != Submitters.end(); ++it) {
					SubmitterData & SubDat = it->second;

					// we will use this "tag" later to identify which
					// CM we are negotiating with when we negotiate
					pAd.Assign(ATTR_SUBMITTER_TAG,flock_col->name());

					if (!capability.empty()) {
						pAd.InsertAttr(ATTR_CAPABILITY, capability);
					}

					updateSubmitterAd(SubDat, pAd, flock_col, flock_level, time_now);
				}
			} else {
				// The case where we send one ad for the sum of all demand
				SubmitterData all;

				pAd.Assign(ATTR_SUBMITTER_TAG,flock_col->name());

				if (!capability.empty()) {
					pAd.InsertAttr(ATTR_CAPABILITY, capability);
				}

				sumAllSubmitterData(all);	

				// and send it to our collectors
				pAd.Assign(ATTR_SCHEDDS_ARE_SUBMITTERS, true);
				updateSubmitterAd(all, pAd, flock_col, flock_level, time_now);
			}
		}
	}

	for (const auto &map_entry : Submitters) {
		const SubmitterData &SubDat = map_entry.second;
			// If two owners are reusing the same submitter, then we can't safely
			// add "extra" flock targets currently.  Otherwise, a malicious Unix user A
			// could submit jobs that cause Unix user B to flock to a pool controlled
			// by Unix user A.
		if (SubDat.owners.size() != 1) {continue;}
		for (const auto &flock_map_entries : SubDat.flock) {
			const auto &pool = flock_map_entries.first;
			if (FlockPools.find(pool) == FlockPools.end()) {
				if (!fill_submitter_ad(pAd, SubDat, pool, INT_MAX)) {continue;}

				pAd.Assign(ATTR_SUBMITTER_TAG, pool);

				auto iter = FlockExtra.find(pool);
				if (iter == FlockExtra.end()) {
					std::pair<std::string, std::unique_ptr<DCCollector>> value(pool, std::unique_ptr<DCCollector>(new DCCollector(pool.c_str())));
					value.second->setOwner(*SubDat.owners.begin());
					value.second->setAuthenticationMethods({"TOKEN"});
					auto iter2 = FlockExtra.insert(std::move(value));
					iter = iter2.first;
				}

				if (iter->second->name()) {
						// NOTE we limit this token to ALLOW-level authorization.
					auto data = m_token_requester.createCallbackData(iter->second->name(),
						*SubDat.owners.begin(), "ALLOW");
					dprintf(D_FULLDEBUG, "Will update collector %s for with ad for "
						"owner %s\n", iter->second->name(),
						SubDat.owners.begin()->c_str());
					iter->second->sendUpdate( UPDATE_OWN_SUBMITTOR_AD, &pAd, adSeq,
						NULL, true, DCTokenRequester::daemonUpdateCallback, data );
				}
			}
		}
	}

	pAd.Delete(ATTR_SUBMITTER_TAG);

	for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
		it->second.OldFlockLevel = it->second.FlockLevel;
	}

	 // Tell our GridUniverseLogic class what we've seen in terms
	 // of Globus Jobs per owner.
	GridJobOwners.startIterations();
	UserIdentity userident;
	GridJobCounts gridcounts;
	while( GridJobOwners.iterate(userident, gridcounts) ) {
		if(gridcounts.GridJobs > 0) {
			GridUniverseLogic::JobCountUpdate(
					userident.username().c_str(),
					userident.osname().c_str(),
					userident.auxid().c_str(),m_unparsed_gridman_selection_expr, 0, 0, 
					gridcounts.GridJobs,
					gridcounts.UnmanagedGridJobs);
		}
	}


	 // send info about deleted owners.
	 // we do this to update the submitter ad currently in the collector
	 // that has JobIdle > 0, and thus the negotiator will waste time contacting us.
	 // put 0 for idle, running, and held jobs. idle=0 so the negotiator stops
	 // trying to contact us, and 0 for running, held so condor_status -submit
	 // is not riddled with question marks.

	pAd.Assign(ATTR_RUNNING_JOBS, 0);
	pAd.Assign(ATTR_IDLE_JOBS, 0);
	pAd.Assign(ATTR_HELD_JOBS, 0);
	pAd.Assign(ATTR_WEIGHTED_RUNNING_JOBS, 0);
	pAd.Assign(ATTR_WEIGHTED_IDLE_JOBS, 0);
	pAd.Assign(ATTR_IDLE_LOCAL_JOBS, 0);
	pAd.Assign(ATTR_RUNNING_LOCAL_JOBS, 0);
	pAd.Assign(ATTR_IDLE_SCHEDULER_JOBS, 0);
	pAd.Assign(ATTR_RUNNING_SCHEDULER_JOBS, 0);

	// clear owner stats in case we have stale ones in pAd
	// PRAGMA_REMIND("tj: perhaps we should be continuing to publish Owner status until the owners decay?")
	if (OwnerStats) { OwnerStats->stats.Pool.Unpublish(pAd); }
	m_xfer_queue_mgr.publish_user_stats(&pAd, "@bogus@", IF_VERBOSEPUB | IF_RECENTPUB);

 	// send ads for owner that don't have jobs idle
	// This is done by looking at the Hits counter that was set above.
	// that are not in the current list (the current list has only owners w/ idle jobs)
	std::string submitter_name;
	for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
		SubmitterData & SubDat = it->second;
		// If this Owner has any jobs in the queue or match records,
		// we don't want to send the, so we continue to the next
		if (SubDat.num.Hits > 0) continue;

		if (user_is_the_new_owner) {
			submitter_name = SubDat.Name();
		} else {
			formatstr(submitter_name, "%s@%s", SubDat.Name(), AccountingDomain.c_str());
		}
		int old_flock_level = SubDat.OldFlockLevel;

		// expire and mark for removal Owners that have not had any hits (i.e jobs in the queue)
		// for more than AbsentSubmitterLifetime. 
		if ( ! SubDat.LastHitTime) {
			// this is unxpected, we really should never get here with LastHitTime of 0, but in case
			// we do. start the decay timer now.
			SubDat.LastHitTime = current_time;
		} else if (AbsentSubmitterLifetime && (current_time - SubDat.LastHitTime > AbsentSubmitterLifetime)) {
			// Now that we've finished using Owner.Name, we can
			// free it.  this marks the entry as unused
			SubDat.name.clear();
		}

		pAd.Assign(ATTR_NAME, submitter_name);

#ifdef UNIX
	// update plugins
	dprintf(D_FULLDEBUG,"Sent owner (0 jobs) ad to schedd plugins\n");
	ScheddPluginManager::Update(UPDATE_SUBMITTOR_AD, &pAd);
#endif

		if (SubDat.lastUpdateTime == update_time) continue; // if we already sent this owner, skip updating
		if (SubDat.lastUpdateTime + AbsentSubmitterUpdateRate > update_time) {
			dprintf( D_FULLDEBUG, "Skipping absent submitter ad for %s. lastupdate=%d Hit=%d Tot=%d Idle=%d Run=%d\n",
					submitter_name.c_str(), (int)(update_time - SubDat.lastUpdateTime),
					SubDat.num.Hits, SubDat.num.JobsCounted, SubDat.num.JobsIdle, SubDat.num.JobsRunning );
			continue; // we updated this owner recently
		}

		// Update collectors
		int num_udates = daemonCore->sendUpdates(UPDATE_SUBMITTOR_AD, &pAd, NULL, true);
		dprintf(D_FULLDEBUG, "Sent absent submitter ad to %d collectors for %s. lastupdate=%d Hit=%d Tot=%d Idle=%d Run=%d\n",
				num_udates, submitter_name.c_str(), (int)(update_time - SubDat.lastUpdateTime),
				SubDat.num.Hits, SubDat.num.JobsCounted, SubDat.num.JobsIdle, SubDat.num.JobsRunning );
		SubDat.lastUpdateTime = update_time;

	  // also update all of the flock hosts
	  if( FlockCollectors.size() ) {
		  int flock_level;
		  for( flock_level=1;
			   flock_level <= old_flock_level &&
				   flock_level <= (int)FlockCollectors.size(); flock_level++ ) {
			  FlockCollectors[flock_level-1].sendUpdate( UPDATE_SUBMITTOR_AD, &pAd, adSeq, NULL, true );
		  }
	  }
	}

	// now that we have repopulated the Owner's counters
	// remove any owners that are no longer used.
	remove_unused_owners();

	// SetMyTypeName( pAd, SCHEDD_ADTYPE );

	// If JobsIdle > 0, then we are asking the negotiator to contact us. 
	// Record the earliest time we asked the negotiator to talk to us.
	if ( JobsIdle >  0 ) {
		// We have idle jobs, we want the negotiator to talk to us.
		// But don't clobber NegotiationRequestTime if already set,
		// since we want the _earliest_ request time.
		if ( NegotiationRequestTime == 0 ) {
			NegotiationRequestTime = time(NULL);
		}
	} else {
		// We don't care of the negotiator talks to us.
		NegotiationRequestTime = 0;
	}

	check_claim_request_timeouts();
	return 0;
}

// schedd_negotiate.cpp doesn't have access to the stats, so it calls this function.
void IncrementResourceRequestsSent() { scheduler.stats.ResourceRequestsSent += 1; }

// create a list of ads similar to what we publish to the collector
// however, if the input pQueryAd contains a STATISTICS_TO_PUBLISH attribute
// use that attibute rather than the default statistics publising flags.
//
// Note: it is ok to return ads that don't match the query the caller is responsible 
// for filtering.   In the future we might improve the code by returing only ads 
// that match the query, but for now we don't.
//
int Scheduler::make_ad_list(
   ClassAdList & ads, 
   ClassAd * pQueryAd /*=NULL*/) // if non-null use this to get ExtraAttributes for building the list
{
   time_t now = time(NULL);

   // we need to copy the schedd Ad because
   // we will be putting it in a list that deletes Ads 
   // when the list is destroyed.
   ClassAd * cad = new ClassAd(*m_adSchedd);

   // TODO: use ATTR_TARGET_TYPE of the queryAd to restrict what ads are created here?

   std::string stats_config;
   if (pQueryAd) {
      pQueryAd->LookupString("STATISTICS_TO_PUBLISH",stats_config);
   }

   // should we call count_jobs() to update the job counts?

   stats.Tick(now);
   stats.JobsSubmitted = GetJobQueuedCount();
   stats.ShadowsRunning = numShadows;
   stats.Autoclusters = autocluster.getNumAutoclusters();

   OtherPoolStats.Tick(now);
   // because cad is a copy of m_adSchedd which is persistent, we have to 
   // actively delete expired statistics atributes.
   OtherPoolStats.UnpublishDisabled(*cad);
   OtherPoolStats.RemoveDisabled();

   int flags = stats.PublishFlags;
   if ( ! stats_config.empty()) {
      flags = generic_stats_ParseConfigString(stats_config.c_str(), "SCHEDD", "SCHEDULER", flags);
   }

   // New for 8.3.6 - temporarily disable the owner stats before calling publish
   // we will publish them in the Submitter ads instead.
   ScheddOtherStats * OwnerStats = OtherPoolStats.Lookup("Owner");
   if (OwnerStats && OwnerStats->by) { OwnerStats->enabled = false; }
   OtherPoolStats.Publish(*cad, flags);
   if (OwnerStats && OwnerStats->by) { OwnerStats->enabled = true; }
   OwnerStats = NULL;

   // publish scheduler generic statistics
   stats.Publish(*cad, stats_config.c_str());

   // publish user statistics
   for(auto const& [key, probe]: daemonCore->dc_stats.UserRuntimes)
		cad->Assign("DIAG_CCS" + key, probe.Total());
   // publish reaper statistics
   for(auto const& [reaper, probe]: daemonCore->dc_stats.ReaperRuntimes)
		cad->Assign("DIAG_CRS" + reaper, probe.Total());
   // publish fsync statistics
   for(auto const& [user, value]: this->FsyncRuntimes)
		cad->Assign("DIAG_CFS" + user, value.Total());

   m_xfer_queue_mgr.publish(cad, stats_config.c_str());

   // publish daemon core stats
   daemonCore->publish(cad);
   daemonCore->dc_stats.Publish(*cad, stats_config.c_str());
   daemonCore->monitor_data.ExportData(cad);

   // We want to insert ATTR_LAST_HEARD_FROM into each ad.  The
   // collector normally does this, so if we're servicing a
   // QUERY_SCHEDD_ADS commannd, we need to do this ourselves or
   // some timing stuff won't work.
   cad->Assign(ATTR_LAST_HEARD_FROM, now);
   //cad->Assign( ATTR_AUTHENTICATED_IDENTITY, ??? );

   // add the Scheduler Ad to the list
   ads.Insert(cad);

   // now add the submitter ads, each one based on the base 
   // submitter ad, note that chained ad's dont delete the 
   // chain parent when they are deleted, so it's safe to 
   // put these ads into the list. 
   for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
      const SubmitterData & Owner = it->second;
      if (Owner.empty()) continue;
      cad = new ClassAd();
      cad->ChainToAd(m_adBase);
      if ( ! fill_submitter_ad(*cad, Owner, "", -1)) {
         delete cad;
         continue;
      }
      cad->Assign(ATTR_SUBMITTER_TAG, HOME_POOL_SUBMITTER_TAG);
      ads.Insert(cad);
   }

   return ads.Length();
}

int Scheduler::handleMachineAdsQuery( Stream * stream, ClassAd & ) {
	int more = 1;
	int num_ads = 0;

	if( IsDebugLevel( D_TEST ) ) {
		pcccDumpTable( D_TEST );
	}

	stream->encode();

	// The HashTable class is /so/ broken:
	//	* no operator !=
	//	* post-increment operator modifies the base class
	//	* no pre-increment operator
	//	* i->second doesn't work, but (*i).second does
	//  * will not work with C++11 for( auto && i : v ) {}
	auto i = matches->begin();
	dprintf( D_TEST, "Dumping match records (with now jobs)...\n" );
	for( ; !(i == matches->end()); i.advance() ) {
		match_rec * match = (*i).second;

		if( match->my_match_ad == NULL ) {
			continue;
		}

		if( !stream->code( more ) || !putClassAd( stream, * match->my_match_ad ) ) {
			dprintf( D_ALWAYS, "Error sending query result to client, aborting.\n" );
			return FALSE;
		}

		if( match->m_now_job.isJobKey() ) {
			dprintf( D_TEST, "Match record '%s' has now job %d.%d\n",
				(*i).first.c_str(), match->m_now_job.cluster, match->m_now_job.proc );
		}

		num_ads++;
	}
	dprintf( D_TEST, "... done dumping match records.\n" );

	more = 0;
	if( !stream->code(more) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending end-of-query to client, aborting.\n" );
		return FALSE;
	}

	dprintf( D_FULLDEBUG, "Sent %d ads in response to query\n", num_ads );
	return TRUE;
}

// in support of condor_status -direct.  allow the query for SCHEDULER and SUBMITTER ads
//
int Scheduler::command_query_ads(int command, Stream* stream)
{
	ClassAd queryAd;
	ClassAd *ad;
	ClassAdList ads;
	int more = 1, num_ads = 0;

	dprintf( D_FULLDEBUG, "In command_query_ads\n" );

	stream->decode();
	stream->timeout(15);
	if( !getClassAd(stream, queryAd) || !stream->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive query on TCP: aborting\n" );
		return FALSE;
	}

	if( command == QUERY_STARTD_ADS ) {
		return handleMachineAdsQuery( stream, queryAd );
	}

	const char * targetType = nullptr;
	std::string targetTypeStr;
	if (queryAd.LookupString(ATTR_TARGET_TYPE, targetTypeStr) && ! targetTypeStr.empty()) {
		targetType = targetTypeStr.c_str();
	}

		// Construct a list of all our ClassAds. we pass queryAd 
		// through so that if there is a STATISTICS_TO_PUBLISH attribute
		// it can be used to control the verbosity of statistics
	this->make_ad_list(ads, &queryAd);

		// Now, find the ClassAds that match.
	stream->encode();
	ads.Open();
	while( (ad = ads.Next()) ) {
		if( IsATargetMatch( &queryAd, ad, targetType ) ) {
			if( !stream->code(more) || !putClassAd(stream, *ad) ) {
				dprintf (D_ALWAYS, 
						 "Error sending query result to client -- aborting\n");
				return FALSE;
			}
			num_ads++;
		}
	}

		// Finally, close up shop.  We have to send NO_MORE.
	more = 0;
	if( !stream->code(more) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending EndOfResponse (0) to client\n" );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "Sent %d ads in response to query\n", num_ads ); 
	return TRUE;
}

#ifdef USE_JOB_QUEUE_USERREC

// in support of condor_qusers query.
//
int Scheduler::command_query_user_ads(int /*command*/, Stream* stream)
{
	ClassAd queryAd;
	int num_ads = 0;

	dprintf( D_FULLDEBUG, "In command_query_user_ads\n" );

	stream->decode();
	stream->timeout(15);
	if( !getClassAd(stream, queryAd) || !stream->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive query on TCP: aborting\n" );
		return FALSE;
	}

	ClassAd summaryAd;
	summaryAd.Assign(ATTR_MY_TYPE, "Summary");
	summaryAd.Assign("TotalUserAds", OwnersInfo.size());

	int limit = INT_MAX;
	queryAd.LookupInteger(ATTR_LIMIT_RESULTS, limit);

	bool has_constraint = queryAd.Lookup(ATTR_REQUIREMENTS);

	const classad::References * proj=nullptr;
	classad::References projection;
	int has_proj = mergeProjectionFromQueryAd(queryAd, ATTR_PROJECTION, projection);
	if (has_proj > 0) { proj = &projection; } // has valid non-empty projection

	int put_opts = 0;
	bool want_server_time = false;
	if (queryAd.LookupBool(ATTR_SEND_SERVER_TIME, want_server_time) && want_server_time) {
		put_opts |= PUT_CLASSAD_SERVER_TIME;
	}

	// Now, find the ClassAds that match.
	stream->encode();
	for (const auto &[name, urec] : OwnersInfo) {
		if (num_ads >= limit) break;
		if ( ! has_constraint || IsAConstraintMatch(&queryAd, urec)) {
			ClassAd ad;
			urec->live.publish(ad,"Num");
			ad.ChainToAd(urec);
			if ( !putClassAd(stream, ad, put_opts, proj)) {
				dprintf (D_ALWAYS,  "Error sending query result to client -- aborting\n");
				return FALSE;
			}
			num_ads++;
		}
	}

	// Finally, close up shop.  We have to send the summary ad to signal the end.
	if( !putClassAd(stream, summaryAd) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending Summary ad to client\n" );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "Sent %d ads in response to query\n", num_ads ); 
	return TRUE;
}

int Scheduler::act_on_user(
	int cmd,
	const std::string & username,
	const ClassAd& cmdAd,
	TransactionWatcher & txn,
	CondorError & /*errstack*/,
	struct UpdateUserAttributesInfo & updatesInfo)
{
	int rval = 0;

	OwnerInfo * urec = find_ownerinfo(username.c_str());

	switch (cmd) {
	case ENABLE_USERREC:
		if (urec) { // enable, not add
			int userrec_id = urec->jid.proc;
			bool enable = urec->IsEnabled();
			bool do_update = false;
			if (cmdAd.LookupBool(ATTR_USERREC_OPT_UPDATE, do_update) && do_update) {
				bool has_enable = cmdAd.LookupBool(ATTR_ENABLED, enable);
				txn.BeginOrContinue(userrec_id);
				UpdateUserAttributes(urec->jid, cmdAd, enable, updatesInfo);
				// if the update is setting enabled to false, make sure that DisableReason is also set to a string
				if (has_enable && ! enable) {
					SetUserAttributeInt(*urec, ATTR_ENABLED, 0); // update will have filtered this
					std::string reason;
					if ( ! cmdAd.LookupString(ATTR_DISABLE_REASON, reason)) {
						SetUserAttributeString(*urec, ATTR_DISABLE_REASON, "by update command");
					}
				}
			} else {
				// if opt_update is not set, and the record exists, then we want to enable it
				// unless the command ad says to disable it, in which case we leave it alone
				enable = true;
				cmdAd.EvaluateAttrBoolEquiv(ATTR_ENABLED, enable);
			}
			if (enable && ! urec->IsEnabled()) {
				txn.BeginOrContinue(userrec_id);
				SetUserAttributeInt(*urec, ATTR_ENABLED, 1);
				DeleteUserAttribute(*urec, ATTR_DISABLE_REASON);
			}
		} else { // user does not exist,  we must add
			bool add_if_not = false;
			if ((cmdAd.LookupBool(ATTR_USERREC_OPT_CREATE, add_if_not) ||
				 cmdAd.LookupBool(ATTR_USERREC_OPT_CREATE_DEPRECATED, add_if_not)) && add_if_not) {
				bool enabled = true;
				cmdAd.EvaluateAttrBoolEquiv(ATTR_ENABLED, enabled);
				int userrec_id = scheduler.nextUnusedUserRecId();
				txn.BeginOrContinue(userrec_id);
				UserRecCreate(userrec_id, username.c_str(), cmdAd, getUserRecDefaultsAd(), enabled);
			} else {
				rval = 2;
			}
		}
		break;
	case DISABLE_USERREC:
		if (urec) {
			txn.BeginOrContinue(urec->jid.proc);
			SetUserAttributeInt(*urec, ATTR_ENABLED, 0);
			std::string reason;
			if (cmdAd.LookupString(ATTR_DISABLE_REASON, reason)) {
				SetUserAttributeString(*urec, ATTR_DISABLE_REASON, reason.c_str());
			} else {
				// TODO set default reason string
			}
		} else { // user does not exist, cannot be disabled
			rval = 2;
		}
		break;
	case EDIT_USERREC:
		if (urec) {
			txn.BeginOrContinue(urec->jid.proc);
			UpdateUserAttributes(urec->jid, cmdAd, urec->IsEnabled(), updatesInfo);
		} else { // user does not exist, cannot be edited
			rval = 2;
		}
		break;
	case RESET_USERREC:
		if (urec) {
			txn.BeginOrContinue(urec->jid.proc);
			// TODO: this should be a DeleteSecureAttribute
			SetUserAttributeInt(*urec, ATTR_MAX_JOBS_RUNNING, -1);
		}
		break;
	case DELETE_USERREC:
		if (urec) { // disable
			txn.BeginOrContinue(urec->jid.proc);
			SetUserAttributeInt(*urec, ATTR_ENABLED, 0);
			// TODO: check if unused so we can just delete it now...
			// UserRecDestroy(urec->jid.proc);
		} else {
			// nothing to do.
		}
		break;

	default:
		rval = 42; // not implemented
		break;
	}

	return rval;
}

// in support of condor_qusers add|edit|enble|disable|reset|delete
//
int Scheduler::command_act_on_user_ads(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmdAd;
	int num_users = 0;
	struct UpdateUserAttributesInfo updatesInfo;

	dprintf( D_FULLDEBUG, "In command_act_on_user_ads\n" );

	stream->decode();
	stream->timeout(15);

	if (!stream->get(num_users)) {
		dprintf( D_ALWAYS, "Failed to receive number of users for %s command: aborting\n", cmd_name);
		return FALSE;
	}

	std::vector<ClassAd> acts;
	for (int ii = 0; ii < num_users; ++ii) {
		ClassAd & ad = acts.emplace_back();
		if( !getClassAd(stream, ad)) {
			dprintf( D_ALWAYS, "Failed to receive %d user ad for %s command: aborting\n", ii, cmd_name);
			return FALSE;
		}
	}

	if (!stream->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive EOM: for %s command: aborting\n", cmd_name );
		return FALSE;
	}
	// done reading input command stream
	stream->encode();

	ClassAd resultAd;
	ReliSock* rsock = (ReliSock*)stream;
	auto * rsock_user = EffectiveUserRec(rsock);
	// TODO: more fine-grained user check? I think this does nothing when NULL is the first arg...
	if ( ! UserCheck2(NULL, rsock_user) || ! isQueueSuperUser(rsock_user)) {
		resultAd.Assign(ATTR_RESULT, EACCES);
		resultAd.Assign(ATTR_ERROR_STRING, "Permission denied");
		if( !putClassAd(stream, resultAd) || !stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
			return FALSE;
		}
		return TRUE;
	}

	int rval = 0;
	int num_ads = 0;
	TransactionWatcher txn;
	std::string username;
	CondorError errstack;

	for (auto & act : acts) {
		if (act.Lookup(ATTR_REQUIREMENTS)) {
			for (const auto& it : OwnersInfo) {
				if (IsAConstraintMatch(&act, it.second)) {
					rval = act_on_user(cmd, it.first, act, txn, errstack, updatesInfo);
					if (rval) break;
					++num_ads;
				}
			}
			if (rval) break;
		} else if (act.LookupString(ATTR_USER, username) && ! username.empty()) {
			rval = act_on_user(cmd, username, act, txn, errstack, updatesInfo);
			if (rval) break;
			++num_ads;
		} else if (act.Lookup(ATTR_USERREC_OPT_ME)) {
			username = rsock->getFullyQualifiedUser();
			rval = act_on_user(cmd, username, act, txn, errstack, updatesInfo);
			if (rval) break;
			++num_ads;
		} else {
			rval = 1;
			errstack.push("SCHEDD", rval, "Dont know what user to act on.");
			break;
		}
	}

	// commit or abort any pending transactions.
	if (rval) { 
		txn.AbortIfAny();
	}
	else {
		rval = txn.CommitIfAny(0, &errstack);
		if (rval) { mapPendingOwners(); }
	}
	// any pending owners we haven't mapped yet should be delete now.
	clearPendingOwners();

	resultAd.Assign(ATTR_NUM_ADS, num_ads);
	resultAd.Assign(ATTR_RESULT, rval);
	if (cmd == EDIT_USERREC) { resultAd.Assign("NumAttributesSet", updatesInfo.valid); }

	std::string errmsg;
	if ( ! errstack.empty()) { 
		errmsg = errstack.getFullText(true);
		resultAd.Assign(ATTR_ERROR_STRING, errmsg);
		errmsg = errstack.getFullText(false); // for dprintf
	} else if (updatesInfo.invalid > 0 || updatesInfo.special > 0) {
		formatstr(errmsg, "%s updates were ignored because ", updatesInfo.valid ? "Some" : "All");
		if (updatesInfo.special > 0) {
			formatstr_cat(errmsg, "%d were reserved attributes", updatesInfo.special);
			if (updatesInfo.invalid > 0) errmsg += " and ";
		}
		if (updatesInfo.invalid > 0) {
			formatstr_cat(errmsg, "%d had invalid values", updatesInfo.invalid);
		}
		resultAd.Assign(ATTR_WARNING, errmsg);
	}


	dprintf( D_FULLDEBUG, "Processed %d ads for command %s : sending result %d %s\n",
		num_ads, cmd_name, rval, errmsg.c_str()); 

	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, resultAd) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}
#endif

static bool
sendJobErrorAd(Stream *stream, int errorCode, std::string errorString)
{
	classad::ClassAd ad;
	ad.InsertAttr(ATTR_OWNER, 0);
	ad.InsertAttr(ATTR_ERROR_STRING, errorString);
	ad.InsertAttr(ATTR_ERROR_CODE, errorCode);

	stream->encode();
	if (!putClassAd(stream, ad) || !stream->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to send error ad for job ads query\n");
	}
	return false;
}

static const std::string & attrjoin(std::string & buf, const char * prefix, const char * attr) {
	if (prefix) { buf = prefix; buf += attr; }
	else { buf = attr; }
	return buf;
}

void LiveJobCounters::publish(ClassAd & ad, const char * prefix) const
{
	std::string buf;
	ad.InsertAttr(attrjoin(buf,prefix,"Jobs"), (long long)(JobsIdle + JobsRunning + JobsHeld + JobsRemoved + JobsCompleted + JobsSuspended));
	ad.InsertAttr(attrjoin(buf,prefix,"Idle"), (long long)JobsIdle);
	ad.InsertAttr(attrjoin(buf,prefix,"Running"), (long long)JobsRunning);
	ad.InsertAttr(attrjoin(buf,prefix,"Removed"), (long long)JobsRemoved);
	ad.InsertAttr(attrjoin(buf,prefix,"Completed"), (long long)JobsCompleted);
	ad.InsertAttr(attrjoin(buf,prefix,"Held"), (long long)JobsHeld);
	ad.InsertAttr(attrjoin(buf,prefix,"Suspended"), (long long)JobsSuspended);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerJobs"), (long long)(SchedulerJobsIdle + SchedulerJobsRunning + SchedulerJobsHeld + SchedulerJobsRemoved + SchedulerJobsCompleted));
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerIdle"), (long long)SchedulerJobsIdle);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerRunning"), (long long)SchedulerJobsRunning);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerRemoved"), (long long)SchedulerJobsRemoved);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerCompleted"), (long long)SchedulerJobsCompleted);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerHeld"), (long long)SchedulerJobsHeld);
}

static bool
sendDone(Stream *stream,
	bool send_job_counts,
	LiveJobCounters* query_counts,
	const char * myname,
	LiveJobCounters* my_counts,
	ClassAd * analysis)
{
	ClassAd ad;
	ad.Assign(ATTR_OWNER, 0);
	ad.Assign(ATTR_ERROR_CODE, 0);
	ad.Assign(ATTR_SERVER_TIME, time(nullptr));

	if (send_job_counts) {
		ad.Assign(ATTR_MY_TYPE, "Summary");
		scheduler.liveJobCounts.publish(ad, "Allusers");
		if (query_counts) { query_counts->publish(ad, NULL); }
		if (my_counts) { my_counts->publish(ad, "My"); }
	}
	if (myname) { ad.Assign("MyName", myname); }
	if (analysis && analysis->size() > 0) { ad.Insert("Analyze", analysis->Copy()); }

	stream->encode();
	if (!putClassAd(stream, ad) || !stream->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to send done message to job query.\n");
		return false;
	}
	return true;
}

void IncrementLiveJobCounter(LiveJobCounters & num, int universe, int status, int increment /*, JobQueueJob * job*/)
{
	if (status == TRANSFERRING_OUTPUT) status = RUNNING;
	switch (universe) {
	case CONDOR_UNIVERSE_SCHEDULER:
		//dprintf(D_ALWAYS | D_BACKTRACE, "IncrementLiveJobCounter(%p, %d, %d, %d) for %d.%d (%p)\n", &num.SchedulerJobsIdle, universe, status, increment, job->jid.cluster, job->jid.proc, job);
		if (status > 0 && status <= HELD) {
			(&num.SchedulerJobsIdle)[status-1] += increment;
		}
		break;
	/*
	case CONDOR_UNIVERSE_LOCAL:
		if (status > 0 && status <= HELD) {
			(&num.LocalJobsIdle)[status-1] += increment;
		}
		break;
	*/
	default:
		//dprintf(D_ALWAYS | D_BACKTRACE, "IncrementLiveJobCounter(%p, %d, %d, %d) for %d.%d (%p)\n", &num.JobsIdle, universe, status, increment, job->jid.cluster, job->jid.proc, job);
		if (status > 0 && status <= HELD) {
			(&num.JobsIdle)[status-1] += increment;
		} else if (status == SUSPENDED) {
			num.JobsSuspended += increment;
		}
		break;
	}
}

struct QueryJobAdsContinuation : Service {

	classad_shared_ptr<classad::ExprTree> requirements;
	classad::References projection;
	LiveJobCounters query_job_counts;
	LiveJobCounters my_job_counts;
	std::string my_name;
	ClassAd analysisAd;
	JobQueueLogType::filter_iterator it;
	int match_limit;
	int match_count;
	bool summary_only;
	bool unfinished_eom;
	bool registered_socket;
	bool send_server_time;
	bool for_analysis{false};

	QueryJobAdsContinuation(classad_shared_ptr<classad::ExprTree> requirements_, int limit, int timeslice_ms=0, int iter_opts=0, bool server_time=true, bool for_anal=false);
	int finish(Stream *);
	// add job analysis to the analysisAd that will be sent with the query summary
	void analyze_job(JobQueueJob * job);
};

QueryJobAdsContinuation::QueryJobAdsContinuation(classad_shared_ptr<classad::ExprTree> requirements_, int limit, int timeslice_ms, int iter_opts, bool server_time, bool for_anal)
	: requirements(requirements_),
	  it(GetJobQueueIterator(*requirements, timeslice_ms)),
	  match_limit(limit),
	  match_count(0),
	  summary_only(false),
	  unfinished_eom(false),
	  registered_socket(false),
	  send_server_time(server_time),
	  for_analysis(for_anal)
{
	it.set_options(iter_opts);
	my_job_counts.clear_counters();
}

int
QueryJobAdsContinuation::finish(Stream *stream) {
	ReliSock *sock = static_cast<ReliSock*>(stream);
	JobQueueLogType::filter_iterator end = GetJobQueueIteratorEnd();
	if (match_limit >= 0 && (match_count >= match_limit)) {
		it = end;
	}
	bool has_backlog = false;
	int put_flags = PUT_CLASSAD_NON_BLOCKING | PUT_CLASSAD_NO_PRIVATE;
	if (send_server_time) {
		put_flags |= PUT_CLASSAD_SERVER_TIME;
	}

	if (unfinished_eom) {
		int retval = sock->finish_end_of_message();
		if (sock->clear_backlog_flag()) {
			return KEEP_STREAM;
		} else if (retval == 1) {
			unfinished_eom = false;
		} else if (!retval) {
			delete this;
			return sendJobErrorAd(sock, 5, "Failed to write EOM to wire");
		}
	}
	while ((it != end) && !has_backlog) {
		JobQueuePayload ad = *it++;
		if (!ad) {
			// Return to DC in case if our time ran out.
			has_backlog = true;
			break;
		}
		if (ad->IsJob()) {
			JobQueueJob * job = dynamic_cast<JobQueueJob*>(ad);
			IncrementLiveJobCounter(query_job_counts, job->Universe(), job->Status(), 1);
			if (for_analysis) analyze_job(job);
		}
		//if (IsDebugCatAndVerbosity(D_COMMAND | D_VERBOSE)) {
		//	dprintf(D_COMMAND | D_VERBOSE, "Writing job %d.%d type=%d%d%d to wire\n", job->jid.cluster, job->jid.proc, job->IsJob(), job->IsCluster(), job->IsJobSet());
		//}
		int retval = 1;
		if ( ! summary_only) {
			if (ad->IsCluster()) {
				// if this is a cluster ad, then we are responding to a -factory query. In that case, we want to fake up
				// a child ad so we can send some extra attributes.
				JobQueueCluster * cad = dynamic_cast<JobQueueCluster*>(ad);
				ClassAd iad;
				cad->PopulateInfoAd(iad, 0, true);
				retval = putClassAd(sock, iad, put_flags,
						projection.empty() ? NULL : &projection);
			} else if (ad->IsJobSet()) {
				// if this is a set ad, then make a temporary child ad for returning the jobset aggregates
				JobQueueJobSet * jobset = dynamic_cast<JobQueueJobSet*>(ad);
				ClassAd iad;
				jobset->jobStatusAggregates.publish(iad, "Num");
				iad.Assign(ATTR_REF_COUNT, jobset->member_count);
				iad.ChainToAd(jobset);
				retval = putClassAd(sock, iad, put_flags,
						projection.empty() ? NULL : &projection);
			} else {
				retval = putClassAd(sock, *ad, put_flags,
						projection.empty() ? NULL : &projection);
			}
		}
		match_count++;
		if (retval == 2) {
			//dprintf(D_FULLDEBUG, "Detecting backlog.\n");
			has_backlog = true;
		} else if (!retval) {
			delete this;
			return sendJobErrorAd(sock, 4, "Failed to write ClassAd to wire");
		}
		retval = sock->end_of_message_nonblocking();
		if (sock->clear_backlog_flag()) {
			//dprintf(D_FULLDEBUG, "Socket EOM will block.\n");
			unfinished_eom = true;
			has_backlog = true;
		}
		if (match_limit >= 0 && (match_count >= match_limit)) {
			it = end;
		}
	}
	if (has_backlog && !registered_socket) {
		int retval = daemonCore->Register_Socket(stream, "Client Response",
			(SocketHandlercpp)&QueryJobAdsContinuation::finish,
			"Query Job Ads Continuation", this, HANDLE_WRITE);
		if (retval < 0) {
			delete this;
			return sendJobErrorAd(sock, 4, "Failed to write ClassAd to wire");
		}
		registered_socket = true;
	} else if (!has_backlog) {
		//dprintf(D_FULLDEBUG, "Finishing condor_q.\n");
		const char * me = NULL;
		LiveJobCounters * mine = NULL;
		if ( ! my_name.empty()) { me = my_name.c_str(); mine = &my_job_counts; }
		int rval = sendDone(sock, true, &query_job_counts, me, mine, &analysisAd);
		delete this;
		return rval;
	}
	return KEEP_STREAM;
}

// add job analysis to the analysisAd that will be sent with the query summary
void
QueryJobAdsContinuation::analyze_job(JobQueueJob * job)
{
	if ( ! job || ! job->IsJob()) return;

	// dprintf(D_COMMAND, "doing analyze_job for autocluster %d\n", job->autocluster_id);

	if (job->autocluster_id > 0) {
		std::string attr = "ac" + std::to_string(job->autocluster_id);
		if (analysisAd.Lookup(attr)) return; // we did this autocluster already

		ClassAd * anad = new ClassAd;
		anad->Assign(ATTR_AUTO_CLUSTER_ID, job->autocluster_id);

		// TODO: keep a log of match rejections by autocluster_id and report them all

		// Short term hack. search all of the jobs in this cluster to try and find one
		// with a match reject reason.
		//
		std::string reason, negname;
		if (job->LookupString(ATTR_LAST_REJ_MATCH_REASON, reason)) {
			anad->Assign(ATTR_LAST_REJ_MATCH_REASON, reason);
			if (job->LookupString(ATTR_LAST_REJ_MATCH_NEGOTIATOR, negname)) {
				anad->Assign(ATTR_LAST_REJ_MATCH_NEGOTIATOR, negname);
			}
			time_t rej_time = 0;
			if (job->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, rej_time)) {
				anad->Assign(ATTR_LAST_REJ_MATCH_TIME, rej_time);
			}
		} else {
		#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
			// check all of the jobs in this autocluster for match info
			// return the most recent success and failure
			// also return counts of Running, Completed, Idle and Total jobs for the autocluster
			time_t last_match_time = 0;
			time_t last_rej_time = 0;
			int num_jobs = 0;
			LiveJobCounters jobcounts;
			for (auto & jid : scheduler.autocluster.joblist(*job)) {
				auto j2 = GetJobAd(jid);
				if ( ! j2) continue;
				++num_jobs;
				time_t time;
				if (j2->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, time) && time > last_rej_time) {
					last_rej_time = time;
					if (j2->LookupString(ATTR_LAST_REJ_MATCH_REASON, reason)) {
						negname.clear();
						j2->LookupString(ATTR_LAST_REJ_MATCH_NEGOTIATOR, negname);
					}
				}
				if (j2->LookupInteger(ATTR_LAST_MATCH_TIME, time) && time > last_match_time) {
					last_match_time = time;
				}
				IncrementLiveJobCounter(jobcounts, j2->Universe(), j2->Status(), 1);
			}

			if (last_match_time) { anad->Assign(ATTR_LAST_MATCH_TIME, last_match_time); }
			if (last_rej_time) { anad->Assign(ATTR_LAST_REJ_MATCH_TIME, last_rej_time); }
			if ( ! reason.empty()) { anad->Assign(ATTR_LAST_REJ_MATCH_REASON, reason); }
			if ( ! negname.empty()) { anad->Assign(ATTR_LAST_REJ_MATCH_NEGOTIATOR, negname); }
			if (jobcounts.JobsRunning) { anad->Assign("NumRunning", jobcounts.JobsRunning); }
			if (jobcounts.JobsCompleted) { anad->Assign("NumCompleted", jobcounts.JobsCompleted); }
			anad->Assign("NumIdle", jobcounts.JobsIdle);
			anad->Assign("NumJobs", num_jobs);
		#endif
		}

		analysisAd.Insert(attr, anad);
	}
}

int Scheduler::command_query_job_ads(int cmd, Stream* stream)
{
	ClassAd queryAd;

	stream->decode();
	stream->timeout(15);
	if( !getClassAd(stream, queryAd) || !stream->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive query on TCP: aborting\n" );
		return FALSE;
	}

	// if the groupby or useautocluster attributes exist and are true
	bool group_by = false;
	queryAd.LookupBool("ProjectionIsGroupBy", group_by);
	bool query_autocluster = false;
	queryAd.LookupBool("QueryDefaultAutocluster", query_autocluster);
	if (query_autocluster || group_by) {
		return command_query_job_aggregates(queryAd, stream);
	}
	bool send_server_time = true;
	queryAd.LookupBool(ATTR_SEND_SERVER_TIME, send_server_time);
	bool query_for_analysis = false;
	queryAd.LookupBool("ForAnalysis", query_for_analysis);

	int dpf_level = D_COMMAND | D_VERBOSE;

	// If the query request that only the querier's jobs be returned
	// we have to figure out who the quierier is and add a clause to the requirements expression
	classad::ExprTree *my_jobs_expr = NULL;
	std::string my_jobs_name; // set only once we have decided to do an only-my-jobs query
	bool was_my_jobs = false;
	if (param_boolean("CONDOR_Q_ONLY_MY_JOBS", true)) {
		my_jobs_expr = queryAd.Lookup("MyJobs");
		was_my_jobs = my_jobs_expr != NULL;
	}
	if (my_jobs_expr) {
		// if this is a 'only my jobs' query, then we want to use the authenticated
		// identity of the caller is. if the connection was already authenticated, use that owner.
		std::string owner;
		ReliSock* rsock = (ReliSock*)stream;
		bool authenticated = false;
		if (rsock->triedAuthentication()) {
			authenticated = rsock->isAuthenticated();
			dprintf(dpf_level, "%s command %s\n", getCommandStringSafe(cmd),
				authenticated ? "was authenticated" : "failed to authenticate");
		}
		const char * p0wn = rsock->getOwner();
		if (p0wn && ( ! authenticated || MATCH == strcasecmp(p0wn, "unauthenticated"))) p0wn = NULL;
		if (user_is_the_new_owner) {
			p0wn = rsock->getFullyQualifiedUser();
		}
		if (IsDebugCatAndVerbosity(dpf_level)) {
			dprintf(dpf_level, "QUERY_JOB_ADS detected %s = %s\n", attr_JobUser.c_str(), p0wn ? p0wn : "<null>");
		}

		long long val;
		// if MyJobs is a literal true/false, then we are being asked to either NOT show
		// just the users jobs, or figure out who the user is and show just their jobs.
		if (ExprTreeIsLiteralNumber(my_jobs_expr, val)) {
			if (val) {
				// MyJobs is just a boolean equivalent TRUE or FALSE, so it's up to the schedd to choose
				// the appropriate ownername.
				if (p0wn) owner = p0wn;
			} else {
				// false means we don't want just this owners jobs. set my_jobs_expr to NULL to signal that.
				owner.clear();
				my_jobs_expr = NULL;
			}
		} else {
			if (p0wn) {
				owner = p0wn;
			} else {
				// if my_jobs_expr is not literal true/valse, then we assume it is Owner==Me
				// and that ME is also an attribute in the query ad. With Me being a guess as
				// to the correct owner.  We will use the suggested value of Me unless we can
				// determine a better value. 
				classad::ExprTree * me_expr = queryAd.Lookup("Me");
				if ( ! me_expr || ! ExprTreeIsLiteralString(me_expr, owner)) {
					owner.clear();
					my_jobs_expr = NULL; // no me or it's not a string, so show all jobs.
				}
			}
		}
		// at this point owner is valid or empty.
		// if empty, or the owner is a queue superuser show all jobs.
		if (owner.empty() || isQueueSuperUser(EffectiveUserRec(rsock))) {
			my_jobs_expr = NULL; 
		}

		// at this point owner should be set if my_jobs_expr is non-null
		// and that means we can construct the correct my_jobs_expr contraint expression
		if (my_jobs_expr) {
			std::string sub_expr;
			bool fully_qualified = owner.find('@') != std::string::npos;
			formatstr(sub_expr, "(%s == \"%s\")", fully_qualified ? ATTR_USER : ATTR_OWNER, owner.c_str());
			classad::ClassAdParser parser;
			my_jobs_expr = parser.ParseExpression(sub_expr.c_str());
			my_jobs_name = owner;
		}
	}

	// at this point, my_jobs_expr is either NULL, or an ExprTree that we are responsible for deleting

	classad::ExprTree *requirements_in = queryAd.Lookup(ATTR_REQUIREMENTS);
	classad::ExprTree *requirements = my_jobs_expr;
	if (requirements_in) {
		bool bval = false;
		requirements_in = SkipExprParens(requirements_in);
		if (IsDebugCatAndVerbosity(dpf_level)) {
			dprintf(dpf_level, "QUERY_JOB_ADS %d formal requirements without excess parens: %s\n", was_my_jobs, ExprTreeToString(requirements_in));
		}
		if ( ! requirements) {
			requirements = requirements_in->Copy();
		} else if ( ! ExprTreeIsLiteralBool(requirements_in, bval) || bval) {
			// if we have both requirements, and requirements_in, and the requirements_in is not a trivial 'true' 
			// we need to join them both with a logical && op.
			requirements = JoinExprTreeCopiesWithOp(classad::Operation::LOGICAL_AND_OP, my_jobs_expr, requirements_in);
			delete my_jobs_expr; my_jobs_expr = NULL;
		}
	} else if ( ! requirements) {
		requirements = classad::Literal::MakeBool(true);
	}
	if ( ! requirements) return sendJobErrorAd(stream, 1, "Failed to create requirements expression");
	if (IsDebugCatAndVerbosity(dpf_level)) {
		dprintf(dpf_level, "QUERY_JOB_ADS %d effective requirements: %s\n", was_my_jobs, ExprTreeToString(requirements));
	}
	classad_shared_ptr<classad::ExprTree> requirements_ptr(requirements);

	int resultLimit=-1;
	if (!queryAd.EvaluateAttrInt(ATTR_LIMIT_RESULTS, resultLimit)) {
		resultLimit = -1;
	}

	int iter_options = 0;
	bool include_cluster = false, include_jobsets = false, no_proc_ads = false;
	if (queryAd.EvaluateAttrBool(ATTR_QUERY_Q_INCLUDE_CLUSTER_AD, include_cluster) && include_cluster) {
		iter_options |= JOB_QUEUE_ITERATOR_OPT_INCLUDE_CLUSTERS;
	}
	if (queryAd.EvaluateAttrBool(ATTR_QUERY_Q_INCLUDE_JOBSET_ADS, include_jobsets) && include_jobsets) {
		iter_options |= JOB_QUEUE_ITERATOR_OPT_INCLUDE_JOBSETS;
	}
	if (queryAd.EvaluateAttrBool(ATTR_QUERY_Q_NO_PROC_ADS, no_proc_ads) && no_proc_ads) {
		iter_options |= JOB_QUEUE_ITERATOR_OPT_NO_PROC_ADS;
	}
	std::string ids;
	if (queryAd.LookupString(ATTR_QUERY_Q_IDS, ids) && ! ids.empty()) {
		// TODO: add a ID based QueryJobAdsContinuation instead of the current JobQueue iteration based
	}

	if (IsDebugCatAndVerbosity(dpf_level)) {
		dprintf(dpf_level, "QUERY_JOB_ADS limit=%d, iter_options=0x%x\n", resultLimit, iter_options);
	}

	QueryJobAdsContinuation *continuation = new QueryJobAdsContinuation(
		requirements_ptr, resultLimit, 1000, iter_options, send_server_time, query_for_analysis);
	int proj_err = mergeProjectionFromQueryAd(queryAd, ATTR_PROJECTION, continuation->projection, true);
	if (proj_err < 0) {
		delete continuation;
		if (proj_err == -1) {
			return sendJobErrorAd(stream, 2, "Unable to evaluate projection list");
		}
		return sendJobErrorAd(stream, 3, "Unable to convert projection list to string list");
	}
	if ( ! my_jobs_name.empty()) {
		// if doing an only-my-jobs query, grab the job counters for this owner
		// and also return the name we settled on
		continuation->my_name = my_jobs_name;
		OwnerInfo * ownerinfo = find_ownerinfo(my_jobs_name.c_str());
		if (ownerinfo) { continuation->my_job_counts = ownerinfo->live; }
	}
	bool summary_only = false;
	if (queryAd.EvaluateAttrBoolEquiv("SummaryOnly", summary_only) && summary_only) {
		continuation->summary_only = true;
	}

	ForkStatus fork_status = schedd_forker.NewJob();
	if (fork_status == FORK_PARENT)
	{ // Successfully forked a child - as far as the schedd cares, this worked.
	  // Throw away the socket and move on.
		// need to delete the parent's copy of the continuation object
		delete continuation;
		return true;
	}
	else if (fork_status == FORK_CHILD)
	{ // Respond to the query from the child.
		int retval;
		while ((retval = continuation->finish(stream)) == KEEP_STREAM) {}
		_exit(!retval);
		ASSERT( false );
		while (true) {}
	}
	else // BUSY or FAILED
	{ // Write the response; let DC handle the callbacks.
		return continuation->finish(stream);
	}
}

void * BeginJobAggregation(bool use_def_autocluster, const char * projection, int result_limit, int return_jobid_limit, classad::ExprTree *constraint)
{
	JobAggregationResults *jar = NULL;
	jar = scheduler.autocluster.aggregateOn(use_def_autocluster, projection, result_limit, constraint);
	if (jar && return_jobid_limit > 0) { jar->set_return_jobid_limit(return_jobid_limit); }
	return (void*)jar;
}

void ComputeJobAggregation(void * aggregation)
{
	if ( ! aggregation)
		return;
	JobAggregationResults *jar = (JobAggregationResults*)aggregation;
	jar->compute();
}

void PauseJobAggregation(void * aggregation)
{
	if ( ! aggregation)
		return;
	JobAggregationResults *jar = (JobAggregationResults*)aggregation;
	jar->pause();
}

ClassAd *GetNextJobAggregate(void * aggregation, bool first)
{
	if ( ! aggregation)
		return NULL;
	JobAggregationResults *jar = (JobAggregationResults*)aggregation;
	if (first) { jar->rewind(); }
	return jar->next();
}
void ReleaseAggregationAd(void *aggregation, ClassAd * ad)
{
	if (aggregation) {
		ASSERT(ad);
		return;
	}
}
void ReleaseAggregation(void *aggregation)
{
	if (aggregation) {
		JobAggregationResults *jar = (JobAggregationResults*)aggregation;
		delete jar;
	}
}

struct QueryAggregatesContinuation : Service {

	void * aggregator;
	int    timeslice;
	classad::References proj;
	bool unfinished_eom;
	bool registered_socket;
	ClassAd * curr_ad;

	QueryAggregatesContinuation(void * aggregator_, int timeslice_ms=0);
	~QueryAggregatesContinuation();
	int finish(Stream *);
};

QueryAggregatesContinuation::QueryAggregatesContinuation(void * aggregator_, int timeslice_ms)
	: aggregator(aggregator_)
	, timeslice(timeslice_ms)
	, unfinished_eom(false)
	, registered_socket(false)
	, curr_ad(NULL)
{
	curr_ad = GetNextJobAggregate(aggregator, true);
}

QueryAggregatesContinuation::~QueryAggregatesContinuation()
{
	if (aggregator) {
		if (curr_ad) { ReleaseAggregationAd(aggregator, curr_ad); curr_ad = NULL; }
		ReleaseAggregation(aggregator);
		aggregator = NULL;
	}
}


int
QueryAggregatesContinuation::finish(Stream *stream) {
	ReliSock *sock = static_cast<ReliSock*>(stream);
	bool has_backlog = false;

	if (unfinished_eom) {
		int retval = sock->finish_end_of_message();
		if (sock->clear_backlog_flag()) {
			return KEEP_STREAM;
		} else if (retval == 1) {
			unfinished_eom = false;
		} else if (!retval) {
			delete this;
			return sendJobErrorAd(sock, 5, "Failed to write EOM to wire");
		}
	}
	while (curr_ad && !has_backlog) {
		if (IsFulldebug(D_FULLDEBUG)) {
			int id=-1, job_count=-1;
			if ( ! curr_ad->EvaluateAttrInt(ATTR_AUTO_CLUSTER_ID, id)) curr_ad->EvaluateAttrInt("Id", id);
			curr_ad->EvaluateAttrInt("JobCount", job_count);
			//dprintf(D_FULLDEBUG, "Writing autocluster %d,%d to wire\n", id, job_count);
		}
		int retval = putClassAd(sock, *curr_ad,
					PUT_CLASSAD_NON_BLOCKING | PUT_CLASSAD_NO_PRIVATE,
					proj.empty() ? NULL : &proj);
		if (retval == 2) {
			dprintf(D_FULLDEBUG, "QueryAggregatesContinuation: Detecting backlog.\n");
			has_backlog = true;
		} else if (!retval) {
			delete this;
			return sendJobErrorAd(sock, 4, "Failed to write ClassAd to wire");
		}
		retval = sock->end_of_message_nonblocking();
		if (sock->clear_backlog_flag()) {
			dprintf(D_FULLDEBUG, "QueryAggregatesContinuation: Socket EOM will block.\n");
			unfinished_eom = true;
			has_backlog = true;
		}
		ReleaseAggregationAd(aggregator, curr_ad);
		if (has_backlog) {
			// we are about to give up the cpu, so pause the aggregator instead of fetching the next item.
			PauseJobAggregation(aggregator);
		} else {
			curr_ad = GetNextJobAggregate(aggregator, false);
		}
	}
	if (has_backlog && !registered_socket) {
		int retval = daemonCore->Register_Socket(stream, "Client Response",
			(SocketHandlercpp)&QueryAggregatesContinuation::finish,
			"Query Aggregates Continuation", this, HANDLE_WRITE);
		if (retval < 0) {
			delete this;
			return sendJobErrorAd(sock, 4, "Failed to write ClassAd to wire");
		}
		registered_socket = true;
	} else if (!has_backlog) {
		//dprintf(D_FULLDEBUG, "Finishing condor_q aggregation.\n");
		delete this;
		return sendDone(sock, false, NULL, NULL, NULL, nullptr);
	}
	return KEEP_STREAM;
}

int Scheduler::command_query_job_aggregates(ClassAd &queryAd, Stream* stream)
{
	classad::ExprTree *constraint = queryAd.Lookup(ATTR_REQUIREMENTS);

	std::string projection;
	if ( ! queryAd.LookupString(ATTR_PROJECTION, projection)) { projection = ""; }

	//bool group_by = false;
	//queryAd.LookupBool("ProjectionIsGroupBy", group_by);
	bool use_def_autocluster = false;
	queryAd.LookupBool("QueryDefaultAutocluster", use_def_autocluster);

	int resultLimit=-1;
	if (!queryAd.EvaluateAttrInt(ATTR_LIMIT_RESULTS, resultLimit)) {
		resultLimit = -1;
	}

	int returnJobidLimit = -1;
	if (!queryAd.EvaluateAttrInt("MaxReturnedJobIds", returnJobidLimit)) {
		returnJobidLimit = -1;
	}

	void *aggregation = BeginJobAggregation(use_def_autocluster, projection.c_str(), resultLimit, returnJobidLimit, constraint);
	if ( ! aggregation) {
		return -1;
	}

	ForkStatus fork_status = schedd_forker.NewJob();
	if (fork_status == FORK_PARENT)
	{ // Successfully forked a child - as far as the schedd cares, this worked.
	  // Throw away the socket and move on.
		// need to free the parent's copy of the aggregation object
		ReleaseAggregation(aggregation);
		return true;
	}
	else // either didn't fork. or I'm in the forked child. 
	{
		ComputeJobAggregation(aggregation);
		QueryAggregatesContinuation *continuation = new QueryAggregatesContinuation(aggregation, 1000);
		if (fork_status == FORK_CHILD) // Respond to the query from the child.
		{
			int retval;
			while ((retval = continuation->finish(stream)) == KEEP_STREAM) {}
			_exit(!retval);
			ASSERT( false );
			while (true) {}
		}
		else // BUSY or FAILED (or doesn't support fork. i.e Windows)
		{
			// Write the response; let DC handle the callbacks.
			return continuation->finish(stream);
		}
	}
}


int 
clear_autocluster_id(JobQueueJob *job, const JOB_ID_KEY & /*jid*/, void *)
{
	job->Delete(ATTR_AUTO_CLUSTER_ID);
	job->autocluster_id = -1;
	return 0;
}

	// This function, given a job, calculates the "weight", or cost
	// of the slot for accounting purposes.  Usually the # of cpus
double 
Scheduler::calcSlotWeight(match_rec *mrec) const {
	if (!mrec) {
		// shouldn't ever happen, but be defensive
		return 1;
	}


		// machine may be null	
	ClassAd *machine = mrec->my_match_ad;
	double job_weight = 1;
	if (m_use_slot_weights && machine) {
			// if the schedd slot weight expression is set and parses,
			// evaluate it here
		double weight = 1.0;
		if ( ! machine->LookupFloat(ATTR_SLOT_WEIGHT, weight)) {
			weight = 1.0;
		}
		// PRAGMA_REMIND("TJ: fix slot weight code to use double, not int")
		job_weight = weight;
	} else {
			// slot weight isn't set, fall back to assuming cpus
		if (machine) {
			if(0 == machine->LookupFloat(ATTR_CPUS, job_weight)) {
				job_weight = 1.0; // or fall back to one if CPUS isn't in the startds
			}
		} else if (scheduler.m_use_slot_weights) {
			// machine == NULL, this happens on schedd restart and reconnect
			// calculate using request_* attributes, as we do with idle
			JobQueueJob *job = GetJobAd(mrec->cluster, mrec->proc);
			if (job) {
				if (scheduler.slotWeightOfJob) {
					classad::Value result;
					int rval = EvalExprToNumber( scheduler.slotWeightOfJob, job, NULL, result );

					if( !rval || !result.IsNumber(job_weight)) {
						job_weight = 1; // Fall back if it doesn't eval
					}
				} else {
					job_weight = scheduler.guessJobSlotWeight(job);
				}
			}
		}
	}
	
	return job_weight;
}

// when there is no SCHEDD_SLOT_WIEGHT expression, try and guess the job weight
// by building a fake STARTD ad and evaluating the startd's SLOT_WEIGHT expression
//
double
Scheduler::guessJobSlotWeight(JobQueueJob * job)
{
	static bool failed_to_init_slot_weight_map_ad = false;
	if ( ! this->slotWeightGuessAd) {
		ClassAd * ad = new ClassAd();
		failed_to_init_slot_weight_map_ad = false;
		this->slotWeightGuessAd = ad;

		auto_free_ptr sw(param("SLOT_WEIGHT"));
		if ( ! sw || ! ad || ! ad->AssignExpr(ATTR_SLOT_WEIGHT, sw)) {
			failed_to_init_slot_weight_map_ad = true;
			dprintf(D_ALWAYS, "Failed to create slotWeightGuessAd SLOT_WEIGHT = %s\n", sw ? sw.ptr() : "NULL");
		} else {
			ad->Assign(ATTR_CPUS, 1);
			ad->Assign(ATTR_MEMORY, 1024);
			ad->Assign(ATTR_DISK, 1024);
			dprintf(D_ALWAYS, "Creating slotWeightGuessAd as:\n");
			dPrintAd(D_ALWAYS, *ad);
		}
	}

	// If no SCHEDD_SLOT_WEIGHT, then push RequestCpus, RequestMemory & RequestDisk
	// into our slotWeightGuessAd as Cpus, Memory & Disk and then evaluate SlotWeight
	// This is wrong in general, but will work in the most common case of setting
	// slot weight to Memory or Cpus + Memory/1024.  That is, it will work if 
	// the job has constants for RequestCpus & RequestMemory. 

	// If the job doesn't have constants, then this code most likely undercounts
	// the job slot weight, so hopefully we can do away with this in the future
	//
	int req_cpus;
	if ( ! job->LookupInteger(ATTR_REQUEST_CPUS, req_cpus)) {
		req_cpus = 1;
	}
	if (failed_to_init_slot_weight_map_ad) {
		return req_cpus;
	}
	ClassAd * ad = this->slotWeightGuessAd;

	long long req_mem = (long long)req_cpus * 1024;
	long long req_disk = (long long)req_cpus * 1024;
	if ( ! job->LookupInteger(ATTR_REQUEST_MEMORY, req_mem)) {
		req_mem = req_cpus * 1024ll;
	}
	if ( ! job->LookupInteger(ATTR_REQUEST_DISK, req_disk)) {
		req_disk = req_cpus * 1024ll;
	}

	ad->Assign(ATTR_CPUS, req_cpus);
	ad->Assign(ATTR_MEMORY, req_mem);
	ad->Assign(ATTR_DISK, req_disk);

	double job_weight = req_cpus;
	if ( ! ad->LookupFloat(ATTR_SLOT_WEIGHT, job_weight)) {
		job_weight = req_cpus;
	}
	return job_weight;
}

int
count_a_job(JobQueueBase* ad, const JOB_ID_KEY& /*jid*/, void*)
{
	int		status;
	int		cur_hosts;
	int		max_hosts;
	int		universe;

		// we may get passed a NULL job ad if, for instance, the job ad was
		// removed via condor_rm -f when some function didn't expect it.
		// So check for it here before continuing onward...
	if ( ! ad) {
		return 0;
	}
	// make sure that OwnerInfo records cannot be deleted while a jobset for that owner exists
	if (ad->IsJobSet()) {
		JobQueueJobSet * jobset = dynamic_cast<JobQueueJobSet*>(ad);
		if (jobset->ownerinfo) {
			jobset->ownerinfo->num.Hits += 1;
		}
		return 0;
	}
	JobQueueJob * job = dynamic_cast<JobQueueJob*>(ad);
	if (! job) return 0;

	// cluster ads get different treatment
	if (job->IsCluster()) {
		// set job->ownerdata pointer if it is not yet set. we do this in case the accounting group
		// or niceness has been queue-edited or otherwise changed. and to insure that the owner
		// map is aware of the reference
		SubmitterData * SubData = NULL;
		OwnerInfo * OwnInfo = scheduler.get_submitter_and_owner(job, SubData);
		if ( ! OwnInfo) {
			dprintf(D_ALWAYS, "Cluster %d has no " ATTR_OWNER " attribute.  Ignoring...\n", job->jid.cluster);
			return 0;
		}
		OwnInfo->num.Hits += 1;
		if (SubData) { SubData->num.Hits += 1; }
		// don't count clusters when tracking unique owners per submitter // SubData->owners.insert(OwnInfo->name);

		// clusters that have no materialized jobs need to be kickstarted to materialize.
		// since materialization is normally triggered by job state changes.
		// We can end up in this stituation we hit the MAX_JOBS_PER_OWNER limit
		// So when we see a factory that has no jobs, we schedule a materialize attempt here
		bool allow_materialize = scheduler.getAllowLateMaterialize();
		if (allow_materialize) {
			JobQueueCluster * clusterad = static_cast<JobQueueCluster*>(job);
			if (clusterad->factory) {
				if ( ! clusterad->HasAttachedJobs() && JobFactoryIsRunning(clusterad)) {
					ScheduleClusterForJobMaterializeNow(clusterad->jid.cluster);
				}
				int unmat = UnMaterializedJobCount(clusterad);
				if (unmat > 0) {
					scheduler.stats.JobsUnmaterialized += unmat;
					if (scheduler.OtherPoolStats.AnyEnabled()) {
						time_t now = time(nullptr);
						ScheddOtherStats * other_stats = scheduler.OtherPoolStats.Matches(*job, now);
						for (ScheddOtherStats * po = other_stats; po; po = po->next) {
							po->stats.JobsUnmaterialized += unmat;
						}
					}
				}
			}
		}
		return 0;
	}

	if (job->LookupInteger(ATTR_JOB_STATUS, status) == 0) {
		dprintf(D_ALWAYS, "Job %d.%d has no %s attribute.  Ignoring...\n",
		        job->jid.cluster, job->jid.proc, ATTR_JOB_STATUS);
		return 0;
	}

	bool noop = false;
	job->LookupBool(ATTR_JOB_NOOP, noop);
	if (noop && status != COMPLETED) {
		int cluster = 0;
		int proc = 0;
		int noop_status = 0;
		int temp = 0;
		PROC_ID job_id;
		if(job->LookupInteger(ATTR_JOB_NOOP_EXIT_SIGNAL, temp) != 0) {
			noop_status = generate_exit_signal(temp);
		}	
		if(job->LookupInteger(ATTR_JOB_NOOP_EXIT_CODE, temp) != 0) {
			noop_status = generate_exit_code(temp);
		}	
		job->LookupInteger(ATTR_CLUSTER_ID, cluster);
		job->LookupInteger(ATTR_PROC_ID, proc);
		dprintf(D_FULLDEBUG, "Job %d.%d is a no-op with status %d\n",
				cluster,proc,noop_status);
		job_id.cluster = cluster;
		job_id.proc = proc;
		set_job_status(cluster, proc, COMPLETED);
		scheduler.WriteTerminateToUserLog( job_id, noop_status );
		return 0;
	}

	if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) == 0) {
		cur_hosts = ((status == RUNNING || status == TRANSFERRING_OUTPUT) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) == 0) {
		max_hosts = ((status == IDLE) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) == 0) {
		universe = CONDOR_UNIVERSE_VANILLA;
	}

	int request_cpus = 0;
    if (job->LookupInteger(ATTR_REQUEST_CPUS, request_cpus) == 0) {
		request_cpus = 1;
	}
	
		// Just in case it is set funny
	if (request_cpus < 1) {
		request_cpus = 1;
	}
	

	// because we set job->ownerdata to NULL above, this will refresh
	// the job->ownerdata pointer. we do this in case the accounting group
	// or niceness has been queue-edited or otherwise changed.
	SubmitterData * SubData = NULL;
	OwnerInfo * OwnInfo = scheduler.get_submitter_and_owner(job, SubData);
	if ( ! OwnInfo) {
		dprintf(D_ALWAYS, "Job %d.%d has no %s attribute.  Ignoring...\n",
		        job->jid.cluster, job->jid.proc, ATTR_OWNER);
		return 0;
	}
		// Keep track of unique owners per submitter.
	SubData->owners.insert(OwnInfo->Name());

	// increment our count of the number of job ads in the queue
	scheduler.JobsTotalAds++;

    time_t now = time(NULL);
    OwnInfo->LastHitTime = now;
    SubData->LastHitTime = now;

    ScheddOtherStats * other_stats = NULL;
    if (scheduler.OtherPoolStats.AnyEnabled()) {
        other_stats = scheduler.OtherPoolStats.Matches(*job, now);
    }
    #define OTHER for (ScheddOtherStats * po = other_stats; po; po = po->next) (po->stats)

    if (status == IDLE || status == RUNNING || status == TRANSFERRING_OUTPUT) {
        /*
         * Not all universes track CurrentHosts and MaxHosts; if there's no information,
         * simply increment Running or Idle by 1.
         */
        if ((status == RUNNING || status == TRANSFERRING_OUTPUT) && !cur_hosts)
        {
                scheduler.JobsRunning += 1;
        }
        else if ((status == IDLE) && !max_hosts)
        {
                scheduler.JobsIdle += 1;
        }
        else
        {
                scheduler.JobsRunning += cur_hosts;
                scheduler.JobsIdle += (max_hosts - cur_hosts);
        }

            // if job is not idle, then update statistics for running jobs
        if (status == RUNNING || status == TRANSFERRING_OUTPUT) {
            scheduler.stats.JobsRunning += 1;
            OTHER.JobsRunning += 1;

            int job_image_size = 0;
            job->LookupInteger("ImageSize_RAW", job_image_size);
            scheduler.stats.JobsRunningSizes += (int64_t)job_image_size * 1024;
            OTHER.JobsRunningSizes += (int64_t)job_image_size * 1024;

            time_t job_start_date = 0;
            time_t job_running_time = 0;
            if (job->LookupInteger(ATTR_JOB_START_DATE, job_start_date))
                job_running_time = (now - job_start_date);
            scheduler.stats.JobsRunningRuntimes += job_running_time;
            OTHER.JobsRunningRuntimes += job_running_time;
        }
    } else if (status == HELD) {
        scheduler.JobsHeld++;
    } else if (status == REMOVED) {
        scheduler.JobsRemoved++;
    }
    #undef OTHER

	// update per-submitter and per-owner counters
	SubmitterCounters * Counters = &SubData->num;
#ifdef USE_JOB_QUEUE_USERREC
	JobQueueUserRec::CountJobsCounters * OwnerCounts = &OwnInfo->num;
#else
	RealOwnerCounters * OwnerCounts = &OwnInfo->num;
#endif

	// Hits also counts matchrecs, which aren't jobs. (hits is sort of a reference count)
	Counters->Hits += 1;
	Counters->JobsCounted += 1;
	
	OwnerCounts->Hits += 1;
	OwnerCounts->JobsCounted += 1;

	if ( (universe != CONDOR_UNIVERSE_GRID) &&	// handle Globus below...
		 (!service_this_universe(universe,job))  )
	{
			// Deal with all the Universes which we do not service, expect
			// for Globus, which we deal with below.
		if (universe == CONDOR_UNIVERSE_SCHEDULER)
		{
			// Count REMOVED or HELD jobs that are in the process of being
			// killed. cur_hosts tells us which these are.
			scheduler.SchedUniverseJobsRunning += cur_hosts;
			scheduler.SchedUniverseJobsIdle += (max_hosts - cur_hosts);
			OwnerCounts->SchedulerJobsRunning += cur_hosts;
			OwnerCounts->SchedulerJobsIdle += (max_hosts - cur_hosts);
			Counters->SchedulerJobsRunning += cur_hosts;
			Counters->SchedulerJobsIdle += (max_hosts - cur_hosts);
		}
		if (universe == CONDOR_UNIVERSE_LOCAL)
		{
			// Count REMOVED or HELD jobs that are in the process of being
			// killed. cur_hosts tells us which these are.
			scheduler.LocalUniverseJobsRunning += cur_hosts;
			scheduler.LocalUniverseJobsIdle += (max_hosts - cur_hosts);
			OwnerCounts->LocalJobsRunning += cur_hosts;
			OwnerCounts->LocalJobsIdle += (max_hosts - cur_hosts);
			Counters->LocalJobsRunning += cur_hosts;
			Counters->LocalJobsIdle += (max_hosts - cur_hosts);
		}
			// We want to record the cluster id of all idle MPI and parallel
		    // jobs

		bool sendToDS = false;
		job->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING, sendToDS);
		if( (sendToDS || universe == CONDOR_UNIVERSE_MPI ||
			 universe == CONDOR_UNIVERSE_PARALLEL) && status == IDLE ) {
			if( max_hosts > cur_hosts ) {
				int cluster = 0;
				job->LookupInteger( ATTR_CLUSTER_ID, cluster );

				int proc = 0;
				job->LookupInteger( ATTR_PROC_ID, proc );
					// Don't add all the procs in the cluster, just the first
				if( proc == 0) {
					dedicated_scheduler.addDedicatedCluster( cluster );
				}
			}
		}

		// bailout now, since all the crud below is only for jobs
		// which the schedd needs to service
		return 0;
	} 

	if ( universe == CONDOR_UNIVERSE_GRID ) {
		// for Grid, count jobs in UNSUBMITTED state by owner.
		// later we make certain there is a grid manager daemon
		// per owner.
		bool job_managed = jobExternallyManaged(job);
		bool job_managed_done = jobManagedDone(job);
		// if status is REMOVED, but the remote job id is not null,
		// then consider the job IDLE for purposes of the logic here.  after all,
		// the gridmanager needs to be around to finish the task of removing the job.
		// if the gridmanager has set Managed="ScheddDone", then it's done
		// with the job and doesn't want to see it again.
		if ( status == REMOVED && job_managed_done == false ) {
			if ( job->LookupString( ATTR_GRID_JOB_ID, NULL, 0 ) )
			{
				// looks like the job's remote job id is still valid,
				// so there is still a job submitted remotely somewhere.
				// fire up the gridmanager to try and really clean it up!
				status = IDLE;
			}
		}

#if 1   // cache ownerdata pointer in job object
		std::string real_owner, domain;
		job->LookupString(ATTR_OWNER,real_owner); // we can't get here if the job has no ATTR_OWNER
		job->LookupString(ATTR_NT_DOMAIN, domain);
#endif
		// Don't count HELD jobs that aren't externally (gridmanager) managed
		// Don't count jobs that the gridmanager has said it's completely
		// done with.
		UserIdentity userident(*job);
		if ( ( status != HELD || job_managed != false ) &&
			 job_managed_done == false ) 
		{
			GridJobCounts * gridcounts = scheduler.GetGridJobCounts(userident);
			ASSERT(gridcounts);
			gridcounts->GridJobs++;
		}
		if ( status != HELD && job_managed == 0 && job_managed_done == 0 ) 
		{
			GridJobCounts * gridcounts = scheduler.GetGridJobCounts(userident);
			ASSERT(gridcounts);
			gridcounts->UnmanagedGridJobs++;
		}

		return 0;
	}

	if (status == IDLE || status == RUNNING || status == TRANSFERRING_OUTPUT) {

			// Update Owner array PrioSet iff knob USE_GLOBAL_JOB_PRIOS is true
			// and iff job is looking for more matches (max-hosts - cur_hosts)
		if ( param_boolean("USE_GLOBAL_JOB_PRIOS",false) &&
			 ((max_hosts - cur_hosts) > 0) )
		{
			int job_prio;
			if ( job->LookupInteger(ATTR_JOB_PRIO,job_prio) ) {
				SubData->PrioSet.insert( job_prio );
			}
		}
			// Update Owners array JobsIdle
		int job_idle = (max_hosts - cur_hosts);
		OwnerCounts->JobsIdle += job_idle;
		Counters->JobsIdle += job_idle;

			// If we're biasing by slot weight, and the job is idle, and everything parsed...
		int job_idle_weight;
		if (scheduler.m_use_slot_weights && (max_hosts > cur_hosts)) {
				// if we're biasing idle jobs by SCHEDD_SLOT_WEIGHT, eval that here
			double job_weight = request_cpus;
			if (scheduler.slotWeightOfJob) {
				classad::Value value;
				int rval = EvalExprToNumber(scheduler.slotWeightOfJob, job, NULL, value);
				if ( ! rval || ! value.IsNumber(job_weight)) {
					job_weight = request_cpus; // fall back if slot weight doesn't evaluate
				}
			} else {
				job_weight = scheduler.guessJobSlotWeight(job);
			}
			job_idle_weight = job_weight * job_idle;
		} else {
			// here: either max_hosts == cur_hosts || !scheduler.m_use_slot_weights
			job_idle_weight = request_cpus * job_idle;
		}
		Counters->WeightedJobsIdle += job_idle_weight;

			// Update per-flock jobs idle
		std::string flock_targets;
		bool include_default_flock = param_boolean("FLOCK_BY_DEFAULT", true);
		if (job->EvaluateAttrString(ATTR_FLOCK_TO, flock_targets)) {
			for (auto& flock_entry: StringTokenIterator(flock_targets)) {
				if (!strcasecmp(flock_entry.c_str(), "default")) {
					include_default_flock = true;
				} else {
					auto iter = SubData->flock.insert({flock_entry, SubmitterFlockCounters()});
					iter.first->second.JobsIdle += job_idle;
					iter.first->second.WeightedJobsIdle += job_idle_weight;
				}
			}
				// Subtract out overlap with default list of flocked pools.
			for (auto& flock_entry: StringTokenIterator(flock_targets)) {
				auto iter = scheduler.FlockPools.find(flock_entry);
				if (iter != scheduler.FlockPools.end()) {
					SubData->flock[flock_entry].JobsIdle -= job_idle;
					SubData->flock[flock_entry].WeightedJobsIdle -= job_idle_weight;
				}
			}
		}
		if (include_default_flock) {
			for (const auto &flock_entry : scheduler.FlockPools) {
				SubData->flock[flock_entry].JobsIdle += job_idle;
				SubData->flock[flock_entry].WeightedJobsIdle += job_idle_weight;
			}
		}

			// Don't update scheduler.Owners[name].JobsRunning here.
			// We do it in Scheduler::count_jobs().

	} else if (status == HELD) {
		OwnerCounts->JobsHeld++;
		Counters->JobsHeld++;
	}

	return 0;
}

bool
service_this_universe(int universe, ClassAd* job)
{
	// "service" seems to really mean find a matching resource or not...

	/*  If a non-grid job is externally managed, it's been grabbed by
		the schedd-on-the-side and we don't want to touch it.
	 */
	if ( universe != CONDOR_UNIVERSE_GRID && jobExternallyManaged( job ) ) {
		return false;
	}

	/* If we made it to here, the WantMatching was not defined.  So
	   figure out what to do based on Universe and other misc logic...
	*/
	switch (universe) {
		case CONDOR_UNIVERSE_GRID:
		case CONDOR_UNIVERSE_MPI:
		case CONDOR_UNIVERSE_PARALLEL:
		case CONDOR_UNIVERSE_SCHEDULER:
			return false;
		case CONDOR_UNIVERSE_LOCAL:
			if (scheduler.usesLocalStartd()) {
				bool reqsFixedup = false;
				job->LookupBool("LocalStartupFixup", reqsFixedup);
				if (!reqsFixedup) {
					job->Assign("LocalStartupFixup", true);
					ExprTree *requirements = job->LookupExpr(ATTR_REQUIREMENTS);
					const char *rhs = ExprTreeToString(requirements);
					std::string newRequirements = std::string("IsLocalStartd && ")  + rhs;
					job->AssignExpr(ATTR_REQUIREMENTS, newRequirements.c_str());
				}
				return true;
			} else {
				return false;
			}
			break;
		default:

			bool sendToDS = false;
			job->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING, sendToDS);
			if (sendToDS) {
				return false;
			} else {
				return true;
			}
	}
}

OwnerInfo *
Scheduler::incrementRecentlyAdded(OwnerInfo * ownerInfo, const char * owner)
{
	if ( ! ownerInfo) { ownerInfo = insert_ownerinfo( owner ); }
	ownerInfo->num.Hits += 1;
	ownerInfo->num.JobsRecentlyAdded += 1;
	return ownerInfo;
}

SubmitterData *
Scheduler::find_submitter(const char * name)
{
	SubmitterDataMap::iterator found = Submitters.find(name);
	if (found != Submitters.end())
		return &found->second;
	return NULL;
}

SubmitterData *
Scheduler::insert_submitter(const char * name)
{
	SubmitterData * Subdat = find_submitter(name);
	if (Subdat) return Subdat;
	Subdat = &Submitters[name];
	Subdat->name = name;
	Subdat->OldFlockLevel = MinFlockLevel;
	Subdat->FlockLevel = MinFlockLevel;
	Subdat->NegotiationTimestamp = time(NULL);
	dprintf(D_FULLDEBUG, "Initializing flock level for %s to %d.\n", name, MinFlockLevel);
	return Subdat;
}

OwnerInfo *
Scheduler::find_ownerinfo(const char * owner)
{
#ifdef USE_JOB_QUEUE_USERREC
	// we want to allow a lookup to prefix match on the domain, so we
	// use lower_bound instead of find.  lower_bound will return the matching item
	// for an exact match, and also when owner is a domain prefix of the OwnersInfo key
	// We also want to allow a bare username or a domain of '.' to match a
	// record with current UID_DOMAIN.
	// Starting at the lower bound, we have to scan all records that match
	// our prefix.
	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		// Search for all records that start with 'user@'.
		// Comparing domains requires special logic in is_same_user().
		std::string user = owner;
		size_t at = user.find_last_of('@');
		if (at == std::string::npos) {
			user += '@';
		} else {
			user.erase(at + 1);
		}
		auto lb = OwnersInfo.lower_bound(user); // first element >= the owner
		while (lb != OwnersInfo.end()) {
			if (is_same_user(owner, lb->first.c_str(), COMPARE_DOMAIN_DEFAULT, scheduler.uidDomain())) {
				return lb->second;
			}
			if (!is_same_user(owner, lb->first.c_str(), COMPARE_IGNORE_DOMAIN, "~")) break;
			++lb;
		}
	} else {
		std::string obuf;
		auto found = OwnersInfo.find(name_of_user(owner, obuf));
		if (found != OwnersInfo.end()) {
			return found->second;
		}
	}
#else
	OwnerInfoMap::iterator found = OwnersInfo.find(owner);
	if (found != OwnersInfo.end())
		return &found->second;
#endif
	return nullptr;
}

#ifdef USE_JOB_QUEUE_USERREC

int Scheduler::nextUnusedUserRecId()
{
	if (NextOwnerId <= LAST_RESERVED_USERREC_ID) { NextOwnerId = LAST_RESERVED_USERREC_ID+1; }
	return NextOwnerId++;
}

JobQueueUserRec * Scheduler::jobqueue_newUserRec(int userrec_id)
{
	auto found = pendingOwners.find(userrec_id);
	if (found != pendingOwners.end()) {
		return found->second;
	}
	if (userrec_id >= NextOwnerId) { NextOwnerId = userrec_id+1; }
	JobQueueUserRec * uad = new JobQueueUserRec(userrec_id);
	pendingOwners[userrec_id] = uad;
	uad->setPending();
	return uad;
}

void Scheduler::jobqueue_deleteUserRec(JobQueueUserRec * uad)
{
	// remove it from the Owners table
	if (!uad->empty()) {
		auto it = scheduler.OwnersInfo.find(uad->Name());
		if (it != scheduler.OwnersInfo.end()) {
			if (it->second == uad) {
				scheduler.OwnersInfo.erase(it);
			}
		}
	}
	// remove it from the pending table
	auto found = pendingOwners.find(uad->jid.proc);
	if (found != pendingOwners.end()) {
		pendingOwners.erase(found);
	}

	// we can't safely delete the object until there a no jobs referencing it.
	// we detect that in count_jobs, so here we put the pointer into the
	// zombie collection until then
	zombieOwners.push_back(uad);
}

// called on shutdown after we delete the job queue.
void Scheduler::deleteZombieOwners()
{
	for (OwnerInfo * owni : zombieOwners) { delete owni; }
	zombieOwners.clear();
}

// called in count_jobs to delete zombie owners that no longer have any refs
void Scheduler::purgeZombieOwners()
{
	auto zombie_dust = [](OwnerInfo* & owni){ 
		if (owni->num.Hits) return false;
		delete owni;
		return true;
	};
	auto it = std::remove_if(zombieOwners.begin(), zombieOwners.end(), zombie_dust);
	zombieOwners.erase(it, zombieOwners.end());
}

// called by InitJobQueue to put newly added UserRec ads into the OwnerInfo map
// and after we commit a transaction, to clear the pending state for new users
void Scheduler::mapPendingOwners()
{
	for (auto it : pendingOwners) {
		JobQueueUserRec * uad = it.second;
		if (uad->jid.proc >= NextOwnerId) { NextOwnerId = uad->jid.proc+1; }
		uad->clearPending();
		uad->flags |= JQU_F_DIRTY; // set dirty to force populate
		uad->PopulateFromAd();
		uad->setStaleConfigSuper(); // force a superuser check next time we need to know
		OwnersInfo[uad->Name()] = uad; // on startup, newly created records will not yet be in the map
	}
	pendingOwners.clear();
	if (NextOwnerId <= LAST_RESERVED_USERREC_ID) { NextOwnerId = LAST_RESERVED_USERREC_ID+1; }
}

// clear the pending owners table and delete any that are still in the pending state
void Scheduler::clearPendingOwners()
{
	for (auto it : pendingOwners) {
		JobQueueUserRec * uad = it.second;
		if (uad->isPending() && ! uad->empty()) {
			// we put pending owners in the owners table so we don't try and create them more than once
			// but if they are still in the pending state now, we have to remove them.
			auto it = scheduler.OwnersInfo.find(uad->Name());
			if (it != scheduler.OwnersInfo.end()) {
				if (it->second == uad) {
					scheduler.OwnersInfo.erase(it);
				}
			}
			delete uad;
		}
	}
	pendingOwners.clear();
}

// call with a bare Owner name, a fully qualified User name
// or (windows only) a partially qualified owner@ntdomain name
// this will lookup the OwnerInfo record, and return it, creating a new (pending) one if needed
OwnerInfo *
Scheduler::insert_ownerinfo(const char * owner)
{
	OwnerInfo * Owner = find_ownerinfo(owner);
	if (Owner) return Owner;

	dprintf(D_ALWAYS, "Owner %s has no JobQueueUserRec\n", owner);

	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		if (YourString("condor") == owner ||
			YourString("condor@password") == owner ||
			YourString("condor@family") == owner ||
			YourString("condor@child") == owner ||
			YourString("condor@parent") == owner) {

			dprintf(D_ERROR | D_BACKTRACE, "Error: insert_ownerinfo(%s) is not allowed\n", owner);
			return &CondorUserRec;
		}

	#if 0
		// before we insert a new ownerinfo, check to see if we already have a pending record
		#ifdef WIN32
		CompareUsersOpt opt = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN | CASELESS_USER);
		#else
		CompareUsersOpt opt = COMPARE_DOMAIN_DEFAULT;
		#endif

		for (auto & [id, rec] : pendingOwners) {
			if (is_same_user(rec->Name(), owner, opt, scheduler.uidDomain())) {
				return rec;
			}
		}
	#endif
	}

	dprintf(D_ALWAYS, "Creating pending JobQueueUserRec for owner %s\n", owner);

	// the owner passed here may or may not have a full domain, (i.e. it may be a ntdomain instead of a fqdn)
	// if it does not have a fully qualified username, then we may want to expand it to a fqdn
	// alternatively, it may be a fqdn that we only want to use the owner part of
	std::string user;
	const char * at = strrchr(owner, '@');
	const char * ntdomain = nullptr;
	if (USERREC_NAME_IS_FULLY_QUALIFIED) { // need a fully qualified name for the JobQueueUserRec
		if ( ! at || MATCH == strcmp(at, "@.")) {
			// no domain supplied, or the domain is "."
			// we need to build a fully qualified username
			if (at) { user.assign(owner, at - owner + 1); } else { user = owner; user += "@"; }
			user += uidDomain();
			owner = user.c_str();
		}
	#ifdef WIN32
		else if ( ! strchr(at, '.')) {
			// domain is partial (a hostname) 
			user.assign(owner, at - owner + 1);
			user += uidDomain();
			owner = user.c_str();
			ntdomain = at+1;
		}
	#endif
	} else { // need a bare Owner name for the JobQueueUserRec
		if (at) {
			// if passed-in owner name had a supplied domain, use the bare name part
			owner = name_of_user(owner, user);
		#ifdef WIN32
			ntdomain = at+1;
		#endif
		}
	}

	int userrec_id = nextUnusedUserRecId();
	JobQueueUserRec * uad = new JobQueueUserRec(userrec_id, owner, ntdomain);
	pendingOwners[userrec_id] = uad;
	uad->setPending();
	// also insert it into OwnerInfo map for the next guy
	Owner = uad;
	OwnersInfo[owner] = Owner;
	ASSERT(Owner);
	return Owner;
}

const OwnerInfo *
Scheduler::lookup_owner_const(const char * owner)
{
	if ( ! owner) return NULL;
	return find_ownerinfo(owner);
}

const OwnerInfo *
Scheduler::insert_owner_const(const char * name)
{
	return insert_ownerinfo(name);
}

#else
const OwnerInfo *
Scheduler::lookup_owner_const(const char * owner)
{
	if ( ! owner) return NULL;
	return find_ownerinfo(owner);
}

OwnerInfo *
Scheduler::insert_ownerinfo(const char * owner)
{
	OwnerInfo * Owner = find_ownerinfo(owner);
	if (Owner) return Owner;
	Owner = &OwnersInfo[owner];
	Owner->name = owner;
	return Owner;
}

const OwnerInfo *
Scheduler::insert_owner_const(const char * name)
{
	return insert_ownerinfo(name);
}
#endif

// lookup (and cache) pointer to the jobs owner instance data
OwnerInfo *
Scheduler::get_ownerinfo(JobQueueJob * job)
{
	if ( ! job) return NULL;
	if ( ! job->ownerinfo) {
		std::string real_owner;
	#ifdef USE_JOB_QUEUE_USERREC
		if ( ! job->LookupString(ATTR_USER,real_owner) ) {
			return NULL;
		}
		const char *owner = real_owner.c_str();
		job->ownerinfo = scheduler.find_ownerinfo(owner);
	#else
		if ( ! job->LookupString(attr_JobUser,real_owner) ) {
			return NULL;
		}
		const char *owner = real_owner.c_str();
		job->ownerinfo = scheduler.insert_ownerinfo(owner);
	#endif
	}
	return job->ownerinfo;
}

// lookup (and cache) pointer to the jobs submitter instance data (aka the OwnerData)
OwnerInfo *
Scheduler::get_submitter_and_owner(JobQueueJob * job, SubmitterData * & submitterdata)
{
	submitterdata = NULL;
	if ( ! job) return NULL;

	// if the cached submitterdata pointer is valid and non-null, we can just use it
	if ( ! (job->dirty_flags & JQJ_CACHE_DIRTY_SUBMITTERDATA)) {
		submitterdata = job->submitterdata;
	}
	if (submitterdata && job->ownerinfo) {
		return job->ownerinfo;
	}

	// otherwise, we need to get owner name and construct submitter name
	// and update the cached pointers to ownerinfo and submitterdata

	std::string real_owner;
#ifdef USE_JOB_QUEUE_USERREC
	if (job->ownerinfo) {
		real_owner = job->ownerinfo->Name();
	} else {
		// Use the fully qualified username as the key for the ownerinfo
		if ( ! job->LookupString(ATTR_USER,real_owner)) {
			return NULL;
		}
		// We really shouldn't get here without the job having a validing ownerinfo pointer
		// but if we can do this lookup and fix it, we keep going.
		job->ownerinfo = scheduler.find_ownerinfo(real_owner.c_str());
		if ( ! job->ownerinfo) return NULL;
	}
	// but use the unqualified "owner" name as the submitter
	auto last_at = real_owner.find_last_of('@');
	if (last_at != std::string::npos) { real_owner.erase(last_at); }
	const char *owner = real_owner.c_str();
#else
	if ( ! job->LookupString(attr_JobUser,real_owner) ) {
		return NULL;
	}
	if (strcmp(UidDomain, AccountingDomain.c_str())) {
		auto last_at = real_owner.find_last_of('@');
		if (last_at != std::string::npos) {
			real_owner = real_owner.substr(0, last_at) + "@" + AccountingDomain;
		}
	}
	const char *owner = real_owner.c_str();
	if ( ! job->ownerinfo) {
		job->ownerinfo = scheduler.insert_ownerinfo(owner);
	}
#endif

	// in the simple case, owner name and submitter name are the same.
	// we start by assuming that will be the case.
	const char * submitter = owner;

	// if there is an accounting group, then the submitter name will be that.
	std::string alias;
	job->LookupString(ATTR_ACCOUNTING_GROUP,alias); // TODDCORE
	if ( ! alias.empty() && (alias != real_owner)) {
		if (user_is_the_new_owner) { alias += std::string("@") + AccountingDomain; }
		submitter = alias.c_str();
	}

	// lookup/insert a submitterdata record for this submitter name and cache the resulting pointer in the job object.
	job->submitterdata = scheduler.insert_submitter(submitter);
	if (job->submitterdata) {
		job->dirty_flags &= ~JQJ_CACHE_DIRTY_SUBMITTERDATA;
		job->submitterdata->isOwnerName = (owner == submitter);
		submitterdata = job->submitterdata;
	}
	return job->ownerinfo;
}


void
Scheduler::remove_unused_owners()
{
	for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ) {
		SubmitterDataMap::iterator prev = it++;
		if (prev->second.empty()) {
			Submitters.erase(prev);
		}
	}

#ifdef USE_JOB_QUEUE_USERREC
	//PRAGMA_REMIND("tj: write this")
#else
	for (OwnerInfoMap::iterator it = OwnersInfo.begin(); it != OwnersInfo.end(); ) {
		OwnerInfoMap::iterator prev = it++;
		if (prev->second.empty()) {
			OwnersInfo.erase(prev);
		}
	}
#endif
}



static bool IsSchedulerUniverse( shadow_rec* srec );
static bool IsLocalUniverse( shadow_rec* srec );

void
abort_job_myself( PROC_ID job_id, JobAction action, bool log_hold )
{
	shadow_rec *srec;
	int mode;

	// NOTE: This function is *not* transaction safe -- it should not be
	// called while a queue management transaction is active.  Why?
	// Because  we call GetJobAd() instead of GetAttributeXXXX().
	// At some point, we should
	// have some code here to assert that is the case.  
	// Questions?  -Todd <tannenba@cs.wisc.edu>

	// First check if there is a shadow assiciated with this process.
	// If so, send it SIGUSR,
	// but do _not_ call DestroyProc - we'll do that via the reaper
	// after the job has exited (and reported its final status to us).
	//
	// If there is no shadow, then simply call DestroyProc() (if we
	// are removing the job).

    dprintf( D_FULLDEBUG, 
			 "abort_job_myself: %d.%d action:%s log_hold:%s\n",
			 job_id.cluster, job_id.proc, getJobActionString(action),
			 log_hold ? "true" : "false" );

		// Note: job_ad should *NOT* be deallocated, so we don't need
		// to worry about deleting it before every return case, etc.
	JobQueueJob* job_ad = GetJobAd( job_id.cluster, job_id.proc );

	if ( !job_ad ) {
        dprintf ( D_ALWAYS, "tried to abort %d.%d; not found.\n", 
                  job_id.cluster, job_id.proc );
        return;
	}

	mode = -1;
	//job_ad->LookupInteger(ATTR_JOB_STATUS,mode);
	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_STATUS, &mode);
	if ( mode == -1 ) {
		EXCEPT("In abort_job_myself: %s attribute not found in job %d.%d",
				ATTR_JOB_STATUS,job_id.cluster, job_id.proc);
	}

	// Mark the job clean
	MarkJobClean(job_id);

	int job_universe = CONDOR_UNIVERSE_VANILLA;
	job_ad->LookupInteger(ATTR_JOB_UNIVERSE,job_universe);


		// If a non-grid job is externally managed, it's been grabbed by
		// the schedd-on-the-side and we don't want to touch it.
	if ( job_universe != CONDOR_UNIVERSE_GRID &&
		 jobExternallyManaged( job_ad ) ) {

		return;
	}

		// Handle Globus Universe
	if (job_universe == CONDOR_UNIVERSE_GRID) {
		bool job_managed = jobExternallyManaged(job_ad);
		bool job_managed_done = jobManagedDone(job_ad);
			// If job_managed is true, then notify the gridmanager and return.
			// If job_managed is false, we will fall through the code at the
			// bottom of this function will handle the operation.
			// Special case: if job_managed and job_managed_done are false,
			// but the job is being removed and the remote job id string is
			// still valid, then consider the job still "managed" so
			// that the gridmanager will be notified.  
			// If the remote job id is still valid, that means there is
			// still a job remotely submitted that has not been removed.  When
			// the gridmanager confirms a job has been removed, it will
			// delete ATTR_GRID_JOB_ID from the ad and set Managed to
			// ScheddDone.
		if ( !job_managed && !job_managed_done && mode==REMOVED ) {
			if ( job_ad->LookupString( ATTR_GRID_JOB_ID, NULL, 0 ) )
			{
				// looks like the job's remote job id is still valid,
				// so there is still a job submitted remotely somewhere.
				// fire up the gridmanager to try and really clean it up!
				job_managed = true;
			}
		}
		if ( job_managed  ) {
			UserIdentity userident(*job_ad);
			GridUniverseLogic::JobRemoved(userident.username().c_str(),
					userident.osname().c_str(),
					userident.auxid().c_str(),
					scheduler.getGridUnparsedSelectionExpr(),
					0,0);
			return;
		}
	}

	bool wantPS = 0;
	job_ad->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING, wantPS);

	if( (job_universe == CONDOR_UNIVERSE_MPI) || 
		(job_universe == CONDOR_UNIVERSE_PARALLEL) || wantPS ) {
		job_id.proc = 0;		// Parallel and MPI shadow is always associated with proc 0
	} 

	// If it is not a Globus Universe job (which has already been
	// dealt with above), then find the process/shadow managing it.
	if ((job_universe != CONDOR_UNIVERSE_GRID) && 
		(srec = scheduler.FindSrecByProcID(job_id)) != NULL) 
	{
		if( srec->pid == 0 ) {
				// there's no shadow process, so there's nothing to
				// kill... we hit this case when we fail to expand a
				// $$() attribute in the job, and put the job on hold
				// before we exec the shadow.
			dprintf ( D_ALWAYS, "abort_job_myself() - No shadow record found\n");
			return;
		}

		// if we have already preempted this shadow, we're done.
		if ( srec->preempted ) {
			dprintf ( D_ALWAYS, "abort_job_myself() - already preempted\n");
			return;
		}

		if( job_universe == CONDOR_UNIVERSE_LOCAL ) {
				/*
				  eventually, we'll want the cases for the other
				  universes with regular shadows to work more like
				  this.  for now, the starter is smarter about hold
				  vs. rm vs. vacate kill signals than the shadow is.
				  -Derek Wright <wright@cs.wisc.edu> 2004-10-28
				*/
			dprintf( D_FULLDEBUG, "Found shadow record for job %d.%d\n",
					 job_id.cluster, job_id.proc );

			int handler_sig=0;
			switch( action ) {
			case JA_HOLD_JOBS:
				handler_sig = SIGUSR1;
				break;
			case JA_REMOVE_JOBS:
				handler_sig = SIGUSR1;
				break;
			case JA_VACATE_JOBS:
				handler_sig = DC_SIGSOFTKILL;
				break;
			case JA_VACATE_FAST_JOBS:
				handler_sig = DC_SIGHARDKILL;
				break;
			case JA_SUSPEND_JOBS:
			case JA_CONTINUE_JOBS:
				dprintf( D_ALWAYS,
						 "Local universe: Ignoring unsupported action (%d %s)\n",
						 action, getJobActionString(action) );
				return;
				break;
			default:
				EXCEPT( "unknown action (%d %s) in abort_job_myself()",
						action, getJobActionString(action) );
			}

			scheduler.sendSignalToShadow(srec->pid,handler_sig,job_id);

		} else if( job_universe != CONDOR_UNIVERSE_SCHEDULER ) {
            
                /* if there is a match printout the info */
			if (srec->match) {
				dprintf( D_FULLDEBUG,
                         "Found shadow record for job %d.%d, host = %s\n",
                         job_id.cluster, job_id.proc, srec->match->peer);
			} else {
                dprintf(D_FULLDEBUG, "Found shadow record for job %d.%d\n",
                        job_id.cluster, job_id.proc);
				dprintf( D_FULLDEBUG, "This job does not have a match\n");
            }
			int shadow_sig=0;
			const char* shadow_sig_str = "UNKNOWN";
			switch( action ) {
			case JA_HOLD_JOBS:
					// for now, use the same as remove
			case JA_REMOVE_JOBS:
				srec->preempt_pending = true;
				shadow_sig = SIGUSR1;
				shadow_sig_str = "SIGUSR1";
				break;
			case JA_SUSPEND_JOBS:
				shadow_sig = DC_SIGSUSPEND;
				shadow_sig_str = "DC_SIGSUSPEND";
				break;
			case JA_CONTINUE_JOBS:
				shadow_sig = DC_SIGCONTINUE;
				shadow_sig_str = "DC_SIGCONTINUE";
				break;
			case JA_VACATE_JOBS:
				srec->preempt_pending = true;
				shadow_sig = SIGTERM;
				shadow_sig_str = "SIGTERM";
				break;
			case JA_VACATE_FAST_JOBS:
				srec->preempt_pending = true;
				shadow_sig = SIGQUIT;
				shadow_sig_str = "SIGQUIT";
				break;
			default:
				EXCEPT( "unknown action (%d %s) in abort_job_myself()",
						action, getJobActionString(action) );
			}
			
			dprintf( D_FULLDEBUG, "Sending %s to shadow\n", shadow_sig_str );
			scheduler.sendSignalToShadow(srec->pid,shadow_sig,job_id);
            
        } else {  // Scheduler universe job
            
            dprintf( D_FULLDEBUG,
                     "Found record for scheduler universe job %d.%d\n",
                     job_id.cluster, job_id.proc);
            
			const OwnerInfo * owni = job_ad->ownerinfo;
			if (! init_user_ids(owni) ) {
				std::string owner = owni ? owni->Name() : "";
				if (owni && owni->NTDomain()) { owner += "@"; owner += owni->NTDomain(); }
				std::string msg;
				dprintf(D_ALWAYS, "init_user_ids(%s) failed - putting job on hold.\n", owner.c_str());
#ifdef WIN32
				formatstr(msg, "Bad or missing credential for user: %s", owner.c_str());
#else
				formatstr(msg, "Unable to switch to user: %s", owner.c_str());
#endif
				holdJob(job_id.cluster, job_id.proc, msg.c_str(), 
						CONDOR_HOLD_CODE::FailedToAccessUserAccount, 0,
					false, true);
				return;
			}
			int kill_sig = -1;
			switch( action ) {

			case JA_HOLD_JOBS:
				kill_sig = findHoldKillSig( job_ad );
				break;

			case JA_REMOVE_JOBS:
				kill_sig = findRmKillSig( job_ad );
				break;

			case JA_VACATE_JOBS:
				kill_sig = findSoftKillSig( job_ad );
				break;

			case JA_VACATE_FAST_JOBS:
				kill_sig = SIGKILL;
				break;

			case JA_SUSPEND_JOBS:
			case JA_CONTINUE_JOBS:
				dprintf( D_ALWAYS,
						 "Scheduler universe: Ignoring unsupported action (%d %s)\n",
						 action, getJobActionString(action) );
				uninit_user_ids();
				return;
				break;

			default:
				EXCEPT( "bad action (%d %s) in abort_job_myself()",
						(int)action, getJobActionString(action) );

			}

				// if we don't have an action-specific kill_sig yet,
				// fall back on the regular ATTR_KILL_SIG
			if( kill_sig <= 0 ) {
				kill_sig = findSoftKillSig( job_ad );
			}
				// if we still don't have anything, default to SIGTERM
			if( kill_sig <= 0 ) {
				kill_sig = SIGTERM;
			}
			const char* sig_name = signalName( kill_sig );
			if( ! sig_name ) {
				sig_name = "UNKNOWN";
			}
			dprintf( D_FULLDEBUG, "Sending %s signal (%s, %d) to "
					 "scheduler universe job pid=%d owner=%s\n",
					 getJobActionString(action), sig_name, kill_sig,
					 srec->pid, owni->Name() );
			priv_state priv = set_user_priv();

			scheduler.sendSignalToShadow(srec->pid,kill_sig,job_id);

			set_priv(priv);

			uninit_user_ids();
		}

		if (mode == REMOVED) {
			srec->removed = TRUE;
		}

		return;
    }

	// If we made it here, we did not find a shadow or other job manager 
	// process for this job.  Just handle the operation ourselves.

	// If there is a match record for this job, try to find a different
	// job to run on it.
	match_rec *mrec = scheduler.FindMrecByJobID(job_id);
	if( mrec ) {
		scheduler.FindRunnableJobForClaim( mrec );
	}

	if( mode == REMOVED ) {
		if( !scheduler.WriteAbortToUserLog(job_id) ) {
			dprintf( D_ALWAYS,"Failed to write abort event to the user log\n" );
		}
		DestroyProc( job_id.cluster, job_id.proc );
	}
	if( mode == HELD ) {
		if( log_hold && !scheduler.WriteHoldToUserLog(job_id) ) {
			dprintf( D_ALWAYS, 
					 "Failed to write hold event to the user log\n" ); 
		}
	}
}

/*
For a given job, determine if the schedd is the responsible
party for evaluating the job's periodic expressions.
The schedd is responsible if the job is scheduler
universe, globus universe with managed==false, or
any other universe when the job is idle or held.
*/

static int
ResponsibleForPeriodicExprs( JobQueueJob *jobad, int & status )
{
	int univ = jobad->Universe();
	bool managed = jobExternallyManaged(jobad);
	if ( managed ) {
		return 0;
	}

	// don't fetch status if the caller already fetched it.
	if (status < 0) {
		jobad->LookupInteger(ATTR_JOB_STATUS,status);
	}

		// Avoid evaluating periodic
		// expressions when the job is on hold for spooling
	if( status == HELD ) {
		int hold_reason_code = -1;
		jobad->LookupInteger(ATTR_HOLD_REASON_CODE,hold_reason_code);
		if( hold_reason_code == CONDOR_HOLD_CODE::SpoolingInput ) {
			dprintf(D_FULLDEBUG,"Skipping periodic expressions for job %d.%d, because hold reason code is '%d'\n",
				jobad->jid.cluster, jobad->jid.proc, hold_reason_code);
			return 0;
		}
	}

	if( univ==CONDOR_UNIVERSE_SCHEDULER || univ==CONDOR_UNIVERSE_LOCAL ) {
		return 1;
	} else if(univ==CONDOR_UNIVERSE_GRID) {
		if( status == REMOVED && !jobManagedDone(jobad) && jobad->LookupString(ATTR_GRID_JOB_ID, NULL, 0) ) {
			// Looks like the job's remote job id is still valid,
			// so there is still a job submitted remotely somewhere,
			// and the gridmanager hasn't said it's done with it.
			// Don't do policy evaluation or call DestroyProc() yet.
			return 0;
		}
		return 1;
	} else {
		switch(status) {
			case IDLE:
				return 1;
			case HELD:
			case COMPLETED:
			case REMOVED:
				if ( jobad->jid.cluster > 0 && jobad->jid.proc > -1 && 
					 scheduler.FindSrecByProcID(jobad->jid) )
				{
						// job removed/completed/held, but shadow still exists
					return 0;
				} else {
						// job removed/completed/held, and shadow is gone
					return 1;
				}
			default:
				return 0;
		}
	}
}

/*
For a given job, evaluate any periodic expressions
and abort, hold, or release the job as necessary.
*/

static int
PeriodicExprEval(JobQueueJob *jobad, const JOB_ID_KEY & /*jid*/, void * pvUser)
{
	int status=-1;
	if(!ResponsibleForPeriodicExprs(jobad, status)) return 1;

	int cluster = jobad->jid.cluster;
	int proc = jobad->jid.proc;
	if(cluster<0 || proc<0) return 1;

	// fetch status if the Responsible didn't, if no status, don't evaluate policy.
	if (status < 0) {
		jobad->LookupInteger(ATTR_JOB_STATUS,status);
		if(status<0) return 1;
	}

	UserPolicy & policy = *(UserPolicy*)pvUser;

	policy.ResetTriggers();
	int action = policy.AnalyzePolicy(*jobad, PERIODIC_ONLY, status);

	// Build a "reason" string for logging
	std::string reason;
	int reason_code;
	int reason_subcode;
	policy.FiringReason(reason,reason_code,reason_subcode);
	if ( reason == "" ) {
		reason = "Unknown user policy expression";
	}

	switch(action) {
		case REMOVE_FROM_QUEUE:
			if(status!=REMOVED) {
				abortJob( cluster, proc, reason.c_str(), true );
			}
			break;
		case HOLD_IN_QUEUE:
			if(status!=HELD) {
				holdJob(cluster, proc, reason.c_str(),
						reason_code, reason_subcode,
						true, false, false, false);
			}
			break;
		case RELEASE_FROM_HOLD:
			if(status==HELD) {
				releaseJob(cluster, proc, reason.c_str(), true);
			}
			break;
		case VACATE_FROM_RUNNING:
			PROC_ID job {cluster, proc};
			abort_job_myself(job, JA_VACATE_JOBS, true);
			break;
	}

	if ( (status == COMPLETED || status == REMOVED) &&
	     ! scheduler.FindSrecByProcID(jobad->jid) )
	{
		DestroyProc(cluster,proc);
	}

	return 1;
}

/*
For all of the jobs in the queue, evaluate the 
periodic user policy expressions.
*/

void
Scheduler::PeriodicExprHandler( int /* timerID */ )
{
	PeriodicExprInterval.setStartTimeNow();

	UserPolicy policy;
	policy.Init();
	WalkJobQueue2(PeriodicExprEval, &policy);

	PeriodicExprInterval.setFinishTimeNow();

	unsigned int time_to_next_run = PeriodicExprInterval.getTimeToNextRun();
	dprintf(D_FULLDEBUG,"Evaluated periodic expressions in %.3fs, "
			"scheduling next run in %us\n",
			PeriodicExprInterval.getLastDuration(),
			time_to_next_run);
	daemonCore->Reset_Timer( periodicid, time_to_next_run );
}


bool
jobPrepNeedsThread( int /* cluster */, int /* proc */ )
{
#ifdef WIN32
	// we never want to run in a thread on Win32, since
	// some of the stuff we do in the JobPrep thread
	// is NOT thread safe!!
	return false;
#endif 

	/*
	The only reason we might need a thread is to chown the sandbox.  However,
	currently jobIsSandboxed claims every vanilla-esque job is sandboxed.  So
	on heavily loaded machines, we're forking before and after every job.  This
	is creating a backlog of PIDs whose reapers callbacks need to be called and
	eventually causes too many PID collisions.  This has been hitting LIGO.  So
	for now, never do it in another thread.  Hopefully by cutting the number of
	fork()s for a single process from 3 (prep, shadow, cleanup) to 1, big
	sites pushing the limits will get a little breathing room.

	The chowning will still happen; that code path is always called.  It's just
	always called in the main thread, not in a new one.
	*/
	
	return false;
}


bool
jobCleanupNeedsThread( int /* cluster */, int /* proc */ )
{

#ifdef WIN32
	// we never want to run this in a thread on Win32, 
	// since much of what we do in here is NOT thread safe.
	return false;
#endif

	/*
	See jobPrepNeedsThread for why we don't ever use threads.
	*/
	return false;
}


/** Last chance to prep a job before it (potentially) starts

This is a last chance to do any final work before starting a
job handler (starting condor_shadow, handing off to condor_gridmanager,
etc).  May block for a long time, so you'll probably want to do this is
a thread.

What do we do here?  At the moment if the job has a "sandbox" directory
("condor_submit -s", Condor-C, or the SOAP interface) we chown it from
condor to the user.  In the future we might allocate a dynamic account here.
*/
int
aboutToSpawnJobHandler( int cluster, int proc, void* )
{
	ASSERT(cluster > 0);
	ASSERT(proc >= 0);

	ClassAd * job_ad = GetJobAd( cluster, proc );
	ASSERT( job_ad ); // No job ad?
	if( ! SpooledJobFiles::jobRequiresSpoolDirectory(job_ad) ) {
			// nothing more to do...
		FreeJobAd( job_ad );
		return TRUE;
	}

	SpooledJobFiles::createJobSpoolDirectory(job_ad,PRIV_USER);

	FreeJobAd(job_ad);

	return TRUE;
}


int
aboutToSpawnJobHandlerDone( int cluster, int proc, 
							void* shadow_record, int )
{
	shadow_rec* srec = (shadow_rec*)shadow_record;
	dprintf( D_FULLDEBUG, 
			 "aboutToSpawnJobHandler() completed for job %d.%d%s\n",
			 cluster, proc, 
			 srec ? ", attempting to spawn job handler" : "" );

		// just to be safe, check one more time to make sure the job
		// is still runnable.
	int status;
	if( ! scheduler.isStillRunnable(cluster, proc, status) ) {
		if( status != -1 ) {  
			PROC_ID job_id;
			job_id.cluster = cluster;
			job_id.proc = proc;
			mark_job_stopped( &job_id );
		}
		if( srec ) {
			scheduler.delete_shadow_rec(srec);
		}
		return FALSE;
	}

	if( srec && srec->recycle_shadow_stream ) {
		scheduler.finishRecycleShadow( srec );
		return TRUE;
	}

	return (int)scheduler.spawnJobHandler( cluster, proc, srec );
}


void
callAboutToSpawnJobHandler( int cluster, int proc, shadow_rec* srec )
{
	if( jobPrepNeedsThread(cluster, proc) ) {
		dprintf( D_FULLDEBUG, "Job prep for %d.%d will block, "
				 "calling aboutToSpawnJobHandler() in a thread\n",
				 cluster, proc );
		Create_Thread_With_Data( aboutToSpawnJobHandler,
								 aboutToSpawnJobHandlerDone,
								 cluster, proc, srec );
	} else {
		dprintf( D_FULLDEBUG, "Job prep for %d.%d will not block, "
				 "calling aboutToSpawnJobHandler() directly\n",
				 cluster, proc );
		aboutToSpawnJobHandler( cluster, proc, srec );
		aboutToSpawnJobHandlerDone( cluster, proc, srec, 0 );
	}
}


bool
Scheduler::spawnJobHandler( int cluster, int proc, shadow_rec* srec )
{
	int universe = 0;
	if( srec ) {
		universe = srec->universe;
	} else {
		GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &universe );
	}
	PROC_ID job_id;
	job_id.cluster = cluster;
	job_id.proc = proc;

	switch( universe ) {

	case CONDOR_UNIVERSE_SCHEDULER:
			// there's no handler in this case, we just spawn directly
		ASSERT( srec == NULL );
		return( start_sched_universe_job(&job_id) != NULL );
		break;

	case CONDOR_UNIVERSE_LOCAL:
		if (!scheduler.m_use_startd_for_local) {
			scheduler.spawnLocalStarter( srec );
			return true;
		} 
		break;

	case CONDOR_UNIVERSE_GRID:
			// grid universe is special, since we handle spawning
			// gridmanagers in a different way, and don't need to do
			// anything here.
		ASSERT( srec == NULL );
		return true;
		break;
		
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
			// There's only one shadow, for all the procs, and it
			// is associated with procid 0.  Assume that if we are
			// passed procid > 0, we've already spawned the one
			// shadow this whole cluster needs
		if (proc > 0) {
			return true;
		}
		break;
	default:
		bool wantPS = 0;
		GetAttributeBool( cluster, proc, ATTR_WANT_PARALLEL_SCHEDULING, &wantPS );
		if (wantPS && (proc > 0)) {
			return true;
		}
		break;
	}
	ASSERT( srec != NULL );

		// if we're still here, make sure we have a match since we
		// have to spawn a shadow...
	if( srec->match ) {
		scheduler.spawnShadow( srec );
		return true;
	}

			// no match: complain and then try the next job...
	dprintf( D_ALWAYS, "match for job %d.%d was deleted - not "
			 "forking a shadow\n", srec->job_id.cluster, 
			 srec->job_id.proc );
	mark_job_stopped( &(srec->job_id) );
	delete_shadow_rec( srec );
	return false;
}


int
jobIsFinished( int cluster, int proc, void* )
{
		// this is (roughly) the inverse of aboutToSpawnHandler().
		// this method gets called whenever the job enters a finished
		// job state (REMOVED or COMPLETED) and the job handler has
		// finally exited.  this is where we should do any clean-up we
		// want now that the job is never going to leave this state...

	ASSERT( cluster > 0 );
	ASSERT( proc >= 0 );

	ClassAd * job_ad = GetJobAd( cluster, proc );
	if( ! job_ad ) {
			/*
			  evil, someone managed to call DestroyProc() before we
			  had a chance to work our magic.  for whatever reason,
			  that call succeeded (though it shouldn't in the usual
			  sandbox case), and now we've got nothing to work with.
			  in this case, we've just got to bail out.
			*/
		dprintf( D_FULLDEBUG, 
				 "jobIsFinished(): %d.%d already left job queue\n",
				 cluster, proc );
		return 0;
	}

#ifndef WIN32
		/* For jobs whose Iwd is on NFS, create and unlink a file in the
		   Iwd. This should force the NFS client to sync with the NFS
		   server and see any files in the directory that were written
		   on a different machine.
		*/
	std::string iwd;
	std::string owner;
	bool is_nfs;
	bool want_flush = false;

	job_ad->LookupBool( ATTR_JOB_IWD_FLUSH_NFS_CACHE, want_flush );
	if ( job_ad->LookupString( ATTR_OWNER, owner ) &&
		 job_ad->LookupString( ATTR_JOB_IWD, iwd ) &&
		 want_flush &&
		 fs_detect_nfs( iwd.c_str(), &is_nfs ) == 0 && is_nfs ) {

		priv_state priv;

		dprintf( D_FULLDEBUG, "(%d.%d) Forcing NFS sync of Iwd\n", cluster,
				 proc );

			// We're not Windows, so we don't need the NT Domain
		if ( !init_user_ids( owner.c_str(), NULL ) ) {
			dprintf( D_ALWAYS, "init_user_ids() failed for user %s!\n",
					 owner.c_str() );
		} else {
			int sync_fd;
			std::string filename_template;
			char *sync_filename;

			priv = set_user_priv();

			formatstr( filename_template, "%s/.condor_nfs_sync_XXXXXX",
									   iwd.c_str() );
			sync_filename = strdup( filename_template.c_str() );
			sync_fd = condor_mkstemp( sync_filename );
			if ( sync_fd >= 0 ) {
				close( sync_fd );
				unlink( sync_filename );
			}

			free( sync_filename );

			set_priv( priv );

			uninit_user_ids();
		}
	}
#endif /* WIN32 */


		// Write the job ad file to the sandbox. This work is done
		// here instead of with AppendHistory in DestroyProc/Cluster
		// because we want to be sure that the job's sandbox exists
		// when we try to write the job ad file to it. In the case of
		// spooled jobs, AppendHistory is only called after the spool
		// has been deleted, which means there is no place for us to
		// write the job ad. Also, generally for jobs that use
		// ATTR_JOB_LEAVE_IN_QUEUE the job ad file would not be
		// written until the job leaves the queue, which would
		// unnecessarily delay the create of the job ad file. At this
		// time the only downside to dropping the file here in the
		// code is that any attributes that change between the
		// completion of a job and its removal from the queue would
		// not be present in the job ad file, but that should be of
		// little consequence.
	WriteCompletionVisa(job_ad);

	// Remove related Jobs
	removeOtherJobs(cluster, proc);


		/*
		  make sure we can switch uids.  if not, there's nothing to
		  do, so we should exit right away.

		  WARNING: if we ever add anything to this function that
		  doesn't require root/admin privledges, we'll also need to
		  change jobCleanupNeedsThread()!
		*/
	if( ! can_switch_ids() ) {
		return 0;
	}

#ifndef WIN32

	if( SpooledJobFiles::jobRequiresSpoolDirectory(job_ad) ) {
		SpooledJobFiles::chownSpoolDirectoryToCondor(job_ad);
	}

#else	/* WIN32 */

// #    error "directory chowning on Win32.  Do we need it?"

#endif

	// release dynamic accounts here

	FreeJobAd( job_ad );
	job_ad = NULL;

	return 0;
}


/**
Returns 0 or positive number on success.
negative number on failure.
*/
int
jobIsFinishedDone( int cluster, int proc, void*, int )
{
	dprintf( D_FULLDEBUG,
			 "jobIsFinished() completed, calling DestroyProc(%d.%d)\n",
			 cluster, proc );
	SetAttributeInt( cluster, proc, ATTR_JOB_FINISHED_HOOK_DONE,
					 time(nullptr), NONDURABLE);
	return DestroyProc( cluster, proc );
}

// Initialize a WriteUserLog object for a given job and return a pointer to
// the WriteUserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a WriteUserLog, so you must check for NULL before
// using the pointer you get back.
WriteUserLog*
Scheduler::InitializeUserLog( PROC_ID job_id ) 
{
	ClassAd *ad = GetJobAd(job_id.cluster,job_id.proc);

	if (!ad) {
		dprintf(D_ALWAYS, "Scheduler::InitializeUserLog got null job ad, probably job already removed?\n");
		return nullptr;
	}
	WriteUserLog* ULog=new WriteUserLog();
	ULog->setCreatorName( Name );

    if (m_userlog_file_cache_max > 0) {
        // This is a bit draconian, but doing it smarter requires more machinery and data,
        // and the log cache can still save plenty of open/close
        if (m_userlog_file_cache.size() >= size_t(m_userlog_file_cache_max)) userlog_file_cache_clear(true);

        // important to do this before invoking initialize() method
        dprintf(D_FULLDEBUG, "Scheduler::InitializeUserLog(): setting log file cache\n");
        ULog->setLogFileCache(&m_userlog_file_cache);
    }

	if ( ! ULog->initialize(*ad, true) ) {
		dprintf ( D_ALWAYS, "WARNING: Failed to initialize user log file for writing!\n");
		delete ULog;
		return NULL;
	}
	// TODO Callers can't distinguish between error and no log files to write to.
	if ( ! ULog->willWrite() ) {
		delete ULog;
		return NULL;
	}

	return ULog;
}

bool
Scheduler::WriteSubmitToUserLog( JobQueueJob* job, bool do_fsync, const char * warning )
{
		// Skip writing submit events for procid != 0 for parallel jobs
	int universe = job->Universe();
	bool wantPS = 0;
	job->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING, wantPS);

	if ( (universe == CONDOR_UNIVERSE_PARALLEL) || wantPS ) {
		if ( job->jid.proc > 0) {
			return true;;
		}
	}

	WriteUserLog* ULog = this->InitializeUserLog( job->jid );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	SubmitEvent event;

	event.setSubmitHost( daemonCore->privateNetworkIpAddr() );
	job->LookupString(ATTR_SUBMIT_EVENT_NOTES, event.submitEventLogNotes);
	job->LookupString(ATTR_SUBMIT_EVENT_USER_NOTES, event.submitEventUserNotes);
	if ( warning != NULL && warning[0] ) {
		event.submitEventWarnings = warning;
	}

	bool status = false;
	if (do_fsync) {
		double startTime = _condor_debug_get_time_double();
		status = ULog->writeEvent(&event, job);
		this->FsyncRuntimes[job->ownerinfo->Name()] += _condor_debug_get_time_double() - startTime;
	} else {
		status = ULog->writeEventNoFsync(&event, job);
	}
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_SUBMIT event for job %d.%d\n",
				 job->jid.cluster, job->jid.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteAbortToUserLog( PROC_ID job_id )
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobAbortedEvent event;

	std::string reasonstr;
	GetAttributeString(job_id.cluster, job_id.proc,
	                   ATTR_REMOVE_REASON, reasonstr);
	event.setReason(reasonstr);

	// Jobs usually have a shadow, and this event is usually written after
	// that shadow dies, but that's by no means certain.  If we happen to
	// have gotten a ToE tag, tell the abort event about it.
	ClassAd * ja = GetJobAd( job_id.cluster, job_id.proc );
	if( ja ) {
		classad::ClassAd * toeTag = dynamic_cast<classad::ClassAd*>(ja->Lookup(ATTR_JOB_TOE));
		event.setToeTag( toeTag );
	}

	bool status =
		ULog->writeEvent(&event, GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

    userlog_file_cache_erase(job_id.cluster, job_id.proc);

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_JOB_ABORTED event for job %d.%d\n",
				 job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteHoldToUserLog( PROC_ID job_id )
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobHeldEvent event;

	std::string reasonstr;
	if( GetAttributeString(job_id.cluster, job_id.proc,
	                       ATTR_HOLD_REASON, reasonstr) < 0 ) {
		dprintf( D_ALWAYS, "Scheduler::WriteHoldToUserLog(): "
				 "Failed to get %s from job %d.%d\n", ATTR_HOLD_REASON,
				 job_id.cluster, job_id.proc );
	}
	event.setReason(reasonstr);

	GetAttributeInt(job_id.cluster, job_id.proc,
	                ATTR_HOLD_REASON_CODE, &event.code);

	GetAttributeInt(job_id.cluster, job_id.proc,
	                ATTR_HOLD_REASON_SUBCODE, &event.subcode);

	bool status =
		ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

    userlog_file_cache_erase(job_id.cluster, job_id.proc);

	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_HELD event for job %d.%d\n",
				 job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteReleaseToUserLog( PROC_ID job_id )
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobReleasedEvent event;

	std::string reasonstr;
	GetAttributeString(job_id.cluster, job_id.proc,
	                   ATTR_RELEASE_REASON, reasonstr);
	event.setReason(reasonstr);

	bool status =
		ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_JOB_RELEASED event for job %d.%d\n",
				 job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteExecuteToUserLog( PROC_ID job_id, const char* sinful )
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}

	const char* host;
	if( sinful ) {
		host = sinful;
	} else {
		host = daemonCore->privateNetworkIpAddr();
	}

	ExecuteEvent event;
	event.setExecuteHost( host );
	bool status =
		ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;
	
	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event for job %d.%d\n",
				job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteEvictToUserLog( PROC_ID job_id, bool checkpointed ) 
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobEvictedEvent event;
	event.checkpointed = checkpointed;

	ClassAd *jobAd = GetJobAd(job_id.cluster, job_id.proc);
	if (!jobAd) {
		dprintf(D_ALWAYS, "Unable to write evict event to job log for job (%d.%d) -- perhaps it was already removed\n",
				job_id.cluster, job_id.proc);
		return false;
	}

	bool status =
		ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

    userlog_file_cache_erase(job_id.cluster, job_id.proc);

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_JOB_EVICTED event for job %d.%d\n",
				 job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteTerminateToUserLog( PROC_ID job_id, int status ) 
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobTerminatedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof(struct rusage) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
	event.total_local_rusage = r;
	event.total_remote_rusage = r;
#endif /* LOOSE32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;
	event.total_sent_bytes = 0;
	event.total_recvd_bytes = 0;

	if( WIFEXITED(status) ) {
			// Normal termination
		event.normal = true;
		event.returnValue = WEXITSTATUS(status);
	} else {
		event.normal = false;
		event.signalNumber = WTERMSIG(status);
	}
	bool rval = ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

    userlog_file_cache_erase(job_id.cluster, job_id.proc);

	if (!rval) {
		dprintf( D_ALWAYS, 
				 "Unable to log ULOG_JOB_TERMINATED event for job %d.%d\n",
				 job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}

bool
Scheduler::WriteRequeueToUserLog( PROC_ID job_id, int status, const char * reason ) 
{
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobEvictedEvent event;
	event.terminate_and_requeued = true;
	struct rusage r;
	memset( &r, 0, sizeof(struct rusage) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
#endif /* LOOSE32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;

	if( WIFEXITED(status) ) {
			// Normal termination
		event.normal = true;
		event.return_value = WEXITSTATUS(status);
	} else {
		event.normal = false;
		event.signal_number = WTERMSIG(status);
	}
	if (reason && reason[0]) {
		event.setReason(reason);
	}
	bool rval = ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

	if (!rval) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED (requeue) event "
				 "for job %d.%d\n", job_id.cluster, job_id.proc );
		return false;
	}
	return true;
}


bool
Scheduler::WriteAttrChangeToUserLog( const char* job_id_str, const char* attr,
					 const char* attr_value,
					 const char* old_value)
{
	PROC_ID job_id;
	StrToProcIdFixMe(job_id_str, job_id);
	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}

	AttributeUpdate event;

	event.setName(attr);
	event.setValue(attr_value);
	event.setOldValue(old_value);
        bool rval = ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
        delete ULog;

        if (!rval) {
                dprintf( D_ALWAYS, "Unable to log ULOG_ATTRIBUTE_UPDATE event "
                                 "for job %d.%d\n", job_id.cluster, job_id.proc );
                return false;
        }

	return true;
}

bool
Scheduler::WriteClusterSubmitToUserLog( JobQueueCluster* cluster, bool do_fsync )
{
	WriteUserLog* ULog = this->InitializeUserLog( cluster->jid );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	ClusterSubmitEvent event;

	event.setSubmitHost( daemonCore->privateNetworkIpAddr() );
	cluster->LookupString(ATTR_SUBMIT_EVENT_NOTES, event.submitEventLogNotes);
	cluster->LookupString(ATTR_SUBMIT_EVENT_USER_NOTES, event.submitEventUserNotes);

	bool status = false;
	if (do_fsync) {
		status = ULog->writeEvent(&event, cluster);
	} else {
		status = ULog->writeEventNoFsync(&event, cluster);
	}
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_CLUSTER_SUBMIT event for job %d.%d\n",
				 cluster->jid.cluster, cluster->jid.proc );
		return false;
	}
	return true;
}

bool
Scheduler::WriteClusterRemoveToUserLog( JobQueueCluster* cluster, bool do_fsync )
{
	WriteUserLog* ULog = this->InitializeUserLog( cluster->jid );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	ClusterRemoveEvent event;

	cluster->LookupString(ATTR_JOB_MATERIALIZE_PAUSE_REASON, event.notes);

	int code = 0;
	GetJobFactoryMaterializeMode(cluster, code);
	switch (code) {
	case mmInvalid: event.completion = ClusterRemoveEvent::CompletionCode::Error; break;
	case mmRunning: event.completion = ClusterRemoveEvent::CompletionCode::Incomplete; break;
	case mmHold: event.completion = ClusterRemoveEvent::CompletionCode::Paused; break;
	case mmNoMoreItems: event.completion = ClusterRemoveEvent::CompletionCode::Complete; break;
	}
	cluster->LookupInteger(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, event.next_proc_id);
	cluster->LookupInteger(ATTR_JOB_MATERIALIZE_NEXT_ROW, event.next_row);

	bool status = false;
	if (do_fsync) {
		status = ULog->writeEvent(&event, cluster);
	} else {
		status = ULog->writeEventNoFsync(&event, cluster);
	}
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_CLUSTER_REMOVE event for job %d.%d\n",
				 cluster->jid.cluster, cluster->jid.proc );
		return false;
	}
	return true;
}

bool
Scheduler::WriteFactoryPauseToUserLog( JobQueueCluster* cluster, int hold_code, const char * reason, bool do_fsync )
{
	WriteUserLog* ULog = this->InitializeUserLog( cluster->jid );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}

	int code = mmRunning;
	if ( ! cluster->LookupInteger(ATTR_JOB_MATERIALIZE_PAUSED, code)) {
		// can't figure out the pause mode of the factory.
		return false;
	}

	// if no reason supplied as an argument, use the one in the cluster ad (if any)
	std::string reason_buf;
	if ( ! reason) {
		cluster->LookupString(ATTR_JOB_MATERIALIZE_PAUSE_REASON, reason_buf);
		if ( ! reason_buf.empty()) { reason = reason_buf.c_str(); }
	}

	bool status;

	if (code != mmRunning) {
		// factory is paused.
		FactoryPausedEvent event;
		if (reason) event.setReason(reason);
		// if the code is mmInvalid, there is no hold code, just use the pause reason code (for now)
		if (code == mmInvalid) {
			event.setPauseCode(code);
		} else if (code == mmHold) {
			// we expect to get here only when code == mmHold, so use the hold_code as the code value
			// the reason text will also be the hold reason text.
			// (the other codes are mmNoMoreItems && mmClusterRemoved, which we don't log)
			event.setPauseCode(code);
			event.setHoldCode(hold_code);
		} else {
			#if 1 //def DEBUG
			// in debug mode, always write the pause code
			event.setPauseCode(code);
			#endif
		}
		if (do_fsync) {
			status = ULog->writeEvent(&event,cluster);
		} else {
			status = ULog->writeEventNoFsync(&event,cluster);
		}
	} else {
		// factory is resumed.
		FactoryResumedEvent event;
		if (reason) event.setReason(reason);

		if (do_fsync) {
			status = ULog->writeEvent(&event,cluster);
		} else {
			status = ULog->writeEventNoFsync(&event,cluster);
		}
	}

	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS, "Unable to log %s event for job %d.%d\n", 
			(code != mmRunning) ? "ULOG_FACTORY_PAUSED" : "ULOG_FACTORY_RESUMED", 
			cluster->jid.cluster, cluster->jid.proc );
		return false;
	}
	return true;
}


int
Scheduler::transferJobFilesReaper(int tid,int exit_status)
{
	dprintf(D_FULLDEBUG,"transferJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

		// find the list of jobs which we just finished receiving the files
	auto spit = spoolJobFileWorkers.find(tid);

	if (spit == spoolJobFileWorkers.end()) {
		dprintf(D_ALWAYS,
			"ERROR - transferJobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}
	std::vector<PROC_ID> *jobs = spit->second;;


	if (WIFSIGNALED(exit_status) || (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) != TRUE)) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		delete jobs;
		spoolJobFileWorkers.erase(spit);
		return false;
	}

		// For each job, modify its ClassAd
	time_t now = time(nullptr);
	int len = (*jobs).size();
	for (int i=0; i < len; i++) {
			// TODO --- maybe put this in a transaction?
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,ATTR_STAGE_OUT_FINISH,now);
	}

		// Now, deallocate memory
	spoolJobFileWorkers.erase(spit);
	delete jobs;
	return true;
}

int
Scheduler::spoolJobFilesReaper(int tid,int exit_status)
{
	dprintf(D_FULLDEBUG,"spoolJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

	time_t now = time(NULL);

		// find the list of jobs which we just finished receiving the files
	auto spit = spoolJobFileWorkers.find(tid);

	if (spit == spoolJobFileWorkers.end()) {
		dprintf(D_ALWAYS,"ERROR - JobFilesReaper no entry for tid %d\n",tid);
		return false;
	}

	std::vector<PROC_ID> *jobs = spit->second;;

	if (WIFSIGNALED(exit_status) || (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) != TRUE)) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers.erase(spit);
		size_t len = (*jobs).size();
		for(size_t jobIndex = 0; jobIndex < len; ++jobIndex) {
			int cluster = (*jobs)[jobIndex].cluster;
			int proc = (*jobs)[jobIndex].proc;
			abortJob( cluster, proc, "Staging of job files failed", true);
		}
		delete jobs;
		return FALSE;
	}


	size_t jobIndex;
	int cluster,proc;
	size_t len = (*jobs).size();
	std::string proxy_file;

		// For each job, modify its ClassAd
	for (jobIndex = 0; jobIndex < len; jobIndex++) {
		cluster = (*jobs)[jobIndex].cluster;
		proc = (*jobs)[jobIndex].proc;

		BeginTransaction();

			// Set ATTR_STAGE_IN_FINISH if not already set.
		time_t spool_completion_time = 0;
		GetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,&spool_completion_time);
		if ( !spool_completion_time ) {
			// The transfer thread specifically slept for 1 second
			// to ensure that the job can't possibly start (and finish)
			// prior to the timestamps on the file.  Unfortunately,
			// we note the transfer finish time _here_.  So we've got 
			// to back off 1 second.
			SetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,now - 1);
		}

		if (GetAttributeString(cluster, proc, ATTR_X509_USER_PROXY, proxy_file) == 0) {
			std::string owner;
			std::string iwd;
			std::string full_file;
			ClassAd proxy_attrs;
			GetAttributeString(cluster, proc, ATTR_OWNER, owner);
			GetAttributeString(cluster, proc, ATTR_JOB_IWD, iwd);
			formatstr(full_file, "%s%c%s", iwd.c_str(), DIR_DELIM_CHAR, proxy_file.c_str());

			if ( ReadProxyFileIntoAd(full_file.c_str(), owner.c_str(), proxy_attrs) ) {
				UpdateJobProxyAttrs((*jobs)[jobIndex], proxy_attrs);
			}
		}

			// And now release the job.
		releaseJob(cluster,proc,"Data files spooled",false,false,false,false);
		CommitTransactionOrDieTrying();
	}

	daemonCore->Register_Timer( 0, 
						(TimerHandlercpp)&Scheduler::reschedule_negotiator_timer,
						"Scheduler::reschedule_negotiator", this );

	spoolJobFileWorkers.erase(spit);
	delete jobs;
	return TRUE;
}

int
Scheduler::transferJobFilesWorkerThread(void *arg, Stream* s)
{
	return generalJobFilesWorkerThread(arg,s);
}

int
Scheduler::spoolJobFilesWorkerThread(void *arg, Stream* s)
{
	int ret_val;
	ret_val = generalJobFilesWorkerThread(arg,s);
		// Now we sleep here for one second.  Why?  So we are certain
		// to transfer back output files even if the job ran for less 
		// than one second. This is because:
		// stat() can't tell the difference between:
		//   1) A job starts up, touches a file, and exits all in one second
		//   2) A job starts up, doesn't touch the file, and exits all in one 
		//      second
		// So if we force the start time of the job to be one second later than
		// the time we know the files were written, stat() should be able
		// to perceive what happened, if anything.
		dprintf(D_ALWAYS,"Scheduler::spoolJobFilesWorkerThread(void *arg, Stream* s) NAP TIME\n");
	sleep(1);
	return ret_val;
}

int
Scheduler::generalJobFilesWorkerThread(void *arg, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	int i;
	std::vector<PROC_ID> *jobs = ((job_data_transfer_t *)arg)->jobs;
	char *peer_version = ((job_data_transfer_t *)arg)->peer_version;
	int mode = ((job_data_transfer_t *)arg)->mode;
	int result;
	time_t old_timeout;
	int cluster, proc;
	
	/* Setup a large timeout; when lots of jobs are being submitted w/ 
	 * large sandboxes, the default is WAY to small...
	 */
	old_timeout = s->timeout(60 * 60 * 8);  

	priv_state xfer_priv = PRIV_UNKNOWN;
#if !defined(WIN32)
	if ( param_boolean( "CHOWN_JOB_SPOOL_FILES", false ) == false ) {
		xfer_priv = PRIV_USER;
	}
#endif

	JobAdsArrayLen = jobs->size();
	if ( mode == TRANSFER_DATA || mode == TRANSFER_DATA_WITH_PERMS ) {
		// if sending sandboxes, first tell the client how many
		// we are about to send.
		dprintf(D_FULLDEBUG, "Scheduler::generalJobFilesWorkerThread: "
			"TRANSFER_DATA/WITH_PERMS: %d jobs to be sent\n", JobAdsArrayLen);
		rsock->encode();
		if ( !rsock->code(JobAdsArrayLen) || !rsock->end_of_message() ) {
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "generalJobFilesWorkerThread(): "
					 "failed to send JobAdsArrayLen (%d) \n",
					 JobAdsArrayLen );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			return FALSE;
		}
	}
	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * ad = GetJobAd( cluster, proc );
		if ( !ad ) {
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "generalJobFilesWorkerThread(): "
					 "job ad %d.%d not found\n",cluster,proc );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

		dprintf(D_JOB, "The submitting job ad as the FileTransferObject sees it\n");
		dPrintAd(D_JOB, *ad);

#if !defined(WIN32)
		if ( xfer_priv == PRIV_USER ) {
			// If sending the output sandbox, first ensure that it's owned
			// by the user, in case we were using the old chowning behavior
			// when the job completed.
			if ( (mode == TRANSFER_DATA || mode == TRANSFER_DATA_WITH_PERMS) &&
				 SpooledJobFiles::jobRequiresSpoolDirectory( ad ) )
			{
				SpooledJobFiles::createJobSpoolDirectory( ad, PRIV_USER );
			}
			std::string owner;
			ad->LookupString( ATTR_OWNER, owner );
			if ( !init_user_ids( owner.c_str(), NULL ) ) {
				dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "generalJobFilesWorkerThread(): "
						 "failed to initialize user id for job %d.%d\n",
						 cluster, proc );
				s->timeout( 10 ); // avoid hanging due to huge timeout
				refuse(s);
				s->timeout(old_timeout);
				return FALSE;
			}
		}
#endif

			// Create a file transfer object, with schedd as the server.
			// If we're receiving files, don't create a file catalog in
			// the FileTransfer object. The submitter's IWD is probably not
			// valid on this machine and we won't use the catalog anyway.
		result = ftrans.SimpleInit(ad, true, true, rsock, xfer_priv,
								   (mode == TRANSFER_DATA ||
									mode == TRANSFER_DATA_WITH_PERMS));
		if ( !result ) {
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			s->timeout(old_timeout);
			if ( xfer_priv == PRIV_USER ) {
				uninit_user_ids();
			}
			return FALSE;
		}
		if ( peer_version[0] != '\0' ) {
			ftrans.setPeerVersion( peer_version );
		}

			// Send or receive files as needed
		if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
			// receive sandbox into the schedd
			result = ftrans.DownloadFiles();

			if ( result ) {
				TemporaryPrivSentry old_priv;
				if ( xfer_priv == PRIV_USER ) {
					set_user_priv();
				}
				AuditLogJobProxy( *rsock, ad );
			}
		} else {
			// send sandbox out of the schedd
			rsock->encode();
			// first send the classad for the job
			result = putClassAd(rsock, *ad);
			if (!result) {
				dprintf(D_AUDIT | D_ERROR_ALSO, *rsock, "generalJobFilesWorkerThread(): "
					"failed to send job ad for job %d.%d \n",
					cluster,proc );
			} else {
				rsock->end_of_message();
				// and then upload the files
				result = ftrans.UploadFiles();
			}
		}

#if !defined(WIN32)
		if ( xfer_priv == PRIV_USER ) {
			uninit_user_ids();
		}
#endif

		if ( !result ) {
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "generalJobFilesWorkerThread(): "
					 "failed to transfer files for job %d.%d \n",
					 cluster,proc );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
	}	
		
		
	rsock->end_of_message();

	int answer;
	if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
		rsock->encode();
		answer = OK;
	} else {
		rsock->decode();
		answer = -1;
	}
	if (!rsock->code(answer)) {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"cannot send answer to client\n");
	}
	rsock->end_of_message();
	s->timeout(old_timeout);

	/* for grid universe jobs there isn't a clear point
	at which we're "about to start the job".  So we just
	hand the sandbox directory over to the end user right now.
	*/
	if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
		for (i=0; i<JobAdsArrayLen; i++) {

			cluster = (*jobs)[i].cluster;
			proc = (*jobs)[i].proc;
			ClassAd * ad = GetJobAd( cluster, proc );

			if ( ! ad ) {
				dprintf(D_ALWAYS, "(%d.%d) Job ad disappeared after spooling but before the sandbox directory could (potentially) be chowned to the user.  Skipping sandbox.  The job may encounter permissions problems.\n", cluster, proc);
				continue;
			}

			int universe = CONDOR_UNIVERSE_VANILLA;
			ad->LookupInteger(ATTR_JOB_UNIVERSE, universe);
			FreeJobAd(ad);

			if(universe == CONDOR_UNIVERSE_GRID) {
				aboutToSpawnJobHandler( cluster, proc, NULL );
			}
		}
	}

	dprintf( D_AUDIT, *rsock, (answer==OK) ? "Transfer completed\n" :
			 "Error received from client\n" );
   return ((answer == OK)?TRUE:FALSE);
}

// This function is used BOTH for uploading and downloading files to the
// schedd. Which path selected is determined by the command passed to this
// function. This function should really be split into two different handlers,
// one for uploading the spool, and one for downloading it. 
int
Scheduler::spoolJobFiles(int mode, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	std::vector<PROC_ID> *jobs = NULL;
	char *constraint_string = NULL;
	int i;
	static int spool_reaper_id = -1;
	static int transfer_reaper_id = -1;
	PROC_ID a_job;
	int tid;
	std::string peer_version;
	std::string job_ids_string;

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 10 );  
	rsock->decode();

	if( ! rsock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD", SCHEDD_ERR_SPOOL_FILES_FAILED,
					"Failure to spool job files - Authentication failed" );
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "spoolJobFiles() aborting: %s\n",
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}	


	rsock->decode();

	switch(mode) {
		case SPOOL_JOB_FILES_WITH_PERMS: // uploading perm files to schedd
		case TRANSFER_DATA_WITH_PERMS:	// downloading perm files from schedd
			peer_version = "";
			if ( !rsock->code(peer_version) ) {
				dprintf(D_AUDIT | D_ERROR_ALSO, *rsock,
					 	"spoolJobFiles(): failed to read peer_version\n" );
				refuse(s);
				return FALSE;
			}
			break;

		default:
			// Non perm commands don't encode a peer version string
			break;
	}

#ifdef USE_JOB_QUEUE_USERREC
	const OwnerInfo * sock_owner = EffectiveUserRec(rsock);
#endif


	// Here the protocol differs somewhat between uploading and downloading.
	// So watch out in terms of understanding this.
	switch(mode) {
		// uploading files to schedd
		// decode the number of jobs I'm about to be sent, and verify the
		// number.
		case SPOOL_JOB_FILES:
		case SPOOL_JOB_FILES_WITH_PERMS:
			// read the number of jobs involved
			if ( !rsock->code(JobAdsArrayLen) ) {
				    dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "spoolJobFiles(): "
							 "failed to read JobAdsArrayLen (%d)\n",
							 JobAdsArrayLen );
					refuse(s);
					return FALSE;
			}
			if ( JobAdsArrayLen <= 0 ) {
				dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "spoolJobFiles(): "
					 	"read bad JobAdsArrayLen value %d\n", JobAdsArrayLen );
				refuse(s);
				return FALSE;
			}
			rsock->end_of_message();
			dprintf(D_FULLDEBUG,"spoolJobFiles(): read JobAdsArrayLen - %d\n",
					JobAdsArrayLen);
			break;

		// downloading files from schedd
		// Decode a constraint string which will be used to gather the jobs out
		// of the queue, which I can then determine what to transfer out of.
		case TRANSFER_DATA:
		case TRANSFER_DATA_WITH_PERMS:
			// read constraint string
			if ( !rsock->code(constraint_string) || constraint_string == NULL )
			{
					dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "spoolJobFiles(): "
						 	"failed to read constraint string\n" );
					refuse(s);
					return FALSE;
			}
			break;

		default:
			break;
	}

	jobs = new std::vector<PROC_ID>;
	ASSERT(jobs);

	time_t now = time(NULL);
	dprintf( D_FULLDEBUG, "Looking at spooling: mode is %d\n", mode);
	switch(mode) {
		// uploading files to schedd 
		case SPOOL_JOB_FILES:
		case SPOOL_JOB_FILES_WITH_PERMS:
			for (i=0; i<JobAdsArrayLen; i++) {
				if (!rsock->code(a_job)) {
					dprintf(D_ALWAYS, "spoolJobFiles(): cannot recv job from client\n");
				}
					// Only add jobs to our list if the caller has permission 
					// to do so.
					// cuz only the owner of a job (or queue super user) 
					// is allowed to transfer data to/from a job.
			#ifdef USE_JOB_QUEUE_USERREC
				JobQueueJob * job = GetJobAd(a_job);
				if (job && (job->IsJob() || job->IsCluster()) && UserCheck2(job, sock_owner)) {
			#else
				std::string job_user;
				GetAttributeString(a_job.cluster, a_job.proc, attr_JobUser.c_str(), job_user);
				if (UserCheck2(NULL, EffectiveUser(rsock), job_user.c_str())) {
			#endif
					jobs->emplace_back(a_job.cluster, a_job.proc);
					formatstr_cat(job_ids_string, "%d.%d, ", a_job.cluster, a_job.proc);

						// Must not allow stagein to happen more than
						// once, because this could screw up
						// subsequent operations, such as rewriting of
						// paths in the ClassAd and the job being in
						// the middle of using the files.
					time_t finish_time = 0;
					if( GetAttributeInt(a_job.cluster,a_job.proc,
					    ATTR_STAGE_IN_FINISH,&finish_time) >= 0 ) {
						dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "spoolJobFiles(): cannot allow"
						         " stagein for job %d.%d, because stagein"
						         " already finished for this job.\n",
						         a_job.cluster, a_job.proc);
						delete jobs;
						return FALSE;
					}
					int holdcode = -1;
					int job_status = -1;
					int job_status_result = GetAttributeInt(a_job.cluster,
						a_job.proc,ATTR_JOB_STATUS,&job_status);
					if( job_status_result >= 0 &&
							GetAttributeInt(a_job.cluster,a_job.proc,
							ATTR_HOLD_REASON_CODE,&holdcode) >= 0) {
						dprintf( D_FULLDEBUG, "job_status is %d\n", job_status);
						if(job_status == HELD &&
								holdcode != CONDOR_HOLD_CODE::SpoolingInput) {
							dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "Job %d.%d is not in hold state for "
								"spooling. Do not allow stagein\n",
								a_job.cluster, a_job.proc);
							delete jobs;
							return FALSE;
						}
					}
					SetAttributeInt(a_job.cluster,a_job.proc,
									ATTR_STAGE_IN_START,now);
				} else {
					// UserCheck2 failed.
					dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "spoolJobFiles(): cannot allow"
							" user %s to spool files for job %d.%d\n",
							EffectiveUserName(rsock), a_job.cluster, a_job.proc);
					delete jobs;
					return FALSE;
				}
			}
			break;

		// downloading files from schedd
		case TRANSFER_DATA:
		case TRANSFER_DATA_WITH_PERMS:
			{
			JobQueueJob * tmp_ad = GetNextJobByConstraint(constraint_string,1);
			JobAdsArrayLen = 0;
			while (tmp_ad) {
				a_job = tmp_ad->jid;
				if (UserCheck2(tmp_ad, sock_owner) )
				{
					jobs->emplace_back(a_job.cluster, a_job.proc);
					JobAdsArrayLen++;
					formatstr_cat(job_ids_string, "%d.%d, ", a_job.cluster, a_job.proc);
				}
				tmp_ad = GetNextJobByConstraint(constraint_string,0);
			}
			dprintf(D_FULLDEBUG, "Scheduler::spoolJobFiles: "
				"TRANSFER_DATA/WITH_PERMS: %d jobs matched constraint %s\n",
				JobAdsArrayLen, constraint_string);
			if (constraint_string) free(constraint_string);
				// Now set ATTR_STAGE_OUT_START
			for (i=0; i<JobAdsArrayLen; i++) {
					// TODO --- maybe put this in a transaction?
				SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,
								ATTR_STAGE_OUT_START,now);
			}
			}
			break;

		default:
			break;

	}

	rsock->end_of_message();

	if (job_ids_string.length() > 2) {
		job_ids_string.erase(job_ids_string.length()-2,2); //Get rid of the extraneous ", "
	}
	dprintf( D_AUDIT, *rsock, "Transferring files for jobs %s\n", 
			 job_ids_string.c_str());

		// DaemonCore will free the thread_arg for us when the thread
		// exits, 
	job_data_transfer_t *thread_arg = (job_data_transfer_t *)malloc( sizeof(job_data_transfer_t) + peer_version.length());
	ASSERT( thread_arg != NULL );
	thread_arg->mode = mode;
	thread_arg->peer_version[0] = '\0';
	strcpy(thread_arg->peer_version, peer_version.c_str());
	thread_arg->jobs = jobs;

	switch(mode) {
		// uploading files to the schedd
		case SPOOL_JOB_FILES:
		case SPOOL_JOB_FILES_WITH_PERMS:
			if ( spool_reaper_id == -1 ) {
				spool_reaper_id = daemonCore->Register_Reaper(
						"spoolJobFilesReaper",
						(ReaperHandlercpp) &Scheduler::spoolJobFilesReaper,
						"spoolJobFilesReaper",
						this
					);
			}


			// Start a new thread (process on Unix) to do the work
			tid = daemonCore->Create_Thread(
					(ThreadStartFunc) &Scheduler::spoolJobFilesWorkerThread,
					(void *)thread_arg,
					s,
					spool_reaper_id
					);
			break;

		// downloading files from the schedd
		case TRANSFER_DATA:
		case TRANSFER_DATA_WITH_PERMS:
			if ( transfer_reaper_id == -1 ) {
				transfer_reaper_id = daemonCore->Register_Reaper(
						"transferJobFilesReaper",
						(ReaperHandlercpp) &Scheduler::transferJobFilesReaper,
						"transferJobFilesReaper",
						this
					);
			}

			// Start a new thread (process on Unix) to do the work
			tid = daemonCore->Create_Thread(
					(ThreadStartFunc) &Scheduler::transferJobFilesWorkerThread,
					(void *)thread_arg,
					s,
					transfer_reaper_id
					);
			break;

		default:
			tid = FALSE;
			break;
	}


	if ( tid == FALSE ) {
		free(thread_arg);
		delete jobs;
		refuse(s);
		return FALSE;
	}

		// Place this tid into a hashtable so our reaper can finish up.
	spoolJobFileWorkers.emplace(tid, jobs);
	
	dprintf( D_AUDIT, *rsock, "spoolJobFiles(): started worker process\n");

	return TRUE;
}

struct UpdateGSICredContinuation : Service {

public:
	UpdateGSICredContinuation(int cmd, const std::string &temp_path,
		const std::string &final_path, const std::string &job_owner,
		PROC_ID jobid, void *state)
	:
	  m_cmd(cmd),
	  m_temp_path(temp_path),
	  m_final_path(final_path),
	  m_job_owner(job_owner),
	  m_jobid(jobid),
	  m_state(state)
	{}

	int finish(Stream *);
	int finish_update(ReliSock *, ReliSock::x509_delegation_result);

private:
	int m_cmd;
	std::string m_temp_path;
	std::string m_final_path;
	std::string m_job_owner;  // empty job owner indicates we should use condor_priv.
	PROC_ID m_jobid;
	void *m_state;
};

int
Scheduler::updateGSICred(int cmd, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	PROC_ID jobid;
	JobQueueJob *jobad;

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 10 );  
	rsock->decode();

	if( ! rsock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD", SCHEDD_ERR_UPDATE_GSI_CRED_FAILED,
					"Failure to update GSI cred - Authentication failed" );
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "updateGSICred(%d) aborting: %s\n", cmd,
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}	


		// read the job id from the client
	rsock->decode();
	if ( !rsock->code(jobid) || !rsock->end_of_message() ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "updateGSICred(%d): "
					 "failed to read job id\n", cmd );
			refuse(s);
			return FALSE;
	}
		// TO DO: Add job proxy info
	dprintf( D_AUDIT | D_FULLDEBUG, *rsock,"updateGSICred(%d): read job id %d.%d\n",
		cmd,jobid.cluster,jobid.proc);
	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if ( !jobad ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "updateGSICred(%d): failed, "
				 "job %d.%d not found\n", cmd, jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}

		// Make certain this user is authorized to do this,
		// cuz only the owner of a job (or queue super user) is allowed
		// to transfer data to/from a job.
	bool authorized = false;
	if (UserCheck2(jobad, EffectiveUserRec(rsock))) {
		authorized = true;
	}
	if ( !authorized ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "updateGSICred(%d): failed, "
				 "user %s not authorized to edit job %d.%d\n", cmd,
				 rsock->getFullyQualifiedUser(),jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}

		// Make certain this job has a x509 proxy, and that this 
		// proxy is sitting in the SPOOL directory
	std::string SpoolSpace;
	SpooledJobFiles::getJobSpoolPath(jobad, SpoolSpace);
	ASSERT(!SpoolSpace.empty());
	char *proxy_path = NULL;
	jobad->LookupString(ATTR_X509_USER_PROXY,&proxy_path);
	if( proxy_path && !fullpath(proxy_path) ) {
		std::string iwd;
		if( jobad->LookupString(ATTR_JOB_IWD,iwd) ) {
			formatstr_cat(iwd,"%c%s",DIR_DELIM_CHAR,proxy_path);
			free(proxy_path);
			proxy_path = strdup(iwd.c_str());
		}
	}
	if ( !proxy_path || strncmp(SpoolSpace.c_str(),proxy_path,SpoolSpace.length()) ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "updateGSICred(%d): failed, "
			 "job %d.%d does not contain a gsi credential in SPOOL\n", 
			 cmd, jobid.cluster, jobid.proc );
		refuse(s);
		if (proxy_path) free(proxy_path);
		return FALSE;
	}
	std::string final_proxy_path(proxy_path);
	std::string temp_proxy_path(final_proxy_path);
	temp_proxy_path += ".tmp";
	free(proxy_path);

	std::string job_owner;
#ifndef WIN32
		// Check the ownership of the job's spool directory and switch
		// our priv state if needed.
		// The permissions on the spool directory may prevent us from
		// stat'ing the proxy file directly as the wrong user.
		// CRUFT: Once we remove the option to have the schedd chown
		//   the job's spool directory (CHOWN_JOB_SPOOL_FILES), this
		//   check can be removed (the files will always be owned by
		//   the job owner).
	StatInfo si( SpoolSpace.c_str() );
	if ( si.Error() != SIGood ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "updateGSICred(%d): failed, "
			"stat of spool dirctory for job %d.%d failed: %d\n",
			cmd, jobid.cluster, jobid.proc, (int)si.Error() );
		refuse(s);
		return FALSE;
	}
	uid_t proxy_uid = si.GetOwner();
	passwd_cache *p_cache = pcache();
	uid_t job_uid;
	jobad->LookupString( ATTR_OWNER, job_owner );
	if ( job_owner.empty() ) {
			// Maybe change EXCEPT to print to the audit log with D_AUDIT
		EXCEPT( "No %s for job %d.%d!", ATTR_OWNER, jobid.cluster,
				jobid.proc );
	}
	if ( !p_cache->get_user_uid( job_owner.c_str(), job_uid ) ) {
			// Failed to find uid for this owner, badness.
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "Failed to find uid for user %s (job %d.%d)\n",
				 job_owner.c_str(), jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}
		// If the uids match, then we need to switch to user priv to
		// access the proxy file.
	priv_state priv;
	if ( proxy_uid == job_uid ) {
			// We're not Windows here, so we don't need the NT Domain
		if ( !init_user_ids( job_owner.c_str(), NULL ) ) {
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "init_user_ids() failed for user %s!\n",
					 job_owner.c_str() );
			refuse(s);
			return FALSE;
		}
		priv = set_user_priv();
	} else {
			// We should already be in condor priv, but we want to save it
			// in the 'priv' variable.
		priv = set_condor_priv();
			// In UpdateGSICredContinuation below, an empty job_owner
			// means we should do file access as condor_priv.
		job_owner.clear();
	}
#endif

		// Decode the proxy off the wire, and store into the
		// file temp_proxy_path, which is known to be in the SPOOL dir
	rsock->decode();
	filesize_t size = 0;
	void *state = NULL;
	ReliSock::x509_delegation_result result;
	if ( cmd == UPDATE_GSI_CRED ) {
		int rc = rsock->get_file(&size,temp_proxy_path.c_str());
		result = (rc == 0) ? ReliSock::delegation_ok : ReliSock::delegation_error;
	} else if ( cmd == DELEGATE_GSI_CRED_SCHEDD ) {
		result = rsock->get_x509_delegation(temp_proxy_path.c_str(), false, &state);
	} else {
		dprintf( D_ALWAYS, "updateGSICred(%d): unknown CEDAR command %d\n",
				 cmd, cmd );
		result = ReliSock::delegation_error;
	}

	UpdateGSICredContinuation* continuation =
		new UpdateGSICredContinuation(cmd,
			temp_proxy_path.c_str(),
			final_proxy_path.c_str(),
			job_owner,
			jobid,
			state);

	int rc = 1;
	if (result == ReliSock::delegation_continue) {
		int retval = daemonCore->Register_Socket(rsock, "UpdateGSI Response",
			(SocketHandlercpp)&UpdateGSICredContinuation::finish,
			"UpdateGSI Response Continuation", continuation, HANDLE_READ);
			// If we fail to register the socket, continue in blocking mode.
		if (retval < 0) {
			continuation->finish_update(rsock, result);
			delete continuation;
		} // else we have registered the continuation; the finish routine will
		  // delete itself.
		else { rc = KEEP_STREAM; }
	} else {
		continuation->finish_update(rsock, result);
		delete continuation;
	}

#ifndef WIN32
		// Now switch back to our old priv state
	set_priv( priv );
	uninit_user_ids();
#endif
	return rc;
}


int
UpdateGSICredContinuation::finish(Stream *stream)
{
	dprintf(D_SECURITY|D_FULLDEBUG, "Finishing X509 delegation.\n");
	ReliSock *rsock = static_cast<ReliSock*>(stream);
	priv_state priv;
#ifndef WIN32
	if (!m_job_owner.empty()) {
		if ( !init_user_ids(m_job_owner.c_str(), NULL) ) {
			dprintf(D_AUDIT | D_ERROR_ALSO, *rsock, "init_user_ids() failed for user %s!\n",
				m_job_owner.c_str());
			delete this;
			return false;
		}
		priv = set_user_priv();
	} else {
		priv = set_condor_priv();
	}
#endif

	ReliSock::x509_delegation_result result = rsock->get_x509_delegation_finish(m_temp_path.c_str(), false, m_state);
	finish_update(rsock, result);

#ifndef WIN32
	set_priv( priv );
	uninit_user_ids();
#endif
	delete this;
	return true;
}


int
UpdateGSICredContinuation::finish_update(ReliSock *rsock, ReliSock::x509_delegation_result result)
{
	int reply;
	if ( result == ReliSock::delegation_error ) {
			// transfer failed
		reply = 0;	// reply of 0 means failure
	} else {
			// transfer worked, now rename the file to final_proxy_path
		if ( rotate_file(m_temp_path.c_str(), m_final_path.c_str()) < 0 ) 
		{
				// the rename failed!!?!?!
			dprintf( D_ALWAYS, "updateGSICred(%d): failed, "
				 "job %d.%d  - could not rename file\n",
				 m_cmd, m_jobid.cluster, m_jobid.proc);
			reply = 0;
		} else {
			reply = 1;	// reply of 1 means success

			ClassAd proxy_attrs;
			if ( ReadProxyFileIntoAd(m_final_path.c_str(), NULL, proxy_attrs) ) {
				UpdateJobProxyAttrs(m_jobid, proxy_attrs);
			}
			AuditLogJobProxy( *rsock, m_jobid, m_final_path.c_str() );
		}
	}

	// Update the proxy expiration time in the job ad
	time_t proxy_expiration;
	proxy_expiration = x509_proxy_expiration_time( m_final_path.c_str() );
	if (proxy_expiration == -1) {
		dprintf( D_ALWAYS, "updateGSICred(%d): failed to read expiration "
				 "time of updated proxy for job %d.%d: %s\n", m_cmd,
				 m_jobid.cluster, m_jobid.proc, x509_error_string() );
	} else {
		SetAttributeInt( m_jobid.cluster, m_jobid.proc,
						 ATTR_X509_USER_PROXY_EXPIRATION, proxy_expiration );
	}

		// Send our reply back to the client
	rsock->encode();
	if (!rsock->code(reply)) {
		dprintf(D_ALWAYS, "updateGSICred: failed to reply to client\n");
	}
	rsock->end_of_message();

	dprintf( D_AUDIT | D_ALWAYS, *rsock, "Refresh GSI cred for job %d.%d %s\n",
		m_jobid.cluster, m_jobid.proc, reply ? "succeeded" : "failed");
	
	return true;
}


int
Scheduler::actOnJobs(int, Stream* s)
{
	ClassAd command_ad;
	int action_num = -1;
	JobAction action = JA_ERROR;
	int reply;
	size_t i;
	size_t num_matches = 0;
	size_t num_cluster_matches = 0;
	int new_status = -1;
	char buf[256];
	std::string reason;
	const char *reason_attr_name = NULL;
	ReliSock* rsock = (ReliSock*)s;
	bool needs_transaction = true;
	action_result_type_t result_type = AR_TOTALS;
	int hold_reason_subcode = 0;

		// Setup array to hold ids of the jobs we're acting on.
	std::vector<PROC_ID> jobs;
	std::vector<PROC_ID> clusters; // holds cluster ids we are acting upon (for late materialization)
	PROC_ID tmp_id;

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 10 );  
	rsock->decode();
	if( ! rsock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD", SCHEDD_ERR_JOB_ACTION_FAILED,
					"Failed to act on jobs - Authentication failed");
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "actOnJobs() aborting: %s\n",
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}

	const OwnerInfo * sock_owner = EffectiveUserRec(rsock);

		// read the command ClassAd + EOM
	if( ! (getClassAd(rsock, command_ad) && rsock->end_of_message()) ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "Can't read command ad from tool\n" );
		refuse( s );
		return FALSE;
	}

		// // // // // // // // // // // // // //
		// Parse the ad to make sure it's valid
		// // // // // // // // // // // // // //

		/* 
		   Find out what they want us to do.  This classad should
		   contain:
		   ATTR_JOB_ACTION - either JA_HOLD_JOBS, JA_RELEASE_JOBS,
					JA_REMOVE_JOBS, JA_REMOVE_X_JOBS, 
					JA_VACATE_JOBS, JA_VACATE_FAST_JOBS, or
					JA_CLEAR_DIRTY_JOB_ATTRS
		   ATTR_ACTION_RESULT_TYPE - either AR_TOTALS or AR_LONG
		   and one of:
		   ATTR_ACTION_CONSTRAINT - a string with a ClassAd constraint 
		   ATTR_ACTION_IDS - a string with a comma seperated list of
		                     job ids to act on

		   In addition, it might also include one of:
					ATTR_REMOVE_REASON, ATTR_RELEASE_REASON, or
					ATTR_HOLD_REASON

		   It may optionally contain ATTR_HOLD_REASON_SUBCODE.
		*/
	if( ! command_ad.LookupInteger(ATTR_JOB_ACTION, action_num) ) {
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock,
				 "actOnJobs(): ClassAd does not contain %s, aborting\n", 
				 ATTR_JOB_ACTION );
		refuse( s );
		return FALSE;
	}
	action = (JobAction)action_num;

		// Make sure we understand the action they requested
	switch( action ) {
	case JA_HOLD_JOBS:
		new_status = HELD;
		reason_attr_name = ATTR_HOLD_REASON;
		break;
	case JA_RELEASE_JOBS:
		new_status = IDLE;
		reason_attr_name = ATTR_RELEASE_REASON;
		break;
	case JA_REMOVE_JOBS:
		new_status = REMOVED;
		reason_attr_name = ATTR_REMOVE_REASON;
		break;
	case JA_REMOVE_X_JOBS:
		new_status = REMOVED;
		reason_attr_name = ATTR_REMOVE_REASON;
		break;
	case JA_SUSPEND_JOBS:
	case JA_CONTINUE_JOBS:
	case JA_VACATE_JOBS:
	case JA_VACATE_FAST_JOBS:
	case JA_CLEAR_DIRTY_JOB_ATTRS:
			// no new_status needed.  also, we're not touching
			// anything in the job queue, so we don't need a
			// transaction, either...
		needs_transaction = false;
		break;
	default:
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "actOnJobs(): ClassAd contains invalid "
				 "%s (%d), aborting\n", ATTR_JOB_ACTION, action_num );
		refuse( s );
		return FALSE;
	}
		// Grab the reason string if the command ad gave it to us
	if( reason_attr_name ) {
		command_ad.LookupString( reason_attr_name, reason );
	}
	if( ! reason.empty() ) {
			// patch up the reason they gave us to include who did
			// it. 
		const char *owner = EffectiveUserName(rsock);
		reason += " (by user "; reason += owner; reason += ")";
	}

	if( action == JA_HOLD_JOBS ) {
		command_ad.LookupInteger(ATTR_HOLD_REASON_SUBCODE,hold_reason_subcode);
	}

	int foo;
		// Default to summary.  Only give long results if they
		// specifically ask for it.  If they didn't specify or
		// specified something that we don't understand, just give
		// them a summary...
	result_type = AR_TOTALS;
	if( command_ad.LookupInteger(ATTR_ACTION_RESULT_TYPE, foo) ) {
		if( foo == AR_LONG ) {
			result_type = AR_LONG;
		}
	}

		// Now, figure out if they want us to deal w/ a constraint or
		// with specific job ids.  We don't allow both.
	char *constraint = NULL;
	std::vector<std::string> job_ids;
		// NOTE: ATTR_ACTION_CONSTRAINT needs to be treated as a bool,
		// not as a string...
	ExprTree *tree;
	tree = command_ad.LookupExpr(ATTR_ACTION_CONSTRAINT);
	if( tree ) {
		const char *value = ExprTreeToString( tree );
		if( ! value ) {
			return false;
		}

			// we want to tack on another clause to make sure we're
			// not doing something invalid
		switch( action ) {
		case JA_REMOVE_JOBS:
				// Don't remove removed jobs
			snprintf( buf, 256, "(ProcId is undefined || (%s!=%d)) && (", ATTR_JOB_STATUS, REMOVED );
			break;
		case JA_REMOVE_X_JOBS:
				// only allow forced removal of previously "removed" jobs
				// including jobs on hold that will go to the removed state
				// upon release.
			snprintf( buf, 256, "(ProcId is undefined || (%s==%d) || (%s==%d && %s=?=%d)) && (", 
				ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, HELD,
				ATTR_JOB_STATUS_ON_RELEASE,REMOVED);
			break;
		case JA_HOLD_JOBS:
				// Don't hold removed/completed jobs (but do match cluster ads - so late materialization works)
			snprintf( buf, 256, "(ProcId is undefined || (%s!=%d && %s!=%d)) && (", ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, COMPLETED );
			break;
		case JA_RELEASE_JOBS:
				// Only release held jobs which aren't waiting for
				// input files to be spooled, (or cluster ads - so late materialization works)
			snprintf( buf, 256, "(ProcId is undefined || (%s==%d && %s=!=%d)) && (", ATTR_JOB_STATUS,
					  HELD, ATTR_HOLD_REASON_CODE,
					  CONDOR_HOLD_CODE::SpoolingInput );
			break;
		case JA_SUSPEND_JOBS:
				// Only suspend running/staging jobs outside local & sched unis
			snprintf( buf, 256,
					  "((%s==%d || %s==%d) && (%s=!=%d && %s=!=%d)) && (",
					  ATTR_JOB_STATUS, RUNNING,
					  ATTR_JOB_STATUS, TRANSFERRING_OUTPUT,
					  ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_LOCAL,
					  ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_SCHEDULER );
			break;
		case JA_VACATE_JOBS:
		case JA_VACATE_FAST_JOBS:
				// Only vacate running/staging jobs
			snprintf( buf, 256, "(%s==%d || %s==%d) && (", ATTR_JOB_STATUS,
					  RUNNING, ATTR_JOB_STATUS, TRANSFERRING_OUTPUT );
			break;
		case JA_CLEAR_DIRTY_JOB_ATTRS:
				// No need to further restrict jobs
			break;
		case JA_CONTINUE_JOBS:
			// Only continue jobs which are suspended
			snprintf( buf, 256, "(%s==%d) && (", ATTR_JOB_STATUS, SUSPENDED );
			break;
		default:
				// Maybe change EXCEPT to print to the audit log with D_AUDIT
			EXCEPT( "impossible: unknown action (%d) in actOnJobs() after "
					"it was already recognized", action_num );
		}
		buf[sizeof(buf)-1] = 0; // snprintf won't null terminate if it runs out of space.
		int size = strlen(buf) + strlen(value) + 3;
		constraint = (char*) malloc( size * sizeof(char) );
		if( ! constraint ) {
				// Maybe change EXCEPT to print to the audit log with D_AUDIT
			EXCEPT( "Out of memory!" );
		}
			// we need to terminate the ()'s after their constraint
		snprintf( constraint, size, "%s%s)", buf, value );
	} else {
		constraint = NULL;
	}
	char * tmp = NULL;
	if( command_ad.LookupString(ATTR_ACTION_IDS, &tmp) ) {
		if( constraint ) {
			dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "actOnJobs(): "
					 "ClassAd has both %s and %s, aborting\n",
					 ATTR_ACTION_CONSTRAINT, ATTR_ACTION_IDS );
			refuse( s );
			free( tmp );
			free( constraint );
			return FALSE;
		}
		job_ids = split(tmp);
		free( tmp );
		tmp = NULL;
	}
	
		// Audit Log reporting
	std::string job_ids_string, initial_constraint;
	if( constraint ) {
		initial_constraint = constraint;
		dprintf( D_AUDIT, *rsock, "%s by constraint %s\n",
				 getJobActionString(action), initial_constraint.c_str());

	} else {
		job_ids_string = join(job_ids, ",");
		dprintf( D_AUDIT, *rsock, "%s jobs %s\n",
				 getJobActionString(action), job_ids_string.c_str());
	}		

		// // // // //
		// REAL WORK
		// // // // //
	
	time_t now = time(0);

	JobActionResults results( result_type );

		// begin a transaction for qmgmt operations
	if( needs_transaction ) { 
		BeginTransaction();
	}

		// process the jobs to set status (and optionally, reason) 
	if( constraint ) {

			// SetAttributeByConstraint is clumsy and doesn't really
			// do what we want.  Instead, we'll just iterate through
			// the Q ourselves so we know exactly what jobs we hit. 

		JobQueueJob* (*GetNextJobFunc) (const char *, int);
		JobQueueJob* job_ad;
		if( action == JA_CLEAR_DIRTY_JOB_ATTRS )
		{
			GetNextJobFunc = &GetNextDirtyJobByConstraint;
		}
		else
		{
			//GetNextJobFunc = &GetNextJobByConstraint;
			GetNextJobFunc = &GetNextJobOrClusterByConstraint;
		}
		for( job_ad = (*GetNextJobFunc)( constraint, 1 );
		     job_ad;
		     job_ad = (*GetNextJobFunc)( constraint, 0 ))
		{
			if (job_ad->jid.proc < 0) {
				clusters.emplace_back(job_ad->jid.cluster, job_ad->jid.proc);
				num_cluster_matches++;
			} else {
				jobs.emplace_back(job_ad->jid.cluster, job_ad->jid.proc);
				num_matches++;
			}
		}
		free( constraint );
		constraint = NULL;

	} else {

			// No need to iterate through the queue, just act on the
			// specific ids we care about...

		std::vector<std::string> expanded_ids;
		expand_mpi_procs(job_ids, expanded_ids);
		for (const auto &idstr: expanded_ids) {
			tmp_id = getProcByString(idstr.c_str());
			if( tmp_id.cluster < 0 ) {
				continue;
			}
			if (tmp_id.proc < 0) {
				clusters.emplace_back(tmp_id.cluster, tmp_id.proc);
				num_cluster_matches++;
			} else {
				jobs.emplace_back(tmp_id.cluster, tmp_id.proc);
				num_matches++;
			}
		}
	}

	int num_success = 0;
	for( i=0; i<num_matches; i++ ) {
		tmp_id = jobs[i];

			// Check to make sure the job's status makes sense for
			// the command we're trying to perform
		int status = -1;
		JobQueueJob * job_ad = GetJobAd(tmp_id);
		int on_release_status = IDLE;
		int hold_reason_code = -1;
		if ( ! job_ad || GetAttributeInt(tmp_id.cluster, tmp_id.proc,  ATTR_JOB_STATUS, &status) < 0)
		{
			results.record( tmp_id, AR_NOT_FOUND );
			jobs[i].cluster = -1;
			continue;
		}
		// Check that this user is allowed to modify this job.
		if( !UserCheck2(job_ad, sock_owner) ) {
			results.record( tmp_id, AR_PERMISSION_DENIED );
			jobs[i].cluster = -1;
			continue;
		}
		switch( action ) { 
		case JA_CONTINUE_JOBS:
			if( status != SUSPENDED ) {
				results.record( tmp_id, AR_BAD_STATUS );
				jobs[i].cluster = -1;
				continue;
			}
			break;
		case JA_SUSPEND_JOBS:
		case JA_VACATE_JOBS:
		case JA_VACATE_FAST_JOBS:
			if( status != RUNNING && status != TRANSFERRING_OUTPUT ) {
				results.record( tmp_id, AR_BAD_STATUS );
				jobs[i].cluster = -1;
				continue;
			}
			break;
		case JA_RELEASE_JOBS:
			GetAttributeInt(tmp_id.cluster, tmp_id.proc,
							ATTR_HOLD_REASON_CODE, &hold_reason_code);
			if( status != HELD || hold_reason_code == CONDOR_HOLD_CODE::SpoolingInput ) {
				results.record( tmp_id, AR_BAD_STATUS );
				jobs[i].cluster = -1;
				continue;
			}
			GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
							ATTR_JOB_STATUS_ON_RELEASE, &on_release_status);
			new_status = on_release_status;
			break;
		case JA_REMOVE_JOBS:
			if( status == REMOVED ) {
				results.record( tmp_id, AR_ALREADY_DONE );
				jobs[i].cluster = -1;
				continue;
			}
			break;
		case JA_REMOVE_X_JOBS:
			if( status == HELD ) {
				GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
								ATTR_JOB_STATUS_ON_RELEASE, &on_release_status);
			}
			if( (status!=REMOVED) && 
				(status!=HELD || on_release_status!=REMOVED) ) 
			{
				results.record( tmp_id, AR_BAD_STATUS );
				jobs[i].cluster = -1;
				continue;
			}
				// set LeaveJobInQueue to false...
			if( SetAttribute( tmp_id.cluster, tmp_id.proc,
							  ATTR_JOB_LEAVE_IN_QUEUE, "False" ) < 0 ) {
				results.record( tmp_id, AR_PERMISSION_DENIED );
				jobs[i].cluster = -1;
				continue;
			}
			break;
		case JA_HOLD_JOBS:
			if( status == HELD ) {
				int hold_code = 0;
				GetAttributeInt(tmp_id.cluster, tmp_id.proc,
				                ATTR_HOLD_REASON_CODE, &hold_code);
				if( hold_code == CONDOR_HOLD_CODE::UserRequest || hold_code == CONDOR_HOLD_CODE::SpoolingInput ) {
					results.record( tmp_id, AR_ALREADY_DONE );
					jobs[i].cluster = -1;
					continue;
				}
			}
			if ( status == REMOVED || status == COMPLETED )
			{
				results.record( tmp_id, AR_PERMISSION_DENIED );
				jobs[i].cluster = -1;
				continue;
			}
			if (holdJob(tmp_id.cluster, tmp_id.proc,
				reason.c_str(),	// hold reason string
				CONDOR_HOLD_CODE::UserRequest,	// hold reason code
				hold_reason_subcode,	// hold reason subcode
				false,	// use_transaction
				false,	// email user?
				false,	// email admin?
				false,	// system hold?
				false	// write to user log?  Set to false cause actOnJobs does not do this here...
				) == false)
			{
				// We already tested above for all possibilities other than AR_PERMISSION_DENIED
				// before calling holdJob(), so if holdJob() fails it is because permission denied...
				results.record(tmp_id, AR_PERMISSION_DENIED);
				jobs[i].cluster = -1;
				continue;
			}
			break;
		case JA_CLEAR_DIRTY_JOB_ATTRS:
			MarkJobClean( tmp_id.cluster, tmp_id.proc );
			results.record( tmp_id, AR_SUCCESS );
			num_success++;
			continue;
		default:
				// Maybe change EXCEPT to print to the audit log with D_AUDIT
			EXCEPT( "impossible: unknown action (%d) in actOnJobs() "
					"after it was already recognized", action_num );
		}

			// Ok, we're happy, do the deed.
		if( action == JA_HOLD_JOBS ||   // if hold, we already invoked holdJobs() above so all done
			action == JA_VACATE_JOBS ||
			action == JA_VACATE_FAST_JOBS || 
		    action == JA_SUSPEND_JOBS || 
		    action == JA_CONTINUE_JOBS ) {
				// vacate is a special case, since we're not
				// trying to modify the job in the queue at
				// all, so we just need to
				// record that we found this job_id and we're
				// done.
			results.record( tmp_id, AR_SUCCESS );
			num_success++;
			continue;
		}
		if( SetAttributeInt(tmp_id.cluster, tmp_id.proc,
							ATTR_JOB_STATUS, new_status) < 0 ) {
			results.record( tmp_id, AR_PERMISSION_DENIED );
			jobs[i].cluster = -1;
			continue;
		}
		if( ! reason.empty() ) {
			SetAttributeString( tmp_id.cluster, tmp_id.proc,
						  reason_attr_name, reason.c_str() );
				// TODO: deal w/ failure here, too?
		}
		SetAttributeInt( tmp_id.cluster, tmp_id.proc,
						 ATTR_ENTERED_CURRENT_STATUS, now );
		fixReasonAttrs( tmp_id, action );
		results.record( tmp_id, AR_SUCCESS );
		num_success++;
	}

	// With late materialization, we may need to operate on clusters (actually the cluster factory) also
	//
	for( i=0; i<num_cluster_matches; i++ ) {
		tmp_id = clusters[i];
		JobQueueCluster * clusterad = GetClusterAd(tmp_id);
		if ( ! clusterad)
			continue;

		switch( action ) { 
		case JA_CONTINUE_JOBS:
		case JA_SUSPEND_JOBS:
		case JA_VACATE_JOBS:
		case JA_VACATE_FAST_JOBS:
		case JA_CLEAR_DIRTY_JOB_ATTRS:
		case JA_ERROR:
			// nothing to do for these
			break;

		case JA_HOLD_JOBS:
			if (clusterad->factory) {
				if (UserCheck2(clusterad, sock_owner) == false ||
					SetAttributeInt(tmp_id.cluster, tmp_id.proc, ATTR_JOB_MATERIALIZE_PAUSED, mmHold) < 0) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					// if we failed to set the pause attribute, take this cluster out of the list so we don't try
					// and actually pause the factory
					clusters[i].cluster = -1;
				} else {
					results.record( tmp_id, AR_SUCCESS );
					num_success++;

					SetAttributeString(tmp_id.cluster, tmp_id.proc, ATTR_JOB_MATERIALIZE_PAUSE_REASON, reason.c_str());
				}
			}
			break;

		case JA_RELEASE_JOBS:
			if (clusterad->factory) {
				if (UserCheck2(clusterad, sock_owner) == false ||
					SetAttributeInt(tmp_id.cluster, tmp_id.proc, ATTR_JOB_MATERIALIZE_PAUSED, mmRunning) < 0) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					// if we failed to set the pause attribute, take this cluster out of the list so we don't try
					// and actually pause the factory
					clusters[i].cluster = -1;
				} else {
					results.record( tmp_id, AR_SUCCESS );
					num_success++;

					DeleteAttribute(tmp_id.cluster, tmp_id.proc, ATTR_JOB_MATERIALIZE_PAUSE_REASON); 
				}
			}
			break;

		case JA_REMOVE_JOBS:
		case JA_REMOVE_X_JOBS:
			if (clusterad->factory) {
				// check to see if we are allowed to pause this factory, but don't actually change it's
				// pause state, the mmClusterRemoved pause mode is a runtime-only schedd state.
				if (UserCheck2(clusterad, sock_owner) == false ||
					SetAttribute(tmp_id.cluster, tmp_id.proc, ATTR_JOB_MATERIALIZE_PAUSED, "3", SetAttribute_QueryOnly) < 0) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					clusters[i].cluster = -1;
				} else {
					PauseJobFactory(clusterad->factory, mmClusterRemoved);
					//PRAGMA_REMIND("TODO: can we remove the cluster now rather than just pausing the factory and scheduling the removal?")
					ScheduleClusterForDeferredCleanup(tmp_id.cluster);
					// we succeeded because we found the cluster, and a Pause 3 will cannot fail.
					results.record( tmp_id, AR_SUCCESS );
					num_success++;
				}
			}
			break;
		}
	}

	ClassAd* response_ad;

	response_ad = results.publishResults();

		// Let them know what action we performed in the reply
	response_ad->Assign( ATTR_JOB_ACTION, action );

		// Set a single attribute which says if the action succeeded
		// on at least one job or if it was a total failure
	response_ad->Assign( ATTR_ACTION_RESULT, num_success ? 1:0 );

		// Return the number of jobs in the queue to the caller can
		// determine appropriate actions
	response_ad->Assign( ATTR_TOTAL_JOB_ADS, scheduler.getJobsTotalAds() );

		// Finally, let them know if the user running this command is
		// a queue super user here
	response_ad->Assign( ATTR_IS_QUEUE_SUPER_USER,
			 isQueueSuperUser(sock_owner) ? true : false );
	
	rsock->encode();
	if( ! (putClassAd(rsock, *response_ad) && rsock->end_of_message()) ) {
			// Failed to send reply, the client might be dead, so
			// abort our transaction.
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock,
				 "actOnJobs: couldn't send results to client: aborting\n" );
		if( needs_transaction ) {
			AbortTransaction();
		}
		return FALSE;
	}

	if( num_success == 0 ) {
			// We didn't do anything, so we want to bail out now...
		if (IsDebugCategory(D_AUDIT)) {
			dprintf(D_AUDIT, *rsock, "actOnJobs: didn't do any work, aborting\n" );
		}
		if (IsFulldebug(D_FULLDEBUG)) {
			dprintf(D_FULLDEBUG, "actOnJobs: didn't do any work, aborting\n" );
		}
		if( needs_transaction ) {
			AbortTransaction();
		}
		return FALSE;
	}

		// If we told them it's good, try to read the reply to make
		// sure the tool is still there and happy...
	rsock->decode();
	if( ! (rsock->code(reply) && rsock->end_of_message() && reply == OK) ) {
			// we couldn't get the reply, or they told us to bail
		dprintf( D_AUDIT | D_ERROR_ALSO, *rsock, "actOnJobs: client not responding: aborting\n" );
		if( needs_transaction ) {
			AbortTransaction();
		}
		return FALSE;
	}

		// We're seeing sporadic test suite failures where this
		// CommitTransaction() appears to take a long time to
		// execute. This dprintf() will help in debugging.
	time_t before = time(NULL);
	if( needs_transaction ) {
		CommitTransactionOrDieTrying();
	}
	time_t after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG, "actOnJobs(): CommitTransaction() took %ld seconds to run\n", after - before );
	}
		
		// If we got this far, we can tell the tool we're happy,
		// since if that CommitTransaction failed, we'd EXCEPT()
	rsock->encode();
	int answer = OK;
	if (!rsock->code( answer )) {
		dprintf(D_FULLDEBUG, "actOnJobs(): tool hung up on us\n");
	}
	rsock->end_of_message();

		// Now that we know the events are logged and commited to
		// the queue, we can do the final actions for these jobs,
		// like killing shadows if needed...
	for( i=0; i<num_matches; i++ ) {
		if( jobs[i].cluster == -1 ) {
			continue;
		}
		enqueueActOnJobMyself( jobs[i], action, true );
	}
	for( i=0; i<num_cluster_matches; i++ ) {
		tmp_id = clusters[i];
		if (tmp_id.cluster < 0) // skip entries for which the attempt to set the pause attribute failed.
			continue;
		JobQueueCluster * clusterad = GetClusterAd(tmp_id);
		if ( ! clusterad || ! clusterad->factory)
			continue;
		if (action == JA_HOLD_JOBS) {
			// log the change in pause state
			setJobFactoryPauseAndLog(clusterad, mmHold, CONDOR_HOLD_CODE::UserRequest, reason.c_str());
		} else if (action == JA_RELEASE_JOBS) {
			setJobFactoryPauseAndLog(clusterad, mmRunning, CONDOR_HOLD_CODE::UserRequest, reason.c_str());
		}
	}

		// In case we have removed jobs that were queued to run, scan
		// our matches and either remove them or pick a different job
		// to run on them.
	ExpediteStartJobs();

		// Audit Log reporting
	if( !initial_constraint.empty() ) {
		dprintf( D_AUDIT, *rsock, "Finished %s by constraint %s\n",
				 getJobActionString(action), initial_constraint.c_str());
	} else {
		dprintf( D_AUDIT, *rsock, "Finished %s jobs %s\n",
				 getJobActionString(action), job_ids_string.c_str());
	}

	return TRUE;
}

class ActOnJobRec: public ServiceData {
public:
	ActOnJobRec(PROC_ID job_id, JobAction action, bool log):
		m_job_id(job_id.cluster,job_id.proc,-1), m_action(action), m_log(log) {}

	CondorID m_job_id;
	JobAction m_action;
	bool m_log;

		/** These are not actually used, because we are
		 *  using the all_dups option to SelfDrainingQueue. */
	virtual int ServiceDataCompare( ServiceData const* other ) const;
	virtual size_t HashFn( ) const;
};

int
ActOnJobRec::ServiceDataCompare( ServiceData const* other ) const
{
	ActOnJobRec const *o = (ActOnJobRec const *)other;

	if( m_action < o->m_action ) {
		return -1;
	}
	else if( m_action > o->m_action ) {
		return 1;
	}
	return m_job_id.ServiceDataCompare( &o->m_job_id );
}

size_t
ActOnJobRec::HashFn( ) const
{
	return m_job_id.HashFn();
}

void
Scheduler::enqueueActOnJobMyself( PROC_ID job_id, JobAction action, bool log )
{
	ActOnJobRec *act_rec = new ActOnJobRec( job_id, action, log );
	bool stopping_job = false;

	if( action == JA_HOLD_JOBS ||
		action == JA_REMOVE_JOBS ||
		action == JA_VACATE_JOBS ||
		action == JA_VACATE_FAST_JOBS ||
	    action == JA_SUSPEND_JOBS ||
		action == JA_CONTINUE_JOBS )
	{
		if( scheduler.FindSrecByProcID(job_id) ) {
				// currently, only jobs with shadows are intended
				// to be handled specially
			stopping_job = true;
		}
	}

	if( stopping_job ) {
			// all of these actions are rate-limited via
			// JOB_STOP_COUNT and JOB_STOP_DELAY
		stop_job_queue.enqueue( act_rec );
	}
	else {
			// these actions are not currently rate-limited, but it is
			// still useful to handle them in a self draining queue, because
			// this may help keep the schedd responsive to other things
		act_on_job_myself_queue.enqueue( act_rec );
	}
}

/**
 * Remove any jobs that match the specified job's OtherJobRemoveRequirements
 * attribute, if it has one.
 * @param: the cluster of the "controlling" job
 * @param: the proc of the "controlling" job
 * @return: true if successful, false otherwise
 */
bool removeOtherJobs( int cluster, int proc )
{
	bool result = true;

	std::string removeConstraint;
	int attrResult = GetAttributeString( cluster, proc,
				ATTR_OTHER_JOB_REMOVE_REQUIREMENTS,
				removeConstraint );
	if ( attrResult == 0 && removeConstraint != "" ) {
		dprintf( D_ALWAYS,
					"Constraint <%s = %s> fired because job (%d.%d) "
					"was removed\n",
					ATTR_OTHER_JOB_REMOVE_REQUIREMENTS,
					removeConstraint.c_str(), cluster, proc );
		std::string reason;
		formatstr( reason,
					"removed because <%s = %s> fired when job (%d.%d)"
					" was removed", ATTR_OTHER_JOB_REMOVE_REQUIREMENTS,
					removeConstraint.c_str(), cluster, proc );
		result = abortJobsByConstraint(removeConstraint.c_str(),
					reason.c_str(), true);
	}

	return result;
}

int
Scheduler::actOnJobMyselfHandler( ServiceData* data )
{
	ActOnJobRec *act_rec = (ActOnJobRec *)data;

	JobAction action = act_rec->m_action;
	bool log    = act_rec->m_log;
	PROC_ID job_id;
	job_id.cluster = act_rec->m_job_id._cluster;
	job_id.proc = act_rec->m_job_id._proc;

	delete act_rec;

	if ( !GetJobAd(job_id.cluster, job_id.proc) ) {
		dprintf(D_ALWAYS, "Job %d.%d is not in the queue, cannot perform action %s\n",
			job_id.cluster, job_id.proc, getJobActionString(action));
		return TRUE;
	}

	switch( action ) {
	case JA_SUSPEND_JOBS:
	case JA_CONTINUE_JOBS:
		// suspend and continue don't actually abort but the code flow 
		// is correct.  If anything abort_job_myself() should be renamed
		// because we are sending an action through to a matching 
		// shadow TSTCLAIR (tstclair@redhat.com) 
	case JA_HOLD_JOBS:
	case JA_REMOVE_JOBS:
	case JA_VACATE_JOBS:
	case JA_VACATE_FAST_JOBS: {
		abort_job_myself( job_id, action, log );
		break;
    }
	case JA_RELEASE_JOBS: {
		WriteReleaseToUserLog( job_id );
		needReschedule();
		break;
    }
	case JA_REMOVE_X_JOBS: {
		if( !scheduler.WriteAbortToUserLog( job_id ) ) {
			dprintf( D_ALWAYS, 
					 "Failed to write abort event to the user log\n" ); 
		}
		DestroyProc( job_id.cluster, job_id.proc );

		shadow_rec * srec = scheduler.FindSrecByProcID(job_id);
		if(srec == NULL) {
			dprintf( D_FULLDEBUG, "(%d.%d) Shadow already gone\n", 
				(int) job_id.cluster, (int)job_id.proc);
		} else {
			dprintf( D_FULLDEBUG, "(%d.%d) Killing shadow %d\n", 
				(int) job_id.cluster, (int)job_id.proc, (int)(srec->pid));
			scheduler.sendSignalToShadow(srec->pid, SIGKILL, job_id);
		}

		break;
    }
	case JA_CLEAR_DIRTY_JOB_ATTRS:
		// Nothing to do
		break;
	case JA_ERROR:
	default:
		EXCEPT( "impossible: unknown action (%d) at the end of actOnJobs()",
				(int)action );
		break;
	}
	return TRUE;
}


void
Scheduler::refuse( Stream* s )
{
	s->encode();
	s->put( NOT_OK );
	s->end_of_message();
}


int
Scheduler::negotiatorSocketHandler (Stream *stream)
{
	int command = -1;

	daemonCore->Cancel_Socket( stream );

	dprintf (D_ALWAYS, "Activity on stashed negotiator socket: %s\n", ((Sock *)stream)->get_sinful_peer());

	// attempt to read a command off the stream
	stream->decode();
	if (!stream->code(command))
	{
		dprintf (D_ALWAYS, "Socket activated, but could not read command\n");
		dprintf (D_ALWAYS, "(Negotiator probably invalidated cached socket)\n");
	}
	else {
		negotiate(command, stream);
	}

		// We called incRefCount() when registering this socket, so
		// release it here.
	stream->decRefCount();

		// We have either deleted the socket or registered it again,
		// so tell our caller to just leave it alone.
	return KEEP_STREAM;
}

int
Scheduler::shadowsSpawnLimit()
{
	if (!canSpawnShadow()) {return 0;}

	if (MaxJobsRunning <= 0) {return -1;}

	bool should_curb = false;
	if (m_adSchedd && m_adSchedd->EvaluateAttrBoolEquiv(ATTR_CURB_MATCHMAKING, should_curb) && should_curb)
	{
		dprintf(D_FULLDEBUG, "Matchmaking curb limits evaluated to true; not offering jobs to matchmaker this round.\n");
		return 0;
	}

	int shadows = numShadows + RunnableJobQueue.size() + num_pending_startd_contacts + startdContactQueue.size();

	return std::max(MaxJobsRunning - shadows, 0);
}

/* 
   Helper function used by both DedicatedScheduler::negotiate() and
   Scheduler::negotiate().  This checks all the various reasons why we
   might not be able to or want to start another shadow
   (MAX_JOBS_RUNNING, swap space problems, etc), and returns true if
   we can proceed or false if we can't start another shadow.  */
bool
Scheduler::canSpawnShadow()
{
	int shadows = numShadows + RunnableJobQueue.size() + num_pending_startd_contacts + startdContactQueue.size();

		// First, check if we have reached our maximum # of shadows 
	if( shadows >= MaxJobsRunning ) {
		if( !RecentlyWarnedMaxJobsRunning ) {
			dprintf( D_ALWAYS, "Reached MAX_JOBS_RUNNING: no more can run\n" );
		}
		RecentlyWarnedMaxJobsRunning = true;
		return false;
	}

	if( ReservedSwap == 0 ) {
			// We're not supposed to care about swap space at all, so
			// none of the rest of the checks matter at all.
		return true;
	}

		// Now, see if we ran out of swap space already.
	if( SwapSpaceExhausted) {
		if( !RecentlyWarnedMaxJobsRunning ) {
			dprintf( D_ALWAYS, "Swap space exhausted! No more jobs can be run!\n" );
			dprintf( D_ALWAYS, "    Solution: get more swap space, or set RESERVED_SWAP = 0\n" );
		}
		RecentlyWarnedMaxJobsRunning = true;
		return false;
	}

	if( ShadowSizeEstimate && shadows >= MaxShadowsForSwap ) {
		if( !RecentlyWarnedMaxJobsRunning ) {
			dprintf( D_ALWAYS, "Swap space estimate reached! No more jobs can be run!\n" );
			dprintf( D_ALWAYS, "    Solution: get more swap space, or set RESERVED_SWAP = 0\n" );
		}
		RecentlyWarnedMaxJobsRunning = true;
		return false;
	}
	
		// We made it.  Everything's cool.
	return true;
}


/* MainScheddNegotiate is a class that overrides virtual methods
   called by ScheddNegotiate when it requires actions to be taken by
   the schedd during negotiation.  See the definition of
   ScheddNegotiate for a description of these functions.
*/
class MainScheddNegotiate: public ScheddNegotiate {
public:
	MainScheddNegotiate(
		int cmd,
		ResourceRequestList *jobs,
		char const *owner,
		char const *remote_pool
	): ScheddNegotiate(cmd,jobs,owner,remote_pool) {}

#if 0 // not used
		// returns true if no job similar to job_id may be started right now
		// (e.g. MAX_JOBS_RUNNING could cause this to return true)
	bool skipAllSuchJobs(PROC_ID job_id);
#endif

		// Define the virtual functions required by ScheddNegotiate //

	virtual bool scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad );

	virtual bool scheduler_skipJob(JobQueueJob * job, ClassAd *match_ad, bool &skip_all_such, const char * &skip_because); // match ad may be null

	virtual bool scheduler_getRequestConstraints(PROC_ID job_id, ClassAd &request_ad, int * match_max);

	virtual void scheduler_handleJobRejected(PROC_ID job_id,int autocluster_id, char const *reason);

	virtual bool scheduler_handleMatch(PROC_ID job_id,char const *claim_id, char const *extra_claims, ClassAd &match_ad, char const *slot_name);

	virtual void scheduler_handleNegotiationFinished( Sock *sock );

	virtual int scheduler_maxJobsToOffer();

};

int
MainScheddNegotiate::scheduler_maxJobsToOffer()
{
	int maxJobs = scheduler.shadowsSpawnLimit();
	if (maxJobs > 0)
	{
		dprintf(D_FULLDEBUG, "Negotiation cycle will offer at most %d jobs to stay under limits.\n", maxJobs);
	}
	else if (!maxJobs)
	{
		dprintf(D_FULLDEBUG, "Scheduler already at internal limits; will not offer any jobs to negotiator.\n");
	}
	return maxJobs;
}

bool
MainScheddNegotiate::scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad )
{
	ClassAd *ad = GetJobAd( job_id.cluster, job_id.proc );
	if( !ad ) {
		return false;
	}

	// return a copy of the job ad.
	job_ad = *ad;

	FreeJobAd( ad );
	return true;
}


	// returns false if we should skip this request ad (i.e. and not send it to the negotiator at all)
	// if return is true, and match_max is not null, match_max will be set to the maximum matches constraint, or MAX_INT
	// The request constraint expression will be added to the request_ad if it is more complex than a simple boolean literal
bool MainScheddNegotiate::scheduler_getRequestConstraints(PROC_ID job_id, ClassAd &request_ad, int * match_max)
{
	// TODO: set this to the max matches we can allow for this resource request
	if (match_max) { *match_max = INT_MAX; }

#ifdef USE_VANILLA_START
	const OwnerInfo* powni = NULL;
	int universe = CONDOR_UNIVERSE_MIN;
	JobQueueJob * job = GetJobAndInfo(job_id, universe, powni);
	if (job) {
		ExprTree * tree = scheduler.flattenVanillaStartExpr(job, powni);
		if (tree) {
			bool bval = false;
			// don't bother to put a literal bool into the request ad, just return it's value
			// the caller will then either skip this request_ad (if it is false), or continue on
			// without a request constraint expression (if it is true)
			if (ExprTreeIsLiteralBool(tree, bval)) {
				delete tree;
				return bval;
			} else {
				std::string tmpbuf="";
				if (IsDebugVerbose(D_MATCH)) {
					ExprTreeToString(tree, tmpbuf);
					dprintf(D_FULLDEBUG | D_MATCH, "START_VANILLA flattened to '%s' for RequestConstraint for job %d.%d\n", tmpbuf.c_str(), job_id.cluster, job_id.proc);
				}

				// Convert SLOT prefix to TARGET and JOB prefix to MY
				NOCASE_STRING_MAP mapping;
				mapping["SLOT"] = "TARGET";
				mapping["JOB"] = "MY";
				RewriteAttrRefs(tree, mapping);

				if (IsDebugVerbose(D_MATCH)) {
					tmpbuf = "";
					ExprTreeToString(tree, tmpbuf);
					dprintf(D_FULLDEBUG | D_MATCH, "returning '%s' as the RequestConstraint for job %d.%d\n", tmpbuf.c_str(), job_id.cluster, job_id.proc);
				}

				request_ad.Insert(ATTR_RESOURCE_REQUEST_CONSTRAINT, tree);

				// now make a new Requirements expression that ANDs in the request constraint
				std::string attr_req(ATTR_REQUIREMENTS);
				ExprTree * reqexp = request_ad.Lookup(attr_req);
				if (reqexp) {
					tmpbuf = ATTR_RESOURCE_REQUEST_CONSTRAINT " && (";
					ExprTreeToString(SkipExprParens(reqexp), tmpbuf);
					tmpbuf += ")";
					request_ad.InsertViaCache(attr_req, tmpbuf);
				} else {
					request_ad.InsertViaCache(attr_req, ATTR_RESOURCE_REQUEST_CONSTRAINT);
				}
			}
		}
	}
#else
	if (job_id.cluster < 0 || request_ad.size() < 0) {
		dprintf(D_ALWAYS, "unexpected arguments to scheduler_getRequestConstraints\n");
	}
#endif

	return true;
}


// match_ad will be null when this is called from ScheddNegotiate::nextJob
bool MainScheddNegotiate::scheduler_skipJob(JobQueueJob * job, ClassAd *match_ad, bool & skip_all_such, const char * &because)
{
	int universe = CONDOR_UNIVERSE_MIN;
	skip_all_such = false;
	because = "job was removed";
	if ( ! job || ! job->LookupInteger(ATTR_JOB_UNIVERSE, universe)) {
		// this can happen if the job was removed while the negotiator was considering it.
		return true;
	}

	// If we need to spawn a shadow and can't, then we can't use this match
	if ((universe != CONDOR_UNIVERSE_GRID) && ! scheduler.canSpawnShadow()) {
		because = "no more shadows";
		skip_all_such = true;
		return true;
	}

	if( scheduler.AlreadyMatched(job, universe) ) {
		because = "job no longer needs a match";
		return true;
	}
	if( ! Runnable(job, because) ) {
		return true;
	}

#ifdef USE_VANILLA_START
	// match_ad will be NULL when this function is called while building up the list of resource requests to send to the negotiator
	if (UniverseUsesVanillaStartExpr(universe)) {
		int runnable = true;
		if (match_ad) {
			//PRAGMA_REMIND("add skip_all_such check to jobCanUseMatch")
			runnable = scheduler.jobCanUseMatch(job, match_ad, getRemotePool() ? getRemotePool() : "", because);
			dprintf(D_MATCH | D_VERBOSE, "jobCanUseMatch returns %d for job %d.%d (%s)\n", runnable, job->jid.cluster, job->jid.proc, because);
		} else {
			runnable = scheduler.jobCanNegotiate(job, because);
			dprintf(D_MATCH | D_VERBOSE, "jobCanNegotiate returns %d for job %d.%d (%s)\n", runnable, job->jid.cluster, job->jid.proc, because);
		}
		return ! runnable;
	}
#else
	if (match_ad) {
		// dprintf(D_MATCH | D_VERBOSE, "VANILLA_START is disabled\n");
	}
#endif

	return false;
}


bool
MainScheddNegotiate::scheduler_handleMatch(PROC_ID job_id,char const *claim_id, char const *extra_claims, ClassAd &match_ad, char const *slot_name)
{
	ASSERT( claim_id );
	ASSERT( slot_name );

	dprintf(D_MATCH,"Received match for job %d.%d (delivered=%d): %s\n",
			job_id.cluster, job_id.proc, m_current_resources_delivered, slot_name);

	if ( strcasecmp(claim_id,"null") == 0 ) {
		dprintf(D_MATCH, "Rejecting match to %s because it has no claim id\n", slot_name);
		return false;
	}

	bool scheddsAreSubmitters = false;
	if (strncmp("AllSubmittersAt", getMatchUser(), 15) == 0) {
		scheddsAreSubmitters = true;
	}

	if (scheddsAreSubmitters) {
		// Not a real match we can directly use.  Send it to our sidecar cm, and let
		// it figure out the fair-share, etc.
		return scheduler.forwardMatchToSidecarCM(claim_id, extra_claims, match_ad, slot_name);
	}

	// Claim pslots if we're configured to do so and this negotiator will
	// make further matches on the claimed pslot for us.
	// TODO Ignore the negotiator's willingness to match once we're smart
	//   enough to fully manage the claimed pslot or will be instructing
	//   the startd to send updates to an AP collector/negotiator.
	bool claim_pslot = false;
	if (m_will_match_claimed_pslots) {
		bool is_pslot = false;
		match_ad.LookupBool(ATTR_SLOT_PARTITIONABLE, is_pslot);
		if (is_pslot) {
			std::string slot_state;
			match_ad.LookupString(ATTR_STATE, slot_state);
			if (slot_state == "Unclaimed" && param_boolean("ENABLE_CLAIMABLE_PARTITIONABLE_SLOTS", false)) {
				claim_pslot = true;
			}
		}
	}

	const char* because = "";
	bool skip_all_such = false;
	JobQueueJob *job = GetJobAd(job_id);
	if (scheduler_skipJob(job, &match_ad, skip_all_such, because) && ! skip_all_such) {
		// See if it is a real match for us

		FindRunnableJob(job_id, &match_ad, getMatchUser());

		// we may have found a new job. but FindRunnableJob doesn't check to see
		// if we hit the shadow limit, so we need to do that here.
		if( job_id.cluster != -1 && job_id.proc != -1 ) {
			int universe = CONDOR_UNIVERSE_MIN;
			const OwnerInfo * powni = NULL;
			GetJobAndInfo(job_id, universe, powni);
			// If we need to spawn a shadow and can't, then we can't use this match
			if ((universe != CONDOR_UNIVERSE_GRID) && ! scheduler.canSpawnShadow()) {
				skip_all_such = true;
				because = "no more shadows";
			} else {
				dprintf(D_MATCH,"Rematched %s to job %d.%d\n", slot_name, job_id.cluster, job_id.proc );
			}
		}
	}
	if (skip_all_such) {
		// No point in trying to find a different job,
		// because we've hit MAX_JOBS_RUNNING or something
		// like that.
		dprintf(D_MATCH,"Rejecting match to %s because %s\n", slot_name, because);
		return false;
	}
	if( job_id.cluster == -1 || job_id.proc == -1 ) {
		dprintf(D_FULLDEBUG,"No job found to run on %s\n",slot_name);
		return false;
	}

	Daemon startd(&match_ad,DT_STARTD,NULL);
	if( !startd.addr() ) {
		dprintf( D_ALWAYS, "Can't find address of startd in match ad:\n" );
		dPrintAd(D_ALWAYS, match_ad);
		return false;
	}

	match_rec *mrec = scheduler.AddMrec(
		claim_id, startd.addr(), &job_id, &match_ad,
		getMatchUser(), getRemotePool() );

	if( !mrec ) {
			// There is already a match for this claim id.
		return false;
	}

	mrec->m_claim_pslot = claim_pslot;

	ContactStartdArgs *args = new ContactStartdArgs( claim_id, extra_claims, startd.addr(), false );

	if( !scheduler.enqueueStartdContact(args) ) {
		delete args;
		scheduler.DelMrec( mrec );
		return false;
	}

	return true;
}

// Negotiator has send the schedd a match to use for any user.  Forward the slot to our cm-on-the-side
// for subsequent matching
bool 
Scheduler::forwardMatchToSidecarCM(const char *claim_id, const char *extra_claims, ClassAd &match_ad, const char *slot_name) {
	dprintf(D_FULLDEBUG, "Forwarding match %s for use only for this schedd\n", slot_name);
	auto_free_ptr local_cm(param("SCHEDD_LOCAL_CM"));

	if (!local_cm) {
		dprintf(D_ALWAYS, "Got forwarding match for %s, but no knob SCHEDD_LOCAL_CM defined, dropping match\n", slot_name);
		return false;
	}

	std::string peer_addr;
	match_ad.LookupString(ATTR_MY_ADDRESS, peer_addr);

	classy_counted_ptr<DCStartd> startd = new DCStartd(slot_name, nullptr, peer_addr.c_str(),claim_id, extra_claims);
	if( !startd->addr() ) {
		dprintf( D_ALWAYS, "Can't find address of startd in match ad:\n" );
		dPrintAd(D_ALWAYS, match_ad);
		return false;
	}

	classy_counted_ptr<DCMsgCallback> cb = new DCMsgCallback(
		(DCMsgCallback::CppFunction)&Scheduler::claimStartdForUs,
		this,
		nullptr);

	ClassAd not_a_real_job;
	// Tell the startd which CM to report to for real work
	if (local_cm) {
		not_a_real_job.Assign("WorkingCM", local_cm.ptr());
	}

	// Tell the startd our name, which will go into the slot ad
	not_a_real_job.Assign(ATTR_SCHEDD_NAME, Name);

	startd->asyncRequestOpportunisticClaim(
		&not_a_real_job,
		slot_name,
		daemonCore->publicNetworkIpAddr(),
		scheduler.aliveInterval(), true,
		STARTD_CONTACT_TIMEOUT, // timeout on individual network ops
		20,       // overall timeout on completing claim request
		cb);

	
	return true;
}

void
MainScheddNegotiate::scheduler_handleJobRejected(PROC_ID job_id, int /*autcluster_id*/, char const *reason)
{
	ASSERT( reason );

	dprintf(D_FULLDEBUG, "Job %d.%d (delivered=%d) rejected: %s\n",
			job_id.cluster, job_id.proc, m_current_resources_delivered, reason);

	if ( job_id.cluster < 0 || job_id.proc < 0 ) {
		// If we asked the negotiator for matches for jobs that can no
		// longer use them, the negotiation code uses a job id of -1.-1.
		return;
	}
		// if job is gone, we are done
	JobQueueJob * job = GetJobAd(job_id);
	if ( ! job) return;

	SetAttributeString(
		job_id.cluster, job_id.proc,
		ATTR_LAST_REJ_MATCH_REASON,	reason, NONDURABLE);

	const char * negname = getNegotiatorName();
	if (negname && *negname) {
		SetAttributeString(job_id.cluster, job_id.proc, ATTR_LAST_REJ_MATCH_NEGOTIATOR, negname, NONDURABLE);
	} else if (job->Lookup(ATTR_LAST_REJ_MATCH_NEGOTIATOR)) {
		DeleteAttribute(job_id.cluster, job_id.proc, ATTR_LAST_REJ_MATCH_NEGOTIATOR);
	}

	SetAttributeInt(
		job_id.cluster, job_id.proc,
		ATTR_LAST_REJ_MATCH_TIME, time(0), NONDURABLE);
}

void
MainScheddNegotiate::scheduler_handleNegotiationFinished( Sock *sock )
{
	int rval =
		daemonCore->Register_Socket(
			sock, "<Negotiator Socket>", 
			(SocketHandlercpp)&Scheduler::negotiatorSocketHandler,
			"<Negotiator Command>", &scheduler);

	if( rval >= 0 ) {
			// do not delete this sock until we get called back
		sock->incRefCount();
	}

	// Negotiator has asked us to send it RRL, but not started negotiation proper
	// don't consider negotiation finished.
	if (RRLRequestIsPending()) {
		dprintf(D_ALWAYS,"Finished sending RRL for %s\n", getMatchUser());
		return;
	}

	// When we get here, it is a bona-fide end of negotiation
	bool satisfied = getSatisfaction();
	char const *remote_pool = getRemotePool();

	dprintf(D_ALWAYS,"Finished negotiating for %s%s%s: %d matched, %d rejected\n",
			getMatchUser(),
			remote_pool ? " in pool " : "",
			remote_pool ? remote_pool : " in local pool",
			getNumJobsMatched(), getNumJobsRejected() );

	scheduler.negotiationFinished( getMatchUser(), remote_pool, satisfied );

}

void
Scheduler::negotiationFinished( char const *owner, char const *remote_pool, bool satisfied )
{
	SubmitterData * Owner = find_submitter(owner);
	if ( ! Owner) {
		dprintf(D_ALWAYS, "Can't find owner \"%s\" in Owners array!\n", owner);
		return;
	}

	int n = 1;
	int flock_level = 0;
	if( remote_pool && *remote_pool ) {
		for (auto& flock_col : FlockCollectors) {
			if( flock_col.name() && !strcmp(remote_pool, flock_col.name()) ) {
				flock_level = n;
				break;
			}
			n++;
		}
		if( flock_level != n ) {
			dprintf(D_ALWAYS,
				"Warning: did not find flocking level for remote pool %s\n",
				remote_pool );
		}
	}

	if( satisfied || Owner->FlockLevel == flock_level ) {
			// NOTE: we do not want to set NegotiationTimestamp if
			// this negotiator is less than our current flocking level
			// and we are unsatisfied, because then if the negotiator
			// at the current flocking level never contacts us, but
			// others do, we will never give up waiting, and we will
			// therefore not advance to the next flocking level.
		Owner->NegotiationTimestamp = time(0);
	}

	if( satisfied ) {
		// We are out of jobs.  Stop flocking with less desirable pools.
		if (Owner->FlockLevel > flock_level && Owner->FlockLevel > MinFlockLevel) {
			if (flock_level < MinFlockLevel) {
				flock_level = MinFlockLevel;
			}
			dprintf(D_ALWAYS,
					"Decreasing flock level for %s to %d from %d.\n",
					owner, flock_level, Owner->FlockLevel);
			Owner->FlockLevel = flock_level;
		}

		timeout(); // invalidate our ads immediately
	} else {
		if (Owner->FlockLevel < MaxFlockLevel &&
		    Owner->FlockLevel == flock_level)
		{ 
			int oldlevel = Owner->FlockLevel;
			Owner->FlockLevel += param_integer("FLOCK_INCREMENT",1,1);
			if(Owner->FlockLevel > MaxFlockLevel) {
				Owner->FlockLevel = MaxFlockLevel;
			}
			dprintf(D_ALWAYS,
					"Increasing flock level for %s to %d from %d.\n",
					owner, Owner->FlockLevel, oldlevel);

			timeout(); // flock immediately
		}
	}
}

/*
** The negotiator wants to give us permission to run a job on some
** server.  We must negotiate to try and match one of our jobs with a
** server which is capable of running it.  NOTE: We must keep job queue
** locked during this operation.
*/

/*
  There's also a DedicatedScheduler::negotiate() method, which is
  called if the negotiator wants to run jobs for the
  "DedicatedScheduler" user.  That's called from this method, and has
  to implement a lot of the same negotiation protocol that we
  implement here.  SO, if anyone finds bugs in here, PLEASE be sure to
  check that the same bug doesn't exist in dedicated_scheduler.C.
  Also, changes the the protocol, new commands, etc, should be added
  in BOTH PLACES.  Thanks!  Yes, that's evil, but the forms of
  negotiation are radically different between the DedicatedScheduler
  and here (we're not even negotiating for specific jobs over there),
  so trying to fit it all into this function wasn't practical.  
     -Derek 2/7/01
*/
int
Scheduler::negotiate(int /*command*/, Stream* s)
{
	int		job_index;
	int		which_negotiator = 0; 		// >0 implies flocking
	std::string remote_pool_buf;
	char const *remote_pool = NULL;
	Sock*	sock = (Sock*)s;
	bool skip_negotiation = false;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

	// Set timeout on socket
	s->timeout( param_integer("NEGOTIATOR_TIMEOUT",20) );

	// Clear variable that keeps track of how long we've been waiting
	// for a negotiation cycle, since now we finally have got what
	// we've been waiting for.  If only Todd can say the same about
	// his life in general.  ;)
	NegotiationRequestTime = 0;
	m_need_reschedule = false;
	if( m_send_reschedule_timer != -1 ) {
		daemonCore->Cancel_Timer( m_send_reschedule_timer );
		m_send_reschedule_timer = -1;
	}

		// set stop/start times on the negotiate timeslice object
	ScopedTimesliceStopwatch negotiate_stopwatch( &m_negotiate_timeslice );

		//
		// CronTab Jobs
		//
	this->calculateCronTabSchedules();		

	dprintf (D_PROTOCOL, "## 2. Negotiating with CM\n");

		// reset this flag so the next time we bump into a limit
		// we issue a log message
	RecentlyWarnedMaxJobsRunning = false;

 	/* if ReservedSwap is 0, then we are not supposed to make any
 	 * swap check, so we can avoid the expensive sysapi_swap_space
 	 * calculation -Todd, 9/97 */
 	if ( ReservedSwap != 0 ) {
 		SwapSpace = sysapi_swap_space();
 	} else {
 		SwapSpace = INT_MAX;
 	}

	SwapSpaceExhausted = FALSE;
	if( ShadowSizeEstimate ) {
		MaxShadowsForSwap = (SwapSpace - ReservedSwap) / ShadowSizeEstimate;
		MaxShadowsForSwap += numShadows;
		dprintf( D_FULLDEBUG, "*** SwapSpace = %d\n", SwapSpace );
		dprintf( D_FULLDEBUG, "*** ReservedSwap = %d\n", ReservedSwap );
		dprintf( D_FULLDEBUG, "*** Shadow Size Estimate = %d\n",
				 ShadowSizeEstimate );
		dprintf( D_FULLDEBUG, "*** Start Limit For Swap = %d\n",
				 MaxShadowsForSwap );
	}

		// We want to read the owner off the wire ASAP, since if we're
		// negotiating for the dedicated scheduler, we don't want to
		// do anything expensive like scanning the job queue, creating
		// a prio rec array, etc.

	//-----------------------------------------------
	// Get Owner name from negotiator
	//-----------------------------------------------
	char owner[200];
	std::string sig_attrs_from_cm;
	int consider_jobprio_min = INT_MIN;
	int consider_jobprio_max = INT_MAX;
	ClassAd negotiate_ad;
	std::string submitter_tag;
	ExprTree *neg_constraint = NULL;
	bool scheddsAreSubmitters = false;
	bool willMatchClaimedPslots = false;
	std::string match_caps; // matchmaker capabilities
	std::string negotiator_name; // negotiator name, for use when there are multiple negotiators in a CM

	s->decode();
	// Keeping body of old if-statement to avoid re-indenting
	{
		if( !getClassAd( s, negotiate_ad ) ) {
			dprintf( D_ALWAYS, "Can't receive negotiation header\n" );
			return (!(KEEP_STREAM));
		}
		if( !negotiate_ad.LookupString(ATTR_OWNER,owner,sizeof(owner)) ) {
			dprintf( D_ALWAYS, "Can't find %s in negotiation header!\n",
					 ATTR_OWNER );
			return (!(KEEP_STREAM));
		}
		if( !negotiate_ad.LookupString(ATTR_AUTO_CLUSTER_ATTRS,sig_attrs_from_cm) ) {
			dprintf( D_ALWAYS, "Can't find %s in negotiation header!\n",
					 ATTR_AUTO_CLUSTER_ATTRS );
			return (!(KEEP_STREAM));
		}
		if( !negotiate_ad.LookupString(ATTR_SUBMITTER_TAG,submitter_tag) ) {
			dprintf( D_ALWAYS, "Can't find %s in negotiation header!\n",
					 ATTR_SUBMITTER_TAG );
			return (!(KEEP_STREAM));
		}
			// jobprio_min and jobprio_max are optional
		negotiate_ad.LookupInteger("JOBPRIO_MIN",consider_jobprio_min);
		negotiate_ad.LookupInteger("JOBPRIO_MAX",consider_jobprio_max);
		neg_constraint = negotiate_ad.Lookup(ATTR_NEGOTIATOR_JOB_CONSTRAINT);
		negotiate_ad.LookupBool(ATTR_MATCH_CLAIMED_PSLOTS, willMatchClaimedPslots);
		negotiate_ad.LookupString(ATTR_MATCH_CAPS, match_caps);
		negotiate_ad.LookupString(ATTR_NEGOTIATOR_NAME, negotiator_name);
		negotiate_ad.LookupBool(ATTR_SCHEDDS_ARE_SUBMITTERS, scheddsAreSubmitters);
		if (strncmp(owner, "AllSubmittersAt", 15) == 0) {
			scheddsAreSubmitters = true;
		}

		if (IsDebugVerbose(D_MATCH)) {
			std::string adbuf;
			dprintf(D_MATCH | D_VERBOSE, "Got NEGOTIATE with ad:\n%s", formatAd(adbuf, negotiate_ad, "\t"));
		}

	}
	if (!s->end_of_message()) {
		dprintf( D_ALWAYS, "Can't receive owner/EOM from manager\n" );
		return (!(KEEP_STREAM));
	}

		// If we utilized a non-negotiated security session, ensure the
		// pool name sent out with the session is identical to the one
		// which came back from the negotiator.  Prevent negotiator fraud!
	const std::string &sess_id = static_cast<ReliSock*>(s)->getSessionID();
	classad::ClassAd policy_ad;
	daemonCore->getSecMan()->getSessionPolicy(sess_id.c_str(), policy_ad);
	std::string policy_remote_pool;
	if (policy_ad.EvaluateAttrString(ATTR_REMOTE_POOL, policy_remote_pool)) {
		submitter_tag = policy_remote_pool;
	}

	
	if (!SubmitterMap.IsSubmitterValid(submitter_tag, owner, time(NULL)) && !scheddsAreSubmitters) {
		dprintf(D_ALWAYS, "Remote negotiator (host=%s, pool=%s) is trying to negotiate with submitter %s;"
			" that submitter was not sent to the negotiator, so aborting the negotiation attempt.\n",
			sock->peer_ip_str(), submitter_tag.c_str(), owner);
		return (!(KEEP_STREAM));
	}

	if( FlockCollectors.size() ) {
			// Use the submitter tag to figure out which negotiator we
			// are talking to.  We insert a different submitter tag
			// into the submitter ad that we send to each CM.  In fact,
			// the tag is just equal to the collector address for the CM.
		if( submitter_tag != HOME_POOL_SUBMITTER_TAG ) {
			int n = 1;
			bool match = false;
			for (auto& flock_col : FlockCollectors) {
				if( submitter_tag == flock_col.name() ){
					which_negotiator = n;
					remote_pool_buf = flock_col.name();
					remote_pool = remote_pool_buf.c_str();
					match = true;
					break;
				}
				n++;
			}
			if( !match ) {
				dprintf(D_ALWAYS, "Unknown negotiator (host=%s,tag=%s).  "
						"Aborting negotiation.\n", sock->peer_ip_str(),
						submitter_tag.c_str());
				return (!(KEEP_STREAM));
			}
		}
	}

	if( remote_pool ) {
		dprintf (D_ALWAYS, "Negotiating for owner: %s (flock level %d, pool %s)\n",
				 scheddsAreSubmitters ? "All Owners" : owner, which_negotiator, remote_pool);
	} else {
		dprintf (D_ALWAYS, "Negotiating for owner: %s\n", scheddsAreSubmitters ? "All Owners" : owner);
	}

	if ( consider_jobprio_min > INT_MIN || consider_jobprio_max < INT_MAX ) {
		dprintf(D_ALWAYS,"Negotiating owner=%s jobprio restricted, min=%d max=%d\n",
			 owner, consider_jobprio_min, consider_jobprio_max);
	}

	//-----------------------------------------------

		// See if the negotiator wants to talk to the dedicated
		// scheduler

	if( ! strcmp(owner, dedicated_scheduler.name()) ) {
		return dedicated_scheduler.negotiate( NEGOTIATE, sock, remote_pool );
	}

		// If we got this far, we're negotiating for a regular user,
		// so go ahead and do our expensive setup operations.

		// Tell the autocluster code what significant attributes the
		// negotiator told us about
	if (sig_attrs_from_cm.length() > 0) {
		if ( autocluster.config(MinimalSigAttrs, sig_attrs_from_cm.c_str()) ) {
			// clear out auto cluster id attributes
			WalkJobQueue(clear_autocluster_id);
			DirtyPrioRecArray(); // should rebuild PrioRecArray
		}
	}

	BuildPrioRecArray();
	JobsStarted = 0;

	// owner here is the ATTR_OWNER from the negotiator, which is really the Submitter (i.e. accounting group, etc)
	if (user_is_the_new_owner || scheddsAreSubmitters) {
		// SubmitterData is keyed by fully qualified names
	} else {
		// SubmitterData is keyed by bare names, so truncate owner at '@'
		char *at_sign = strchr(owner, '@');
		if (at_sign) *at_sign = '\0';
	}
	// find owner in the Owners array
	SubmitterData * Owner = find_submitter(owner);
	if ( ! Owner && !scheddsAreSubmitters) {
		dprintf(D_ALWAYS, "Can't find owner %s in Owners array!\n", owner);
		skip_negotiation = true;
	} else if (shadowsSpawnLimit() == 0) {
		// shadowsSpawnLimit() prints reason for limit of 0
		skip_negotiation = true;
	}

	ResourceRequestList *resource_requests = new ResourceRequestList;
	ResourceRequestCluster *cluster = NULL;
	int next_cluster = 0;
	int skipped_auto_cluster = -1;

	// std::string'ify owner to speed up comparisons in the loop
	std::string owner_str(owner);

	for(job_index = 0; job_index < N_PrioRecs && !skip_negotiation; job_index++) {
		prio_rec *prec = &PrioRec[job_index];

		// make sure job isn't flagged as not needing matching
		if (prec->not_runnable || prec->matched)
		{
			continue;
		}

		// make sure owner matches what negotiator wants

		if (!scheddsAreSubmitters && owner_str != prec->submitter)
		{
			continue;
		}

		// make sure jobprio is in the range the negotiator wants
		if ( consider_jobprio_min > prec->job_prio ||
			 prec->job_prio > consider_jobprio_max )
		{
			continue;
		}

		int auto_cluster_id;
		if( NegotiateAllJobsInCluster ) {
				// give every job a new auto cluster
			auto_cluster_id = ++next_cluster;
		}
		else {
			auto_cluster_id = prec->auto_cluster_id;
		}

		if ( auto_cluster_id == skipped_auto_cluster ) {
			continue;
		}

		if( !cluster || cluster->getAutoClusterId() != auto_cluster_id )
		{
				// We always trust the home pool...
			bool should_match_flock = submitter_tag != HOME_POOL_SUBMITTER_TAG;
			JobQueueJob* job_ad = (should_match_flock || neg_constraint) ? GetJobAd( prec->id ) : nullptr;

				// Determine whether we should flock this cluster with this pool.
			if (job_ad && !JobCanFlock(*job_ad, submitter_tag)) {
				continue;
			}
			if ( neg_constraint ) {
				if ( job_ad == NULL || EvalExprBool( job_ad, neg_constraint ) == false ) {
					skipped_auto_cluster = auto_cluster_id;
					continue;
				}
			}
			cluster = new ResourceRequestCluster( auto_cluster_id );
			resource_requests->push_back( cluster );
		}
		cluster->addJob( prec->id );
	}

	classy_counted_ptr<MainScheddNegotiate> sn =
		new MainScheddNegotiate(
			NEGOTIATE,
			resource_requests,
			owner,
			remote_pool
		);

	sn->setWillMatchClaimedPslots(willMatchClaimedPslots);
	sn->setMatchCaps(match_caps); // negotiator tells us about diagnostic capabilities
	sn->setNegotiatorName(negotiator_name); // self-reported negotiator name, for when there are multiple per CM

		// handle the rest of the negotiation protocol asynchronously
	sn->negotiate(sock);

	return KEEP_STREAM;
}


int
Scheduler::CmdDirectAttach(int, Stream* stream)
{
	ReliSock *rsock = (ReliSock*)stream;
	ClassAd cmd_ad;
	int num_ads = 0;
	std::string claim_id;
	ClassAd slot_ad;
	std::string slot_user;
	std::string slot_submitter;
	PROC_ID jobid;
	jobid.cluster = jobid.proc = -1;

	dprintf(D_FULLDEBUG, "Got DIRECT_ATTACH from %s\n", rsock->peer_description());

	if (!getClassAd(rsock, cmd_ad)) {
		dprintf(D_ALWAYS, "CmdDirectAttach() failed to read command ad\n");
		return 0;
	}

	slot_user = rsock->getFullyQualifiedUser();

		// If the startd doesn't set a submitter, then we'll match jobs
		// from any submitter. But the code below ensures we only match
		// jobs from the authenticated user identity.
	cmd_ad.LookupString(ATTR_SUBMITTER, slot_submitter);

		// TODO handle alternate submitter names
	MainScheddNegotiate sn(0, nullptr, slot_submitter.c_str(), nullptr);

	cmd_ad.LookupInteger(ATTR_NUM_ADS, num_ads);
	dprintf(D_FULLDEBUG, "CmdDirectAttach() reading %d slot ads\n", num_ads);

	for (int i = 0; i < num_ads; i++) {
		std::string slot_name;
		if (!rsock->get_secret(claim_id) || !getClassAd(rsock, slot_ad)) {
			dprintf(D_ALWAYS, "CmdDirectAttach() failed to read slot ad %d\n", i);
			return 0;
		}

			// TODO allow trusted users to match all jobs
			//   Could use ATTR_NEGOTIATOR_SCHEDDS_ARE_SUBMITTERS
		slot_ad.Assign(ATTR_AUTHENTICATED_IDENTITY, slot_user);
		slot_ad.Assign(ATTR_RESTRICT_TO_AUTHENTICATED_IDENTITY, true);

		slot_ad.LookupString(ATTR_NAME, slot_name);

			// TODO handle pre-claimed slots
			// TODO handle slots already in use
		sn.scheduler_handleMatch(jobid, claim_id.c_str(), "", slot_ad, slot_name.c_str());
	}

	if (!rsock->end_of_message()) {
		dprintf(D_ALWAYS, "CmdDirectAttach() failed to read eom\n");
		return 0;
	}

	ClassAd reply_ad;
	reply_ad.Assign(ATTR_ACTION_RESULT, OK);

	rsock->encode();
	if (!putClassAd(rsock, reply_ad) || !rsock->end_of_message()) {
		dprintf(D_ALWAYS, "CmdDirectAttach() failed to send reply\n");
		return 0;
	}

	return 0;
}


void
Scheduler::release_claim(int, Stream *sock)
{
	char	*claim_id = NULL;
	match_rec *mrec;

	dprintf( D_ALWAYS, "Got RELEASE_CLAIM from %s\n", 
			 sock->peer_description() );

	if (!sock->get_secret(claim_id)) {
		dprintf (D_ALWAYS, "Failed to get ClaimId\n");
		return;
	}
	if( matches->lookup(claim_id, mrec) != 0 ) {
			// We couldn't find this match in our table, perhaps it's
			// from a dedicated resource.
		dedicated_scheduler.DelMrec( claim_id );
	}
	else {
			// The startd has sent us RELEASE_CLAIM because it has
			// destroyed the claim.  There is therefore no need for us
			// to send RELEASE_CLAIM to the startd.
		mrec->needs_release_claim = false;

		DelMrec( mrec );
	}
	free(claim_id);
	dprintf (D_PROTOCOL, "## 7(*)  Completed release_claim\n");
	return;
}

void
Scheduler::contactStartd( ContactStartdArgs* args ) 
{
	dprintf( D_FULLDEBUG, "In Scheduler::contactStartd()\n" );

	match_rec *mrec = NULL;

	if( args->isDedicated() ) {
		mrec = dedicated_scheduler.FindMrecByClaimID(args->claimId());
	}
	else {
		mrec = scheduler.FindMrecByClaimID(args->claimId());
	}

	if(!mrec) {
		// The match must have gotten deleted during the time this
		// operation was queued.
		dprintf( D_FULLDEBUG, "In contactStartd(): no match record found for %s", args->claimId() );
		return;
	}

	std::string description;
	formatstr( description, "%s %d.%d", mrec->description(),
						 mrec->cluster, mrec->proc ); 

	int cluster = mrec->cluster;
	int proc = mrec->proc;

		// We need an expanded job ad here so that the startd can see
		// NegotiatorMatchExpr values.
	ClassAd *jobAd;
	if( mrec->is_dedicated ) {
		jobAd = dedicated_scheduler.GetMatchRequestAd( mrec );
	}
	else {
		jobAd = GetExpandedJobAd(JOB_ID_KEY(mrec->cluster, mrec->proc), false);
	}
	if( ! jobAd ) {
			// The match rec may have been deleted by now if the job
			// was put on hold in GetJobAd().
		mrec = NULL;

		char const *reason = "find/expand";
		if( !args->isDedicated() ) {
			if( GetJobAd(cluster, proc) ) {
				reason = "expand";
			}
			else {
				reason = "find";
			}
		}
		dprintf( D_ALWAYS,
				 "Failed to %s job when requesting claim %s\n",
				 reason, description.c_str() );

		if( args->isDedicated() ) {
			mrec = dedicated_scheduler.FindMrecByClaimID(args->claimId());
		}
		else {
			mrec = scheduler.FindMrecByClaimID(args->claimId());
		}

		if( mrec ) {
			DelMrec( mrec );
		}
		return;
	}

		// If the slot we are about to claim is partitionable, edit it
		// so it will look like the resulting dynamic slot. We want to avoid 
		// re-using a claim to a partitionable slot
		// for jobs that do not fit the dynamically created slot. In the
		// past we did this fixup during the negotiation cycle, but now that
		// we can get matches directly back from the startd, we need to do it
		// here as well.
	if ( jobAd && mrec && mrec->my_match_ad )
	{
		if ( !ScheddNegotiate::fixupPartitionableSlot(jobAd,mrec->my_match_ad) ) {
			// The job classad does not have required attributes (such as 
			// requested memory) to enable the startd to create a dynamic slot.
			// Since this claim request is simply going to fail, lets throw
			// this match away now (seems like we could do something better?) - 
			// while it is not ideal to throw away the match in this instance,
			// it is consistent with what we current do during negotiation.
			DelMrec ( mrec );
			return;
		}
		// The slot ad has just been modified to look like a dynamic slot.
		// We need to re-optimize the requirements expression to pick up
		// the modified resource values.
		OptimizeMachineAdForMatchmaking( mrec->my_match_ad );
	}

    // some attributes coming out of negotiator's matching process that need to
    // make a subway transfer from slot/match ad to job/request ad, on their way
    // to the claim, and then eventually back around to the negotiator for use in
    // preemption policies:
    CopyAttribute(ATTR_REMOTE_GROUP, *jobAd, *mrec->my_match_ad);
    CopyAttribute(ATTR_REMOTE_NEGOTIATING_GROUP, *jobAd, *mrec->my_match_ad);
    CopyAttribute(ATTR_REMOTE_AUTOREGROUP, *jobAd, *mrec->my_match_ad);

	// Tell the startd side who should send alives... startd or schedd
	jobAd->Assign( ATTR_STARTD_SENDS_ALIVES, mrec->m_startd_sends_alives );	
	// Tell the startd if to should not send alives if starter is alive
	jobAd->Assign( ATTR_STARTER_HANDLES_ALIVES, 
					param_boolean("STARTER_HANDLES_ALIVES",true) );

	// Tell the startd our name, which will go into the slot ad
	jobAd->Assign(ATTR_SCHEDD_NAME, Name);

	// Setup to claim the slot asynchronously

	classy_counted_ptr<DCMsgCallback> cb = new DCMsgCallback(
		(DCMsgCallback::CppFunction)&Scheduler::claimedStartd,
		this,
		mrec);

	ASSERT( !mrec->claim_requester.get() );
	mrec->claim_requester = cb;
	mrec->setStatus( M_STARTD_CONTACT_LIMBO );

	classy_counted_ptr<DCStartd> startd = new DCStartd(mrec->description(),NULL,mrec->peer,mrec->claim_id.claimId(), args->extraClaims());

	this->num_pending_startd_contacts++;

	int deadline_timeout = -1;
	if( RequestClaimTimeout > 0 ) {
			// Add in a little slop time so that schedd has a chance
			// to cancel operation before deadline runs out.
			// This results in a slightly more friendly log message.
		deadline_timeout = RequestClaimTimeout + 60;
	}

	startd->asyncRequestOpportunisticClaim(
		jobAd,
		description.c_str(),
		daemonCore->publicNetworkIpAddr(),
		scheduler.aliveInterval(), mrec->m_claim_pslot,
		STARTD_CONTACT_TIMEOUT, // timeout on individual network ops
		deadline_timeout,       // overall timeout on completing claim request
		cb );

	delete jobAd;

		// Now wait for callback...
}

void 
Scheduler::claimStartdForUs(DCMsgCallback *cb) {
	ClaimStartdMsg *msg = (ClaimStartdMsg *)cb->getMessage();
	// Should we do something special here?
	dprintf(D_FULLDEBUG, "Completed claiming startd for sidecar CM for %s\n", msg->description());
}

void
Scheduler::claimedStartd( DCMsgCallback *cb ) {
	ClaimStartdMsg *msg = (ClaimStartdMsg *)cb->getMessage();

	this->num_pending_startd_contacts--;
	scheduler.rescheduleContactQueue();

	match_rec *match = (match_rec *)cb->getMiscDataPtr();
	if( msg->deliveryStatus() == DCMsg::DELIVERY_CANCELED && !match) {
		// if match is NULL, then this message must have been canceled
		// from within ~match_rec, in which case there is nothing to do
		return;
	}
	ASSERT( match );

		// Remove callback pointer from the match record, since the claim
		// request operation is finished.
	match->claim_requester = NULL;

	if( !msg->claimed_startd_success() ) {
		// Re-enable the job for matching in the PrioRec array
		for (int i = 0; i < N_PrioRecs; i++) {
			if (PrioRec[i].id.cluster == match->cluster && PrioRec[i].id.proc == match->proc) {
				PrioRec[i].matched = false;
				break;
			}
		}
		scheduler.DelMrec(match);
		return;
	}

	// If we got information for the newly-claimed slot, then update our
	// local data to match.
	// Make sure to preserve attributes from the old ad that were added
	// outside of the startd.
	// If the ClaimId changed, make a new match_rec with the new ClaimId
	// and copy over all of the job-related data.
	// For now, delete the old match_rec. Eventually, we may want to keep
	// it around (it should be for the claimed pslot).
	if (msg->have_claimed_slot_info()) {
		for (auto & slotInfo : msg->claimed_slots()) {
			if (slotInfo.claim_id != match->claim_id.claimId()) {
				PROC_ID job_id(match->cluster, match->proc);
				SetMrecJobID(match, -1, -1);
				ClassAd slotAd(slotInfo.slot_ad);
				slotAd.Update(match->m_added_attrs);
				match_rec* new_match = AddMrec(slotInfo.claim_id.c_str(), match->peer, &job_id, &slotAd, match->user, match->m_pool.c_str());
				if (new_match) {
					// AddMrec can fail and return null for reasons other than out-of-memory
					DelMrec(match);
					match = new_match;
				}
			} else {
				match->my_match_ad->CopyFrom(slotInfo.slot_ad);
				match->my_match_ad->Update(match->m_added_attrs);
			}

			break; // TODO: remove this to handle more than a single slot in the claim reply
		}
	}

	match->setStatus( M_CLAIMED );

	// now that we've completed authentication (if enabled),
	// authorize this startd for READ operations
	//
	if ( match->auth_hole_id == NULL ) {
		match->auth_hole_id = new std::string;
		ASSERT(match->auth_hole_id != NULL);
		if (msg->startd_fqu() && *msg->startd_fqu()) {
			formatstr(*match->auth_hole_id, "%s/%s",
			                            msg->startd_fqu(),
			                            msg->startd_ip_addr());
		}
		else {
			*match->auth_hole_id = msg->startd_ip_addr();
		}
		IpVerify* ipv = daemonCore->getSecMan()->getIpVerify();
		if (!ipv->PunchHole(READ, *match->auth_hole_id)) {
			dprintf(D_ALWAYS,
			        "WARNING: IpVerify::PunchHole error for %s: "
			            "job %d.%d may fail to execute\n",
			        match->auth_hole_id->c_str(),
			        match->cluster,
			        match->proc);
			delete match->auth_hole_id;
			match->auth_hole_id = NULL;
		}
	}

	// If the startd returned any "leftover" partitionable slot resources,
	// we want to create a match record for it (so we can subsequently find
	// a job to run on it). 
	if ( msg->have_leftovers()) {			

		ScheddNegotiate *sn;
		if (match->is_dedicated) {
			// Pass NULLs to constructor since we aren't actually going to
			// negotiate - we just want to invoke 
			// MainScheddNegotiate::scheduler_handleMatch(), which
			// probably could/should be changed to be declared as a static method.
			// Actually, must pass in owner so FindRunnableJob will find a job.

			sn = new DedicatedScheddNegotiate(0, NULL, match->user,
				match->getPool().empty() ? nullptr : match->getPool().c_str());
		} else {
			// Use the DedSched
			sn = new MainScheddNegotiate(0, NULL, match->user,
				match->getPool().empty() ? nullptr : match->getPool().c_str());
		}		

			// Setting cluster.proc to -1.-1 should result in the schedd
			// invoking FindRunnableJob to select an appropriate matching job.
		PROC_ID jobid;
		jobid.cluster = -1; jobid.proc = -1;

		if (match->is_dedicated) {
			const ClassAd *msg_ad = msg->getJobAd();
			msg_ad->LookupInteger(ATTR_CLUSTER_ID, jobid.cluster);
			msg_ad->LookupInteger(ATTR_PROC_ID, jobid.proc);
		}
	
		std::string last_slot_name;
		msg->leftover_startd_ad()->LookupString(ATTR_LAST_SLOT_NAME, last_slot_name);

		if (last_slot_name.length() > 0) {
			match->my_match_ad->Assign(ATTR_NAME, last_slot_name);
		}

			// Need to pass handleMatch a slot name; grab from newly split slot ad
		std::string slot_name_buf;
		msg->leftover_startd_ad()->LookupString(ATTR_NAME,slot_name_buf);
		char const *slot_name = slot_name_buf.c_str();

		// Copy attributes that were added to the original slot ad after
		// it left the startd into the fresh pslot leftovers ad
		msg->leftover_startd_ad()->Update(match->m_added_attrs);

			// dprintf a message saying we got a new match, but be certain
			// to only output the public claim id (keep the capability private)
		ClaimIdParser idp( msg->leftover_claim_id() );
		dprintf( D_FULLDEBUG,
				"Received match from startd, leftover slot ad %s claim %s\n",
				slot_name, idp.publicClaimId()  );

			// Tell the schedd about the leftover resources it can go claim.
			// Note this claiming will happen asynchronously.
		sn->scheduler_handleMatch(jobid,msg->leftover_claim_id(), "",
			*(msg->leftover_startd_ad()),last_slot_name.length() > 0 ? last_slot_name.c_str() : slot_name);

		delete sn;
	} 

	if (match->is_dedicated) {
			// Set a timer to call handleDedicatedJobs() when we return,
			// since we might be able to spawn something now.
		dedicated_scheduler.handleDedicatedJobTimer( 0 );
	}
	else {
		scheduler.StartJob( match );
	}
}


bool
Scheduler::enqueueStartdContact( ContactStartdArgs* args )
{
	 startdContactQueue.push(args);
	 dprintf( D_FULLDEBUG, "Enqueued contactStartd startd=%s\n",
			  args->sinful() );  

	 rescheduleContactQueue();

	 return true;
}


void
Scheduler::rescheduleContactQueue()
{
	if( startdContactQueue.empty() ) {
		return; // nothing to do
	}
		 /*
		   If we haven't already done so, register a timer to go off
           in zero seconds to call checkContactQueue().  This will
		   start the process of claiming the startds *after* we have
           completed negotiating and returned control to daemonCore. 
		 */
	if( checkContactQueue_tid == -1 ) {
		checkContactQueue_tid = daemonCore->Register_Timer( 0,
			(TimerHandlercpp)&Scheduler::checkContactQueue,
			"checkContactQueue", this );
	}
	if( checkContactQueue_tid == -1 ) {
			// Error registering timer!
		EXCEPT( "Can't register daemonCore timer!" );
	}
}

void
Scheduler::checkContactQueue( int /* timerID */ )
{
	ContactStartdArgs *args;

		// clear out the timer tid, since we made it here.
	checkContactQueue_tid = -1;

		// Contact startds as long as (a) there are still entries in our
		// queue, (b) there are not too many registered sockets in
		// daemonCore, which ensures we do not run ourselves out
		// of socket descriptors.
	while( !daemonCore->TooManyRegisteredSockets() &&
		   (num_pending_startd_contacts < max_pending_startd_contacts
            || max_pending_startd_contacts <= 0) &&
		   (!startdContactQueue.empty()) ) {
			// there's a pending registration in the queue:

		args = startdContactQueue.front();
		startdContactQueue.pop();
		dprintf( D_FULLDEBUG, "In checkContactQueue(), args = %p, "
				 "host=%s\n", args, args->sinful() ); 
		contactStartd( args );
		delete args;
	}
}


bool
Scheduler::enqueueReconnectJob( PROC_ID job )
{
	 jobsToReconnect.push_back(job);
	 dprintf( D_FULLDEBUG,
			  "Enqueued job %d.%d to spawn shadow for reconnect\n",
			  job.cluster, job.proc );

		 /*
		   If we haven't already done so, register a timer to go off
           in zero seconds to call checkContactQueue().  This will
		   start the process of claiming the startds *after* we have
           completed negotiating and returned control to daemonCore. 
		 */
	if( checkReconnectQueue_tid == -1 ) {
		checkReconnectQueue_tid = daemonCore->Register_Timer( 0,
			(TimerHandlercpp)&Scheduler::checkReconnectQueue,
			"checkReconnectQueue", this );
	}
	if( checkReconnectQueue_tid == -1 ) {
			// Error registering timer!
		EXCEPT( "Can't register daemonCore timer!" );
	}
	return true;
}


void
Scheduler::checkReconnectQueue( int /* timerID */ )
{
		// clear out the timer tid, since we made it here.
	checkReconnectQueue_tid = -1;

	for ( PROC_ID job: jobsToReconnect) {
		makeReconnectRecords(&job, nullptr);
	}
	jobsToReconnect.clear();
}


void
Scheduler::makeReconnectRecords( PROC_ID* job, const ClassAd* match_ad ) 
{
	int cluster = job->cluster;
	int proc = job->proc;
	char* pool = NULL;
	std::string user;
	std::string claim_id;
	char* startd_addr = NULL;
	char* startd_name = NULL;
	char* startd_principal = NULL;

	// NOTE: match_ad could be deallocated when this function returns,
	// so if we need to keep it around, we must make our own copy of it.
	if( GetAttributeString(cluster, proc, ATTR_ACCOUNTING_GROUP, user) < 0 ) {
		if( GetAttributeString(cluster, proc, attr_JobUser.c_str(), user) < 0 ) {
				// we've got big trouble, just give up.
			dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
					 attr_JobUser.c_str(), cluster, proc );
			mark_job_stopped( job );
			scheduler.stats.JobsRestartReconnectsAttempting -= 1;
			scheduler.stats.JobsRestartReconnectsFailed += 1;
			return;
		}
	} else if (user_is_the_new_owner) {
		// if using fully qualified usernames, we have to append the domain to accounting groups
		user += std::string("@") + AccountingDomain;
	}
	if( GetPrivateAttributeString(cluster, proc, ATTR_CLAIM_ID, claim_id) < 0 ) {
			//
			// No attribute. Clean up and return
			//
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				ATTR_CLAIM_ID, cluster, proc );
		mark_job_stopped( job );
		scheduler.stats.JobsRestartReconnectsAttempting -= 1;
		scheduler.stats.JobsRestartReconnectsFailed += 1;
		return;
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_REMOTE_HOST, &startd_name) < 0 ) {
			//
			// No attribute. Clean up and return
			//
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				ATTR_REMOTE_HOST, cluster, proc );
		mark_job_stopped( job );
		scheduler.stats.JobsRestartReconnectsAttempting -= 1;
		scheduler.stats.JobsRestartReconnectsFailed += 1;
		return;
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_STARTD_IP_ADDR, &startd_addr) < 0 ) {
			// We only expect to get here when reading a job queue created
			// by a version of Condor older than 7.1.3, because we no longer
			// rely on the claim id to tell us how to connect to the startd.
		dprintf( D_ALWAYS, "WARNING: %s not in job queue for %d.%d, "
				 "so using claimid.\n", ATTR_STARTD_IP_ADDR, cluster, proc );
		ClaimIdParser id_parser(claim_id.c_str());
		startd_addr = strdup(id_parser.startdSinfulAddr());
		SetAttributeString(cluster, proc, ATTR_STARTD_IP_ADDR, startd_addr);
	}
	
	int universe = -1;
	GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &universe );
	ASSERT(universe != -1);


	if( GetAttributeStringNew(cluster, proc, ATTR_REMOTE_POOL,
							  &pool) < 0 ) {
		pool = NULL;
	}

	if( 0 > GetAttributeStringNew(cluster,
	                              proc,
	                              ATTR_STARTD_PRINCIPAL,
	                              &startd_principal)) {
		startd_principal = NULL;
	}

	WriteUserLog* ULog = this->InitializeUserLog( *job );
	if ( ULog ) {
		JobDisconnectedEvent event;
		const char* txt = "Local schedd and job shadow died, "
			"schedd now running again";
		event.setDisconnectReason(txt);
		event.startd_addr = startd_addr;
		event.startd_name = startd_name;

		if( !ULog->writeEventNoFsync(&event,GetJobAd(cluster,proc)) ) {
			dprintf( D_ALWAYS, "Unable to log ULOG_JOB_DISCONNECTED event\n" );
		}
		delete ULog;
		ULog = NULL;
	}

	dprintf( D_FULLDEBUG, "Adding match record for disconnected job %d.%d "
			 "(%s: %s)\n", cluster, proc, attr_JobUser.c_str(), user.c_str() );
	ClaimIdParser idp( claim_id.c_str() );
	dprintf( D_FULLDEBUG, "ClaimId: %s\n", idp.publicClaimId() );
	if( pool ) {
		dprintf( D_FULLDEBUG, "Pool: %s (via flocking)\n", pool );
	}
		// note: AddMrec will makes its own copy of match_ad
	match_rec *mrec = AddMrec( claim_id.c_str(), startd_addr, job, match_ad, 
							   user.c_str(), pool );

		// authorize this startd for READ access
	if (startd_principal != NULL) {
		mrec->auth_hole_id = new std::string(startd_principal);
		ASSERT(mrec->auth_hole_id != NULL);
		free(startd_principal);
		IpVerify* ipv = daemonCore->getIpVerify();
		if (!ipv->PunchHole(READ, *mrec->auth_hole_id)) {
			dprintf(D_ALWAYS,
			        "WARNING: IpVerify::PunchHole error for %s: "
			            "job %d.%d may fail to execute\n",
			        mrec->auth_hole_id->c_str(),
			        mrec->cluster,
			        mrec->proc);
			delete mrec->auth_hole_id;
			mrec->auth_hole_id = NULL;
		}
	}

	if( pool ) {
		free( pool );
		pool = NULL;
	}
	if( startd_addr ) {
		free( startd_addr );
		startd_addr = NULL;
	}
	if( startd_name ) {
		free( startd_name );
		startd_name = NULL;
	}

		// this should never be NULL, particularly after the checks
		// above, but just to be extra safe, check here, too.
	if( !mrec ) {
		dprintf( D_ALWAYS, "ERROR: failed to create match_rec for %d.%d\n",
				 cluster, proc );
		mark_job_stopped( job );
		return;
	}
	
	mrec->setStatus( M_CLAIMED );  // it's claimed now.  we'll set
								   // this to active as soon as we
								   // spawn the reconnect shadow.

		/*
		  We don't want to use the version of add_shadow_rec() that
		  takes arguments for the data and creates a new record since
		  it won't know we want this srec for reconnect mode, and will
		  do a few other things we don't want.  instead, just create
		  the shadow reconrd ourselves, since that's all the other
		  function really does.  Later on, we'll call the version of
		  add_shadow_rec that just takes a pointer to an srec, since
		  that's the one that actually does all the interesting work
		  to add it to all the tables, etc, etc.
		*/
	shadow_rec *srec = new shadow_rec;
	srec->pid = 0;
	srec->job_id.cluster = cluster;
	srec->job_id.proc = proc;
	srec->universe = universe;
	srec->match = mrec;
	srec->preempted = FALSE;
	srec->removed = FALSE;
	srec->conn_fd = -1;
	srec->isZombie = FALSE; 
	srec->is_reconnect = true;
	srec->keepClaimAttributes = false;

		// the match_rec also needs to point to the srec...
	mrec->shadowRec = srec;

		// finally, enqueue this job in our RunnableJob queue.
	addRunnableJob( srec );
}

/**
 * Given a ClassAd from the job queue, we check to see if it
 * has the ATTR_SCHEDD_INTERVAL attribute defined. If it does, then
 * then we will simply update it with the latest value
 * 
 * @param job - the ClassAd to update
 * @return true if no error occurred, false otherwise
 **/
int
updateSchedDInterval( JobQueueJob *job, const JOB_ID_KEY& id, void* /*user*/ )
{
		//
		// Check if the job has the ScheddInterval attribute set
		// If so, then we need to update it
		//
	if ( job->LookupExpr( ATTR_SCHEDD_INTERVAL ) ) {
			//
			// This probably isn't a too serious problem if we
			// are unable to update the job ad
			//
		if ( ! job->Assign( ATTR_SCHEDD_INTERVAL, (int)scheduler.SchedDInterval.getMaxInterval() ) ) {
			dprintf( D_ALWAYS, "Failed to update job %d.%d's %s attribute!\n",
							   id.cluster, id.proc, ATTR_SCHEDD_INTERVAL );
		}
	}
	return ( true );
}

// stuff in the schedd that must be initialzed after InitJobQueue
void
PostInitJobQueue()
{
	mark_jobs_idle();
	load_job_factories();

	daemonCore->Register_Timer( 0,
						(TimerHandlercpp)&Scheduler::WriteRestartReport,
						"Scheduler::WriteRestartReport", &scheduler );

		// The below must happen _after_ InitJobQueue is called.
	if ( scheduler.autocluster.config(scheduler.MinimalSigAttrs) ) {
		// clear out auto cluster id attributes
		WalkJobQueue(clear_autocluster_id);
	}

		//
		// Update the SchedDInterval attributes in jobs if they
		// have it defined. This will be for JobDeferral and
		// CronTab jobs
		//
	WalkJobQueue(updateSchedDInterval);

	extern int dump_job_q_stats(int cat);
	dump_job_q_stats(D_FULLDEBUG);
}


void
Scheduler::ExpediteStartJobs() const
{
	if( startjobsid == -1 ) {
		return;
	}

	Timeslice timeslice;
	ASSERT( daemonCore->GetTimerTimeslice( startjobsid, timeslice ) );

	if( !timeslice.isNextRunExpedited() ) {
		timeslice.expediteNextRun();
		ASSERT( daemonCore->ResetTimerTimeslice( startjobsid, timeslice ) );
		dprintf(D_FULLDEBUG,"Expedited call to StartJobs()\n");
	}
}

/*
 * Weiru
 * This function iterate through all the match records, for every match do the
 * following: if the match is inactive, which means that the agent hasn't
 * returned, do nothing. If the match is active and there is a job running,
 * do nothing. If the match is active and there is no job running, then start
 * a job. For the third situation, there are two cases. We might already know
 * what job to execute because we used a particular job to negoitate. Or we
 * might not know. In the first situation, the proc field of the match record
 * will be a number greater than or equal to 0. In this case we just start the
 * job in the match record. In the other case, the proc field will be -1, we'll
 * have to pick a job from the same cluster and start it. In any case, if there
 * is no more job to start for a match, inform the startd and the accountant of
 * it and delete the match record.
 *
 * Jim B. -- Also check for SCHED_UNIVERSE jobs that need to be started.
 */
void
Scheduler::StartJobs( int /* timerID */ )
{
	match_rec *rec;
    
		/* If we are trying to exit, don't start any new jobs! */
	if ( ExitWhenDone ) {
		return;
	}
	
		//
		// CronTab Jobs
		//
	this->calculateCronTabSchedules();
		
	dprintf(D_FULLDEBUG, "-------- Begin starting jobs --------\n");
	matches->startIterations();
	while(matches->iterate(rec) == 1) {
			// If it's not in M_CLAIMED status, then it's not ready
			// to start a job.
		if ( rec->status == M_CLAIMED ) {
			StartJob( rec );
		}
	}
	if( LocalUniverseJobsIdle > 0 || SchedUniverseJobsIdle > 0 ) {
		AddRunnableLocalJobs();
	}

	dprintf(D_FULLDEBUG, "-------- Done starting jobs --------\n");
}

void
Scheduler::StartJob(match_rec *rec)
{
	PROC_ID id;

	ASSERT( rec );
	switch(rec->status) {
	case M_UNCLAIMED:
		dprintf(D_FULLDEBUG, "match (%s) unclaimed\n", rec->description());
		return;
	case M_STARTD_CONTACT_LIMBO:
		dprintf ( D_FULLDEBUG, "match (%s) waiting for startd contact\n", 
				  rec->description() );
		return;
	case M_ACTIVE:
	case M_CLAIMED:
		if ( rec->shadowRec ) {
			dprintf(D_FULLDEBUG, "match (%s) already running a job\n",
					rec->description());
			return;
		}
			// Go ahead and start a shadow.
		break;
	default:
		EXCEPT( "Unknown status in match rec (%d)", rec->status );
	}

	id.cluster = rec->cluster;
	id.proc = rec->proc;
	const char * reason = "job was removed";
	JobQueueJob * job = GetJobAd(id);
	if ( ! Runnable(job, reason) || ! jobCanUseMatch(job, rec->my_match_ad, rec->getPool(), reason)) {
		if (IsDebugLevel(D_MATCH)) {
			dprintf(D_MATCH, "match (%s) was activated, but job %d.%d no longer runnable because %s. searching for new job\n", 
				rec->description(), id.cluster, id.proc, reason);
		}
			// find the job with the highest priority
		id.proc = -1;
		if( !FindRunnableJobForClaim(rec) ) {
			return;
		}
		id.cluster = rec->cluster;
		id.proc = rec->proc;
	}

	if(!(rec->shadowRec = StartJob(rec, &id))) {
                
			// Start job failed. Throw away the match. The reason being that we
			// don't want to keep a match around and pay for it if it's not
			// functioning and we don't know why. We might as well get another
			// match.

		dprintf(D_ALWAYS,"Failed to start job for %s; relinquishing\n",
				rec->description());
		DelMrec(rec);
		mark_job_stopped( &id );

			/* We want to send some email to the administrator
			   about this.  We only want to do it once, though. */
		if ( !sent_shadow_failure_email ) {
			sent_shadow_failure_email = TRUE;
			FILE *email = email_admin_open("Failed to start shadow.");
			if( email ) {
				fprintf( email,
						 "Condor failed to start the condor_shadow.\n\n"
						 "This may be a configuration problem or a "
						 "problem with\n"
						 "permissions on the condor_shadow binary.\n" );
				char *schedlog = param ( "SCHEDD_LOG" );
				if ( schedlog ) {
					email_asciifile_tail( email, schedlog, 50 );
					free ( schedlog );
				}
				email_close ( email );
			} else {
					// Error sending the message
				dprintf( D_ALWAYS, 
						 "ERROR: Can't send email to the Condor "
						 "Administrator\n" );
			}
		}
		return;
	}
	dprintf(D_FULLDEBUG, "Match (%s) - running %d.%d\n",
			rec->description(), id.cluster, id.proc);

               // We've commited to starting a job on this match, copy the
               // job's keep_idle times to the match
	int keep_claim_idle_time = 0;
    GetAttributeInt(id.cluster,id.proc,ATTR_JOB_KEEP_CLAIM_IDLE,&keep_claim_idle_time);
    if (keep_claim_idle_time > 0) {
            rec->keep_while_idle = keep_claim_idle_time;
    } else {
            rec->keep_while_idle = 0;
    }
    rec->idle_timer_deadline = 0;

		// Now that the shadow has spawned, consider this match "ACTIVE"
	rec->setStatus( M_ACTIVE );
}

bool
Scheduler::FindRunnableJobForClaim(match_rec* mrec)
{
	ASSERT( mrec );

	PROC_ID new_job_id;
	new_job_id.cluster = -1;
	new_job_id.proc = -1;

	if( mrec->my_match_ad && !ExitWhenDone ) {
		FindRunnableJob(new_job_id,mrec->my_match_ad,mrec->user);
	}
	auto job_ad = GetJobAd(new_job_id);
	if (job_ad && !JobCanFlock(*job_ad, mrec->getPool())) {
		return false;
	}

	if( new_job_id.proc == -1 ) {
			// no more jobs to run
		if (mrec->idle_timer_deadline < time(0))  {
			dprintf(D_ALWAYS,
				"match (%s) out of jobs; relinquishing\n",
				mrec->description() );
			DelMrec(mrec);
			return false;
		} else {
			dprintf(D_FULLDEBUG, "Job requested to keep this claim idle for next job for %d seconds\n", mrec->keep_while_idle);
		}
		return false;
	}

	dprintf(D_ALWAYS,
			"match (%s) switching to job %d.%d\n",
			mrec->description(), new_job_id.cluster, new_job_id.proc );

	SetMrecJobID(mrec,new_job_id);
	return true;
}

void
Scheduler::AddRunnableLocalJobs()
{
	if ( ExitWhenDone ) {
		return;
	}
	  
	double begin = _condor_debug_get_time_double();
	
	// Instead of walking the job queue, walk the prio queue of local jobs
	for (std::set<LocalJobRec>::iterator it = LocalJobsPrioQueue.begin(); it != LocalJobsPrioQueue.end(); /*++it not here*/) {
		std::set<LocalJobRec>::iterator curr = it++; // save and advance the iterator, in case we want to erase the item
		JobQueueJob* job = GetJobAd(curr->job_id);
		// If this id no longer refers to a job, remove it from the list.
		if (!job) { 
			// TODO: Should we warn when this happens??
			LocalJobsPrioQueue.erase(curr);
			continue;
		}

		// We only want to look at jobs that are in IDLE status
		// TODO: Figure out why RunLocalJob (formerly find_idle_local_jobs)
		// also allows RUNNING and TRANSFERRING_OUTPUT jobs to go ahead
		int status;
		job->LookupInteger(ATTR_JOB_STATUS, status);
		if (status != IDLE) continue;
		// TODO: Figure out where we enforce the START_SCHEDULER_UNIVERSE constraint
		// TODO: Make sure that StartLocalJob does not do anything to WalkJobQ_start_local_jobs_runtime
		int univ = job->Universe();
		if (univ == CONDOR_UNIVERSE_LOCAL && scheduler.m_use_startd_for_local) {
			continue;
		}

		if (job->IsNoopJob()) {
			continue;
		}

		PROC_ID id = job->jid;
		int	cur_hosts;
		int	max_hosts;

		if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) != 1) {
			cur_hosts = 0; // At this point the job must be status idle
		}
		if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) != 1) {
			max_hosts = 1; 
		}
	
		//
		// Before evaluating whether we can run this job, first make 
		// sure its even eligible to run
		//
		if ( max_hosts > cur_hosts) {
			
			if (!IsLocalJobEligibleToRun(job)) {
				continue;
			}

			//
			// It's safe to go ahead and run the job!
			//
			if( univ == CONDOR_UNIVERSE_LOCAL ) {
				dprintf( D_FULLDEBUG, "Found idle local universe job %d.%d\n",
					 id.cluster, id.proc );
				shadow_rec* local_rec = NULL;

				// set our CurrentHosts to 1 so we don't consider this job
				// still idle.  we'll actually mark it as status RUNNING once
				// we spawn the starter for it.  unlike other kinds of jobs,
				// local universe jobs don't have to worry about having the
				// status wrong while the job sits in the RunnableJob queue,
				// since we're not negotiating for them at all... 
				SetAttributeInt( id.cluster, id.proc, ATTR_CURRENT_HOSTS, 1 );

				//
				// If we start a local universe job, the LocalUniverseJobsRunning
				// tally isn't updated so we have no way of knowing whether we can
				// start the next job. This would cause a ton of local jobs to
				// just get fired off all at once even though there was a limit set.
				// So instead, I am following Derek's example with the scheduler 
				// universe and updating our running tally
				// Andy - 11.14.2004 - pavlo@cs.wisc.edu
				//
				if ( this->LocalUniverseJobsIdle > 0 ) {
					this->LocalUniverseJobsIdle--;
				}
				this->LocalUniverseJobsRunning++;

				local_rec = add_shadow_rec( 0, &id, CONDOR_UNIVERSE_LOCAL, NULL, -1 );
				addRunnableJob( local_rec );
			} else {
				// if there is a per-owner scheduler job limit that is smaller than the per-owner job limit
				if (scheduler.MaxRunningSchedulerJobsPerOwner > 0 &&
					scheduler.MaxRunningSchedulerJobsPerOwner < scheduler.MaxJobsPerOwner) {
					OwnerInfo * owndat = scheduler.get_ownerinfo(job);
					if ( ! owndat) {
						dprintf( D_FULLDEBUG,
							"Skipping idle scheduler universe job %d.%d because it has no ownerinfo pointer\n",
							id.cluster, id.proc);
						continue;
					}
					if (owndat->num.SchedulerJobsRunning >= scheduler.MaxRunningSchedulerJobsPerOwner) {
						dprintf( D_FULLDEBUG,
							 "Skipping idle scheduler universe job %d.%d because %s already has %d Scheduler jobs running\n",
							 id.cluster, id.proc, owndat->Name(), owndat->num.SchedulerJobsRunning );
						continue;
					}
				} else if (scheduler.MaxRunningSchedulerJobsPerOwner == 0) {
					dprintf( D_FULLDEBUG,
						 "Skipping idle scheduler universe job %d.%d because Scheduler Universe is disabled by MaxRunningSchedulerJobsPerOwner==0 \n",
						 id.cluster, id.proc );
					continue;
				}
				dprintf( D_FULLDEBUG,
					 "Found idle scheduler universe job %d.%d\n",
					 id.cluster, id.proc );
				/*
				  we've decided to spawn a scheduler universe job.
				  instead of doing that directly, we'll go through our
				  aboutToSpawnJobHandler() hook isntead.  inside
				  aboutToSpawnJobHandlerDone(), if the job is a
				  scheduler universe job, we'll spawn it then.  this
				  wrapper handles all the logic for if we want to
				  invoke the hook in its own thread or not, etc.
				*/
				callAboutToSpawnJobHandler( id.cluster, id.proc, NULL );
			}
		}
	}
	double runtime = _condor_debug_get_time_double() - begin;
	WalkJobQ_add_runnable_local_jobs_runtime += runtime;
}

bool
Scheduler::IsLocalJobEligibleToRun(JobQueueJob* job) {

	PROC_ID id = job->jid;

	// The jobs will now attempt to have their requirements
	// evalulated. We first check to see if the requirements are defined.
	// If they are not, then we'll continue.
	// If they are, then we make sure that they evaluate to true.
	// Evaluate the schedd's ad and the job ad against each 
	// other. We break it out so we can print out any errors 
	// as needed
	//
	ClassAd scheddAd;
	scheduler.publish( &scheddAd );

	//
	// Select the start expression based on the universe
	//
	int univ = job->Universe();
	const char *universeExp = ( univ == CONDOR_UNIVERSE_LOCAL ?
							ATTR_START_LOCAL_UNIVERSE :
							ATTR_START_SCHEDULER_UNIVERSE );

	//
	// Start Universe Evaluation (a.k.a. schedd requirements).
	// Only if this attribute is NULL or evaluates to true 
	// will we allow a job to start.
	//
	bool requirementsMet = true;
	if ( scheddAd.LookupExpr( universeExp ) != NULL ) {
		//
		// We have this inner block here because the job
		// should not be allowed to start if the schedd's 
		// requirements failed to evaluate for some reason
		//
		if ( EvalBool( universeExp, &scheddAd, job, requirementsMet ) ) {
			if ( ! requirementsMet ) {
				dprintf( D_FULLDEBUG, "%s evaluated to false for job %d.%d. "
									"Unable to start job.\n",
									universeExp, id.cluster, id.proc );
			}
		} else {
			requirementsMet = false;
			dprintf( D_ALWAYS, "The schedd's %s attribute could "
							"not be evaluated for job %d.%d. "
							"Unable to start job\n",
							universeExp, id.cluster, id.proc );
		}
	}

	if ( ! requirementsMet) {
		if (IsDebugVerbose(D_ALWAYS)) {
			char *exp = sPrintExpr( scheddAd, universeExp );
			if ( exp ) {
				dprintf( D_FULLDEBUG, "Failed expression '%s'\n", exp );
				free( exp );
			}
		}
		return false;
	}
	//
	// Job Requirements Evaluation
	//
	if ( job->LookupExpr( ATTR_REQUIREMENTS ) != NULL ) {
			// Treat undefined/error as FALSE for job requirements, too.
		if ( EvalBool(ATTR_REQUIREMENTS, job, &scheddAd, requirementsMet) ) {
			if ( !requirementsMet ) {
				dprintf( D_FULLDEBUG, "The %s attribute for job %d.%d "
							"evaluated to false. Unable to start job\n",
							ATTR_REQUIREMENTS, id.cluster, id.proc );
			}
		} else {
			requirementsMet = false;
			dprintf( D_FULLDEBUG, "The %s attribute for job %d.%d did "
						"not evaluate. Unable to start job\n",
						ATTR_REQUIREMENTS, id.cluster, id.proc );
		}

	}
	//
	// If the job's requirements failed up above, we will want to 
	// print the expression to the user and return
	//
	if ( ! requirementsMet) {
		if (IsDebugVerbose(D_ALWAYS)) {
			char *exp = sPrintExpr( *job, ATTR_REQUIREMENTS );
			if ( exp ) {
				dprintf( D_FULLDEBUG, "Failed expression '%s'\n", exp );
				free( exp );
			// This is too verbose.
			//dprintf(D_FULLDEBUG,"Schedd ad that failed to match:\n");
			//dPrintAd(D_FULLDEBUG, scheddAd);
			//dprintf(D_FULLDEBUG,"Job ad that failed to match:\n");
			//dPrintAd(D_FULLDEBUG, *job);
			}
		}
		return false;
	}

	// If we got this far, the job is eligible to run
	return true;
}

shadow_rec*
Scheduler::StartJob(match_rec* mrec, PROC_ID* job_id)
{
	int		universe = -1;
	int		rval;

	rval = GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE, 
							&universe);
	if (rval < 0) {
		dprintf(D_ALWAYS, "Couldn't find %s Attribute for job "
				"(%d.%d) assuming standard.\n",	ATTR_JOB_UNIVERSE,
				job_id->cluster, job_id->proc);
	}
	return start_std( mrec, job_id, universe );
}


//-----------------------------------------------------------------
// Start Job Handler
//-----------------------------------------------------------------

void
Scheduler::StartJobHandler( int /* timerID */ )
{
	shadow_rec* srec;
	PROC_ID* job_id=NULL;
	int cluster, proc;
	int status;
	ClassAd *job_ad = NULL;

		// clear out our timer id since the hander just went off
	StartJobTimer = -1;

		// if we're trying to shutdown, don't start any new jobs!
	if( ExitWhenDone ) {
		return;
	}

	// get job from runnable job queue
	while( 1 ) {	
		if( RunnableJobQueue.empty() ) {
				// our queue is empty, we're done.
			return;
		}
		srec = RunnableJobQueue.front();
		RunnableJobQueue.pop();

		// Check to see if job ad is still around; it may have been
		// removed while we were waiting in RunnableJobQueue
		job_id=&srec->job_id;
		cluster = job_id->cluster;
		proc = job_id->proc;
		job_ad = GetJobAd(cluster, proc);
		if( !isStillRunnable(cluster, proc, status) ||
			(job_ad && srec->is_reconnect && !jobLeaseIsValid(job_ad, cluster, proc)) ) {
			if( status != -1 ) {  
					/*
					  the job still exists, it's just been removed or
					  held.  NOTE: it's ok to call mark_job_stopped(),
					  since we want to clear out ATTR_CURRENT_HOSTS,
					  the shadow birthday, etc.  mark_job_stopped()
					  won't touch ATTR_JOB_STATUS unless it's
					  currently "RUNNING", so we won't clobber it...
					*/
				mark_job_stopped( job_id );
			}
				/*
				  no matter what, if we're not able to start this job,
				  we need to delete the shadow record, remove it from
				  whatever match record it's associated with, and try
				  the next job.
				*/
			delete_shadow_rec(srec);
			continue;
		}

			// if we got this far, we're definitely starting the job,
			// so deal with the aboutToSpawnJobHandler hook...
		int universe = srec->universe;
		callAboutToSpawnJobHandler( cluster, proc, srec );

		bool wantPS = 0;
		job_ad->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING, wantPS);

		if( (universe == CONDOR_UNIVERSE_MPI) || 
			(universe == CONDOR_UNIVERSE_PARALLEL) || wantPS ) {
			
			if (proc != 0) {
				dprintf( D_ALWAYS, "StartJobHandler called for MPI or Parallel job, with "
					   "non-zero procid for job (%d.%d)\n", cluster, proc);

				continue;
			}
			
				// We've just called callAboutToSpawnJobHandler on procid 0,
				// now call it on the rest of them
			proc = 1;
			while( GetJobAd(cluster, proc) ) {
				callAboutToSpawnJobHandler( cluster, proc, srec);
				proc++;
			}
		}

			// we're done trying to spawn a job at this time.  call
			// tryNextJob() to let our timer logic handle the rest.
		tryNextJob();
		return;
	}
}

bool VanillaMatchAd::Insert(const std::string &attr, ClassAd*ad)
{
	return static_cast<classad::ClassAd*>(this)->Insert(attr, ad);
}

bool VanillaMatchAd::EvalExpr(ExprTree *expr, classad::Value &val)
{
	classad::EvalState state;
	state.SetScopes(this);
	return expr->Evaluate(state , val);
}

bool VanillaMatchAd::EvalAsBool(ExprTree *expr, bool def_value)
{
	bool bval = def_value;
	classad::Value val;
	if (EvalExpr(expr, val) && val.IsBooleanValueEquiv(bval)) {
		return bval;
	}
	return def_value;
}


void VanillaMatchAd::Init(ClassAd* slot_ad, const OwnerInfo* powni, JobQueueJob * job)
{
	// Insert the slot ad, making sure that the old slot ad is removed (i.e. not deleted)
	std::string slot_attr("SLOT");
	this->Remove(slot_attr);
	if (slot_ad) {
		this->Insert(slot_attr, slot_ad);
	}

	std::string owner_attr("OWNER");
	this->Remove(owner_attr);
	if (powni) {
		owner_ad.Assign("name", powni->Name());
		owner_ad.Assign("JobsRunning", powni->live.JobsRunning + powni->live.JobsSuspended);
		owner_ad.Assign("JobsHeld", powni->live.JobsHeld);
		owner_ad.Assign("JobsIdle", powni->live.JobsIdle);
		this->Insert(owner_attr, &owner_ad);
	}

	std::string job_attr("JOB");
	this->Remove(job_attr);
	if (job) { this->Insert(job_attr, job); }
}

void VanillaMatchAd::Reset()
{
	std::string slot_attr("SLOT");
	this->Remove(slot_attr);

	std::string owner_attr("OWNER");
	this->Remove(owner_attr);

	std::string job_attr("JOB");
	this->Remove(job_attr);
}

// convert the vanilla start expression to a sub-expression that references the SLOT ad
// references to the USER ad or JOB ad will be converted to constants, and then the constants will be folded
//
ExprTree * Scheduler::flattenVanillaStartExpr(JobQueueJob * job, const OwnerInfo * powni)
{
	ExprTree * expr = vanilla_start_expr.Expr();
	if ( ! expr) return NULL;

	classad::Value flat_val;
	ExprTree * flat_expr = NULL;

	VanillaMatchAd vad;
	vad.Init(NULL, powni, job);
	if (vad.FlattenAndInline(expr, flat_val, flat_expr)) {
		vad.Reset();

		if ( ! flat_expr) {
			flat_expr = classad::Literal::MakeLiteral(flat_val);
		} else {
			/* caller does this now.
			NOCASE_STRING_MAP mapping;
			mapping["SLOT"] = "TARGET";
			mapping["JOB"] = "MY";
			RewriteAttrRefs(flat_expr, mapping);
			*/
		}
	} else {
		flat_expr = expr->Copy(); // flattening failed, use the unflat expression.
	}

	return flat_expr;
}

// Check START_VANILLA_UNIVERSE of a job vs a SLOT.  This is called after the negotiator
// has given us a match, but before we try and activate it.
//
bool Scheduler::jobCanUseMatch(JobQueueJob * job, ClassAd * slot_ad, const std::string &pool, const char *&reason)
{
	if ( ! job) {
		reason = "job not found";
		return false;
	}

	bool runnable = true;
	reason = "no START_VANILLA expression";
	ExprTree * expr = vanilla_start_expr.Expr();
	if (expr) {
		VanillaMatchAd vad;

		const OwnerInfo * powni = NULL;
		GetJobInfo(job, powni);

		vad.Init(slot_ad, powni, job);

		runnable = vad.EvalAsBool(expr, false);

		vad.Reset();
		reason = "from eval of START_VANILLA";
	}
	if (!JobCanFlock(*job, pool)) {
		reason = "job cannot flock with pool:";
		runnable = false;
	}

	return runnable;
}

// Check START_VANILLA_UNIVERSE of a job for all possible slots. This is called when the schedd
// is building up the match list
//
bool Scheduler::jobCanNegotiate(JobQueueJob * job, const char *&reason)
{
	// TODO: move this into the ScheddNegotiate so we can avoid having to setup the VanillaMatchAd repeatedly
	if ( ! job) {
		reason = "job not found";
		return false;
	}

	bool runnable = true;
	ExprTree * expr = vanilla_start_expr.Expr();
	if (expr) {
		VanillaMatchAd vad;

		const OwnerInfo * powni = NULL;
		GetJobInfo(job, powni);

		vad.Init(NULL, powni, job);

		classad::Value val;
		if (vad.EvalExpr(expr, val)) {
			if ( ! val.IsBooleanValueEquiv(runnable)) {
				runnable = true;
				reason = "indeterminate";
			} else {
				reason = runnable ? "matches everything" : "matches nothing";
			}
		}
		vad.Reset();
	}

	return runnable;
}

// check to see if the START_VANILLA_UNIVERSE expression evaluates to a constant bool
// against the current (possibly incomplete) vanilla match ad. We use this to
// early out in FindRunnableJob
bool Scheduler::vanillaStartExprIsConst(VanillaMatchAd &vad, bool &bval)
{
	int   is_const = true;
	bool  const_value = true;

	// if we haven't yet checked to see if the vanilla start expresssion evaluates to a constant, do that now.
	ExprTree * expr = vanilla_start_expr.Expr();
	if (expr) {
		is_const = false;
		classad::Value val;
		val.SetUndefinedValue();
		if (vad.EvalExpr(expr, val)) {
			is_const = val.IsBooleanValueEquiv(const_value);
		}
	}
	bval = const_value;
	return is_const;
}

bool Scheduler::evalVanillaStartExpr(VanillaMatchAd &vad)
{
	ExprTree * expr = vanilla_start_expr.Expr();
	if (expr) {
		return vad.EvalAsBool(expr, false);
	}
	return true;
}


bool
Scheduler::isStillRunnable( int cluster, int proc, int &status )
{
	ClassAd* job = GetJobAd( cluster, proc );
	if( ! job ) {
			// job ad disappeared, definitely not still runnable.
		dprintf( D_FULLDEBUG,
				 "Job %d.%d was deleted while waiting to start\n",
				 cluster, proc );
			// let our caller know the job is totally gone
		status = -1;
		return false;
	}

	if( job->LookupInteger(ATTR_JOB_STATUS, status) == 0 ) {
		EXCEPT( "Job %d.%d has no %s while waiting to start!",
				cluster, proc, ATTR_JOB_STATUS );
	}
	switch( status ) {
	case IDLE:
	case RUNNING:
	case SUSPENDED:
	case TRANSFERRING_OUTPUT:
			// these are the cases we expect.  if it's local
			// universe, it'll still be IDLE.  if it's not local,
			// it'll already be marked as RUNNING...  just break
			// out of the switch and carry on.
		return true;
		break;

	case REMOVED:
	case HELD:
	case JOB_STATUS_BLOCKED:
	case COMPLETED:
	case JOB_STATUS_FAILED:
		dprintf( D_FULLDEBUG,
				 "Job %d.%d was %s while waiting to start\n",
				 cluster, proc, getJobStatusString(status) );
		return false;
		break;

	default:
		EXCEPT( "StartJobHandler: Unknown status (%d) for job %d.%d",
				status, cluster, proc ); 
		break;
	}
	return false;
}


void
Scheduler::spawnShadow( shadow_rec* srec )
{
	//-------------------------------
	// Actually fork the shadow
	//-------------------------------

	bool	rval;
	ArgList args;

	match_rec* mrec = srec->match;
	int universe = srec->universe;
	PROC_ID* job_id = &srec->job_id;

	char* 	shadow_path = NULL;
	bool wants_reconnect = srec->is_reconnect;

	shadow_path = param("SHADOW");

	args.AppendArg("condor_shadow");

	std::string argbuf;

	formatstr(argbuf,"%d.%d", job_id->cluster, job_id->proc);
	args.AppendArg(argbuf);

	if(wants_reconnect) {
		args.AppendArg("--reconnect");
	}

	// pass the public ip/port of the schedd (used w/ reconnect)
	// We need this even if we are not currently in reconnect mode,
	// because the shadow may go into reconnect mode at any time.
	formatstr(argbuf, "--schedd=%s", daemonCore->publicNetworkIpAddr());
	args.AppendArg(argbuf);

	if( m_have_xfer_queue_contact ) {
		formatstr(argbuf, "--xfer-queue=%s", m_xfer_queue_contact.c_str());
		args.AppendArg(argbuf);
	}

		// pass the private socket ip/port for use just by shadows
	args.AppendArg(MyShadowSockName);

	args.AppendArg("-");

	bool want_udp = true;
#ifndef WIN32
		// To save memory in the shadow, do not create a UDP command
		// socket under unix.  Windows doesn't _need_ UDP either,
		// because signals can be delivered via TCP, but the
		// performance impact of doing that has not been measured.
		// Under unix, all common signals are delivered via unix
		// signals.

	want_udp = false;
#endif

	rval = spawnJobHandlerRaw( srec, shadow_path, args, NULL, "shadow",
							   want_udp );

	free( shadow_path );

	if( ! rval ) {
		mark_job_stopped(job_id);
		delete_shadow_rec(srec);
		return;
	}

	dprintf( D_ALWAYS, "Started shadow for job %d.%d on %s, "
			 "(shadow pid = %d)\n", job_id->cluster, job_id->proc,
			 mrec->description(), srec->pid );

    //time_t now = time(NULL);
    time_t now = stats.Tick();
    stats.ShadowsStarted += 1;
    stats.ShadowsRunning = numShadows;

	OtherPoolStats.Tick(now);

		// If this is a reconnect shadow, update the mrec with some
		// important info.  This usually happens in StartJobs(), but
		// in the case of reconnect, we don't go through that code. 
	if( wants_reconnect ) {
			// Now that the shadow is alive, the match is "ACTIVE"
		mrec->setStatus( M_ACTIVE );
		mrec->cluster = job_id->cluster;
		mrec->proc = job_id->proc;
		dprintf(D_FULLDEBUG, "Match (%s) - running %d.%d\n",
		        mrec->description(), mrec->cluster, mrec->proc );

		/*
		  If we just spawned a reconnect shadow, we want to update
		  ATTR_LAST_JOB_LEASE_RENEWAL in the job ad.  This normally
		  gets done inside add_shadow_rec(), but we don't want that
		  behavior for reconnect shadows or we clobber the valuable
		  info that was left in the job queue.  So, we do it here, now
		  that we already wrote out the job ClassAd to the shadow's
		  pipe.
		*/
		SetAttributeInt( job_id->cluster, job_id->proc, 
						 ATTR_LAST_JOB_LEASE_RENEWAL, time(0) );
	}

		// if this is a shadow for an MPI job, we need to tell the
		// dedicated scheduler we finally spawned it so it can update
		// some of its own data structures, too.
	bool sendToDS = false;
	GetAttributeBool(job_id->cluster, job_id->proc, ATTR_WANT_PARALLEL_SCHEDULING, &sendToDS);

	if( (sendToDS || universe == CONDOR_UNIVERSE_MPI ) ||
	    (universe == CONDOR_UNIVERSE_PARALLEL) ){
		dedicated_scheduler.shadowSpawned( srec );
	}
}


void
Scheduler::setNextJobDelay( ClassAd *job_ad, ClassAd *machine_ad ) {
	int delay = 0;
	ASSERT( job_ad );

	int cluster,proc;
	job_ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
	job_ad->LookupInteger(ATTR_PROC_ID, proc);

	EvalInteger(ATTR_NEXT_JOB_START_DELAY,job_ad,machine_ad,delay);
	if( MaxNextJobDelay && delay > MaxNextJobDelay ) {
		dprintf(D_ALWAYS,
				"Job %d.%d has %s = %d, which is greater than "
				"MAX_NEXT_JOB_START_DELAY=%d\n",
				cluster,
				proc,
				ATTR_NEXT_JOB_START_DELAY,
				delay,
				MaxNextJobDelay);

		delay = MaxNextJobDelay;
	}
	if( delay > jobThrottleNextJobDelay ) {
		jobThrottleNextJobDelay = delay;
		dprintf(D_FULLDEBUG,"Job %d.%d setting next job delay to %ds\n",
				cluster,
				proc,
				delay);
	}
}

void
Scheduler::tryNextJob()
{
		// Re-set timer if there are any jobs left in the queue
	if( !RunnableJobQueue.empty() ) {
		StartJobTimer = daemonCore->
		// Queue the next job start via the daemoncore timer.  jobThrottle()
		// implements job bursting, and returns the proper delay for the timer.
			Register_Timer( jobThrottle(),
							(TimerHandlercpp)&Scheduler::StartJobHandler,
							"start_job", this ); 
	} else {
		ExpediteStartJobs();
	}
}


bool
Scheduler::spawnJobHandlerRaw( shadow_rec* srec, const char* path, 
							   ArgList const &args, Env const *env, 
							   const char* name, bool want_udp)
{
	int pid = -1;
	PROC_ID* job_id = &srec->job_id;
	ClassAd* job_ad = NULL;
	int create_process_opts = 0;

	if (!want_udp) {
		create_process_opts |= DCJOBOPT_NO_UDP;
	}

	Env extra_env;
	if( ! env ) {
		extra_env.Import(); // copy schedd's environment
	}
	else {
		std::string msg;
		extra_env.MergeFrom(*env);
	}
	env = &extra_env;

	// Now add USERID_MAP to the environment so the child process does
	// not have to look up stuff we already know.  In some
	// environments (e.g. where NSS is used), we have observed cases
	// where about half of the shadow's private memory was consumed by
	// junk allocated inside of the first call to getpwuid(), so this
	// is worth optimizing.  This may also reduce load on the ldap
	// server.

#ifndef WIN32
	passwd_cache *p = pcache();
	if( p ) {
		std::string usermap;
		p->getUseridMap(usermap);
		if( !usermap.empty() ) {
			extra_env.SetEnv("_condor_USERID_MAP",usermap.c_str());
		}
	}
#endif

		/* Setup the array of fds for stdin, stdout, stderr */
	int* std_fds_p = NULL;
	int std_fds[3];
	int pipe_fds[2];
	pipe_fds[0] = -1;
	pipe_fds[1] = -1;
	if( ! daemonCore->Create_Pipe(pipe_fds) ) {
		dprintf( D_ALWAYS, 
				 "ERROR: Can't create DC pipe for writing job "
				 "ClassAd to the %s, aborting\n", name );
		return false;
	} 
		// pipe_fds[0] is the read-end of the pipe.  we want that
		// setup as STDIN for the handler.  we'll hold onto the
		// write end of it so we can write the job ad there.
	std_fds[0] = pipe_fds[0];
	std_fds[1] = -1;
	std_fds[2] = -1;
	std_fds_p = std_fds;

        /* Get the handler's nice increment.  For now, we just use the
		   same config attribute for all handlers. */
    int niceness = param_integer( "SHADOW_RENICE_INCREMENT",0 );

	int rid;
	if( ( srec->universe == CONDOR_UNIVERSE_MPI) ||
		( srec->universe == CONDOR_UNIVERSE_PARALLEL)) {
		rid = dedicated_scheduler.rid;
	} else {
		rid = shadowReaperId;
	}
	

		/*
		  now, add our shadow record to our various tables.  we don't
		  yet know the pid, but that's the only thing we're missing.
		  otherwise, we've already got our match ad and all that.
		  this allows us to call GetJobAd() once (and expand the $$()
		  stuff, which is what the final argument of "true" does)
		  before we actually try to spawn the shadow.  if there's a
		  failure, we can bail out without actually having spawned the
		  shadow, but everything else will still work.
		  NOTE: ONLY when GetJobAd() is called with expStartdAd==true
		  do we want to delete the result...
		*/

	srec->pid = 0; 
	add_shadow_rec( srec );
    time_t now = stats.Tick();
    stats.ShadowsRunning = numShadows;

	OtherPoolStats.Tick(now);

		// expand $$ stuff and persist expansions so they can be
		// retrieved on restart for reconnect
	job_ad = GetExpandedJobAd( *job_id, true );
	if( ! job_ad ) {
			// this might happen if the job is asking for
			// something in $$() that doesn't exist in the machine
			// ad and/or if the machine ad is already gone for some
			// reason.  so, verify the job is still here...
		if( ! GetJobAd(*job_id) ) {
			EXCEPT( "Impossible: GetJobAd() returned NULL for %d.%d " 
					"but that job is already known to exist",
					job_id->cluster, job_id->proc );
		}

			// the job is still there, it just failed b/c of $$()
			// woes... abort.
		dprintf( D_ALWAYS, "ERROR: Failed to get classad for job "
				 "%d.%d, can't spawn %s, aborting\n", 
				 job_id->cluster, job_id->proc, name );
		for( int i = 0; i < 2; i++ ) {
			if( pipe_fds[i] >= 0 ) {
				daemonCore->Close_Pipe( pipe_fds[i] );
			}
		}
			// our caller will deal with cleaning up the srec
			// as appropriate...  
		return false;
	}
	std::string secret;
	if (GetPrivateAttributeString(job_id->cluster, job_id->proc, ATTR_CLAIM_ID, secret) == 0) {
		job_ad->Assign(ATTR_CLAIM_ID, secret);
	}
	if (GetPrivateAttributeString(job_id->cluster, job_id->proc, ATTR_CLAIM_IDS, secret) == 0) {
		job_ad->Assign(ATTR_CLAIM_IDS, secret);
	}

	FamilyInfo fi;
	FamilyInfo *fip = NULL;

	if (IsLocalUniverse(srec)) {
		fip = &fi;
		fi.max_snapshot_interval = 15;
	}
	
	/* For now, we should create the handler as PRIV_ROOT so it can do
	   priv switching between PRIV_USER (for handling syscalls, moving
	   files, etc), and PRIV_CONDOR (for writing to log files).
	   Someday, hopefully soon, we'll fix this and spawn the
	   shadow/handler with PRIV_USER_FINAL... */
	std::string daemon_sock = SharedPortEndpoint::GenerateEndpointName(name);
	pid = daemonCore->Create_Process( path, args, PRIV_ROOT, rid, 
	                                  true, true, env, NULL, fip, NULL, 
	                                  std_fds_p, NULL, niceness,
									  NULL, create_process_opts,
									  NULL, NULL, daemon_sock.c_str());
	if( pid == FALSE ) {
		std::string arg_string;
		args.GetArgsStringForDisplay(arg_string);
		dprintf( D_ERROR, "spawnJobHandlerRaw: "
				 "CreateProcess(%s, %s) failed\n", path, arg_string.c_str() );
		for( int i = 0; i < 2; i++ ) {
			if( pipe_fds[i] >= 0 ) {
				daemonCore->Close_Pipe( pipe_fds[i] );
			}
		}
		if( job_ad ) {
			delete job_ad;
			job_ad = NULL;
		}
			// again, the caller will deal w/ cleaning up the srec
		return false;
	} 

		// if it worked, store the pid in our shadow record, and add
		// this srec to our table of srec's by pid.
	srec->pid = pid;
	add_shadow_rec_pid( srec );

		// finally, now that the handler has been spawned, we need to
		// do some things with the pipe:
		// 1) close our copy of the read end of the pipe, so we
		// don't leak it.  we have to use DC::Close_Pipe() for
		// this, not just close(), so things work on windoze.
	daemonCore->Close_Pipe( pipe_fds[0] );

		// 2) dump out the job ad to the write end, since the
		// handler is now alive and can read from the pipe.
	ASSERT( job_ad );
	std::string ad_str;
	sPrintAdWithSecrets(ad_str, *job_ad);
	const char* ptr = ad_str.c_str();
	int len = ad_str.length();
	while (len) {
		int bytes_written = daemonCore->Write_Pipe(pipe_fds[1], ptr, len);
		if (bytes_written == -1) {
			dprintf(D_ALWAYS, "writeJobAd: Write_Pipe failed\n");
			break;
		}
		ptr += bytes_written;
		len -= bytes_written;
	}

		// TODO: if this is an MPI job, we should really write all
		// the match info (ClaimIds, sinful strings and machine
		// ads) to the pipe before we close it, but that's just a
		// performance optimization, not a correctness issue.

		// Now that all the data is written to the pipe, we can
		// safely close the other end, too.  
	daemonCore->Close_Pipe(pipe_fds[1]);

	{
		ClassAd *machine_ad = NULL;
		if(srec->match ) {
			machine_ad = srec->match->my_match_ad;
		}
		setNextJobDelay( job_ad, machine_ad );
	}

	if( job_ad ) {
		delete job_ad;
		job_ad = NULL;
	}
	return true;
}


shadow_rec*
Scheduler::start_std( match_rec* mrec , PROC_ID* job_id, int univ )
{

	dprintf( D_FULLDEBUG, "Scheduler::start_std - job=%d.%d on %s\n",
			job_id->cluster, job_id->proc, mrec->peer );

	mark_serial_job_running(job_id);

	// add job to run queue
	shadow_rec* srec=add_shadow_rec( 0, job_id, univ, mrec, -1 );
	addRunnableJob( srec );
	return srec;
}

void
Scheduler::addRunnableJob( shadow_rec* srec )
{
	if( ! srec ) {
		EXCEPT( "Scheduler::addRunnableJob called with NULL srec!" );
	}

	dprintf( D_FULLDEBUG, "Queueing job %d.%d in runnable job queue\n",
			 srec->job_id.cluster, srec->job_id.proc );

	RunnableJobQueue.push(srec);

	if( StartJobTimer<0 ) {
		// Queue the next job start via the daemoncore timer.
		// jobThrottle() implements job bursting, and returns the
		// proper delay for the timer.
		StartJobTimer = daemonCore->
			Register_Timer( jobThrottle(), 
							(TimerHandlercpp)&Scheduler::StartJobHandler,
							"StartJobHandler", this );
	}
}


void
Scheduler::spawnLocalStarter( shadow_rec* srec )
{
	static bool notify_admin = true;
	PROC_ID* job_id = &srec->job_id;
	char* starter_path;
	ArgList starter_args;
	bool rval;

	dprintf( D_FULLDEBUG, "Starting local universe job %d.%d\n",
			 job_id->cluster, job_id->proc );

		// Someday, we'll probably want to use the shadow_list, a
		// shadow object, etc, etc.  For now, we're just going to keep
		// things a little more simple in the first pass.
	starter_path = param( "STARTER_LOCAL" );
	if( ! starter_path ) {
		dprintf( D_ALWAYS, "Can't start local universe job %d.%d: "
				 "STARTER_LOCAL not defined!\n", job_id->cluster,
				 job_id->proc );
		holdJob( job_id->cluster, job_id->proc,
				 "No condor_starter installed that supports local universe",
				 CONDOR_HOLD_CODE::NoCompatibleShadow, 0, false,
				 false, notify_admin, true );
		delete_shadow_rec( srec );
		notify_admin = false;
		return;
	}

	starter_args.AppendArg("condor_starter");
	starter_args.AppendArg("-f");

	starter_args.AppendArg("-job-cluster");
	starter_args.AppendArg(std::to_string(job_id->cluster));

	starter_args.AppendArg("-job-proc");
	starter_args.AppendArg(std::to_string(job_id->proc));

	starter_args.AppendArg("-header");
	std::string header;
	formatstr(header,"(%d.%d) ",job_id->cluster,job_id->proc);
	starter_args.AppendArg(header);

	starter_args.AppendArg("-job-input-ad");
	starter_args.AppendArg("-");
	starter_args.AppendArg("-schedd-addr");
	starter_args.AppendArg(MyShadowSockName);

	if(IsFulldebug(D_FULLDEBUG)) {
		std::string argstring;
		starter_args.GetArgsStringForDisplay(argstring);
		dprintf( D_FULLDEBUG, "About to spawn %s %s\n", 
				 starter_path, argstring.c_str() );
	}

	mark_serial_job_running( job_id );

	BeginTransaction();
		// add CLAIM_ID to this job ad so schedd can be authorized by
		// starter by virtue of this shared secret (e.g. for
		// CREATE_JOB_OWNER_SEC_SESSION
	char *public_part = Condor_Crypt_Base::randomHexKey();
	char *private_part = Condor_Crypt_Base::randomHexKey();
	ClaimIdParser cidp(public_part,NULL,private_part);
	SetPrivateAttributeString( job_id->cluster, job_id->proc, ATTR_CLAIM_ID, cidp.claimId() );
	free( public_part );
	free( private_part );

	CommitNonDurableTransactionOrDieTrying();

	Env starter_env;
	starter_env.SetEnv("_condor_EXECUTE",LocalUnivExecuteDir);
	
	rval = spawnJobHandlerRaw( srec, starter_path, starter_args,
							   &starter_env, "starter", true );

	free( starter_path );
	starter_path = NULL;

	if( ! rval ) {
		dprintf( D_ERROR, "Can't spawn local starter for "
				 "job %d.%d\n", job_id->cluster, job_id->proc );
		mark_job_stopped( job_id );
		delete_shadow_rec( srec );
		return;
	}

	dprintf( D_ALWAYS, "Spawned local starter (pid %d) for job %d.%d\n",
			 srec->pid, job_id->cluster, job_id->proc );
}



void
Scheduler::initLocalStarterDir( void )
{
	static bool first_time = true;
	mode_t mode;
#ifdef WIN32
	mode_t desired_mode = _S_IREAD | _S_IWRITE;
#else
		// We want execute to be world-writable w/ the sticky bit set.  
	mode_t desired_mode = (0777 | S_ISVTX);
#endif

	std::string dir_name;
	char* tmp = param( "LOCAL_UNIV_EXECUTE" );
	if( ! tmp ) {
		tmp = param( "SPOOL" );		
		if( ! tmp ) {
			EXCEPT( "SPOOL directory not defined in config file!" );
		}
			// If you change this default, make sure you change
			// condor_preen, too, so that it doesn't nuke your
			// directory (assuming you still use SPOOL).
		formatstr( dir_name, "%s%c%s", tmp, DIR_DELIM_CHAR,
						  "local_univ_execute" );
	} else {
		dir_name = tmp;
	}
	free( tmp );
	tmp = NULL;
	if( LocalUnivExecuteDir ) {
		free( LocalUnivExecuteDir );
	}
	LocalUnivExecuteDir = strdup( dir_name.c_str() );

	StatInfo exec_statinfo( dir_name.c_str() );
	if( ! exec_statinfo.IsDirectory() ) {
			// our umask is going to mess this up for us, so we might
			// as well just do the chmod() seperately, anyway, to
			// ensure we've got it right.  the extra cost is minimal,
			// since we only do this once...
		dprintf( D_FULLDEBUG, "initLocalStarterDir(): %s does not exist, "
				 "calling mkdir()\n", dir_name.c_str() );
		if( mkdir(dir_name.c_str(), 0777) < 0 ) {
			dprintf( D_ALWAYS, "initLocalStarterDir(): mkdir(%s) failed: "
					 "%s (errno %d)\n", dir_name.c_str(), strerror(errno),
					 errno );
				// TODO: retry as priv root or something?  deal w/ NFS
				// and root squashing, etc...
			return;
		}
		mode = 0777;
	} else {
		mode = exec_statinfo.GetMode();
		if( first_time ) {
				// if this is the startup-case (not reconfig), and the
				// directory already exists, we want to attempt to
				// remove everything in it, to clean up any droppings
				// left by starters that died prematurely, etc.
			dprintf( D_FULLDEBUG, "initLocalStarterDir: "
					 "%s already exists, deleting old contents\n",
					 dir_name.c_str() );
			Directory exec_dir( &exec_statinfo, PRIV_CONDOR );
			exec_dir.Remove_Entire_Directory();
			first_time = false;
		}
	}

		// we know the directory exists, now make sure the mode is
		// right for our needs...
	if( (mode & desired_mode) != desired_mode ) {
		dprintf( D_FULLDEBUG, "initLocalStarterDir(): "
				 "Changing permission on %s\n", dir_name.c_str() );
		if( chmod(dir_name.c_str(), (mode|desired_mode)) < 0 ) {
			dprintf( D_ALWAYS, 
					 "initLocalStarterDir(): chmod(%s) failed: "
					 "%s (errno %d)\n", dir_name.c_str(), 
					 strerror(errno), errno );
		}
	}
}


shadow_rec*
Scheduler::start_sched_universe_job(PROC_ID* job_id)
{

	std::string a_out_name;
	std::string input;
	std::string output;
	std::string error;
	std::string x509_proxy;
	ArgList args;
	std::string argbuf;
	std::string error_msg;
	std::string owner, iwd;
	std::string domain;
	int		pid;
	StatInfo* filestat;
	bool is_executable;
	shadow_rec *retval = NULL;
	Env envobject;
	std::string env_error_msg;
    int niceness = 0;
	std::string tmpCwd;
	int inouterr[3] = {-1, -1, -1};
	bool cannot_open_files = false;
	priv_state priv;
	int i;
	int core_size_truncated = 0;
	size_t core_size = 0;
	size_t *core_size_ptr = NULL;
	char *ckpt_name = NULL;
	// This is the temporary directory we create for the job, but it
	// is not where we run the job. We put the .job.ad file here.
	std::string job_execute_dir;
	std::string job_ad_path;
	bool wrote_job_ad = false;
	bool directory_exists = false;
	FamilyInfo fi;

	fi.max_snapshot_interval = 15;

	is_executable = false;


	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
		job_id->cluster, job_id->proc );

	JobQueueJob * userJob = GetJobAd(job_id->cluster,job_id->proc);
	ASSERT(userJob);

	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_IWD,
		iwd) < 0) {
#ifndef WIN32
		iwd = "/tmp";
#else
		// try to get the temp dir, otherwise just use the root directory
		char* tempdir = getenv("TEMP");
		iwd = ((tempdir) ? tempdir : "\\");
#endif
	}

	// who is this job going to run as...
	if (GetAttributeString(job_id->cluster, job_id->proc, 
		ATTR_OWNER, owner) < 0) {
		dprintf(D_FULLDEBUG, "Scheduler::start_sched_universe_job"
			"--setting owner to \"nobody\"\n" );
		owner = "nobody";
	}
    std::string create_process_err_msg;
	OptionalCreateProcessArgs cpArgs(create_process_err_msg);

	// get the nt domain too, if we have it
	GetAttributeString(job_id->cluster, job_id->proc, ATTR_NT_DOMAIN, domain);

	// sanity check to make sure this job isn't going to start as root.
	if (strcasecmp(owner.c_str(), "root") == 0 ) {
		dprintf(D_ALWAYS, "Aborting job %d.%d.  Tried to start as root.\n",
			job_id->cluster, job_id->proc);
		goto wrapup;
	}

	// switch to the user in question to make some checks about what I'm 
	// about to execute and then to execute.

	if (! init_user_ids(owner.c_str(), domain.c_str()) ) {
		if ( ! domain.empty()) { owner += "@"; owner += domain; }
		std::string tmpstr;
#ifdef WIN32
		formatstr(tmpstr, "Bad or missing credential for user: %s", owner.c_str());
#else
		formatstr(tmpstr, "Unable to switch to user: %s", owner.c_str());
#endif
		holdJob(job_id->cluster, job_id->proc, tmpstr.c_str(),
				CONDOR_HOLD_CODE::FailedToAccessUserAccount, 0,
				false, true);
		goto wrapup;
	}

	priv = set_user_priv(); // need user's privs...

	// Here we are going to look into the spool directory which contains the
	// user's executables as the user. Be aware that even though the spooled
	// executable probably is owned by Condor in most circumstances, we
	// must ensure the user can at least execute it.

	ckpt_name = GetSpooledExecutablePath(job_id->cluster, Spool);
	a_out_name = ckpt_name;
	free(ckpt_name); ckpt_name = NULL;
	errno = 0;
	filestat = new StatInfo(a_out_name.c_str());
	ASSERT(filestat);

	if (filestat->Error() == SIGood) {
		is_executable = filestat->IsExecutable();

		if (!is_executable) {
			// The file is present, but the user cannot execute it? Put the job
			// on hold.
			set_priv( priv );  // back to regular privs...

			holdJob(job_id->cluster, job_id->proc, 
				"Spooled executable is not executable!",
					CONDOR_HOLD_CODE::FailedToCreateProcess, EACCES,
				false, true);

			delete filestat;
			filestat = NULL;

			goto wrapup;
		}
	}

	delete filestat;
	filestat = NULL;

	if ( !is_executable ) {
		// If we have determined that the executable is not present in the
		// spool, then it must be in the user's initialdir, or wherever they 
		// specified in the classad. Either way, it must be executable by them.

		// Sanity check the classad to ensure we have an executable.
		a_out_name = "";
		userJob->LookupString(ATTR_JOB_CMD,a_out_name);
		if (a_out_name.length()==0) {
			set_priv( priv );  // back to regular privs...
			holdJob(job_id->cluster, job_id->proc, 
				"Executable unknown - not specified in job ad!",
					CONDOR_HOLD_CODE::FailedToCreateProcess, ENOENT,
				false, true);
			goto wrapup;
		}

		// If the executable filename isn't an absolute path, prepend
		// the IWD.
		if ( !fullpath( a_out_name.c_str() ) ) {
			std::string tmp = a_out_name;
			formatstr( a_out_name, "%s%c%s", iwd.c_str(), DIR_DELIM_CHAR, tmp.c_str() );
		}
		
		// Now check, as the user, if we may execute it.
		filestat = new StatInfo(a_out_name.c_str());
		is_executable = false;
		if ( filestat ) {
			is_executable = filestat->IsExecutable();
			delete filestat;
		}
		if ( !is_executable ) {
			std::string tmpstr;
			formatstr( tmpstr, "File '%s' is missing or not executable", a_out_name.c_str() );
			set_priv( priv );  // back to regular privs...
			holdJob(job_id->cluster, job_id->proc, tmpstr.c_str(),
					CONDOR_HOLD_CODE::FailedToCreateProcess, EACCES,
					false, true);
			goto wrapup;
		}
	}
	
	
	// Get std(in|out|err)
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_INPUT,
		input) < 0) {
		input = NULL_FILE;
		
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_OUTPUT,
		output) < 0) {
		output = NULL_FILE;
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ERROR,
		error) < 0) {
		error = NULL_FILE;
	}
	
	//change to IWD before opening files, easier than prepending 
	//IWD if not absolute pathnames
	condor_getcwd(tmpCwd);
	if (chdir(iwd.c_str())) {
		dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n", iwd.c_str(), strerror(errno));
	}
	
	// now open future in|out|err files
	
#ifdef WIN32
	
	// submit gives us /dev/null regardless of the platform.
	// normally, the starter would handle this translation,
	// but since we're the schedd, we'll have to do it ourselves.
	// At least for now. --stolley
	
	if (nullFile(input.c_str())) {
		input = WINDOWS_NULL_FILE;
	}
	if (nullFile(output.c_str())) {
		output = WINDOWS_NULL_FILE;
	}
	if (nullFile(error.c_str())) {
		error = WINDOWS_NULL_FILE;
	}
	
#endif
	
	if ((inouterr[0] = safe_open_wrapper_follow(input.c_str(), O_RDONLY, S_IREAD)) < 0) {
		dprintf ( D_ERROR, "Open of %s failed, errno %d\n", input.c_str(), errno );
		cannot_open_files = true;
	}
	if ((inouterr[1] = safe_open_wrapper_follow(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_ERROR, "Open of %s failed, errno %d\n", output.c_str(), errno );
		cannot_open_files = true;
	}
	if ((inouterr[2] = safe_open_wrapper_follow(error.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_ERROR, "Open of %s failed, errno %d\n", error.c_str(), errno );
		cannot_open_files = true;
	}

	formatstr(job_execute_dir, "%s%cdir_%d_%d", LocalUnivExecuteDir, DIR_DELIM_CHAR, job_id->cluster, job_id->proc);
	{
		TemporaryPrivSentry tps(PRIV_CONDOR);
		if( mkdir(job_execute_dir.c_str(), 0755) < 0 ) {
			if (errno == EEXIST) {
				directory_exists = true;
			} else {
				dprintf( D_ERROR,
					"couldn't create dir %s: (%s, errno=%d).\n",
					job_execute_dir.c_str(),
					strerror(errno), errno );
			}
		} else {
			directory_exists = true;
		}

		if(directory_exists) {
			// construct the full path to the job ad file
			job_ad_path = job_execute_dir + DIR_DELIM_CHAR + ".job.ad";

			// write it
			FILE *job_ad_fp = NULL;
			if (!(job_ad_fp = safe_fopen_wrapper_follow(job_ad_path.c_str(), "w"))) {
				dprintf ( D_ERROR, "Open of %s failed (%s, errno=%d).\n", job_ad_path.c_str(), strerror(errno), errno );
			} else {
				// fPrindAd does not have any usable error reporting.
				fPrintAd(job_ad_fp, *userJob);
				wrote_job_ad = true;
				fclose(job_ad_fp);
			}
		}
	}

	//change back to whence we came
	if ( tmpCwd.length() ) {
		if (chdir(tmpCwd.c_str())) {
			dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n",
					tmpCwd.c_str(), strerror(errno));
		}
	}
	
	set_priv( priv );  // back to regular privs...
	if ( cannot_open_files ) {
		goto wrapup;
	}
	
	if(!envobject.MergeFrom(userJob,env_error_msg)) {
		dprintf(D_ALWAYS,"Failed to read job environment: %s\n",
				env_error_msg.c_str());
		goto wrapup;
	}
	
	// stick a CONDOR_ID environment variable in job's environment
	char condor_id_string[PROC_ID_STR_BUFLEN];
	ProcIdToStr(*job_id,condor_id_string);
	envobject.SetEnv("CONDOR_ID",condor_id_string);

	// Set X509_USER_PROXY in the job's environment if the job ad says
	// we have a proxy.
	if (GetAttributeString(job_id->cluster, job_id->proc, 
						   ATTR_X509_USER_PROXY, x509_proxy) == 0) {
		envobject.SetEnv("X509_USER_PROXY",x509_proxy);
	}

	// If the scheduler universe job ad has ATTR_JOB_SEND_CREDENTIAL=True, then
	// we assume a credential for this user has been stored in the credd when the
	// scheduler universe job was submitted.  So here we set config param
	// SEC_CRENDENTIAL_PRODUCER to the magic value CRENDENTIAL_ALREADY_STORED.
	// This will result in meta-scheduler like DAGMan not bother trying to restash
	// a credential every time they run condor_submit.
	{
		bool have_stored_credential = false;
		GetAttributeBool(job_id->cluster, job_id->proc,
						   ATTR_JOB_SEND_CREDENTIAL, &have_stored_credential);
		if (have_stored_credential) {
			envobject.SetEnv("_condor_SEC_CREDENTIAL_PRODUCER","CREDENTIAL_ALREADY_STORED");
		}
	}

	// Don't use a_out_name for argv[0], use
	// "condor_scheduniv_exec.cluster.proc" instead. 
	formatstr(argbuf,"condor_scheduniv_exec.%d.%d",job_id->cluster,job_id->proc);
	args.AppendArg(argbuf);

	if(!args.AppendArgsFromClassAd(userJob,error_msg)) {
		dprintf(D_ALWAYS,"Failed to read job arguments: %s\n",
				error_msg.c_str());
		goto wrapup;
	}

        // get the job's nice increment
	niceness = param_integer( "SCHED_UNIV_RENICE_INCREMENT",niceness );

		// If there is a requested coresize for this job,
		// enforce it.  It is truncated because you can't put an
		// unsigned integer into a classad. I could rewrite condor's
		// use of ATTR_CORE_SIZE to be a float, but then when that
		// attribute is read/written to the job queue log by/or
		// shared between versions of Condor which view the type
		// of that attribute differently, calamity would arise.

	if (GetAttributeInt(job_id->cluster, job_id->proc, 
						   ATTR_CORE_SIZE, &core_size_truncated) == 0) {
		// make the hard limit be what is specified.
		core_size = (size_t)core_size_truncated;
	} else {
		core_size = 0;
	}
	core_size_ptr = &core_size;

		// Update the environment to point at the job ad.
	if( wrote_job_ad && !envobject.SetEnv("_CONDOR_JOB_AD", job_ad_path.c_str()) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_JOB_AD environment variable\n");
	}

	if (param_boolean("USE_LOCAL_SCHEDD_FOR_SCHEDULER_UNIVERSE", true))
	{
		std::string ad_file;
		if (!param(ad_file, "SCHEDD_DAEMON_AD_FILE") || !envobject.SetEnv("_condor_SCHEDD_DAEMON_AD_FILE", ad_file)) {
			dprintf(D_ALWAYS, "Failed to set _condor_SCHEDD_DAEMON_AD_FILE environment variable\n");
		}
		if (!param(ad_file, "SCHEDD_ADDRESS_FILE") || !envobject.SetEnv("_condor_SCHEDD_ADDRESS_FILE", ad_file)) {
			dprintf(D_ALWAYS, "Failed to set _condor_SCHEDD_DAEMON_AD_FILE environment variable\n");
		}
	}

		// Scheduler universe jobs should not be told about the shadow
		// command socket in the inherit buffer.
	daemonCore->SetInheritParentSinful( NULL );

	pid = daemonCore->CreateProcessNew( a_out_name, args,
		 cpArgs.priv(PRIV_USER_FINAL)
		 .reaperID(shadowReaperId)
		 .wantCommandPort(false).wantUDPCommandPort(false)
		 .familyInfo(&fi)
		 .cwd(iwd.c_str())
		 .env(&envobject)
		 .std(inouterr)
		 .niceInc(niceness)
		 .jobOptMask(DCJOBOPT_NO_ENV_INHERIT)
		 .coreHardLimit(core_size_ptr)
		 );

	daemonCore->SetInheritParentSinful( MyShadowSockName );

	if ( pid <= 0 ) {
		dprintf ( D_ERROR, "Create_Process problems!\n" );
		goto wrapup;
	}
	
	dprintf ( D_ALWAYS, "Successfully created sched universe process\n" );
	mark_serial_job_running(job_id);
	WriteExecuteToUserLog( *job_id );

		/* this is somewhat evil.  these values are absolutely
		   essential to have accurate when we're trying to shutdown
		   (see Scheduler::preempt()).  however, we only set them
		   correctly inside count_jobs(), and there's no guarantee
		   we'll call that before we next try to shutdown.  so, we
		   manually update them here, to keep them accurate until the
		   next time we call count_jobs().
		   -derek <wright@cs.wisc.edu> 2005-04-01
		*/
	if( SchedUniverseJobsIdle > 0 ) {
		SchedUniverseJobsIdle--;
	}
	SchedUniverseJobsRunning++;

	// if we have a per-user scheduler jobs limit.  then also keep the
	//  per-user count of idle and running scheduler jobs up-to-date
	if (MaxRunningSchedulerJobsPerOwner >= 0 && MaxRunningSchedulerJobsPerOwner < MaxJobsPerOwner) {
		SubmitterData * subdat = NULL;
		OwnerInfo * owninfo = get_submitter_and_owner(userJob, subdat);
		if (owninfo) {
			if (owninfo->num.SchedulerJobsIdle > 0) {
				owninfo->num.SchedulerJobsIdle--;
			}
			owninfo->num.SchedulerJobsRunning++;
		}
		if (subdat) {
			if (subdat->num.SchedulerJobsIdle > 0) {
				subdat->num.SchedulerJobsIdle--;
			}
			subdat->num.SchedulerJobsRunning++;
		}
	}

	retval =  add_shadow_rec( pid, job_id, CONDOR_UNIVERSE_SCHEDULER, NULL, -1 );

wrapup:
	uninit_user_ids();
	if(userJob) {
		FreeJobAd(userJob);
	}
	// now close those open fds - we don't want to leak them if there was an error
	for (i=0 ; i < 3; i++ ) {
		if (inouterr[i] != -1) {
				if (close(inouterr[i] ) == -1) {
					dprintf ( D_ALWAYS, "FD closing problem, errno = %d\n", errno );
				}
		}
	}
	return retval;
}

void
Scheduler::display_shadow_recs()
{
	if( !IsFulldebug(D_FULLDEBUG) ) {
		return; // avoid needless work below
	}

	dprintf( D_FULLDEBUG, "\n");
	dprintf( D_FULLDEBUG, "..................\n" );
	dprintf( D_FULLDEBUG, ".. Shadow Recs (%d/%d)\n", numShadows, numMatches );
	for (const auto &[pid, r]: shadowsByPid) {
		int cur_hosts=-1, status=-1;
		GetAttributeInt(r->job_id.cluster, r->job_id.proc, ATTR_CURRENT_HOSTS, &cur_hosts);
		GetAttributeInt(r->job_id.cluster, r->job_id.proc, ATTR_JOB_STATUS, &status);

		dprintf(D_FULLDEBUG, ".. %d, %d.%d, %s, %s, cur_hosts=%d, status=%d\n",
				r->pid, r->job_id.cluster, r->job_id.proc,
				r->preempted ? "T" : "F" ,
				r->match ? r->match->peer : "localhost",
				cur_hosts, status);
	}
	dprintf( D_FULLDEBUG, "..................\n\n" );
}

shadow_rec::shadow_rec():
	pid(-1),
	universe(0),
    match(NULL),
	preempted(false),
	preempt_pending(false),
	conn_fd(-1),
	removed(FALSE),
	isZombie(FALSE),
	is_reconnect(false),
	reconnect_done(false),
	keepClaimAttributes(false),
	recycle_shadow_stream(NULL),
	exit_already_handled(false)
{
	prev_job_id.proc = -1;
	prev_job_id.cluster = -1;
	job_id.proc = -1;
	job_id.cluster = -1;
}

shadow_rec::~shadow_rec()
{
	if( recycle_shadow_stream ) {
		dprintf(D_ALWAYS,"Failed to finish switching shadow %d to new job %d.%d\n",pid,job_id.cluster,job_id.proc);
		delete recycle_shadow_stream;
		recycle_shadow_stream = NULL;
	}
}

struct shadow_rec *
Scheduler::add_shadow_rec( int pid, PROC_ID* job_id, int univ,
						   match_rec* mrec, int fd )
{
	shadow_rec *new_rec = new shadow_rec;

	new_rec->pid = pid;
	new_rec->job_id = *job_id;
	new_rec->universe = univ;
	new_rec->match = mrec;
	new_rec->preempted = FALSE;
	new_rec->removed = FALSE;
	new_rec->conn_fd = fd;
	new_rec->isZombie = FALSE; 
	new_rec->keepClaimAttributes = false;
	
	if (pid) {
		add_shadow_rec(new_rec);
	} else if ( new_rec->match && !new_rec->match->getPool().empty() ) {
		SetAttributeString(new_rec->job_id.cluster, new_rec->job_id.proc,
						ATTR_REMOTE_POOL,
						new_rec->match->getPool().empty() ? nullptr : new_rec->match->getPool().c_str(),
						NONDURABLE);
	}
	return new_rec;
}

void add_shadow_birthdate(int cluster, int proc, bool is_reconnect)
{
	dprintf( D_ALWAYS, "Starting add_shadow_birthdate(%d.%d)\n",
			 cluster, proc );
    time_t now = time(NULL);
	time_t current_time = now;
	time_t job_start_date = 0;
	SetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE, current_time);
	if (GetAttributeInt(cluster, proc,
						ATTR_JOB_START_DATE, &job_start_date) < 0) {
		// this is the first time the job has ever run, so set JobStartDate
		SetAttributeInt(cluster, proc, ATTR_JOB_START_DATE, current_time);
        
        time_t qdate = 0;
        GetAttributeInt(cluster, proc, ATTR_Q_DATE, &qdate);

		time_t now = scheduler.stats.Tick();
		scheduler.stats.JobsStarted += 1;
		scheduler.stats.JobsAccumTimeToStart += (current_time - qdate);

		if (scheduler.OtherPoolStats.AnyEnabled()) {

			ScheddOtherStats * other_stats = NULL;
			ClassAd * job_ad = GetJobAd(cluster, proc);
			if (job_ad) {
				other_stats = scheduler.OtherPoolStats.Matches(*job_ad, now);
				FreeJobAd(job_ad);
			}
			for (ScheddOtherStats * po = other_stats; po; po = po->next) {
				po->stats.JobsStarted += 1;
				po->stats.JobsAccumTimeToStart += (current_time - qdate);
			}
			scheduler.OtherPoolStats.Tick(now);
		}
	}

	// If we're reconnecting, the old ATTR_JOB_CURRENT_START_DATE is still
	// correct
	if ( !is_reconnect ) {
		// Update the current start & last start times
		if ( GetAttributeInt(cluster, proc,
							 ATTR_JOB_CURRENT_START_DATE, 
							 &job_start_date) >= 0 ) {
			// It's been run before, so copy the current into last
			SetAttributeInt(cluster, proc, ATTR_JOB_LAST_START_DATE, 
							job_start_date);
		}
		// Update current
		SetAttributeInt(cluster, proc, ATTR_JOB_CURRENT_START_DATE,
						current_time);
	}

	int job_univ = CONDOR_UNIVERSE_VANILLA;
	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &job_univ);


		// Update the job's counter for the number of times a shadow
		// was started (if this job has a shadow at all, that is).
		// For the local universe, "shadow" means local starter.
	int num = 0;
	switch (job_univ) {
	case CONDOR_UNIVERSE_SCHEDULER:
			// CRUFT: ATTR_JOB_RUN_COUNT is deprecated
		if (GetAttributeInt(cluster, proc, ATTR_JOB_RUN_COUNT, &num) < 0) {
			num = 0;
		}
		num++;
		SetAttributeInt(cluster, proc, ATTR_JOB_RUN_COUNT, num);
		break;

	default:
		if (GetAttributeInt(cluster, proc, ATTR_NUM_SHADOW_STARTS, &num) < 0) {
			num = 0;
		}
		num++;
		SetAttributeInt(cluster, proc, ATTR_NUM_SHADOW_STARTS, num);
			// CRUFT: ATTR_JOB_RUN_COUNT is deprecated
		SetAttributeInt(cluster, proc, ATTR_JOB_RUN_COUNT, num);
	}

	if( job_univ == CONDOR_UNIVERSE_VM ) {
		// check if this run is a restart from checkpoint
		time_t lastckptTime = 0;
		GetAttributeInt(cluster, proc, ATTR_LAST_CKPT_TIME, &lastckptTime);
		if( lastckptTime > 0 ) {
			// There was a checkpoint.
			// Update restart count from a checkpoint 
			std::string vmtype;
			int num_restarts = 0;
			GetAttributeInt(cluster, proc, ATTR_NUM_RESTARTS, &num_restarts);
			SetAttributeInt(cluster, proc, ATTR_NUM_RESTARTS, ++num_restarts);
		}
	}

	// Delete vacate reason from any previous execution attempt
	DeleteAttribute(cluster, proc, ATTR_VACATE_REASON);
	DeleteAttribute(cluster, proc, ATTR_VACATE_REASON_CODE);
	DeleteAttribute(cluster, proc, ATTR_VACATE_REASON_SUBCODE);
}

static void
RotateAttributeList( int cluster, int proc, char const *attrname, int start_index, int history_len )
{
	std::string attr_start_index;
	formatstr(attr_start_index, "%s%d", attrname, start_index);

	if (history_len < 2) {
		// nothing to rotate if list has just 0 or 1 entries....
		return;
	} else {
		// Only rotate if there is something new in MachineAttrX0 (the start_index element)
		char *value = NULL;
		GetAttributeExprNew(cluster, proc, attr_start_index.c_str(), &value);
		if (value) {
			free(value);
		} else {
			// MachineAttrX0 is empty, should not rotate
			return;
		}
	}

		// Rotate
	int index;
	for(index=start_index+history_len-1;
		index>start_index;
		index--)
	{
		std::string attr;
		formatstr(attr,"%s%d",attrname,index-1);

		char *value=NULL;
		GetAttributeExprNew(cluster,proc,attr.c_str(),&value);
		if (value) {
			formatstr(attr,"%s%d",attrname,index);
			SetAttribute(cluster,proc,attr.c_str(),value);
			free( value );
		}
	}

		// While it would make sense to now delete the start_index element (it now lives in index start_index+1),
		// historically MachineAttr0 was always present in jobs that were held or went back to Idle, so we will
		// keep it around for legacy happiness as it does no harm.
	// DeleteAttribute(cluster, proc, attr_start_index.c_str());
}

void
Scheduler::InsertMachineAttrs( int cluster, int proc, ClassAd *machine_ad, bool do_rotation )
{
	// Some explanation about the "do_rotation" parameter:
	// If do_rotation is False, then only insert the MachineAttr0 attribute and do not rotate.
	// If do_rotation is True, then only do the rotation.
	// The idea here is the schedd should invoke this function with do_rotation=False when
	// launching a shadow, and invoke with do_rotation=True whenever a job goes back to Idle.
	// This is because we do not want to rotate until after the shadow activates the claim, as 
	// we want minimal modifications to the job ad between matching, claiming, and activation.
	// Todd Tannenbaum, 4/2021

	if (!do_rotation) {
		ASSERT(machine_ad);
	}

	classad::ClassAdUnParser unparser;
	classad::ClassAd *machine;
	machine = machine_ad;

	ClassAd *job = GetJobAd( cluster, proc );

	if( !job ) {
		return;
	}

	bool already_in_transaction = InTransaction();
	if( !already_in_transaction ) {
	    BeginTransaction();
	}

		// First do the old-style match_list stuff

		// Look to see if the job wants info about 
		// old matches.  If so, store info about old
		// matches in the job ad as:
		//   LastMatchName0 = "some-startd-ad-name"
		//   LastMatchName1 = "some-other-startd-ad-name"
		//   ....
		// LastMatchName0 will hold the most recent.  The
		// list will be rotated with a max length defined
		// by attribute ATTR_JOB_LAST_MATCH_LIST_LENGTH, which
		// has a default of 0 (i.e. don't keep this info).
	int list_len = 0;
	job->LookupInteger(ATTR_LAST_MATCH_LIST_LENGTH,list_len);
	if ( list_len > 0 ) {
		if (do_rotation) {
			RotateAttributeList(cluster, proc, ATTR_LAST_MATCH_LIST_PREFIX, 0, list_len);
		} else {
			std::string attr_buf;
			std::string slot_name;
			machine_ad->LookupString(ATTR_NAME, slot_name);

			formatstr(attr_buf, "%s0", ATTR_LAST_MATCH_LIST_PREFIX);
			SetAttributeString(cluster, proc, attr_buf.c_str(), slot_name.c_str());
		}
	}

		// End of old-style match_list stuff

		// Increment ATTR_NUM_MATCHES
	if (!do_rotation) {
		int num_matches = 0;
		job->LookupInteger(ATTR_NUM_MATCHES, num_matches);
		num_matches++;

		SetAttributeInt(cluster, proc, ATTR_NUM_MATCHES, num_matches);

		SetAttributeInt(cluster, proc, ATTR_LAST_MATCH_TIME, time(0));
	}

		// Now handle JOB_MACHINE_ATTRS

	std::string user_machine_attrs;
	GetAttributeString(cluster,proc,ATTR_JOB_MACHINE_ATTRS,user_machine_attrs);

	int history_len = 1;
	GetAttributeInt(cluster,proc,ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH,&history_len);

	if( m_job_machine_attrs_history_length > history_len ) {
		history_len = m_job_machine_attrs_history_length;
	}

	if( history_len == 0 ) {
		return;
	}

	std::vector<std::string> machine_attrs = split(user_machine_attrs);

	for (auto& attr: m_job_machine_attrs) {
		if (!contains_anycase(machine_attrs, attr)) {
			machine_attrs.emplace_back(attr);
		}
	}

	for (auto& attr: machine_attrs) {
		std::string result_attr;
		formatstr(result_attr, "%s%s", ATTR_MACHINE_ATTR_PREFIX, attr.c_str());

		if (do_rotation) {
			RotateAttributeList(cluster,proc,result_attr.c_str(),0,history_len);
		} else {
			classad::Value result;
			if (!machine->EvaluateAttr(attr, result)) {
				result.SetErrorValue();
			}
			std::string unparsed_result;

			unparser.Unparse(unparsed_result, result);
			result_attr += "0";
			SetAttribute(cluster,proc,result_attr.c_str(),unparsed_result.c_str());
		}
	}

	FreeJobAd( job );

	if( !already_in_transaction ) {
		CommitNonDurableTransactionOrDieTrying();
	}
}

struct shadow_rec *
Scheduler::add_shadow_rec( shadow_rec* new_rec )
{
	if ( new_rec->universe != CONDOR_UNIVERSE_SCHEDULER &&
		 new_rec->universe != CONDOR_UNIVERSE_LOCAL ) {

		numShadows++;
	}
	if( new_rec->pid ) {
		shadowsByPid.emplace(new_rec->pid, new_rec);
	}
	shadowsByProcID.emplace(new_rec->job_id, new_rec);

		// To improve performance and to keep our sanity in case we
		// get killed in the middle of this operation, do all of these
		// queue management ops within a transaction.
	BeginTransaction();

	match_rec* mrec = new_rec->match;
	int cluster = new_rec->job_id.cluster;
	int proc = new_rec->job_id.proc;

	if( mrec && !new_rec->is_reconnect ) {

			// we don't want to set any of these things if this is a
			// reconnect shadow_rec we're adding.  All this does is
			// re-writes stuff that's already accurate in the job ad,
			// or, in the case of ATTR_LAST_JOB_LEASE_RENEWAL,
			// clobbers accurate info with a now-bogus value.

		SetPrivateAttributeString( cluster, proc, ATTR_CLAIM_ID, mrec->claim_id.claimId() );
		SetAttributeString( cluster, proc, ATTR_PUBLIC_CLAIM_ID, mrec->claim_id.publicClaimId() );
		SetAttributeString( cluster, proc, ATTR_STARTD_IP_ADDR, mrec->peer );
		SetAttributeInt( cluster, proc, ATTR_LAST_JOB_LEASE_RENEWAL,
						 time(0) );

		bool have_remote_host = false;
		if( mrec->my_match_ad ) {
			char* tmp = NULL;
			mrec->my_match_ad->LookupString(ATTR_NAME, &tmp );
			if( tmp ) {
				SetAttributeString( cluster, proc, ATTR_REMOTE_HOST, tmp );
				have_remote_host = true;
				free( tmp );
				tmp = NULL;
			}
			int slot = 1;
			mrec->my_match_ad->LookupInteger( ATTR_SLOT_ID, slot );
			SetAttributeInt(cluster,proc,ATTR_REMOTE_SLOT_ID,slot);

			InsertMachineAttrs(cluster,proc,mrec->my_match_ad,false);
		}
		if( ! have_remote_host ) {
				// CRUFT
			dprintf( D_ALWAYS, "ERROR: add_shadow_rec() doesn't have %s "
					 "for of remote resource for setting %s, using "
					 "inferior alternatives!\n", ATTR_NAME, 
					 ATTR_REMOTE_HOST );
			condor_sockaddr addr;
			if( mrec->peer && mrec->peer[0] && addr.from_sinful(mrec->peer) ) {
					// make local copy of static hostname buffer
				std::string hostname = get_hostname(addr);
				if (hostname.length() > 0) {
					SetAttributeString( cluster, proc, ATTR_REMOTE_HOST,
										hostname.c_str() );
					dprintf( D_FULLDEBUG, "Used inverse DNS lookup (%s)\n",
							 hostname.c_str() );
				} else {
						// Error looking up host info...
						// Just use the sinful string
					SetAttributeString( cluster, proc, ATTR_REMOTE_HOST, 
										mrec->peer );
					dprintf( D_ALWAYS, "Inverse DNS lookup failed! "
							 "Using ip/port %s", mrec->peer );
				}
			}
		}
		if( !mrec->getPool().empty() ) {
			SetAttributeString(cluster, proc, ATTR_REMOTE_POOL, mrec->getPool().c_str());
		}
		if ( mrec->auth_hole_id ) {
			SetAttributeString(cluster,
			                   proc,
			                   ATTR_STARTD_PRINCIPAL,
			                   mrec->auth_hole_id->c_str());
		}
	}
	GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &new_rec->universe );
	add_shadow_birthdate( cluster, proc, new_rec->is_reconnect );
	CommitTransactionOrDieTrying();
	if( new_rec->pid ) {
		dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
				 new_rec->pid, cluster, proc );
		//scheduler.display_shadow_recs();
	}
	RecomputeAliveInterval(cluster,proc);
	return new_rec;
}


void
Scheduler::add_shadow_rec_pid( shadow_rec* new_rec )
{
	if( ! new_rec->pid ) {
		EXCEPT( "add_shadow_rec_pid() called on an srec without a pid!" );
	}
	shadowsByPid.emplace(new_rec->pid, new_rec);
	dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
			 new_rec->pid, new_rec->job_id.cluster, new_rec->job_id.proc );
	//scheduler.display_shadow_recs();
}


void
Scheduler::RecomputeAliveInterval(int cluster, int proc)
{
	// This function makes certain that the schedd sends keepalives to the startds
	// often enough to ensure that no claims are killed before a job's
	// ATTR_JOB_LEASE_DURATION has passed...  This makes certain that 
	// jobs utilizing the disconnection starter/shadow feature are not killed
	// off before they should be.

	int interval = 0;
	GetAttributeInt( cluster, proc, ATTR_JOB_LEASE_DURATION, &interval );
	if ( interval > 0 ) {
			// Divide by three, so that even if we miss two keep
			// alives in a row, the startd won't kill the claim.
		interval /= 3;
			// Floor value: no way are we willing to send alives more often
			// than every 10 seconds
		if ( interval < 10 ) {
			interval = 10;
		}
			// If this is the smallest interval we've seen so far,
			// then update leaseAliveInterval.
		if ( leaseAliveInterval > interval ) {
			leaseAliveInterval = interval;
		}
			// alive_interval is the value we actually set in a timer.
			// make certain it is smaller than the smallest 
			// ATTR_JOB_LEASE_DURATION we've seen.
		if ( alive_interval > leaseAliveInterval ) {
			alive_interval = leaseAliveInterval;
			daemonCore->Reset_Timer(aliveid,10,alive_interval);
			dprintf(D_FULLDEBUG,
					"Reset alive_interval to %d based on %s in job %d.%d\n",
					alive_interval,ATTR_JOB_LEASE_DURATION,cluster,proc);
		}
	}
}


void
CkptWallClock(int /* tid */)
{
	int first_time = 1;
	time_t current_time = time(0);
	ClassAd *ad;
	bool began_transaction = false;
	while( (ad = GetNextJob(first_time)) ) {
		first_time = 0;
		int status = IDLE;
		ad->LookupInteger(ATTR_JOB_STATUS, status);
		if (status == RUNNING || status == TRANSFERRING_OUTPUT) {
			time_t bday = 0;
			ad->LookupInteger(ATTR_SHADOW_BIRTHDATE, bday);
			time_t run_time = current_time - bday;
			if (bday && run_time > WallClockCkptInterval) {
				int cluster, proc;
				ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
				ad->LookupInteger(ATTR_PROC_ID, proc);

				if( !began_transaction ) {
					began_transaction = true;
					BeginTransaction();
				}

				SetAttributeInt(cluster, proc, ATTR_JOB_WALL_CLOCK_CKPT,
								run_time);
			}
		}
	}
	if( began_transaction ) {
		CommitTransactionOrDieTrying();
	}
}

static void
update_remote_wall_clock(int cluster, int proc)
{
		// update ATTR_JOB_REMOTE_WALL_CLOCK.  note: must do this before
		// we call check_zombie below, since check_zombie is where the
		// job actually gets removed from the queue if job completed or deleted
	time_t bday = 0;
	GetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE,&bday);
	if (bday) {
		double accum_time = 0;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
		double delta = (double) time(NULL) - bday;
		SetAttributeFloat(cluster, proc, ATTR_JOB_LAST_REMOTE_WALL_CLOCK, delta);
		accum_time += delta;
			// We want to update our wall clock time and delete
			// our wall clock checkpoint inside a transaction, so
			// we are sure not to double-count.  The wall-clock
			// checkpoint (see CkptWallClock above) ensures that
			// if we crash before committing our wall clock time,
			// we won't lose too much.  
			// Luckily we're now already inside a transaction, since
			// *all* of the qmgmt ops we do when we delete the shadow
			// rec are inside a transaction now... 
		SetAttributeFloat(cluster, proc,
						  ATTR_JOB_REMOTE_WALL_CLOCK,accum_time);
		DeleteAttribute(cluster, proc, ATTR_JOB_WALL_CLOCK_CKPT);

		double slot_weight = 1;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0,&slot_weight);
		double slot_time = 0;
		GetAttributeFloat(cluster, proc,
						  ATTR_CUMULATIVE_SLOT_TIME,&slot_time);
		slot_time += delta*slot_weight;
		SetAttributeFloat(cluster, proc,
						  ATTR_CUMULATIVE_SLOT_TIME,slot_time);
	}
}



void
Scheduler::delete_shadow_rec(int pid)
{
	auto it = shadowsByPid.find(pid);
	if( it != shadowsByPid.end()) {
		delete_shadow_rec(it->second);
	} else {
		dprintf( D_ALWAYS, "ERROR: can't find shadow record for pid %d\n", pid );
	}
}


void
Scheduler::delete_shadow_rec( shadow_rec *rec )
{
	if ( rec->is_reconnect && !rec->reconnect_done ) {
		// TODO Should we try to update the JobsRestartReconnectsBadput
		//   stat on an interrupted reconnect attempt?
		scheduler.stats.JobsRestartReconnectsAttempting += -1;
		scheduler.stats.JobsRestartReconnectsInterrupted += 1;
	}
	if ( FindSrecByProcID(rec->job_id) == NULL ) {
		// add_shadow_rec() wasn't called, do simple cleanup
		// TODO Failure to spawn a reconnect shadow should probably still
		//   do the code below our early return here.
		RemoveShadowRecFromMrec(rec);
		delete rec;
		return;
	}

	int cluster = rec->job_id.cluster;
	int proc = rec->job_id.proc;
	int pid = rec->pid;

	if( pid ) {
		dprintf( D_FULLDEBUG,
				 "Deleting shadow rec for PID %d, job (%d.%d)\n",
				 pid, cluster, proc );
	} else {
		dprintf( D_FULLDEBUG, "Deleting shadow rec for job (%d.%d) "
				 "no shadow PID -- shadow never spawned\n",
				 cluster, proc );
	}

	BeginTransaction();

	int job_status = IDLE;
	GetAttributeInt( cluster, proc, ATTR_JOB_STATUS, &job_status );

	if( pid ) {
			// we only need to update this if we spawned a shadow.
		update_remote_wall_clock(cluster, proc);
	}

		/*
		  For ATTR_REMOTE_HOST and ATTR_CLAIM_ID, we only want to save
		  what we have in ATTR_LAST_* if we actually spawned a
		  shadow...
		*/
	if( pid ) {
		char* last_host = NULL;
		GetAttributeStringNew( cluster, proc, ATTR_REMOTE_HOST, &last_host );
		if( last_host ) {
			SetAttributeString( cluster, proc, ATTR_LAST_REMOTE_HOST,
								last_host );
			free( last_host );
			last_host = NULL;
		}

        char* last_pool = NULL;
		GetAttributeStringNew( cluster, proc, ATTR_REMOTE_POOL, &last_pool );
		if( last_pool ) {
			SetAttributeString( cluster, proc, ATTR_LAST_REMOTE_POOL,
								last_pool );
			free( last_pool );
			last_pool = NULL;
		} else {
            // If RemotePool is not defined, be sure to remove the last remote pool (if it exists)
             DeleteAttribute( cluster, proc, ATTR_LAST_REMOTE_POOL );
        }

	}

	if( pid ) {
		char* last_claim = NULL;
		GetAttributeStringNew( cluster, proc, ATTR_PUBLIC_CLAIM_ID, &last_claim );
		if( last_claim ) {
			SetAttributeString( cluster, proc, ATTR_LAST_PUBLIC_CLAIM_ID, 
								last_claim );
			free( last_claim );
			last_claim = NULL;
		}

		GetAttributeStringNew( cluster, proc, ATTR_PUBLIC_CLAIM_IDS, &last_claim );
		if( last_claim ) {
			SetAttributeString( cluster, proc, ATTR_LAST_PUBLIC_CLAIM_IDS, 
								last_claim );
			free( last_claim );
			last_claim = NULL;
		}
	}

		//
		// If the job is not in a terminal state (i.e. COMPLETED or REMOVED), then
		// do not remove the ClaimId or RemoteHost if the keepClaimAttributes
		// flag is set. This means that we want this job to reconnect
		// when the schedd comes back online.
		//
	if ( (!rec->keepClaimAttributes) || job_status == COMPLETED || job_status == REMOVED ) {
		DeletePrivateAttribute( cluster, proc, ATTR_CLAIM_ID );
		DeleteAttribute( cluster, proc, ATTR_PUBLIC_CLAIM_ID );
		DeletePrivateAttribute( cluster, proc, ATTR_CLAIM_IDS );
		DeleteAttribute( cluster, proc, ATTR_PUBLIC_CLAIM_IDS );
		DeleteAttribute( cluster, proc, ATTR_STARTD_IP_ADDR );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_HOST );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_POOL );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_SLOT_ID );
		DeleteAttribute( cluster, proc, ATTR_DELEGATED_PROXY_EXPIRATION );
		DeleteAttribute( cluster, proc, ATTR_TRANSFERRING_INPUT );
		DeleteAttribute( cluster, proc, ATTR_TRANSFERRING_OUTPUT );
		DeleteAttribute( cluster, proc, ATTR_TRANSFER_QUEUED );
	} else {
		dprintf( D_FULLDEBUG, "Job %d.%d has keepClaimAttributes set to true. "
					    "Not removing %s and %s attributes.\n",
					    cluster, proc, ATTR_CLAIM_ID, ATTR_REMOTE_HOST );
	}

	// If job not in a terminal state, rotate the MachineAttr attributes
	// so we are ready for the next match...
	if (job_status != COMPLETED && job_status != REMOVED) {
		InsertMachineAttrs(cluster, proc, nullptr, true);
	}

	DeleteAttribute( cluster, proc, ATTR_SHADOW_BIRTHDATE );

		// we want to commit all of the above changes before we
		// call check_zombie() since it might do it's own
		// transactions of one sort or another...
		// Nothing written in this transaction requires immediate sync to disk.
	CommitNonDurableTransactionOrDieTrying();

	if( ! rec->keepClaimAttributes ) {
			// We do _not_ want to call check_zombie if we are detaching
			// from a job for later reconnect, because check_zombie
			// does stuff that should only happen if the shadow actually
			// exited, such as setting CurrentHosts=0.
		check_zombie( pid, &(rec->job_id) );
	}

		// If the shadow went away, this match is no longer
		// "ACTIVE", it's just "CLAIMED"
	if( rec->match ) {
			// Be careful, since there might not be a match record
			// for this shadow record anymore... 
		rec->match->setStatus( M_CLAIMED );
	}

	if( rec->keepClaimAttributes &&  rec->match ) {
			// We are shutting down and detaching from this claim.
			// Remove the claim record without sending RELEASE_CLAIM
			// to the startd.
		rec->match->needs_release_claim = false;
		DelMrec(rec->match);
	}
	else {
		RemoveShadowRecFromMrec(rec);
	}

	if( pid ) {
		shadowsByPid.erase(pid);
	}
	shadowsByProcID.erase(rec->job_id);
	if ( rec->conn_fd != -1 ) {
		close(rec->conn_fd);
	}

	if ( rec->universe != CONDOR_UNIVERSE_SCHEDULER &&
		 rec->universe != CONDOR_UNIVERSE_LOCAL ) {
		numShadows -= 1;
	}
	delete rec;
	if( ExitWhenDone && numShadows == 0 ) {
		return;
	}
	if( pid ) {
	  //display_shadow_recs();
	}

	dprintf( D_FULLDEBUG, "Exited delete_shadow_rec( %d )\n", pid );
	return;
}

/*
** Mark a job as running.
*/
void
mark_job_running(PROC_ID* job_id)
{
	int status = 0;
	int orig_max = 1; // If it was not set this is the same default

	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, &status);
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_MAX_HOSTS, &orig_max);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_ORIG_MAX_HOSTS,
					orig_max);


	if( status == RUNNING ) {
		EXCEPT( "Trying to run job %d.%d, but already marked RUNNING!",
			job_id->cluster, job_id->proc );
	}

	status = RUNNING;

	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, status);
	SetAttributeInt(job_id->cluster, job_id->proc,
					ATTR_ENTERED_CURRENT_STATUS, time(0) );
	SetAttributeInt(job_id->cluster, job_id->proc,
					ATTR_LAST_SUSPENSION_TIME, 0 );


		// If this is a scheduler universe job, increment the
		// job counter for the number of times it started executing.
	int univ = CONDOR_UNIVERSE_VANILLA;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE, &univ);
	if (univ == CONDOR_UNIVERSE_SCHEDULER) {
		int num = 0;
		if (GetAttributeInt(job_id->cluster, job_id->proc,
							ATTR_NUM_JOB_STARTS, &num) < 0) {
			num = 0;
		}
		num++;
		SetAttributeInt(job_id->cluster, job_id->proc,
						ATTR_NUM_JOB_STARTS, num);
	}
	MarkJobClean(*job_id);
}

void
mark_serial_job_running( PROC_ID *job_id )
{
	BeginTransaction();
	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
		// nothing that has been written in this transaction needs to
		// be immediately synced to disk
	CommitNonDurableTransactionOrDieTrying();
}

/*
** Mark a job as stopped, (Idle).  Do not call directly.  
** Call the non-underscore version below instead.
*/
void
_mark_job_stopped(PROC_ID* job_id)
{
	int		status    = 0;
	int		orig_max  = 0;
	int		had_orig  = 0;

		// NOTE: This function is wrapped in a NONDURABLE transaction.

	had_orig = GetAttributeInt(job_id->cluster, job_id->proc, 
								ATTR_ORIG_MAX_HOSTS, &orig_max);

	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, &status);

		// Always set CurrentHosts to 0 here, because we increment
		// CurrentHosts before we set the job status to RUNNING, so
		// CurrentHosts may need to be reset even if job status never
		// changed to RUNNING.  It is very important that we keep
		// CurrentHosts accurate, because we use it to determine if we
		// need to negotiate for more matches.
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 0);

		/*
		  Always clear out ATTR_SHADOW_BIRTHDATE.  If there's no
		  shadow and the job is stopped, it's dumb to leave the shadow
		  birthday attribute in the job ad.  this confuses condor_q
		  if the job is marked as running, added to the runnable job
		  queue, and then JOB_START_DELAY is big.  we used to clear
		  this out in mark_job_running(), but that's not really a good
		  idea.  it's better to just clear it out whenever the shadow
		  is gone.  Derek <wright@cs.wisc.edu>
		*/
	DeleteAttribute( job_id->cluster, job_id->proc, ATTR_SHADOW_BIRTHDATE );

	// if job isn't RUNNING, then our work is already done
	if (status == RUNNING || status == TRANSFERRING_OUTPUT) {

		SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, IDLE);
		SetAttributeInt( job_id->cluster, job_id->proc,
						 ATTR_ENTERED_CURRENT_STATUS, time(0) );
		SetAttributeInt( job_id->cluster, job_id->proc,
						 ATTR_LAST_SUSPENSION_TIME, 0 );

		if (had_orig >= 0) {
			SetAttributeInt(job_id->cluster, job_id->proc, ATTR_MAX_HOSTS,
							orig_max);
		}
		DeleteAttribute( job_id->cluster, job_id->proc, ATTR_REMOTE_POOL );

		dprintf( D_FULLDEBUG, "Marked job %d.%d as IDLE\n", job_id->cluster,
				 job_id->proc );
	}	
}


/* Parallel jobs may have many procs (job classes) in a cluster.  We should
   mark all of them stopped when the job stops. */
void
mark_job_stopped(PROC_ID* job_id)
{
	bool already_in_transaction = InTransaction();
	if( !already_in_transaction ) {
		BeginTransaction();
	}

	int universe = CONDOR_UNIVERSE_VANILLA;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE,
					&universe);
	int wantPS = 0;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_WANT_PARALLEL_SCHEDULING,
					&wantPS);
	if( (universe == CONDOR_UNIVERSE_MPI) || 
		(universe == CONDOR_UNIVERSE_PARALLEL) || wantPS ){
		ClassAd *ad;
		ad = GetNextJob(1);
		while (ad != NULL) {
			PROC_ID tmp_id;
			ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == job_id->cluster) {
				ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				_mark_job_stopped(&tmp_id);
			}
			ad = GetNextJob(0);
		}
	} else {
		_mark_job_stopped(job_id);
	}

	if( !already_in_transaction ) {
			// It is ok to use a NONDURABLE transaction here.
			// The worst that can happen if this transaction is
			// lost is that we will try to reconnect to the job
			// and find that it is no longer running.
		CommitNonDurableTransactionOrDieTrying();
	}
}



/*
 * Ask daemonCore to check to see if a given process is still alive
 */
inline int
Scheduler::is_alive(shadow_rec* srec)
{
	return daemonCore->Is_Pid_Alive(srec->pid);
}

void
Scheduler::clean_shadow_recs()
{
	dprintf( D_FULLDEBUG, "============ Begin clean_shadow_recs =============\n" );

	for (const auto &[pid, rec]: shadowsByPid) {
		if( !is_alive(rec) ) {
			if ( rec->isZombie ) { // bad news...means we missed a reaper
				dprintf( D_ALWAYS,
				"Zombie process has not been cleaned up by reaper - pid %d\n", rec->pid );
			} else {
				dprintf( D_FULLDEBUG, "Setting isZombie status for pid %d\n", rec->pid );
				rec->isZombie = TRUE;
			}

			// we don't want this old safety code to run anymore (stolley)
			//			delete_shadow_rec( rec->pid );
		}
	}
    stats.ShadowsRunning = numShadows;
	dprintf( D_FULLDEBUG, "============ End clean_shadow_recs =============\n" );
}


void
Scheduler::preempt( int n, bool force_sched_jobs )
{
	bool preempt_sched = force_sched_jobs;

	dprintf( D_ALWAYS, "Called preempt( %d )%s%s\n", n, 
			 force_sched_jobs  ? " forcing scheduler/local univ preemptions" : "",
			 ExitWhenDone ? " for a graceful shutdown" : "" );

	/* Now we loop until we are out of shadows or until we've preempted
	 * `n' shadows.  Note that the behavior of this loop is slightly 
	 * different if ExitWhenDone is True.  If ExitWhenDone is True, we
	 * will always preempt `n' new shadows, used for a progressive shutdown.  If
	 * ExitWhenDone is False, we will preempt n minus the number of shadows we
	 * have previously told to preempt but are still waiting for them to exit.
	 */
	for (const auto &[pid, rec]: shadowsByPid) {
		if (n == 0) break;
		if(is_alive(rec)) {
			if( rec->preempted ) {
				if( ! ExitWhenDone ) {
						// if we're not trying to exit, we should
						// consider this record already in the process
						// of preempting, and let it count towards our
						// "n" shadows to preempt.
					n--;
				}
					// either way, if we already preempted this srec
					// there's nothing more to do here, and we need to
					// keep looking for another srec to preempt (or
					// bail out of our iteration if we've hit "n").
				continue;
			}

				// if we got this far, it's an srec that hasn't been
				// preempted yet.  based on the universe, do the right
				// thing to preempt it.
			int cluster = rec->job_id.cluster;
			int proc = rec->job_id.proc; 
			ClassAd* job_ad;
			int kill_sig;

			switch( rec->universe ) {
			case CONDOR_UNIVERSE_LOCAL:
				if( ! preempt_sched ) {
					continue;
				}
				dprintf( D_ALWAYS, "Sending DC_SIGSOFTKILL to handler for "
						 "local universe job %d.%d (pid: %d)\n", 
						 cluster, proc, rec->pid );
				sendSignalToShadow(rec->pid,DC_SIGSOFTKILL,rec->job_id);
				break;

			case CONDOR_UNIVERSE_SCHEDULER:
				if( ! preempt_sched ) {
					continue;
				}
				job_ad = GetJobAd( rec->job_id.cluster,
								   rec->job_id.proc );  
				kill_sig = findSoftKillSig( job_ad );
				if( kill_sig <= 0 ) {
					kill_sig = SIGTERM;
				}
				FreeJobAd( job_ad );
				dprintf( D_ALWAYS, "Sending %s to scheduler universe job "
						 "%d.%d (pid: %d)\n", signalName(kill_sig), 
						 cluster, proc, rec->pid );
				sendSignalToShadow(rec->pid,kill_sig,rec->job_id);
				break;

			default:
				// all other universes	
				if( rec->match ) {
						//
						// If we're in graceful shutdown mode, we only want to
						// send a vacate command to jobs that do not have a lease
						//
					bool skip_vacate = false;
					if ( ExitWhenDone ) {
						int lease = 0;
						GetAttributeInt( cluster, proc,
									ATTR_JOB_LEASE_DURATION, &lease );
						skip_vacate = ( lease > 0 );
							//
							// We set keepClaimAttributes so that RemoteHost and ClaimId
							// are not removed from the job's ad when we delete
							// the shadow record
							//
						rec->keepClaimAttributes = skip_vacate;
					}
						//
						// Send a vacate if appropriate
						//
					if ( ! skip_vacate ) {
						send_vacate( rec->match, DEACTIVATE_CLAIM );
						dprintf( D_ALWAYS, 
								"Sent vacate command to %s for job %d.%d\n",
								rec->match->peer, cluster, proc );
					}
						//
						// If shutting down, send a SIGKILL
						//
					if ( ExitWhenDone ) {
							//
							// Call the blocking form of Send_Signal, rather than
							// sendSignalToShadow().
							//
						daemonCore->Send_Signal( rec->pid, SIGKILL );
						dprintf( D_ALWAYS, 
								"Sent signal %d to %s [pid %d] for job %d.%d\n",
								SIGKILL, rec->match->peer, rec->pid, cluster, proc );
							// Keep iterating and preempting more without
							// decrementing n here.  Why?  Because we didn't
							// really preempt this job: we just killed the
							// shadow and left the job running so that we
							// can reconnect to it later.  No need to throttle
							// the rate of preemption to avoid i/o overload
							// from checkpoints or anything.  In fact, it
							// is better to quickly kill all the shadows so
							// that we can restart and reconnect before the
							// lease expires.
						continue;
					}
				} else {
						/*
						   A shadow record without a match for any
						   universe other than local, and
						   scheduler (which we already handled above)
						   is a shadow for which the claim was
						   relinquished (by the startd).  In this
						   case, the shadow is on its way out, anyway,
						   so there's no reason to send it a signal.
						*/
				}
			} // SWITCH
				// if we're here, we really preempted it, so
				// decrement n so we let this count towards our goal.
			n--;
		} // IF
	} // WHILE

		/*
		  we've now broken out of our loop.  if n is still >0, it
		  means we wanted to preempt more than we were able to do.
		  this could be because of a mis-match regarding scheduler
		  universe jobs (namely, all we have left are scheduler jobs,
		  but we have a bogus value for SchedUniverseJobsRunning and
		  don't think we want to preempt any of those).  so, if we
		  weren't trying to preempt scheduler but we still have n to
		  preempt, try again and force scheduler preemptions.
		  derek <wright@cs.wisc.edu> 2005-04-01

		  We only want to do this if we're shutting down.
		*/ 
	if( n > 0 && preempt_sched == false && ExitWhenDone ) {
		preempt( n, true );
	}
}

void
send_vacate(match_rec* match,int cmd)
{
	classy_counted_ptr<DCStartd> startd = new DCStartd( match->description(),NULL,match->peer,match->claim_id.claimId() );
	classy_counted_ptr<DCClaimIdMsg> msg = new DCClaimIdMsg( cmd, match->claim_id.claimId() );

	msg->setSuccessDebugLevel(D_ALWAYS);
	msg->setTimeout( STARTD_CONTACT_TIMEOUT );
	if (match->use_sec_session) {
		msg->setSecSessionId( match->claim_id.secSessionId() );
	}

	if ( !startd->hasUDPCommandPort() || param_boolean("SCHEDD_SEND_VACATE_VIA_TCP",true) ) {
		dprintf( D_FULLDEBUG, "Called send_vacate( %s, %d ) via TCP\n", 
				 match->peer, cmd );
		msg->setStreamType(Stream::reli_sock);
	} else {
		dprintf( D_FULLDEBUG, "Called send_vacate( %s, %d ) via UDP\n", 
				 match->peer, cmd );
		msg->setStreamType(Stream::safe_sock);
	}
	startd->sendMsg( msg.get() );
}

void
Scheduler::swap_space_exhausted()
{
	SwapSpaceExhausted = TRUE;
}

/*
  We maintain two tables which should be consistent, return TRUE if they
  are, and FALSE otherwise.  The tables are the ShadowRecs, a list
  of currently running jobs, and PrioRec a list of currently runnable
  jobs.  We will say they are consistent if none of the currently
  runnable jobs are already listed as running jobs.
*/
int
Scheduler::shadow_prio_recs_consistent()
{
	int		i;
	struct shadow_rec	*srp;
	int		status = 0;
	int universe = 0;

	dprintf( D_FULLDEBUG, "Checking consistency of running and runnable jobs\n" );
	BadCluster = -1;
	BadProc = -1;

	for( i=0; i<N_PrioRecs; i++ ) {
		if( (srp=FindSrecByProcID(PrioRec[i].id)) ) {
			BadCluster = srp->job_id.cluster;
			BadProc = srp->job_id.proc;
			universe = srp->universe;
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_STATUS, &status);
			if (status != RUNNING && status != TRANSFERRING_OUTPUT &&
				universe!=CONDOR_UNIVERSE_MPI &&
				universe!=CONDOR_UNIVERSE_PARALLEL) {
				// display_shadow_recs();
				// dprintf(D_FULLDEBUG,"shadow_prio_recs_consistent(): PrioRec %d - id = %d.%d, owner = %s\n",i,PrioRec[i].id.cluster,PrioRec[i].id.proc,PrioRec[i].owner);
				dprintf( D_ALWAYS, "ERROR: Found a consistency problem in the PrioRec array for job %d.%d !!!\n", PrioRec[i].id.cluster,PrioRec[i].id.proc );
				return FALSE;
			}
		}
	}
	dprintf( D_FULLDEBUG, "Shadow and PrioRec Tables are consistent\n" );
	return TRUE;
}

/*
  If we have an MPI cluster with > 1 proc, the user
  might condor_rm/_hold/_release one of those procs.
  If so, we need to treat it as if all of the procs
  in the cluster are _rm'd/_held/_released.  This
  copies all the procs from job_ids to expanded_ids,
  adding any sibling mpi procs if needed.
*/
void
Scheduler::expand_mpi_procs(const std::vector<std::string> &job_ids, std::vector<std::string> &expanded_ids) {
	char buf[40];

	for (const auto &id: job_ids) {
		expanded_ids.emplace_back(id);
	}

	for (const auto &id: job_ids) {
		PROC_ID p = getProcByString(id.c_str());
		if( (p.cluster < 0) || (p.proc < 0) ) {
			continue;
		}

		int universe = -1;
		GetAttributeInt(p.cluster, p.proc, ATTR_JOB_UNIVERSE, &universe);
		int wantPS = 0;
		GetAttributeInt(p.cluster, p.proc, ATTR_WANT_PARALLEL_SCHEDULING, &wantPS);
		if ((universe != CONDOR_UNIVERSE_MPI) && (universe != CONDOR_UNIVERSE_PARALLEL) && (!wantPS))
			continue;
		
		
		int proc_index = 0;
		while ((GetJobAd(p.cluster, proc_index) )) {
			snprintf(buf, 40, "%d.%d", p.cluster, proc_index);
			if (!contains(expanded_ids,buf)) {
				expanded_ids.emplace_back(buf);
			}
			proc_index++;
		}
	}
}

void
Scheduler::mail_problem_message()
{
	FILE	*mailer;

	dprintf( D_ALWAYS, "Mailing administrator (%s)\n",
			 CondorAdministrator ? CondorAdministrator : "<undefined>" );

	mailer = email_admin_open("CONDOR Problem");
	if (mailer == NULL)
	{
		// Could not send email, probably because no admin 
		// email address defined.  Just return.  No biggie.  Keep 
		// your pants on.
		return;
	}

	fprintf( mailer, "Problem with condor_schedd %s\n", Name );
	fprintf( mailer, "Job %d.%d is in the runnable job table,\n",
												BadCluster, BadProc );
	fprintf( mailer, "but we already have a shadow record for it.\n" );

	email_close(mailer);
}

static bool
IsSchedulerUniverse( shadow_rec* srec )
{
	if( ! srec ) {
		return false;
	}
	return srec->universe == CONDOR_UNIVERSE_SCHEDULER;
}


static bool
IsLocalUniverse( shadow_rec* srec )
{
	if( ! srec ) {
		return false;
	}
	return srec->universe == CONDOR_UNIVERSE_LOCAL;
}


/*
** Wrapper for setting the job status to deal with Parallel jobs, which can 
** contain multiple procs.
*/
void
set_job_status(int cluster, int proc, int status)
{
	int universe = CONDOR_UNIVERSE_VANILLA;
	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe);

	int wantPS = 0;
	GetAttributeInt(cluster, proc, ATTR_WANT_PARALLEL_SCHEDULING, &wantPS);

	BeginTransaction();

	if( ( universe == CONDOR_UNIVERSE_MPI) || 
		( universe == CONDOR_UNIVERSE_PARALLEL) || wantPS ) {
		ClassAd *ad;
		ad = GetNextJob(1);
		while (ad != NULL) {
			PROC_ID tmp_id;
			ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == cluster) {
				ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				SetAttributeInt(tmp_id.cluster, tmp_id.proc, ATTR_JOB_STATUS,
								status);
				SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								 ATTR_ENTERED_CURRENT_STATUS,
								 time(0) );
				SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								 ATTR_LAST_SUSPENSION_TIME, 0 ); 
			}
			ad = GetNextJob(0);
		}
	} else {
		SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, status);
		SetAttributeInt( cluster, proc, ATTR_ENTERED_CURRENT_STATUS,
						 time(nullptr) );
		SetAttributeInt( cluster, proc,
						 ATTR_LAST_SUSPENSION_TIME, 0 ); 
	}

		// Nothing written in this transaction requires immediate
		// sync to disk.
	CommitNonDurableTransactionOrDieTrying();
}


void
Scheduler::child_exit(int pid, int status)
{
	shadow_rec*     srec;
	int             StartJobsFlag=TRUE;
	PROC_ID	        job_id;
	bool            srec_was_local_universe = false;
	std::string        claim_id;
	// if we do not start a new job, should we keep the claim?
	bool            keep_claim = false; // by default, no
	bool            srec_keep_claim_attributes;

	srec = FindSrecByPid(pid);
	ASSERT(srec);

	if( srec->match ) {
		if (srec->exit_already_handled && (srec->match->keep_while_idle == 0)) {
			DelMrec( srec->match );
			srec->match = NULL;
		} else {
			int exitstatus = WEXITSTATUS(status);
			if ((srec->match->keep_while_idle > 0) && ((exitstatus == JOB_EXITED) || (exitstatus == JOB_SHOULD_REMOVE) || (exitstatus == JOB_KILLED))) {
				srec->match->setStatus(M_CLAIMED);
				srec->match->shadowRec = NULL;
				srec->match->idle_timer_deadline = time(NULL) + srec->match->keep_while_idle;
				srec->match = NULL;
			}
		}
	}

	// Is this match earmarked for a high-priority job?  We could, based
	// on the code in Scheduler::RecycleShadow(), put this code after
	// the check to see if the exit has already been handled, but I think
	// this makes it less likely that changes there will cause problems.
	if( srec->match && srec->match->m_now_job.isJobKey() ) {
		PROC_ID bid = srec->match->m_now_job;
		pcccGot( bid, srec->match );
		StartJobsFlag = FALSE;
		keep_claim = true;

		if( pcccSatisfied( bid ) ) {
			pcccStartCoalescing( bid, 20 );
		}
	}

	if( srec->exit_already_handled ) {
		delete_shadow_rec( srec );
		return;
	}

	job_id.cluster = srec->job_id.cluster;
	job_id.proc = srec->job_id.proc;

	if( srec->match ) {
		claim_id = srec->match->claim_id.claimId();
	}
	// store this in case srec is deleted before we need it
	srec_keep_claim_attributes = srec->keepClaimAttributes;

	//
	// If it is a SCHEDULER universe job, then we have a special
	// handler methods to take care of it
	//
	if (IsSchedulerUniverse(srec)) {
 		// scheduler universe process
		daemonCore->Kill_Family( pid );
		scheduler_univ_job_exit(pid,status,srec);
		delete_shadow_rec( pid );
		// even though this will get set correctly in
		// count_jobs(), try to keep it accurate here, too.
		if( SchedUniverseJobsRunning > 0 ) {
			SchedUniverseJobsRunning--;
		}
	} else {
		const char* name = NULL;
		//
		// Local Universe
		//
		if( IsLocalUniverse(srec) ) {
			daemonCore->Kill_Family(pid);
			srec_was_local_universe = true;
			name = "Local starter";
			//
			// Following the scheduler universe example, we need
			// to try to keep track of how many local jobs are
			// running in realtime
			//
			if ( this->LocalUniverseJobsRunning > 0 ) {
				this->LocalUniverseJobsRunning--;
			} else {
				dprintf(D_ALWAYS, "Warning: unexpected count for  local universe jobs: %d\n", LocalUniverseJobsRunning);
			}
		} else {
			// A real shadow
			name = "Shadow";
		} if ( daemonCore->Was_Not_Responding(pid) ) {
			// this shadow was killed by daemon core because it was hung.
			// make the schedd treat this like a Shadow Exception so job
			// just goes back into the queue as idle, but if it happens
			// to many times we relinquish the match.
			dprintf( D_ALWAYS,
					 "%s pid %d successfully killed because the %s was hung.\n",
					 name, pid, name );
			status = JOB_EXCEPTION << 8;
		}

		//
		// If the job exited with a status code, we can use
		// that to figure out what exactly we should be doing with
		// the job in the queue
		//
		if ( WIFEXITED(status) ) {
			int wExitStatus = WEXITSTATUS( status );
		    dprintf( D_ALWAYS,
			 		"%s pid %d for job %d.%d exited with status %d\n",
					 name, pid, srec->job_id.cluster, srec->job_id.proc,
					 wExitStatus );

			// Now call this method to perform the correct
			// action based on our status code
			this->jobExitCode( job_id, wExitStatus );

		 	// We never want to try to start jobs if we have
		 	// either of these exit codes
			 if ( wExitStatus == JOB_NO_MEM ||
			 	  wExitStatus == JOB_EXEC_FAILED ) {
				StartJobsFlag = FALSE;
			}

	 	} else if( WIFSIGNALED(status) ) {
 			// The job died with a signal, so there's not much
 			// that we can do for it
			dprintf( D_ERROR, "%s pid %d died with %s\n",
					 name, pid, daemonCore->GetExceptionString(status) );

			// If the shadow was killed (i.e. by this schedd) and
			// we are preserving the claim for reconnect, then
			// do not delete the claim.
			keep_claim = srec_keep_claim_attributes;
		}

		// We always want to delete the shadow record regardless
		// of how the job exited
		delete_shadow_rec( pid );

	} 

	//
	// If the job was a local universe job, we will want to
	// call count on it so that it can be marked idle again
	// if need be.
	//
	if ( srec_was_local_universe == true ) {
		JobQueueJob *job_ad = GetJobAd(job_id);
		count_a_job( job_ad, job_ad->jid, NULL);
	}

	// If we're not trying to shutdown, now that either an agent
	// or a shadow (or both) have exited, we should try to
	// start another job.
	if( ! ExitWhenDone && StartJobsFlag ) {
		if( !claim_id.empty() ) {
				// Try finding a new job for this claim.
			match_rec *mrec = scheduler.FindMrecByClaimID( claim_id.c_str() );
			if( mrec ) {
				this->StartJob( mrec );
			}
		} else {
			this->ExpediteStartJobs();
		}
	} else if( !keep_claim ) {
		if( !claim_id.empty() ) {
			DelMrec( claim_id.c_str() );
		}
	}
}

/**
 * Based on the exit status code for a job, we will preform
 * an appropriate action on the job in the queue. If an exception
 * also occured, we will report that ourselves as well.
 * 
 * Much of this logic was originally in child_exit() but it has been
 * moved into a separate function so that it can be called in cases
 * where the job isn't really exiting.
 * 
 * @param job_id - the identifier for the job
 * @param exit_code - we use this to determine the action to take on the job
 * @return true if the job was updated successfully
 * @see exit.h
 * @see condor_error_policy.h
 **/
bool
Scheduler::jobExitCode( PROC_ID job_id, int exit_code ) 
{
	bool ret = true; 
	
		// Try to get the shadow record.
		// If we are unable to get the srec, then we need to be careful
		// down in the logic below
	shadow_rec *srec = this->FindSrecByProcID( job_id );
	
		// Get job status.  Note we only except if there is no job status AND the job
		// is still in the queue, since we do not want to except if the job ad is gone
		// perhaps due to condor_rm -f.
	int q_status = -1;
	if (GetAttributeInt(job_id.cluster,job_id.proc,
						ATTR_JOB_STATUS,&q_status) < 0)	
	{
		if ( GetJobAd(job_id.cluster,job_id.proc) ) {
			// job exists, but has no status.  impossible!
			EXCEPT( "ERROR no job status for %d.%d in Scheduler::jobExitCode()!",
				job_id.cluster, job_id.proc );
		} else {
			// job does not exist anymore, so we have no work to do here.
			// since we have nothing to do in this function, return.
			return ret;
		}
	}

		// update exit code statistics
	time_t updateTime = time(nullptr);
	stats.Tick(updateTime);
	stats.JobsSubmitted = GetJobQueuedCount();
	stats.Autoclusters = autocluster.getNumAutoclusters();

	ScheddOtherStats * other_stats = NULL;
	if (OtherPoolStats.AnyEnabled()) {
		ClassAd * job_ad = GetJobAd( job_id.cluster, job_id.proc );
		if (job_ad) {
			other_stats = OtherPoolStats.Matches(*job_ad, updateTime);
			FreeJobAd(job_ad);
		}
		OtherPoolStats.Tick(updateTime);
	}
	#define OTHER for (ScheddOtherStats * po = other_stats; po; po = po->next) (po->stats)

	stats.JobsExited += 1;
	OTHER.JobsExited += 1;

		// get attributes that we will need to update goodput & badput statistics.
		//
	bool is_badput = false;
	bool is_goodput = false;
	int job_image_size = 0;
	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_IMAGE_SIZE, &job_image_size);
	time_t job_start_date = 0;
	time_t job_running_time = 0;
	if (0 == GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_CURRENT_START_DATE, &job_start_date))
		job_running_time = (updateTime - job_start_date);


		// We get the name of the daemon that had a problem for 
		// nice log messages...
	std::string daemon_name;
	if ( srec != NULL ) {
		daemon_name = ( IsLocalUniverse( srec ) ? "Local Starter" : "Shadow" );
	}

	MarkJobClean( job_id );
		//
		// If this boolean gets set to true, then we need to report
		// that an Exception occurred for the job.
		// This was broken out of the SWITCH statement below because
		// we might need to have extra logic to handle errors they
		// want to take an action but still want to report the Exception
		//
	bool reportException = false;

		//
		// Based on the job's exit code, we will perform different actions
		// on the job
		//
		// TODO If this was a reconnect shadow and it didn't indicate that
		//   reconnect succeeded and its exit code isn't
		//   JOB_RECONNECT_FAILED, then we don't know whether reconnect
		//   succeeded. We should decrement JobsRestartReconnectsAttempting,
		//   but which reconnect stat should we increment?
	if ( exit_code == JOB_RECONNECT_FAILED ) {
		// Treat JOB_RECONNECT_FAILED exit code just like JOB_SHOULD_REQUEUE,
		// except also update a few JobRestartReconnect statistics.
		exit_code = JOB_SHOULD_REQUEUE;
		// All of these conditions should be true if the exit code is
		// JOB_RECONNECT_FAILED, but let's not rely on the shadow and
		// local starter to be bug-free to keep our stats correct.
		if ( srec && srec->is_reconnect && !srec->reconnect_done ) {
			scheduler.stats.JobsRestartReconnectsAttempting -= 1;
			scheduler.stats.JobsRestartReconnectsFailed += 1;
			scheduler.stats.JobsRestartReconnectsBadput += job_running_time;
			srec->reconnect_done = true;
		}
	}
	if (exit_code == JOB_SHOULD_REQUEUE || exit_code == JOB_NOT_STARTED) {
		long long ival = 0;
		classad::Value val;
		JobQueueJob * job_ad = GetJobAd( job_id.cluster, job_id.proc );
		if (m_jobCoolDownExpr && job_ad->EvaluateExpr(m_jobCoolDownExpr, val) && val.IsNumber(ival) && ival > 0) {
			int cnt = 0;
			GetAttributeInt(job_id.cluster, job_id.proc, ATTR_NUM_JOB_COOL_DOWNS, &cnt);
			cnt++;
			SetAttributeInt(job_id.cluster, job_id.proc, ATTR_NUM_JOB_COOL_DOWNS, cnt);
			SetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_COOL_DOWN_EXPIRATION, time(nullptr) + ival);
			stats.JobsCoolDown += 1;
			OTHER.JobsCoolDown += 1;
		}
		else {
			int code = 0;
			GetAttributeInt(job_id.cluster, job_id.proc, ATTR_VACATE_REASON_CODE, &code);
			if (code > 0 && code < CONDOR_HOLD_CODE::VacateBase) {
				std::string reason;
				int subcode = 0;
				GetAttributeString(job_id.cluster, job_id.proc, ATTR_VACATE_REASON, reason);
				SetAttributeString(job_id.cluster, job_id.proc, ATTR_HOLD_REASON, reason.c_str());
				SetAttributeInt(job_id.cluster, job_id.proc, ATTR_HOLD_REASON_CODE, code);
				if (GetAttributeInt(job_id.cluster, job_id.proc, ATTR_VACATE_REASON_SUBCODE, &subcode) >= 0) {
					SetAttributeInt(job_id.cluster, job_id.proc, ATTR_HOLD_REASON_SUBCODE, subcode);
				}
				exit_code = JOB_SHOULD_HOLD;
			}
		}
	}
	switch( exit_code ) {
		case JOB_NO_MEM:
			this->swap_space_exhausted();
			stats.JobsShadowNoMemory += 1;
			OTHER.JobsShadowNoMemory += 1;
			// Fall through...
			//@fallthrough@
		case JOB_EXEC_FAILED:
				//
				// The calling function will make sure that
				// we don't try to start new jobs
				//
			stats.JobsExecFailed += 1;
			OTHER.JobsExecFailed += 1;
			break;

		case JOB_CKPTED:
		case JOB_SHOULD_REQUEUE:
		case JOB_NOT_STARTED:
			if( srec != NULL && !srec->removed && srec->match ) {
				// Don't delete matches we're trying to use for a now job.
				if(! srec->match->m_now_job.isJobKey()) {
					DelMrec(srec->match);
				}
			}
            switch (exit_code) {
               case JOB_CKPTED:
                  stats.JobsCheckpointed += 1;
                  OTHER.JobsCheckpointed += 1;
                  is_goodput = true;
                  break;
               case JOB_SHOULD_REQUEUE:
                  stats.JobsShouldRequeue += 1;
                  OTHER.JobsShouldRequeue += 1;
                  is_goodput = true;
                  break;
               case JOB_NOT_STARTED:
                  stats.JobsNotStarted += 1;
                  OTHER.JobsNotStarted += 1;
                  break;
               }
			break;

		case JOB_SHADOW_USAGE:
			EXCEPT( "%s exited with incorrect usage!", daemon_name.c_str() );
			break;

		case JOB_BAD_STATUS:
			EXCEPT( "%s exited because job status != RUNNING", daemon_name.c_str() );
			break;

		case JOB_SHOULD_REMOVE:
			dprintf( D_ALWAYS, "Removing job %d.%d\n",
					 job_id.cluster, job_id.proc );
				// If we have a shadow record, then set this flag 
				// so we treat this just like a condor_rm
			if ( srec != NULL ) {
				srec->removed = true;
			}
			stats.JobsShouldRemove += 1;
			OTHER.JobsShouldRemove += 1;
				// no break, fall through and do the action
				//@fallthrough@
		case JOB_NO_CKPT_FILE:
		case JOB_KILLED:
				// If the job isn't being HELD, we'll remove it
			if ( q_status != HELD && q_status != IDLE ) {
				set_job_status( job_id.cluster, job_id.proc, REMOVED );
			}
			is_badput = true;
			stats.JobsKilled += 1;
			OTHER.JobsKilled += 1;
			break;

		case JOB_EXITED_AND_CLAIM_CLOSING:
			if( srec != NULL && srec->match ) {
					// startd is not accepting more jobs on this claim
				srec->match->needs_release_claim = false;
				DelMrec(srec->match);
			}
			stats.JobsExitedAndClaimClosing += 1;
			OTHER.JobsExitedAndClaimClosing += 1;
			// no break, fall through
			//@fallthrough@
		case JOB_EXITED:
			dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
			stats.JobsExitedNormally += 1;
			OTHER.JobsExitedNormally += 1;
			stats.JobsCompleted += 1;
			OTHER.JobsCompleted += 1;
			is_goodput = true;
			// no break, fall through and do the action
			//@fallthrough@
		case JOB_COREDUMPED:
			if (JOB_COREDUMPED == exit_code) {
				stats.JobsCoredumped += 1;
				OTHER.JobsCoredumped += 1;
				is_badput = true;
			}
				// If the job isn't being HELD, set it to COMPLETED
			if ( q_status != HELD ) {
				set_job_status( job_id.cluster, job_id.proc, COMPLETED ); 
			}
			break;

		case JOB_MISSED_DEFERRAL_TIME: {

				//
				// Super Hack! - Missed Deferral Time
				// The job missed the time that it was suppose to
				// start executing, so we'll add an error message
				// to the remove reason so that it shows up in the userlog
				//
				// This is suppose to be temporary until we have some
				// kind of error handling in place for jobs
				// that never started
				// Andy Pavlo - 01.24.2006 - pavlo@cs.wisc.edu
				//
			std::string _error("\"Job missed deferred execution time\"");
			if ( SetAttribute( job_id.cluster, job_id.proc,
					  		  ATTR_HOLD_REASON, _error.c_str() ) < 0 ) {
				dprintf( D_ALWAYS, "WARNING: Failed to set %s to %s for "
						 "job %d.%d\n", ATTR_HOLD_REASON, _error.c_str(), 
						 job_id.cluster, job_id.proc );
			}
			if ( SetAttributeInt(job_id.cluster, job_id.proc,
								 ATTR_HOLD_REASON_CODE,
								 CONDOR_HOLD_CODE::MissedDeferredExecutionTime)
				 < 0 ) {
				dprintf( D_ALWAYS, "WARNING: Failed to set %s to %d for "
						 "job %d.%d\n", ATTR_HOLD_REASON_CODE,
						 CONDOR_HOLD_CODE::MissedDeferredExecutionTime,
						 job_id.cluster, job_id.proc );
			}
			dprintf( D_ALWAYS, "Job %d.%d missed its deferred execution time\n",
							job_id.cluster, job_id.proc );
		}
				// no break, fall through and do the action
				//@fallthrough@
		case JOB_SHOULD_HOLD: {
				// Regardless of the state that the job currently
				// is in, we'll put it on HOLD
				// But let a REMOVED job stay that way.
			if ( q_status != HELD && q_status != REMOVED && q_status != COMPLETED ) {
				dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
						 job_id.cluster, job_id.proc );
				set_job_status( job_id.cluster, job_id.proc, HELD );
			}
			is_badput = true;
			
				// If the job has a CronTab schedule, we will want
				// to remove cached scheduling object so that if
				// it is ever released we will always calculate a new
				// runtime for it. This prevents a job from going
				// on hold, then released only to fail again
				// because a new runtime wasn't calculated for it
			CronTab *cronTab = NULL;
			if ( this->cronTabs->lookup( job_id, cronTab ) >= 0 ) {
					// Delete the cached object				
				if ( cronTab ) {
					delete cronTab;
					this->cronTabs->remove(job_id);
					this->cronTabs->insert(job_id, nullptr);
				}
			} // CronTab

			if (JOB_MISSED_DEFERRAL_TIME == exit_code) {
				stats.JobsMissedDeferralTime += 1;
				OTHER.JobsMissedDeferralTime += 1;
			} else {
				stats.JobsShouldHold += 1;
				OTHER.JobsShouldHold += 1;
			}
			break;
		}

		case DPRINTF_ERROR:
			dprintf( D_ALWAYS,
					 "ERROR: %s had fatal error writing its log file\n",
					 daemon_name.c_str() );
			stats.JobsDebugLogError += 1;
			OTHER.JobsDebugLogError += 1;
			// We don't want to break, we want to fall through 
			// and treat this like a shadow exception for now.
			//@fallthrough@
		case JOB_EXCEPTION:
			if ( exit_code == JOB_EXCEPTION ){
				dprintf( D_ALWAYS,
						 "ERROR: %s exited with exception code!\n",
						 daemon_name.c_str() );
			}
			// We don't want to break, we want to fall through 
			// and treat this like a shadow exception for now.
			//@fallthrough@
		default:
				//
				// The default case is now a shadow exception in case ANYTHING
				// goes wrong with the shadow exit status
				//
			if ( ( exit_code != DPRINTF_ERROR ) &&
				 ( exit_code != JOB_EXCEPTION ) ) {
				dprintf( D_ALWAYS,
						 "ERROR: %s exited with unknown value %d!\n",
						 daemon_name.c_str(), exit_code );
			}
				// The logic to report a shadow exception has been
				// moved down below. We just set this flag to 
				// make sure we hit it
			reportException = true;
			stats.JobsExitException += 1;
			OTHER.JobsExitException += 1;
			is_badput = true;
			break;
	} // SWITCH
	
		// calculate badput and goodput statistics.
		//
	if ( ! is_goodput && ! is_badput) {
		stats.JobsAccumChurnTime += job_running_time;
		OTHER.JobsAccumChurnTime += job_running_time;
	} else {
		time_t job_pre_exec_time = 0;  // unless we see job_start_exec_date
		time_t job_post_exec_time = 0;
		time_t job_executing_time = 0;
		// this time is set in the shadow (remoteresource::beginExecution) so we don't need to worry
		// if we are talking to a shadow that supports it. the shadow and schedd should be from the same build.
		time_t job_start_exec_date = 0;
		if (0 == GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_CURRENT_START_EXECUTING_DATE, &job_start_exec_date)) {
			job_pre_exec_time = MAX(0, job_start_exec_date - job_start_date);
			job_executing_time = updateTime - MAX(job_start_date, job_start_exec_date);
			if (job_executing_time < 0) {
				stats.JobsWierdTimestamps += 1;
				OTHER.JobsWierdTimestamps += 1;
			}
		} else if (is_badput) {
			stats.JobsAccumChurnTime += job_running_time;
			OTHER.JobsAccumChurnTime += job_running_time;
		}
		// this time is also set in the shadow, but there is no gurantee that transfer output ever happened
		// so it may not exist. it's possible for transfer out date to be from a previous run, so we
		// have to make sure that it's at least later than the start time for this run before we use it.
		time_t job_start_xfer_out_date = 0;
		if (0 == GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_CURRENT_START_TRANSFER_OUTPUT_DATE, &job_start_xfer_out_date)
			&& job_start_xfer_out_date >= job_start_date) {
			job_post_exec_time = MAX(0, updateTime - job_start_xfer_out_date);
			job_executing_time = job_start_xfer_out_date - MAX(job_start_date, job_start_exec_date);
			if (job_executing_time < 0 || job_executing_time > updateTime) {
				stats.JobsWierdTimestamps += 1;
				OTHER.JobsWierdTimestamps += 1;
			}
		}

		stats.JobsAccumPreExecuteTime += job_pre_exec_time;
		stats.JobsAccumPostExecuteTime += job_post_exec_time;
		stats.JobsAccumExecuteTime += MAX(0, job_executing_time);
		stats.JobsAccumExecuteAltTime += MAX(0, job_running_time - (job_pre_exec_time + job_post_exec_time));

		if (is_goodput) {
			stats.JobsAccumRunningTime += job_running_time;
			stats.JobsCompletedSizes += (int64_t)job_image_size * 1024;
			stats.JobsCompletedRuntimes += job_running_time;
		} else if (is_badput) {
			stats.JobsAccumBadputTime += job_running_time;
			if (reportException) { stats.JobsAccumExceptionalBadputTime += job_running_time; }
			stats.JobsBadputSizes += (int64_t)job_image_size * 1024;
			stats.JobsBadputRuntimes += job_running_time;
		}
		if (other_stats) {
			OTHER.JobsAccumPreExecuteTime += job_pre_exec_time;
			OTHER.JobsAccumPostExecuteTime += job_post_exec_time;
			OTHER.JobsAccumExecuteTime += MAX(0, job_executing_time);
			OTHER.JobsAccumExecuteAltTime += MAX(0, job_running_time - (job_pre_exec_time + job_post_exec_time));
			if (is_goodput) {
				OTHER.JobsAccumRunningTime += job_running_time;
				OTHER.JobsCompletedSizes += (int64_t)job_image_size * 1024;
				OTHER.JobsCompletedRuntimes += job_running_time;
			} else if (is_badput) {
				OTHER.JobsAccumBadputTime += job_running_time;
				if (reportException) { OTHER.JobsAccumExceptionalBadputTime += job_running_time; }
				OTHER.JobsBadputSizes += (int64_t)job_image_size * 1024;
				OTHER.JobsBadputRuntimes += job_running_time;
			}
		}
	}

#undef OTHER

		// Report the ShadowException
		// This used to be in the default case in the switch statement
		// above, but we might need to do this in other cases in
		// the future
	if (reportException && srec != NULL) {
			// Record the shadow exception in the job ad.
		int num_excepts = 0;
		GetAttributeInt(job_id.cluster, job_id.proc,
						ATTR_NUM_SHADOW_EXCEPTIONS, &num_excepts);
		num_excepts++;
		SetAttributeInt(job_id.cluster, job_id.proc,
						ATTR_NUM_SHADOW_EXCEPTIONS, num_excepts, NONDURABLE);
		if (!srec->removed && srec->match) {
				// Record that we had an exception.  This function will
				// relinquish the match if we get too many exceptions 
			HadException(srec->match);
		}
	}
	return (ret);	
}

void
Scheduler::scheduler_univ_job_exit(int pid, int status, shadow_rec * srec)
{
	ASSERT(srec);

	PROC_ID job_id;
	job_id.cluster = srec->job_id.cluster;
	job_id.proc = srec->job_id.proc;

	std::string dir_name;
	formatstr(dir_name, "dir_%d_%d", job_id.cluster, job_id.proc);
	Directory execute_dir( LocalUnivExecuteDir, PRIV_CONDOR );
	if ( execute_dir.Find_Named_Entry( dir_name.c_str() ) ) {
		dprintf( D_FULLDEBUG, "Removing %s%c%s\n", LocalUnivExecuteDir, DIR_DELIM_CHAR, dir_name.c_str() );
		if (!execute_dir.Remove_Current_File()) {
			dprintf( D_ERROR, "Failed to remove execute directory %s%c%s for scheduler universe job.\n", LocalUnivExecuteDir, DIR_DELIM_CHAR, dir_name.c_str() );
		}
	} else {
		dprintf( D_ALWAYS, "Execute sub-directory (%s) missing in %s.\n", dir_name.c_str(), LocalUnivExecuteDir );
	}

	if ( daemonCore->Was_Not_Responding(pid) ) {
		// this job was killed by daemon core because it was hung.
		// just restart the job.
		dprintf(D_ALWAYS,
			"Scheduler universe job pid %d killed because "
			"it was hung - will restart\n"
			,pid);
		set_job_status( job_id.cluster, job_id.proc, IDLE ); 
		return;
	}

	bool exited = false;

	if(WIFEXITED(status)) {
		dprintf( D_ALWAYS,
				 "scheduler universe job (%d.%d) pid %d "
				 "exited with status %d\n", job_id.cluster,
				 job_id.proc, pid, WEXITSTATUS(status) );
		exited = true;
	} else if(WIFSIGNALED(status)) {
		dprintf( D_ALWAYS,
				 "scheduler universe job (%d.%d) pid %d died "
				 "with %s\n", job_id.cluster, job_id.proc, pid, 
				 daemonCore->GetExceptionString(status) );
	} else {
		dprintf( D_ALWAYS,
				 "scheduler universe job (%d.%d) pid %d exited "
				 "in some unknown way (0x%08x)\n", 
				 job_id.cluster, job_id.proc, pid, status);
	}

	if(srec->preempted) {
		// job exited b/c we removed or held it.  the
		// job's queue status will already be correct, so
		// we don't have to change anything else...
		WriteEvictToUserLog( job_id );
		return;
	}

	if(exited) {
		SetAttributeInt( job_id.cluster, job_id.proc, 
						ATTR_JOB_EXIT_STATUS, WEXITSTATUS(status) );
		SetAttribute( job_id.cluster, job_id.proc,
					  ATTR_ON_EXIT_BY_SIGNAL, "FALSE" );
		SetAttributeInt( job_id.cluster, job_id.proc,
						 ATTR_ON_EXIT_CODE, WEXITSTATUS(status) );
	} else {
			/* we didn't try to kill this job via rm or hold,
			   so either it killed itself or was killed from
			   the outside world.  either way, from our
			   perspective, it is now completed.
			*/
		SetAttribute( job_id.cluster, job_id.proc,
					  ATTR_ON_EXIT_BY_SIGNAL, "TRUE" );
		SetAttributeInt( job_id.cluster, job_id.proc,
						 ATTR_ON_EXIT_SIGNAL, WTERMSIG(status) );
	}

	int action;
	std::string reason;
	int reason_code;
	int reason_subcode;
	ClassAd * job_ad = GetJobAd( job_id.cluster, job_id.proc );
	if ( ! job_ad ) {
		dprintf( D_ALWAYS, "Scheduler universe job %d.%d has no job ad!\n",
			job_id.cluster, job_id.proc );
		return;
	}
	{
		UserPolicy policy;
		policy.Init();
		action = policy.AnalyzePolicy(*job_ad, PERIODIC_THEN_EXIT);
		policy.FiringReason(reason,reason_code,reason_subcode);
		if ( reason == "" ) {
			reason = "Unknown user policy expression";
		}
	}

	switch(action) {
		case REMOVE_FROM_QUEUE:
			scheduler_univ_job_leave_queue(job_id, status, job_ad);
			break;

		case STAYS_IN_QUEUE:
			set_job_status( job_id.cluster,	job_id.proc, IDLE ); 
			WriteRequeueToUserLog(job_id, status, reason.c_str());
			break;

		case HOLD_IN_QUEUE:
				// passing "false" to write_user_log, as
				// delete_shadow_rec will do that later
			holdJob(job_id.cluster, job_id.proc, reason.c_str(),
					reason_code, reason_subcode,
					true,false,false,false,false);
			break;

		case RELEASE_FROM_HOLD:
			dprintf(D_ALWAYS,
				"(%d.%d) Job exited.  User policy attempted to release "
				"job, but it wasn't on hold.  Allowing job to exit queue.\n", 
				job_id.cluster, job_id.proc);
			scheduler_univ_job_leave_queue(job_id, status, job_ad);
			break;

		case UNDEFINED_EVAL:
			dprintf( D_ALWAYS,
				"(%d.%d) Problem parsing user policy for job: %s.  "
				"Putting job on hold.\n",
				 job_id.cluster, job_id.proc, reason.c_str());
			holdJob(job_id.cluster, job_id.proc, reason.c_str(),
					reason_code, reason_subcode,
				true,false,false,true);
			break;

		default:
			dprintf( D_ALWAYS,
				"(%d.%d) User policy requested unknown action of %d. "
				"Putting job on hold. (Reason: %s)\n",
				 job_id.cluster, job_id.proc, action, reason.c_str());
			std::string reason2 = "Unknown action (";
			reason2 += std::to_string( action );
			reason2 += ") ";
			reason2 += reason;
			holdJob(job_id.cluster, job_id.proc, reason2.c_str(),
					CONDOR_HOLD_CODE::JobPolicyUndefined, 0,
				true,false,false,true);
			break;
	}

	FreeJobAd(job_ad);
	job_ad = NULL;
}


void
Scheduler::scheduler_univ_job_leave_queue(PROC_ID job_id, int status, ClassAd *ad)
{
	set_job_status( job_id.cluster,	job_id.proc, COMPLETED ); 
	WriteTerminateToUserLog( job_id, status );
	Email email;
	email.sendExit(ad, JOB_EXITED);
}

void
Scheduler::kill_zombie(int, PROC_ID* job_id )
{
	 mark_job_stopped( job_id );
}

/*
** The shadow running this job has died.  If things went right, the job
** has been marked as idle or completed as appropriate.
** However, if the shadow terminated abnormally, the job might still
** be marked as running (a zombie).  Here we check for that conditon,
** and mark the job with the appropriate status.
** 1/98: And if the job is maked completed or removed, we delete it
** from the queue.
*/
void
Scheduler::check_zombie(int pid, PROC_ID* job_id)
{
 
	int	  status = -1;
	
	if( GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS,
						&status) < 0 ) {
		dprintf(D_ALWAYS,"ERROR fetching job (%d.%d) status in check_zombie !\n",
				job_id->cluster,
				job_id->proc);
		return;
	}

	dprintf( D_FULLDEBUG, "Entered check_zombie( %d, 0x%p, st=%d )\n", 
			 pid, job_id, status );

	// set cur-hosts to zero
	SetAttributeInt( job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 0, NONDURABLE ); 

	switch( status ) {
	case RUNNING:
	case TRANSFERRING_OUTPUT: {
			//
			// If the job is running, we are in middle of executing
			// a graceful shutdown, and the job has a lease, then we 
			// do not want to go ahead and kill the zombie and 
			// mark the job as stopped. This ensures that when the schedd
			// comes back up we will reconnect the shadow to it if the
			// lease is still valid.
			//
		int lease = 0;
		GetAttributeInt( job_id->cluster, job_id->proc, ATTR_JOB_LEASE_DURATION, &lease );
		if ( ExitWhenDone && lease > 0 ) {
			dprintf( D_FULLDEBUG,	"Not marking job %d.%d as stopped because "
							"in graceful shutdown and job has a lease\n",
							job_id->cluster, job_id->proc );
			//
			// Otherwise, do the deed...
			//
		} else {
			kill_zombie( pid, job_id );
		}
		break;
	}
	case HELD:
		if( !scheduler.WriteHoldToUserLog(*job_id)) {
			dprintf( D_ALWAYS, 
					 "Failed to write hold event to the user log for job %d.%d\n",
					 job_id->cluster, job_id->proc );
		}
		break;
	case REMOVED:
		if( !scheduler.WriteAbortToUserLog(*job_id)) {
			dprintf( D_ALWAYS, 
					 "Failed to write abort event to the user log for job %d.%d\n",
					 job_id->cluster, job_id->proc ); 
		}
			// No break, fall through and do the deed...
			//@fallthrough@
	case COMPLETED:
	case JOB_STATUS_FAILED:
		DestroyProc( job_id->cluster, job_id->proc );
		break;
	default:
		break;
	}
		//
		// I'm not sure if this is proper place for this,
		// but I need to check to see if the job uses the CronTab
		// scheduling. If it does, then we'll call out to have the
		// next execution time calculated for it
		// 11.01.2005 - Andy - pavlo@cs.wisc.edu 
		//
	CronTab *cronTab = NULL;
	if ( this->cronTabs->lookup( *job_id, cronTab ) >= 0 ) {
			//
			// Set the force flag to true so it will always 
			// calculate the next execution time
			//
		ClassAd *job_ad = GetJobAd( job_id->cluster, job_id->proc );
		this->calculateCronTabSchedule( job_ad, true );
	}

	dprintf( D_FULLDEBUG, "Exited check_zombie( %d, 0x%p )\n", pid,
			 job_id );
}

void
cleanup_ckpt_files(int cluster, int proc, const char *owner)
{
	std::string	ckpt_name;
	std::string	owner_buf;

		/* In order to remove from the checkpoint server, we need to know
		 * the owner's name.  If not passed in, look it up now.
  		 */
	if ( owner == NULL ) {
		if ( GetAttributeString(cluster,proc,ATTR_OWNER,owner_buf) < 0 ) {
			dprintf(D_ALWAYS,"ERROR: cleanup_ckpt_files(): cannot determine owner for job %d.%d\n",cluster,proc);
		} else {
			owner = owner_buf.c_str();
		}
	}

	ClassAd * jobAd = GetJobAd(cluster, proc);
	if( jobAd ) {
		std::string checkpointDestination;
		if( jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination) ) {
			scheduler.doCheckpointCleanUp( cluster, proc, jobAd );
		}
		SpooledJobFiles::removeJobSpoolDirectory(jobAd);
		FreeJobAd(jobAd);
	}
}


size_t pidHash(const int &pid)
{
	return pid;
}


// initialize the configuration parameters and classad.  Since we call
// this again when we reconfigure, we have to be careful not to leak
// memory. 
void
Scheduler::Init()
{
	char*					tmp;
	static	int				schedd_name_in_config = 0;
	static  bool			first_time_in_init = true;

		////////////////////////////////////////////////////////////////////
		// Grab all the essential parameters we need from the config file.
		////////////////////////////////////////////////////////////////////

    stats.Reconfig();

	if (first_time_in_init) {
		if (param_boolean("USE_JOBSETS", false)) {
			ASSERT(jobSets == nullptr);
			jobSets = new JobSets();
			ASSERT(jobSets);
		}

		// secret knob.  set to FALSE to cause persistent user records to be deleted on startup
		EnablePersistentOwnerInfo = param_boolean("PERSISTENT_USER_RECORDS", true);

		// UID_DOMAIN is a restart knob for the SCHEDD
		UidDomain = param( "UID_DOMAIN" );

		// setup the global attribute name we will use as the canonical 'owner' of a job
		// historically this was "Owner", but in 8.9 we switch to "User" so that we use
		// the fully qualified name and can handle jobs from other domains in the schedd
		// NOTE: that when user_is_the_new_owner is true, the code conflicts badly with USERREC_NAME_IS_FULLY_QUALIFIED
		user_is_the_new_owner = false; // param_boolean("USER_IS_THE_NEW_OWNER", false);
		if (user_is_the_new_owner) {
			ownerinfo_attr_name = ATTR_USER;
		} else {
			ownerinfo_attr_name = ATTR_OWNER;
		}
	}
	if (jobSets) {
		jobSets->reconfig();
	}

	if( Spool ) free( Spool );
	if( !(Spool = param("SPOOL")) ) {
		EXCEPT( "No spool directory specified in config file" );
	}

	if( CondorAdministrator ) free( CondorAdministrator );
	if( ! (CondorAdministrator = param("CONDOR_ADMIN")) ) {
		dprintf(D_FULLDEBUG, 
			"WARNING: CONDOR_ADMIN not specified in config file" );
	}


	ignore_domain_mismatch_when_setting_owner = param_boolean("TRUST_UID_DOMAIN", false);
	ignore_domain_for_OwnerCheck = param_boolean("IGNORE_DOMAIN_FOR_JOB_OWNER_CHECK", true);
	warn_domain_for_OwnerCheck = param_boolean("WARN_DOMAIN_FOR_JOB_OWNER_CHECK", true);
	job_owner_must_be_UidDomain = param_boolean("JOB_OWNER_MUST_BE_FROM_UID_DOMAIN", false);
	allow_submit_from_known_users_only = param_boolean("ALLOW_SUBMIT_FROM_KNOWN_USERS_ONLY", false);

	// Secret knob to set the geneneric CE domain value
	// when QmgmtSetEffectiveOwner is passed a value with this domain, the value is re-written to UID_DOMAIN
	param(GenericCEDomain, "GENERIC_CE_DOMAIN", "users.htcondor.org");

	param(AccountingDomain, "ACCOUNTING_DOMAIN");

		////////////////////////////////////////////////////////////////////
		// Grab all the optional parameters from the config file.
		////////////////////////////////////////////////////////////////////

	if( schedd_name_in_config ) {
		tmp = param( "SCHEDD_NAME" );
		free( Name );
		Name = build_valid_daemon_name( tmp );
		free( tmp );
	} else {
		if( ! Name ) {
			tmp = param( "SCHEDD_NAME" );
			if( tmp ) {
				Name = build_valid_daemon_name( tmp );
				schedd_name_in_config = 1;
				free( tmp );
			} else {
				Name = default_daemon_name();
			}
		}
	}

	m_include_default_flock_param = param_boolean("FLOCK_BY_DEFAULT", true);

	dprintf( D_FULLDEBUG, "Using name: %s\n", Name );

	if( AccountantName ) free( AccountantName );
	if( ! (AccountantName = param("ACCOUNTANT_HOST")) ) {
		dprintf( D_FULLDEBUG, "No Accountant host specified in config file\n" );
	}

	InitJobHistoryFile("HISTORY", "PER_JOB_HISTORY_DIR"); // or re-init it, as the case may be

		//
		// We keep a copy of the last interval
		// If it changes, then we need update all the job ad's
		// that use it (job deferral, crontab). 
		// Except that this update must be after the queue is initialized
		//
	double orig_SchedDInterval = SchedDInterval.getMaxInterval();

	SchedDInterval.setDefaultInterval( param_integer( "SCHEDD_INTERVAL", 300 ) );
	SchedDInterval.setMaxInterval( SchedDInterval.getDefaultInterval() );

	SchedDInterval.setMinInterval( param_integer("SCHEDD_MIN_INTERVAL",5) );
	SchedDInterval.setInitialInterval( 0 );

	SchedDInterval.setTimeslice( param_double("SCHEDD_INTERVAL_TIMESLICE",0.05,0,1) );

		//
		// We only want to update if this is a reconfig
		// If the schedd is just starting up, there isn't a job
		// queue at this point
		//
	
	if ( !first_time_in_init ){
		double diff = this->SchedDInterval.getMaxInterval()
			- orig_SchedDInterval;
		if(diff < -1e-4 || diff > 1e-4) {
			// 
			// This will only update the job's that have the old
			// ScheddInterval attribute defined
			//
			WalkJobQueue(updateSchedDInterval);
		}
	}

		// Delay sending negotiation request if we are spending more
		// than this amount of time negotiating.  This is currently an
		// intentionally undocumented knob, because it's behavior is
		// not at all well defined, given that the negotiator can
		// initiate negotiation at any time.  We also hope to
		// upgrade the negotiation protocol to avoid blocking the
		// schedd, which should make this all unnecessary.
	m_negotiate_timeslice.setTimeslice( param_double("SCHEDD_NEGOTIATE_TIMESLICE",0.1) );
		// never delay negotiation request longer than this amount of time
	m_negotiate_timeslice.setMaxInterval( SchedDInterval.getMaxInterval() );

	// default every 24 hours
	QueueCleanInterval = param_integer( "QUEUE_CLEAN_INTERVAL",24*60*60 );

	// default every hour
	WallClockCkptInterval = param_integer( "WALL_CLOCK_CKPT_INTERVAL",60*60 );

	JobStartDelay = param_integer( "JOB_START_DELAY", 0 );
	
	JobStartCount =	param_integer(
						"JOB_START_COUNT",			// name
						DEFAULT_JOB_START_COUNT,	// default value
						DEFAULT_JOB_START_COUNT		// min value
					);

	MaxNextJobDelay = param_integer( "MAX_NEXT_JOB_START_DELAY", 60*10 );

	JobsThisBurst = -1;

		// Estimate that we can afford to use 80% of memory for shadows
		// and each running shadow requires 800k of private memory.
		// This works out to about 1 shadow per MB of total memory.
		// We don't use SHADOW_SIZE_ESTIMATE here, because until 7.4,
		// that was explicitly set to 1800k in the default config file.
	int default_max_jobs_running = sysapi_phys_memory_raw_no_param();

		// Under Linux (not sure about other OSes), the default TCP
		// ephemeral port range is 32768-61000.  Each shadow needs 2
		// ports, sometimes 3, and depending on how fast shadows are
		// finishing, there will be some ports in CLOSE_WAIT, so the
		// following is a conservative upper bound on how many shadows
		// we can run.  Would be nice to check the ephemeral port
		// range directly.
	if( default_max_jobs_running > 10000 || default_max_jobs_running <= 0 ) {
		default_max_jobs_running = 10000;
	}
#ifdef WIN32
		// Apparently under Windows things don't scale as well.
		// Under 64-bit, we should be able to scale higher, but
		// we currently don't have a way to detect that.
	if( default_max_jobs_running > 200) {
		default_max_jobs_running = 200;
	}
#endif

	MaxJobsRunning = param_integer("MAX_JOBS_RUNNING",default_max_jobs_running);

	AllowLateMaterialize = param_boolean("SCHEDD_ALLOW_LATE_MATERIALIZE", false);
	MaxMaterializedJobsPerCluster = param_integer("MAX_MATERIALIZED_JOBS_PER_CLUSTER", MaxMaterializedJobsPerCluster);
	NonDurableLateMaterialize = param_boolean("SCHEDD_NON_DURABLE_LATE_MATERIALIZE", true);

	m_userRecDefaultsAd.Clear();
	auto_free_ptr user_def_props(param("SCHEDD_USER_DEFAULT_PROPERTIES"));
	if (user_def_props) {
		if ( ! initAdFromString(user_def_props, m_userRecDefaultsAd)) {
			dprintf(D_ERROR, "Warning: SCHEDD_USER_DEFAULT_PROPERTIES value has errors.\n");
		}
		UserRecFixupDefaultsAd(m_userRecDefaultsAd);
		user_def_props.clear();
	}
	// TODO: add SCHEDD_USER_CREATION_TRANSFORM ?

	m_extendedSubmitCommands.Clear();
	auto_free_ptr extended_cmds(param("EXTENDED_SUBMIT_COMMANDS"));
	if (extended_cmds) {
		initAdFromString(extended_cmds, m_extendedSubmitCommands);
		extended_cmds.clear();
	}
	param(m_extendedSubmitHelpFile, "EXTENDED_SUBMIT_HELPFILE");

	EnableJobQueueTimestamps = param_boolean("SCHEDD_JOB_QUEUE_TIMESTAMPS", false);

		// Limit number of simultaenous connection attempts to startds.
		// This avoids the schedd getting so busy authenticating with
		// startds that it can't keep up with shadows.
		// note: the special value 0 means 'unlimited'
	max_pending_startd_contacts = param_integer( "MAX_PENDING_STARTD_CONTACTS", 0, 0 );


#ifdef USE_VANILLA_START
		// Start "vanilla" universe expression
		// All jobs that make a shadow (i.e. not Local, Grid or Scheduler universe)
		// will evaluate this expression in addition to their requirements
		// and not activate a claim if it does not evaluate to true.
	vanilla_start_expr.clear(); // clear explicitly because set(NULL) won't clear.. is that a bug?
	//vanilla_start_expr_is_const = -1; // force it to recalculate
	//vanilla_start_expr_const_value = true;
	vanilla_start_expr.set(param("START_VANILLA_UNIVERSE"));
	int expr_error = 0;
	vanilla_start_expr.Expr(&expr_error); // force it to parse, a noop if there is no expression.
	if (expr_error) {
		dprintf(D_ALWAYS, "START_VANILLA_UNIVERSE expression '%s' does not parse, defaulting to TRUE\n", vanilla_start_expr.c_str()); 
		vanilla_start_expr.clear();
	}
#endif

	// there are some attribs that we always want in the significant attributes list,
	// regardless of what the negotiators want. That minimum set is 4 things, plus whatever 
	// job references there are in the vanilla_start_expr
	MinimalSigAttrs.clear();
	if ( ! vanilla_start_expr.empty()) {
		GetAttrRefsOfScope(vanilla_start_expr.Expr(), MinimalSigAttrs, "JOB");
	}
	MinimalSigAttrs.insert(ATTR_REQUIREMENTS);
	MinimalSigAttrs.insert(ATTR_RANK);
#ifdef NO_DEPRECATED_NICE_USER
	MinimalSigAttrs.insert(ATTR_NICE_USER);
#endif
	MinimalSigAttrs.insert(ATTR_CONCURRENCY_LIMITS);
	MinimalSigAttrs.insert(ATTR_FLOCK_TO);

		//
		// Start Local Universe Expression
		// This will be added into the requirements expression for
		// the schedd to know whether we can start a local job 
		// 
	ExprTree *tmp_expr;
	if ( this->StartLocalUniverse ) {
		free( this->StartLocalUniverse );
		this->StartLocalUniverse = NULL;
	}
	tmp = param( "START_LOCAL_UNIVERSE" );
	if ( tmp && ParseClassAdRvalExpr( tmp, tmp_expr ) == 0 ) {
		this->StartLocalUniverse = tmp;
		tmp = NULL;
		delete tmp_expr;
	} else {
		// Default Expression
		this->StartLocalUniverse = strdup( "TotalLocalJobsRunning < 200" );
		dprintf( D_FULLDEBUG, "Using default expression for "
				 "START_LOCAL_UNIVERSE: %s\n", this->StartLocalUniverse );
	}
	free( tmp );

		//
		// Start Scheduler Universe Expression
		// This will be added into the requirements expression for
		// the schedd to know whether we can start a scheduler job 
		// 
	if ( this->StartSchedulerUniverse ) {
		free( this->StartSchedulerUniverse );
		this->StartSchedulerUniverse = NULL;
	}
	tmp = param( "START_SCHEDULER_UNIVERSE" );
	if ( tmp && ParseClassAdRvalExpr( tmp, tmp_expr ) == 0 ) {
		this->StartSchedulerUniverse = tmp;
		tmp = NULL;
		delete tmp_expr;
	} else {
		// Default Expression
		this->StartSchedulerUniverse = strdup( "TotalSchedulerJobsRunning < 500" );
		dprintf( D_FULLDEBUG, "Using default expression for "
				 "START_SCHEDULER_UNIVERSE: %s\n", this->StartSchedulerUniverse );
	}
	free( tmp );

	// Job Cool Down expression
	delete m_jobCoolDownExpr;
	m_jobCoolDownExpr = nullptr;
	tmp = param("SYSTEM_ON_VACATE_COOL_DOWN");
	if (tmp) {
		ParseClassAdRvalExpr(tmp, m_jobCoolDownExpr);
		free(tmp);
	}

	MaxRunningSchedulerJobsPerOwner = param_integer("MAX_RUNNING_SCHEDULER_JOBS_PER_OWNER", 200);
	MaxJobsSubmitted = param_integer("MAX_JOBS_SUBMITTED",INT_MAX);
	MaxJobsPerOwner = param_integer( "MAX_JOBS_PER_OWNER", INT_MAX );
	MaxJobsPerSubmission = param_integer( "MAX_JOBS_PER_SUBMISSION", INT_MAX );


	NegotiateAllJobsInCluster = param_boolean_crufty("NEGOTIATE_ALL_JOBS_IN_CLUSTER", false);

	STARTD_CONTACT_TIMEOUT = param_integer("STARTD_CONTACT_TIMEOUT",45);

		// Decide the directory we should use for the execute
		// directory for local universe starters.  Create it if it
		// doesn't exist, fix the permissions (1777 on UNIX), and, if
		// it's the first time we've hit this method (on startup, not
		// reconfig), we remove any subdirectories that might have
		// been left due to starter crashes, etc.
	initLocalStarterDir();

	m_use_startd_for_local = param_boolean("SCHEDD_USES_STARTD_FOR_LOCAL_UNIVERSE", false);

	if (m_use_startd_for_local) {
		launch_local_startd();
	}

	if (matches == NULL) {
		matches = new HashTable <std::string, match_rec *> (hashFunction);
		matchesByJobID =
			new HashTable<PROC_ID, match_rec *>(hashFuncPROC_ID);
	}

	char *flock_collector_hosts, *flock_negotiator_hosts;
	flock_collector_hosts = param( "FLOCK_COLLECTOR_HOSTS" );
	flock_negotiator_hosts = param( "FLOCK_NEGOTIATOR_HOSTS" );

	if( flock_collector_hosts ) {
		FlockCollectors.clear();
		for (const auto& name : StringTokenIterator(flock_collector_hosts)) {
			FlockCollectors.emplace_back(name.c_str());
		}
		MaxFlockLevel = FlockCollectors.size();
		MinFlockLevel = param_integer("MIN_FLOCK_LEVEL", 0, 0, MaxFlockLevel);

		FlockNegotiators.clear();
		size_t idx = 0;
		for (const auto& name : StringTokenIterator(flock_negotiator_hosts)) {
			if (idx > FlockCollectors.size()) {
				break;
			}
			FlockNegotiators.emplace_back(DT_NEGOTIATOR, name.c_str(), FlockCollectors[idx].name());
			idx++;
		}
		if( FlockCollectors.size() != FlockNegotiators.size() ) {
			dprintf(D_ALWAYS, "FLOCK_COLLECTOR_HOSTS and "
					"FLOCK_NEGOTIATOR_HOSTS lists are not the same size."
					"Flocking disabled.\n");
			MaxFlockLevel = 0;
			MinFlockLevel = 0;
		}
	}
	if (flock_collector_hosts) free(flock_collector_hosts);
	if (flock_negotiator_hosts) free(flock_negotiator_hosts);

	FlockExtra.clear();

	// fetch all params that start with SCHEDD_COLLECT_STATS_FOR_ and
	// use them to define other scheduler stats pools.  the value of this
	// param should be a classad expression that evaluates agains the job ad
	// to a boolean.
	//
	{
		Regex re; int errcode = 0, erroffset = 0;
		ASSERT(re.compile("schedd_collect_stats_(by|for)_(.+)", &errcode, &erroffset, PCRE2_CASELESS));
		
		OtherPoolStats.DisableAll();

		std::vector<std::string> names;
		if (param_names_matching(re, names)) {

			for (size_t ii = 0; ii < names.size(); ++ii) {

				//dprintf(D_FULLDEBUG, "Found %s\n", names[ii]);
				const std::string name = names[ii];
				char * filter = param(names[ii].c_str());
				if ( ! filter) {
					dprintf(D_ALWAYS, "Ignoring param '%s' : value is empty\n", names[ii].c_str());
					continue;
				}

				// the pool prefix will be the first submatch of the regex of the param name.
				// unfortunately it's been lowercased by the time we get here, so we can't
				// let the user choose the case, just capitalize it and use it as the prefix
				std::vector<std::string> groups;
				if (re.match(name, &groups)) {
					std::string byorfor = groups[1]; // this will by "by" or "for"
					std::string other = groups[2]; // this will be lowercase
					if (isdigit(other[0])) {
						// can't start atributes with a digit, start with _ instead
						formatstr(other,"_%s", groups[2].c_str());
					} else {
						other.at(0) = toupper(other[0]); // capitalize it.
					}

					// for 'by' type stats, we also allow an expiration.
					time_t lifetime = 0;
					const int one_week = 60*60*24*7; // 60sec*60min*24hr*7day
					bool by = (MATCH == strcasecmp(byorfor.c_str(), "by"));
					if (by) {
						std::string expires_name;
						formatstr(expires_name, "schedd_expire_stats_by_%s", other.c_str());
						lifetime = (time_t)param_integer(expires_name.c_str(), one_week);
					}

					dprintf(D_FULLDEBUG, "Collecting stats %s '%s' life=%" PRId64 " trigger is %s\n", 
					        byorfor.c_str(), other.c_str(), (int64_t)lifetime, filter);
					OtherPoolStats.Enable(other.c_str(), filter, by, lifetime);
				}
				free(filter);
			}
		}

		// TJ: 8.3.6 automatically create a collection of stats by owner if there is not one already.
		// but only publish the aggregate counters in the schedd ad.
		if ( ! OtherPoolStats.Contains("Owner")) {
			OtherPoolStats.Enable("Owner", "Owner", true, 60*60*24*7);
		}

		OtherPoolStats.RemoveDisabled();
		OtherPoolStats.Reconfig(stats.RecentWindowMax, stats.RecentWindowQuantum);
	}

	/* default 5 megabytes */
	ReservedSwap = param_integer( "RESERVED_SWAP", 0 );
	ReservedSwap *= 1024;

	/* Value specified in kilobytes */
	ShadowSizeEstimate = param_integer( "SHADOW_SIZE_ESTIMATE",DEFAULT_SHADOW_SIZE );

	alive_interval = param_integer("ALIVE_INTERVAL",300,0);
	if( alive_interval > leaseAliveInterval ) {
			// adjust alive_interval to shortest interval of jobs in the queue
		alive_interval = leaseAliveInterval;
	}
		// Don't allow the user to specify an alive interval larger
		// than leaseAliveInterval, or the startd may start killing off
		// jobs before ATTR_JOB_LEASE_DURATION has passed, thereby screwing
		// up the operation of the disconnected shadow/starter feature.

		//
		// CronTab Table
		// We keep a list of proc_id's for jobs that define a cron
		// schedule for exection. We first get the pointer to the 
		// original table, and then instantiate a new one
		//
	HashTable<PROC_ID, CronTab*> *origCronTabs = this->cronTabs;
	this->cronTabs = new HashTable<PROC_ID, CronTab*>(
												hashFuncPROC_ID );
		//
		// Now if there was a table from before, we will want
		// to copy all the proc_id's into our new table. We don't
		// keep the CronTab objects because they'll get reinstantiated
		// later on when the Schedd tries to calculate the next runtime
		// We have a little safety check to make sure that the the job 
		// actually exists before adding it back in
		//
		// Note: There could be a problem if MaxJobsRunning is substaintially
		// less than what it was from before on a reconfig, and in which case
		// the new cronTabs hashtable might not be big enough to store all
		// the old jobs. This unlikely for now because I doubt anybody will
		// be submitting that many CronTab jobs, but it is still possible.
		// See the comments about about automatically resizing HashTable's
		//
	if ( origCronTabs != NULL ) {
		CronTab *cronTab;
		PROC_ID id;
		origCronTabs->startIterations();
		while ( origCronTabs->iterate( id, cronTab ) == 1 ) {
			if ( cronTab ) delete cronTab;
			ClassAd *cronTabAd = GetJobAd( id.cluster, id.proc );
			if ( cronTabAd ) {
				this->cronTabs->insert( id, NULL );
			}
		} // WHILE
		delete origCronTabs;
	}

	MaxExceptions = param_integer("MAX_SHADOW_EXCEPTIONS", 2);

	PeriodicExprInterval.setMinInterval( param_integer("PERIODIC_EXPR_INTERVAL", 60) );

	PeriodicExprInterval.setMaxInterval( param_integer("MAX_PERIODIC_EXPR_INTERVAL", 1200) );

	PeriodicExprInterval.setTimeslice( param_double("PERIODIC_EXPR_TIMESLICE", 0.01,0,1) );

	RequestClaimTimeout = param_integer("REQUEST_CLAIM_TIMEOUT",60*30);

	int int_val = param_integer( "JOB_IS_FINISHED_INTERVAL", 0, 0 );
	job_is_finished_queue.setPeriod( int_val );
	int_val = param_integer( "JOB_IS_FINISHED_COUNT", 1, 1 );
	job_is_finished_queue.setCountPerInterval(int_val);

	JobStopDelay = param_integer( "JOB_STOP_DELAY", 0, 0 );
	stop_job_queue.setPeriod( JobStopDelay );

	JobStopCount = param_integer( "JOB_STOP_COUNT", 1, 1 );
	stop_job_queue.setCountPerInterval( JobStopCount );

	m_protected_url_map.clear();
	auto_free_ptr protectedUrlMapFile(param("PROTECTED_URL_TRANSFER_MAPFILE"));
	if (protectedUrlMapFile) {
		m_protected_url_map.ParseCanonicalizationFile(protectedUrlMapFile.ptr(), true, true, true);
	}
		////////////////////////////////////////////////////////////////////
		// Initialize the queue managment code
		////////////////////////////////////////////////////////////////////

	InitQmgmt();


		//////////////////////////////////////////////////////////////
		// Initialize our classad
		//////////////////////////////////////////////////////////////
	if( m_adBase ) delete m_adBase;
	m_adBase = new ClassAd();

    // first put attributes into the Base ad that we want to
    // share between the Scheduler AD and the Submitter Ad
    //
    m_adBase->Assign(ATTR_SCHEDD_IP_ADDR, daemonCore->publicNetworkIpAddr());
        // Tell negotiator to send us the startd ad
		// As of 7.1.3, the negotiator no longer pays attention to this
		// attribute; it _always_ sends the resource request ad.
		// For backward compatibility with older negotiators, we still set it.
    m_adBase->Assign(ATTR_WANT_RESOURCE_AD, true);

       // add the basic daemon core attribs
    daemonCore->publish(m_adBase);

	// make a base add for use with chained submitter ads as a copy of the schedd ad
	// and fill in some standard attribs that will change only on reconfig.
	// the rest are added in count_jobs()
	// m_adSchedd->CopyFrom( * m_adBase ) is the obvious thing to do here,
	// but explodes trying to unchain the base ad?  Use a placement new to
	// maintain the lifetime of the pointer, instead.
	if( m_adSchedd ) {
		m_adSchedd->~ClassAd();
		m_adSchedd = new (m_adSchedd) ClassAd( * m_adBase );
	} else {
		m_adSchedd = new ClassAd( * m_adBase );
	}
	SetMyTypeName(*m_adSchedd, SCHEDD_ADTYPE);
	m_adSchedd->Assign(ATTR_NAME, Name);

	// Record the transfer queue expression so the negotiator can predict
	// which transfer queue a job will use if it starts in the schedd.
	std::string transfer_queue_expr_str;
	param(transfer_queue_expr_str, "TRANSFER_QUEUE_USER_EXPR");
	classad::ClassAdParser parser;
	classad::ExprTree *transfer_queue_expr = NULL;
	if (parser.ParseExpression(transfer_queue_expr_str, transfer_queue_expr) && transfer_queue_expr)
	{
		dprintf(D_FULLDEBUG, "TransferQueueUserExpr = %s\n", transfer_queue_expr_str.c_str());
	}
	else
	{
		dprintf(D_ALWAYS, "TransferQueueUserExpr is set to an invalid expression: %s\n", transfer_queue_expr_str.c_str());
	}
	m_adSchedd->Insert(ATTR_TRANSFER_QUEUE_USER_EXPR, transfer_queue_expr);

	std::string curb_expr_str;
	param(curb_expr_str, "CURB_MATCHMAKING");
	classad::ExprTree *curb_expr = NULL;
	if (parser.ParseExpression(curb_expr_str, curb_expr) && curb_expr)
	{
		dprintf(D_FULLDEBUG, "CurbMatchmaking = %s\n", curb_expr_str.c_str());
		m_adSchedd->Insert(ATTR_CURB_MATCHMAKING, curb_expr);
	}
	else
	{
		dprintf(D_ALWAYS, "CURB_MATCHMAKING is set to an invalid expression: %s\n", curb_expr_str.c_str());
	}

	// reset the ConfigSuperUser flag on JobQueueUserRec records to "don't know"
	// this will trigger a fixup from config the next time we try and do something as that user
	// Note that this will have no effect on records that are inherently super
	for (auto &oi : OwnersInfo) { oi.second->setStaleConfigSuper(); }

	// This is foul, but a SCHEDD_ADTYPE _MUST_ have a NUM_USERS attribute
	// (see condor_classad/classad.C
	// Since we don't know how many there are yet, just say 0, it will get
	// fixed in count_job() -Erik 12/18/2006
	m_adSchedd->Assign(ATTR_NUM_USERS, 0);

	char *collectorHost = NULL;
	collectorHost  = param("COLLECTOR_HOST");
	if (collectorHost) {
		m_adSchedd->Assign(ATTR_COLLECTOR_HOST, collectorHost); 
		free(collectorHost);
	}

		// Now create another command port to be used exclusively by shadows.
		// Stash the sinfull string of this new command port in MyShadowSockName.
	if ( ! MyShadowSockName ) {
		shadowCommandrsock = new ReliSock;
		shadowCommandssock = new SafeSock;

		if ( !shadowCommandrsock || !shadowCommandssock ) {
			EXCEPT("Failed to create Shadow Command socket");
		}
		// Note: BindAnyLocalCommandPort() is in daemon core
		if ( !BindAnyLocalCommandPort(shadowCommandrsock,shadowCommandssock)) {
			EXCEPT("Failed to bind Shadow Command socket");
		}
		if ( !shadowCommandrsock->listen() ) {
			EXCEPT("Failed to post a listen on Shadow Command socket");
		}
		daemonCore->Register_Command_Socket( (Stream*)shadowCommandrsock );
		daemonCore->Register_Command_Socket( (Stream*)shadowCommandssock );

		MyShadowSockName = strdup( shadowCommandrsock->get_sinful() );

		sent_shadow_failure_email = FALSE;

		// In the inherit buffer we pass to our children, tell them to use
		// the shadow command socket.
		daemonCore->SetInheritParentSinful( MyShadowSockName );
	}

		// Startup the cron logic (only do it once, though)
	if ( ! CronJobMgr ) {
		CronJobMgr = new ScheddCronJobMgr( );
		CronJobMgr->Initialize( "schedd" );
	}

	m_xfer_queue_mgr.InitAndReconfig();
	m_have_xfer_queue_contact = m_xfer_queue_mgr.GetContactInfo(MyShadowSockName, m_xfer_queue_contact);

		/* Code to handle GRIDMANAGER_SELECTION_EXPR.  If set, we need to (a) restart
		 * running gridmanagers if the setting changed value, and (b) parse the
		 * expression and stash the parsed form (so we don't reparse over and over).
		 */
	char * expr = param("GRIDMANAGER_SELECTION_EXPR");
	if (m_parsed_gridman_selection_expr) {
		delete m_parsed_gridman_selection_expr;
		m_parsed_gridman_selection_expr = NULL;	
	}
	if ( expr ) {
		std::string temp;
		formatstr(temp,"string(%s)",expr);
		free(expr);
		expr = strdup(temp.c_str());
		ParseClassAdRvalExpr(temp.c_str(),m_parsed_gridman_selection_expr);	
			// if the expression in the config file is not valid, 
			// the m_parsed_gridman_selection_expr will still be NULL.  in this case,
			// pretend like it isn't set at all in the config file.
		if ( m_parsed_gridman_selection_expr == NULL ) {
			dprintf(D_ALWAYS,
				"ERROR: ignoring GRIDMANAGER_SELECTION_EXPR (%s) - failed to parse\n",
				expr);
			free(expr);
			expr = NULL;
		} 
	}
		/* If GRIDMANAGER_SELECTION_EXPR changed, we need to restart all running
		 * gridmanager asap.  Be careful to consider not only a changed expr, but
		 * also the presence of a expr when one did not exist before, and vice-versa.
		 */
	if ( (expr && !m_unparsed_gridman_selection_expr) ||
		 (!expr && m_unparsed_gridman_selection_expr) ||
		 (expr && m_unparsed_gridman_selection_expr && 
		       strcmp(m_unparsed_gridman_selection_expr,expr)!=0) )
	{
			/* GRIDMANAGER_SELECTION_EXPR changed, we need to kill off all running
			 * running gridmanagers asap so they can be restarted w/ the new expression.
			 */
		GridUniverseLogic::shutdown_fast();
	}
	if (m_unparsed_gridman_selection_expr) {
		free(m_unparsed_gridman_selection_expr);
	}
	m_unparsed_gridman_selection_expr = expr;
		/* End of support for  GRIDMANAGER_SELECTION_EXPR */

	std::string job_machine_attrs_str;
	param(job_machine_attrs_str,"SYSTEM_JOB_MACHINE_ATTRS");
	m_job_machine_attrs = split(job_machine_attrs_str);

	m_job_machine_attrs_history_length = param_integer("SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH",1,0);

	int max_history_concurrency = param_integer("HISTORY_HELPER_MAX_CONCURRENCY", 50);
	HistoryQue.setup(1000, max_history_concurrency);

    m_userlog_file_cache_max = param_integer("USERLOG_FILE_CACHE_MAX", 0, 0);
    m_userlog_file_cache_clear_interval = param_integer("USERLOG_FILE_CACHE_CLEAR_INTERVAL", 60, 0);

	if (slotWeightOfJob) {
		delete slotWeightOfJob;
		slotWeightOfJob = NULL;
	}
	if (slotWeightGuessAd) {
		delete slotWeightGuessAd;
		slotWeightGuessAd = NULL;
		// we will re-create this when/if  we need it.
	}
	m_use_slot_weights = param_boolean("SCHEDD_USE_SLOT_WEIGHT", true);

	char *sw = param("SCHEDD_SLOT_WEIGHT");
	if (sw) {
		ParseClassAdRvalExpr(sw, slotWeightOfJob);

		if (m_use_slot_weights && slotWeightOfJob) {
			// Check for Cpus, Disk or Memory references in the SCHEDD_SLOT_WEIGHT expression
			ClassAd ad;
			classad::References refs;
			ad.GetExternalReferences(slotWeightOfJob, refs, false);
			if (refs.find(ATTR_CPUS) != refs.end() ||
				refs.find(ATTR_DISK) != refs.end() ||
				refs.find(ATTR_MEMORY) != refs.end() ||
				refs.find("TARGET") != refs.end()) {
				dprintf(D_ALWAYS, "Warning: the SCHEDD_SLOT_WEIGHT expression '%s' refers to TARGET, Cpus, Disk or Memory. It must refer only to job attributes like RequestCpus\n", sw);
			}
		}
		free(sw);
	} else {
		// special case for trival slot SLOT_WEIGHT expressions. create a SCHEDD_SLOT_WEIGHT on the fly
		sw = param("SLOT_WEIGHT");
		if (sw) {
			if (MATCH == strcasecmp(sw, "Cpus")) {
				ParseClassAdRvalExpr("RequestCpus", slotWeightOfJob);
			} else if (MATCH == strcasecmp(sw, "Memory")) {
				ParseClassAdRvalExpr("RequestMemory", slotWeightOfJob);
			}
			free(sw);
		}
	}

	//
	// Handle submit requirements.
	//
	m_submitRequirements.clear();
	std::string submitRequirementNames;
	if( param( submitRequirementNames, "SUBMIT_REQUIREMENT_NAMES" ) ) {

		for (auto& srName: StringTokenIterator(submitRequirementNames)) {
			if( strcmp( srName.c_str(), "NAMES" ) == 0 ) { continue; }
			std::string srAttributeName;
			formatstr( srAttributeName, "SUBMIT_REQUIREMENT_%s", srName.c_str() );

			std::string srAttribute;
			if( param( srAttribute, srAttributeName.c_str() ) ) {
				// Can this safely be hoisted out of the loop?
				classad::ClassAdParser parser;

				classad::ExprTree * submitRequirement = parser.ParseExpression( srAttribute );
				if( submitRequirement != NULL ) {

					// Handle the corresponding rejection reason.
					std::string srrAttribute;
					std::string srrAttributeName;
					classad::ExprTree * submitReason = NULL;
					formatstr( srrAttributeName, "SUBMIT_REQUIREMENT_%s_REASON", srName.c_str() );
					if( param( srrAttribute, srrAttributeName.c_str() ) ) {
						submitReason = parser.ParseExpression( srrAttribute );
						if( submitReason == NULL ) {
							dprintf( D_ALWAYS, "Unable to parse submit requirement reason %s, ignoring.\n", srName.c_str() );
						}
					}

					std::string isWarningName;
					formatstr( isWarningName, "SUBMIT_REQUIREMENT_%s_IS_WARNING", srName.c_str() );
					bool isWarning = param_boolean( isWarningName.c_str(), false );

					m_submitRequirements.emplace_back(srName, submitRequirement, submitReason, isWarning);
				} else {
					dprintf( D_ALWAYS, "Unable to parse submit requirement %s, ignoring.\n", srName.c_str() );
				}

				// We could add SUBMIT_REQUIREMENT_<NAME>_REJECT_REASON, as well.
			} else {
				dprintf( D_ALWAYS, "Submit requirement %s not defined, ignoring.\n", srName.c_str() );
			}
		}
	}

	bool new_match_password = param_boolean( "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true );
	if ( new_match_password != m_matchPasswordEnabled ) {
		IpVerify* ipv = daemonCore->getIpVerify();
		if ( new_match_password ) {
			ipv->PunchHole( CLIENT_PERM, EXECUTE_SIDE_MATCHSESSION_FQU );
			ipv->PunchHole( NEGOTIATOR, NEGOTIATOR_SIDE_MATCHSESSION_FQU );
		} else {
			ipv->FillHole( CLIENT_PERM, EXECUTE_SIDE_MATCHSESSION_FQU );
			ipv->FillHole( NEGOTIATOR, NEGOTIATOR_SIDE_MATCHSESSION_FQU );
		}
		m_matchPasswordEnabled = new_match_password;
	}

	// Read config and initialize job transforms.
	jobTransforms.initAndReconfig();

	first_time_in_init = false;
}

void
Scheduler::Register()
{
	 // message handlers for schedd commands
	 daemonCore->Register_CommandWithPayload( NEGOTIATE, 
		 "NEGOTIATE", 
		 (CommandHandlercpp)&Scheduler::negotiate, "negotiate", 
		 this, NEGOTIATOR );
	 daemonCore->Register_Command( RESCHEDULE, "RESCHEDULE", 
			(CommandHandlercpp)&Scheduler::reschedule_negotiator, 
			"reschedule_negotiator", this, WRITE);
	 daemonCore->Register_CommandWithPayload(ACT_ON_JOBS, "ACT_ON_JOBS", 
			(CommandHandlercpp)&Scheduler::actOnJobs, 
			"actOnJobs", this, WRITE,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(SPOOL_JOB_FILES, "SPOOL_JOB_FILES", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(TRANSFER_DATA, "TRANSFER_DATA", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(SPOOL_JOB_FILES_WITH_PERMS,
			"SPOOL_JOB_FILES_WITH_PERMS", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(TRANSFER_DATA_WITH_PERMS,
			"TRANSFER_DATA_WITH_PERMS", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(UPDATE_GSI_CRED,"UPDATE_GSI_CRED",
			(CommandHandlercpp)&Scheduler::updateGSICred,
			"updateGSICred", this, WRITE,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(DELEGATE_GSI_CRED_SCHEDD,
			"DELEGATE_GSI_CRED_SCHEDD",
			(CommandHandlercpp)&Scheduler::updateGSICred,
			"updateGSICred", this, WRITE,
			true /*force authentication*/);
	 // CRUFT This is for the condor_transferd, which is no longer supported
	 //daemonCore->Register_CommandWithPayload(REQUEST_SANDBOX_LOCATION,
	 //		"REQUEST_SANDBOX_LOCATION",
	 //		(CommandHandlercpp)&Scheduler::requestSandboxLocation,
	 //		"requestSandboxLocation", this, WRITE,
	 //		true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(RECYCLE_SHADOW,
			"RECYCLE_SHADOW",
			(CommandHandlercpp)&Scheduler::RecycleShadow,
			"RecycleShadow", this, DAEMON,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(DIRECT_ATTACH,
			"DIRECT_ATTACH",
			(CommandHandlercpp)&Scheduler::CmdDirectAttach,
			"DirectAttach", this, WRITE,
			true /*force authentication*/);

		 // Commands used by the startd are registered at READ
		 // level rather than something like DAEMON or WRITE in order
		 // to reduce the level of authority that the schedd must
		 // grant the startd.  In order for these commands to
		 // succeed, the startd must present the secret claim id,
		 // so it is deemed safe to open these commands up to READ
		 // access.
	daemonCore->Register_CommandWithPayload(RELEASE_CLAIM, "RELEASE_CLAIM", 
			(CommandHandlercpp)&Scheduler::release_claim_command_handler,
			"release_claim", this, READ);
	daemonCore->Register_CommandWithPayload( ALIVE, "ALIVE", 
			(CommandHandlercpp)&Scheduler::receive_startd_alive,
			"receive_startd_alive", this, READ);

	// Command handler for testing file access.  I set this as WRITE as we
	// don't want people snooping the permissions on our machine.
	daemonCore->Register_CommandWithPayload( ATTEMPT_ACCESS, "ATTEMPT_ACCESS",
								  &attempt_access_handler,
								  "attempt_access_handler", WRITE );
#ifdef WIN32
	// Command handler for stashing credentials.
	daemonCore->Register_CommandWithPayload( STORE_CRED, "STORE_CRED",
								&store_cred_handler,
								"cred_access_handler", WRITE,
								true /*force authentication*/);
#endif

    // command handler in support of condor_status -direct query of our ads
    //
	daemonCore->Register_CommandWithPayload(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
                                (CommandHandlercpp)&Scheduler::command_query_ads,
                                 "command_query_ads", this, READ);
	daemonCore->Register_CommandWithPayload(QUERY_SUBMITTOR_ADS,"QUERY_SUBMITTOR_ADS",
                                (CommandHandlercpp)&Scheduler::command_query_ads,
                                 "command_query_ads", this, READ);
	daemonCore->Register_CommandWithPayload(QUERY_STARTD_ADS,"QUERY_STARTD_ADS",
                                (CommandHandlercpp)&Scheduler::command_query_ads,
                                 "command_query_ads", this, READ);

	// Command handler for remote history	
	daemonCore->Register_CommandWithPayload(QUERY_SCHEDD_HISTORY,"QUERY_SCHEDD_HISTORY",
				(CommandHandlercpp)&HistoryHelperQueue::command_handler,
				"command_history", &HistoryQue, READ);

	// Command handler for 'condor_q' queries.  Broken out from QMGMT for the 8.1.4 series.
	// QMGMT is a massive hulk of global variables - we want to make this non-blocking and have
	// multiple queries active in-process at once.
	daemonCore->Register_CommandWithPayload(QUERY_JOB_ADS, "QUERY_JOB_ADS",
				(CommandHandlercpp)&Scheduler::command_query_job_ads,
				"command_query_job_ads", this, READ);

	daemonCore->Register_CommandWithPayload(QUERY_JOB_ADS_WITH_AUTH, "QUERY_JOB_ADS_WITH_AUTH",
				(CommandHandlercpp)&Scheduler::command_query_job_ads,
				"command_query_job_ads", this, READ, true /*force authentication*/);

#ifdef USE_JOB_QUEUE_USERREC

	daemonCore->Register_CommandWithPayload(QUERY_USERREC_ADS, "QUERY_USERREC_ADS",
		(CommandHandlercpp)&Scheduler::command_query_user_ads,
		"command_query_user_ads", this, READ, true /*force authentication*/);

	daemonCore->Register_CommandWithPayload(ENABLE_USERREC, "ENABLE_USERREC", // enable/add user/owner
		(CommandHandlercpp)&Scheduler::command_act_on_user_ads,
		"command_act_on_user_ads", this, WRITE, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(DISABLE_USERREC, "DISABLE_USERREC",
		(CommandHandlercpp)&Scheduler::command_act_on_user_ads,
		"command_act_on_user_ads", this, WRITE, true /*force authentication*/);

	//disable these until we decide permissions
	daemonCore->Register_CommandWithPayload(EDIT_USERREC, "EDIT_USERREC",
		(CommandHandlercpp)&Scheduler::command_act_on_user_ads,
		"command_act_on_user_ads", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(RESET_USERREC, "RESET_USERREC",
		(CommandHandlercpp)&Scheduler::command_act_on_user_ads,
		"command_act_on_user_ads", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(DELETE_USERREC, "DELETE_USERREC",
		(CommandHandlercpp)&Scheduler::command_act_on_user_ads,
		"command_act_on_user_ads", this, ADMINISTRATOR, true /*force authentication*/);

#endif

	// Note: The QMGMT READ/WRITE commands have the same command handler.
	// This is ok, because authorization to do write operations is verified
	// internally in the command handler.
	daemonCore->Register_CommandWithPayload( QMGMT_READ_CMD, "QMGMT_READ_CMD",
								  &handle_q,
								  "handle_q", READ );

	// This command always requires authentication.  Therefore, it is
	// more efficient to force authentication when establishing the
	// security session than to possibly create an unauthenticated
	// security session that has to be authenticated every time in
	// the command handler.
	daemonCore->Register_CommandWithPayload( QMGMT_WRITE_CMD, "QMGMT_WRITE_CMD",
								  &handle_q,
								  "handle_q", WRITE,
								  true /* force authentication */ );

	daemonCore->Register_CommandWithPayload( GET_JOB_CONNECT_INFO, "GET_JOB_CONNECT_INFO",
								  (CommandHandlercpp)&Scheduler::get_job_connect_info_handler,
								  "get_job_connect_info", this, WRITE,
								  true /*force authentication*/);

	daemonCore->Register_CommandWithPayload( CLEAR_DIRTY_JOB_ATTRS, "CLEAR_DIRTY_JOB_ATTRS",
								  (CommandHandlercpp)&Scheduler::clear_dirty_job_attrs_handler,
								  "clear_dirty_job_attrs_handler", this, WRITE );

	daemonCore->Register_CommandWithPayload( EXPORT_JOBS, "EXPORT_JOBS",
								  (CommandHandlercpp)&Scheduler::export_jobs_handler,
								  "export_jobs_handler", this, WRITE,
								  true /*force authentication*/);
	daemonCore->Register_CommandWithPayload( IMPORT_EXPORTED_JOB_RESULTS, "IMPORT_EXPORTED_JOB_RESULTS",
								  (CommandHandlercpp)&Scheduler::import_exported_job_results_handler,
								  "import_exported_job_results_handler", this, WRITE,
								  true /*force authentication*/);
	daemonCore->Register_CommandWithPayload( UNEXPORT_JOBS, "UNEXPORT_JOBS",
								  (CommandHandlercpp)&Scheduler::unexport_jobs_handler,
								  "unexport_jobs_handler", this, WRITE,
								  true /*force authentication*/);

	 // These commands are for a startd reporting directly to the schedd sans negotiation
	daemonCore->Register_CommandWithPayload(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
        						  (CommandHandlercpp)&Scheduler::receive_startd_update,
								  "receive_startd_update",this,ADVERTISE_STARTD_PERM);

	daemonCore->Register_CommandWithPayload(INVALIDATE_STARTD_ADS,"INVALIDATE_STARTD_ADS",
        						  (CommandHandlercpp)&Scheduler::receive_startd_invalidate,
								  "receive_startd_invalidate",this,ADVERTISE_STARTD_PERM);

	daemonCore->Register_CommandWithPayload( REASSIGN_SLOT, "REASSIGN_SLOT",
			(CommandHandlercpp)&Scheduler::reassign_slot_handler,
			"reassign_slot_handler", this, WRITE);


		//
		// Handles requests from a collector to issue tokens on behalf of its users.
		// Only pre-created sessions are allowed to use this command; this is enforced
		// in the command handler itself.
		//
	daemonCore->Register_CommandWithPayload( COLLECTOR_TOKEN_REQUEST, "DC_COLLECTOR_TOKEN_REQUEST",
			(CommandHandlercpp)&Scheduler::handle_collector_token_request,
			"handle_collector_token_request()", this, ALLOW, true );

	 // reaper
	shadowReaperId = daemonCore->Register_Reaper(
		"reaper",
		(ReaperHandlercpp)&Scheduler::child_exit_from_reaper,
		"child_exit", this );

	// register all the timers
	RegisterTimers();

	// Now is a good time to instantiate the GridUniverse
	_gridlogic = new GridUniverseLogic;

	m_xfer_queue_mgr.RegisterHandlers();

}

void
Scheduler::RegisterTimers()
{
	static int cleanid = -1, wallclocktid = -1;
	// Note: aliveid is a data member of the Scheduler class
	static int oldQueueCleanInterval = -1;

	Timeslice start_jobs_timeslice;

	// clear previous timers
	if (startjobsid >= 0) {
		daemonCore->GetTimerTimeslice(startjobsid,start_jobs_timeslice);
		daemonCore->Cancel_Timer(startjobsid);
	}
	else {
		start_jobs_timeslice.setInitialInterval(10);
	}
		// Copy settings for start jobs timeslice from schedDInterval,
		// since we currently don't have any reason to want them to
		// be configured independently.  We do _not_ currently copy
		// the minimum interval, so frequent calls are allowed as long
		// as the timeslice is within the limit.
	start_jobs_timeslice.setDefaultInterval( SchedDInterval.getDefaultInterval() );
	start_jobs_timeslice.setMaxInterval( SchedDInterval.getMaxInterval() );
	start_jobs_timeslice.setTimeslice( SchedDInterval.getTimeslice() );

	if (aliveid >= 0) {
		daemonCore->Cancel_Timer(aliveid);
	}
	if (periodicid>=0) {
		daemonCore->Cancel_Timer(periodicid);
	}

	 // timer handlers
	if (timeoutid < 0) {
		timeoutid = daemonCore->Register_Timer(10, 10,
			(TimerHandlercpp)&Scheduler::timeout,"timeout",this);
	}
	startjobsid = daemonCore->Register_Timer( start_jobs_timeslice,
		(TimerHandlercpp)&Scheduler::StartJobs,"StartJobs",this);
	aliveid = daemonCore->Register_Timer(10, alive_interval,
		(TimerHandlercpp)&Scheduler::sendAlives,"sendAlives", this);
    // Preset the job queue clean timer only upon cold start, or if the timer
    // value has been changed.  If the timer period has not changed, leave the
    // timer alone.  This will avoid undesirable behavior whereby timer is
    // preset upon every reconfig, and job queue is not cleaned often enough.
    if  (  QueueCleanInterval != oldQueueCleanInterval) {
        if (cleanid >= 0) {
            daemonCore->Cancel_Timer(cleanid);
        }
        cleanid =
            daemonCore->Register_Timer(QueueCleanInterval,QueueCleanInterval,
            CleanJobQueue,"CleanJobQueue");
    }
    oldQueueCleanInterval = QueueCleanInterval;

	if (WallClockCkptInterval) {
		if( wallclocktid != -1 ) {
			daemonCore->Reset_Timer_Period(wallclocktid,WallClockCkptInterval);
		}
		else {
			wallclocktid = daemonCore->Register_Timer(WallClockCkptInterval,
												  WallClockCkptInterval,
												  CkptWallClock,
												  "CkptWallClock");
		}
	} else {
		if( wallclocktid != -1 ) {
			daemonCore->Cancel_Timer( wallclocktid );
		}
		wallclocktid = -1;
	}

		// We've seen a test suite run where the schedd never called
		// PeriodicExprHandler(). Add some debug statements so that
		// we know why if it happens again.
	if (PeriodicExprInterval.getMinInterval()>0) {
		unsigned int time_to_next_run = PeriodicExprInterval.getTimeToNextRun();
		if ( time_to_next_run == 0 ) {
				// Can't use 0, because that means it's a one-time timer
			time_to_next_run = 1;
		}
		periodicid = daemonCore->Register_Timer(
			time_to_next_run,
			time_to_next_run,
			(TimerHandlercpp)&Scheduler::PeriodicExprHandler,"PeriodicExpr",this);
		dprintf( D_FULLDEBUG, "Registering PeriodicExprHandler(), next "
				 "callback in %u seconds\n", time_to_next_run );
	} else {
		dprintf( D_FULLDEBUG, "Periodic expression evaluation disabled! "
				 "(getMinInterval()=%f, PERIODIC_EXPR_INTERVAL=%d)\n",
				 PeriodicExprInterval.getMinInterval(),
				 param_integer("PERIODIC_EXPR_INTERVAL", 60) );
		periodicid = -1;
	}
}

void Scheduler::reconfig() {
	/***********************************
	 * WARNING!!  WARNING!! WARNING, WILL ROBINSON!!!!
	 *
	 * DO NOT PUT CALLS TO PARAM() HERE - YOU PROBABLY WANT TO PUT THEM IN
	 * Scheduler::Init().  Note that reconfig() calls Init(), but Init() does
	 * NOT call reconfig() !!!  So if you initalize settings via param() calls
	 * in this function, likely the schedd will not work as you expect upon
	 * startup until the poor confused user runs condor_reconfig!!
	 ***************************************/

	Init();

	RegisterTimers();			// reset timers


		// clear out auto cluster id attributes
	if ( autocluster.config(MinimalSigAttrs) ) {
		WalkJobQueue(clear_autocluster_id);
	}

	timeout();

		// The following use of param() is ok, despite the warning at the
		// top of this function that this function is not called at init time.
		// SetMaxHistoricalLogs is initialized in main_init(), we just need
		// to check here for changes.  
	int max_saved_rotations = param_integer( "MAX_JOB_QUEUE_LOG_ROTATIONS", DEFAULT_MAX_JOB_QUEUE_LOG_ROTATIONS );
	SetMaxHistoricalLogs(max_saved_rotations);

}

// NOTE: this is likely unreachable now, and may be removed
void
Scheduler::update_local_ad_file() 
{
	daemonCore->UpdateLocalAd(m_adSchedd);
	return;
}

// This function is called by a timer when we are shutting down
void
Scheduler::attempt_shutdown( int /* timerID */ )
{
	if ( numShadows || SchedUniverseJobsRunning || LocalUniverseJobsRunning ) {
		if( !daemonCore->GetPeacefulShutdown() ) {
			preempt( JobStopCount );
		}
		return;
	}

	if ( CronJobMgr && ( ! CronJobMgr->ShutdownOk() ) ) {
		return;
	}

	schedd_exit( );
}

// Perform graceful shutdown.
void
Scheduler::shutdown_graceful()
{
	dprintf( D_FULLDEBUG, "Now in shutdown_graceful\n" );

	// If there's nothing to do, shutdown
	if(  ( numShadows == 0 ) && SchedUniverseJobsRunning == 0 &&
		 LocalUniverseJobsRunning == 0 &&
		 ( CronJobMgr && ( CronJobMgr->ShutdownOk() ) )  ) {
		schedd_exit();
	}

	if ( ExitWhenDone ) {
		// we already are attempting to gracefully shutdown
		return;
	}

	/* 
 		There are shadows running, so set a flag that tells the
		reaper to exit when all the shadows are gone, and start
		shutting down shadows.  Set a Timer to shutdown a shadow
		every JobStartDelay seconds.
 	 */
	MaxJobsRunning = 0;
	ExitWhenDone = TRUE;
	daemonCore->Register_Timer( 0, MAX(JobStopDelay,1), 
					(TimerHandlercpp)&Scheduler::attempt_shutdown,
					"attempt_shutdown()", this );

	// Shut down the cron logic
	if( CronJobMgr ) {
		CronJobMgr->Shutdown( false );
	}

}

// Perform fast shutdown.
void
Scheduler::shutdown_fast()
{
	dprintf( D_FULLDEBUG, "Now in shutdown_fast. Sending signals to shadows\n" );

	int sig;
	for (const auto &[pid, rec]: shadowsByPid) {
		if(	rec->universe == CONDOR_UNIVERSE_LOCAL ) { 
			sig = DC_SIGHARDKILL;
		} else {
			sig = SIGKILL;
		}
			// Call the blocking form of Send_Signal, rather than
			// sendSignalToShadow().
		daemonCore->Send_Signal(rec->pid,sig);
		dprintf( D_ALWAYS, "Sent signal %d to shadow [pid %d] for job %d.%d\n",
					sig, rec->pid,
					rec->job_id.cluster, rec->job_id.proc );
	}

	// Shut down the cron logic
	if( CronJobMgr ) {
		CronJobMgr->Shutdown( true );
	}

	DestroyJobQueue();
		// Since this is just sending a bunch of UDP updates, we can
		// still invalidate our classads, even on a fast shutdown.
	invalidate_ads();

#ifdef UNIX
	ScheddPluginManager::Shutdown();
	ClassAdLogPluginManager::Shutdown();
#endif

	dprintf( D_ALWAYS, "All shadows have been killed, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::schedd_exit()
{
		// Shut down the cron logic
	if( CronJobMgr ) {
		dprintf( D_ALWAYS, "Deleting CronJobMgr\n" );
		CronJobMgr->Shutdown( true );
		delete CronJobMgr;
		CronJobMgr = NULL;
	}

		// write a clean job queue on graceful shutdown so we can
		// quickly recover on restart
	CleanJobQueue();

		// Deallocate the memory in the job queue so we don't think
		// we're leaking anything. 
	DestroyJobQueue();

		// Invalidate our classads at the collector, since we're now
		// gone.  
	invalidate_ads();

#ifdef UNIX
	ScheddPluginManager::Shutdown();
	ClassAdLogPluginManager::Shutdown();
#endif

	dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::invalidate_ads()
{
	std::string line;

		// The ClassAd we need to use is totally different from the
		// regular one, so just create a temporary one
	ClassAd * cad = new ClassAd;
    SetMyTypeName( *cad, QUERY_ADTYPE );
    cad->Assign(ATTR_TARGET_TYPE, SCHEDD_ADTYPE );

        // Invalidate the schedd ad
	formatstr( line, "TARGET.%s == \"%s\"", ATTR_NAME, Name );
	cad->AssignExpr( ATTR_REQUIREMENTS, line.c_str() );
	cad->Assign( ATTR_NAME, Name );
	cad->Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr() );


		// Update collectors
	daemonCore->sendUpdates(INVALIDATE_SCHEDD_ADS, cad, NULL, false);

	if (NumSubmitters == 0) {
		// no submitter ads to invalidate
		delete cad;
		return;
	}

		// Invalidate all our submittor ads.
		// Disable creation of new TCP connections to ensure a speedy
		// cleanup at shutdown.

	cad->Assign( ATTR_SCHEDD_NAME, Name );
	cad->Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr() );

	for (SubmitterDataMap::iterator it = Submitters.begin(); it != Submitters.end(); ++it) {
		const SubmitterData & Owner = it->second;
		daemonCore->sendUpdates(INVALIDATE_SUBMITTOR_ADS, cad, NULL, false);
		std::string submitter = Owner.Name();
		if (user_is_the_new_owner) {
			// owner names are already fully qualified
		} else {
			submitter += "@"; submitter += AccountingDomain;
		}
		cad->Assign( ATTR_NAME, submitter );

		formatstr( line, "TARGET.%s == \"%s\" && TARGET.%s == \"%s\"",
					  ATTR_SCHEDD_NAME, Name,
					  ATTR_NAME, submitter.c_str() );
		cad->AssignExpr( ATTR_REQUIREMENTS, line.c_str() );

		DCCollectorAdSequences & adSeq = daemonCore->getUpdateAdSeq();
		if( FlockCollectors.size() && FlockLevel > 0 ) {
			int level;
			for( level=1;
				 level <= FlockLevel && level <= (int)FlockCollectors.size(); level++ ) {
				FlockCollectors[level-1].allowNewTcpConnections(false);
				FlockCollectors[level-1].sendUpdate( INVALIDATE_SUBMITTOR_ADS, cad, adSeq, NULL, false );
			}
		}
	}

	delete cad;
}


int
Scheduler::reschedule_negotiator(int, Stream *s)
{
	if( s && !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to receive end of message for RESCHEDULE.\n");
		return 0;
	}

		// don't bother the negotiator if we are shutting down
	if ( ExitWhenDone ) {
		return 0;
	}

	needReschedule();

	StartJobs(); // now needed because of claim reuse, is this too expensive?
	return 0;
}

void
Scheduler::needReschedule()
{
	m_need_reschedule = true;

		// Update the central manager and request a reschedule.  We
		// don't call sendReschedule() directly below, because
		// timeout() has internal logic to avoid doing its work too
		// frequently, and we want to send the reschedule after
		// updating our ad in the collector, not before.
	daemonCore->Reset_Timer(timeoutid,0,1);
}

void
Scheduler::sendReschedule( int /* timerID */ )
{
	// If SCHEDD_SEND_RESCHEDULE is false (eg on a CE or schedd
	// without a negotiator, don't spam the logs with messages
	// about a missing negotiator.
	if (!param_boolean("SCHEDD_SEND_RESCHEDULE", true)) {
		return;
	}

	if( !m_negotiate_timeslice.isTimeToRun() ) {
			// According to our negotiate timeslice object, we are
			// spending too much of our time negotiating, so delay
			// sending the reschedule command to the negotiator.  That
			// _might_ help, but there is no guarantee, since the
			// negotiator can decide to initiate negotiation at any
			// time.

		if( m_send_reschedule_timer == -1 ) {
			m_send_reschedule_timer = daemonCore->Register_Timer(
				m_negotiate_timeslice.getTimeToNextRun(),
				(TimerHandlercpp)&Scheduler::sendReschedule,
				"Scheduler::sendReschedule",
				this);
		}
		dprintf( D_FULLDEBUG,
				 "Delaying sending RESCHEDULE to negotiator for %d seconds.\n",
				 m_negotiate_timeslice.getTimeToNextRun() );
		return;
	}

	if( m_send_reschedule_timer != -1 ) {
		daemonCore->Cancel_Timer( m_send_reschedule_timer );
		m_send_reschedule_timer = -1;
	}

	dprintf( D_FULLDEBUG, "Sending RESCHEDULE command to negotiator(s)\n" );

	classy_counted_ptr<Daemon> negotiator = new Daemon(DT_NEGOTIATOR);
	classy_counted_ptr<DCCommandOnlyMsg> msg = new DCCommandOnlyMsg(RESCHEDULE);
	negotiator->locate(Daemon::LOCATE_FOR_LOOKUP);

	Stream::stream_type st = negotiator->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
	msg->setStreamType(st);
	msg->setTimeout(NEGOTIATOR_CONTACT_TIMEOUT);

	// since we may be sending reschedule periodically, make sure they do
	// not pile up
	msg->setDeadlineTimeout( 300 );

	negotiator->sendMsg( msg.get() );

	if( FlockNegotiators.size() ) {
		for( int i=0; i<FlockLevel && i < (int)FlockNegotiators.size(); i++ ) {
			st = FlockNegotiators[i].hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
			negotiator = new Daemon( FlockNegotiators[i] );
			msg = new DCCommandOnlyMsg(RESCHEDULE);
			msg->setStreamType(st);
			msg->setTimeout(NEGOTIATOR_CONTACT_TIMEOUT);

			negotiator->sendMsg( msg.get() );
		}
	}
}

void
Scheduler::OptimizeMachineAdForMatchmaking(ClassAd *ad)
{
	// We may be re-optimizing this ad after mutating it.
	// Undo any previous optimization first.
	classad::MatchClassAd::UnoptimizeAdForMatchmaking( ad );

		// The machine ad will be passed as the RIGHT ad during
		// matchmaking (i.e. in the call to IsAMatch()), so
		// optimize it accordingly.
	std::string error_msg;
	if( !classad::MatchClassAd::OptimizeRightAdForMatchmaking( ad, &error_msg ) ) {
		std::string name;
		ad->LookupString(ATTR_NAME,name);
		dprintf(D_ALWAYS,
				"Failed to optimize machine ad %s for matchmaking: %s\n",	
			name.c_str(),
				error_msg.c_str());
	}
}


match_rec*
Scheduler::AddMrec(char const* id, char const* peer, PROC_ID* jobId, const ClassAd* my_match_ad,
				   char const *user, char const *pool, match_rec **pre_existing)
{
	match_rec *rec;

	if( pre_existing ) {
		*pre_existing = NULL;
	}
	if(!id || !peer)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not added\n"); 
		return NULL;
	} 
	// spit out a warning and return NULL if we already have this mrec
	match_rec *tempRec;
	if( matches->lookup( id, tempRec ) == 0 ) {
		char const *pubid = tempRec->claim_id.publicClaimId();
		dprintf( D_ALWAYS,
				 "attempt to add pre-existing match \"%s\" ignored\n",
				 pubid ? pubid : "(null)" );
		if( pre_existing ) {
			*pre_existing = tempRec;
		}
		return NULL;
	}


	rec = new match_rec(id, peer, jobId, my_match_ad, user, pool, false);
	if(!rec)
	{
		EXCEPT("Out of memory!");
	} 

	if( matches->insert( id, rec ) != 0 ) {
		ClaimIdParser cid(id);
		dprintf( D_ALWAYS, "match \"%s\" insert failed\n", cid.publicClaimId());
		delete rec;
		return NULL;
	}
	ASSERT( matchesByJobID->insert( *jobId, rec ) == 0 );
	numMatches++;

		// Update CurrentRank in the startd ad.  Why?  Because when we
		// reuse this match for a different job (in
		// FindRunnableJob()), we make sure it has a rank >= the
		// startd CurrentRank, in order to avoid potential
		// rejection by the startd.

	JobQueueJob *job_ad = GetJobAd(jobId->cluster,jobId->proc);
	if( job_ad && rec->my_match_ad ) {
		float new_startd_rank = 0;
		if( EvalFloat(ATTR_RANK, rec->my_match_ad, job_ad, new_startd_rank) ) {
			rec->my_match_ad->Assign(ATTR_CURRENT_RANK, new_startd_rank);
		}
	}

	// timestamp the first time a job with this ClusterId got a match
	if (job_ad && job_ad->Cluster() && ! job_ad->Cluster()->Lookup(ATTR_FIRST_JOB_MATCH_DATE)) {
		job_ad->Cluster()->Assign(ATTR_FIRST_JOB_MATCH_DATE, time(nullptr));
	}

	// These are attributes that were added to the slot ad after it
	// left the startd. We want to preserve them when we get a fresh copy
	// of the slot ad from the startd.
	if(rec->my_match_ad) {
			// Carry Negotiator Match expressions over from the
			// match record.
		size_t len = strlen(ATTR_NEGOTIATOR_MATCH_EXPR);
		for (auto itr = rec->my_match_ad->begin(); itr != rec->my_match_ad->end(); itr++) {
			if(!strncmp(itr->first.c_str(), ATTR_NEGOTIATOR_MATCH_EXPR, len)) {
				CopyAttribute(itr->first, rec->m_added_attrs, *rec->my_match_ad);
			}
		}

		// These attributes are added by the schedd to slot ads that
		// arrive via DIRECT_ATTACH.
		// ATTR_AUTHENTICATED_IDENTITY is also added by the collector
		// when the slot ad passes through it.
		CopyAttribute(ATTR_AUTHENTICATED_IDENTITY, rec->m_added_attrs, *rec->my_match_ad);
		CopyAttribute(ATTR_RESTRICT_TO_AUTHENTICATED_IDENTITY, rec->m_added_attrs, *rec->my_match_ad);
	}

	if( rec->my_match_ad ) {
		OptimizeMachineAdForMatchmaking( rec->my_match_ad );
	}

	return rec;
}

// All deletions of match records _MUST_ go through DelMrec() to ensure
// proper cleanup.
int
Scheduler::DelMrec(char const* id)
{
	match_rec *rec;

	if(!id)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not deleted\n");
		return -1;
	}

	if( matches->lookup(id, rec) != 0 ) {
			// Couldn't find it, return failure
		return -1;
	}

	return DelMrec( rec );
}


int
Scheduler::DelMrec(match_rec *match) {
	if (this->unlinkMrec(match) == 0) {
		delete match;
		return 0;
	}
	return -1;
}

int
Scheduler::unlinkMrec(match_rec* match)
{
	if(!match)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not deleted\n");
		return -1;
	}

	if( match->is_dedicated ) {
			// This is a convenience for code that is shared with
			// DedicatedScheduler, such as contactStartd().
		return dedicated_scheduler.DelMrec( match );
	}

	// release the claim on the startd
	if( match->needs_release_claim) {
		send_vacate(match, RELEASE_CLAIM);
	}

	dprintf( D_ALWAYS, "Match record (%s, %d.%d) deleted\n",
			 match->description(), match->cluster, match->proc ); 

	matches->remove(match->claim_id.claimId());

	PROC_ID jobId;
	jobId.cluster = match->cluster;
	jobId.proc = match->proc;
	matchesByJobID->remove(jobId);

		// fill any authorization hole we made for this match
	if (match->auth_hole_id != NULL) {
		IpVerify* ipv = daemonCore->getSecMan()->getIpVerify();
		if (!ipv->FillHole(READ, *match->auth_hole_id)) {
			dprintf(D_ALWAYS,
			        "WARNING: IpVerify::FillHole error for %s\n",
			        match->auth_hole_id->c_str());
		}
		delete match->auth_hole_id;
		match->auth_hole_id = NULL;
	}

		// Remove this match from the associated shadowRec.
	if (match->shadowRec)
		match->shadowRec->match = NULL;
	
	numMatches--; 
	return 0;
}

shadow_rec*
Scheduler::FindSrecByPid(int pid)
{
	auto it = shadowsByPid.find(pid);
	if (it == shadowsByPid.end()) {
		return nullptr;
	}
	return it->second;
}

shadow_rec*
Scheduler::FindSrecByProcID(PROC_ID proc)
{
	auto it = shadowsByProcID.find(proc);
	if (it == shadowsByProcID.end()) {
		return nullptr;
	}
	return it->second;
}

match_rec *
Scheduler::FindMrecByJobID(PROC_ID job_id) {
	match_rec *match = NULL;
	if( matchesByJobID->lookup( job_id, match ) < 0) {
		return NULL;
	}
	return match;
}

match_rec *
Scheduler::FindMrecByClaimID(char const *claim_id) {
	match_rec *rec = NULL;
	(void) matches->lookup(claim_id, rec);
	return rec;
}

void
Scheduler::SetMrecJobID(match_rec *match, PROC_ID job_id) {
	PROC_ID old_job_id;
	old_job_id.cluster = match->cluster;
	old_job_id.proc = match->proc;

	if( old_job_id.cluster == job_id.cluster && old_job_id.proc == job_id.proc ) {
		return; // no change
	}

	matchesByJobID->remove(old_job_id);

	match->cluster = job_id.cluster;
	match->proc = job_id.proc;
	if( match->proc != -1 ) {
		ASSERT( matchesByJobID->insert(job_id, match) == 0 );
	}
}

void
Scheduler::SetMrecJobID(match_rec *match, int cluster, int proc) {
	PROC_ID job_id;
	job_id.cluster = cluster;
	job_id.proc = proc;
	SetMrecJobID( match, job_id );
}

void
Scheduler::RemoveShadowRecFromMrec( shadow_rec* shadow )
{
	if( shadow->match ) {
		match_rec *mrec = shadow->match;
		mrec->shadowRec = NULL;
		shadow->match = NULL;

			// re-associate match with the original job cluster
			// and set its status back to CLAIMED
		SetMrecJobID(mrec,mrec->origcluster,-1);
		mrec->setStatus( M_CLAIMED );
		if( mrec->is_dedicated ) {
			deallocMatchRec( mrec );
		}
	}
}

int Scheduler::AlreadyMatched(JobQueueJob * job, int universe)
{
	bool wantPS = 0;
	job->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING, wantPS);

	if ( ! job || ! job->IsJob() ||
		 (universe == CONDOR_UNIVERSE_MPI) ||
		 (universe == CONDOR_UNIVERSE_GRID) ||
		 (universe == CONDOR_UNIVERSE_PARALLEL) || wantPS ) {
		return FALSE;
	}

	if( FindMrecByJobID(job->jid) ) {
			// It is possible for there to be a match rec but no shadow rec,
			// if the job is waiting in the runnable job queue before the
			// shadow is launched.
		return TRUE;
	}
	if( FindSrecByProcID(job->jid) ) {
			// It is possible for there to be a shadow rec but no match rec,
			// if the match was deleted but the shadow has not yet gone away.
		return TRUE;
	}
	return FALSE;

}
int Scheduler::AlreadyMatched(PROC_ID* id)
{
	int universe = CONDOR_UNIVERSE_MIN;
	const OwnerInfo * powni = NULL;
	JobQueueJob * job = GetJobAndInfo(*id, universe, powni);

		// Failing to find the job or get the JOB_UNIVERSE is common 
		// because the job may have left the queue.
		// So in this case, just return FALSE, since a job
		// not in the queue is most certainly not matched :)
	if ( ! job || ! job->IsJob() || ! universe)
		return FALSE;

	return AlreadyMatched(job, universe);
}

/*
 * go through match reords and send alive messages to all the startds.
 */

bool
sendAlive( match_rec* mrec )
{
	classy_counted_ptr<DCStartd> startd = new DCStartd( mrec->description(),NULL,mrec->peer,mrec->claim_id.claimId() );
	classy_counted_ptr<DCClaimIdMsg> msg = new DCClaimIdMsg( ALIVE, mrec->claim_id.claimId() );

	msg->setSuccessDebugLevel(D_PROTOCOL);
	msg->setTimeout( STARTD_CONTACT_TIMEOUT );
	// since we send these messages periodically, we do not want
	// any single attempt to hang around forever and potentially pile up
	msg->setDeadlineTimeout( 300 );
	Stream::stream_type st = startd->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
	msg->setStreamType( st );
	if (mrec->use_sec_session) {
		msg->setSecSessionId( mrec->claim_id.secSessionId() );
	}

	dprintf (D_PROTOCOL,"## 6. Sending alive msg to %s\n", mrec->description());

	startd->sendMsg( msg.get() );

	if( msg->deliveryStatus() == DCMsg::DELIVERY_FAILED ) {
			// Status may also be DELIVERY_PENDING, in which case, we
			// do not know whether it will succeed or not.  Since the
			// return code from this function is not terribly
			// important, we just return true in that case.
		return false;
	}

		/* TODO: Someday, espcially once the accountant is done, 
		   the startd should send a keepalive ACK back to the schedd.  
		   If there is no shadow to this machine, and we have not 
		   had a startd keepalive ACK in X amount of time, then we 
		   should relinquish the match.  Since the accountant is 
		   not done and we are in fire mode, leave this 
		   for V6.1.  :^) -Todd 9/97
		*/
	return true;
}

int
Scheduler::receive_startd_alive(int cmd, Stream *s) const
{
	// Received a keep-alive from a startd.  
	// Protocol: startd sends up the match id, and we send back 
	// the current alive_interval, or -1 if we cannot find an active 
	// match for this job.  
	
	ASSERT( cmd == ALIVE );

	char *claim_id = NULL;
	int ret_value;
	match_rec* match = NULL;
	ClassAd *job_ad = NULL;

	s->decode();
	s->timeout(1);	// its a short message so data should be ready for us

	if ( !s->get_secret(claim_id) || !s->end_of_message() ) {
		if (claim_id) free(claim_id);
		return FALSE;
	}
	
	if ( claim_id ) {
		// Find out if this claim_id is still something we care about
		// by first trying to find a match record. Note we also must
		// check the dedicated scheduler data structs, since the dedicated
		// scheduler keeps its own sets of match records.
		match = scheduler.FindMrecByClaimID(claim_id);
		if (!match) {
			match = dedicated_scheduler.FindMrecByClaimID(claim_id);
		}
	}

	if ( match ) {
			// If we're sending keep-alives, stop it, since the startd
			// wants to send them.
		match->m_startd_sends_alives = true;

		ret_value = alive_interval;
			// If this match is active, i.e. we have a shadow, then
			// update the ATTR_LAST_JOB_LEASE_RENEWAL in RAM.  We will
			// commit it to disk in a big transaction batch via the timer
			// handler method sendAlives().  We do this for scalability; we
			// may have thousands of startds sending us updates...
		if ( match->status == M_ACTIVE ) {
			job_ad = GetJobAd(match->cluster, match->proc);
			if (job_ad) {
				job_ad->Assign(ATTR_LAST_JOB_LEASE_RENEWAL, time(nullptr));
			}
		}
	} else {
		ret_value = -1;
		if (claim_id) {
			ClaimIdParser idp( claim_id );
			dprintf(D_ALWAYS, "Received startd keepalive for unknown claimid %s\n",
				idp.publicClaimId() );
		}
	}

	s->encode();
	if (!s->code(ret_value)) {
		dprintf(D_FULLDEBUG, "Unable to send ACK to startd keepalive, startd disconnected\n");
	}
	s->end_of_message();

	if (claim_id) free(claim_id);
	return TRUE;
}


	// For scheduler-created sessions, this will allow the collector to requests tokens.
int
Scheduler::handle_collector_token_request(int, Stream *stream)
{
		// Check: is this my session?
	const auto &sess_id = static_cast<ReliSock*>(stream)->getSessionID();
	classad::ClassAd policy_ad;
	daemonCore->getSecMan()->getSessionPolicy(sess_id.c_str(), policy_ad);
	bool is_my_session = false;
	if (!policy_ad.EvaluateAttrBool("ScheddSession", is_my_session) || !is_my_session) {
		dprintf(D_ALWAYS, "Ignoring a collector token request from an unknown session.\n");
		return false;
	}

		// Check: is this the collector session?
	if (strcmp(static_cast<Sock*>(stream)->getFullyQualifiedUser(),
		COLLECTOR_SIDE_MATCHSESSION_FQU))
	{
		dprintf(D_ALWAYS, "Ignoring a collector token request from non-collector (%s).\n",
			static_cast<Sock*>(stream)->getFullyQualifiedUser());
		return false;
	}

	classad::ClassAd ad;
	if (!getClassAd(stream, ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_collector_token_request: failed to read input from collector\n");
		return false;
	}

	int error_code = 0;
	std::string error_string;

	std::string requested_identity;
	if (!ad.EvaluateAttrString(ATTR_SEC_USER, requested_identity)) {
		error_code = 1;
		error_string = "No identity requested.";
	}

	auto peer_location = static_cast<Sock*>(stream)->peer_ip_str();

	std::set<std::string> config_bounding_set;
	std::string config_bounding_set_str;
	if (param(config_bounding_set_str, "SEC_IMPERSONATION_TOKEN_LIMITS")) {
		for (auto& authz: StringTokenIterator(config_bounding_set_str)) {
			config_bounding_set.insert(authz);
		}
	}

	std::vector<std::string> bounding_set;
	std::string bounding_set_str;
	if (ad.EvaluateAttrString(ATTR_SEC_LIMIT_AUTHORIZATION, bounding_set_str)) {
		for (auto& authz: StringTokenIterator(bounding_set_str)) {
			if (config_bounding_set.empty() || (config_bounding_set.find(authz) != config_bounding_set.end())) {
				bounding_set.emplace_back(authz);
			}
		}
			// If all potential bounds were removed by the set intersection,
			// throw an error instead of generating an "all powerful" token.
		if (!config_bounding_set.empty() && bounding_set.empty()) {
			error_code = 2;
			error_string = "All requested authorizations were eliminated by the"
				" SEC_IMPERSONATION_TOKEN_LIMITS setting";
		}
	} else if (!config_bounding_set.empty()) {
		for (const auto &authz : config_bounding_set) {
			bounding_set.push_back(authz);
		}
	}

	int requested_lifetime = -1;
	int max_lifetime = param_integer("SEC_ISSUED_TOKEN_EXPIRATION", -1);
	if (!ad.EvaluateAttrInt(ATTR_SEC_TOKEN_LIFETIME, requested_lifetime)) {
		requested_lifetime = -1;
	}
	if ((max_lifetime > 0) && (requested_lifetime > max_lifetime)) {
		requested_lifetime = max_lifetime;
	} else if ((max_lifetime > 0) && (requested_lifetime < 0)) {
		requested_lifetime = max_lifetime;
	}

	classad::ClassAd result_ad;
	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
		result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
	} else {
		CondorError err;
		std::string final_key_name = htcondor::get_token_signing_key(err);
		std::string token;
		if (final_key_name.empty()) {
			result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
			result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
		} else if (!Condor_Auth_Passwd::generate_token(
			requested_identity,
			final_key_name,
			bounding_set,
			requested_lifetime,
			token,
			static_cast<Sock*>(stream)->getUniqueId(),
			&err))
		{
			result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
			result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
		} else if (token.empty()) { // Should never happen if we got here...
			error_code = 2;
			error_string = "Internal state error.";
		} else {
			result_ad.InsertAttr(ATTR_SEC_TOKEN, token);
			dprintf(D_AUDIT, *static_cast<Sock*>(stream),
				"Collector %s token request for ID %s, bounding set %s,"
				" and lifetime %d issued.\n", peer_location, requested_identity.c_str(),
				bounding_set_str.empty() ? "(none)" : bounding_set_str.c_str(),
				requested_lifetime);
		}
	}

	if (!stream->get_encryption()) {
		result_ad.Clear();
		result_ad.InsertAttr(ATTR_ERROR_STRING, "Request to server was not encrypted.");
		result_ad.InsertAttr(ATTR_ERROR_CODE, 3);
	}

	stream->encode();
	if (!putClassAd(stream, result_ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_collector_token_request: failed to send response ad to collector.\n");
		return false;
	}
	return true;
}

void
Scheduler::sendAlives( int /* timerID */ )
{
	match_rec	*mrec;
	int		  	numsent=0;
	bool starter_handles_alives = param_boolean("STARTER_HANDLES_ALIVES",true);

		/*
		  we need to timestamp any job ad with the last time we sent a
		  keepalive if the claim is active (i.e. there's a shadow for
		  it).  this way, if we've been disconnected, we know how long
		  it was since our last attempt to communicate.  we need to do
		  this *before* we actually send the keep alives, so that if
		  we're killed in the middle of this operation, we'll err on
		  the side of thinking the job might still be alive and
		  waiting a little longer to give up and start it elsewhere,
		  instead of potentially starting it on a new host while it's
		  still running on the last one.  so, we actually iterate
		  through the mrec's twice, first to see which ones we *would*
		  send a keepalive to and to write the timestamp into the job
		  queue, and then a second time to actually send the
		  keepalives.  we write all the timestamps to the job queue
		  within a transaction not because they're logically together
		  and they need to be atomically commited, but instead as a
		  performance optimization.  we don't want to wait for the
		  expensive fsync() operation for *every* claim we're trying
		  to keep alive.  ideally, the qmgmt code will only do a
		  single fsync() if these are all written within a single
		  transaction...  2003-12-07 Derek <wright@cs.wisc.edu>
		*/

	time_t now = time(nullptr);
	BeginTransaction();
	matches->startIterations();
	while (matches->iterate(mrec) == 1) {
		if( mrec->status == M_ACTIVE ) {
			time_t renew_time = 0;
			if ( starter_handles_alives && 
				 mrec->shadowRec && mrec->shadowRec->pid > 0 ) 
			{
				// If we're trusting the existance of the shadow to 
				// keep the claim alive (because of kernel sockopt keepalives),
				// set ATTR_LAST_JOB_LEASE_RENEWAL to the current time.
				renew_time = now;
			} else if ( mrec->m_startd_sends_alives ) {
				// if the startd sends alives, then the ATTR_LAST_JOB_LEASE_RENEWAL
				// is updated someplace else in RAM only when we receive a keepalive
				// ping from the startd.  So here
				// we just want to read it out of RAM and set it via SetAttributeInt
				// so it is written to disk.  Doing things this way allows us
				// to update the queue persistently all in one transaction, even
				// if startds are sending updates asynchronously.  -Todd Tannenbaum 
				GetAttributeInt(mrec->cluster,mrec->proc,
								ATTR_LAST_JOB_LEASE_RENEWAL,&renew_time);
			} else {
				// If we're sending the alives, then we need to set
				// ATTR_LAST_JOB_LEASE_RENEWAL to the current time.
				renew_time = now;
			}
			SetAttributeInt( mrec->cluster, mrec->proc, 
							 ATTR_LAST_JOB_LEASE_RENEWAL, renew_time ); 
		}
	}
	CommitNonDurableTransactionOrDieTrying();

	matches->startIterations();
	while (matches->iterate(mrec) == 1) {
		if( mrec->m_startd_sends_alives == false &&
			( mrec->status == M_ACTIVE || mrec->status == M_CLAIMED ) ) {

			if( sendAlive( mrec ) ) {
				numsent++;
			}
		}

		// If we have a shadow and are using the new alive protocol,
		// check whether the lease has expired. If it has, kill the match
		// and the shadow.
		// TODO Kill the match if the lease is expired when there is no
		//   shadow. This is low priority, since we'll notice the lease
		//   expiration when we try to start another job.
		if ( mrec->m_startd_sends_alives == true && mrec->status == M_ACTIVE &&
			 mrec->shadowRec && mrec->shadowRec->pid > 0 ) {
			int lease_duration = -1;
			time_t last_lease_renewal = -1;
			GetAttributeInt( mrec->cluster, mrec->proc,
							 ATTR_JOB_LEASE_DURATION, &lease_duration );
			GetAttributeInt( mrec->cluster, mrec->proc,
							 ATTR_LAST_JOB_LEASE_RENEWAL, &last_lease_renewal );

			// If the job has no lease attribute, the startd sets the
			// claim lease to 6 times the alive_interval we sent when we
			// requested the claim.
			if ( lease_duration <= 0 ) {
				lease_duration = 6 * alive_interval;
			}

			if ( last_lease_renewal + lease_duration < now ) {
				// The claim lease has expired. Kill the match
				// and the shadow, but make the job requeue and
				// don't try to notify the startd.
				shadow_rec *srec = mrec->shadowRec;
				ASSERT( srec );
				mrec->needs_release_claim = false;
				DelMrec( mrec );
				jobExitCode( srec->job_id, JOB_RECONNECT_FAILED );
				srec->exit_already_handled = true;
				daemonCore->Send_Signal( srec->pid, SIGKILL );
			}
		}
	}
	if( numsent ) { 
		dprintf( D_PROTOCOL, "## 6. (Done sending alive messages to "
				 "%d startds)\n", numsent );
	}

	// Just so we don't have to deal with a seperate DC timer for
	// this, just call the dedicated_scheduler's version of the
	// same thing so we keep all of those claims alive, too.
	dedicated_scheduler.sendAlives();
}

void
Scheduler::HadException( match_rec* mrec ) 
{
	if( !mrec ) {
			// If there's no mrec, we can't do anything.
		return;
	}
	mrec->num_exceptions++;
	if( mrec->num_exceptions >= MaxExceptions ) {
		dprintf( D_ERROR, 
				 "Match for cluster %d has had %d shadow exceptions, relinquishing.\n",
				 mrec->cluster, mrec->num_exceptions );
		DelMrec(mrec);
	}
}

//
// publish()
// Populates the ClassAd for the schedd
//
int
Scheduler::publish( ClassAd *cad ) {
	int ret = (int)true;
	char *temp;
	
		// -------------------------------------------------------
		// Many of these might not be necessary for the 
		// general case of publish() and should probably be
		// moved back into dumpState()
		// -------------------------------------------------------
	cad->Assign( ATTR_SCHEDD_IP_ADDR, daemonCore->InfoCommandSinfulString() );
	cad->Assign( "MyShadowSockname", MyShadowSockName );
	cad->Assign( "SchedDInterval", (int)SchedDInterval.getDefaultInterval() );
	cad->Assign( "QueueCleanInterval", QueueCleanInterval );
	cad->Assign( "JobStartDelay", JobStartDelay );
	cad->Assign( "JobStartCount", JobStartCount );
	cad->Assign( "JobsThisBurst", JobsThisBurst );
	cad->Assign( "MaxJobsRunning", MaxJobsRunning );
	cad->Assign( "MaxJobsSubmitted", MaxJobsSubmitted );
	cad->Assign( "MaxJobsPerOwner", MaxJobsPerOwner );
	if (MaxRunningSchedulerJobsPerOwner >= 0 && MaxRunningSchedulerJobsPerOwner < MaxJobsPerOwner) {
		cad->Assign( "MaxRunningSchedulerJobsPerOwner", MaxRunningSchedulerJobsPerOwner );
	}
	cad->Assign( "MaxJobsPerSubmission", MaxJobsPerSubmission );
	cad->Assign( "JobsStarted", JobsStarted );
	cad->Assign( "SwapSpace", SwapSpace );
	cad->Assign( "ShadowSizeEstimate", ShadowSizeEstimate );
	cad->Assign( "SwapSpaceExhausted", SwapSpaceExhausted );
	cad->Assign( "ReservedSwap", ReservedSwap );
	cad->Assign( "JobsIdle", JobsIdle );
	cad->Assign( "JobsRunning", JobsRunning );
	cad->Assign( "BadCluster", BadCluster );
	cad->Assign( "BadProc", BadProc );
	cad->Assign( "NumOwners", NumUniqueOwners );
	cad->Assign( "NumSubmitters", NumSubmitters );
	cad->Assign( "NegotiationRequestTime", NegotiationRequestTime  );
	cad->Assign( "ExitWhenDone", ExitWhenDone );
	cad->Assign( "StartJobTimer", StartJobTimer );
	if ( CondorAdministrator ) {
		cad->Assign( "CondorAdministrator", CondorAdministrator );
	}
	cad->Assign( "AccountantName", AccountantName );
	cad->Assign( "UidDomain", UidDomain );
	cad->Assign( "AccountingDomain", AccountingDomain );
	cad->Assign( "MinFlockLevel", MinFlockLevel );
	cad->Assign( "MaxFlockLevel", MaxFlockLevel );
	cad->Assign( "FlockLevel", FlockLevel );
	cad->Assign( "MaxExceptions", MaxExceptions );
	cad->Assign( "HasContainer", true );
	cad->Assign( "HasSIF", true );
	
		// -------------------------------------------------------
		// Basic Attributes
		// -------------------------------------------------------

	temp = param( "ARCH" );
	if ( temp ) {
		cad->Assign( ATTR_ARCH, temp );
		free( temp );
	}

	temp = param( "OPSYS" );
	if ( temp ) {
		cad->Assign( ATTR_OPSYS, temp );
		free( temp );
	}

	temp = param( "OPSYSVER" );
	if ( temp ) {
		cad->Assign( ATTR_OPSYSVER, temp );
		free( temp );
	}

	temp = param( "OPSYSANDVER" );
	if ( temp ) {
		cad->Assign( ATTR_OPSYS_AND_VER, temp );
		free( temp );
	}
	
	unsigned long phys_mem = sysapi_phys_memory( );
	cad->Assign( ATTR_MEMORY, (int)phys_mem );
	
	cad->Assign( ATTR_DISK, sysapi_disk_space(this->LocalUnivExecuteDir) );

	cad->Assign( ATTR_CPUS, 1 );

		// -------------------------------------------------------
		// Local Universe Attributes
		// -------------------------------------------------------
	cad->Assign( ATTR_TOTAL_LOCAL_IDLE_JOBS,
				 this->LocalUniverseJobsIdle );
	cad->Assign( ATTR_TOTAL_LOCAL_RUNNING_JOBS,
				 this->LocalUniverseJobsRunning );
	
		//
		// Limiting the # of local universe jobs allowed to start
		//
	if ( this->StartLocalUniverse ) {
		cad->AssignExpr( ATTR_START_LOCAL_UNIVERSE, this->StartLocalUniverse );
	}

		// -------------------------------------------------------
		// Scheduler Universe Attributes
		// -------------------------------------------------------
	cad->Assign( ATTR_TOTAL_SCHEDULER_IDLE_JOBS,
				 this->SchedUniverseJobsIdle );
	cad->Assign( ATTR_TOTAL_SCHEDULER_RUNNING_JOBS,
				 this->SchedUniverseJobsRunning );
	
		//
		// Limiting the # of scheduler universe jobs allowed to start
		//
	if ( this->StartSchedulerUniverse ) {
		cad->AssignExpr( ATTR_START_SCHEDULER_UNIVERSE,
						 this->StartSchedulerUniverse );
	}
	
	return ( ret );
}

int
Scheduler::get_job_connect_info_handler(int cmd, Stream* s) {
		// This command does blocking network connects to the startd
		// and starter.  For now, use fork to avoid blocking the schedd.
		// Eventually, use threads.
	ForkStatus fork_status;
	fork_status = schedd_forker.NewJob();
	if( fork_status == FORK_PARENT ) {
		return TRUE;
	}

	int rc = get_job_connect_info_handler_implementation(cmd,s);
	if( fork_status == FORK_CHILD ) {
		schedd_forker.WorkerDone(); // never returns
		ASSERT( false );
	}
	return rc;
}

int
Scheduler::get_job_connect_info_handler_implementation(int, Stream* s) {
	Sock *sock = (Sock *)s;
	const OwnerInfo * sock_owner = nullptr;
	ClassAd input;
	ClassAd reply;
	PROC_ID jobid;
	std::string error_msg;
	JobQueueJob *jobad;
	int job_status = -1;
	match_rec *mrec = NULL;
	std::string job_claimid_buf;
	char const *job_claimid = NULL;
	char const *match_sec_session_id = NULL;
	int universe = -1;
	std::string startd_name;
	std::string starter_addr;
	std::string starter_claim_id;
	std::string job_owner_session_info;
	std::string starter_version;
	bool retry_is_sensible = false;
	bool job_is_suitable = false;
	ClassAd starter_ad;
	int ltimeout = 120;

		// This command is called for example by condor_ssh_to_job
		// in order to establish a security session for communication
		// with the starter.  The caller must be authorized to act
		// as the owner of the job, which is verified below.  The starter
		// then checks that this schedd is indeed in possession of the
		// secret claim id associated with this running job.


		// force authentication
	if( !sock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ||
			! sock->getFullyQualifiedUser() )
		{
			dprintf( D_ALWAYS,
					 "GET_JOB_CONNECT_INFO: authentication failed: %s\n", 
					 errstack.getFullText().c_str() );
			return FALSE;
		}
	}
	sock_owner = EffectiveUserRec(sock);

	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,
				"Failed to receive input ClassAd for GET_JOB_CONNECT_INFO\n");
		return FALSE;
	}

	if( !input.LookupInteger(ATTR_CLUSTER_ID,jobid.cluster) ||
		!input.LookupInteger(ATTR_PROC_ID,jobid.proc) ) {
		error_msg = "Job id missing from GET_JOB_CONNECT_INFO request";
		goto error_wrapup;
	}

	dprintf(D_AUDIT, *sock, "GET_JOB_CONNECT_INFO for job %d.%d\n", jobid.cluster, jobid.proc );

	input.LookupString(ATTR_SESSION_INFO,job_owner_session_info);

	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if( !jobad ) {
		formatstr(error_msg, "No such job: %d.%d", jobid.cluster, jobid.proc);
		goto error_wrapup;
	}

	if( !sock_owner || !UserCheck2(jobad,sock_owner) ) {
		formatstr(error_msg, "%s is not authorized for access to the starter for job %d.%d",
						  EffectiveUserName(sock), jobid.cluster, jobid.proc);
		goto error_wrapup;
	}

	jobad->LookupInteger(ATTR_JOB_STATUS,job_status);
	jobad->LookupInteger(ATTR_JOB_UNIVERSE,universe);

	job_is_suitable = false;
	switch( universe ) {
	case CONDOR_UNIVERSE_GRID:
	case CONDOR_UNIVERSE_SCHEDULER:
		break; // these universes not supported
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
	{
		std::string claim_ids;
		std::string remote_hosts_string;
		int subproc = -1;
		if( GetPrivateAttributeString(jobid.cluster,jobid.proc,ATTR_CLAIM_IDS,claim_ids)==0 &&
			jobad->LookupString(ATTR_ALL_REMOTE_HOSTS,remote_hosts_string) ) {
			std::vector<std::string> claim_idlist = split(claim_ids, ",");
			std::vector<std::string> remote_hosts = split(remote_hosts_string, ",");
			input.LookupInteger(ATTR_SUB_PROC_ID,subproc);
			if( claim_idlist.size() == 1 && subproc == -1 ) {
				subproc = 0;
			}
			if( subproc == -1 || subproc >= (int)claim_idlist.size() || subproc >= (int)remote_hosts.size() ) {
				formatstr(error_msg, "This is a parallel job.  Please specify job %d.%d.X where X is an integer from 0 to %d.",jobid.cluster,jobid.proc,(int)claim_idlist.size()-1);
				goto error_wrapup;
			}
			else {
				mrec = dedicated_scheduler.FindMrecByClaimID(claim_idlist[subproc].c_str());
				startd_name = remote_hosts[subproc];
				if( mrec && mrec->peer ) {
					job_is_suitable = true;
				}
			}
		}
		else if (job_status != RUNNING && job_status != TRANSFERRING_OUTPUT) {
			retry_is_sensible = true;
		}
		break;
	}
	case CONDOR_UNIVERSE_LOCAL: {
		shadow_rec *srec = FindSrecByProcID(jobid);
		if( !srec ) {
			retry_is_sensible = true;
		}
		else {
			startd_name = get_local_fqdn();
				// NOTE: this does not get the CCB address of the starter.
				// If there is one, we'll get it when we call the starter
				// below.  (We don't need it ourself, because it is on the
				// same machine, but our client might not be.)
			const char *addr = daemonCore->InfoCommandSinfulString( srec->pid );
			starter_addr = addr ? addr : "";
			if( starter_addr.empty() ) {
				retry_is_sensible = true;
				break;
			}
			starter_ad.Assign(ATTR_STARTER_IP_ADDR,starter_addr);
			GetPrivateAttributeString(jobid.cluster,jobid.proc,ATTR_CLAIM_ID,job_claimid_buf);
			job_claimid = job_claimid_buf.c_str();
			match_sec_session_id = NULL; // no match sessions for local univ
			job_is_suitable = true;
		}
		break;
	}
	default:
	{
		mrec = FindMrecByJobID(jobid);
		if( mrec && mrec->peer ) {
			jobad->LookupString(ATTR_REMOTE_HOST,startd_name);
			job_is_suitable = true;
		}
		else if (job_status != RUNNING && job_status != TRANSFERRING_OUTPUT) {
			retry_is_sensible = true;
		}
		break;
	}
	}

		// machine ad can't be reliably supplied (e.g. after reconnect),
		// so best to never supply it here
	if( job_is_suitable && 
		!param_boolean("SCHEDD_ENABLE_SSH_TO_JOB",true,true,jobad,NULL) )
	{
		formatstr(error_msg, "Job %d.%d is denied by SCHEDD_ENABLE_SSH_TO_JOB.",
						  jobid.cluster,jobid.proc);
		goto error_wrapup;
	}


	if( !job_is_suitable )
	{
		if( !retry_is_sensible ) {
				// this must be a job universe that we don't support
			formatstr(error_msg, "Job %d.%d does not support remote access.",
							  jobid.cluster,jobid.proc);
		}
		else {
			reply.Assign( ATTR_JOB_STATUS, job_status );
			if( job_status == HELD ) {
				CopyAttribute( ATTR_HOLD_REASON, reply, *jobad );
			}
			formatstr(error_msg, "Job %d.%d is not running.",
							  jobid.cluster,jobid.proc);
		}
		goto error_wrapup;
	}

	if( mrec ) { // locate starter by calling startd
		std::string global_job_id;
		std::string startd_addr = mrec->peer;

		DCStartd startd(startd_name.c_str(),NULL,startd_addr.c_str(),mrec->claim_id.claimId() );

		jobad->LookupString(ATTR_GLOBAL_JOB_ID,global_job_id);

		if( !startd.locateStarter(global_job_id.c_str(),mrec->claim_id.claimId(),daemonCore->publicNetworkIpAddr(),&starter_ad,ltimeout) )
		{
			error_msg = "Failed to get address of starter for this job";
			retry_is_sensible = true; // maybe shadow hasn't activated starter yet?
			goto error_wrapup;
		}
		job_claimid = mrec->claim_id.claimId();
		if (mrec->use_sec_session) {
			match_sec_session_id = mrec->claim_id.secSessionId();
		}
	}

		// now connect to the starter and create a security session for
		// our client to use
	{
//		starter_ad.LookupString(ATTR_STARTER_IP_ADDR,starter_addr);

		DCStarter starter;
		if( !starter.initFromClassAd(&starter_ad) ) {
			error_msg = "Failed to read address of starter for this job";
			retry_is_sensible = true; // maybe shadow hasn't activated starter yet?
			goto error_wrapup;
		}

		if( !starter.createJobOwnerSecSession(ltimeout,job_claimid,match_sec_session_id,job_owner_session_info.c_str(),starter_claim_id,error_msg,starter_version,starter_addr) ) {
			retry_is_sensible = true; // This can mean the starter is blocked fetching a docker image
			goto error_wrapup; // error_msg already set
		}
	}

	reply.Assign(ATTR_RESULT,true);
	reply.Assign(ATTR_STARTER_IP_ADDR,starter_addr);
	reply.Assign(ATTR_CLAIM_ID,starter_claim_id);
	reply.Assign(ATTR_VERSION,starter_version);
	reply.Assign(ATTR_REMOTE_HOST,startd_name);
	if( !putClassAd(s, reply) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,
				"Failed to send response to GET_JOB_CONNECT_INFO\n");
	}

	dprintf(D_FULLDEBUG,"Produced connect info for %s job %d.%d startd %s.\n",
			sock->getFullyQualifiedUser(), jobid.cluster, jobid.proc,
			starter_addr.c_str() );

	return TRUE;

 error_wrapup:
	dprintf(D_AUDIT|D_ERROR_ALSO, *sock, "GET_JOB_CONNECT_INFO failed: %s\n",error_msg.c_str() );
	reply.Assign(ATTR_RESULT,false);
	reply.Assign(ATTR_ERROR_STRING,error_msg);
	if( retry_is_sensible ) {
		reply.Assign(ATTR_RETRY,retry_is_sensible);
	}
	if( !putClassAd(s, reply) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,
				"Failed to send error response to GET_JOB_CONNECT_INFO\n");
	}
	return FALSE;
}

#if 0 // TODO: this function is untested, but probably works...
int
fixAttrUser(JobQueueJob *job, const JOB_ID_KEY & /*jid*/, void *oldUidDomain_raw)
{
	YourString oldUidDomain(static_cast<char *>(oldUidDomain_raw));
	std::string user;

	if (job->LookupString(ATTR_USER, user) && domain_of_user(user.c_str(),"#") == oldUidDomain) {
		size_t at_sign = user.find_last_of('@');
		if (at_sign != std::string::npos) {
			user.erase(at_sign+1);
			user += scheduler.uidDomain();
			job->Assign(ATTR_USER, user);
		}
	}

	return 0;
}
#endif

void
fixReasonAttrs( PROC_ID job_id, JobAction action )
{
		/* 
		   Fix the given job so that any existing reason attributes in
		   the ClassAd are modified and changed so that everything
		   makes sense.  For example, if we're releasing a job, we
		   want to move the HoldReason to LastHoldReason...

		   CAREFUL!  This method is called from within a queue
		   management transaction...
		*/
	switch( action ) {

	case JA_HOLD_JOBS:
		moveStrAttr( job_id, ATTR_RELEASE_REASON, 
					 ATTR_LAST_RELEASE_REASON, false );
		break;

	case JA_RELEASE_JOBS:
		moveStrAttr( job_id, ATTR_HOLD_REASON, ATTR_LAST_HOLD_REASON,
					 true );
		moveIntAttr( job_id, ATTR_HOLD_REASON_CODE, 
					 ATTR_LAST_HOLD_REASON_CODE, true );
		moveIntAttr( job_id, ATTR_HOLD_REASON_SUBCODE, 
					 ATTR_LAST_HOLD_REASON_SUBCODE, true );
		DeleteAttribute(job_id.cluster,job_id.proc,
					 ATTR_JOB_STATUS_ON_RELEASE);
		break;

	case JA_REMOVE_JOBS:
		moveStrAttr( job_id, ATTR_HOLD_REASON, ATTR_LAST_HOLD_REASON,
					 false );
		moveIntAttr( job_id, ATTR_HOLD_REASON_CODE, 
					 ATTR_LAST_HOLD_REASON_CODE, false );
		moveIntAttr( job_id, ATTR_HOLD_REASON_SUBCODE, 
					 ATTR_LAST_HOLD_REASON_SUBCODE, false );
		DeleteAttribute(job_id.cluster,job_id.proc,
					 ATTR_JOB_STATUS_ON_RELEASE);
		break;

	//Don't do anything for the items below, here for completeness	
	//case JA_SUSPEND_JOBS: 
	//case JA_CONTINUE_JOBS:

	default:
		return;
	}
}


bool
moveStrAttr( PROC_ID job_id, const char* old_attr, const char* new_attr,
			 bool verbose )
{
	std::string value;
	int rval;

	if( GetAttributeString(job_id.cluster, job_id.proc,
						   old_attr, value) < 0 ) { 
		if( verbose ) {
			dprintf( D_FULLDEBUG, "No %s found for job %d.%d\n",
					 old_attr, job_id.cluster, job_id.proc );
		}
		return false;
	}
	
	rval = SetAttributeString( job_id.cluster, job_id.proc, new_attr,
							   value.c_str() ); 

	if( rval < 0 ) { 
		if( verbose ) {
			dprintf( D_FULLDEBUG, "Can't set %s for job %d.%d\n",
					 new_attr, job_id.cluster, job_id.proc );
		}
		return false;
	}
		// If we successfully set the new attr, we can delete the old. 
	DeleteAttribute( job_id.cluster, job_id.proc, old_attr );
	return true;
}

bool
moveIntAttr( PROC_ID job_id, const char* old_attr, const char* new_attr,
			 bool verbose )
{
	long long value;
	int rval;

	if( GetAttributeInt(job_id.cluster, job_id.proc, old_attr, &value) < 0 ) {
		if( verbose ) {
			dprintf( D_FULLDEBUG, "No %s found for job %d.%d\n",
					 old_attr, job_id.cluster, job_id.proc );
		}
		return false;
	}
	
	rval = SetAttribute( job_id.cluster, job_id.proc, new_attr,
						 std::to_string(value).c_str() );

	if( rval < 0 ) { 
		if( verbose ) {
			dprintf( D_FULLDEBUG, "Can't set %s for job %d.%d\n",
					 new_attr, job_id.cluster, job_id.proc );
		}
		return false;
	}
		// If we successfully set the new attr, we can delete the old. 
	DeleteAttribute( job_id.cluster, job_id.proc, old_attr );
	return true;
}

/*
Abort this job by changing the state to removed,
telling the shadow (gridmanager) to shut down,
and destroying the data structure itself.
Note that in some configurations abort_job_myself
will have already called DestroyProc, but that's ok, because
the cluster.proc numbers are not re-used.
Does not start or end a transaction.
*/

static bool
abortJobRaw( int cluster, int proc, const char *reason )
{
	PROC_ID job_id;

	job_id.cluster = cluster;
	job_id.proc = proc;

	if( SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, REMOVED) < 0 ) {
		dprintf(D_ALWAYS,"Couldn't change state of job %d.%d\n",cluster,proc);
		return false;
	}

	// Add the remove reason to the job's attributes
	if( reason && *reason ) {
		if ( SetAttributeString( cluster, proc, ATTR_REMOVE_REASON,
								 reason ) < 0 ) {
			dprintf( D_ALWAYS, "WARNING: Failed to set %s to \"%s\" for "
					 "job %d.%d\n", ATTR_REMOVE_REASON, reason, cluster,
					 proc );
		}
	}

	fixReasonAttrs( job_id, JA_REMOVE_JOBS );

	// enqueue the job abort
	scheduler.enqueueActOnJobMyself(job_id, JA_REMOVE_JOBS, true);

	return true;
}

/*
Abort a job by shutting down the shadow, changing the job state,
writing to the user log, and updating the job queue.
Performs a complete transaction if desired.
*/

bool
abortJob( int cluster, int proc, const char *reason, bool use_transaction )
{
	bool result;

	if( use_transaction ) {
		BeginTransaction();
	}

	result = abortJobRaw( cluster, proc, reason );

	if(use_transaction) {
		if(result) {
			CommitTransactionOrDieTrying();
		} else {
			AbortTransaction();
		}
	}

	return result;
}

bool
abortJobsByConstraint( const char *constraint,
					   const char *reason,
					   bool use_transaction )
{
	bool result = true;

	std::vector<PROC_ID> jobs;
	int job_count;

	dprintf(D_FULLDEBUG, "abortJobsByConstraint: '%s'\n", constraint);

	if ( use_transaction ) {
		BeginTransaction();
	}

	job_count = 0;
	ClassAd *ad = GetNextJobByConstraint(constraint, 1);
	while ( ad ) {
		int cluster, proc;
		if (!ad->LookupInteger(ATTR_CLUSTER_ID, cluster) ||
			!ad->LookupInteger(ATTR_PROC_ID, proc)) {

			result = false;
			job_count = 0;
			break;
		}
		jobs.emplace_back(cluster, proc);

		dprintf(D_FULLDEBUG, "remove by constraint matched: %d.%d\n", cluster, proc);

		job_count++;

		ad = GetNextJobByConstraint(constraint, 0);
	}

	job_count--;
	std::vector<PROC_ID> removedJobs;
	while ( job_count >= 0 ) {
		dprintf(D_FULLDEBUG, "removing: %d.%d\n",
				jobs[job_count].cluster, jobs[job_count].proc);

		bool tmpResult = abortJobRaw(jobs[job_count].cluster,
									   jobs[job_count].proc,
									   reason);
		if ( tmpResult ) {
			removedJobs.emplace_back(jobs[job_count].cluster, jobs[job_count].proc);
		}
		result = result && tmpResult;
		job_count--;
	}

	if ( use_transaction ) {
		if ( result ) {
			CommitTransactionOrDieTrying();
		} else {
			AbortTransaction();
		}
	}

	return result;
}

void
incrementJobAdAttr(int cluster, int proc, const char* attrName, const char *nestedAdAttrName)
{
	long long val = 0;
	if (!attrName || !attrName[0]) return;
	if (nestedAdAttrName) {
		// Here we are going to increment an attribute in an ad nested inside the job ad.

		// First, get the nested ad as a string, and parse to a classad
		// This nested ad just has attrname=integer attributes - since it
		// will not contain any string values we do not need to worry about
		// setting up the parser for old vs new quoting rules.
		classad::ClassAdParser parser;
		char *adAsString = nullptr;
		ClassAd ad;
		GetAttributeExprNew(cluster, proc, nestedAdAttrName, &adAsString);
		if (adAsString) {
			parser.ParseClassAd(adAsString, ad, true);
			free(adAsString);
		}

		// Next update the unparsed ad
		ad.LookupInteger(attrName, val);
		ad.Assign(attrName, ++val);

		// Finally unparse ad back to a string, and write it back to the job log.
		classad::ClassAdUnParser unparser;
		std::string result;
		unparser.Unparse(result, &ad);
		SetAttribute(cluster, proc, nestedAdAttrName, result.c_str());
	} else {
		// Here we are going to increment an attribute in the job ad.

		GetAttributeInt(cluster, proc, attrName, &val);
		SetAttributeInt(cluster, proc, attrName, ++val);
	}

}

/*
Hold a job by stopping the shadow, changing the job state,
writing to the user log, and updating the job queue.
Does not start or end a transaction.
*/

static bool
holdJobRaw( int cluster, int proc, const char* reason,
			int reason_code, int reason_subcode,
		 bool email_user,
		 bool email_admin, bool system_hold )
{
	int status = -1;
	PROC_ID tmp_id;
	tmp_id.cluster = cluster;
	tmp_id.proc = proc;

	if ( cluster < 1 || proc < 0 ) {
		dprintf(D_FULLDEBUG,"holdJobRaw failed, job id (%d.%d) is malformed\n",
			cluster, proc);
		return false;
	}

	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) < 0 ) {   
		dprintf( D_ALWAYS, "Job %d.%d has no %s attribute.  Can't hold\n",
				 cluster, proc, ATTR_JOB_STATUS );
		return false;
	}
	if( status == HELD ) {
		// Allow a held job's reason code to be changed to UserRequest
		int old_reason_code = 0;
		if( reason_code == CONDOR_HOLD_CODE::UserRequest ) {
			GetAttributeInt(cluster, proc, ATTR_HOLD_REASON_CODE, &old_reason_code);
		}
		if( old_reason_code == CONDOR_HOLD_CODE::UserRequest ||
		    old_reason_code == CONDOR_HOLD_CODE::SpoolingInput ||
		    reason_code != CONDOR_HOLD_CODE::UserRequest ) {

			dprintf( D_ALWAYS, "Job %d.%d is already on hold\n",
			         cluster, proc );
			return false;
		}
	}

	if( reason ) {
		std::string fixed_reason;
		if( reason[0] == '"' ) {
			fixed_reason += reason;
		} else {
			fixed_reason += '"';
			fixed_reason += reason;
			fixed_reason += '"';
		}
		if( SetAttribute(cluster, proc, ATTR_HOLD_REASON, 
						 fixed_reason.c_str()) < 0 ) {
			dprintf( D_ALWAYS, "WARNING: Failed to set %s to \"%s\" for "
					 "job %d.%d\n", ATTR_HOLD_REASON, reason, cluster,
					 proc );
		}
	}

	if( SetAttributeInt(cluster, proc, ATTR_HOLD_REASON_CODE, reason_code) < 0 ) {
		dprintf( D_ALWAYS, "ERROR: Failed to set %s to %d for "
				 "job %d.%d\n", ATTR_HOLD_REASON_CODE, reason_code, cluster, proc );
		return false;
	}

	if( SetAttributeInt(cluster, proc, ATTR_HOLD_REASON_SUBCODE, reason_subcode) < 0 ) {
		dprintf( D_ALWAYS, "ERROR: Failed to set %s to %d for "
				 "job %d.%d\n", ATTR_HOLD_REASON_SUBCODE, reason_subcode, cluster, proc );
		return false;
	}

	if( SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, HELD) < 0 ) {
		dprintf( D_ALWAYS, "ERROR: Failed to set %s to HELD for "
				 "job %d.%d\n", ATTR_JOB_STATUS, cluster, proc );
		return false;
	}

	fixReasonAttrs( tmp_id, JA_HOLD_JOBS );

	if( SetAttributeInt(cluster, proc, ATTR_ENTERED_CURRENT_STATUS, 
						time(0)) < 0 ) {
		dprintf( D_ALWAYS, "WARNING: Failed to set %s for job %d.%d\n",
				 ATTR_ENTERED_CURRENT_STATUS, cluster, proc );
	}

	if( SetAttributeInt(cluster, proc, ATTR_LAST_SUSPENSION_TIME, 0) < 0 ) {
		dprintf( D_ALWAYS, "WARNING: Failed to set %s for job %d.%d\n",
				 ATTR_LAST_SUSPENSION_TIME, cluster, proc );
	}

	// Update count in job ad of "system holds", whatever that is supposed to mean
	// (this is a legacy attribute from way back when, keep it the same as it was for now)
	if ( system_hold ) {
		incrementJobAdAttr(cluster, proc, ATTR_NUM_SYSTEM_HOLDS);
	}

	dprintf( D_ALWAYS, "Job %d.%d put on hold: %s\n", cluster, proc,
			 reason );

	// replacing this with the call to enqueueActOnJobMyself
	// in holdJob AFTER the transaction; otherwise the job status
	// doesn't get properly updated for some reason
	//abort_job_myself( tmp_id, JA_HOLD_JOBS, true );

		// finally, email anyone our caller wants us to email.
	if( email_user || email_admin ) {
		ClassAd* job_ad;
		job_ad = GetJobAd( cluster, proc );
		if( ! job_ad ) {
			dprintf( D_ALWAYS, "ERROR: Can't find ClassAd for job %d.%d "
					 "can't send email to anyone about it\n", cluster,
					 proc );
				// even though we can't send the email, we still held
				// the job, so return true.
			return true;  
		}

		if( email_user ) {
			Email email;
			email.sendHold( job_ad, reason );
		}
		if( email_admin ) {
			Email email;
			email.sendHoldAdmin( job_ad, reason );
		}
		FreeJobAd( job_ad );
	}
	return true;
}

/*
Hold a job by shutting down the shadow, changing the job state,
writing to the user log, and updating the job queue.
Performs a complete transaction if desired.
*/

bool
holdJob( int cluster, int proc, const char* reason,
		 int reason_code, int reason_subcode,
		 bool use_transaction, bool email_user,
		 bool email_admin, bool system_hold, bool write_to_user_log )
{
	bool result;

	if(use_transaction) {
		BeginTransaction();
	}

	result = holdJobRaw(cluster,proc,reason,reason_code,reason_subcode,email_user,email_admin,system_hold);

	if(use_transaction) {
		if(result) {
			CommitTransactionOrDieTrying();
		} else {
			AbortTransaction();
		}
	}

	// need this now to take the place of abort_job_myself
	// within holdJobRaw to ensure correct job status change
	if (result) {
		PROC_ID id;
		id.cluster = cluster;
		id.proc = proc;
		scheduler.enqueueActOnJobMyself(id,JA_HOLD_JOBS, write_to_user_log);
	}

	return result;
}

/*
Release a job by changing the job state,
writing to the user log, and updating the job queue.
Does not start or end a transaction.
*/

static bool
releaseJobRaw( int cluster, int proc, const char* reason,
		 bool email_user,
		 bool email_admin, bool write_to_user_log )
{
	int status = -1;
	PROC_ID tmp_id;
	tmp_id.cluster = cluster;
	tmp_id.proc = proc;


	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) < 0 ) {   
		dprintf( D_ALWAYS, "Job %d.%d has no %s attribute.  Can't release\n",
				 cluster, proc, ATTR_JOB_STATUS );
		return false;
	}
	if( status != HELD ) {
		return false;
	}

	if( reason ) {
		std::string fixed_reason;
		if( reason[0] == '"' ) {
			fixed_reason += reason;
		} else {
			fixed_reason += '"';
			fixed_reason += reason;
			fixed_reason += '"';
		}
		if( SetAttribute(cluster, proc, ATTR_RELEASE_REASON, 
						 fixed_reason.c_str()) < 0 ) {
			dprintf( D_ALWAYS, "WARNING: Failed to set %s to \"%s\" for "
					 "job %d.%d\n", ATTR_RELEASE_REASON, reason, cluster,
					 proc );
		}
	}

	int status_on_release = IDLE;
	GetAttributeInt(cluster,proc,ATTR_JOB_STATUS_ON_RELEASE,&status_on_release);
	if( SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, 
			status_on_release) < 0 ) 
	{
		dprintf( D_ALWAYS, "ERROR: Failed to set %s to status %d for job %d.%d\n", 
				ATTR_JOB_STATUS, status_on_release,cluster, proc );
		return false;
	}

	fixReasonAttrs( tmp_id, JA_RELEASE_JOBS );

	if( SetAttributeInt(cluster, proc, ATTR_ENTERED_CURRENT_STATUS, 
						time(0)) < 0 ) {
		dprintf( D_ALWAYS, "WARNING: Failed to set %s for job %d.%d\n",
				 ATTR_ENTERED_CURRENT_STATUS, cluster, proc );
	}

	if( SetAttributeInt(cluster, proc, ATTR_LAST_SUSPENSION_TIME, 0 ) < 0 ) {
		dprintf( D_ALWAYS, "WARNING: Failed to set %s for job %d.%d\n",
				 ATTR_LAST_SUSPENSION_TIME, cluster, proc );
	}

	if ( write_to_user_log ) {
		scheduler.WriteReleaseToUserLog(tmp_id);
	}

	dprintf( D_ALWAYS, "Job %d.%d released from hold: %s\n", cluster, proc,
			 reason );

		// finally, email anyone our caller wants us to email.
	if( email_user || email_admin ) {
		ClassAd* job_ad;
		job_ad = GetJobAd( cluster, proc );
		if( ! job_ad ) {
			dprintf( D_ALWAYS, "ERROR: Can't find ClassAd for job %d.%d "
					 "can't send email to anyone about it\n", cluster,
					 proc );
				// even though we can't send the email, we still held
				// the job, so return true.
			return true;  
		}

		if( email_user ) {
			Email email;
			email.sendRelease( job_ad, reason );
		}
		if( email_admin ) {
			Email email;
			email.sendReleaseAdmin( job_ad, reason );
		}
		FreeJobAd( job_ad );
	}
	return true;
}

/*
Release a job by changing the job state,
writing to the user log, and updating the job queue.
Performs a complete transaction if desired.
*/

bool
releaseJob( int cluster, int proc, const char* reason,
		 bool use_transaction, bool email_user,
		 bool email_admin, bool write_to_user_log )
{
	bool result;

	if(use_transaction) {
		BeginTransaction();
	}

	result = releaseJobRaw(cluster,proc,reason,email_user,email_admin,write_to_user_log);

	if(use_transaction) {
		if(result) {
			CommitTransactionOrDieTrying();
		} else {
			AbortTransaction();
		}
	}

	scheduler.needReschedule();

	return result;
}

bool setJobFactoryPauseAndLog(JobQueueCluster * cad, int pause_mode, int hold_code, const std::string & reason)
{
	if ( ! cad) return false;

	cad->Assign(ATTR_JOB_MATERIALIZE_PAUSED, pause_mode);
	if (reason.empty() || pause_mode == mmRunning) {
		cad->Delete(ATTR_JOB_MATERIALIZE_PAUSE_REASON);
	} else {
		if (strchr(reason.c_str(), '\n')) {
			// make sure that the reason  has no embedded newlines because if it does,
			// it will cause the SCHEDD to abort when it rotates the job log.
			std::string msg(reason);
			std::replace(msg.begin(), msg.end(), '\n', ' ');
			std::replace(msg.begin(), msg.end(), '\r', ' ');
			cad->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, msg);
		} else {
			cad->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, reason);
		}
	}

	if (cad->factory) {
		// make sure that the factory state is in sync with the pause mode
		CheckJobFactoryPause(cad->factory, pause_mode);
	}

	// log the change in pause state
	const char * reason_ptr = reason.empty() ? NULL : reason.c_str();
	scheduler.WriteFactoryPauseToUserLog(cad, hold_code, reason_ptr);
	return true;
}

// Throttle job starts, with bursts of JobStartCount jobs, every
// JobStartDelay seconds.  That is, start JobStartCount jobs as quickly as
// possible, then delay for JobStartDelay seconds.  The daemoncore timer is
// used to generate the job start bursts and delays.  This function should be
// used to generate the delays for all timer calls of StartJobHandler().  See
// the schedd WISDOM file for the need and rationale for job bursting and
// jobThrottle().
int
Scheduler::jobThrottle( void )
{
	int delay;

	if ( ++JobsThisBurst < JobStartCount ) {
		delay = 0;
	} else {
		JobsThisBurst = 0;
		delay = JobStartDelay;
	}

	if ( jobThrottleNextJobDelay > 0 ) {
		delay = MAX(delay,jobThrottleNextJobDelay);
		jobThrottleNextJobDelay = 0;
	}

	dprintf( D_FULLDEBUG, "start next job after %d sec, JobsThisBurst %d\n",
			delay, JobsThisBurst);
	return delay;
}

GridJobCounts *
Scheduler::GetGridJobCounts(UserIdentity user_identity) {
	GridJobCounts * gridcounts = 0;
	if( GridJobOwners.lookup(user_identity, gridcounts) == 0 ) {
		ASSERT(gridcounts);
		return gridcounts;
	}
	// No existing entry.
	GridJobCounts newcounts;
	GridJobOwners.insert(user_identity, newcounts);
	GridJobOwners.lookup(user_identity, gridcounts);
	ASSERT(gridcounts); // We just added it. Where did it go?
	return gridcounts;
}

int
Scheduler::jobIsFinishedHandler( ServiceData* data )
{
	CondorID* job_id = (CondorID*)data;
	if( ! job_id ) {
		return FALSE;
	}
	int cluster = job_id->_cluster;
	int proc = job_id->_proc;
	delete job_id;
	job_id = NULL;

		//
		// Remove the record from our cronTab lists
		// We do it here before we fire off any threads
		// so that we don't cause problems
		//
	PROC_ID id;
	id.cluster = cluster;
	id.proc    = proc;
	CronTab *cronTab;
	if ( this->cronTabs->lookup( id, cronTab ) >= 0 ) {
		if ( cronTab != NULL) {
			delete cronTab;
			this->cronTabs->remove(id);
		}
	}

	// This gets called on jobs with LeaveJobInQueue set, which we don't want.
	// doCheckpointCleanUp( cluster, proc );

	if( jobCleanupNeedsThread(cluster, proc) ) {
		dprintf( D_FULLDEBUG, "Job cleanup for %d.%d will block, "
				 "calling jobIsFinished() in a thread\n", cluster, proc );
		Create_Thread_With_Data( jobIsFinished, jobIsFinishedDone,
								 cluster, proc, NULL );
	} else {
			// don't need a thread, just call the blocking version
			// (which will return right away), and the reaper (which
			// will call DestroyProc())
		dprintf( D_FULLDEBUG, "Job cleanup for %d.%d will not block, "
				 "calling jobIsFinished() directly\n", cluster, proc );

		jobIsFinished( cluster, proc );
		jobIsFinishedDone( cluster, proc );
	}


	return TRUE;
}


bool
Scheduler::enqueueFinishedJob( int cluster, int proc )
{
	CondorID* id = new CondorID( cluster, proc, -1 );

	if( !job_is_finished_queue.enqueue( id, false ) ) {
			// the only reason the above can fail is because the job
			// is already in the queue
		dprintf( D_FULLDEBUG, "enqueueFinishedJob(): job %d.%d already "
				 "in queue to run jobIsFinished()\n", cluster, proc );
		delete id;
		return false;
	}

	dprintf( D_FULLDEBUG, "Job %d.%d is finished\n", cluster, proc );
	return true;
}

// Methods to manipulate the supplemental ClassAd list
int
Scheduler::adlist_register( const char *name )
{
	return extra_ads.Register( name );
}

int
Scheduler::adlist_replace( const char *name, ClassAd *newAd )
{
	return extra_ads.Replace( name, newAd );
}

int
Scheduler::adlist_delete( const char *name )
{
	return extra_ads.Delete( name );
}

int
Scheduler::adlist_publish( ClassAd *resAd )
{
	return extra_ads.Publish( resAd );
}

bool jobExternallyManaged(ClassAd * ad)
{
	ASSERT(ad);
	std::string job_managed;
	if( ! ad->LookupString(ATTR_JOB_MANAGED, job_managed) ) {
		return false;
	}
	return job_managed == MANAGED_EXTERNAL;
}

bool jobManagedDone(ClassAd * ad)
{
	ASSERT(ad);
	std::string job_managed;
	if( ! ad->LookupString(ATTR_JOB_MANAGED, job_managed) ) {
		return false;
	}
	return job_managed == MANAGED_DONE;
}


bool 
Scheduler::claimLocalStartd()
{
	Daemon startd(DT_STARTD, NULL, NULL);
	const char *startd_addr = NULL;	// local startd sinful string
	int slot_id;
	int number_of_claims = 0;
	char claim_id[155];	
	std::string slot_state;
	char job_owner[200];

	if ( NegotiationRequestTime==0 ) {
		// We aren't expecting any negotiation cycle
		return false;
	}

		 // If we are trying to exit, don't start any new jobs!
	if ( ExitWhenDone ) {
		return false;
	}

		// Check when we last had a negotiation cycle; if recent, return.
	int claimlocal_interval = param_integer("SCHEDD_ASSUME_NEGOTIATOR_GONE",
			2'000'000'000);
	if ( time(NULL) - NegotiationRequestTime < claimlocal_interval ) {
			// we have negotiated recently, no need to claim the local startd
		return false;
	}

		// Find the local startd.
	if ( !startd.locate() || !(startd_addr=startd.addr()) ) {
		// failed to locate a local startd, probably because one is not running
		return false;
	}

	dprintf(D_ALWAYS,
			"Haven't heard from negotiator, trying to claim local startd @ %s\n",
			startd_addr );

		// Fetch all the slot (machine) ads from the local startd
	CondorError errstack;
	CondorQuery query(STARTD_AD);
	QueryResult q;
	ClassAdList result;
	q = query.fetchAds(result, startd_addr, &errstack);
	if ( q != Q_OK ) {
		dprintf(D_FULLDEBUG,
				"ERROR: could not fetch ads from local startd : %s (%s)\n",
				startd_addr, getStrQueryResult(q) );
		return false;
	}


	ClassAd *machine_ad = NULL;
	result.Rewind();

		/*	For each machine ad, make a match rec and enqueue a request
			to claim the resource.
		 */
	while ( (machine_ad = result.Next()) ) {

		slot_id = 0;		
		machine_ad->LookupInteger(ATTR_SLOT_ID, slot_id);

			// first check if this startd is unclaimed
		slot_state = " ";	// clear out old value before we reuse it
		machine_ad->LookupString(ATTR_STATE, slot_state);
		if ( slot_state != getClaimStateString(CLAIM_UNCLAIMED) ) {
			dprintf(D_FULLDEBUG, "Local startd slot %d is not unclaimed\n",
					slot_id);
			continue;
		}

			// now get the location of the claim id file
		std::string file_name = startdClaimIdFile(slot_id);
		if (file_name.empty()) continue;
			// now open it as user condor and read out the claim
		claim_id[0] = '\0';	// so we notice if we fail to read
			// note: claim file written w/ condor priv by the startd
		priv_state old_priv = set_condor_priv(); 
		FILE* fp=safe_fopen_wrapper_follow(file_name.c_str(),"r");
		if ( fp ) {
			if (fscanf(fp,"%150s\n",claim_id) != 1) {
				dprintf(D_ALWAYS, "Failed to fscanf claim_id from file %s\n", file_name.c_str());
				continue;
			}
			fclose(fp);
		}
		set_priv(old_priv);	// switch our priv state back
		claim_id[150] = '\0';	// make certain it is null terminated
			// if we failed to get the claim, move on
		if ( !claim_id[0] ) {
			dprintf(D_ALWAYS,"Failed to read startd claim id from file %s\n",
				file_name.c_str());
			continue;
		}

		PROC_ID matching_jobid;
		matching_jobid.proc = -1;

		FindRunnableJob(matching_jobid,machine_ad,NULL);
		if( matching_jobid.proc < 0 ) {
				// out of jobs.  start over w/ the next startd ad.
			continue;
		}
		ClassAd *jobad = GetJobAd( matching_jobid.cluster, matching_jobid.proc );
		ASSERT( jobad );

		job_owner[0]='\0';
		jobad->LookupString(attr_JobUser,job_owner,sizeof(job_owner));
		ASSERT(job_owner[0]);

		match_rec* mrec = AddMrec( claim_id, startd_addr, &matching_jobid, machine_ad,
						job_owner,	// special Owner name
						NULL			// optional negotiator name
						);

		if( mrec ) {		
			/*
				We have successfully added a match_rec.  Now enqueue
				a request to go claim this resource.
				We don't want to call contactStartd
				directly because we do not want to block.
				So...we enqueue the args for a later
				call.  (The later call will be made from
				the startdContactSockHandler)
			*/
			ContactStartdArgs *args = 
						new ContactStartdArgs(claim_id, "", startd_addr, false);
			enqueueStartdContact(args);
			dprintf(D_ALWAYS, "Claiming local startd slot %d at %s\n",
					slot_id, startd_addr);
			number_of_claims++;
		}	
	}

		// Return true if we claimed anything, false if otherwise
	return number_of_claims ? true : false;
}

/**
 * Adds a job to various job indexes, called on startup and when new jobs are committed to the queue.
 * 
 * @param jobAd - the new job to be added to the cronTabs table
 * @param loading_job_queue - true if this function is called when reloading the job queue
 **/
void
Scheduler::indexAJob( JobQueueJob* jobAd, bool /*loading_job_queue*/ )
{
	int univ = jobAd->Universe();
	if ( univ == CONDOR_UNIVERSE_LOCAL || univ == CONDOR_UNIVERSE_SCHEDULER ) {
		int job_prio = 0;
		// If JobPrio is not set in the job ad, it will default to 0
		jobAd->LookupInteger( ATTR_JOB_PRIO, job_prio );
		LocalJobRec rec = LocalJobRec( job_prio, jobAd->jid );
		LocalJobsPrioQueue.insert( rec );
	}
}

/**
 * Removes a job from various job indexes, called when the job object is deleted
 * 
 * @param jobAd - the new job to be added to the cronTabs table
 **/
void
Scheduler::removeJobFromIndexes( const JOB_ID_KEY& job_id, int job_prio )
{
	// TODO: Figure out where SchedulerJobsRunning gets decremented
	LocalJobRec rec = LocalJobRec( job_prio, job_id );
	LocalJobsPrioQueue.erase( rec );
	return;
}

/**
 * Adds a job to our list of CronTab jobs
 * We will check to see if the job has already been added and
 * whether it defines a valid CronTab schedule before adding it
 * to our table
 * 
 * @param jobAd - the new job to be added to the cronTabs table
 **/
void
Scheduler::addCronTabClassAd( JobQueueJob *jobAd )
{
	if ( NULL == m_adSchedd ) return;
	CronTab *cronTab = NULL;
	PROC_ID id = jobAd->jid;
	if ( this->cronTabs->lookup( id, cronTab ) < 0 &&
		 CronTab::needsCronTab( jobAd ) ) {
		this->cronTabs->insert( id, NULL );
	}
}

/**
 * Adds a cluster to be checked for jobs that define CronTab jobs
 * This is needed because there is a gap from when we can find out
 * that a job has a CronTab attribute and when it gets proc_id. So the 
 * queue managment code can add a cluster_id to a list that we will
 * check later on to see whether a jobs within the cluster need 
 * to be added to the main cronTabs table
 * 
 * @param cluster_id - the cluster to be checked for CronTab jobs later on
 * @see processCronTabClusterIds
 **/
void
Scheduler::addCronTabClusterId( int cluster_id )
{
	if ( cluster_id >= 0 ) {
		cronTabClusterIds.insert( cluster_id );
	}
}

/**
 * Checks our list of cluster_ids to see whether any of the jobs
 * define a CronTab schedule. If any do, then we will add them to
 * our cronTabs table.
 * 
 * @see addCronTabClusterId
 **/
void 
Scheduler::processCronTabClusterIds( )
{
	CronTab *cronTab = NULL;
		//
		// Loop through all the cluster_ids that we have stored
		// For each cluster, we will inspect the job ads of all its
		// procs to see if they have defined crontab information
		//
	for ( auto itr = cronTabClusterIds.begin(); itr != cronTabClusterIds.end(); itr++ ) {
		JobQueueCluster * cad = GetClusterAd(*itr);
		if ( ! cad) continue;

		for (JobQueueJob * job = cad->FirstJob(); job != nullptr; job = cad->NextJob(job)) {
			if (this->cronTabs->lookup(job->jid, cronTab) < 0 && CronTab::needsCronTab(job)) {
				this->cronTabs->insert(job->jid, nullptr);
			}
		}
	} // WHILE
	cronTabClusterIds.clear();
}

/**
 * Run through all the CronTab jobs and calculate their next run times
 * We first check to see if there any new cluster ids that we need 
 * to scan for new CronTab jobs. We then run through all the jobs in
 * our cronTab table and call the calculate function
 **/
void
Scheduler::calculateCronTabSchedules( )
{
	PROC_ID id;
	CronTab *cronTab = NULL;
	this->processCronTabClusterIds();
	this->cronTabs->startIterations();
	while ( this->cronTabs->iterate( id, cronTab ) >= 1 ) {
		ClassAd *jobAd = GetJobAd( id.cluster, id.proc );
		if ( jobAd ) {
			this->calculateCronTabSchedule( jobAd );
		}
	} // WHILE
	return;
}

/**
 * For a given job, calculate the next runtime based on their CronTab
 * schedule. We keep a table of PROC_IDs and CronTab objects so that 
 * we only need to parse the schedule once. A new time is calculated when
 * either the cached CronTab object is deleted, the last calculated time
 * is in the past, or we are called with the 'calculate' flag set to true
 * 
 * NOTE:
 * To handle when the system time steps, the master will call a condor_reconfig
 * and we will delete our cronTab cache. This will force us to recalculate 
 * next run time for all our jobs
 * 
 * @param jobAd - the job to calculate the ne
 * @param calculate - if true, we will always calculate a new run time
 * @return true if no error occured, false otherwise
 * @see condor_crontab.C
 **/
bool 
Scheduler::calculateCronTabSchedule( ClassAd *jobAd, bool calculate )
{
	PROC_ID id;
	jobAd->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	jobAd->LookupInteger(ATTR_PROC_ID, id.proc);
			
		//
		// Check whether this needs a schedule
		//
	if ( !CronTab::needsCronTab( jobAd ) ) {	
		this->cronTabs->remove( id );
		return ( true );
	}
	
		//
		// Make sure that we don't change the deferral time
		// for running jobs
		//
	int status;
	if ( jobAd->LookupInteger( ATTR_JOB_STATUS, status ) == 0 ) {
		dprintf( D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				 ATTR_JOB_STATUS);
		return ( false );
	}
	if ( status != IDLE ) {
		return ( true );
	}

		//
		// If this is set to true then the cron schedule in the job ad 
		// had proper syntax and was parsed by the CronTab object successfully
		//
	bool valid = true;
		//
		// CronTab validation errors
		//
	std::string error;
		//
		// See if we can get the cached scheduler object 
		//
	CronTab *cronTab = NULL;
	(void) this->cronTabs->lookup( id, cronTab );
	if ( ! cronTab ) {
			//
			// There wasn't a cached object, so we'll need to create
			// one then shove it back in the lookup table
			// Make sure that the schedule is valid first
			//
		if ( CronTab::validate( jobAd, error ) ) {
			cronTab = new CronTab( jobAd );
				//
				// You never know what might have happended during
				// the initialization so it's good to check after
				// we instantiate the object
				//
			valid = cronTab->isValid();
			if ( valid ) {
				this->cronTabs->insert( id, cronTab, true );
			} else {
				error = cronTab->getError();
				delete cronTab;
				cronTab = 0;
			}
				//
				// We set the force flag to true so that we are 
				// sure to calculate the next runtime even if
				// the job ad already has one in it
				//
			calculate = true;

			//
			// It was invalid!
			// We'll notify the user and put the job on hold
			//
		} else {
			valid = false;
		}
	}

		//
		// Now determine whether we need to calculate a new runtime.
		// We first check to see if there is already a deferral time
		// for the job, and if it is, whether it's in the past
		// If it's in the past, we'll set the calculate flag to true
		// so that we will always calculate a new time
		//
	if ( ! calculate && jobAd->LookupExpr( ATTR_DEFERRAL_TIME ) != NULL ) {
			//
			// First get the DeferralTime
			//
		time_t deferralTime = 0;
		jobAd->LookupInteger( ATTR_DEFERRAL_TIME, deferralTime );
			//
			// Now look to see if they also have a DeferralWindow
			//
		int deferralWindow = 0;
		if ( jobAd->LookupExpr( ATTR_DEFERRAL_WINDOW ) != NULL ) {
			jobAd->LookupInteger( ATTR_DEFERRAL_WINDOW, deferralWindow );
		}
			//
			// Now if the current time is greater than the
			// DeferralTime + Window, than we know that this time is
			// way in the past and we need to calculate a new one
			// for the job
			//
		calculate = ( (long)time( NULL ) > ( deferralTime + deferralWindow ) );
	}
		//
		//	1) valid
		//		The CronTab object must have parsed the parameters
		//		for the schedule successfully
		//	3) force
		//		Always calculate a new time
		//	
	if ( valid && calculate ) {
			//
			// Get the next runtime from our current time
			// I used to subtract the DEFERRAL_WINDOW time from the current
			// time to allow the schedd to schedule job's that were suppose
			// to happen in the past. Think this is a bad idea because 
			// it may cause "thrashing" to occur when trying to schedule
			// the job for times that it will never be able to make
			//
		time_t runTime = cronTab->nextRunTime( );

		dprintf( D_FULLDEBUG, "Calculating next execution time for Job %d.%d = %lld\n",
				 id.cluster, id.proc, (long long)runTime );
			//
			// We have a valid runtime, so we need to update our job ad
			//
		if ( runTime != CRONTAB_INVALID ) {
				//
				// This is when our job should start execution
				// We only need to update the attribute because
				// condor_submit has done all the work to set up the
				// the job's Requirements expression
				//
			jobAd->Assign( ATTR_DEFERRAL_TIME,	runTime );	
					
		} else {
				//
				// We got back an invalid response
				// This is a little odd because the parameters
				// should have come back invalid when we instantiated
				// the object up above, but either way it's a good 
				// way to check
				//			
			valid = false;
		}
	} // CALCULATE NEXT RUN TIME
		//
		// After jumping through all our hoops, check to see
		// if the cron scheduling failed, meaning that the
		// crontab parameters were incorrect. We should
		// put the job on hold. condor_submit does check to make
		// sure that the cron schedule syntax is valid but the job
		// may have been submitted by an older version. The key thing
		// here is that if the scheduling failed the job should
		// NEVER RUN. They wanted it to run at a certain time, but
		// we couldn't figure out what that time was, so we can't just
		// run the job regardless because it may cause big problems
		//
	if ( !valid ) { 
			//
			// Get the error message to report back to the user
			// If we have a cronTab object then get the error 
			// message from that, otherwise look at the static 
			// error log which will be populated on CronTab::validate()
			//
		std::string reason( "Invalid cron schedule parameters: " );
		if ( cronTab != NULL ) {
			reason += cronTab->getError();
		} else {
			reason += error;
		}
			//
			// Throw the job on hold. For this call we want to:
			// 	use_transaction - true
			//	email_user		- true
			//	email_admin		- false
			//	system_hold		- false
			//
		holdJob( id.cluster, id.proc, reason.c_str(),
				 CONDOR_HOLD_CODE::InvalidCronSettings, 0,
				 true, true );
	}
	
	return ( valid );
}

class DCShadowKillMsg: public DCSignalMsg {
public:
	DCShadowKillMsg(pid_t pid, int sig, PROC_ID proc):
		DCSignalMsg(pid,sig)
	{
		m_proc = proc;
		m_sig = sig;
	}

	virtual MessageClosureEnum messageSent(
				DCMessenger *messenger, Sock *sock )
	{
		shadow_rec *srec = scheduler.FindSrecByProcID( m_proc );
		if( srec && srec->pid == thePid() ) {
			switch(m_sig)
			{
			case DC_SIGSUSPEND:
			case DC_SIGCONTINUE:
				break;
			default:
				srec->preempt_pending = false;
				srec->preempted = true;
			}
		}
		return DCSignalMsg::messageSent(messenger,sock);
	}

	virtual void messageSendFailed( DCMessenger *messenger )
	{
		// TODO Should we do anything else about this failure?
		shadow_rec *srec = scheduler.FindSrecByProcID( m_proc );
		if( srec && srec->pid == thePid() ) {
			srec->preempt_pending = false;
		}
		DCSignalMsg::messageSendFailed( messenger );
	}

private:
	PROC_ID m_proc;
	int m_sig;
};

void
Scheduler::sendSignalToShadow(pid_t pid,int sig,PROC_ID proc)
{
	classy_counted_ptr<DCShadowKillMsg> msg = new DCShadowKillMsg(pid,sig,proc);
	daemonCore->Send_Signal_nonblocking(msg.get());

		// When this operation completes, the handler in DCShadowKillMsg
		// will take care of setting shadow_rec->preempted = TRUE.
}

static
void
WriteCompletionVisa(ClassAd* ad)
{
	priv_state prev_priv_state;
	bool value;
	std::string iwd;

	ASSERT(ad);

	if (!ad->LookupBool(ATTR_WANT_SCHEDD_COMPLETION_VISA, value) ||
	    !value)
	{
		if (!ad->LookupBool(ATTR_JOB_SANDBOX_JOBAD, value) ||
		    !value)
		{
			return;
		}
	}

	if (!ad->LookupString(ATTR_JOB_IWD, iwd)) {
		dprintf(D_ERROR,
		        "WriteCompletionVisa ERROR: Job contained no IWD\n");
		return;
	}

	prev_priv_state = set_user_priv_from_ad(*ad);
	classad_visa_write(ad,
	                   get_mySubSystem()->getName(),
	                   daemonCore->InfoCommandSinfulString(),
	                   iwd.c_str(),
	                   NULL);
	set_priv(prev_priv_state);
	uninit_user_ids();
}

int
Scheduler::RecycleShadow(int /*cmd*/, Stream *stream)
{
		// This is called by the shadow when it wants to get a new job.
		// Two things are going on here: getting the exit reason for
		// the existing job and getting a new job.
	int shadow_pid = 0;
	int previous_job_exit_reason = 0;
	shadow_rec *srec;
	match_rec *mrec;
	PROC_ID prev_job_id;
	PROC_ID new_job_id;
	Sock *sock = (Sock *)stream;

		// force authentication
	sock->decode();
	if( !sock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ||
			! sock->getFullyQualifiedUser() )
		{
			dprintf( D_ALWAYS,
					 "RecycleShadow(): authentication failed: %s\n", 
					 errstack.getFullText().c_str() );
			return FALSE;
		}
	}

	stream->decode();
	if( !stream->get( shadow_pid ) ||
		!stream->get( previous_job_exit_reason ) ||
		!stream->end_of_message() )
	{
		dprintf(D_ALWAYS,
			"recycleShadow() failed to receive job exit reason from shadow\n");
		return FALSE;
	}

	srec = FindSrecByPid( shadow_pid );
	if( !srec ) {
		dprintf(D_ALWAYS,"recycleShadow() called with unknown shadow pid %d\n",
				shadow_pid);
		return FALSE;
	}
	prev_job_id = srec->job_id;
	mrec = srec->match;

		// currently we only support serial jobs here
	if( !mrec || !mrec->user || srec->preempted || srec->preempt_pending ||
		(srec->universe != CONDOR_UNIVERSE_VANILLA &&
		 srec->universe != CONDOR_UNIVERSE_JAVA &&
		 srec->universe != CONDOR_UNIVERSE_VM) )
	{
		stream->encode();
		stream->put((int)0);
		stream->end_of_message();
		return FALSE;
	}

		// If this match is earmarked for high-priority job, don't reuse
		// the shadow.  It's possible that we could, but for now, keep
		// things simple and only spawn the high-priority job in child_exit().
	if( mrec->m_now_job.isJobKey() ) {
		stream->encode();
		stream->put((int)0);
		stream->end_of_message();
		return FALSE;
	}

		// verify that whoever is running this command is either the
		// queue super user or the owner of the claim
#ifdef USE_JOB_QUEUE_USERREC
	// we don't actually want to UserCheck2 for recycling shadows
	// since the cmd_user will always be condor or equiv, and the match_user
	// will never be. UserCheck2 is essentially a complex way to
	// check to see if the cmd_user is a queue superuser.
	// RecycleShadow is registered at DAEMON level, we should trust that.
#else
	char const *cmd_user = EffectiveUser(sock);
	const char * match_user = mrec->user;
	std::string match_owner;
	if (user_is_the_new_owner) {
		// UserCheck wants fully qualified users
	} else {
		char const *at_sign = strchr(mrec->user,'@');
		if( at_sign ) {
			match_owner.append(mrec->user,at_sign-mrec->user);
			match_user = match_owner.c_str();
		}
	}

	if( !cmd_user || !UserCheck2(NULL, cmd_user, match_user) ) {
		dprintf(D_ALWAYS,
				"RecycleShadow() called by %s failed authorization check!\n",
				cmd_user ? cmd_user->Name() : "(unauthenticated)");
		return FALSE;
	}
#endif

		// Now handle the exit reason specified for the existing job.
	if( prev_job_id.cluster != -1 ) {
		dprintf(D_ALWAYS,
			"Shadow pid %d for job %d.%d reports job exit reason %d.\n",
			shadow_pid, prev_job_id.cluster, prev_job_id.proc,
			previous_job_exit_reason );

		jobExitCode( prev_job_id, previous_job_exit_reason );
		srec->exit_already_handled = true;
	}

	if (mrec->keep_while_idle) {
		mrec->idle_timer_deadline = time(NULL) + mrec->keep_while_idle;
	}

	if( !FindRunnableJobForClaim(mrec) ) {
		dprintf(D_FULLDEBUG,
			"No runnable jobs for shadow pid %d (was running job %d.%d); shadow will exit.\n",
			shadow_pid, prev_job_id.cluster, prev_job_id.proc);
		stream->encode();
		stream->put((int)0);
		stream->end_of_message();
		return TRUE;
	}

	new_job_id.cluster = mrec->cluster;
	new_job_id.proc = mrec->proc;

	dprintf(D_ALWAYS,
			"Shadow pid %d switching to job %d.%d.\n",
			shadow_pid, new_job_id.cluster, new_job_id.proc );

    time_t now = stats.Tick();
    stats.ShadowsRecycled += 1;
    stats.ShadowsRunning = numShadows;
	OtherPoolStats.Tick(now);

		// the add/delete_shadow_rec() functions update the job
		// ads, so we need to do that here
	delete_shadow_rec( srec );
	SetMrecJobID(mrec,new_job_id);
	srec = new shadow_rec;
	srec->pid = shadow_pid;
	srec->match = mrec;
	mrec->shadowRec = srec;
	srec->job_id = new_job_id;
	srec->prev_job_id = prev_job_id;
	srec->recycle_shadow_stream = stream;
	add_shadow_rec( srec );

	mark_serial_job_running(&new_job_id);

	mrec->setStatus( M_ACTIVE );

	callAboutToSpawnJobHandler(new_job_id.cluster, new_job_id.proc, srec);
	return KEEP_STREAM;
}

void
Scheduler::finishRecycleShadow(shadow_rec *srec)
{
	Stream *stream = srec->recycle_shadow_stream;
	srec->recycle_shadow_stream = NULL;

	int shadow_pid = srec->pid;
	PROC_ID new_job_id = srec->job_id;

	ASSERT( stream );

	stream->encode();

	ClassAd *new_ad = NULL;
	if( new_job_id.proc >= 0 ) {
		new_ad = GetExpandedJobAd(new_job_id, true);
		if( !new_ad ) {
			dprintf(D_ALWAYS,
					"Failed to expand job ad when switching shadow %d "
					"to new job %d.%d\n",
					shadow_pid, new_job_id.cluster, new_job_id.proc);

			jobExitCode( new_job_id, JOB_SHOULD_REQUEUE );
			srec->exit_already_handled = true;
		}
	}
	if( new_ad ) {
		std::string secret;
		if (GetPrivateAttributeString(new_job_id.cluster, new_job_id.proc, ATTR_CLAIM_ID, secret) == 0) {
			new_ad->Assign(ATTR_CLAIM_ID, secret);
		}
		if (GetPrivateAttributeString(new_job_id.cluster, new_job_id.proc, ATTR_CLAIM_IDS, secret) == 0) {
			new_ad->Assign(ATTR_CLAIM_IDS, secret);
		}

			// give the shadow the new job
		stream->put((int)1);
		putClassAd(stream, *new_ad);
	}
	else {
			// tell the shadow, "no job found"
		stream->put((int)0);
	}
	stream->end_of_message();

		// Get final ACK from shadow if we gave it a new job.
		// Without an ACK from the shadow, we could end up processing
		// an exit reason from the shadow that was meant for the previous
		// job rather than the new job.
	if( new_ad ) {
		stream->decode();
		int ok = 0;
		if( !stream->get(ok) ||
			!stream->end_of_message() ||
			!ok )
		{
			dprintf(D_ALWAYS,
				"Failed to get ok when switching shadow %d to a new job.\n",
				shadow_pid);

			jobExitCode( new_job_id, JOB_SHOULD_REQUEUE );
			srec->exit_already_handled = true;
		}
	}

	delete new_ad;
	delete stream;
}

int
Scheduler::FindGManagerPid(PROC_ID job_id)
{
	JobQueueJob *job_ad = GetJobAd(job_id.cluster,job_id.proc);

	if ( ! job_ad ) {
		return -1;
	}

	UserIdentity userident(*job_ad);
	return GridUniverseLogic::FindGManagerPid(userident.username().c_str(),
                                        userident.auxid().c_str(), 0, 0);
}

int
Scheduler::clear_dirty_job_attrs_handler(int /*cmd*/, Stream *stream)
{
	int cluster_id;
	int proc_id;
	Sock *sock = (Sock *)stream;

		// force authentication
	sock->decode();
	if( !sock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ||
			! sock->getFullyQualifiedUser() )
		{
			dprintf( D_ALWAYS,
					 "clear_dirty_job_attrs_handler(): authentication failed: %s\n", 
					 errstack.getFullText().c_str() );
			return FALSE;
		}
	}

	sock->decode();
	if( !sock->get( cluster_id ) ||
		!sock->get( proc_id ) ||
		!sock->end_of_message() )
	{
		dprintf(D_ALWAYS,
			"clear_dirty_job_attrs_handler() failed to receive job id\n");
		return FALSE;
	}

	MarkJobClean( cluster_id, proc_id );
	return TRUE;
}

int
Scheduler::receive_startd_update(int /*cmd*/, Stream *stream) {
	dprintf(D_COMMAND, "Schedd got update ad from local startd\n");

	ClassAd *machineAd = new ClassAd;
	if (!getClassAd(stream, *machineAd)) {
		dprintf(D_ALWAYS, "Error receiving update ad from local startd\n");
		return TRUE;
	}

	ClassAd *privateAd = new ClassAd;
	if (!getClassAd(stream, *privateAd)) {
		dprintf(D_ALWAYS, "Error receiving update private ad from local startd\n");
		return TRUE;
	}

	// I'm pretty sure setting these attibutes in machineAd is unecessary.
	char *claim_id = 0;
	if ( !privateAd->LookupString(ATTR_CLAIM_ID, &claim_id) ) {
		privateAd->LookupString(ATTR_CAPABILITY, &claim_id);
	}
	machineAd->Assign(ATTR_CLAIM_ID, claim_id);
	machineAd->Assign(ATTR_CAPABILITY, claim_id);

	char *name = 0;
	machineAd->LookupString(ATTR_NAME, &name);

	char *state = 0;
	machineAd->LookupString(ATTR_STATE, &state);

	if (strcmp(state, "Claimed") == 0) {
			// It is claimed by someone, we don't care about it anymore
		if (m_unclaimedLocalStartds.count(name) > 0) {
			delete m_unclaimedLocalStartds[name];
			m_unclaimedLocalStartds.erase(name);
		} 
		free(name);
		free(claim_id);
		free(state);
		return TRUE;
	} else {
		if (m_unclaimedLocalStartds.count(name) > 0) {
			dprintf(D_FULLDEBUG, "Local slot %s was already unclaimed, removing it\n", name);
			delete m_unclaimedLocalStartds[name];
			m_unclaimedLocalStartds.erase(name);
		} 
			// Pass this machine into our match list
		ScheddNegotiate *sn;
		PROC_ID jobid;
		jobid.cluster = jobid.proc = -1;

		sn = new MainScheddNegotiate(0, NULL, NULL, NULL);
		sn->scheduler_handleMatch(jobid,claim_id, "", *machineAd, name);
		delete sn;

		m_unclaimedLocalStartds[name] = machineAd;
		free(name);
		free(claim_id);
		free(state);
		return TRUE;
	}
	return TRUE;
}

int
Scheduler::receive_startd_invalidate(int /*cmd*/, Stream * /*stream*/) {
	// Will this ever come in, other than shutdown?
	return TRUE;
}

int
Scheduler::local_startd_reaper(int pid, int status) {
	dprintf(D_ALWAYS, "Local Startd (pid %d) exited with status (%d)\n", pid, status);
	m_local_startd_pid = -1;

	// should schedule timer to restart after some backoff
	return TRUE;
}

int
Scheduler::launch_local_startd() {
	if (m_local_startd_pid != -1) {
		// There's one already runnning, we got here because of a reconfig.
		return TRUE;
	}

	int rid = daemonCore->Register_Reaper(
						"localStartdReaper",
						(ReaperHandlercpp) &Scheduler::local_startd_reaper,
						"localStartdReaper",
						this);

	if (rid < 0) {
		EXCEPT("Can't register reaper for local startd" );
	}

	  // The arguments for our startd
	ArgList args;
	args.AppendArg("condor_startd");
	// no longer needed, foreground is default for daemons other than the master
	//args.AppendArg("-f"); // The startd is daemon-core, so run in the "foreground"
	args.AppendArg("-local-name"); // This is the local startd, not the vanilla one
	args.AppendArg("LOCALSTARTD");


	Env env;
	env.Import(); // copy schedd's environment
	env.SetEnv("_condor_STARTD_LOG", "$(LOG)" DIR_DELIM_STR "LocalStartLog");

	// set the EXECUTE knob
	std::string tmpstr;
	param(tmpstr, "LOCAL_UNIV_EXECUTE");
	if ( ! tmpstr.empty()) {
		env.SetEnv("_condor_EXECUTE", tmpstr.c_str());
		tmpstr.clear();
	}

	// Force START expression to be START_LOCAL_UNIVERSE and only match local universe jobs
	std::string localConstraint;
	formatstr(localConstraint, "(" ATTR_JOB_UNIVERSE " == %d)", CONDOR_UNIVERSE_LOCAL);
	param(tmpstr, "START_LOCAL_UNIVERSE");
	if ( ! tmpstr.empty()) {
		localConstraint += " && ";
		localConstraint += tmpstr;
	}
	env.SetEnv("_condor_START", localConstraint.c_str());

	std::string mysinful(daemonCore->publicNetworkIpAddr());
	mysinful.erase(0,1);
	mysinful.erase(mysinful.length()-1);

		// Force this local startd to report not to the collector
		// but to this schedd
	env.SetEnv("_condor_CONDOR_HOST", mysinful.c_str());
	env.SetEnv("_condor_COLLECTOR_HOST", mysinful.c_str());

		// Force the requirements to only run local jobs from this schedd
	//env.SetEnv("_condor_START", "JobUniverse == 11");
	env.SetEnv("_condor_IS_LOCAL_STARTD", "true");

		// Figure out the path to the startd binary
	std::string path;
	param( path, "STARTD", "" );

	if (path.length() == 0) {
		// Very unusual that STARTD isn't defined, something is wrong...
		dprintf(D_ALWAYS, "Can't find path to STARTD daemon in config file: unable to run local universe jobs\n");
		return false;
	}

	std::string daemon_sock = SharedPortEndpoint::GenerateEndpointName( "local_universe_startd" );

    std::string create_process_err_msg;
	OptionalCreateProcessArgs cpArgs(create_process_err_msg);
	m_local_startd_pid = daemonCore->CreateProcessNew( path, args,
		 cpArgs.priv(PRIV_ROOT)
		 .reaperID(rid)
		 .env(&env)
		 .daemonSock(daemon_sock.c_str())
	);
	dprintf(D_ALWAYS, "Launched startd for local jobs with pid %d\n", m_local_startd_pid);
	return TRUE;
}

bool
Scheduler::shouldCheckSubmitRequirements() {
	return m_submitRequirements.size() != 0;
}

int
Scheduler::checkSubmitRequirements( ClassAd * procAd, CondorError * errorStack ) {
	int rval = 0;
	SubmitRequirements::iterator it = m_submitRequirements.begin();
	for( ; it != m_submitRequirements.end(); ++it ) {
		classad::Value result;
		rval = EvalExprToBool( it->requirement.get(), m_adSchedd, procAd, result, "SCHEDD", "JOB" );

		if( rval ) {
			bool bVal;
			if( ! result.IsBooleanValueEquiv( bVal ) ) {
				if ( errorStack ) {
					errorStack->pushf( "QMGMT", 1, "Submit requirement %s evaluated to non-boolean.\n", it->name.c_str() );
				}
				return -3;
			}

			if( ! bVal ) {
				classad::Value reason;
				std::string reasonString;
				formatstr( reasonString, "Submit requirement %s not met.\n", it->name.c_str() );

				if( it->reason != NULL ) {
					int sval = EvalExprToString( it->reason.get(), m_adSchedd, procAd, reason, "SCHEDD", "JOB" );
					if( ! sval ) {
						dprintf( D_ALWAYS, "Submit requirement reason %s failed to evaluate.\n", it->name.c_str() );
					} else {
						std::string userReasonString;
						if( reason.IsStringValue( userReasonString ) ) {
							reasonString = userReasonString;
						} else {
							dprintf( D_ALWAYS, "Submit requirement reason %s did not evaluate to a string.\n", it->name.c_str() );
						}
					}
				}

				if ( errorStack ) {
					if( it->isWarning ) {
						errorStack->pushf( "QMGMT", 0, "%s", reasonString.c_str() );
						continue;
					} else {
						errorStack->pushf( "QMGMT", 2, "%s", reasonString.c_str() );
					}
				}
				return -1;
			}
		} else {
			if ( errorStack ) {
				errorStack->pushf( "QMGMT", 3, "Submit requirement %s failed to evaluate.\n", it->name.c_str() );
			}
			return -2;
		}
	}

	return 0;
}

void
Scheduler::WriteRestartReport( int /* timerID */ )
{
	static time_t restart_time = 0;
	static int total_reconnects = 0;
	std::string filename;

	param( filename, "SCHEDD_RESTART_REPORT" );
	if ( filename.empty() ) {
		return;
	}

	if ( restart_time == 0 ) {
		restart_time = time(NULL);
	}
	if ( total_reconnects == 0 ) {
		total_reconnects = stats.JobsRestartReconnectsAttempting.value +
			stats.JobsRestartReconnectsFailed.value +
			stats.JobsRestartReconnectsLeaseExpired.value +
			stats.JobsRestartReconnectsSucceeded.value +
			stats.JobsRestartReconnectsInterrupted.value;
	}

	struct tm *restart_tm = localtime( &restart_time );
	char restart_time_str[80];
	strftime( restart_time_str, 80, "%m/%d/%y %H:%M:%S", restart_tm );

	std::string report;
	formatstr( report, "The schedd %s restarted at %s.\n"
			   "It attempted to reconnect to machines where its jobs may still be running.\n",
			   Name, restart_time_str );
	if ( stats.JobsRestartReconnectsAttempting.value == 0 ) {
		formatstr_cat( report, "All reconnect attempts have finished.\n" );
	}
	formatstr_cat( report, "Here is a summary of the reconnect attempts:\n\n" );
	formatstr_cat( report, "%d total jobs where reconnecting is possible\n",
				   total_reconnects );
	formatstr_cat( report, "%d reconnects are still being attempted\n",
				   stats.JobsRestartReconnectsAttempting.value );
	formatstr_cat( report, "%d reconnects weren't attempted because the lease expired before the schedd restarted\n",
				   stats.JobsRestartReconnectsLeaseExpired.value );
	formatstr_cat( report, "%d reconnects failed\n",
				   stats.JobsRestartReconnectsFailed.value );
	formatstr_cat( report, "%d reconnects were interrupted by job removal or other event\n",
				   stats.JobsRestartReconnectsInterrupted.value );
	formatstr_cat( report, "%d reconnects succeeded\n",
				   stats.JobsRestartReconnectsSucceeded.value );
		// TODO Include stats.JobsRestartReconnectsBadput?

	FILE *report_fp = safe_fcreate_replace_if_exists( filename.c_str(), "w" );
	if ( report_fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open schedd restart report file '%s', errno=%d (%s)\n", filename.c_str(), errno, strerror(errno) );
		return;
	}
	fprintf( report_fp, "%s", report.c_str() );
	fclose( report_fp );

	if ( stats.JobsRestartReconnectsAttempting.value == 0 ) {
		FILE *mailer = NULL;
		std::string email_subject;

		formatstr( email_subject, "Schedd restart report for %s", Name );

		char *address = param( "SCHEDD_ADMIN_EMAIL" );
		if(address) {
			mailer = email_nonjob_open( address, email_subject.c_str() );
			free( address );
		} else {
			mailer = email_admin_open( email_subject.c_str() );
		}

		if( mailer == NULL ) {
			return;
		}

		fprintf( mailer, "%s", report.c_str() );

		email_close( mailer );

		return;
	}

	daemonCore->Register_Timer( 60,
						(TimerHandlercpp)&Scheduler::WriteRestartReport,
						"Scheduler::WriteRestartReport", this );
}


void handleReassignSlotError( Sock * sock, const char * msg ) {
	ClassAd reply;
	reply.Assign( ATTR_RESULT, false );
	reply.Assign( ATTR_ERROR_STRING, msg );
	if( ! putClassAd( sock, reply ) || ! sock->end_of_message() ) {
		dprintf( D_ALWAYS, "REASSIGN_SLOT: failed to send error response.\n" );
	} else {
		dprintf( D_ALWAYS, "REASSIGN_SLOT: %s.\n", msg );
	}
}

#define RS_TEST_SMV		1
#define RS_TEST_PCCC	2
int Scheduler::reassign_slot_handler( int cmd, Stream * s ) {
	ASSERT( cmd == REASSIGN_SLOT );
	Sock * sock = reinterpret_cast<Sock *>(s);

	// Force authentication.  Only their owner can reassign a claim from
	// one job to another.
	if(! sock->triedAuthentication()) {
		CondorError errorStack;
		if(! SecMan::authenticate_sock( sock, WRITE, & errorStack ) ||
		  ! sock->getFullyQualifiedUser() ) {
			dprintf( D_ALWAYS, "REASSIGN_SLOT: authentication failed: %s\n",
				errorStack.getFullText().c_str() );
			return FALSE;
		}
	}

	ClassAd request;
	if( ! getClassAd( s, request ) || ! s->end_of_message() ) {
		dprintf( D_ALWAYS, "REASSIGN_SLOT: Failed to receive request ClassAd\n" );
		return FALSE;
	}

	PROC_ID bid; // beneficiary job's job ID.
	std::string bidString;
	if( request.LookupString( "BeneficiaryJobID", bidString ) ) {
		bid = getProcByString( bidString.c_str() );
		if( bid.cluster == -1 && bid.proc == -1 ) {
			handleReassignSlotError( sock, "invalid now-job ID" );
		}
	} else {
		if( ! request.LookupInteger( "Beneficiary" ATTR_CLUSTER_ID, bid.cluster ) ||
		  ! request.LookupInteger( "Beneficiary" ATTR_PROC_ID, bid.proc ) ) {
			handleReassignSlotError( sock, "missing now-job ID" );
			return FALSE;
		}
	}

	PROC_ID vID;
	std::vector< PROC_ID> vids;

	std::string vidString;
	if( request.LookupString( "VictimJobIDs", vidString ) ) {
		for (auto& vs: StringTokenIterator(vidString)) {
			PROC_ID vID;
			if(! vID.set( vs.c_str() )) {
				handleReassignSlotError( sock, "invalid vacate-job ID in list" );
				return FALSE;
			}
			vids.push_back( vID );
		}

		if( vids.empty() ) {
			handleReassignSlotError( sock, "invalid vacate-job ID list" );
			return FALSE;
		}

		dprintf( D_COMMAND, "REASSIGN SLOT: to %d.%d from %s\n", bid.cluster, bid.proc, vidString.c_str() );
	} else {
		PROC_ID vID;
		if( ! request.LookupInteger( "Victim" ATTR_CLUSTER_ID, vID.cluster ) ||
		  ! request.LookupInteger( "Victim" ATTR_PROC_ID, vID.proc ) ) {
			handleReassignSlotError( sock, "missing vacate-job ID" );
			return FALSE;
		}
		vids.push_back( vID );

		dprintf( D_COMMAND, "REASSIGN_SLOT: from %d.%d to %d.%d\n", vids[0].cluster, vids[0].proc, bid.cluster, bid.proc );
	}

	// FIXME: Throttling.

	JobQueueJob * bAd = GetJobAd( bid.cluster, bid.proc );
	if(! bAd) {
		handleReassignSlotError( sock, "no such job (now-job)" );
		return FALSE;
	}
	if(! UserCheck2( bAd, EffectiveUserRec(sock) )) {
		handleReassignSlotError( sock, "you must own the now-job" );
		return FALSE;
	}

	if( bAd->Universe() != CONDOR_UNIVERSE_VANILLA ) {
		handleReassignSlotError( sock, "the now-job must be in the vanilla universe" );
		return FALSE;
	}

	if( bAd->Status() != IDLE ) {
		handleReassignSlotError( sock, "the now-job must be idle" );
		return FALSE;
	}

	for( unsigned v = 0; v < vids.size(); ++v ) {
		JobQueueJob * vAd = GetJobAd( vids[v].cluster, vids[v].proc );
		if(! vAd) {
			handleReassignSlotError( sock, "no such job (vacate-job)" );
			return FALSE;
		}
		if(! UserCheck2( vAd, EffectiveUserRec(sock) )) {
			handleReassignSlotError( sock, "you must own the vacate-job" );
			return FALSE;
		}

		if( vAd->Universe() != CONDOR_UNIVERSE_VANILLA ) {
			handleReassignSlotError( sock, "vacate-job must be in the vanilla universe" );
			return FALSE;
		}

		// Assume that we want the vacate-job to finish TRANSFERRING_OUTPUT.
		if( vAd->Status() != RUNNING ) {
			handleReassignSlotError( sock, "vacate-job must be running" );
			return FALSE;
		}
	}

	int flags = 0;
	request.LookupInteger( "Flags", flags );

	// If we're testing send_matchless_vacate(), don't do anything else.
	if( flags & RS_TEST_SMV ) {
		for( unsigned v = 0; v < vids.size(); ++v ) {
			match_rec * match = FindMrecByJobID( vids[v] );
			if(! match) {
				handleReassignSlotError( sock, "no match for vacate-job ID" );
				return FALSE;
			}

			send_matchless_vacate( match->description(), NULL, match->peer,
				match->claim_id.claimId(), RELEASE_CLAIM );
		}
	} else if( flags & RS_TEST_PCCC ) {
		if( pcccTest() ) {
			DC_Exit( 0 );
		} else {
			DC_Exit( 1 );
		}
	} else {
		// It's safe to deactivate each victim's claim.
		if(! pcccNew( bid )) {
			handleReassignSlotError( sock, "the now-job must not already be scheduled to run immediately" );
			return FALSE;
		}
		for( unsigned v = 0; v < vids.size(); ++v ) {
			match_rec * match = FindMrecByJobID( vids[v] );
			if(! match) {
				handleReassignSlotError( sock, "no match for vacate-job ID" );
				return FALSE;
			}

			pcccWants( bid, match );
			match->m_now_job = bid;
			enqueueActOnJobMyself( vids[v], JA_VACATE_FAST_JOBS, true );
		}
	}

	// We could return KEEP_STREAM and block the client until we'd actually
	// started the beneficiary job, but we can skip that for now.
	ClassAd reply;
	reply.Assign( ATTR_RESULT, true );
	if( ! putClassAd( sock, reply ) || ! sock->end_of_message() ) {
		dprintf( D_ALWAYS, "failed to send success response.\n" );
	}
	return TRUE;
}


void
Scheduler::token_request_callback(bool success, void *miscdata)
{
	auto self = reinterpret_cast<Scheduler*>(miscdata);

	if (success) {
		daemonCore->Reset_Timer(self->timeoutid,0,1);
	}
}


bool
Scheduler::ExportJobs(ClassAd & result, std::set<int> & clusters, const char *output_dir, const OwnerInfo *user, const char * new_spool_dir /*="##"*/)
{
	dprintf(D_ALWAYS,"ExportJobs(...,'%s','%s')\n",output_dir,new_spool_dir);
	OwnerInfo * owner = nullptr;

	// verify that all of the jobs have the same owner and get the owner
	//
	for (auto it = clusters.begin(); it != clusters.end(); ++it) {
		const int next_cluster = *it;
		JobQueueCluster * jqc = GetClusterAd(next_cluster);
		if ( ! jqc) {
			dprintf(D_ALWAYS, "ExportJobs(): Ignoring missing cluster id %d\n", next_cluster);
			continue;
		}
		if ( ! owner) {
			owner = jqc->ownerinfo;
		} else if ( owner != jqc->ownerinfo ) {
			result.Assign(ATTR_ERROR_STRING, "Cannot export for more than one user");
			dprintf(D_ALWAYS, "ExportJobs(): Multiple owners %s and %s, aborting\n", owner->Name(), jqc->ownerinfo->Name());
			return false;
		}
		// TODO check for late materialization
		// TODO check for spooled
		// TODO check for active jobs (running, externally managed)
	}
	if ( clusters.empty() ) {
		result.Assign(ATTR_ERROR_STRING, "No clusters to export");
		dprintf(D_ALWAYS, "ExportJobs(): No clusters to export\n");
		return false;
	}

	JobQueueCluster * jqc = GetClusterAd(*(clusters.begin()));

	// verify the user is authorized to edit these jobs
	if ( ! UserCheck2(jqc, user) ) {
		result.Assign(ATTR_ERROR_STRING, "User not authorized to export given jobs");
		dprintf(D_ALWAYS, "ExportJobs(): User %s not authorized to export jobs owned by %s, aborting\n", user->Name(), jqc->ownerinfo->Name());
		return false;
	}

	TemporaryPrivSentry tps(true);
	if ( ! jqc->ownerinfo || !init_user_ids_from_ad(*jqc) ) {
		result.Assign(ATTR_ERROR_STRING, "Failed to init user ids");
		dprintf(D_ALWAYS, "ExportJobs(): Failed to init user ids!\n");
		return false;
	}
	set_user_priv();

	// TODO make output_dir if it doesn't exist?
	// TODO: truncate output job_queue.log if it is non-empty?

	std::string queue_fname = output_dir;
	dircat(output_dir, "job_queue.log", queue_fname);
	GenericClassAdCollection<JobQueueKey, ClassAd*> export_queue(new ConstructClassAdLogTableEntry<ClassAd>());

	if ( !export_queue.InitLogFile(queue_fname.c_str(), 0) ) {
		result.Assign(ATTR_ERROR_STRING, "Failed to initialize export job log");
		dprintf(D_ALWAYS, "ExportJobs(): Failed to initialize export job log %s\n", queue_fname.c_str());
		return false;
	}

	BeginTransaction();

	// write the a header ad to the job queue
	JobQueueKey hdr_id(0,0);
	int tmp_int = 0;
	export_queue.NewClassAd(hdr_id, JOB_ADTYPE);
	if ( GetAttributeInt(0, 0, ATTR_NEXT_CLUSTER_NUM, &tmp_int) >= 0 ) {
		std::string int_str = std::to_string(tmp_int);
		export_queue.SetAttribute(hdr_id, ATTR_NEXT_CLUSTER_NUM, int_str.c_str());
	}

	int num_jobs = 0;
	for (auto cid = clusters.begin(); cid != clusters.end(); ++cid) {
		JobQueueCluster * jqc = GetClusterAd(*cid);
		if ( ! jqc) continue;

		// export the cluster classad, marking all of the jobs in the cluster as leave-in-queue
		export_queue.NewClassAd(jqc->jid, JOB_ADTYPE);
		for ( auto attr_itr = jqc->begin(); attr_itr != jqc->end(); attr_itr++ ) {
			if (YourStringNoCase(ATTR_JOB_LEAVE_IN_QUEUE) == attr_itr->first.c_str()) {
				// nothing
			} else {
				const char *val = ExprTreeToString(attr_itr->second);
				export_queue.SetAttribute(jqc->jid, attr_itr->first.c_str(), val);
			}
		}
		export_queue.SetAttribute(jqc->jid, ATTR_JOB_LEAVE_IN_QUEUE, "true");

		if (jqc->factory) {
			// TODO: move the submit digest and itemdata files
			// and rewrite the path to the digest
		}

		// export all jobs in the cluster
		for (JobQueueJob * job = jqc->FirstJob(); job != NULL; job = jqc->NextJob(job)) {
			// skip jobs that are already externally managed.
			if (jobExternallyManaged(job)) continue;

			// TODO: skip jobs that aren't idle?

			// add the job to the external queue
			export_queue.NewClassAd(job->jid, JOB_ADTYPE);

			// copy the job attributes to the external queue
			for ( auto attr_itr = job->begin(); attr_itr != job->end(); attr_itr++ ) {
				if (YourStringNoCase(ATTR_JOB_LEAVE_IN_QUEUE) == attr_itr->first.c_str()) {
					// nothing
				} else if (YourStringNoCase(ATTR_JOB_IWD) == attr_itr->first.c_str()) {
					char *dir = gen_ckpt_name(new_spool_dir, job->jid.cluster, job->jid.proc, 0);
					std::string val;
					val = '"';
					val += dir;
					val += '"';
					free(dir);
					export_queue.SetAttribute(job->jid, ATTR_JOB_IWD, val.c_str());
				} else {
					const char *val = ExprTreeToString(attr_itr->second);
					export_queue.SetAttribute(job->jid, attr_itr->first.c_str(), val);
				}
			}

			// mark the job as externally managed in the local job queue
			SetAttributeString(job->jid.cluster, job->jid.proc, ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
			SetAttributeString(job->jid.cluster, job->jid.proc, ATTR_JOB_MANAGED_MANAGER, "Lumberjack");
			++num_jobs;
		}
	}

	CommitTransactionOrDieTrying();

	// flush changes, then close the log file
	export_queue.FlushLog();
	export_queue.StopLog();

	result.Assign("Log", queue_fname);
	result.Assign("User", jqc->ownerinfo->Name());
	result.Assign(ATTR_TOTAL_JOB_ADS, num_jobs);
	result.Assign(ATTR_TOTAL_CLUSTER_ADS, (long)clusters.size());

	dprintf(D_ALWAYS,"ExportJobs() returning true\n");
	return true;
}

int
Scheduler::export_jobs_handler(int /*cmd*/, Stream *stream)
{
	ReliSock *rsock = (ReliSock*)stream;
	ClassAd reqAd; // classad specifying arguments for the export
	ClassAd resultAd; // classad returning results of the export

	// IWD of exported jobs will be re-written to use this directory as a base 
	// the default value of ## is intended to be replaced by a script that post-processes the job log.
	std::string new_spool_dir("##");

	rsock->decode();
	rsock->timeout(15);
	if( !getClassAd(rsock, reqAd) || !rsock->end_of_message()) {
		dprintf( D_ALWAYS, "export_jobs_handler Failed to receive classad in the request\n" );
		return FALSE;
	}

	std::string export_list;    // list of cluster ids to export
	std::string export_dir;     // where to write the exported job queue and other files
	ExprTree * constr_expr = reqAd.Lookup(ATTR_ACTION_CONSTRAINT);
	if ( ! reqAd.LookupString("ExportDir", export_dir) ||
		( ! constr_expr && ! reqAd.LookupString(ATTR_ACTION_IDS, export_list)))
	{
		resultAd.Assign(ATTR_ACTION_RESULT, NOT_OK);
		resultAd.Assign(ATTR_ERROR_STRING, "Invalid arguments");
		resultAd.Assign(ATTR_ERROR_CODE, SCHEDD_ERR_MISSING_ARGUMENT);
	}
	else 
	{
		reqAd.LookupString("NewSpoolDir", new_spool_dir); // the reqest ad *may* contain an override for the checkpoint directory
		std::set<int> clusters;
		// we have a constrain expression, so we have to turn that into a set of cluster ids
		if (constr_expr) {
			schedd_runtime_probe runtime;
			struct _cluster_ids_args {
				std::set<int> * pids;
				ExprTree * constraint;
			} args = { &clusters, constr_expr };

			// build up a set of cluster ids that match the constraint expression
			WalkJobQueueEntries(
				(WJQ_WITH_CLUSTERS | WJQ_WITH_NO_JOBS),
				[](JobQueuePayload job, const JOB_ID_KEY & cid, void * pv) -> int {
					struct _cluster_ids_args & args = *(struct _cluster_ids_args*)pv;
					if ( ! args.constraint || EvalExprBool(job, args.constraint)) {
						args.pids->insert(cid.cluster);
					}
					return 0;
				},
				&args,
				runtime);

		} else {
			// we have a string list of cluster ids
			// build a set of cluster ids from the string of ids that was passed
			StringTokenIterator sit(export_list);
			for (const char * id = sit.first(); id != NULL; id = sit.next()) {
				clusters.insert(atoi(id));
			}
		}
		if ( ! ExportJobs(resultAd, clusters, export_dir.c_str(), EffectiveUserRec(rsock), new_spool_dir.c_str())) {
			resultAd.Assign(ATTR_ACTION_RESULT, NOT_OK);
			resultAd.Assign(ATTR_ERROR_CODE, SCHEDD_ERR_EXPORT_FAILED);
		} else {
			resultAd.Assign(ATTR_ACTION_RESULT, OK);
		}
	}

	// send a reply consisting of a result classad
	rsock->encode();
	if ( ! putClassAd(rsock, resultAd)) {
		dprintf( D_ALWAYS, "Error sending export-jobs result to client, aborting.\n" );
		return FALSE;
	}
	if ( ! rsock->end_of_message()) {
		dprintf( D_ALWAYS, "Error sending end-of-message to client, aborting.\n" );
		return FALSE;
	}

	return TRUE;
}


bool
Scheduler::ImportExportedJobResults(ClassAd & result, const char * import_dir, const OwnerInfo * user)
{
	dprintf(D_ALWAYS,"ImportExportedJobResults(%s)\n", import_dir);

	std::string import_job_log;
	formatstr(import_job_log, "%s/job_queue.log", import_dir);

	TemporaryPrivSentry tps(true);
	if ( ! init_user_ids(user) ) {
		result.Assign(ATTR_ERROR_STRING, "Failed to init user ids");
		dprintf(D_ALWAYS, "ImportExportedJobResults(): Failed to init user ids!\n");
		return false;
	}
	set_user_priv();

	// Load the external job_log
	GenericClassAdCollection<JobQueueKey, ClassAd*> import_queue(new ConstructClassAdLogTableEntry<ClassAd>());
	if ( !import_queue.InitLogFile(import_job_log.c_str(), 0) ) {
		result.Assign(ATTR_ERROR_STRING, "Failed to initialize import job log");
		dprintf(D_ALWAYS, "ImportExportedJobResults(): Failed to initialize import job log\n");
		return false;
	}
	// close the log file and disable changes
	import_queue.StopLog();

	std::set<int> clusters_found;
	std::set<int> clusters_checked;

	BeginTransaction();

	int num_jobs = 0;
	std::string tmpstr;
	ConstraintHolder hold_reason;

	// Note that in the import_queue at this time, the proc ads are not chained to the cluster ads
	ClassAd *ad;
	JobQueueKey jid;
	import_queue.StartIterateAllClassAds();
	while(import_queue.Iterate(jid,ad)) {
		if (jid.cluster <= 0) continue; // ignore the header ad and set ads
		if (jid.proc < 0) {
			// this is a cluster ad, we only care about factory ads
			// TODO: handle late materialization factories
			clusters_found.insert(jid.cluster);
		} else {
			// this is a proc ad, check to see if it corresponds to a job that we have

			JobQueueJob * job = GetJobAd(jid);
			if ( ! job) {
				// ignore if this doesn't correspond to a job in the queue.
				// TODO: handle jobs that were externally materialized
				continue;
			}

			// verify the user is authorized to edit this job
			if ( clusters_checked.find(jid.cluster) == clusters_checked.end() ) {
				if ( ! UserCheck2(job, user) ) {
					result.Assign(ATTR_ERROR_STRING, "User not authorized to import given jobs");
					dprintf(D_ALWAYS, "ImportExportexJobResults(): User %s not authorized to import results for job owned by %s, aborting\n",
						user->Name(), job->ownerinfo->Name());
					AbortTransaction();
					return false;
				}
				clusters_checked.insert(jid.cluster);
			}

			// check to see if this corresponds to a job that is externally managed
			// and not HELD or REMOVED, we skip external jobs for which this is not true.
			if (job->Status() == HELD || job->Status() == REMOVED) {
				continue;
			}
			if ( ! job->LookupString(ATTR_JOB_MANAGED, tmpstr) || tmpstr != MANAGED_EXTERNAL) {
				continue;
			}
			if ( ! job->LookupString(ATTR_JOB_MANAGED_MANAGER, tmpstr) || tmpstr != "Lumberjack") {
				continue;
			}

			++num_jobs;

		#ifdef SMART_MERGE_BACK_OF_JOB_STATUS // smarter merge-back of job state
			// grab the state from the external ad
			int external_status = -1;
			int hold_code=-1, hold_subcode=-1;
			ad->LookupInteger(ATTR_JOB_STATUS, external_status);
			if (external_status == HELD) {
				// If external statis held, grab hold info and remove it from the ad
				// so it doesn't get merged back.
				ad->LookupInteger(ATTR_HOLD_REASON_CODE, hold_code);
				ad->LookupInteger(ATTR_HOLD_REASON_SUBCODE, hold_subcode);
				ad->Delete(ATTR_HOLD_REASON_CODE);
				ad->Delete(ATTR_HOLD_REASON_SUBCODE);
				hold_reason.set(ad->Remove(ATTR_HOLD_REASON));
			}
			ad->Delete(ATTR_JOB_STATUS);
		#endif

			// delete the attributes from the external ad we don't want to merge back
			ad->Delete(ATTR_JOB_LEAVE_IN_QUEUE);
			ad->Delete(ATTR_JOB_IWD);
			// delete immutable attributes that we couldn't merge back even if we wanted to
			ad->Delete(ATTR_OWNER);
			ad->Delete(ATTR_USER);
			ad->Delete(ATTR_MY_TYPE);
			ad->Delete(ATTR_TARGET_TYPE);
			//ad->Delete(ATTR_PROC_ID);

			// merge back attributes that have changed
			for (auto it = ad->begin(); it != ad->end(); ++it) {
				ExprTree * expr = job->Lookup(it->first);
				if ( ! expr || ! (*expr == *(it->second))) {
					const char *val = ExprTreeToString(it->second);
					SetAttribute(jid.cluster, jid.proc, it->first.c_str(), val);
				}
			}

		#ifdef SMART_MERGE_BACK_OF_JOB_STATUS // smarter merge-back of job state
			if (job->Status() != external_status) {
				// update state counters
				SetAttributeInt(jid.cluster, jid.proc, ATTR_JOB_STATE, external_status);
				if (external_status == HELD) {
					SetAttribute(jid.cluster, jid.proc, ATTR_HOLD_REASON_CODE, hold_code);
					SetAttribute(jid.cluster, jid.proc, ATTR_HOLD_REASON_SUBCODE, hold_subcode);
					SetAttribute(jid.cluster, jid.proc, ATTR_HOLD_REASON, hold_reason.c_str());
				}
			}
		#endif

			// take the job out of the managed state
			DeleteAttribute(jid.cluster, jid.proc, ATTR_JOB_MANAGED);
			DeleteAttribute(jid.cluster, jid.proc, ATTR_JOB_MANAGED_MANAGER);
		}
	}

	CommitTransactionOrDieTrying();

	result.Assign(ATTR_TOTAL_JOB_ADS, num_jobs);

	dprintf(D_ALWAYS,"ImportExportedJobResults() returning true\n");
	return true;
}

int
Scheduler::import_exported_job_results_handler(int /*cmd*/, Stream *stream)
{
	ReliSock *rsock = (ReliSock*)stream;
	ClassAd reqAd; // classad specifying arguments for the export
	ClassAd resultAd; // classad returning results of the export

	rsock->decode();
	rsock->timeout(15);
	if( !getClassAd(rsock, reqAd) || !rsock->end_of_message()) {
		dprintf( D_ALWAYS, "import_exported_job_results_handler Failed to receive classad in the request\n" );
		return FALSE;
	}

	std::string import_dir;     // directory containing the modified job queue and other files from previous export
	if ( ! reqAd.LookupString("ExportDir", import_dir) || import_dir.empty())
	{
		resultAd.Assign(ATTR_ACTION_RESULT, NOT_OK);
		resultAd.Assign(ATTR_ERROR_STRING, "Invalid arguments");
		resultAd.Assign(ATTR_ERROR_CODE,SCHEDD_ERR_MISSING_ARGUMENT );
	}
	else 
	{
		dprintf(D_ALWAYS,"Calling ImportExportedJobResults(%s)\n", import_dir.c_str());
		if ( ! ImportExportedJobResults(resultAd, import_dir.c_str(), EffectiveUserRec(rsock))) {
			resultAd.Assign(ATTR_ACTION_RESULT, NOT_OK);
			resultAd.Assign(ATTR_ERROR_CODE, 1);
		} else {
			resultAd.Assign(ATTR_ACTION_RESULT, OK);
		}
	}

	// send a reply consisting of a result classad
	rsock->encode();
	if ( ! putClassAd(rsock, resultAd)) {
		dprintf( D_ALWAYS, "Error sending export-jobs result to client, aborting.\n" );
		return FALSE;
	}
	if ( ! rsock->end_of_message()) {
		dprintf( D_ALWAYS, "Error sending end-of-message to client, aborting.\n" );
		return FALSE;
	}

	return TRUE;
}


bool
Scheduler::UnexportJobs(ClassAd & result, std::set<int> & clusters, const OwnerInfo * user)
{
	dprintf(D_ALWAYS,"UnexportJobs(...)\n");

	if ( clusters.empty() ) {
		result.Assign(ATTR_ERROR_STRING, "No clusters to unexport");
		dprintf(D_ALWAYS, "ExportJobs(): No clusters to unexport\n");
		return false;
	}

	BeginTransaction();

	int num_jobs = 0;
	for (auto cid = clusters.begin(); cid != clusters.end(); ++cid) {
		JobQueueCluster * jqc = GetClusterAd(*cid);
		if ( ! jqc) continue;

		// verify the user is authorized to edit this job cluster
		if ( ! UserCheck2(jqc, user) ) {
			result.Assign(ATTR_ERROR_STRING, "User not authorized to unexport given jobs");
			dprintf(D_ALWAYS, "UnexportJobs(): User %s not authorized to unexport job owned by %s, aborting\n", user->Name(), jqc->ownerinfo->Name());
			AbortTransaction();
			return false;
		}

		// unexport all jobs in the cluster
		for (JobQueueJob * job = jqc->FirstJob(); job != NULL; job = jqc->NextJob(job)) {
			// skip jobs that are not externally managed.
			if (!jobExternallyManaged(job)) continue;

			// TODO: verify external manager is "Lumberjack"?

			// take the job out of the managed state
			DeleteAttribute(job->jid.cluster, job->jid.proc, ATTR_JOB_MANAGED);
			DeleteAttribute(job->jid.cluster, job->jid.proc, ATTR_JOB_MANAGED_MANAGER);
			++num_jobs;
		}
	}

	CommitTransactionOrDieTrying();

	result.Assign(ATTR_TOTAL_JOB_ADS, num_jobs);
	result.Assign(ATTR_TOTAL_CLUSTER_ADS, (long)clusters.size());

	dprintf(D_ALWAYS,"UnexportJobs() returning true\n");
	return true;
}

int
Scheduler::unexport_jobs_handler(int /*cmd*/, Stream *stream)
{
	ReliSock *rsock = (ReliSock*)stream;
	ClassAd reqAd; // classad specifying arguments for the unexport
	ClassAd resultAd; // classad returning results of the unexport

	rsock->decode();
	rsock->timeout(15);
	if( !getClassAd(rsock, reqAd) || !rsock->end_of_message()) {
		dprintf( D_ALWAYS, "unexport_jobs_handler Failed to receive classad in the request\n" );
		return FALSE;
	}

	std::string unexport_list;    // list of cluster ids to unexport
	ExprTree * constr_expr = reqAd.Lookup(ATTR_ACTION_CONSTRAINT);
	if ( ! constr_expr && ! reqAd.LookupString(ATTR_ACTION_IDS, unexport_list) )
	{
		resultAd.Assign(ATTR_ACTION_RESULT, NOT_OK);
		resultAd.Assign(ATTR_ERROR_STRING, "Invalid arguments");
		resultAd.Assign(ATTR_ERROR_CODE, SCHEDD_ERR_MISSING_ARGUMENT);
	}
	else
	{
		std::set<int> clusters;
		// we have a constrain expression, so we have to turn that into a set of cluster ids
		if (constr_expr) {
			schedd_runtime_probe runtime;
			struct _cluster_ids_args {
				std::set<int> * pids;
				ExprTree * constraint;
			} args = { &clusters, constr_expr };

			// build up a set of cluster ids that match the constraint expression
			WalkJobQueueEntries(
				(WJQ_WITH_CLUSTERS | WJQ_WITH_NO_JOBS),
				[](JobQueuePayload job, const JOB_ID_KEY & cid, void * pv) -> int {
					struct _cluster_ids_args & args = *(struct _cluster_ids_args*)pv;
					if ( ! args.constraint || EvalExprBool(job, args.constraint)) {
						args.pids->insert(cid.cluster);
					}
					return 0;
				},
				&args,
				runtime);

		} else {
			// we have a string list of cluster ids
			// build a set of cluster ids from the string of ids that was passed
			StringTokenIterator sit(unexport_list);
			for (const char * id = sit.first(); id != NULL; id = sit.next()) {
				clusters.insert(atoi(id));
			}
		}
		if ( ! UnexportJobs(resultAd, clusters, EffectiveUserRec(rsock)) ) {
			resultAd.Assign(ATTR_ACTION_RESULT, NOT_OK);
			resultAd.Assign(ATTR_ERROR_CODE, 1);
		} else {
			resultAd.Assign(ATTR_ACTION_RESULT, OK);
		}
	}

	// send a reply consisting of a result classad
	rsock->encode();
	if ( ! putClassAd(rsock, resultAd)) {
		dprintf( D_ALWAYS, "Error sending unexport-jobs result to client, aborting.\n" );
		return FALSE;
	}
	if ( ! rsock->end_of_message()) {
		dprintf( D_ALWAYS, "Error sending end-of-message to client, aborting.\n" );
		return FALSE;
	}

	return TRUE;
}

