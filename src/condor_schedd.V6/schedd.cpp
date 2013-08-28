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
#include "condor_collector.h"
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
#include "../condor_ckpt_server/server_interface.h"
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
#include "nullfile.h"
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
#include "condor_netdb.h"
#include "fs_util.h"
#include "condor_mkstemp.h"
#include "tdman.h"
#include "utc_time.h"
#include "schedd_files.h"
#include "file_sql.h"
#include "condor_getcwd.h"
#include "set_user_priv_from_ad.h"
#include "classad_visa.h"
#include "subsystem_info.h"
#include "../condor_privsep/condor_privsep.h"
#include "authentication.h"
#include "setenv.h"
#include "classadHistory.h"
#include "forkwork.h"
#include "condor_open.h"
#include "schedd_negotiate.h"
#include "filename_tools.h"
#include "ipv6_hostname.h"
#include "globus_utils.h"
#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
#include "ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#endif
#endif
#include <algorithm>
#include <sstream>

#if defined(WINDOWS) && !defined(MAXINT)
	#define MAXINT INT_MAX
#endif

#define DEFAULT_SHADOW_SIZE 800
#define DEFAULT_JOB_START_COUNT 1

#define SUCCESS 1
#define CANT_RUN 0

char const * const HOME_POOL_SUBMITTER_TAG = "";

extern char *gen_ckpt_name();

extern GridUniverseLogic* _gridlogic;

#include "condor_qmgr.h"
#include "qmgmt.h"
#include "condor_vm_universe_types.h"
#include "enum_utils.h"

extern "C"
{
/*	int SetCkptServerHost(const char *host);
	int RemoveLocalOrRemoteFile(const char *, const char *);
	int FileExists(const char *, const char *);
	char* gen_ckpt_name(char*, int, int, int);
	int getdtablesize();
*/
	int prio_compar(prio_rec*, prio_rec*);
}

extern char* Spool;
extern char * Name;
static char * NameInEnv = NULL;
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

extern FILESQL *FILEObj;

// priority records
extern prio_rec *PrioRec;
extern int N_PrioRecs;
extern int grow_prio_recs(int);

void cleanup_ckpt_files(int , int , char*);
void send_vacate(match_rec*, int);
void mark_job_stopped(PROC_ID*);
void mark_job_running(PROC_ID*);
void mark_serial_job_running( PROC_ID *job_id );
int fixAttrUser( ClassAd *job );
shadow_rec * find_shadow_rec(PROC_ID*);
bool service_this_universe(int, ClassAd*);
bool jobIsSandboxed( ClassAd* ad );
bool getSandbox( int cluster, int proc, MyString & path );
bool jobPrepNeedsThread( int cluster, int proc );
bool jobCleanupNeedsThread( int cluster, int proc );
bool jobExternallyManaged(ClassAd * ad);
bool jobManagedDone(ClassAd * ad);
int  count( ClassAd *job );
static void WriteCompletionVisa(ClassAd* ad);


int	WallClockCkptInterval = 0;
int STARTD_CONTACT_TIMEOUT = 45;  // how long to potentially block

#ifdef CARMI_OPS
struct shadow_rec *find_shadow_by_cluster( PROC_ID * );
#endif

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
		break;
	default:
		return;
	}

	if ( !strcmp( get_condor_username(), sock.getOwner() ) ) {
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

void AuditLogJobProxy( Sock &sock, PROC_ID job_id, const char *proxy_file )
{
	// Quickly determine if we're writing to the audit log.
	if ( !IsDebugCategory( D_AUDIT ) ) {
		return;
	}

	dprintf( D_AUDIT, sock, "Received proxy for job %d.%d\n",
			 job_id.cluster, job_id.proc );
	dprintf( D_AUDIT, sock, "proxy path: %s\n", proxy_file );

#if defined(HAVE_EXT_GLOBUS)
	globus_gsi_cred_handle_t proxy_handle = x509_proxy_read( proxy_file );

	if ( proxy_handle == NULL ) {
		dprintf( D_AUDIT|D_FAILURE, sock, "Failed to read job proxy: %s\n",
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

	x509_proxy_free( proxy_handle );

	dprintf( D_AUDIT, sock, "proxy expiration: %d\n", (int)expire_time );
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

void AuditLogJobProxy( Sock &sock, ClassAd *job_ad )
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

unsigned int UserIdentity::HashFcn(const UserIdentity & index)
{
	return index.m_username.Hash() + index.m_domain.Hash() + index.m_auxid.Hash();
}

UserIdentity::UserIdentity(const char *user, const char *domainname, 
						   ClassAd *ad):
	m_username(user), 
	m_domain(domainname),
	m_auxid("")
{
	ExprTree *tree = const_cast<ExprTree *>(scheduler.getGridParsedSelectionExpr());
	classad::Value val;
	const char *str;
	if ( ad && tree && 
		 EvalExprTree(tree,ad,NULL,val) && val.IsStringValue(str) )
	{
		m_auxid = str;
	}
}

struct job_data_transfer_t {
	int mode;
	char *peer_version;
	ExtArray<PROC_ID> *jobs;
};

match_rec::match_rec( char const* claim_id, char const* p, PROC_ID* job_id, 
					  const ClassAd *match, char const *the_user, char const *my_pool,
					  bool is_dedicated_arg ):
	ClaimIdParser(claim_id)
{
	peer = strdup( p );
	origcluster = cluster = job_id->cluster;
	proc = job_id->proc;
	status = M_UNCLAIMED;
	entered_current_status = (int)time(0);
	shadowRec = NULL;
	alive_countdown = 0;
	num_exceptions = 0;
	if( match ) {
		my_match_ad = new ClassAd( *match );
		if( IsDebugLevel(D_MACHINE) ) {
			dprintf( D_MACHINE, "*** ClassAd of Matched Resource ***\n" );
			dPrintAd( D_MACHINE, *my_match_ad );
			dprintf( D_MACHINE | D_NOHEADER, "*** End of ClassAd ***\n" );
		}		
	} else {
		my_match_ad = NULL;
	}
	user = strdup( the_user );
	if( my_pool ) {
		pool = strdup( my_pool );
	} else {
		pool = NULL;
	}
	sent_alive_interval = false;
	this->is_dedicated = is_dedicated_arg;
	allocated = false;
	scheduled = false;
	needs_release_claim = false;
	claim_requester = NULL;
	auth_hole_id = NULL;
	m_startd_sends_alives = false;

	makeDescription();

	bool suppress_sec_session = true;

	if( param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION",false) ) {
		if( secSessionId() == NULL ) {
			dprintf(D_FULLDEBUG,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: did not create security session from claim id, because claim id does not contain session information: %s\n",publicClaimId());
		}
		else {
			bool rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
				DAEMON,
				secSessionId(),
				secSessionKey(),
				secSessionInfo(),
				EXECUTE_SIDE_MATCHSESSION_FQU,
				peer,
				0 );

			if( rc ) {
					// we're good to go; use the claimid security session
				suppress_sec_session = false;
			}
			if( !rc ) {
				dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create security session for %s, so will try to obtain a new security session\n",publicClaimId());
			}
		}
	}
	if( suppress_sec_session ) {
		suppressSecSession( true );
			// Now secSessionId() will always return NULL, so we will
			// not try to do anything with the claimid security session.
			// Most importantly, we will not try to delete it when this
			// match rec is destroyed.  (If we failed to create the session,
			// that may because it already exists, and this is a duplicate
			// match record that will soon be thrown out.)
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
		m_startd_sends_alives = false;
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

	if( m_description.Length() ) {
		m_description += " ";
	}
	if( IsFulldebug(D_FULLDEBUG) ) {
		m_description += publicClaimId();
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
	if( pool ) {
		free(pool);
	}

	if( claim_requester.get() ) {
			// misc_data points to this object, so NULL it out, just to be safe
		claim_requester->setMiscDataPtr( NULL );
		claim_requester->cancelMessage();
		claim_requester = NULL;
	}

	if( secSessionId() ) {
			// Expire the session after enough time to let the final
			// RELEASE_CLAIM command finish, in case it is still in
			// progress.  This also allows us to more gracefully
			// handle any final communication from the startd that may
			// still be in flight.
		daemonCore->getSecMan()->SetSessionExpiration(secSessionId(),time(NULL)+600);
			// In case we get the same claim id again before the slop time
			// expires, mark this session as "lingering" so we know it can
			// be replaced.
		daemonCore->getSecMan()->SetSessionLingerFlag(secSessionId());
	}
}


void
match_rec::setStatus( int stat )
{
	status = stat;
	entered_current_status = (int)time(0);
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


ContactStartdArgs::ContactStartdArgs( char const* the_claim_id, char* sinfulstr, bool is_dedicated ) 
{
	csa_claim_id = strdup( the_claim_id );
	csa_sinful = strdup( sinfulstr );
	csa_is_dedicated = is_dedicated;
}


ContactStartdArgs::~ContactStartdArgs()
{
	free( csa_claim_id );
	free( csa_sinful );
}

	// Years of careful research
static const int USER_HASH_SIZE = 100;

Scheduler::Scheduler() :
    m_adSchedd(NULL),
    m_adBase(NULL),
	GridJobOwners(USER_HASH_SIZE, UserIdentity::HashFcn, updateDuplicateKeys),
	stop_job_queue( "stop_job_queue" ),
	act_on_job_myself_queue( "act_on_job_myself_queue" ),
	job_is_finished_queue( "job_is_finished_queue", 1 ),
	m_local_startd_pid(-1)
{
	MyShadowSockName = NULL;
	shadowCommandrsock = NULL;
	shadowCommandssock = NULL;
	QueueCleanInterval = 0; JobStartDelay = 0;
	JobStopDelay = 0;
	JobStopCount = 1;
	RequestClaimTimeout = 0;
	MaxJobsRunning = 0;
	MaxJobsSubmitted = INT_MAX;
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

	stats.Init(0);

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

	ShadowSizeEstimate = 0;

	N_Owners = 0;
	NegotiationRequestTime = 0;

		//gotiator = NULL;
	CondorAdministrator = NULL;
	Mail = NULL;
	alive_interval = 0;
	leaseAliveInterval = 500000;	// init to a nice big number
	aliveid = -1;
	ExitWhenDone = FALSE;
	matches = NULL;
	matchesByJobID = NULL;
	shadowsByPid = NULL;
	spoolJobFileWorkers = NULL;

	shadowsByProcID = NULL;
	resourcesByProcID = NULL;
	numMatches = 0;
	numShadows = 0;
	FlockCollectors = NULL;
	FlockNegotiators = NULL;
	MaxFlockLevel = 0;
	FlockLevel = 0;
	StartJobTimer=-1;
	timeoutid = -1;
	startjobsid = -1;
	periodicid = -1;

#ifdef HAVE_EXT_POSTGRESQL
	quill_enabled = FALSE;
	quill_is_remotely_queryable = 0; //false
	quill_name = NULL;
	quill_db_name = NULL;
	quill_db_ip_addr = NULL;
	quill_db_query_password = NULL;
#endif

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

	jobThrottleNextJobDelay = 0;
#ifdef HAVE_EXT_POSTGRESQL
	prevLHF = 0;
#endif

}


Scheduler::~Scheduler()
{
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
	if (Mail)
		free(Mail);
	if (matches) {
		matches->startIterations();
		match_rec *rec;
		HashKey id;
		while (matches->iterate(id, rec) == 1) {
			delete rec;
		}
		delete matches;
	}
	if (matchesByJobID) {
		delete matchesByJobID;
	}
	if (shadowsByPid) {
		shadowsByPid->startIterations();
		shadow_rec *rec;
		int pid;
		while (shadowsByPid->iterate(pid, rec) == 1) {
			delete rec;
		}
		delete shadowsByPid;
	}
	if (spoolJobFileWorkers) {
		spoolJobFileWorkers->startIterations();
		ExtArray<PROC_ID> * rec;
		int pid;
		while (spoolJobFileWorkers->iterate(pid, rec) == 1) {
			delete rec;
		}
		delete spoolJobFileWorkers;
	}
	if (shadowsByProcID) {
		delete shadowsByProcID;
	}
	if ( resourcesByProcID ) {
		resourcesByProcID->startIterations();
		ClassAd* jobAd;
		while (resourcesByProcID->iterate(jobAd) == 1) {
			if (jobAd) delete jobAd;
		}
		delete resourcesByProcID;
	}
	if( FlockCollectors ) {
		delete FlockCollectors;
		FlockCollectors = NULL;
	}
	if( FlockNegotiators ) {
		delete FlockNegotiators;
		FlockNegotiators = NULL;
	}
	if ( checkContactQueue_tid != -1 && daemonCore ) {
		daemonCore->Cancel_Timer(checkContactQueue_tid);
	}

	int i;
	for( i=0; i<N_Owners; i++) {
		if( Owners[i].Name ) { 
			free( Owners[i].Name );
			Owners[i].Name = NULL;
		}
	}

	if (_gridlogic) {
		delete _gridlogic;
	}
	if ( m_parsed_gridman_selection_expr ) {
		delete m_parsed_gridman_selection_expr;
	}
	if ( m_unparsed_gridman_selection_expr ) {
		free(m_unparsed_gridman_selection_expr);
	}
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
}

// If a job has been spooling for 12 hours,
// It may well be that the remote condor_submit died
// So we kill this job
int check_for_spool_zombies(ClassAd *ad)
{
	int cluster;
	if( !ad->LookupInteger( ATTR_CLUSTER_ID, cluster) ) {
		return 0;
	}
	int proc;
	if( !ad->LookupInteger( ATTR_PROC_ID, proc) ) {
		return 0;
	}
	int hold_status;
	if( GetAttributeInt(cluster,proc,ATTR_JOB_STATUS,&hold_status) >= 0 ) {
		if(hold_status == HELD) {
			int hold_reason_code;
			if( GetAttributeInt(cluster,proc,ATTR_HOLD_REASON_CODE,
					&hold_reason_code) >= 0) {
				if(hold_reason_code == CONDOR_HOLD_CODE_SpoolingInput) {
					dprintf( D_FULLDEBUG, "Job %d.%d held for spooling. "
						"Checking how long...\n",cluster,proc);
					int stage_in_start;
					int ret = GetAttributeInt(cluster,proc,ATTR_STAGE_IN_START,
							&stage_in_start);
					if(ret >= 0) {
						time_t now = time(NULL);
						int diff = now - stage_in_start;
						dprintf( D_FULLDEBUG, "Job %d.%d on hold for %d seconds.\n",
							cluster,proc,diff);
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
Scheduler::timeout()
{
	static bool min_interval_timer_set = false;
	static bool walk_job_queue_timer_set = false;

		// If we are called too frequently, delay.
	SchedDInterval.expediteNextRun();
	unsigned int time_to_next_run = SchedDInterval.getTimeToNextRun();

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

	count_jobs();

	clean_shadow_recs();

		// Spooling should not take too long
	WalkJobQueue(check_for_spool_zombies);

	/* Call preempt() if we are running more than max jobs; however, do not
	 * call preempt() here if we are shutting down.  When shutting down, we have
	 * a timer which is progressively preempting just one job at a time.
	 */
	int real_jobs = numShadows - SchedUniverseJobsRunning 
		- LocalUniverseJobsRunning;
	if( (real_jobs > MaxJobsRunning) && (!ExitWhenDone) ) {
		dprintf( D_ALWAYS, 
				 "Preempting %d jobs due to MAX_JOBS_RUNNING change\n",
				 (real_jobs - MaxJobsRunning) );
		preempt( real_jobs - MaxJobsRunning );
		m_need_reschedule = false;
	}

	if( LocalUniverseJobsIdle > 0 || SchedUniverseJobsIdle > 0 ) {
		this->calculateCronTabSchedules();
		StartLocalJobs();
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
Scheduler::fill_submitter_ad(ClassAd & pAd, int owner_num, int flock_level)
{
	const int i = owner_num;
	const int dprint_level = D_FULLDEBUG;
	const bool want_dprintf = flock_level < 1; // dprintf if not flocking

	if (Owners[i].FlockLevel >= flock_level) {
		pAd.Assign(ATTR_IDLE_JOBS, Owners[i].JobsIdle);
		if (want_dprintf)
			dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_IDLE_JOBS, Owners[i].JobsIdle);
	} else if (Owners[i].OldFlockLevel >= flock_level ||
				Owners[i].JobsRunning > 0) {
		pAd.Assign(ATTR_IDLE_JOBS, (int)0);
	} else {
		// if we're no longer flocking with this pool and
		// we're not running jobs in the pool, then don't send
		// an update
		return false;
	}

	pAd.Assign(ATTR_RUNNING_JOBS, Owners[i].JobsRunning);
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_RUNNING_JOBS, Owners[i].JobsRunning);

	pAd.Assign(ATTR_IDLE_JOBS, Owners[i].JobsIdle);
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_IDLE_JOBS, Owners[i].JobsIdle);

	pAd.Assign(ATTR_WEIGHTED_RUNNING_JOBS, Owners[i].WeightedJobsRunning);
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_WEIGHTED_RUNNING_JOBS, Owners[i].WeightedJobsRunning);

	pAd.Assign(ATTR_WEIGHTED_IDLE_JOBS, Owners[i].WeightedJobsIdle);
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_WEIGHTED_IDLE_JOBS, Owners[i].WeightedJobsIdle);

	pAd.Assign(ATTR_HELD_JOBS, Owners[i].JobsHeld);
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_HELD_JOBS, Owners[i].JobsHeld);

	pAd.Assign(ATTR_FLOCKED_JOBS, Owners[i].JobsFlocked);
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %d\n", ATTR_FLOCKED_JOBS, Owners[i].JobsFlocked);

	MyString str;
	if ( param_boolean("USE_GLOBAL_JOB_PRIOS",false) ) {
		int max_entries = param_integer("MAX_GLOBAL_JOB_PRIOS",500);
		int num_prios = Owners[i].PrioSet.size();
		if (num_prios > max_entries) {
			pAd.Assign(ATTR_JOB_PRIO_ARRAY_OVERFLOW, num_prios);
			if (want_dprintf)
				dprintf (dprint_level, "Changed attribute: %s = %d\n",
						 ATTR_JOB_PRIO_ARRAY_OVERFLOW, num_prios);
		} else {
			// if no overflow, do not advertise ATTR_JOB_PRIO_ARRAY_OVERFLOW
			pAd.Delete(ATTR_JOB_PRIO_ARRAY_OVERFLOW);
		}
		// reverse iterator to go high to low prio
		std::set<int>::reverse_iterator rit;
		int num_entries = 0;
		for (rit=Owners[i].PrioSet.rbegin();
			 rit!=Owners[i].PrioSet.rend() && num_entries < max_entries;
			 ++rit)
		{
			if ( !str.IsEmpty() ) {
				str += ",";
			}
			str += *rit;
			num_entries++;
		}
		// NOTE: we rely on that fact that str.Value() will return "", not NULL, if empty
		pAd.Assign(ATTR_JOB_PRIO_ARRAY, str.Value());
		if (want_dprintf)
			dprintf (dprint_level, "Changed attribute: %s = %s\n", ATTR_JOB_PRIO_ARRAY,str.Value());
	}

	str.formatstr("%s@%s", Owners[i].Name, UidDomain);
	pAd.Assign(ATTR_NAME, str.Value());
	if (want_dprintf)
		dprintf (dprint_level, "Changed attribute: %s = %s@%s\n", ATTR_NAME, Owners[i].Name, UidDomain);

	return true;
}

/*
** Examine the job queue to determine how many CONDOR jobs we currently have
** running, and how many individual users own them.
*/
int
Scheduler::count_jobs()
{
	ClassAd * cad = m_adSchedd;
	int		i, j;

	 // copy owner data to old-owners table
	ExtArray<OwnerData> OldOwners(Owners);
	int Old_N_Owners=N_Owners;

	N_Owners = 0;
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
	stats.JobsRunningRuntimes = 0;
	stats.JobsRunningSizes = 0;

	// clear owner table contents
	time_t current_time = time(0);
	for ( i = 0; i < Owners.getsize(); i++) {
		Owners[i].Name = NULL;
		Owners[i].JobsRunning = 0;
		Owners[i].JobsIdle = 0;
		Owners[i].JobsHeld = 0;
		Owners[i].JobsFlocked = 0;
		Owners[i].FlockLevel = 0;
		Owners[i].OldFlockLevel = 0;
		Owners[i].NegotiationTimestamp = current_time;
		Owners[i].PrioSet.clear();
	}

	GridJobOwners.clear();

		// Clear out the DedicatedScheduler's list of idle dedicated
		// job cluster ids, since we're about to re-create it.
	dedicated_scheduler.clearDedicatedClusters();

	WalkJobQueue((int(*)(ClassAd *)) count );

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
		char *at_sign = strchr(rec->user, '@');
		if (at_sign) *at_sign = '\0';
		int OwnerNum = insert_owner( rec->user );
		if (at_sign) *at_sign = '@';
		if (rec->shadowRec && !rec->pool) {
				// Sum up the # of cpus claimed by this user and advertise it as
				// WeightedJobsRunning.  Technically, should look at SlotWeight
				// but the IdleJobRunning only know about request_cpus, etc.
				// and hard-codes cpus as the weight, so we do the same here.
				// This is needed as the HGQ code in the negotitator needs to
				// know weighted demand to dole out surplus properly.
			int request_cpus = 1;
			if (rec->my_match_ad) {
				if(0 == rec->my_match_ad->LookupInteger(ATTR_CPUS, request_cpus)) {
					request_cpus = 1;
				}
			}
			Owners[OwnerNum].WeightedJobsRunning += request_cpus;
			Owners[OwnerNum].JobsRunning++;
		} else {				// in remote pool, so add to Flocked count
			Owners[OwnerNum].JobsFlocked++;
			JobsFlocked++;
		}
	}

	// set FlockLevel for owners
	if (MaxFlockLevel) {
		for ( i=0; i < N_Owners; i++) {
			for ( j=0; j < Old_N_Owners; j++) {
				if (!strcmp(OldOwners[j].Name,Owners[i].Name)) {
					Owners[i].FlockLevel = OldOwners[j].FlockLevel;
					Owners[i].OldFlockLevel = OldOwners[j].OldFlockLevel;
						// Remember our last negotiation time if we have
						// idle jobs, so we can determine if the negotiator
						// is ignoring us and we should flock.  If we don't
						// have any idle jobs, we leave NegotiationTimestamp
						// at its initial value (the current time), since
						// we don't want any negotiations, and thus we don't
						// want to timeout and increase our flock level.
					if (Owners[i].JobsIdle) {
						Owners[i].NegotiationTimestamp =
							OldOwners[j].NegotiationTimestamp;
					}
				}
			}
			// if this owner hasn't received a negotiation in a long time,
			// then we should flock -- we need this case if the negotiator
			// is down or simply ignoring us because our priority is so low
			if ((current_time - Owners[i].NegotiationTimestamp >
				 SchedDInterval.getDefaultInterval()*2) && (Owners[i].FlockLevel < MaxFlockLevel)) {
				int old_flocklevel = Owners[i].FlockLevel;
				Owners[i].FlockLevel += param_integer("FLOCK_INCREMENT",1,1);
				if(Owners[i].FlockLevel > MaxFlockLevel) {
					Owners[i].FlockLevel = MaxFlockLevel;
				}
				Owners[i].NegotiationTimestamp = current_time;
				dprintf(D_ALWAYS,
						"Increasing flock level for %s to %d from %d. (Due to lack of activity from negotiator)\n",
						Owners[i].Name, Owners[i].FlockLevel, old_flocklevel);
			}
			if (Owners[i].FlockLevel > FlockLevel) {
				FlockLevel = Owners[i].FlockLevel;
			}
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
	dprintf( D_FULLDEBUG, "N_Owners = %d\n", N_Owners );
	dprintf( D_FULLDEBUG, "MaxJobsRunning = %d\n", MaxJobsRunning );

	// later when we compute job priorities, we will need PrioRec
	// to have as many elements as there are jobs in the queue.  since
	// we just counted the jobs, lets make certain that PrioRec is 
	// large enough.  this keeps us from guessing to small and constantly
	// growing PrioRec... Add 5 just to be sure... :^) -Todd 8/97
	grow_prio_recs( JobsRunning + JobsIdle + 5 );
	
	cad->Assign(ATTR_NUM_USERS, N_Owners);
	cad->Assign(ATTR_MAX_JOBS_RUNNING, MaxJobsRunning);

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

	m_xfer_queue_mgr.publish(cad);

	// one last check for any newly-queued jobs
	// this count is cumulative within the qmgmt package
	// TJ: get daemon-core message-time here and pass into stats.Tick()
	stats.Tick();
	stats.JobsSubmitted = GetJobQueuedCount();

	OtherPoolStats.Tick();
	// because cad is really m_adSchedd which is persistent, we have to 
	// actively delete expired statistics atributes.
	OtherPoolStats.UnpublishDisabled(*cad);
	OtherPoolStats.RemoveDisabled();
	OtherPoolStats.Publish(*cad);

	// publish scheduler generic statistics
	stats.Publish(*cad);

	daemonCore->publish(cad);
	daemonCore->dc_stats.Publish(*cad);
	daemonCore->monitor_data.ExportData(cad);
	extra_ads.Publish( cad );

	if ( param_boolean("ENABLE_SOAP", false) ) {
			// If we can support the SOAP API let's let the world know!
		cad->Assign(ATTR_HAS_SOAP_API, true);
	}

	// can't do this at init time, the job_queue_log doesn't exist at that time.
	int job_queue_birthdate = (int)GetOriginalJobQueueBirthdate();
	cad->Assign(ATTR_JOB_QUEUE_BIRTHDATE, job_queue_birthdate);
	m_adBase->Assign(ATTR_JOB_QUEUE_BIRTHDATE, job_queue_birthdate);

	daemonCore->UpdateLocalAd(cad);

		// log classad into sql log so that it can be updated to DB
#ifdef HAVE_EXT_POSTGRESQL
	FILESQL::daemonAdInsert(cad, "ScheddAd", FILEObj, prevLHF);
#endif

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
	ScheddPluginManager::Update(UPDATE_SCHEDD_AD, cad);
#endif
#endif
	
		// Update collectors
	int num_updates = daemonCore->sendUpdates(UPDATE_SCHEDD_AD, cad, NULL, true);
	dprintf( D_FULLDEBUG, 
			 "Sent HEART BEAT ad to %d collectors. Number of submittors=%d\n",
			 num_updates, N_Owners );   

	// send the schedd ad to our flock collectors too, so we will
	// appear in condor_q -global and condor_status -schedd
	if( FlockCollectors ) {
		FlockCollectors->rewind();
		Daemon* d;
		DCCollector* col;
		FlockCollectors->next(d);
		for( i=0; d && i < FlockLevel; i++ ) {
			col = (DCCollector*)d;
			col->sendUpdate( UPDATE_SCHEDD_AD, cad, NULL, true );
			FlockCollectors->next( d );
		}
	}

	// --------------------------------------------------------------------------------------
	// begin publishing submitter ADs
	//

	// Set attributes common to all submitter Ads
	// 
	SetMyTypeName(*m_adBase, SUBMITTER_ADTYPE);
	m_adBase->Assign(ATTR_SCHEDD_NAME, Name);
	daemonCore->publish(m_adBase);
	extra_ads.Publish(m_adBase);

	// Create a new add for the per-submitter attribs 
	// and chain it to the base ad.

	ClassAd pAd;
	pAd.ChainToAd(m_adBase);
	pAd.Assign(ATTR_SUBMITTER_TAG,HOME_POOL_SUBMITTER_TAG);

	MyString submitter_name;
	for ( i=0; i<N_Owners; i++) {

	  if ( !fill_submitter_ad(pAd,i) ) continue;

	  dprintf( D_ALWAYS, "Sent ad to central manager for %s@%s\n", 
			   Owners[i].Name, UidDomain );

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
	  ScheddPluginManager::Update(UPDATE_SUBMITTOR_AD, &pAd);
#endif
#endif
		// Update collectors
	  num_updates = daemonCore->sendUpdates(UPDATE_SUBMITTOR_AD, &pAd, NULL, true);
	  dprintf( D_ALWAYS, "Sent ad to %d collectors for %s@%s\n", 
			   num_updates, Owners[i].Name, UidDomain );
	}

	// update collector of the pools with which we are flocking, if
	// any
	Daemon* d;
	Daemon* flock_neg;
	DCCollector* flock_col;
	if( FlockCollectors && FlockNegotiators ) {
		FlockCollectors->rewind();
		FlockNegotiators->rewind();
		for( int flock_level = 1;
			 flock_level <= MaxFlockLevel; flock_level++) {
			FlockNegotiators->next( flock_neg );
			FlockCollectors->next( d );
			flock_col = (DCCollector*)d;
			if( ! (flock_col && flock_neg) ) { 
				continue;
			}
			for (i=0; i < N_Owners; i++) {
				Owners[i].JobsRunning = 0;
				Owners[i].JobsFlocked = 0;
			}
			matches->startIterations();
			match_rec *mRec;
			while(matches->iterate(mRec) == 1) {
				char *at_sign = strchr(mRec->user, '@');
				if (at_sign) *at_sign = '\0';
				int OwnerNum = insert_owner( mRec->user );
				if (at_sign) *at_sign = '@';
				if (mRec->shadowRec && mRec->pool &&
					!strcmp(mRec->pool, flock_neg->pool())) {
					Owners[OwnerNum].JobsRunning++;
				} else {
						// This is a little weird.  We're sending an update
						// to a pool we're flocking with.  We count jobs
						// running in that pool as "RunningJobs" and jobs
						// running in other pools (including the local pool)
						// as "FlockedJobs".  It bends the terminology a bit,
						// but it's the best I can think of for now.
					Owners[OwnerNum].JobsFlocked++;
				}
			}
			// update submitter ad in this pool for each owner
			for (i=0; i < N_Owners; i++) {

				if ( !fill_submitter_ad(pAd,i,flock_level) ) {
					// if we're no longer flocking with this pool and
					// we're not running jobs in the pool, then don't send
					// an update
					continue;
				}

					// we will use this "tag" later to identify which
					// CM we are negotiating with when we negotiate
				pAd.Assign(ATTR_SUBMITTER_TAG,flock_col->name());

				flock_col->sendUpdate( UPDATE_SUBMITTOR_AD, &pAd, NULL, true );
			}
		}
	}

	pAd.Delete(ATTR_SUBMITTER_TAG);

	for (i=0; i < N_Owners; i++) {
		Owners[i].OldFlockLevel = Owners[i].FlockLevel;
	}

	 // Tell our GridUniverseLogic class what we've seen in terms
	 // of Globus Jobs per owner.
	GridJobOwners.startIterations();
	UserIdentity userident;
	GridJobCounts gridcounts;
	while( GridJobOwners.iterate(userident, gridcounts) ) {
		if(gridcounts.GridJobs > 0) {
			GridUniverseLogic::JobCountUpdate(
					userident.username().Value(),
					userident.domain().Value(),
					userident.auxid().Value(),m_unparsed_gridman_selection_expr, 0, 0, 
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

 	// send ads for owner that don't have jobs idle
	// This is done by looking at the old owners list and searching for owners
	// that are not in the current list (the current list has only owners w/ idle jobs)
	for ( i=0; i<Old_N_Owners; i++) {

		// check that the old name is not in the new list
		int k;
		for(k=0; k<N_Owners;k++) {
		  if (!strcmp(OldOwners[i].Name,Owners[k].Name)) break;
		}

		// In case we want to update this ad, we have to build the submitter
		// name string that we will be assigning with before we free the owner name.
		submitter_name.formatstr("%s@%s", OldOwners[i].Name, UidDomain);

		// Now that we've finished using OldOwners[i].Name, we can
		// free it.
		if ( OldOwners[i].Name ) {
			free(OldOwners[i].Name);
			OldOwners[i].Name = NULL;
		}

		  // If k < N_Owners, we found this OldOwner in the current
		  // Owners table, therefore, we don't want to send the
		  // submittor ad with 0 jobs, so we continue to the next
		  // entry in the OldOwner table.
		if (k<N_Owners) continue;

		pAd.Assign(ATTR_NAME, submitter_name.Value());
		dprintf (D_FULLDEBUG, "Changed attribute: %s = %s\n", ATTR_NAME, submitter_name.Value());

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
	// update plugins
	dprintf(D_FULLDEBUG,"Sent owner (0 jobs) ad to schedd plugins\n");
	ScheddPluginManager::Update(UPDATE_SUBMITTOR_AD, &pAd);
#endif
#endif

		// Update collectors
	  int num_udates = 
		  daemonCore->sendUpdates(UPDATE_SUBMITTOR_AD, &pAd, NULL, true);
	  dprintf(D_ALWAYS, "Sent owner (0 jobs) ad to %d collectors\n",
			  num_udates);

	  // also update all of the flock hosts
	  Daemon *da;
	  if( FlockCollectors ) {
		  int flock_level;
		  for( flock_level=1, FlockCollectors->rewind();
			   flock_level <= OldOwners[i].OldFlockLevel &&
				   FlockCollectors->next(da); flock_level++ ) {
			  ((DCCollector*)da)->sendUpdate( UPDATE_SUBMITTOR_AD, &pAd, NULL, true );
		  }
	  }
	}

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

   MyString stats_config;
   if (pQueryAd) {
      pQueryAd->LookupString("STATISTICS_TO_PUBLISH",stats_config);
   }

   // should we call count_jobs() to update the job counts?

   stats.Tick(now);
   stats.JobsSubmitted = GetJobQueuedCount();
   stats.ShadowsRunning = numShadows;

   OtherPoolStats.Tick(now);
   // because cad is a copy of m_adSchedd which is persistent, we have to 
   // actively delete expired statistics atributes.
   OtherPoolStats.UnpublishDisabled(*cad);
   OtherPoolStats.RemoveDisabled();

   int flags = stats.PublishFlags;
   if ( ! stats_config.IsEmpty()) {
      flags = generic_stats_ParseConfigString(stats_config.Value(), "SCHEDD", "SCHEDULER", flags);
   }
   OtherPoolStats.Publish(*cad, flags);

   // publish scheduler generic statistics
   stats.Publish(*cad, stats_config.Value());

   m_xfer_queue_mgr.publish(cad, stats_config.Value());

   // publish daemon core stats
   daemonCore->publish(cad);
   daemonCore->dc_stats.Publish(*cad, stats_config.Value());
   daemonCore->monitor_data.ExportData(cad);

   // We want to insert ATTR_LAST_HEARD_FROM into each ad.  The
   // collector normally does this, so if we're servicing a
   // QUERY_SCHEDD_ADS commannd, we need to do this ourselves or
   // some timing stuff won't work.
   cad->Assign(ATTR_LAST_HEARD_FROM, (int)now);
   //cad->Assign( ATTR_AUTHENTICATED_IDENTITY, ??? );

   // add the Scheduler Ad to the list
   ads.Insert(cad);

   // now add the submitter ads, each one based on the base 
   // submitter ad, note that chained ad's dont delete the 
   // chain parent when they are deleted, so it's safe to 
   // put these ads into the list. 
   MyString submitter_name;
   for (int ii = 0; ii < N_Owners; ++ii) {
      cad = new ClassAd();
      cad->ChainToAd(m_adBase);
      submitter_name.formatstr("%s@%s", Owners[ii].Name, UidDomain);
      cad->Assign(ATTR_NAME, submitter_name.Value());
      cad->Assign(ATTR_SUBMITTER_TAG,HOME_POOL_SUBMITTER_TAG);

      cad->Assign(ATTR_RUNNING_JOBS, Owners[ii].JobsRunning);
      cad->Assign(ATTR_IDLE_JOBS, Owners[ii].JobsIdle);
      cad->Assign(ATTR_WEIGHTED_RUNNING_JOBS, Owners[ii].WeightedJobsRunning);
      cad->Assign(ATTR_WEIGHTED_IDLE_JOBS, Owners[ii].WeightedJobsIdle);
      cad->Assign(ATTR_HELD_JOBS, Owners[ii].JobsHeld);
      cad->Assign(ATTR_FLOCKED_JOBS, Owners[ii].JobsFlocked);
      ads.Insert(cad);
   }

   return ads.Length();
}

// in support of condor_status -direct.  allow the query for SCHEDULER and SUBMITTER ads
//
int Scheduler::command_query_ads(int, Stream* stream) 
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

#if defined(ADD_TARGET_SCOPING)
	AddExplicitTargetRefs( queryAd );
#endif

		// Construct a list of all our ClassAds. we pass queryAd 
		// through so that if there is a STATISTICS_TO_PUBLISH attribute
		// it can be used to control the verbosity of statistics
    this->make_ad_list(ads, &queryAd);
	
		// Now, find the ClassAds that match.
	stream->encode();
	ads.Open();
	while( (ad = ads.Next()) ) {
		if( IsAHalfMatch( &queryAd, ad ) ) {
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


int 
clear_autocluster_id( ClassAd *job )
{
	job->Delete(ATTR_AUTO_CLUSTER_ID);
	return 0;
}

int
count( ClassAd *job )
{
	int		status;
	int		niceUser;
	MyString owner_buf;
	MyString owner_buf2;
	char const*	owner;
	MyString domain;
	int		cur_hosts;
	int		max_hosts;
	int		universe;

		// we may get passed a NULL job ad if, for instance, the job ad was
		// removed via condor_rm -f when some function didn't expect it.
		// So check for it here before continuing onward...
	if ( job == NULL ) {  
		return 0;
	}

	if (job->LookupInteger(ATTR_JOB_STATUS, status) == 0) {
		dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				ATTR_JOB_STATUS);
		return 0;
	}

	int noop = 0;
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
		universe = CONDOR_UNIVERSE_STANDARD;
	}

	int request_cpus = 0;
    if (job->LookupInteger(ATTR_REQUEST_CPUS, request_cpus) == 0) {
		request_cpus = 1;
	}
	
		// Just in case it is set funny
	if (request_cpus < 1) {
		request_cpus = 1;
	}
	

	// Sometimes we need the read username owner, not the accounting group
	MyString real_owner;
	if( ! job->LookupString(ATTR_OWNER,real_owner) ) {
		dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				ATTR_OWNER);
		return 0;
	}

	// calculate owner for per submittor information.
	job->LookupString(ATTR_ACCOUNTING_GROUP,owner_buf);	// TODDCORE
	if ( owner_buf.Length() == 0 ) {
		job->LookupString(ATTR_OWNER,owner_buf);
		if ( owner_buf.Length() == 0 ) {	
			dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
					ATTR_OWNER);
			return 0;
		}
	}
	owner = owner_buf.Value();

	// grab the domain too, if it exists
	job->LookupString(ATTR_NT_DOMAIN, domain);
	
	// With NiceUsers, the number of owners is
	// not the same as the number of submittors.  So, we first
	// check if this job is being submitted by a NiceUser, and
	// if so, insert it as a new entry in the "Owner" table
	if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
		owner_buf2.formatstr("%s.%s",NiceUserName,owner);
		owner=owner_buf2.Value();
	}

	// increment our count of the number of job ads in the queue
	scheduler.JobsTotalAds++;

	// build a list of other stats pools that match this job
	//
	time_t now = time(NULL);
	ScheddOtherStats * other_stats = NULL;
	if (scheduler.OtherPoolStats.AnyEnabled()) {
		other_stats = scheduler.OtherPoolStats.Matches(*job,now);
	}
	#define OTHER for (ScheddOtherStats * po = other_stats; po; po = po->next) (po->stats)

	// insert owner even if REMOVED or HELD for condor_q -{global|sub}
	// this function makes its own copies of the memory passed in 
	int OwnerNum = scheduler.insert_owner( owner );

	if ( (universe != CONDOR_UNIVERSE_GRID) &&	// handle Globus below...
		 (!service_this_universe(universe,job))  ) 
	{
			// Deal with all the Universes which we do not service, expect
			// for Globus, which we deal with below.
		if( universe == CONDOR_UNIVERSE_SCHEDULER ) 
		{
			// Count REMOVED or HELD jobs that are in the process of being
			// killed. cur_hosts tells us which these are.
			scheduler.SchedUniverseJobsRunning += cur_hosts;
			scheduler.SchedUniverseJobsIdle += (max_hosts - cur_hosts);
		}
		if( universe == CONDOR_UNIVERSE_LOCAL ) 
		{
			// Count REMOVED or HELD jobs that are in the process of being
			// killed. cur_hosts tells us which these are.
			scheduler.LocalUniverseJobsRunning += cur_hosts;
			scheduler.LocalUniverseJobsIdle += (max_hosts - cur_hosts);
		}
			// We want to record the cluster id of all idle MPI and parallel
		    // jobs

		int sendToDS = 0;
		job->LookupBool("WantParallelScheduling", sendToDS);
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
		// for Globus, count jobs in UNSUBMITTED state by owner.
		// later we make certain there is a grid manager daemon
		// per owner.
		int real_status = status;
		bool want_service = service_this_universe(universe,job);
		bool job_managed = jobExternallyManaged(job);
		bool job_managed_done = jobManagedDone(job);
		// if job is not already being managed : if we want matchmaking 
		// for this job, but we have not found a 
		// match yet, consider it "held" for purposes of the logic here.  we
		// have no need to tell the gridmanager to deal with it until we've
		// first found a match.
		if ( (job_managed == false) && (want_service && cur_hosts == 0) ) {
			status = HELD;
		}
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

		// Don't count HELD jobs that aren't externally (gridmanager) managed
		// Don't count jobs that the gridmanager has said it's completely
		// done with.
		UserIdentity userident(real_owner.Value(),domain.Value(),job);
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
			// If we do not need to do matchmaking on this job (i.e.
			// service this globus universe job), than we can bailout now.
		if (!want_service) {
			return 0;
		}
		status = real_status;	// set status back for below logic...
	}

	if (status == IDLE || status == RUNNING || status == TRANSFERRING_OUTPUT) {
		scheduler.JobsRunning += cur_hosts;
		scheduler.JobsIdle += (max_hosts - cur_hosts);

			// Update Owner array PrioSet iff knob USE_GLOBAL_JOB_PRIOS is true
			// and iff job is looking for more matches (max-hosts - cur_hosts)
		if ( param_boolean("USE_GLOBAL_JOB_PRIOS",false) &&
			 ((max_hosts - cur_hosts) > 0) )
		{
			int job_prio;
			if ( job->LookupInteger(ATTR_JOB_PRIO,job_prio) ) {
				scheduler.Owners[OwnerNum].PrioSet.insert( job_prio );
			}
		}
			// Update Owners array JobsIdle
		scheduler.Owners[OwnerNum].JobsIdle += (max_hosts - cur_hosts);
		scheduler.Owners[OwnerNum].WeightedJobsIdle += request_cpus * (max_hosts - cur_hosts);

			// Don't update scheduler.Owners[OwnerNum].JobsRunning here.
			// We do it in Scheduler::count_jobs().

		int job_image_size = 0;
		job->LookupInteger("ImageSize_RAW", job_image_size);
		scheduler.stats.JobsRunningSizes += (int64_t)job_image_size * 1024;
		OTHER.JobsRunningSizes += (int64_t)job_image_size * 1024;

		int job_start_date = 0;
		int job_running_time = 0;
		if (job->LookupInteger(ATTR_JOB_START_DATE, job_start_date))
			job_running_time = (now - job_start_date);
		scheduler.stats.JobsRunningRuntimes += job_running_time;
		OTHER.JobsRunningRuntimes += job_running_time;
	} else if (status == HELD) {
		scheduler.JobsHeld++;
		scheduler.Owners[OwnerNum].JobsHeld++;
	} else if (status == REMOVED) {
		scheduler.JobsRemoved++;
	}

	#undef OTHER
	return 0;
}

bool
service_this_universe(int universe, ClassAd* job)
{
	/*  If a non-grid job is externally managed, it's been grabbed by
		the schedd-on-the-side and we don't want to touch it.
	 */
	if ( universe != CONDOR_UNIVERSE_GRID && jobExternallyManaged( job ) ) {
		return false;
	}

	/* If WantMatching attribute exists, evaluate it to discover if we want
	   to "service" this universe or not.  BTW, "service" seems to really mean
	   find a matching resource or not.... 
	   Note: EvalBool returns 0 if evaluation is undefined or error, and
	   return 1 otherwise....
	*/
	int want_matching;
	if ( job->EvalBool(ATTR_WANT_MATCHING,NULL,want_matching) == 1 ) {
		if ( want_matching ) {
			return true;
		} else {
			return false;
		}
	}

	/* If we made it to here, the WantMatching was not defined.  So
	   figure out what to do based on Universe and other misc logic...
	*/
	switch (universe) {
		case CONDOR_UNIVERSE_GRID:
			{
				// If this Globus job is already being managed, then the schedd
				// should leave it alone... the gridmanager is dealing with it.
				if ( jobExternallyManaged(job) ) {
					return false;
				}			
				// Now if not managed, if GridResource has a "$$", then this
				// job is at least _matchable_, so return true, else false.
				MyString resource = "";
				job->LookupString( ATTR_GRID_RESOURCE, resource );
				if ( strstr( resource.Value(), "$$" ) ) {
					return true;
				}

				return false;
			}
			break;
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

			int sendToDS = 0;
			job->LookupBool("WantParallelScheduling", sendToDS);
			if (sendToDS) {
				return false;
			} else {
				return true;
			}
	}
}

int
Scheduler::insert_owner(char const* owner)
{
	int		i;
	for ( i=0; i<N_Owners; i++ ) {
		if( strcmp(Owners[i].Name,owner) == 0 ) {
			return i;
		}
	}

	Owners[i].Name = strdup( owner );

	N_Owners +=1;
	return i;
}


static bool IsSchedulerUniverse( shadow_rec* srec );
static bool IsLocalUniverse( shadow_rec* srec );

extern "C" {

void
abort_job_myself( PROC_ID job_id, JobAction action, bool log_hold,
				  bool notify )
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
			 "abort_job_myself: %d.%d action:%s log_hold:%s notify:%s\n", 
			 job_id.cluster, job_id.proc, getJobActionString(action),
			 log_hold ? "true" : "false",
			 notify ? "true" : "false" );

		// Note: job_ad should *NOT* be deallocated, so we don't need
		// to worry about deleting it before every return case, etc.
	ClassAd* job_ad = GetJobAd( job_id.cluster, job_id.proc );

	if ( !job_ad ) {
        dprintf ( D_ALWAYS, "tried to abort %d.%d; not found.\n", 
                  job_id.cluster, job_id.proc );
        return;
	}

	mode = -1;
	//job_ad->LookupInteger(ATTR_JOB_STATUS,mode);
	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_STATUS, &mode);
	if ( mode == -1 ) {
		EXCEPT("In abort_job_myself: %s attribute not found in job %d.%d\n",
				ATTR_JOB_STATUS,job_id.cluster, job_id.proc);
	}

	// Mark the job clean
	MarkJobClean(job_id);

	int job_universe = CONDOR_UNIVERSE_STANDARD;
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
			if( ! notify ) {
					// caller explicitly does not the gridmanager notified??
					// buyer had better beware, but we will honor what
					// we are told.  
					// nothing to do
				return;
			}
			MyString owner;
			MyString domain;
			job_ad->LookupString(ATTR_OWNER,owner);
			job_ad->LookupString(ATTR_NT_DOMAIN,domain);
			UserIdentity userident(owner.Value(),domain.Value(),job_ad);
			GridUniverseLogic::JobRemoved(userident.username().Value(),
					userident.domain().Value(),
					userident.auxid().Value(),
					scheduler.getGridUnparsedSelectionExpr(),
					0,0);
			return;
		}
	}

	if( (job_universe == CONDOR_UNIVERSE_MPI) || 
		(job_universe == CONDOR_UNIVERSE_PARALLEL) ) {
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
			if( ! notify ) {
					// nothing to do
				return;
			}
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
            
			if( ! notify ) {
					// nothing to do
				return;
			}

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
				shadow_sig = SIGTERM;
				shadow_sig_str = "SIGTERM";
				break;
			case JA_VACATE_FAST_JOBS:
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
            
			MyString owner;
			MyString domain;
			job_ad->LookupString(ATTR_OWNER,owner);
			job_ad->LookupString(ATTR_NT_DOMAIN,domain);
			if (! init_user_ids(owner.Value(), domain.Value()) ) {
				MyString msg;
				dprintf(D_ALWAYS, "init_user_ids() failed - putting job on "
					   "hold.\n");
#ifdef WIN32
				msg.formatstr("Bad or missing credential for user: %s", owner.Value());
#else
				msg.formatstr("Unable to switch to user: %s", owner.Value());
#endif
				holdJob(job_id.cluster, job_id.proc, msg.Value(), 
						CONDOR_HOLD_CODE_FailedToAccessUserAccount, 0,
					false, false, true, false, false);
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
					 srec->pid, owner.Value() );
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

} /* End of extern "C" */

/*
For a given job, determine if the schedd is the responsible
party for evaluating the job's periodic expressions.
The schedd is responsible if the job is scheduler
universe, globus universe with managed==false, or
any other universe when the job is idle or held.
*/

static int
ResponsibleForPeriodicExprs( ClassAd *jobad )
{
	int status=-1, univ=-1;
	PROC_ID jobid;

	jobad->LookupInteger(ATTR_JOB_STATUS,status);
	jobad->LookupInteger(ATTR_JOB_UNIVERSE,univ);
	bool managed = jobExternallyManaged(jobad);

	if ( managed ) {
		return 0;
	}

		// temporary for 7.2 only: avoid evaluating periodic
		// expressions when the job is on hold for spooling
	if( status == HELD ) {
		int hold_reason_code = -1;
		jobad->LookupInteger(ATTR_HOLD_REASON_CODE,hold_reason_code);
		if( hold_reason_code == CONDOR_HOLD_CODE_SpoolingInput ) {
			int cluster = -1, proc = -1;
			jobad->LookupInteger(ATTR_CLUSTER_ID, cluster);
			jobad->LookupInteger(ATTR_PROC_ID, proc);
			dprintf(D_FULLDEBUG,"Skipping periodic expressions for job %d.%d, because hold reason code is '%d'\n",cluster,proc,hold_reason_code);
			return 0;
		}
	}

	if( univ==CONDOR_UNIVERSE_SCHEDULER || univ==CONDOR_UNIVERSE_LOCAL ) {
		return 1;
	} else if(univ==CONDOR_UNIVERSE_GRID) {
		return 1;
	} else {
		switch(status) {
			case HELD:
			case IDLE:
			case COMPLETED:
				return 1;
			case REMOVED:
				jobid.cluster = -1;
				jobid.proc = -1;
				jobad->LookupInteger(ATTR_CLUSTER_ID,jobid.cluster);
				jobad->LookupInteger(ATTR_PROC_ID,jobid.proc);
				if ( jobid.cluster > 0 && jobid.proc > -1 && 
					 scheduler.FindSrecByProcID(jobid) )
				{
						// job removed, but shadow still exists
					return 0;
				} else {
						// job removed, and shadow is gone
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
PeriodicExprEval( ClassAd *jobad )
{
	int cluster=-1, proc=-1, status=-1, action=-1;

	if(!ResponsibleForPeriodicExprs(jobad)) return 1;

	jobad->LookupInteger(ATTR_CLUSTER_ID,cluster);
	jobad->LookupInteger(ATTR_PROC_ID,proc);
	jobad->LookupInteger(ATTR_JOB_STATUS,status);

	if(cluster<0 || proc<0 || status<0) return 1;

	UserPolicy policy;
	policy.Init(jobad);

	action = policy.AnalyzePolicy(PERIODIC_ONLY);

	// Build a "reason" string for logging
	MyString reason;
	int reason_code;
	int reason_subcode;
	policy.FiringReason(reason,reason_code,reason_subcode);
	if ( reason == "" ) {
		reason = "Unknown user policy expression";
	}

	switch(action) {
		case REMOVE_FROM_QUEUE:
			if(status!=REMOVED) {
				abortJob( cluster, proc, reason.Value(), true );
			}
			break;
		case HOLD_IN_QUEUE:
			if(status!=HELD) {
				holdJob(cluster, proc, reason.Value(),
						reason_code, reason_subcode,
						true, false, false, false, false);
			}
			break;
		case RELEASE_FROM_HOLD:
			if(status==HELD) {
				releaseJob(cluster, proc, reason.Value(), true);
			}
			break;
	}

	if ( status == COMPLETED || status == REMOVED ) {
		// Note: should also call DestroyProc on REMOVED, but 
		// that will screw up globus universe jobs until we fix
		// up confusion w/ MANAGED==True.  The issue is a job may be
		// removed; if the remove failed, it may be placed on hold
		// with managed==false.  If it is released again, we want the 
		// gridmanager to go at it again.....  
		// So for now, just call if status==COMPLETED -Todd <tannenba@cs.wisc.edu>
		if ( status == COMPLETED ) {
			DestroyProc(cluster,proc);
		}
		return 1;
	}

	return 1;
}

/*
For all of the jobs in the queue, evaluate the 
periodic user policy expressions.
*/

void
Scheduler::PeriodicExprHandler( void )
{
	PeriodicExprInterval.setStartTimeNow();

	WalkJobQueue(PeriodicExprEval);

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


bool
getSandbox( int cluster, int proc, MyString & path )
{
	char * sandbox = gen_ckpt_name(Spool, cluster, proc, 0);
	if( ! sandbox ) {
		free(sandbox); sandbox = NULL;
		return false;
	}
	path = sandbox;
	free(sandbox); sandbox = NULL;
	return true;
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
			scheduler.RemoveShadowRecFromMrec(srec);
			delete srec;
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
	int universe;
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
	RemoveShadowRecFromMrec( srec );
	delete srec;
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
	MyString iwd;
	MyString owner;
	BOOLEAN is_nfs;
	int want_flush = 0;

	job_ad->EvalBool( ATTR_JOB_IWD_FLUSH_NFS_CACHE, NULL, want_flush );
	if ( job_ad->LookupString( ATTR_OWNER, owner ) &&
		 job_ad->LookupString( ATTR_JOB_IWD, iwd ) &&
		 want_flush &&
		 fs_detect_nfs( iwd.Value(), &is_nfs ) == 0 && is_nfs ) {

		priv_state priv;

		dprintf( D_FULLDEBUG, "(%d.%d) Forcing NFS sync of Iwd\n", cluster,
				 proc );

			// We're not Windows, so we don't need the NT Domain
		if ( !init_user_ids( owner.Value(), NULL ) ) {
			dprintf( D_ALWAYS, "init_user_ids() failed for user %s!\n",
					 owner.Value() );
		} else {
			int sync_fd;
			MyString filename_template;
			char *sync_filename;

			priv = set_user_priv();

			filename_template.formatstr( "%s/.condor_nfs_sync_XXXXXX",
									   iwd.Value() );
			sync_filename = strdup( filename_template.Value() );
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
					 (int)time(NULL), NONDURABLE);
	return DestroyProc( cluster, proc );
}

namespace {
	void InitializeMask(WriteUserLog* ulog, int c, int p)
	{
		MyString msk;
		GetAttributeString(c, p, ATTR_DAGMAN_WORKFLOW_MASK, msk);
		dprintf( D_FULLDEBUG, "Mask is \"%s\"\n",msk.Value());
		Tokenize(msk.Value());
		while(const char* mask = GetNextToken(",",true)) {
			dprintf( D_FULLDEBUG, "Adding \"%s\" to mask\n",mask);
			ulog->AddToMask(ULogEventNumber(atoi(mask)));
		}
	}
}
// Initialize a WriteUserLog object for a given job and return a pointer to
// the WriteUserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a WriteUserLog, so you must check for NULL before
// using the pointer you get back.
WriteUserLog*
Scheduler::InitializeUserLog( PROC_ID job_id ) 
{
	MyString logfilename;
	MyString dagmanNodeLog;
	ClassAd *ad = GetJobAd(job_id.cluster,job_id.proc);
	std::vector<const char*> logfiles;
	if( getPathToUserLog(ad, logfilename) ) {
		logfiles.push_back(logfilename.Value());	
	}
	if( getPathToUserLog(ad, dagmanNodeLog, ATTR_DAGMAN_WORKFLOW_LOG) ) {			
		logfiles.push_back(dagmanNodeLog.Value());
	}
	if( logfiles.empty() ) {
			// if there is no userlog file defined, then our work is
			// done...  
		return NULL;
	}
	MyString owner;
	MyString domain;
	MyString iwd;
	MyString gjid;
	int use_xml;

	GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);
	GetAttributeString(job_id.cluster, job_id.proc, ATTR_NT_DOMAIN, domain);
	GetAttributeString(job_id.cluster, job_id.proc, ATTR_GLOBAL_JOB_ID, gjid);

	for(std::vector<const char*>::iterator p = logfiles.begin();
			p != logfiles.end(); ++p) {
		dprintf( D_FULLDEBUG, "Writing record to user logfile=%s owner=%s\n",
			*p, owner.Value() );
	}

	WriteUserLog* ULog=new WriteUserLog();
	ULog->setUseXML(0 <= GetAttributeBool(job_id.cluster, job_id.proc,
		ATTR_ULOG_USE_XML, &use_xml) && 1 == use_xml);
	ULog->setCreatorName( Name );
	if (ULog->initialize(owner.Value(), domain.Value(), logfiles,
			job_id.cluster, job_id.proc, 0, gjid.Value())) {
		if(logfiles.size() > 1) {
			InitializeMask(ULog,job_id.cluster, job_id.proc);
		}
		return ULog;
	} else {
			// If the user log is in the spool directory, try writing to
			// it as user condor. The spool directory spends some of its
			// time owned by condor.
		char *tmp = gen_ckpt_name( Spool, job_id.cluster, job_id.proc, 0 );
		std::string SpoolDir(tmp);
		SpoolDir += DIR_DELIM_CHAR;
		free( tmp );
		if ( !strncmp( SpoolDir.c_str(), logfilename.Value(),
					SpoolDir.length() ) && ULog->initialize( logfiles,
					job_id.cluster, job_id.proc, 0, gjid.Value() ) ) {
			if(logfiles.size() > 1) {
				InitializeMask(ULog,job_id.cluster,job_id.proc);
			}
			return ULog;
		}
		for(std::vector<const char*>::iterator p = logfiles.begin();
				p != logfiles.end(); ++p) {
			dprintf ( D_ALWAYS, "WARNING: Invalid user log file specified: "
				"%s\n", *p);
		}
		delete ULog;
		return NULL;
	}
}

bool
Scheduler::WriteSubmitToUserLog( PROC_ID job_id, bool do_fsync )
{
	std::string submitUserNotes, submitEventNotes;

		// Skip writing submit events for procid != 0 for parallel jobs
	int universe = -1;
	GetAttributeInt( job_id.cluster, job_id.proc, ATTR_JOB_UNIVERSE, &universe );
	if ( universe == CONDOR_UNIVERSE_PARALLEL ) {
		if ( job_id.proc > 0) {
			return true;;
		}
	}

	WriteUserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	SubmitEvent event;
	ClassAd *job_ad = GetJobAd(job_id.cluster,job_id.proc);

	event.setSubmitHost( daemonCore->privateNetworkIpAddr() );
	if ( job_ad->LookupString(ATTR_SUBMIT_EVENT_NOTES, submitEventNotes) ) {
		event.submitEventLogNotes = strnewp(submitEventNotes.c_str());
	}
	if ( job_ad->LookupString(ATTR_SUBMIT_EVENT_USER_NOTES, submitUserNotes) ) {
		event.submitEventUserNotes = strnewp(submitUserNotes.c_str());
	}

	ULog->setEnableFsync(do_fsync);
	bool status = ULog->writeEvent(&event, job_ad);
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS,
				 "Unable to log ULOG_SUBMIT event for job %d.%d\n",
				 job_id.cluster, job_id.proc );
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

	char* reason = NULL;
	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  ATTR_REMOVE_REASON, &reason) >= 0 ) {
		event.setReason( reason );
		free( reason );
	}

	bool status =
		ULog->writeEvent(&event, GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

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

	char* reason = NULL;
	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  ATTR_HOLD_REASON, &reason) >= 0 ) {
		event.setReason( reason );
		free( reason );
	} else {
		dprintf( D_ALWAYS, "Scheduler::WriteHoldToUserLog(): "
				 "Failed to get %s from job %d.%d\n", ATTR_HOLD_REASON,
				 job_id.cluster, job_id.proc );
	}

	int hold_reason_code;
	if( GetAttributeInt(job_id.cluster, job_id.proc,
	                    ATTR_HOLD_REASON_CODE, &hold_reason_code) >= 0 )
	{
		event.setReasonCode(hold_reason_code);
	}

	int hold_reason_subcode;
	if( GetAttributeInt(job_id.cluster, job_id.proc,
	                    ATTR_HOLD_REASON_SUBCODE, &hold_reason_subcode)	>= 0 )
	{
		event.setReasonSubCode(hold_reason_subcode);
	}

	bool status =
		ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;

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

	char* reason = NULL;
	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  ATTR_RELEASE_REASON, &reason) >= 0 ) {
		event.setReason( reason );
		free( reason );
	}

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
	bool status =
		ULog->writeEvent(&event,GetJobAd(job_id.cluster,job_id.proc));
	delete ULog;
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
	if(reason) {
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
	StrToProcId(job_id_str, job_id);
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


int
Scheduler::transferJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs = NULL;
	int i;

	dprintf(D_FULLDEBUG,"transferJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,
			"ERROR - transferJobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (WIFSIGNALED(exit_status) || (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == FALSE)) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		delete jobs;
		return FALSE;
	}

		// For each job, modify its ClassAd
	time_t now = time(NULL);
	int len = (*jobs).getlast() + 1;
	for (i=0; i < len; i++) {
			// TODO --- maybe put this in a transaction?
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,ATTR_STAGE_OUT_FINISH,now);
	}

		// Now, deallocate memory
	spoolJobFileWorkers->remove(tid);
	delete jobs;
	return TRUE;
}

int
Scheduler::spoolJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs = NULL;

	dprintf(D_FULLDEBUG,"spoolJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

	time_t now = time(NULL);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,"ERROR - JobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (WIFSIGNALED(exit_status) || (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == FALSE)) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		int len = (*jobs).getlast() + 1;
		for(int jobIndex = 0; jobIndex < len; ++jobIndex) {
			int cluster = (*jobs)[jobIndex].cluster;
			int proc = (*jobs)[jobIndex].proc;
			abortJob( cluster, proc, "Staging of job files failed", true);
		}
		delete jobs;
		return FALSE;
	}


	int jobIndex,cluster,proc;
	int len = (*jobs).getlast() + 1;

		// For each job, modify its ClassAd
	for (jobIndex = 0; jobIndex < len; jobIndex++) {
		cluster = (*jobs)[jobIndex].cluster;
		proc = (*jobs)[jobIndex].proc;

		BeginTransaction();

			// Set ATTR_STAGE_IN_FINISH if not already set.
		int spool_completion_time = 0;
		GetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,&spool_completion_time);
		if ( !spool_completion_time ) {
			// The transfer thread specifically slept for 1 second
			// to ensure that the job can't possibly start (and finish)
			// prior to the timestamps on the file.  Unfortunately,
			// we note the transfer finish time _here_.  So we've got 
			// to back off 1 second.
			SetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,now - 1);
		}

			// And now release the job.
		releaseJob(cluster,proc,"Data files spooled",false,false,false,false);
		CommitTransaction();
	}

	daemonCore->Register_Timer( 0, 
						(TimerHandlercpp)&Scheduler::reschedule_negotiator_timer,
						"Scheduler::reschedule_negotiator", this );

	spoolJobFileWorkers->remove(tid);
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
	ExtArray<PROC_ID> *jobs = ((job_data_transfer_t *)arg)->jobs;
	char *peer_version = ((job_data_transfer_t *)arg)->peer_version;
	int mode = ((job_data_transfer_t *)arg)->mode;
	int result;
	int old_timeout;
	int cluster, proc;
	
	/* Setup a large timeout; when lots of jobs are being submitted w/ 
	 * large sandboxes, the default is WAY to small...
	 */
	old_timeout = s->timeout(60 * 60 * 8);  

	JobAdsArrayLen = jobs->getlast() + 1;
//	dprintf(D_FULLDEBUG,"TODD spoolJobFilesWorkerThread: JobAdsArrayLen=%d\n",JobAdsArrayLen);
	if ( mode == TRANSFER_DATA || mode == TRANSFER_DATA_WITH_PERMS ) {
		// if sending sandboxes, first tell the client how many
		// we are about to send.
		dprintf(D_FULLDEBUG, "Scheduler::generalJobFilesWorkerThread: "
			"TRANSFER_DATA/WITH_PERMS: %d jobs to be sent\n", JobAdsArrayLen);
		rsock->encode();
		if ( !rsock->code(JobAdsArrayLen) || !rsock->end_of_message() ) {
			dprintf( D_AUDIT | D_FAILURE, *rsock, "generalJobFilesWorkerThread(): "
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
			dprintf( D_AUDIT | D_FAILURE, *rsock, "generalJobFilesWorkerThread(): "
					 "job ad %d.%d not found\n",cluster,proc );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

		dprintf(D_ALWAYS, "The submitting job ad as the FileTransferObject sees it\n");
		dPrintAd(D_ALWAYS, *ad);

			// Create a file transfer object, with schedd as the server.
			// If we're receiving files, don't create a file catalog in
			// the FileTransfer object. The submitter's IWD is probably not
			// valid on this machine and we won't use the catalog anyway.
		result = ftrans.SimpleInit(ad, true, true, rsock, PRIV_UNKNOWN,
								   (mode == TRANSFER_DATA ||
									mode == TRANSFER_DATA_WITH_PERMS));
		if ( !result ) {
			dprintf( D_AUDIT | D_FAILURE, *rsock, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
		if ( peer_version != NULL ) {
			ftrans.setPeerVersion( peer_version );
		}

			// Send or receive files as needed
		if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
			// receive sandbox into the schedd
			result = ftrans.DownloadFiles();

			if ( result ) {
				AuditLogJobProxy( *rsock, ad );
			}
		} else {
			// send sandbox out of the schedd
			rsock->encode();
			// first send the classad for the job
			result = putClassAd(rsock, *ad);
			if (!result) {
				dprintf(D_AUDIT | D_FAILURE, *rsock, "generalJobFilesWorkerThread(): "
					"failed to send job ad for job %d.%d \n",
					cluster,proc );
			} else {
				rsock->end_of_message();
				// and then upload the files
				result = ftrans.UploadFiles();
			}
		}

		if ( !result ) {
			dprintf( D_AUDIT | D_FAILURE, *rsock, "generalJobFilesWorkerThread(): "
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
	rsock->code(answer);
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

			int universe = CONDOR_UNIVERSE_STANDARD;
			ad->LookupInteger(ATTR_JOB_UNIVERSE, universe);
			FreeJobAd(ad);

			if(universe == CONDOR_UNIVERSE_GRID) {
				aboutToSpawnJobHandler( cluster, proc, NULL );
			}
		}
	}

	if ( peer_version ) {
		free( peer_version );
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
	ExtArray<PROC_ID> *jobs = NULL;
	char *constraint_string = NULL;
	int i;
	static int spool_reaper_id = -1;
	static int transfer_reaper_id = -1;
	PROC_ID a_job;
	int tid;
	char *peer_version = NULL;
	std::ostringstream job_ids;
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
			dprintf( D_AUDIT | D_FAILURE, *rsock, "spoolJobFiles() aborting: %s\n",
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}	


	rsock->decode();

	switch(mode) {
		case SPOOL_JOB_FILES_WITH_PERMS: // uploading perm files to schedd
		case TRANSFER_DATA_WITH_PERMS:	// downloading perm files from schedd
			peer_version = NULL;
			if ( !rsock->code(peer_version) ) {
				dprintf(D_AUDIT | D_FAILURE, *rsock, 
					 	"spoolJobFiles(): failed to read peer_version\n" );
				refuse(s);
				return FALSE;
			}
				// At this point, we are responsible for deallocating
				// peer_version with free()
			break;

		default:
			// Non perm commands don't encode a peer version string
			break;
	}


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
				    dprintf( D_AUDIT | D_FAILURE, *rsock, "spoolJobFiles(): "
							 "failed to read JobAdsArrayLen (%d)\n",
							 JobAdsArrayLen );
					refuse(s);
					return FALSE;
			}
			if ( JobAdsArrayLen <= 0 ) {
				dprintf( D_AUDIT | D_FAILURE, *rsock, "spoolJobFiles(): "
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
					dprintf( D_AUDIT | D_FAILURE, *rsock, "spoolJobFiles(): "
						 	"failed to read constraint string\n" );
					refuse(s);
					return FALSE;
			}
			break;

		default:
			break;
	}

	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	setQSock(rsock);	// so OwnerCheck() will work

	time_t now = time(NULL);
	dprintf( D_FULLDEBUG, "Looking at spooling: mode is %d\n", mode);
	switch(mode) {
		// uploading files to schedd 
		case SPOOL_JOB_FILES:
		case SPOOL_JOB_FILES_WITH_PERMS:
			for (i=0; i<JobAdsArrayLen; i++) {
				rsock->code(a_job);
					// Only add jobs to our list if the caller has permission 
					// to do so.
					// cuz only the owner of a job (or queue super user) 
					// is allowed to transfer data to/from a job.
				if (OwnerCheck(a_job.cluster,a_job.proc)) {
					(*jobs)[i] = a_job;
					job_ids << a_job.cluster << "." << a_job.proc << ", ";

						// Must not allow stagein to happen more than
						// once, because this could screw up
						// subsequent operations, such as rewriting of
						// paths in the ClassAd and the job being in
						// the middle of using the files.
					int finish_time;
					if( GetAttributeInt(a_job.cluster,a_job.proc,
					    ATTR_STAGE_IN_FINISH,&finish_time) >= 0 ) {
						dprintf( D_AUDIT | D_FAILURE, *rsock, "spoolJobFiles(): cannot allow"
						         " stagein for job %d.%d, because stagein"
						         " already finished for this job.\n",
						         a_job.cluster, a_job.proc);
						unsetQSock();
						return FALSE;
					}
					int holdcode;
					int job_status;
					int job_status_result = GetAttributeInt(a_job.cluster,
						a_job.proc,ATTR_JOB_STATUS,&job_status);
					if( job_status_result >= 0 &&
							GetAttributeInt(a_job.cluster,a_job.proc,
							ATTR_HOLD_REASON_CODE,&holdcode) >= 0) {
						dprintf( D_FULLDEBUG, "job_status is %d\n", job_status);
						if(job_status == HELD &&
								holdcode != CONDOR_HOLD_CODE_SpoolingInput) {
							dprintf( D_AUDIT | D_FAILURE, *rsock, "Job %d.%d is not in hold state for "
								"spooling. Do not allow stagein\n",
								a_job.cluster, a_job.proc);
							unsetQSock();
							return FALSE;
						}
					}
					SetAttributeInt(a_job.cluster,a_job.proc,
									ATTR_STAGE_IN_START,now);
				}
			}
			break;

		// downloading files from schedd
		case TRANSFER_DATA:
		case TRANSFER_DATA_WITH_PERMS:
			{
			ClassAd * tmp_ad = GetNextJobByConstraint(constraint_string,1);
			JobAdsArrayLen = 0;
			while (tmp_ad) {
				if ( tmp_ad->LookupInteger(ATTR_CLUSTER_ID,a_job.cluster) &&
				 	tmp_ad->LookupInteger(ATTR_PROC_ID,a_job.proc) &&
				 	OwnerCheck(a_job.cluster, a_job.proc) )
				{
					(*jobs)[JobAdsArrayLen++] = a_job;
					job_ids << a_job.cluster << "." << a_job.proc << ", ";
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

	unsetQSock();

	rsock->end_of_message();

	job_ids_string = job_ids.str();
	job_ids_string.erase(job_ids_string.length()-2,2); //Get rid of the extraneous ", "
	dprintf( D_AUDIT, *rsock, "Transferring files for jobs %s\n", 
			 job_ids_string.c_str());

		// DaemonCore will free the thread_arg for us when the thread
		// exits, but we need to free anything pointed to by
		// job_data_transfer_t ourselves. generalJobFilesWorkerThread()
		// will free 'peer_version' and our reaper will free 'jobs' (the
		// reaper needs 'jobs' for some of its work).
	job_data_transfer_t *thread_arg = (job_data_transfer_t *)malloc( sizeof(job_data_transfer_t) );
	ASSERT( thread_arg != NULL );
	thread_arg->mode = mode;
	thread_arg->peer_version = peer_version;
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
		if ( peer_version ) {
			free( peer_version );
		}
		delete jobs;
		refuse(s);
		return FALSE;
	}

		// Place this tid into a hashtable so our reaper can finish up.
	spoolJobFileWorkers->insert(tid, jobs);
	
	dprintf( D_AUDIT, *rsock, "spoolJobFiles(): started worker process\n");

	return TRUE;
}

int
Scheduler::updateGSICred(int cmd, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	PROC_ID jobid;
	ClassAd *jobad;
	int reply;

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
			dprintf( D_AUDIT | D_FAILURE, *rsock, "updateGSICred(%d) aborting: %s\n", cmd,
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}	


		// read the job id from the client
	rsock->decode();
	if ( !rsock->code(jobid) || !rsock->end_of_message() ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock, "updateGSICred(%d): "
					 "failed to read job id\n", cmd );
			refuse(s);
			return FALSE;
	}
		// TO DO: Add job proxy info
	dprintf( D_AUDIT | D_FULLDEBUG, *rsock,"updateGSICred(%d): read job id %d.%d\n",
		cmd,jobid.cluster,jobid.proc);
	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if ( !jobad ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock, "updateGSICred(%d): failed, "
				 "job %d.%d not found\n", cmd, jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}

		// Make certain this user is authorized to do this,
		// cuz only the owner of a job (or queue super user) is allowed
		// to transfer data to/from a job.
	bool authorized = false;
	setQSock(rsock);	// so OwnerCheck() will work
	if (OwnerCheck(jobid.cluster,jobid.proc)) {
		authorized = true;
	}
	unsetQSock();
	if ( !authorized ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock, "updateGSICred(%d): failed, "
				 "user %s not authorized to edit job %d.%d\n", cmd,
				 rsock->getFullyQualifiedUser(),jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}

		// Make certain this job has a x509 proxy, and that this 
		// proxy is sitting in the SPOOL directory
	char* SpoolSpace = gen_ckpt_name(Spool,jobid.cluster,jobid.proc,0);
	ASSERT(SpoolSpace);
	char *proxy_path = NULL;
	jobad->LookupString(ATTR_X509_USER_PROXY,&proxy_path);
	if( proxy_path && is_relative_to_cwd(proxy_path) ) {
		MyString iwd;
		if( jobad->LookupString(ATTR_JOB_IWD,iwd) ) {
			iwd.formatstr_cat("%c%s",DIR_DELIM_CHAR,proxy_path);
			free(proxy_path);
			proxy_path = strdup(iwd.Value());
		}
	}
	if ( !proxy_path || strncmp(SpoolSpace,proxy_path,strlen(SpoolSpace)) ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock, "updateGSICred(%d): failed, "
			 "job %d.%d does not contain a gsi credential in SPOOL\n", 
			 cmd, jobid.cluster, jobid.proc );
		refuse(s);
		free(SpoolSpace);
		if (proxy_path) free(proxy_path);
		return FALSE;
	}
	free(SpoolSpace);
	MyString final_proxy_path(proxy_path);
	MyString temp_proxy_path(final_proxy_path);
	temp_proxy_path += ".tmp";
	free(proxy_path);

#ifndef WIN32
		// Check the ownership of the proxy and switch our priv state
		// if needed
	StatInfo si( final_proxy_path.Value() );
	if ( si.Error() == SINoFile ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock, "updateGSICred(%d): failed, "
			 "job %d.%d's proxy doesn't exist\n", 
			 cmd, jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}
	uid_t proxy_uid = si.GetOwner();
	passwd_cache *p_cache = pcache();
	uid_t job_uid;
	char *job_owner = NULL;
	jobad->LookupString( ATTR_OWNER, &job_owner );
	if ( !job_owner ) {
			// Maybe change EXCEPT to print to the audit log with D_AUDIT
		EXCEPT( "No %s for job %d.%d!", ATTR_OWNER, jobid.cluster,
				jobid.proc );
	}
	if ( !p_cache->get_user_uid( job_owner, job_uid ) ) {
			// Failed to find uid for this owner, badness.
		dprintf( D_AUDIT | D_FAILURE, *rsock, "Failed to find uid for user %s (job %d.%d)\n",
				 job_owner, jobid.cluster, jobid.proc );
		free( job_owner );
		refuse(s);
		return FALSE;
	}
		// If the uids match, then we need to switch to user priv to
		// access the proxy file.
	priv_state priv;
	if ( proxy_uid == job_uid ) {
			// We're not Windows here, so we don't need the NT Domain
		if ( !init_user_ids( job_owner, NULL ) ) {
			dprintf( D_AUDIT | D_FAILURE, *rsock, "init_user_ids() failed for user %s!\n",
					 job_owner );
			free( job_owner );
			refuse(s);
			return FALSE;
		}
		priv = set_user_priv();
	} else {
			// We should already be in condor priv, but we want to save it
			// in the 'priv' variable.
		priv = set_condor_priv();
	}
	free( job_owner );
	job_owner = NULL;
#endif

		// Decode the proxy off the wire, and store into the
		// file temp_proxy_path, which is known to be in the SPOOL dir
	rsock->decode();
	filesize_t size = 0;
	int rc;
	if ( cmd == UPDATE_GSI_CRED ) {
		rc = rsock->get_file(&size,temp_proxy_path.Value());
	} else if ( cmd == DELEGATE_GSI_CRED_SCHEDD ) {
		rc = rsock->get_x509_delegation(&size,temp_proxy_path.Value());
	} else {
		dprintf( D_ALWAYS, "updateGSICred(%d): unknown CEDAR command %d\n",
				 cmd, cmd );
		rc = -1;
	}
	if ( rc < 0 ) {
			// transfer failed
		reply = 0;	// reply of 0 means failure
	} else {
			// transfer worked, now rename the file to final_proxy_path
		if ( rotate_file(temp_proxy_path.Value(),
						 final_proxy_path.Value()) < 0 ) 
		{
				// the rename failed!!?!?!
			dprintf( D_ALWAYS, "updateGSICred(%d): failed, "
				 "job %d.%d  - could not rename file\n",
				 cmd, jobid.cluster,jobid.proc);
			reply = 0;
		} else {
			reply = 1;	// reply of 1 means success

			AuditLogJobProxy( *rsock, jobid, final_proxy_path.Value() );
		}
	}

#ifndef WIN32
		// Now switch back to our old priv state
	set_priv( priv );

	uninit_user_ids();
#endif

		// Send our reply back to the client
	rsock->encode();
	rsock->code(reply);
	rsock->end_of_message();

	dprintf( D_AUDIT | D_ALWAYS, *rsock,"Refresh GSI cred for job %d.%d %s\n",
		jobid.cluster,jobid.proc,reply ? "suceeded" : "failed");
	
	return TRUE;
}


int
Scheduler::actOnJobs(int, Stream* s)
{
	ClassAd command_ad;
	int action_num = -1;
	JobAction action = JA_ERROR;
	int reply, i;
	int num_matches = 0;
	int new_status = -1;
	char buf[256];
	char *reason = NULL;
	const char *reason_attr_name = NULL;
	ReliSock* rsock = (ReliSock*)s;
	bool notify = true;
	bool needs_transaction = true;
	action_result_type_t result_type = AR_TOTALS;
	int hold_reason_subcode = 0;

		// Setup array to hold ids of the jobs we're acting on.
	ExtArray<PROC_ID> jobs;
	PROC_ID tmp_id;
	tmp_id.cluster = -1;
	tmp_id.proc = -1;
	jobs.setFiller( tmp_id );

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
			dprintf( D_AUDIT | D_FAILURE, *rsock, "actOnJobs() aborting: %s\n",
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}

		// read the command ClassAd + EOM
	if( ! (getClassAd(rsock, command_ad) && rsock->end_of_message()) ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock, "Can't read command ad from tool\n" );
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

		   In addition, it might also include:
		   ATTR_NOTIFY_JOB_SCHEDULER (true or false)
		   and one of: ATTR_REMOVE_REASON, ATTR_RELEASE_REASON, or
		               ATTR_HOLD_REASON

		   It may optionally contain ATTR_HOLD_REASON_SUBCODE.
		*/
	if( ! command_ad.LookupInteger(ATTR_JOB_ACTION, action_num) ) {
		dprintf( D_AUDIT | D_FAILURE, *rsock,
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
		dprintf( D_AUDIT | D_FAILURE, *rsock, "actOnJobs(): ClassAd contains invalid "
				 "%s (%d), aborting\n", ATTR_JOB_ACTION, action_num );
		refuse( s );
		return FALSE;
	}
		// Grab the reason string if the command ad gave it to us
	char *tmp = NULL;
	const char *owner;
	if( reason_attr_name ) {
		command_ad.LookupString( reason_attr_name, &tmp );
	}
	if( tmp ) {
			// patch up the reason they gave us to include who did
			// it. 
		owner = rsock->getOwner();
		int size = strlen(tmp) + strlen(owner) + 14;
		reason = (char*)malloc( size * sizeof(char) );
		if( ! reason ) {
				// Maybe change EXCEPT to print to the audit log with D_AUDIT
			EXCEPT( "Out of memory!" );
		}
		sprintf( reason, "\"%s (by user %s)\"", tmp, owner );
		free( tmp );
		tmp = NULL;
	}

	if( action == JA_HOLD_JOBS ) {
		command_ad.LookupInteger(ATTR_HOLD_REASON_SUBCODE,hold_reason_subcode);
	}

	int foo;
	if( ! command_ad.LookupBool(ATTR_NOTIFY_JOB_SCHEDULER, foo) ) {
		notify = true;
	} else {
		notify = (bool) foo;
	}

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
	StringList job_ids;
		// NOTE: ATTR_ACTION_CONSTRAINT needs to be treated as a bool,
		// not as a string...
	ExprTree *tree;
	tree = command_ad.LookupExpr(ATTR_ACTION_CONSTRAINT);
	if( tree ) {
		const char *value = ExprTreeToString( tree );
		if( ! value ) {
				// TODO: deal with this kind of error
			free(reason);
			return false;
		}

			// we want to tack on another clause to make sure we're
			// not doing something invalid
		switch( action ) {
		case JA_REMOVE_JOBS:
				// Don't remove removed jobs
			snprintf( buf, 256, "(%s!=%d) && (", ATTR_JOB_STATUS, REMOVED );
			break;
		case JA_REMOVE_X_JOBS:
				// only allow forced removal of previously "removed" jobs
				// including jobs on hold that will go to the removed state
				// upon release.
			snprintf( buf, 256, "((%s==%d) || (%s==%d && %s=?=%d)) && (", 
				ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, HELD,
				ATTR_JOB_STATUS_ON_RELEASE,REMOVED);
			break;
		case JA_HOLD_JOBS:
				// Don't hold held jobs
			snprintf( buf, 256, "(%s!=%d) && (", ATTR_JOB_STATUS, HELD );
			break;
		case JA_RELEASE_JOBS:
				// Only release held jobs which aren't waiting for
				// input files to be spooled
			snprintf( buf, 256, "(%s==%d && %s=!=%d) && (", ATTR_JOB_STATUS,
					  HELD, ATTR_HOLD_REASON_CODE,
					  CONDOR_HOLD_CODE_SpoolingInput );
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
	tmp = NULL;
	if( command_ad.LookupString(ATTR_ACTION_IDS, &tmp) ) {
		if( constraint ) {
			dprintf( D_AUDIT | D_FAILURE, *rsock, "actOnJobs(): "
					 "ClassAd has both %s and %s, aborting\n",
					 ATTR_ACTION_CONSTRAINT, ATTR_ACTION_IDS );
			refuse( s );
			free( tmp );
			free( constraint );
			if( reason ) { free( reason ); }
			return FALSE;
		}
		job_ids.initializeFromString( tmp );
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
		tmp = job_ids.print_to_string();
		if ( tmp ) {
			job_ids_string = tmp;
			free( tmp ); 
			tmp = NULL;
		}
		dprintf( D_AUDIT, *rsock, "%s jobs %s\n",
				 getJobActionString(action), job_ids_string.c_str());
	}		

		// // // // //
		// REAL WORK
		// // // // //
	
	int now = (int)time(0);

	JobActionResults results( result_type );

		// Set the Q_SOCK so that qmgmt will perform checking on the
		// classads it's touching to enforce the owner...
	setQSock( rsock );

		// begin a transaction for qmgmt operations
	if( needs_transaction ) { 
		BeginTransaction();
	}

		// process the jobs to set status (and optionally, reason) 
	if( constraint ) {

			// SetAttributeByConstraint is clumsy and doesn't really
			// do what we want.  Instead, we'll just iterate through
			// the Q ourselves so we know exactly what jobs we hit. 

		ClassAd* (*GetNextJobFunc) (const char *, int);
		ClassAd* job_ad;
		if( action == JA_CLEAR_DIRTY_JOB_ATTRS )
		{
			GetNextJobFunc = &GetNextDirtyJobByConstraint;
		}
		else
		{
			GetNextJobFunc = &GetNextJobByConstraint;
		}
		for( job_ad = (*GetNextJobFunc)( constraint, 1 );
		     job_ad;
		     job_ad = (*GetNextJobFunc)( constraint, 0 ))
		{
			if(	job_ad->LookupInteger(ATTR_CLUSTER_ID,tmp_id.cluster) &&
				job_ad->LookupInteger(ATTR_PROC_ID,tmp_id.proc) ) 
			{
				jobs[num_matches++] = tmp_id;
			} 
		}
		free( constraint );
		constraint = NULL;

	} else {

			// No need to iterate through the queue, just act on the
			// specific ids we care about...

		StringList expanded_ids;
		expand_mpi_procs(&job_ids, &expanded_ids);
		expanded_ids.rewind();
		while( (tmp=expanded_ids.next()) ) {
			tmp_id = getProcByString( tmp );
			if( tmp_id.cluster < 0 || tmp_id.proc < 0 ) {
				continue;
			}
			jobs[num_matches++] = tmp_id;
		}
	}

	int num_success = 0;
	for( i=0; i<num_matches; i++ ) {
		tmp_id = jobs[i];

			// Check to make sure the job's status makes sense for
			// the command we're trying to perform
		int status;
		int on_release_status = IDLE;
		int hold_reason_code = -1;
		if( GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
							ATTR_JOB_STATUS, &status) < 0 ) {
			results.record( tmp_id, AR_NOT_FOUND );
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
			if( status != HELD || hold_reason_code == CONDOR_HOLD_CODE_SpoolingInput ) {
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
				results.record( tmp_id, AR_ALREADY_DONE );
				jobs[i].cluster = -1;
				continue;
			}
			if ( status == REMOVED &&
				 SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								  ATTR_JOB_STATUS_ON_RELEASE,
								  status ) < 0 )
			{
				results.record( tmp_id, AR_PERMISSION_DENIED );
				jobs[i].cluster = -1;
				continue;
			}
			if( SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								 ATTR_HOLD_REASON_CODE,
								 CONDOR_HOLD_CODE_UserRequest ) < 0 ) {
				results.record( tmp_id, AR_PERMISSION_DENIED );
				jobs[i].cluster = -1;
				continue;
			}
			if( SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								 ATTR_HOLD_REASON_SUBCODE,
								 hold_reason_subcode ) < 0 )
			{
				results.record( tmp_id, AR_PERMISSION_DENIED );
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
		if( action == JA_VACATE_JOBS || 
			action == JA_VACATE_FAST_JOBS || 
		    action == JA_SUSPEND_JOBS || 
		    action == JA_CONTINUE_JOBS ) {
				// vacate is a special case, since we're not
				// trying to modify the job in the queue at
				// all, so we just need to make sure we're
				// authorized to vacate this job, and if so,
				// record that we found this job_id and we're
				// done.
			ClassAd *cad = GetJobAd( tmp_id.cluster, tmp_id.proc, false );
			if( ! cad ) {
					// Maybe change EXCEPT to print to the audit log with D_AUDIT
				EXCEPT( "impossible: GetJobAd(%d.%d) returned false "
						"yet GetAttributeInt(%s) returned success",
						tmp_id.cluster, tmp_id.proc, ATTR_JOB_STATUS );
			}
			if( !OwnerCheck(cad, rsock->getOwner()) ) {
				results.record( tmp_id, AR_PERMISSION_DENIED );
				jobs[i].cluster = -1;
				continue;
			}
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
		if( reason ) {
			SetAttribute( tmp_id.cluster, tmp_id.proc,
						  reason_attr_name, reason );
				// TODO: deal w/ failure here, too?
		}
		SetAttributeInt( tmp_id.cluster, tmp_id.proc,
						 ATTR_ENTERED_CURRENT_STATUS, now );
		fixReasonAttrs( tmp_id, action );
		results.record( tmp_id, AR_SUCCESS );
		num_success++;
	}

	if( reason ) { free( reason ); }

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
			 isQueueSuperUser(rsock->getOwner()) ? true : false );
	
	rsock->encode();
	if( ! (putClassAd(rsock, *response_ad) && rsock->end_of_message()) ) {
			// Failed to send reply, the client might be dead, so
			// abort our transaction.
		dprintf( D_AUDIT | D_FAILURE, *rsock,
				 "actOnJobs: couldn't send results to client: aborting\n" );
		if( needs_transaction ) {
			AbortTransaction();
		}
		unsetQSock();
		return FALSE;
	}

	if( num_success == 0 ) {
			// We didn't do anything, so we want to bail out now...
		dprintf( D_AUDIT | D_FAILURE | D_FULLDEBUG, *rsock, 
				 "actOnJobs: didn't do any work, aborting\n" );
		if( needs_transaction ) {
			AbortTransaction();
		}
		unsetQSock();
		return FALSE;
	}

		// If we told them it's good, try to read the reply to make
		// sure the tool is still there and happy...
	rsock->decode();
	if( ! (rsock->code(reply) && rsock->end_of_message() && reply == OK) ) {
			// we couldn't get the reply, or they told us to bail
		dprintf( D_AUDIT | D_FAILURE, *rsock, "actOnJobs: client not responding: aborting\n" );
		if( needs_transaction ) {
			AbortTransaction();
		}
		unsetQSock();
		return FALSE;
	}

		// We're seeing sporadic test suite failures where this
		// CommitTransaction() appears to take a long time to
		// execute. This dprintf() will help in debugging.
	time_t before = time(NULL);
	if( needs_transaction ) {
		CommitTransaction();
	}
	time_t after = time(NULL);
	if ( (after - before) > 5 ) {
		dprintf( D_FULLDEBUG, "actOnJobs(): CommitTransaction() took %ld seconds to run\n", after - before );
	}
		
	unsetQSock();

		// If we got this far, we can tell the tool we're happy,
		// since if that CommitTransaction failed, we'd EXCEPT()
	rsock->encode();
	int answer = OK;
	rsock->code( answer );
	rsock->end_of_message();

		// Now that we know the events are logged and commited to
		// the queue, we can do the final actions for these jobs,
		// like killing shadows if needed...
	for( i=0; i<num_matches; i++ ) {
		if( jobs[i].cluster == -1 ) {
			continue;
		}
		enqueueActOnJobMyself( jobs[i], action, notify, true );
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
	ActOnJobRec(PROC_ID job_id, JobAction action, bool notify, bool log):
		m_job_id(job_id.cluster,job_id.proc,-1), m_action(action), m_notify(notify), m_log(log) {}

	CondorID m_job_id;
	JobAction m_action;
	bool m_notify;
	bool m_log;

		/** These are not actually used, because we are
		 *  using the all_dups option to SelfDrainingQueue. */
	virtual int ServiceDataCompare( ServiceData const* other ) const;
	virtual unsigned int HashFn( ) const;
};

int
ActOnJobRec::ServiceDataCompare( ServiceData const* other ) const
{
	ActOnJobRec const *o = (ActOnJobRec const *)other;

	if( m_notify < o->m_notify ) {
		return -1;
	}
	else if( m_notify > o->m_notify ) {
		return 1;
	}
	else if( m_action < o->m_action ) {
		return -1;
	}
	else if( m_action > o->m_action ) {
		return 1;
	}
	return m_job_id.ServiceDataCompare( &o->m_job_id );
}

unsigned int
ActOnJobRec::HashFn( ) const
{
	return m_job_id.HashFn();
}

void
Scheduler::enqueueActOnJobMyself( PROC_ID job_id, JobAction action, bool notify, bool log )
{
	ActOnJobRec *act_rec = new ActOnJobRec( job_id, action, notify, log );
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
static bool
removeOtherJobs( int cluster, int proc )
{
	bool result = true;

	MyString removeConstraint;
	int attrResult = GetAttributeString( cluster, proc,
				ATTR_OTHER_JOB_REMOVE_REQUIREMENTS,
				removeConstraint );
	if ( attrResult == 0 && removeConstraint != "" ) {
		dprintf( D_ALWAYS,
					"Constraint <%s = %s> fired because job (%d.%d) "
					"was removed\n",
					ATTR_OTHER_JOB_REMOVE_REQUIREMENTS,
					removeConstraint.Value(), cluster, proc );
		MyString reason;
		reason.formatstr(
					"removed because <%s = %s> fired when job (%d.%d)"
					" was removed", ATTR_OTHER_JOB_REMOVE_REQUIREMENTS,
					removeConstraint.Value(), cluster, proc );
		result = abortJobsByConstraint(removeConstraint.Value(),
					reason.Value(), true);
	}

	return result;
}

int
Scheduler::actOnJobMyselfHandler( ServiceData* data )
{
	ActOnJobRec *act_rec = (ActOnJobRec *)data;

	JobAction action = act_rec->m_action;
	bool notify = act_rec->m_notify;
	bool log    = act_rec->m_log;
	PROC_ID job_id;
	job_id.cluster = act_rec->m_job_id._cluster;
	job_id.proc = act_rec->m_job_id._proc;

	delete act_rec;

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
		abort_job_myself( job_id, action, log, notify );		

			//
			// Changes here to fix gittrac #741 and #1490:
			// 1) Child job removing code below is enabled for all
			//    platforms.
			// 2) Child job removing code below is only executed when
			//    *removing* a job.
			// 3) Child job removing code is only executed when the
			//    removed job has ChildRemoveConstraint set in its classad.
			// The main reason for doing this is that, if we don't, when
			// a DAGMan job is held and then removed, its child jobs
			// are left running.
			//
		if ( action == JA_REMOVE_JOBS ) {
			(void)removeOtherJobs(job_id.cluster, job_id.proc);
		}
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

/* 
   Helper function used by both DedicatedScheduler::negotiate() and
   Scheduler::negotiate().  This checks all the various reasons why we
   might not be able to or want to start another shadow
   (MAX_JOBS_RUNNING, swap space problems, etc), and returns true if
   we can proceed or false if we can't start another shadow.  */
bool
Scheduler::canSpawnShadow()
{
	int shadows = numShadows + RunnableJobQueue.Length() + num_pending_startd_contacts + startdContactQueue.Length();

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

		// returns true if no job similar to job_id may be started right now
		// (e.g. MAX_JOBS_RUNNING could cause this to return true)
	bool skipAllSuchJobs(PROC_ID job_id);

		// Define the virtual functions required by ScheddNegotiate //

	virtual bool scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad );

	virtual bool scheduler_skipJob(PROC_ID job_id);

	virtual void scheduler_handleJobRejected(PROC_ID job_id,char const *reason);

	virtual bool scheduler_handleMatch(PROC_ID job_id,char const *claim_id,ClassAd &match_ad, char const *slot_name);

	virtual void scheduler_handleNegotiationFinished( Sock *sock );

};

bool
MainScheddNegotiate::scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad )
{
	ClassAd *ad = GetJobAd( job_id.cluster, job_id.proc );
	if( !ad ) {
		return false;
	}

	job_ad = *ad;

	FreeJobAd( ad );
	return true;
}

bool
MainScheddNegotiate::scheduler_skipJob(PROC_ID job_id)
{
	if( scheduler.AlreadyMatched(&job_id) ) {
		return true;
	}
	if( !Runnable(&job_id) ) {
		return true;
	}

	return skipAllSuchJobs(job_id);
}

bool
MainScheddNegotiate::skipAllSuchJobs(PROC_ID job_id)
{
		// Figure out if this request would result in another shadow
		// process if matched.  If Grid, the answer is no.  Otherwise,
		// always yes.

	int job_universe;

	if (GetAttributeInt(job_id.cluster, job_id.proc,
						ATTR_JOB_UNIVERSE, &job_universe) < 0) {
		dprintf(D_FULLDEBUG, "Failed to get universe for job %d.%d\n",
				job_id.cluster, job_id.proc);
		return true;
	}
	int shadow_num_increment = 1;
	if(job_universe == CONDOR_UNIVERSE_GRID) {
		shadow_num_increment = 0;
	}

		// Next, make sure we could start another shadow without
		// violating some limit.
	if( shadow_num_increment ) {
		if( !scheduler.canSpawnShadow() ) {
			return true;
		}
	}

	return false;
}

bool
MainScheddNegotiate::scheduler_handleMatch(PROC_ID job_id,char const *claim_id,ClassAd &match_ad, char const *slot_name)
{
	ASSERT( claim_id );
	ASSERT( slot_name );

	dprintf(D_FULLDEBUG,"Received match for job %d.%d (delivered=%d): %s\n",
			job_id.cluster, job_id.proc, m_current_resources_delivered, slot_name);

	if( scheduler_skipJob(job_id) ) {

		if( job_id.cluster != -1 && job_id.proc != -1 ) {
			if( skipAllSuchJobs(job_id) ) {
					// No point in trying to find a different job,
					// because we've hit MAX_JOBS_RUNNING or something
					// like that.
				dprintf(D_FULLDEBUG,
					"Rejecting match to %s "
					"because no job may be started to run on it right now.\n",
					slot_name);
				return false;
			}

			dprintf(D_FULLDEBUG,
					"Skipping job %d.%d because it no longer needs a match.\n",
					job_id.cluster,job_id.proc);
		}

		FindRunnableJob(job_id,&match_ad,getOwner());

		if( job_id.cluster != -1 && job_id.proc != -1 ) {
			dprintf(D_FULLDEBUG,"Rematched %s to job %d.%d\n",
					slot_name, job_id.cluster, job_id.proc );
		}
	}
	if( job_id.cluster == -1 || job_id.proc == -1 ) {
		dprintf(D_FULLDEBUG,"No job found to run on %s\n",slot_name);
		return false;
	}

	if ( strcasecmp(claim_id,"null") == 0 ) {
			// No ClaimId given by the matchmaker.  This means
			// the resource we were matched with does not support
			// the claiming protocol.
			//
			// So, set the matched attribute in our classad to be true,
			// and store a copy of match_ad in a hashtable.

		scheduler.InsertMachineAttrs(job_id.cluster,job_id.proc,&match_ad);

		ClassAd *tmp_ad = NULL;
		scheduler.resourcesByProcID->lookup(job_id,tmp_ad);
		if ( tmp_ad ) delete tmp_ad;
		tmp_ad = new ClassAd( match_ad );
		scheduler.resourcesByProcID->insert(job_id,tmp_ad);

			// Update matched attribute in job ad
		SetAttribute(job_id.cluster,job_id.proc,
					 ATTR_JOB_MATCHED,"True",NONDURABLE);
		SetAttributeInt(job_id.cluster,job_id.proc,
						ATTR_CURRENT_HOSTS,1,NONDURABLE);

		return true;
	}

	Daemon startd(&match_ad,DT_STARTD,NULL);
	if( !startd.addr() ) {
		dprintf( D_ALWAYS, "Can't find address of startd in match ad:\n" );
		dPrintAd(D_ALWAYS, match_ad);
		return false;
	}

	match_rec *mrec = scheduler.AddMrec(
		claim_id, startd.addr(), &job_id, &match_ad,
		getOwner(), getRemotePool() );

	if( !mrec ) {
			// There is already a match for this claim id.
		return false;
	}

	ContactStartdArgs *args = new ContactStartdArgs( claim_id, startd.addr(), false );

	if( !scheduler.enqueueStartdContact(args) ) {
		delete args;
		scheduler.DelMrec( mrec );
		return false;
	}

	return true;
}

void
MainScheddNegotiate::scheduler_handleJobRejected(PROC_ID job_id,char const *reason)
{
	ASSERT( reason );

	dprintf(D_FULLDEBUG, "Job %d.%d (delivered=%d) rejected: %s\n",
			job_id.cluster, job_id.proc, m_current_resources_delivered, reason);

	SetAttributeString(
		job_id.cluster, job_id.proc,
		ATTR_LAST_REJ_MATCH_REASON,	reason, NONDURABLE);

	SetAttributeInt(
		job_id.cluster, job_id.proc,
		ATTR_LAST_REJ_MATCH_TIME, (int)time(0), NONDURABLE);
}

void
MainScheddNegotiate::scheduler_handleNegotiationFinished( Sock *sock )
{
	bool satisfied = getSatisfaction();
	char const *remote_pool = getRemotePool();

	dprintf(D_ALWAYS,"Finished negotiating for %s%s%s: %d matched, %d rejected\n",
			getOwner(),
			remote_pool ? " in pool " : "",
			remote_pool ? remote_pool : " in local pool",
			getNumJobsMatched(), getNumJobsRejected() );

	scheduler.negotiationFinished( getOwner(), remote_pool, satisfied );

	int rval =
		daemonCore->Register_Socket(
			sock, "<Negotiator Socket>", 
			(SocketHandlercpp)&Scheduler::negotiatorSocketHandler,
			"<Negotiator Command>", &scheduler, ALLOW);

	if( rval >= 0 ) {
			// do not delete this sock until we get called back
		sock->incRefCount();
	}
}

void
Scheduler::negotiationFinished( char const *owner, char const *remote_pool, bool satisfied )
{
	int owner_num;
	for (owner_num = 0;
		 owner_num < N_Owners && strcmp(Owners[owner_num].Name, owner);
		 owner_num++) ;
	if (owner_num == N_Owners) {
		dprintf(D_ALWAYS, "Can't find owner %s in Owners array!\n", owner);
		return;
	}

	Daemon *flock_col = NULL;
	int n,flock_level = 0;
	if( remote_pool && *remote_pool ) {
		for( n=1, FlockCollectors->rewind();
			 FlockCollectors->next(flock_col);
			 n++)
		{
			if( !flock_col->name() ) {
				continue;
			}
			if( !strcmp(remote_pool,flock_col->name()) ) {
				flock_level = n;
				break;
			}
		}
		if( flock_level != n ) {
			dprintf(D_ALWAYS,
				"Warning: did not find flocking level for remote pool %s\n",
				remote_pool );
		}
	}

	if( satisfied || Owners[owner_num].FlockLevel == flock_level ) {
			// NOTE: we do not want to set NegotiationTimestamp if
			// this negotiator is less than our current flocking level
			// and we are unsatisfied, because then if the negotiator
			// at the current flocking level never contacts us, but
			// others do, we will never give up waiting, and we will
			// therefore not advance to the next flocking level.
		Owners[owner_num].NegotiationTimestamp = time(0);
	}

	if( satisfied ) {
		// We are out of jobs.  Stop flocking with less desirable pools.
		if (Owners[owner_num].FlockLevel > flock_level ) {
			dprintf(D_ALWAYS,
					"Decreasing flock level for %s to %d from %d.\n",
					owner, flock_level, Owners[owner_num].FlockLevel);
			Owners[owner_num].FlockLevel = flock_level;
		}

		timeout(); // invalidate our ads immediately
	} else {
		if (Owners[owner_num].FlockLevel < MaxFlockLevel &&
		    Owners[owner_num].FlockLevel == flock_level)
		{ 
			int oldlevel = Owners[owner_num].FlockLevel;
			Owners[owner_num].FlockLevel+= param_integer("FLOCK_INCREMENT",1,1);
			if(Owners[owner_num].FlockLevel > MaxFlockLevel) {
				Owners[owner_num].FlockLevel = MaxFlockLevel;
			}
			dprintf(D_ALWAYS,
					"Increasing flock level for %s to %d from %d.\n",
					owner, Owners[owner_num].FlockLevel,oldlevel);

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
Scheduler::negotiate(int command, Stream* s)
{
	int		job_index;
	int		jobs;						// # of jobs that CAN be negotiated
	int		which_negotiator = 0; 		// >0 implies flocking
	MyString remote_pool_buf;
	char const *remote_pool = NULL;
	Daemon*	neg_host = NULL;	
	int		owner_num;
	Sock*	sock = (Sock*)s;
	bool skip_negotiation = false;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

		// Prior to 7.5.4, the negotiator sent NEGOTIATE_WITH_SIGATTRS
		// As of 7.5.4, since we are putting ATTR_SUBMITTER_TAG into
		// the submitter ads, the negotiator sends NEGOTIATE
	if (command != NEGOTIATE_WITH_SIGATTRS && command != NEGOTIATE)
	{
		dprintf(D_ALWAYS,
				"Negotiator command was %d (not NEGOTIATE_WITH_SIGATTRS or NEGOTIATE) "
				"--- aborting\n", command);
		return (!(KEEP_STREAM));
	}

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

	// SubmitterAds with different attributes can elicit a
	// change of the command int for each submitter ad conversation, so
	// we explicit print it out to help us debug.
	dprintf(D_ALWAYS, "Using negotiation protocol: %s\n", 
		getCommandString(command));

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
	char owner[200], *ownerptr = owner;
	char *sig_attrs_from_cm = NULL;	
	int consider_jobprio_min = INT_MIN;
	int consider_jobprio_max = INT_MAX;
	ClassAd negotiate_ad;
	MyString submitter_tag;
	s->decode();
	if( command == NEGOTIATE ) {
		if( !getClassAd( s, negotiate_ad ) ) {
			dprintf( D_ALWAYS, "Can't receive negotiation header\n" );
			return (!(KEEP_STREAM));
		}
		if( !negotiate_ad.LookupString(ATTR_OWNER,owner,sizeof(owner)) ) {
			dprintf( D_ALWAYS, "Can't find %s in negotiation header!\n",
					 ATTR_OWNER );
			return (!(KEEP_STREAM));
		}
		if( !negotiate_ad.LookupString(ATTR_AUTO_CLUSTER_ATTRS,&sig_attrs_from_cm) ) {
			dprintf( D_ALWAYS, "Can't find %s in negotiation header!\n",
					 ATTR_AUTO_CLUSTER_ATTRS );
			return (!(KEEP_STREAM));
		}
		if( !negotiate_ad.LookupString(ATTR_SUBMITTER_TAG,submitter_tag) ) {
			dprintf( D_ALWAYS, "Can't find %s in negotiation header!\n",
					 ATTR_SUBMITTER_TAG );
			free(sig_attrs_from_cm);
			return (!(KEEP_STREAM));
		}
			// jobprio_min and jobprio_max are optional
		negotiate_ad.LookupInteger("JOBPRIO_MIN",consider_jobprio_min);
		negotiate_ad.LookupInteger("JOBPRIO_MAX",consider_jobprio_max);
	}
	else {
			// old NEGOTIATE_WITH_SIGATTRS protocol
		if (!s->get(ownerptr,sizeof(owner))) {
			dprintf( D_ALWAYS, "Can't receive owner from manager\n" );
			return (!(KEEP_STREAM));
		}
		if (!s->code(sig_attrs_from_cm)) {	// result is mallec-ed!
			dprintf( D_ALWAYS, "Can't receive sig attrs from manager\n" );
			return (!(KEEP_STREAM));
		}
	}
	if (!s->end_of_message()) {
		dprintf( D_ALWAYS, "Can't receive owner/EOM from manager\n" );
		free(sig_attrs_from_cm);
		return (!(KEEP_STREAM));
	}

	if( FlockCollectors && command == NEGOTIATE ) {
			// Use the submitter tag to figure out which negotiator we
			// are talking to.  We insert a different submitter tag
			// into the submitter ad that we send to each CM.  In fact,
			// the tag is just equal to the collector address for the CM.
		if( submitter_tag != HOME_POOL_SUBMITTER_TAG ) {
			int n;
			bool match = false;
			Daemon *flock_col = NULL;
			for( n=1, FlockCollectors->rewind();
				 FlockCollectors->next(flock_col);
				 n++)
			{
				if( submitter_tag == flock_col->name() ){
					which_negotiator = n;
					remote_pool_buf = flock_col->name();
					remote_pool = remote_pool_buf.Value();
					match = true;
					break;
				}
			}
			if( !match ) {
				dprintf(D_ALWAYS, "Unknown negotiator (host=%s,tag=%s).  "
						"Aborting negotiation.\n", sock->peer_ip_str(),
						submitter_tag.Value());
				free(sig_attrs_from_cm);
				return (!(KEEP_STREAM));
			}
		}
	}
	else if( FlockNegotiators && command == NEGOTIATE_WITH_SIGATTRS ) {
			// This is the old (pre 7.5.4) method for determining
			// which negotiator we are talking to.  It is brittle
			// because it depends on a DNS lookup of the negotiator
			// name matching the peer address.  This is the only place
			// in the schedd where we really depend on NEGOTIATOR_HOST
			// and FLOCK_NEGOTIATOR_HOSTS.

		// first, check if this is our local negotiator
		condor_sockaddr endpoint_addr = sock->peer_addr();
		std::vector<condor_sockaddr> addrs;
		std::vector<condor_sockaddr>::iterator iter;
		bool match = false;
		Daemon negotiator (DT_NEGOTIATOR);
		char *negotiator_hostname = negotiator.fullHostname();
		if (!negotiator_hostname) {
			dprintf(D_ALWAYS, "Negotiator hostname lookup failed!\n");
			free(sig_attrs_from_cm);
			return (!(KEEP_STREAM));
		}
		addrs = resolve_hostname(negotiator_hostname);
		if (addrs.empty()) {
			dprintf(D_ALWAYS, "gethostbyname for local negotiator (%s) failed!"
					"  Aborting negotiation.\n", negotiator_hostname);
			free(sig_attrs_from_cm);
			return (!(KEEP_STREAM));
		}
		for (iter = addrs.begin(); iter != addrs.end(); ++iter) {
			const condor_sockaddr& addr = *iter;
			if (addr.compare_address(endpoint_addr)) {
					match = true;
				break;
				}
			}
		// if it isn't our local negotiator, check the FlockNegotiators list.
		if (!match) {
			int n;
			for( n=1, FlockNegotiators->rewind();
				 !match && FlockNegotiators->next(neg_host); n++) {
				addrs = resolve_hostname(neg_host->fullHostname());
				for (iter = addrs.begin(); iter != addrs.end(); ++iter) {
					const condor_sockaddr& addr = *iter;
					if (addr.compare_address(endpoint_addr)) {
						match = true;
						which_negotiator = n;
						remote_pool_buf = neg_host->pool();
						remote_pool = remote_pool_buf.Value();
						break;
					}
				}
			}
		}
		if (!match) {
			dprintf(D_ALWAYS, "Unknown negotiator (%s).  "
					"Aborting negotiation.\n", sock->peer_ip_str());
			free(sig_attrs_from_cm);
			return (!(KEEP_STREAM));
		}
	}
	else if( FlockCollectors ) {
		EXCEPT("Unexpected negotiation command %d\n", command);
	}


	if( remote_pool ) {
		dprintf (D_ALWAYS, "Negotiating for owner: %s (flock level %d, pool %s)\n",
				 owner, which_negotiator, remote_pool);
	} else {
		dprintf (D_ALWAYS, "Negotiating for owner: %s\n", owner);
	}

	if ( consider_jobprio_min > INT_MIN || consider_jobprio_max < INT_MAX ) {
		dprintf(D_ALWAYS,"Negotiating owner=%s jobprio restricted, min=%d max=%d\n",
			 owner, consider_jobprio_min, consider_jobprio_max);
	}

	//-----------------------------------------------

		// See if the negotiator wants to talk to the dedicated
		// scheduler

	if( ! strcmp(owner, dedicated_scheduler.name()) ) {
			// Just let the DedicatedScheduler class do its thing. 
		if (sig_attrs_from_cm) {
			free(sig_attrs_from_cm);
		}
		return dedicated_scheduler.negotiate( command, sock, remote_pool );
	}

		// If we got this far, we're negotiating for a regular user,
		// so go ahead and do our expensive setup operations.

		// Tell the autocluster code what significant attributes the
		// negotiator told us about
	if ( sig_attrs_from_cm ) {
		if ( autocluster.config(sig_attrs_from_cm) ) {
			// clear out auto cluster id attributes
			WalkJobQueue( (int(*)(ClassAd *))clear_autocluster_id );
			DirtyPrioRecArray(); // should rebuild PrioRecArray
		}
		free(sig_attrs_from_cm);
		sig_attrs_from_cm = NULL;
	}

	BuildPrioRecArray();
	jobs = N_PrioRecs;

	JobsStarted = 0;

	// find owner in the Owners array
	char *at_sign = strchr(owner, '@');
	if (at_sign) *at_sign = '\0';
	for (owner_num = 0;
		 owner_num < N_Owners && strcmp(Owners[owner_num].Name, owner);
		 owner_num++) ;
	if (owner_num == N_Owners) {
		dprintf(D_ALWAYS, "Can't find owner %s in Owners array!\n", owner);
		jobs = 0;
		skip_negotiation = true;
	} else if (Owners[owner_num].FlockLevel < which_negotiator) {
		dprintf(D_FULLDEBUG,
				"This user is no longer flocking with this negotiator.\n");
		jobs = 0;
		skip_negotiation = true;
	}

	ResourceRequestList *resource_requests = new ResourceRequestList;
	ResourceRequestCluster *cluster = NULL;
	int next_cluster = 0;

	for(job_index = 0; job_index < N_PrioRecs && !skip_negotiation; job_index++) {
		prio_rec *prec = &PrioRec[job_index];

		// make sure owner matches what negotiator wants
		if(strcmp(owner,prec->owner)!=0)
		{
			jobs--;
			continue;
		}

		// make sure jobprio is in the range the negotiator wants
		if ( consider_jobprio_min > prec->job_prio ||
			 prec->job_prio > consider_jobprio_max )
		{
			jobs--;
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

		if( !cluster || cluster->getAutoClusterId() != auto_cluster_id )
		{
			cluster = new ResourceRequestCluster( auto_cluster_id );
			resource_requests->push_back( cluster );
		}
		cluster->addJob( prec->id );
	}

	classy_counted_ptr<MainScheddNegotiate> sn =
		new MainScheddNegotiate(
			command,
			resource_requests,
			owner,
			remote_pool
		);

		// handle the rest of the negotiation protocol asynchronously
	sn->negotiate(sock);

	return KEEP_STREAM;
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
	if( matches->lookup(HashKey(claim_id), mrec) != 0 ) {
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
	FREE (claim_id);
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

	MyString description;
	description.formatstr( "%s %d.%d", mrec->description(),
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
		jobAd = GetJobAd( mrec->cluster, mrec->proc, true, false );
	}
	if( ! jobAd ) {
			// The match rec may have been deleted by now if the job
			// was put on hold in GetJobAd().
		mrec = NULL;

		char const *reason = "find/expand";
		if( !args->isDedicated() ) {
			if( GetJobAd( cluster, proc, false ) ) {
				reason = "expand";
			}
			else {
				reason = "find";
			}
		}
		dprintf( D_ALWAYS,
				 "Failed to %s job when requesting claim %s\n",
				 reason, description.Value() );

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
	if ( (jobAd && mrec && mrec->my_match_ad) && 
		!ScheddNegotiate::fixupPartitionableSlot(jobAd,mrec->my_match_ad) )
	{
			// The job classad does not have required attributes (such as 
			// requested memory) to enable the startd to create a dynamic slot.
			// Since this claim request is simply going to fail, lets throw
			// this match away now (seems like we could do something better?) - 
			// while it is not ideal to throw away the match in this instance,
			// it is consistent with what we current do during negotiation.
		DelMrec ( mrec );
		return;
	}

    // some attributes coming out of negotiator's matching process that need to
    // make a subway transfer from slot/match ad to job/request ad, on their way
    // to the claim, and then eventually back around to the negotiator for use in
    // preemption policies:
    jobAd->CopyAttribute(ATTR_REMOTE_GROUP, mrec->my_match_ad);
    jobAd->CopyAttribute(ATTR_REMOTE_NEGOTIATING_GROUP, mrec->my_match_ad);
    jobAd->CopyAttribute(ATTR_REMOTE_AUTOREGROUP, mrec->my_match_ad);

		// Setup to claim the slot asynchronously

	jobAd->Assign( ATTR_STARTD_SENDS_ALIVES, mrec->m_startd_sends_alives );

	classy_counted_ptr<DCMsgCallback> cb = new DCMsgCallback(
		(DCMsgCallback::CppFunction)&Scheduler::claimedStartd,
		this,
		mrec);

	ASSERT( !mrec->claim_requester.get() );
	mrec->claim_requester = cb;
	mrec->setStatus( M_STARTD_CONTACT_LIMBO );

	classy_counted_ptr<DCStartd> startd = new DCStartd(mrec->description(),NULL,mrec->peer,mrec->claimId());

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
		description.Value(),
		daemonCore->publicNetworkIpAddr(),
		scheduler.aliveInterval(),
		STARTD_CONTACT_TIMEOUT, // timeout on individual network ops
		deadline_timeout,       // overall timeout on completing claim request
		cb );

	delete jobAd;

		// Now wait for callback...
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
		scheduler.DelMrec(match);
		return;
	}

	match->setStatus( M_CLAIMED );

	// now that we've completed authentication (if enabled),
	// authorize this startd for READ operations
	//
	if ( match->auth_hole_id == NULL ) {
		match->auth_hole_id = new MyString;
		ASSERT(match->auth_hole_id != NULL);
		if (msg->startd_fqu() && *msg->startd_fqu()) {
			match->auth_hole_id->formatstr("%s/%s",
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
			        match->auth_hole_id->Value(),
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

			sn = new DedicatedScheddNegotiate(0, NULL, match->user, NULL);
		} else {
			// Use the DedSched
			sn = new MainScheddNegotiate(0, NULL, match->user, NULL);
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
			// Need to pass handleMatch a slot name; grab from leftover slot ad
		std::string slot_name_buf;
		msg->leftover_startd_ad()->LookupString(ATTR_NAME,slot_name_buf);
		char const *slot_name = slot_name_buf.c_str();

			// dprintf a message saying we got a new match, but be certain
			// to only output the public claim id (keep the capability private)
		ClaimIdParser idp( msg->leftover_claim_id() );
		dprintf( D_FULLDEBUG,
				"Received match from startd, leftover slot ad %s claim %s\n",
				slot_name, idp.publicClaimId()  );

			// Tell the schedd about the leftover resources it can go claim.
			// Note this claiming will happen asynchronously.
		sn->scheduler_handleMatch(jobid,msg->leftover_claim_id(),
			*(msg->leftover_startd_ad()),slot_name);

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
	 if( startdContactQueue.enqueue(args) < 0 ) {
		 dprintf( D_ALWAYS, "Failed to enqueue contactStartd "
				  "startd=%s\n", args->sinful() );
		 return false;
	 }
	 dprintf( D_FULLDEBUG, "Enqueued contactStartd startd=%s\n",
			  args->sinful() );  

	 rescheduleContactQueue();

	 return true;
}


void
Scheduler::rescheduleContactQueue()
{
	if( startdContactQueue.IsEmpty() ) {
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
Scheduler::checkContactQueue() 
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
		   (!startdContactQueue.IsEmpty()) ) {
			// there's a pending registration in the queue:

		startdContactQueue.dequeue ( args );
		dprintf( D_FULLDEBUG, "In checkContactQueue(), args = %p, "
				 "host=%s\n", args, args->sinful() ); 
		contactStartd( args );
		delete args;
	}
}


bool
Scheduler::enqueueReconnectJob( PROC_ID job )
{
	 if( ! jobsToReconnect.Append(job) ) {
		 dprintf( D_ALWAYS, "Failed to enqueue job id (%d.%d)\n",
				  job.cluster, job.proc );
		 return false;
	 }
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
Scheduler::checkReconnectQueue( void ) 
{
	PROC_ID job;
	CondorQuery query(STARTD_AD);
	ClassAdList job_ads;
	MyString constraint;

		// clear out the timer tid, since we made it here.
	checkReconnectQueue_tid = -1;

	jobsToReconnect.Rewind();
	while( jobsToReconnect.Next(job) ) {
		makeReconnectRecords(&job, NULL);
		jobsToReconnect.DeleteCurrent();
	}
}


void
Scheduler::makeReconnectRecords( PROC_ID* job, const ClassAd* match_ad ) 
{
	int cluster = job->cluster;
	int proc = job->proc;
	char* pool = NULL;
	char* owner = NULL;
	char* claim_id = NULL;
	char* startd_addr = NULL;
	char* startd_name = NULL;
	char* startd_principal = NULL;

	// NOTE: match_ad could be deallocated when this function returns,
	// so if we need to keep it around, we must make our own copy of it.
	if( GetAttributeStringNew(cluster, proc, ATTR_ACCOUNTING_GROUP, &owner) < 0 ) {
		if( GetAttributeStringNew(cluster, proc, ATTR_OWNER, &owner) < 0 ) {
				// we've got big trouble, just give up.
			dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
					 ATTR_OWNER, cluster, proc );
			mark_job_stopped( job );
			return;
		}
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_CLAIM_ID, &claim_id) < 0 ) {
			//
			// No attribute. Clean up and return
			//
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				ATTR_CLAIM_ID, cluster, proc );
		mark_job_stopped( job );
		free( owner );
		return;
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_REMOTE_HOST, &startd_name) < 0 ) {
			//
			// No attribute. Clean up and return
			//
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				ATTR_REMOTE_HOST, cluster, proc );
		mark_job_stopped( job );
		free( claim_id );
		free( owner );
		return;
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_STARTD_IP_ADDR, &startd_addr) < 0 ) {
			// We only expect to get here when reading a job queue created
			// by a version of Condor older than 7.1.3, because we no longer
			// rely on the claim id to tell us how to connect to the startd.
		dprintf( D_ALWAYS, "WARNING: %s not in job queue for %d.%d, "
				 "so using claimid.\n", ATTR_STARTD_IP_ADDR, cluster, proc );
		startd_addr = getAddrFromClaimId( claim_id );
		SetAttributeString(cluster, proc, ATTR_STARTD_IP_ADDR, startd_addr);
	}
	
	int universe;
	GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &universe );

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
		event.setDisconnectReason( txt );
		event.setStartdAddr( startd_addr );
		event.setStartdName( startd_name );

		if( !ULog->writeEventNoFsync(&event,GetJobAd(cluster,proc)) ) {
			dprintf( D_ALWAYS, "Unable to log ULOG_JOB_DISCONNECTED event\n" );
		}
		delete ULog;
		ULog = NULL;
	}

	dprintf( D_FULLDEBUG, "Adding match record for disconnected job %d.%d "
			 "(owner: %s)\n", cluster, proc, owner );
	ClaimIdParser idp( claim_id );
	dprintf( D_FULLDEBUG, "ClaimId: %s\n", idp.publicClaimId() );
	if( pool ) {
		dprintf( D_FULLDEBUG, "Pool: %s (via flocking)\n", pool );
	}
		// note: AddMrec will makes its own copy of match_ad
	match_rec *mrec = AddMrec( claim_id, startd_addr, job, match_ad, 
							   owner, pool );

		// authorize this startd for READ access
	if (startd_principal != NULL) {
		mrec->auth_hole_id = new MyString(startd_principal);
		ASSERT(mrec->auth_hole_id != NULL);
		free(startd_principal);
		IpVerify* ipv = daemonCore->getIpVerify();
		if (!ipv->PunchHole(READ, *mrec->auth_hole_id)) {
			dprintf(D_ALWAYS,
			        "WARNING: IpVerify::PunchHole error for %s: "
			            "job %d.%d may fail to execute\n",
			        mrec->auth_hole_id->Value(),
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
	if( owner ) {
		free( owner );
		owner = NULL;
	}
	if( startd_addr ) {
		free( startd_addr );
		startd_addr = NULL;
	}
	if( startd_name ) {
		free( startd_name );
		startd_name = NULL;
	}
	if( claim_id ) {
		free( claim_id );
		claim_id = NULL;
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
updateSchedDInterval( ClassAd *job )
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
			PROC_ID id;
			job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
			job->LookupInteger(ATTR_PROC_ID, id.proc);
			dprintf( D_ALWAYS, "Failed to update job %d.%d's %s attribute!\n",
							   id.cluster, id.proc, ATTR_SCHEDD_INTERVAL );
		}
	}
	return ( true );
}

int
find_idle_local_jobs( ClassAd *job )
{
	int	status;
	int	cur_hosts;
	int	max_hosts;
	int	univ;
	PROC_ID id;

	int noop = 0;
	job->LookupBool(ATTR_JOB_NOOP, noop);
	if (noop) {
		return 0;
	}

	if (job->LookupInteger(ATTR_JOB_UNIVERSE, univ) != 1) {
		univ = CONDOR_UNIVERSE_STANDARD;
	}

	if( univ != CONDOR_UNIVERSE_LOCAL && univ != CONDOR_UNIVERSE_SCHEDULER ) {
		return 0;
	}

	if (univ == CONDOR_UNIVERSE_LOCAL && scheduler.m_use_startd_for_local) {
		return 0;
	}

	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);
	job->LookupInteger(ATTR_JOB_STATUS, status);

	if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) != 1) {
		cur_hosts = ((status == RUNNING || status == TRANSFERRING_OUTPUT) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) != 1) {
		max_hosts = ((status == IDLE) ? 1 : 0);
	}
	
		//
		// Before evaluating whether we can run this job, first make 
		// sure its even eligible to run
		// We do not count REMOVED or HELD jobs
		//
	if ( max_hosts > cur_hosts &&
		(status == IDLE || status == RUNNING || status == TRANSFERRING_OUTPUT) ) {
			//
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
		const char *universeExp = ( univ == CONDOR_UNIVERSE_LOCAL ?
									ATTR_START_LOCAL_UNIVERSE :
									ATTR_START_SCHEDULER_UNIVERSE );
	
			//
			// Start Universe Evaluation (a.k.a. schedd requirements).
			// Only if this attribute is NULL or evaluates to true 
			// will we allow a job to start.
			//
		bool requirementsMet = true;
		int requirements = 1;
		if ( scheddAd.LookupExpr( universeExp ) != NULL ) {
				//
				// We have this inner block here because the job
				// should not be allowed to start if the schedd's 
				// requirements failed to evaluate for some reason
				//
			if ( scheddAd.EvalBool( universeExp, job, requirements ) ) {
				requirementsMet = (bool)requirements;
				if ( ! requirements ) {
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

		if ( ! requirementsMet ) {
			char *exp = sPrintExpr( scheddAd, universeExp );
			if ( exp ) {
				dprintf( D_FULLDEBUG, "Failed expression '%s'\n", exp );
				free( exp );
			}
			return ( 0 );
		}
			//
			// Job Requirements Evaluation
			//
		if ( job->LookupExpr( ATTR_REQUIREMENTS ) != NULL ) {
				// Treat undefined/error as FALSE for job requirements, too.
			if ( job->EvalBool(ATTR_REQUIREMENTS, &scheddAd, requirements) ) {
				requirementsMet = (bool)requirements;
				if ( !requirements ) {
					dprintf( D_ALWAYS, "The %s attribute for job %d.%d "
							 "evaluated to false. Unable to start job\n",
							 ATTR_REQUIREMENTS, id.cluster, id.proc );
				}
			} else {
				requirementsMet = false;
				dprintf( D_ALWAYS, "The %s attribute for job %d.%d did "
						 "not evaluate. Unable to start job\n",
						 ATTR_REQUIREMENTS, id.cluster, id.proc );
			}

		}
			//
			// If the job's requirements failed up above, we will want to 
			// print the expression to the user and return
			//
		if ( ! requirementsMet ) {
			char *exp = sPrintExpr( *job, ATTR_REQUIREMENTS );
			if ( exp ) {
				dprintf( D_FULLDEBUG, "Failed expression '%s'\n", exp );
				free( exp );
			}
			// This is too verbose.
			//dprintf(D_FULLDEBUG,"Schedd ad that failed to match:\n");
			//dPrintAd(D_FULLDEBUG, scheddAd);
			//dprintf(D_FULLDEBUG,"Job ad that failed to match:\n");
			//dPrintAd(D_FULLDEBUG, *job);
			return ( 0 );
		}

			//
			// It's safe to go ahead and run the job!
			//
		if( univ == CONDOR_UNIVERSE_LOCAL ) {
			dprintf( D_FULLDEBUG, "Found idle local universe job %d.%d\n",
					 id.cluster, id.proc );
			scheduler.start_local_universe_job( &id );
		} else {
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
	return 0;
}

void
Scheduler::ExpediteStartJobs()
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
Scheduler::StartJobs()
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
		StartJob( rec );
	}
	if( LocalUniverseJobsIdle > 0 || SchedUniverseJobsIdle > 0 ) {
		StartLocalJobs();
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

		// This is the case we want to try and start a job.
	id.cluster = rec->cluster;
	id.proc = rec->proc; 
	if(!Runnable(&id)) {
			// find the job in the cluster with the highest priority
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
Scheduler::FindRunnableJobForClaim(match_rec* mrec,bool accept_std_univ)
{
	ASSERT( mrec );

	PROC_ID new_job_id;
	new_job_id.cluster = -1;
	new_job_id.proc = -1;

	if( mrec->my_match_ad && !ExitWhenDone ) {
		FindRunnableJob(new_job_id,mrec->my_match_ad,mrec->user);
	}
	if( !accept_std_univ && new_job_id.proc == -1 ) {
		int new_universe = -1;
		GetAttributeInt(new_job_id.cluster,new_job_id.proc,ATTR_JOB_UNIVERSE,&new_universe);
		if( new_universe == CONDOR_UNIVERSE_STANDARD ) {
			new_job_id.proc = -1;
		}
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
Scheduler::StartLocalJobs()
{
	if ( ExitWhenDone ) {
		return;
	}
	WalkJobQueue( (int(*)(ClassAd *))find_idle_local_jobs );
}

shadow_rec*
Scheduler::StartJob(match_rec* mrec, PROC_ID* job_id)
{
	int		universe;
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
Scheduler::StartJobHandler()
{
	shadow_rec* srec;
	PROC_ID* job_id=NULL;
	int cluster, proc;
	int status;

		// clear out our timer id since the hander just went off
	StartJobTimer = -1;

		// if we're trying to shutdown, don't start any new jobs!
	if( ExitWhenDone ) {
		return;
	}

	// get job from runnable job queue
	while( 1 ) {	
		if( RunnableJobQueue.dequeue(srec) < 0 ) {
				// our queue is empty, we're done.
			return;
		}

		// Check to see if job ad is still around; it may have been
		// removed while we were waiting in RunnableJobQueue
		job_id=&srec->job_id;
		cluster = job_id->cluster;
		proc = job_id->proc;
		if( ! isStillRunnable(cluster, proc, status) ) {
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
			RemoveShadowRecFromMrec(srec);
			delete srec;
			continue;
		}

		if ( privsep_enabled() ) {
			// If there is no available transferd for this job (and it 
			// requires it), then start one and put the job back into the queue
			if ( jobNeedsTransferd(cluster, proc, srec->universe) ) {
				if (! availableTransferd(cluster, proc) ) {
					dprintf(D_ALWAYS, 
						"Deferring job start until transferd is registered\n");

					// stop the running of this job
					mark_job_stopped( job_id );
					RemoveShadowRecFromMrec(srec);
					delete srec;
				
					// start up a transferd for this job.
					startTransferd(cluster, proc);

					continue;
				}
			}
		}
			

			// if we got this far, we're definitely starting the job,
			// so deal with the aboutToSpawnJobHandler hook...
		int universe = srec->universe;
		callAboutToSpawnJobHandler( cluster, proc, srec );

		if( (universe == CONDOR_UNIVERSE_MPI) || 
			(universe == CONDOR_UNIVERSE_PARALLEL)) {
			
			if (proc != 0) {
				dprintf( D_ALWAYS, "StartJobHandler called for MPI or Parallel job, with "
					   "non-zero procid for job (%d.%d)\n", cluster, proc);
			}
			
				// We've just called callAboutToSpawnJobHandler on procid 0,
				// now call it on the rest of them
			proc = 1;
			while( GetJobAd( cluster, proc, false)) {
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

bool
Scheduler::jobNeedsTransferd( int cluster, int proc, int univ )
{
	ClassAd *jobad = GetJobAd(cluster, proc);
	ASSERT(jobad);

	// XXX remove this when shadow/starter usage is implemented
	return false;

	/////////////////////////////////////////////////////////////////////////
	// Selection of a transferd is universe based. It all depends upon
	// whether or not transfer_input/output_files is available for the
	// universe in question.
	/////////////////////////////////////////////////////////////////////////

	switch(univ) {
		case CONDOR_UNIVERSE_VANILLA:
		case CONDOR_UNIVERSE_JAVA:
		case CONDOR_UNIVERSE_MPI:
		case CONDOR_UNIVERSE_PARALLEL:
		case CONDOR_UNIVERSE_VM:
			return true;
			break;

		default:
			return false;
			break;
	}

	return false;
}

bool
Scheduler::availableTransferd( int cluster, int proc )
{
	TransferDaemon *td = NULL;

	return availableTransferd(cluster, proc, td);
}

bool
Scheduler::availableTransferd( int cluster, int proc, TransferDaemon *&td_ref )
{
	MyString fquser;
	TransferDaemon *td = NULL;
	ClassAd *jobad = GetJobAd(cluster, proc);

	ASSERT(jobad);

	jobad->LookupString(ATTR_USER, fquser);

	td_ref = NULL;

	td = m_tdman.find_td_by_user(fquser);
	if (td == NULL) {
		return false;
	}

	// only return true if there is a transferd ready and waiting for this
	// user
	if (td->get_status() == TD_REGISTERED) {
		dprintf(D_ALWAYS, "Scheduler::availableTransferd() "
			"Found a transferd for user %s\n", fquser.Value());
		td_ref = td;
		return true;
	}

	return false;
}

bool
Scheduler::startTransferd( int cluster, int proc )
{
	MyString fquser;
	MyString rand_id;
	TransferDaemon *td = NULL;
	ClassAd *jobad = NULL;
	MyString desc;

	// just do a quick check in case a higher layer had already started one
	// for this job.
	if (availableTransferd(cluster, proc)) {
		return true;
	}

	jobad = GetJobAd(cluster, proc);
	ASSERT(jobad);

	jobad->LookupString(ATTR_USER, fquser);

	/////////////////////////////////////////////////////////////////////////
	// It could be that a td had already been started, but hadn't registered
	// yet. In that case, consider it started.
	/////////////////////////////////////////////////////////////////////////

	td = m_tdman.find_td_by_user(fquser);
	if (td == NULL) {
		// No td found at all in any state, so start one.

		// XXX fix this rand_id to be dealt with better, like maybe the tdman
		// object assigns it or something.
		rand_id.randomlyGenerateHex(64);
		td = new TransferDaemon(fquser, rand_id, TD_PRE_INVOKED);
		ASSERT(td != NULL);

		// set up the default registration callback
		desc = "Transferd Registration callback";
		td->set_reg_callback(desc,
			(TDRegisterCallback)
			 	&Scheduler::td_register_callback, this);

		// set up the default reaper callback
		desc = "Transferd Reaper callback";
		td->set_reaper_callback(desc,
			(TDReaperCallback)
				&Scheduler::td_reaper_callback, this);
	
		// Have the td manager object start this td up.
		// XXX deal with failure here a bit better.
		m_tdman.invoke_a_td(td);
	}

	return true;
}


bool
Scheduler::isStillRunnable( int cluster, int proc, int &status )
{
	ClassAd* job = GetJobAd( cluster, proc, false );
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
	case COMPLETED:
		dprintf( D_FULLDEBUG,
				 "Job %d.%d was %s while waiting to start\n",
				 cluster, proc, getJobStatusString(status) );
		return false;
		break;

	default:
		EXCEPT( "StartJobHandler: Unknown status (%d) for job %d.%d\n",
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

	Shadow*	shadow_obj = NULL;
	int		sh_is_dc = FALSE;
	char* 	shadow_path = NULL;
	bool wants_reconnect = false;

	wants_reconnect = srec->is_reconnect;

#ifdef WIN32
		// nothing to choose on NT, there's only 1 shadow
	shadow_path = param("SHADOW");
	sh_is_dc = TRUE;
	bool sh_reads_file = true;
#else
		// UNIX

	if( ! shadow_obj ) {
		switch( universe ) {
		case CONDOR_UNIVERSE_STANDARD:
			shadow_obj = shadow_mgr.findShadow( ATTR_HAS_CHECKPOINTING );
			if( ! shadow_obj ) {
				dprintf( D_ALWAYS, "Trying to run a STANDARD job but you "
						 "do not have a condor_shadow that will work, "
						 "aborting.\n" );
				noShadowForJob( srec, NO_SHADOW_STD );
				srec = NULL;
				return;
			}
			break;
		case CONDOR_UNIVERSE_VANILLA:
		case CONDOR_UNIVERSE_LOCAL: // but only when m_use_start_for_local is true
			shadow_obj = shadow_mgr.findShadow( ATTR_IS_DAEMON_CORE ); 
			if( ! shadow_obj ) {
				dprintf( D_ALWAYS, "Trying to run a VANILLA job, but you "
						 "do not have a daemon-core-based shadow, "
						 "aborting.\n" );
				noShadowForJob( srec, NO_SHADOW_DC_VANILLA );
				return;
			}
			break;
		case CONDOR_UNIVERSE_JAVA:
			shadow_obj = shadow_mgr.findShadow( ATTR_HAS_JAVA );
			if( ! shadow_obj ) {
				dprintf( D_ALWAYS, "Trying to run a JAVA job but you "
						 "do not have a condor_shadow that will work, "
						 "aborting.\n" );
				noShadowForJob( srec, NO_SHADOW_JAVA );
				return;
			}
			break;
		case CONDOR_UNIVERSE_MPI:
		case CONDOR_UNIVERSE_PARALLEL:
			shadow_obj = shadow_mgr.findShadow( ATTR_HAS_MPI );
			if( ! shadow_obj ) {
				dprintf( D_ALWAYS, "Trying to run a MPI job but you "
						 "do not have a condor_shadow that will work, "
						 "aborting.\n" );
				noShadowForJob( srec, NO_SHADOW_MPI );
				return;
			}
			break;
		case CONDOR_UNIVERSE_VM:
			shadow_obj = shadow_mgr.findShadow( ATTR_HAS_VM);
			if( ! shadow_obj ) {
				dprintf( D_ALWAYS, "Trying to run a VM job but you "
						"do not have a condor_shadow that will work, "
						"aborting.\n" );
				noShadowForJob( srec, NO_SHADOW_VM );
				return;
			}
			break;
		default:
			EXCEPT( "StartJobHandler() does not support %d universe jobs",
					universe );
		}
	}

	sh_is_dc = (int)shadow_obj->isDC();
	bool sh_reads_file = shadow_obj->provides( ATTR_HAS_JOB_AD_FROM_FILE );
	shadow_path = strdup( shadow_obj->path() );

	if ( shadow_obj ) {
		delete( shadow_obj );
		shadow_obj = NULL;
	}

#endif /* ! WIN32 */

	if( wants_reconnect && !(sh_is_dc && sh_reads_file) ) {
		dprintf( D_ALWAYS, "Trying to reconnect but you do not have a "
				 "condor_shadow that will work, aborting.\n" );
		noShadowForJob( srec, NO_SHADOW_RECONNECT );
		free(shadow_path);
		return;
	}

	args.AppendArg("condor_shadow");
	if(sh_is_dc) {
		args.AppendArg("-f");
	}

	MyString argbuf;

	// send the location of the transferd the shadow should use for
	// this user. Due to the nasty method of command line argument parsing
	// by the shadow, this should be first on the command line.
	if ( privsep_enabled() && 
			jobNeedsTransferd(job_id->cluster, job_id->proc, universe) )
	{
		TransferDaemon *td = NULL;
		switch( universe ) {
			case CONDOR_UNIVERSE_VANILLA:
			case CONDOR_UNIVERSE_JAVA:
			case CONDOR_UNIVERSE_MPI:
			case CONDOR_UNIVERSE_PARALLEL:
			case CONDOR_UNIVERSE_VM:
				if (! availableTransferd(job_id->cluster, job_id->proc, td) )
				{
					dprintf(D_ALWAYS,
						"Scheduler::spawnShadow() Race condition hit. "
						"Thought transferd was available and it wasn't. "
						"stopping execution of job.\n");

					mark_job_stopped(job_id);
					if( find_shadow_rec(job_id) ) { 
						// we already added the srec to our tables..
						delete_shadow_rec( srec );
						srec = NULL;
					}
				}
				args.AppendArg("--transferd");
				args.AppendArg(td->get_sinful());
				break;

			case CONDOR_UNIVERSE_STANDARD:
				/* no transferd for this universe */
				break;

		default:
			EXCEPT( "StartJobHandler() does not support %d universe jobs",
					universe );
			break;

		}
	}

	if ( sh_reads_file ) {
		if( sh_is_dc ) { 
			argbuf.formatstr("%d.%d",job_id->cluster,job_id->proc);
			args.AppendArg(argbuf.Value());

			if(wants_reconnect) {
				args.AppendArg("--reconnect");
			}

			// pass the public ip/port of the schedd (used w/ reconnect)
			// We need this even if we are not currently in reconnect mode,
			// because the shadow may go into reconnect mode at any time.
			argbuf.formatstr("--schedd=%s", daemonCore->publicNetworkIpAddr());
			args.AppendArg(argbuf.Value());

			if( m_have_xfer_queue_contact ) {
				argbuf.formatstr("--xfer-queue=%s", m_xfer_queue_contact.c_str());
				args.AppendArg(argbuf.Value());
			}

				// pass the private socket ip/port for use just by shadows
			args.AppendArg(MyShadowSockName);
				
			args.AppendArg("-");
		} else {
			args.AppendArg(MyShadowSockName);
			args.AppendArg(mrec->peer);
			args.AppendArg("*");
			args.AppendArg(job_id->cluster);
			args.AppendArg(job_id->proc);
			args.AppendArg("-");
		}
	} else {
			// CRUFT: pre-6.7.0 shadows...
		args.AppendArg(MyShadowSockName);
		args.AppendArg(mrec->peer);
		args.AppendArg(mrec->claimId());
		args.AppendArg(job_id->cluster);
		args.AppendArg(job_id->proc);
	}

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
							   sh_is_dc, sh_reads_file, want_udp );

	free( shadow_path );

	if( ! rval ) {
		mark_job_stopped(job_id);
		if( find_shadow_rec(job_id) ) { 
				// we already added the srec to our tables..
			delete_shadow_rec( srec );
			srec = NULL;
		} else {
				// we didn't call add_shadow_rec(), so we can just do
				// a little bit of clean-up and delete it. 
			RemoveShadowRecFromMrec(srec);
			delete srec;
			srec = NULL;
		}
		return;
	}

	dprintf( D_ALWAYS, "Started shadow for job %d.%d on %s, "
			 "(shadow pid = %d)\n", job_id->cluster, job_id->proc,
			 mrec->description(), srec->pid );

    //time_t now = time(NULL);
    stats.Tick();
    stats.ShadowsStarted += 1;
    stats.ShadowsRunning = numShadows;

	OtherPoolStats.Tick();

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
						 ATTR_LAST_JOB_LEASE_RENEWAL, (int)time(0) );
	}

		// if this is a shadow for an MPI job, we need to tell the
		// dedicated scheduler we finally spawned it so it can update
		// some of its own data structures, too.
	int sendToDS = 0;
	GetAttributeInt(job_id->cluster, job_id->proc, "WantParallelScheduling", &sendToDS);

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

	job_ad->EvalInteger(ATTR_NEXT_JOB_START_DELAY,machine_ad,delay);
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
	if( !RunnableJobQueue.IsEmpty() ) {
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
							   const char* name, bool is_dc, bool wants_pipe,
							   bool want_udp)
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
		MyString usermap;
		p->getUseridMap(usermap);
		if( !usermap.IsEmpty() ) {
			MyString envname;
			envname.formatstr("_%s_USERID_MAP",myDistro->Get());
			extra_env.SetEnv(envname.Value(),usermap.Value());
		}
	}
#endif

		/* Setup the array of fds for stdin, stdout, stderr */
	int* std_fds_p = NULL;
	int std_fds[3];
	int pipe_fds[2];
	pipe_fds[0] = -1;
	pipe_fds[1] = -1;
	if( wants_pipe ) {
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
	} else {
		std_fds[0] = -1;
	}
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
    stats.Tick();
    stats.ShadowsRunning = numShadows;

	OtherPoolStats.Tick();

		// expand $$ stuff and persist expansions so they can be
		// retrieved on restart for reconnect
	job_ad = GetJobAd( job_id->cluster, job_id->proc, true, true );
	if( ! job_ad ) {
			// this might happen if the job is asking for
			// something in $$() that doesn't exist in the machine
			// ad and/or if the machine ad is already gone for some
			// reason.  so, verify the job is still here...
		if( ! GetJobAd(job_id->cluster, job_id->proc, false) ) {
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
	pid = daemonCore->Create_Process( path, args, PRIV_ROOT, rid, 
	                                  is_dc, env, NULL, fip, NULL, 
	                                  std_fds_p, NULL, niceness,
									  NULL, create_process_opts);
	if( pid == FALSE ) {
		MyString arg_string;
		args.GetArgsStringForDisplay(&arg_string);
		dprintf( D_FAILURE|D_ALWAYS, "spawnJobHandlerRaw: "
				 "CreateProcess(%s, %s) failed\n", path, arg_string.Value() );
		if( wants_pipe ) {
			for( int i = 0; i < 2; i++ ) {
				if( pipe_fds[i] >= 0 ) {
					daemonCore->Close_Pipe( pipe_fds[i] );
				}
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
		// do some things with the pipe (if there is one):
	if( wants_pipe ) {
			// 1) close our copy of the read end of the pipe, so we
			// don't leak it.  we have to use DC::Close_Pipe() for
			// this, not just close(), so things work on windoze.
		daemonCore->Close_Pipe( pipe_fds[0] );

			// 2) dump out the job ad to the write end, since the
			// handler is now alive and can read from the pipe.
		ASSERT( job_ad );
		MyString ad_str;
		sPrintAd(ad_str, *job_ad);
		const char* ptr = ad_str.Value();
		int len = ad_str.Length();
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
	}

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


void
Scheduler::noShadowForJob( shadow_rec* srec, NoShadowFailure_t why )
{
	static bool notify_std = true;
	static bool notify_java = true;
	static bool notify_win32 = true;
	static bool notify_dc_vanilla = true;
	static bool notify_old_vanilla = true;

	static char std_reason [] = 
		"No condor_shadow installed that supports standard universe jobs";
	static char java_reason [] = 
		"No condor_shadow installed that supports JAVA jobs";
	static char win32_reason [] = 
		"No condor_shadow installed that supports WIN32 jobs";
	static char dc_vanilla_reason [] = 
		"No condor_shadow installed that supports vanilla jobs on "
		"V6.3.3 or newer resources";
	static char old_vanilla_reason [] = 
		"No condor_shadow installed that supports vanilla jobs on "
		"resources older than V6.3.3";

	PROC_ID job_id;
	char* hold_reason=NULL;
	bool* notify_admin=NULL;

	if( ! srec ) {
		dprintf( D_ALWAYS, "ERROR: Called noShadowForJob with NULL srec!\n" );
		return;
	}
	job_id = srec->job_id;

	switch( why ) {
	case NO_SHADOW_STD:
		hold_reason = std_reason;
		notify_admin = &notify_std;
		break;
	case NO_SHADOW_JAVA:
		hold_reason = java_reason;
		notify_admin = &notify_java;
		break;
	case NO_SHADOW_WIN32:
		hold_reason = win32_reason;
		notify_admin = &notify_win32;
		break;
	case NO_SHADOW_DC_VANILLA:
		hold_reason = dc_vanilla_reason;
		notify_admin = &notify_dc_vanilla;
		break;
	case NO_SHADOW_OLD_VANILLA:
		hold_reason = old_vanilla_reason;
		notify_admin = &notify_old_vanilla;
		break;
	case NO_SHADOW_RECONNECT:
			// this is a special case, since we're not going to email
			// or put the job on hold, we just want to mark it as idle
			// and clean up the mrec and srec...
		break;
	default:
		EXCEPT( "Unknown reason (%d) in Scheduler::noShadowForJob",
				(int)why );
	}

		// real work begins

		// reset a bunch of state in the ClassAd for this job
	mark_job_stopped( &job_id );

		// since we couldn't spawn this shadow, we should remove this
		// shadow record from the match record and delete the shadow
		// rec so we don't leak memory or tie up this match
	RemoveShadowRecFromMrec( srec );
	delete srec;

	if( why == NO_SHADOW_RECONNECT ) {
			// we're done
		return;
	}

		// hold the job, since we won't be able to run it without
		// human intervention
	holdJob( job_id.cluster, job_id.proc, hold_reason, 
			 CONDOR_HOLD_CODE_NoCompatibleShadow, 0,
			 true, true, true, *notify_admin );

		// regardless of what it used to be, we need to record that we
		// no longer want to notify the admin for this kind of error
	*notify_admin = false;
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


shadow_rec*
Scheduler::start_local_universe_job( PROC_ID* job_id )
{
	shadow_rec* srec = NULL;

		// set our CurrentHosts to 1 so we don't consider this job
		// still idle.  we'll actually mark it as status RUNNING once
		// we spawn the starter for it.  unlike other kinds of jobs,
		// local universe jobs don't have to worry about having the
		// status wrong while the job sits in the RunnableJob queue,
		// since we're not negotiating for them at all... 
	SetAttributeInt( job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1 );

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

	srec = add_shadow_rec( 0, job_id, CONDOR_UNIVERSE_LOCAL, NULL, -1 );
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

	if( RunnableJobQueue.enqueue(srec) ) {
		EXCEPT( "Cannot put job into run queue\n" );
	}

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
				 CONDOR_HOLD_CODE_NoCompatibleShadow, 0,
				 false, true, notify_admin, true );
		notify_admin = false;
		return;
	}

	starter_args.AppendArg("condor_starter");
	starter_args.AppendArg("-f");

	starter_args.AppendArg("-job-cluster");
	starter_args.AppendArg(job_id->cluster);

	starter_args.AppendArg("-job-proc");
	starter_args.AppendArg(job_id->proc);

	starter_args.AppendArg("-header");
	MyString header;
	header.formatstr("(%d.%d) ",job_id->cluster,job_id->proc);
	starter_args.AppendArg(header.Value());

	starter_args.AppendArg("-job-input-ad");
	starter_args.AppendArg("-");
	starter_args.AppendArg("-schedd-addr");
	starter_args.AppendArg(MyShadowSockName);

	if(IsFulldebug(D_FULLDEBUG)) {
		MyString argstring;
		starter_args.GetArgsStringForDisplay(&argstring);
		dprintf( D_FULLDEBUG, "About to spawn %s %s\n", 
				 starter_path, argstring.Value() );
	}

	mark_serial_job_running( job_id );

	BeginTransaction();
		// add CLAIM_ID to this job ad so schedd can be authorized by
		// starter by virtue of this shared secret (e.g. for
		// CREATE_JOB_OWNER_SEC_SESSION
	char *public_part = Condor_Crypt_Base::randomHexKey();
	char *private_part = Condor_Crypt_Base::randomHexKey();
	ClaimIdParser cidp(public_part,NULL,private_part);
	SetAttributeString( job_id->cluster, job_id->proc, ATTR_CLAIM_ID, cidp.claimId() );
	free( public_part );
	free( private_part );

	CommitTransaction(NONDURABLE);

	Env starter_env;
	MyString execute_env;
	execute_env.formatstr( "_%s_EXECUTE", myDistro->Get());
	starter_env.SetEnv(execute_env.Value(),LocalUnivExecuteDir);
	
	rval = spawnJobHandlerRaw( srec, starter_path, starter_args,
							   &starter_env, "starter", true, true, true );

	free( starter_path );
	starter_path = NULL;

	if( ! rval ) {
		dprintf( D_ALWAYS|D_FAILURE, "Can't spawn local starter for "
				 "job %d.%d\n", job_id->cluster, job_id->proc );
		mark_job_stopped( job_id );
			// TODO: we're definitely leaking shadow recs in this case
			// (and have been for a while).  must fix ASAP.
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

	MyString dir_name;
	char* tmp = param( "LOCAL_UNIV_EXECUTE" );
	if( ! tmp ) {
		tmp = param( "SPOOL" );		
		if( ! tmp ) {
			EXCEPT( "SPOOL directory not defined in config file!" );
		}
			// If you change this default, make sure you change
			// condor_preen, too, so that it doesn't nuke your
			// directory (assuming you still use SPOOL).
		dir_name.formatstr( "%s%c%s", tmp, DIR_DELIM_CHAR,
						  "local_univ_execute" );
	} else {
		dir_name = tmp;
	}
	free( tmp );
	tmp = NULL;
	if( LocalUnivExecuteDir ) {
		free( LocalUnivExecuteDir );
	}
	LocalUnivExecuteDir = strdup( dir_name.Value() );

	StatInfo exec_statinfo( dir_name.Value() );
	if( ! exec_statinfo.IsDirectory() ) {
			// our umask is going to mess this up for us, so we might
			// as well just do the chmod() seperately, anyway, to
			// ensure we've got it right.  the extra cost is minimal,
			// since we only do this once...
		dprintf( D_FULLDEBUG, "initLocalStarterDir(): %s does not exist, "
				 "calling mkdir()\n", dir_name.Value() );
		if( mkdir(dir_name.Value(), 0777) < 0 ) {
			dprintf( D_ALWAYS, "initLocalStarterDir(): mkdir(%s) failed: "
					 "%s (errno %d)\n", dir_name.Value(), strerror(errno),
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
					 dir_name.Value() );
			Directory exec_dir( &exec_statinfo, PRIV_CONDOR );
			exec_dir.Remove_Entire_Directory();
			first_time = false;
		}
	}

		// we know the directory exists, now make sure the mode is
		// right for our needs...
	if( (mode & desired_mode) != desired_mode ) {
		dprintf( D_FULLDEBUG, "initLocalStarterDir(): "
				 "Changing permission on %s\n", dir_name.Value() );
		if( chmod(dir_name.Value(), (mode|desired_mode)) < 0 ) {
			dprintf( D_ALWAYS, 
					 "initLocalStarterDir(): chmod(%s) failed: "
					 "%s (errno %d)\n", dir_name.Value(), 
					 strerror(errno), errno );
		}
	}
}


shadow_rec*
Scheduler::start_sched_universe_job(PROC_ID* job_id)
{

	MyString a_out_name;
	MyString input;
	MyString output;
	MyString error;
	MyString x509_proxy;
	ArgList args;
	MyString argbuf;
	MyString error_msg;
	MyString owner, iwd;
	MyString domain;
	int		pid;
	StatInfo* filestat;
	bool is_executable;
	ClassAd *userJob = NULL;
	shadow_rec *retval = NULL;
	Env envobject;
	MyString env_error_msg;
    int niceness = 0;
	MyString tmpCwd;
	int inouterr[3];
	bool cannot_open_files = false;
	priv_state priv;
	int i;
	size_t *core_size_ptr = NULL;
	char *ckpt_name = NULL;

	is_executable = false;

	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
		job_id->cluster, job_id->proc );

	userJob = GetJobAd(job_id->cluster,job_id->proc);
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

	// get the nt domain too, if we have it
	GetAttributeString(job_id->cluster, job_id->proc, ATTR_NT_DOMAIN, domain);

	// sanity check to make sure this job isn't going to start as root.
	if (strcasecmp(owner.Value(), "root") == 0 ) {
		dprintf(D_ALWAYS, "Aborting job %d.%d.  Tried to start as root.\n",
			job_id->cluster, job_id->proc);
		goto wrapup;
	}

	// switch to the user in question to make some checks about what I'm 
	// about to execute and then to execute.

	if (! init_user_ids(owner.Value(), domain.Value()) ) {
		MyString tmpstr;
#ifdef WIN32
		tmpstr.formatstr("Bad or missing credential for user: %s", owner.Value());
#else
		tmpstr.formatstr("Unable to switch to user: %s", owner.Value());
#endif
		holdJob(job_id->cluster, job_id->proc, tmpstr.Value(),
				CONDOR_HOLD_CODE_FailedToAccessUserAccount, 0,
				false, false, true, false, false);
		goto wrapup;
	}

	priv = set_user_priv(); // need user's privs...

	// Here we are going to look into the spool directory which contains the
	// user's executables as the user. Be aware that even though the spooled
	// executable probably is owned by Condor in most circumstances, we
	// must ensure the user can at least execute it.

	ckpt_name = gen_ckpt_name(Spool, job_id->cluster, ICKPT, 0);
	a_out_name = ckpt_name;
	free(ckpt_name); ckpt_name = NULL;
	errno = 0;
	filestat = new StatInfo(a_out_name.Value());
	ASSERT(filestat);

	if (filestat->Error() == SIGood) {
		is_executable = filestat->IsExecutable();

		if (!is_executable) {
			// The file is present, but the user cannot execute it? Put the job
			// on hold.
			set_priv( priv );  // back to regular privs...

			holdJob(job_id->cluster, job_id->proc, 
				"Spooled executable is not executable!",
					CONDOR_HOLD_CODE_FailedToCreateProcess, EACCES,
				false, false, true, false, false );

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
		if (a_out_name.Length()==0) {
			set_priv( priv );  // back to regular privs...
			holdJob(job_id->cluster, job_id->proc, 
				"Executable unknown - not specified in job ad!",
					CONDOR_HOLD_CODE_FailedToCreateProcess, ENOENT,
				false, false, true, false, false );
			goto wrapup;
		}

		// If the executable filename isn't an absolute path, prepend
		// the IWD.
		if ( !fullpath( a_out_name.Value() ) ) {
			std::string tmp = a_out_name;
			formatstr( a_out_name, "%s%c%s", iwd.Value(), DIR_DELIM_CHAR, tmp.c_str() );
		}
		
		// Now check, as the user, if we may execute it.
		filestat = new StatInfo(a_out_name.Value());
		is_executable = false;
		if ( filestat ) {
			is_executable = filestat->IsExecutable();
			delete filestat;
		}
		if ( !is_executable ) {
			MyString tmpstr;
			tmpstr.formatstr( "File '%s' is missing or not executable", a_out_name.Value() );
			set_priv( priv );  // back to regular privs...
			holdJob(job_id->cluster, job_id->proc, tmpstr.Value(),
					CONDOR_HOLD_CODE_FailedToCreateProcess, EACCES,
					false, false, true, false, false);
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
	if (chdir(iwd.Value())) {
		dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n", iwd.Value(), strerror(errno));
	}
	
	// now open future in|out|err files
	
#ifdef WIN32
	
	// submit gives us /dev/null regardless of the platform.
	// normally, the starter would handle this translation,
	// but since we're the schedd, we'll have to do it ourselves.
	// At least for now. --stolley
	
	if (nullFile(input.Value())) {
		input = WINDOWS_NULL_FILE;
	}
	if (nullFile(output.Value())) {
		output = WINDOWS_NULL_FILE;
	}
	if (nullFile(error.Value())) {
		error = WINDOWS_NULL_FILE;
	}
	
#endif
	
	if ((inouterr[0] = safe_open_wrapper_follow(input.Value(), O_RDONLY, S_IREAD)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", input.Value(), errno );
		cannot_open_files = true;
	}
	if ((inouterr[1] = safe_open_wrapper_follow(output.Value(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", output.Value(), errno );
		cannot_open_files = true;
	}
	if ((inouterr[2] = safe_open_wrapper_follow(error.Value(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", error.Value(), errno );
		cannot_open_files = true;
	}
	
	//change back to whence we came
	if ( tmpCwd.Length() ) {
		if (chdir(tmpCwd.Value())) {
			dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n",
					tmpCwd.Value(), strerror(errno));
		}
	}
	
	if ( cannot_open_files ) {
	/* I'll close the opened files in the same priv state I opened them
		in just in case the OS cares about such things. */
		if (inouterr[0] >= 0) {
			if (close(inouterr[0]) == -1) {
				dprintf(D_ALWAYS, 
					"Failed to close input file fd for '%s' because [%d %s]\n",
					input.Value(), errno, strerror(errno));
			}
		}
		if (inouterr[1] >= 0) {
			if (close(inouterr[1]) == -1) {
				dprintf(D_ALWAYS,  
					"Failed to close output file fd for '%s' because [%d %s]\n",
					output.Value(), errno, strerror(errno));
			}
		}
		if (inouterr[2] >= 0) {
			if (close(inouterr[2]) == -1) {
				dprintf(D_ALWAYS,  
					"Failed to close error file fd for '%s' because [%d %s]\n",
					output.Value(), errno, strerror(errno));
			}
		}
		set_priv( priv );  // back to regular privs...
		goto wrapup;
	}
	
	set_priv( priv );  // back to regular privs...
	
	if(!envobject.MergeFrom(userJob,&env_error_msg)) {
		dprintf(D_ALWAYS,"Failed to read job environment: %s\n",
				env_error_msg.Value());
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

	// Don't use a_out_name for argv[0], use
	// "condor_scheduniv_exec.cluster.proc" instead. 
	argbuf.formatstr("condor_scheduniv_exec.%d.%d",job_id->cluster,job_id->proc);
	args.AppendArg(argbuf.Value());

	if(!args.AppendArgsFromClassAd(userJob,&error_msg)) {
		dprintf(D_ALWAYS,"Failed to read job arguments: %s\n",
				error_msg.Value());
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
	int core_size_truncated;
	size_t core_size;
	if (GetAttributeInt(job_id->cluster, job_id->proc, 
						   ATTR_CORE_SIZE, &core_size_truncated) == 0) {
		// make the hard limit be what is specified.
		core_size = (size_t)core_size_truncated;
		core_size_ptr = &core_size;
	}
	
	pid = daemonCore->Create_Process( a_out_name.Value(), args, PRIV_USER_FINAL, 
	                                  shadowReaperId, FALSE,
	                                  &envobject, iwd.Value(), NULL, NULL, inouterr,
	                                  NULL, niceness, NULL,
	                                  DCJOBOPT_NO_ENV_INHERIT,
	                                  core_size_ptr );
	
	// now close those open fds - we don't want them here.
	for ( i=0 ; i<3 ; i++ ) {
		if ( close( inouterr[i] ) == -1 ) {
			dprintf ( D_ALWAYS, "FD closing problem, errno = %d\n", errno );
		}
	}

	if ( pid <= 0 ) {
		dprintf ( D_FAILURE|D_ALWAYS, "Create_Process problems!\n" );
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

	retval =  add_shadow_rec( pid, job_id, CONDOR_UNIVERSE_SCHEDULER, NULL, -1 );

wrapup:
	uninit_user_ids();
	if(userJob) {
		FreeJobAd(userJob);
	}
	return retval;
}

void
Scheduler::display_shadow_recs()
{
	struct shadow_rec *r;

	if( !IsFulldebug(D_FULLDEBUG) ) {
		return; // avoid needless work below
	}

	dprintf( D_FULLDEBUG, "\n");
	dprintf( D_FULLDEBUG, "..................\n" );
	dprintf( D_FULLDEBUG, ".. Shadow Recs (%d/%d)\n", numShadows, numMatches );
	shadowsByPid->startIterations();
	while (shadowsByPid->iterate(r) == 1) {

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
    preempted(FALSE),
	conn_fd(-1),
	removed(FALSE),
	isZombie(FALSE),
	is_reconnect(false),
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
	new_rec->is_reconnect = false;
	new_rec->keepClaimAttributes = false;
	
	if (pid) {
		add_shadow_rec(new_rec);
	} else if ( new_rec->match && new_rec->match->pool ) {
		SetAttributeString(new_rec->job_id.cluster, new_rec->job_id.proc,
						   ATTR_REMOTE_POOL, new_rec->match->pool, NONDURABLE);
	}
	return new_rec;
}

void add_shadow_birthdate(int cluster, int proc, bool is_reconnect)
{
	dprintf( D_ALWAYS, "Starting add_shadow_birthdate(%d.%d)\n",
			 cluster, proc );
    time_t now = time(NULL);
	int current_time = (int)now;
	int job_start_date = 0;
	SetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE, current_time);
	if (GetAttributeInt(cluster, proc,
						ATTR_JOB_START_DATE, &job_start_date) < 0) {
		// this is the first time the job has ever run, so set JobStartDate
		SetAttributeInt(cluster, proc, ATTR_JOB_START_DATE, current_time);
        
        int qdate = 0;
        GetAttributeInt(cluster, proc, ATTR_Q_DATE, &qdate);

		scheduler.stats.Tick();
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
			scheduler.OtherPoolStats.Tick();
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

	int job_univ = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &job_univ);


		// Update the job's counter for the number of times a shadow
		// was started (if this job has a shadow at all, that is).
		// For the local universe, "shadow" means local starter.
	int num;
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
		int lastckptTime = 0;
		GetAttributeInt(cluster, proc, ATTR_LAST_CKPT_TIME, &lastckptTime);
		if( lastckptTime > 0 ) {
			// There was a checkpoint.
			// Update restart count from a checkpoint 
			MyString vmtype;
			int num_restarts = 0;
			GetAttributeInt(cluster, proc, ATTR_NUM_RESTARTS, &num_restarts);
			SetAttributeInt(cluster, proc, ATTR_NUM_RESTARTS, ++num_restarts);

			GetAttributeString(cluster, proc, ATTR_JOB_VM_TYPE, vmtype);
			if( strcasecmp(vmtype.Value(), CONDOR_VM_UNIVERSE_VMWARE ) == 0 ) {
				// In vmware vm universe, vmware disk may be 
				// a sparse disk or snapshot disk. So we can't estimate the disk space 
				// in advanace because the sparse disk or snapshot disk will 
				// grow up while running a VM.
				// So we will just add 100MB to disk space.
				int vm_disk_space = 0;
				GetAttributeInt(cluster, proc, ATTR_DISK_USAGE, &vm_disk_space);
				if( vm_disk_space > 0 ) {
					vm_disk_space += 100*1024;
				}
				SetAttributeInt(cluster, proc, ATTR_DISK_USAGE, vm_disk_space);
			}
		}
	}
}

static void
RotateAttributeList( int cluster, int proc, char const *attrname, int start_index, int history_len )
{
	int index;
	for(index=start_index+history_len-1;
		index>start_index;
		index--)
	{
		MyString attr;
		attr.formatstr("%s%d",attrname,index-1);

		char *value=NULL;
		if( GetAttributeExprNew(cluster,proc,attr.Value(),&value) == 0 ) {
			attr.formatstr("%s%d",attrname,index);
			SetAttribute(cluster,proc,attr.Value(),value);
			free( value );
		}
	}
}

void
Scheduler::InsertMachineAttrs( int cluster, int proc, ClassAd *machine_ad )
{
	ASSERT( machine_ad );

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
		RotateAttributeList(cluster,proc,ATTR_LAST_MATCH_LIST_PREFIX,0,list_len);
		std::string attr_buf;
		std::string slot_name;
		machine_ad->LookupString(ATTR_NAME,slot_name);

		formatstr(attr_buf,"%s0",ATTR_LAST_MATCH_LIST_PREFIX);
		SetAttributeString(cluster,proc,attr_buf.c_str(),slot_name.c_str());
	}

		// End of old-style match_list stuff

		// Increment ATTR_NUM_MATCHES
	int num_matches = 0;
	job->LookupInteger(ATTR_NUM_MATCHES,num_matches);
	num_matches++;

	SetAttributeInt(cluster,proc,ATTR_NUM_MATCHES,num_matches);

	SetAttributeInt(cluster,proc,ATTR_LAST_MATCH_TIME,(int)time(0));

		// Now handle JOB_MACHINE_ATTRS

	MyString user_machine_attrs;
	GetAttributeString(cluster,proc,ATTR_JOB_MACHINE_ATTRS,user_machine_attrs);

	int history_len = 1;
	GetAttributeInt(cluster,proc,ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH,&history_len);

	if( m_job_machine_attrs_history_length > history_len ) {
		history_len = m_job_machine_attrs_history_length;
	}

	if( history_len == 0 ) {
		return;
	}

	StringList machine_attrs(user_machine_attrs.Value());

	machine_attrs.create_union( m_job_machine_attrs, true );

	machine_attrs.rewind();
	char const *attr;
	while( (attr=machine_attrs.next()) != NULL ) {
		MyString result_attr;
		result_attr.formatstr("%s%s",ATTR_MACHINE_ATTR_PREFIX,attr);

		RotateAttributeList(cluster,proc,result_attr.Value(),0,history_len);

		classad::Value result;
		if( !machine->EvaluateAttr(attr,result) ) {
			result.SetErrorValue();
		}
		std::string unparsed_result;

		unparser.Unparse(unparsed_result,result);
		result_attr += "0";
		SetAttribute(cluster,proc,result_attr.Value(),unparsed_result.c_str());
	}

	FreeJobAd( job );

	if( !already_in_transaction ) {
		CommitTransaction(NONDURABLE);
	}
}

struct shadow_rec *
Scheduler::add_shadow_rec( shadow_rec* new_rec )
{
	numShadows++;
	if( new_rec->pid ) {
		shadowsByPid->insert(new_rec->pid, new_rec);
	}
	shadowsByProcID->insert(new_rec->job_id, new_rec);

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

		SetAttributeString( cluster, proc, ATTR_CLAIM_ID, mrec->claimId() );
		SetAttributeString( cluster, proc, ATTR_PUBLIC_CLAIM_ID, mrec->publicClaimId() );
		SetAttributeString( cluster, proc, ATTR_STARTD_IP_ADDR, mrec->peer );
		SetAttributeInt( cluster, proc, ATTR_LAST_JOB_LEASE_RENEWAL,
						 (int)time(0) ); 

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

			InsertMachineAttrs(cluster,proc,mrec->my_match_ad);
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
				MyString hostname = get_hostname(addr);
				if (hostname.Length() > 0) {
					SetAttributeString( cluster, proc, ATTR_REMOTE_HOST,
										hostname.Value() );
					dprintf( D_FULLDEBUG, "Used inverse DNS lookup (%s)\n",
							 hostname.Value() );
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
		if( mrec->pool ) {
			SetAttributeString(cluster, proc, ATTR_REMOTE_POOL, mrec->pool);
		}
		if ( mrec->auth_hole_id ) {
			SetAttributeString(cluster,
			                   proc,
			                   ATTR_STARTD_PRINCIPAL,
			                   mrec->auth_hole_id->Value());
		}
	}
	GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &new_rec->universe );
	add_shadow_birthdate( cluster, proc, new_rec->is_reconnect );
	CommitTransaction();
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
	shadowsByPid->insert(new_rec->pid, new_rec);
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
CkptWallClock()
{
	int first_time = 1;
	int current_time = (int)time(0); // bad cast, but ClassAds only know ints
	ClassAd *ad;
	bool began_transaction = false;
	while( (ad = GetNextJob(first_time)) ) {
		first_time = 0;
		int status = IDLE;
		ad->LookupInteger(ATTR_JOB_STATUS, status);
		if (status == RUNNING || status == TRANSFERRING_OUTPUT) {
			int bday = 0;
			ad->LookupInteger(ATTR_SHADOW_BIRTHDATE, bday);
			int run_time = current_time - bday;
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
		CommitTransaction();
	}
}

static void
update_remote_wall_clock(int cluster, int proc)
{
		// update ATTR_JOB_REMOTE_WALL_CLOCK.  note: must do this before
		// we call check_zombie below, since check_zombie is where the
		// job actually gets removed from the queue if job completed or deleted
	int bday = 0;
	GetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE,&bday);
	if (bday) {
		float accum_time = 0;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
		float delta = (float)(time(NULL) - bday);
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

		float slot_weight = 1;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0,&slot_weight);
		float slot_time = 0;
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
	shadow_rec *rec;
	if( shadowsByPid->lookup(pid, rec) == 0 ) {
		delete_shadow_rec( rec );
	} else {
		dprintf( D_ALWAYS, "ERROR: can't find shadow record for pid %d\n",
				 pid );
	}
}


void
Scheduler::delete_shadow_rec( shadow_rec *rec )
{

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
		// Do not remove the ClaimId or RemoteHost if the keepClaimAttributes
		// flag is set. This means that we want this job to reconnect
		// when the schedd comes back online.
		//
	if ( ! rec->keepClaimAttributes ) {
		DeleteAttribute( cluster, proc, ATTR_CLAIM_ID );
		DeleteAttribute( cluster, proc, ATTR_PUBLIC_CLAIM_ID );
		DeleteAttribute( cluster, proc, ATTR_CLAIM_IDS );
		DeleteAttribute( cluster, proc, ATTR_PUBLIC_CLAIM_IDS );
		DeleteAttribute( cluster, proc, ATTR_STARTD_IP_ADDR );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_HOST );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_POOL );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_SLOT_ID );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_VIRTUAL_MACHINE_ID ); // CRUFT
		DeleteAttribute( cluster, proc, ATTR_DELEGATED_PROXY_EXPIRATION );
		DeleteAttribute( cluster, proc, ATTR_TRANSFERRING_INPUT );
		DeleteAttribute( cluster, proc, ATTR_TRANSFERRING_OUTPUT );
		DeleteAttribute( cluster, proc, ATTR_TRANSFER_QUEUED );
	} else {
		dprintf( D_FULLDEBUG, "Job %d.%d has keepClaimAttributes set to true. "
					    "Not removing %s and %s attributes.\n",
					    cluster, proc, ATTR_CLAIM_ID, ATTR_REMOTE_HOST );
	}

	DeleteAttribute( cluster, proc, ATTR_SHADOW_BIRTHDATE );

		// we want to commit all of the above changes before we
		// call check_zombie() since it might do it's own
		// transactions of one sort or another...
		// Nothing written in this transaction requires immediate sync to disk.
	CommitTransaction( NONDURABLE );

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
		shadowsByPid->remove(pid);
	}
	shadowsByProcID->remove(rec->job_id);
	if ( rec->conn_fd != -1 ) {
		close(rec->conn_fd);
	}

	delete rec;
	numShadows -= 1;
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
	int status;
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
					ATTR_ENTERED_CURRENT_STATUS, (int)time(0) );
	SetAttributeInt(job_id->cluster, job_id->proc,
					ATTR_LAST_SUSPENSION_TIME, 0 );


		// If this is a scheduler universe job, increment the
		// job counter for the number of times it started executing.
	int univ = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE, &univ);
	if (univ == CONDOR_UNIVERSE_SCHEDULER) {
		int num;
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
	CommitTransaction( NONDURABLE );
}

/*
** Mark a job as stopped, (Idle).  Do not call directly.  
** Call the non-underscore version below instead.
*/
void
_mark_job_stopped(PROC_ID* job_id)
{
	int		status;
	int		orig_max;
	int		had_orig;

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
						 ATTR_ENTERED_CURRENT_STATUS, (int)time(0) );
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

	int universe = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE,
					&universe);
	if( (universe == CONDOR_UNIVERSE_MPI) || 
		(universe == CONDOR_UNIVERSE_PARALLEL)){
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
		CommitTransaction( NONDURABLE );
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
	shadow_rec *rec;

	dprintf( D_FULLDEBUG, "============ Begin clean_shadow_recs =============\n" );

	shadowsByPid->startIterations();
	while (shadowsByPid->iterate(rec) == 1) {
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
	shadow_rec *rec;
	bool preempt_sched = force_sched_jobs;

	dprintf( D_ALWAYS, "Called preempt( %d )%s%s\n", n, 
			 force_sched_jobs  ? " forcing scheduler univ preemptions" : "",
			 ExitWhenDone ? " for a graceful shutdown" : "" );

	if( n >= numShadows-SchedUniverseJobsRunning-LocalUniverseJobsRunning ) {
			// we only want to start preempting scheduler/local
			// universe jobs once all the shadows have been
			// preempted...
		preempt_sched = true;
	}

	shadowsByPid->startIterations();

	/* Now we loop until we are out of shadows or until we've preempted
	 * `n' shadows.  Note that the behavior of this loop is slightly 
	 * different if ExitWhenDone is True.  If ExitWhenDone is True, we
	 * will always preempt `n' new shadows, used for a progressive shutdown.  If
	 * ExitWhenDone is False, we will preempt n minus the number of shadows we
	 * have previously told to preempt but are still waiting for them to exit.
	 */
	while (shadowsByPid->iterate(rec) == 1 && n > 0) {
		if( is_alive(rec) ) {
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
						// Send a vacate
						//
					if ( ! skip_vacate ) {
						send_vacate( rec->match, DEACTIVATE_CLAIM );
						dprintf( D_ALWAYS, 
								"Sent vacate command to %s for job %d.%d\n",
								rec->match->peer, cluster, proc );
						//
						// Otherwise, send a SIGKILL
						//
					} else {
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
		*/ 
	if( n > 0 && preempt_sched == false ) {
		preempt( n, true );
	}
}

void
send_vacate(match_rec* match,int cmd)
{
	classy_counted_ptr<DCStartd> startd = new DCStartd( match->description(),NULL,match->peer,match->claimId() );
	classy_counted_ptr<DCClaimIdMsg> msg = new DCClaimIdMsg( cmd, match->claimId() );

	msg->setSuccessDebugLevel(D_ALWAYS);
	msg->setTimeout( STARTD_CONTACT_TIMEOUT );
	msg->setSecSessionId( match->secSessionId() );

	if ( !startd->hasUDPCommandPort() || param_boolean("SCHEDD_SEND_VACATE_VIA_TCP",false) ) {
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
	int		status, universe;

	dprintf( D_ALWAYS, "Checking consistency running and runnable jobs\n" );
	BadCluster = -1;
	BadProc = -1;

	for( i=0; i<N_PrioRecs; i++ ) {
		if( (srp=find_shadow_rec(&PrioRec[i].id)) ) {
			BadCluster = srp->job_id.cluster;
			BadProc = srp->job_id.proc;
			universe = srp->universe;
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_STATUS, &status);
			if (status != RUNNING && status != TRANSFERRING_OUTPUT &&
				universe!=CONDOR_UNIVERSE_MPI &&
				universe!=CONDOR_UNIVERSE_PARALLEL) {
				// display_shadow_recs();
				// dprintf(D_ALWAYS,"shadow_prio_recs_consistent(): PrioRec %d - id = %d.%d, owner = %s\n",i,PrioRec[i].id.cluster,PrioRec[i].id.proc,PrioRec[i].owner);
				dprintf( D_ALWAYS, "ERROR: Found a consistency problem!!!\n" );
				return FALSE;
			}
		}
	}
	dprintf( D_ALWAYS, "Tables are consistent\n" );
	return TRUE;
}

/*
  Search the shadow record table for a given job id.  Return a pointer
  to the record if it is found, and NULL otherwise.
*/
struct shadow_rec*
Scheduler::find_shadow_rec(PROC_ID* id)
{
	shadow_rec *rec;

	if (shadowsByProcID->lookup(*id, rec) < 0)
		return NULL;
	return rec;
}

#ifdef CARMI_OPS
struct shadow_rec*
Scheduler::find_shadow_by_cluster( PROC_ID *id )
{
	int		my_cluster;
	shadow_rec	*rec;

	my_cluster = id->cluster;

	shadowsByProcID->startIterations();
	while (shadowsByProcID->iterate(rec) == 1) {
		if( my_cluster == rec->job_id.cluster) {
				return rec;
		}
	}
	return NULL;
}
#endif

/*
  If we have an MPI cluster with > 1 proc, the user
  might condor_rm/_hold/_release one of those procs.
  If so, we need to treat it as if all of the procs
  in the cluster are _rm'd/_held/_released.  This
  copies all the procs from job_ids to expanded_ids,
  adding any sibling mpi procs if needed.
*/
void
Scheduler::expand_mpi_procs(StringList *job_ids, StringList *expanded_ids) {
	job_ids->rewind();
	char *id;
	char buf[40];
	while( (id = job_ids->next())) {
		expanded_ids->append(id);
	}

	job_ids->rewind();
	while( (id = job_ids->next()) ) {
		PROC_ID p = getProcByString(id);
		if( (p.cluster < 0) || (p.proc < 0) ) {
			continue;
		}

		int universe = -1;
		GetAttributeInt(p.cluster, p.proc, ATTR_JOB_UNIVERSE, &universe);
		if ((universe != CONDOR_UNIVERSE_MPI) && (universe != CONDOR_UNIVERSE_PARALLEL))
			continue;
		
		
		int proc_index = 0;
		while( (GetJobAd(p.cluster, proc_index, false) )) {
			snprintf(buf, 40, "%d.%d", p.cluster, proc_index);
			if (! expanded_ids->contains(buf)) {
				expanded_ids->append(buf);
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
	int universe = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe);

	BeginTransaction();

	if( ( universe == CONDOR_UNIVERSE_MPI) || 
		( universe == CONDOR_UNIVERSE_PARALLEL) ) {
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
								 (int)time(0) ); 
				SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								 ATTR_LAST_SUSPENSION_TIME, 0 ); 
			}
			ad = GetNextJob(0);
		}
	} else {
		SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, status);
		SetAttributeInt( cluster, proc, ATTR_ENTERED_CURRENT_STATUS,
						 (int)time(0) );
		SetAttributeInt( cluster, proc,
						 ATTR_LAST_SUSPENSION_TIME, 0 ); 
	}

		// Nothing written in this transaction requires immediate
		// sync to disk.
	CommitTransaction( NONDURABLE );
}

void
Scheduler::child_exit(int pid, int status)
{
	shadow_rec*		srec;
	int				StartJobsFlag=TRUE;
	PROC_ID			job_id;
	bool			srec_was_local_universe = false;
	MyString        claim_id;
		// if we do not start a new job, should we keep the claim?
	bool            keep_claim = false; // by default, no
	bool            srec_keep_claim_attributes;

	srec = FindSrecByPid(pid);
	ASSERT(srec);

		if( srec->match ) {
			if (srec->exit_already_handled && (srec->match->keep_while_idle == 0)) {
				DelMrec( srec->match );
			} else {
				int exitstatus = WEXITSTATUS(status);
				if ((srec->match->keep_while_idle > 0) && ((exitstatus == JOB_EXITED) || (exitstatus == JOB_SHOULD_REMOVE) || (exitstatus == JOB_KILLED))) {
					srec->match->status = M_CLAIMED;
					srec->match->shadowRec = NULL;
					srec->match->idle_timer_deadline = time(NULL) + srec->match->keep_while_idle;
					srec->match = NULL;
				}
			}
		}

	if( srec->exit_already_handled ) {
		delete_shadow_rec( srec );
		return;
	}

	job_id.cluster = srec->job_id.cluster;
	job_id.proc = srec->job_id.proc;

	if( srec->match ) {
		claim_id = srec->match->claimId();
	}
		// store this in case srec is deleted before we need it
	srec_keep_claim_attributes = srec->keepClaimAttributes;

		//
		// If it is a SCHEDULER universe job, then we have a special
		// handler methods to take care of it
		//
	if (IsSchedulerUniverse(srec)) {
 		// scheduler universe process 
		scheduler_univ_job_exit(pid,status,srec);
		delete_shadow_rec( pid );
			// even though this will get set correctly in
			// count_jobs(), try to keep it accurate here, too.  
		if( SchedUniverseJobsRunning > 0 ) {
			SchedUniverseJobsRunning--;
		}
	} else if (srec) {
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
			}
			else
			{
				dprintf(D_ALWAYS, "Warning: unexpected count for  local universe jobs: %d\n", LocalUniverseJobsRunning);
			}
		} else {
				// A real shadow
			name = "Shadow";
		}
		if ( daemonCore->Was_Not_Responding(pid) ) {
			// this shadow was killed by daemon core because it was hung.
			// make the schedd treat this like a Shadow Exception so job
			// just goes back into the queue as idle, but if it happens
			// to many times we relinquish the match.
			dprintf( D_ALWAYS,
					 "%s pid %d successfully killed because it was hung.\n",
					 name, pid );
			status = JOB_EXCEPTION;
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
			dprintf( D_FAILURE|D_ALWAYS, "%s pid %d died with %s\n",
					 name, pid, daemonCore->GetExceptionString(status) );

				// If the shadow was killed (i.e. by this schedd) and
				// we are preserving the claim for reconnect, then
				// do not delete the claim.
			 keep_claim = srec_keep_claim_attributes;
		}
		
			// We always want to delete the shadow record regardless 
			// of how the job exited
		delete_shadow_rec( pid );

	} else {
			// Hmm -- doesn't seem like we can ever get here, given 
			// that we deference srec before the if... wenger 2011-02-09
			//
			// There wasn't a shadow record, so that agent dies after
			// deleting match. We want to make sure that we don't
			// call to start more jobs
			// 
		StartJobsFlag=FALSE;
	 }  // big if..else if...

		//
		// If the job was a local universe job, we will want to
		// call count on it so that it can be marked idle again
		// if need be.
		//
	if ( srec_was_local_universe == true ) {
		ClassAd *job_ad = GetJobAd( job_id.cluster, job_id.proc );
		count( job_ad );
	}

		// If we're not trying to shutdown, now that either an agent
		// or a shadow (or both) have exited, we should try to
		// start another job.
	if( ! ExitWhenDone && StartJobsFlag ) {
		if( !claim_id.IsEmpty() ) {
				// Try finding a new job for this claim.
			match_rec *mrec = scheduler.FindMrecByClaimID( claim_id.Value() );
			if( mrec ) {
				this->StartJob( mrec );
			}
		}
		else {
			this->ExpediteStartJobs();
		}
	}
	else if( !keep_claim ) {
		if( !claim_id.IsEmpty() ) {
			DelMrec( claim_id.Value() );
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
	int q_status;
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
	time_t updateTime = time(NULL);
	stats.Tick(updateTime);
	stats.JobsSubmitted = GetJobQueuedCount();

	MyString other;
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
	int universe = 0;
	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_UNIVERSE, &universe);
	int job_image_size = 0;
	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_IMAGE_SIZE, &job_image_size);
	int job_start_date = 0;
	int job_running_time = 0;
	if (0 == GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_CURRENT_START_DATE, &job_start_date))
		job_running_time = (updateTime - job_start_date);


		// We get the name of the daemon that had a problem for 
		// nice log messages...
	MyString daemon_name;
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
	switch( exit_code ) {
		case JOB_NO_MEM:
			this->swap_space_exhausted();
			stats.JobsShadowNoMemory += 1;
			OTHER.JobsShadowNoMemory += 1;

		case JOB_EXEC_FAILED:
				//
				// The calling function will make sure that
				// we don't try to start new jobs
				//
			stats.JobsExecFailed += 1;
			OTHER.JobsExecFailed += 1;
			break;

		case JOB_CKPTED:
		case JOB_NOT_CKPTED:
				// no break, fall through and do the action
		// case JOB_SHOULD_REQUEUE:
				// we can't have the same value twice in our
				// switch, so we can't really have a valid case
				// for this, but it's the same number as
				// JOB_NOT_CKPTED, so we're safe.
		case JOB_NOT_STARTED:
			if( srec != NULL && !srec->removed && srec->match ) {
				DelMrec(srec->match);
			}
            switch (exit_code) {
               case JOB_CKPTED:
                  stats.JobsCheckpointed += 1;
                  OTHER.JobsCheckpointed += 1;
                  is_goodput = true;
                  break;
               case JOB_SHOULD_REQUEUE:
               //case JOB_NOT_CKPTED: for CONDOR_UNIVERSE_STANDARD
                  stats.JobsShouldRequeue += 1;
                  OTHER.JobsShouldRequeue += 1;
                  // for standard universe this is actually case JOB_NOT_CKPTED
                  if (CONDOR_UNIVERSE_STANDARD == universe) {
                     is_badput = true;
                  } else {
                     is_goodput = true;
                  }
                  break;
               case JOB_NOT_STARTED:
                  stats.JobsNotStarted += 1;
                  OTHER.JobsNotStarted += 1;
                  break;
               }
			break;

		case JOB_SHADOW_USAGE:
			EXCEPT( "%s exited with incorrect usage!\n", daemon_name.Value() );
			break;

		case JOB_BAD_STATUS:
			EXCEPT( "%s exited because job status != RUNNING", daemon_name.Value() );
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
		case JOB_EXITED:
			dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
			stats.JobsExitedNormally += 1;
			OTHER.JobsExitedNormally += 1;
			stats.JobsCompleted += 1;
			OTHER.JobsCompleted += 1;
			is_goodput = true;
			// no break, fall through and do the action
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
			MyString _error("\"Job missed deferred execution time\"");
			if ( SetAttribute( job_id.cluster, job_id.proc,
					  		  ATTR_HOLD_REASON, _error.Value() ) < 0 ) {
				dprintf( D_ALWAYS, "WARNING: Failed to set %s to %s for "
						 "job %d.%d\n", ATTR_HOLD_REASON, _error.Value(), 
						 job_id.cluster, job_id.proc );
			}
			if ( SetAttributeInt(job_id.cluster, job_id.proc,
								 ATTR_HOLD_REASON_CODE,
								 CONDOR_HOLD_CODE_MissedDeferredExecutionTime)
				 < 0 ) {
				dprintf( D_ALWAYS, "WARNING: Failed to set %s to %d for "
						 "job %d.%d\n", ATTR_HOLD_REASON_CODE,
						 CONDOR_HOLD_CODE_MissedDeferredExecutionTime,
						 job_id.cluster, job_id.proc );
			}
			dprintf( D_ALWAYS, "Job %d.%d missed its deferred execution time\n",
							job_id.cluster, job_id.proc );
		}
				// no break, fall through and do the action

		case JOB_SHOULD_HOLD: {
			dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
					 job_id.cluster, job_id.proc );
				// Regardless of the state that the job currently
				// is in, we'll put it on HOLD
			set_job_status( job_id.cluster, job_id.proc, HELD );
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
					 daemon_name.Value() );
			stats.JobsDebugLogError += 1;
			OTHER.JobsDebugLogError += 1;
			// We don't want to break, we want to fall through 
			// and treat this like a shadow exception for now.

		case JOB_EXCEPTION:
			if ( exit_code == JOB_EXCEPTION ){
				dprintf( D_ALWAYS,
						 "ERROR: %s exited with job exception code!\n",
						 daemon_name.Value() );
			}
			// We don't want to break, we want to fall through 
			// and treat this like a shadow exception for now.

		default:
				//
				// The default case is now a shadow exception in case ANYTHING
				// goes wrong with the shadow exit status
				//
			if ( ( exit_code != DPRINTF_ERROR ) &&
				 ( exit_code != JOB_EXCEPTION ) ) {
				dprintf( D_ALWAYS,
						 "ERROR: %s exited with unknown value %d!\n",
						 daemon_name.Value(), exit_code );
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
		int job_pre_exec_time = 0;  // unless we see job_start_exec_date
		int job_post_exec_time = 0;
		int job_executing_time = 0;
		// this time is set in the shadow (remoteresource::beginExecution) so we don't need to worry
		// if we are talking to a shadow that supports it. the shadow and schedd should be from the same build.
		int job_start_exec_date = 0; 
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
		int job_start_xfer_out_date = 0;
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
	MyString reason;
	int reason_code;
	int reason_subcode;
	ClassAd * job_ad = GetJobAd( job_id.cluster, job_id.proc );
	ASSERT( job_ad ); // No job ad?
	{
		UserPolicy policy;
		policy.Init(job_ad);
		action = policy.AnalyzePolicy(PERIODIC_THEN_EXIT);
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
			WriteRequeueToUserLog(job_id, status, reason.Value());
			break;

		case HOLD_IN_QUEUE:
				// passing "false" to write_user_log, as
				// delete_shadow_rec will do that later
			holdJob(job_id.cluster, job_id.proc, reason.Value(),
					reason_code, reason_subcode,
				true,false,false,false,false,false);
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
				 job_id.cluster, job_id.proc, reason.Value());
			holdJob(job_id.cluster, job_id.proc, reason.Value(),
					reason_code, reason_subcode,
				true,false,false,false,true);
			break;

		default:
			dprintf( D_ALWAYS,
				"(%d.%d) User policy requested unknown action of %d. "
				"Putting job on hold. (Reason: %s)\n",
				 job_id.cluster, job_id.proc, action, reason.Value());
			MyString reason2 = "Unknown action (";
			reason2 += action;
			reason2 += ") ";
			reason2 += reason;
			holdJob(job_id.cluster, job_id.proc, reason2.Value(),
					CONDOR_HOLD_CODE_JobPolicyUndefined, 0,
				true,false,false,false,true);
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
 
	int	  status;
	
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
	case COMPLETED:
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

#ifdef WIN32
	// On Win32, we don't deal with the old ckpt server, so we stub it,
	// thus we do not have to link in the ckpt_server_api.
#include "directory.h"
int 
RemoveLocalOrRemoteFile(const char *, const char *, const char *)
{
	return 0;
}
int
SetCkptServerHost(const char *)
{
	return 0;
}
#endif // of ifdef WIN32

void
cleanup_ckpt_files(int cluster, int proc, const char *owner)
{
    MyString	ckpt_name_buf;
	char const *ckpt_name;
	MyString	owner_buf;
	MyString	server;
	int		universe = CONDOR_UNIVERSE_STANDARD;

		/* In order to remove from the checkpoint server, we need to know
		 * the owner's name.  If not passed in, look it up now.
  		 */
	if ( owner == NULL ) {
		if ( GetAttributeString(cluster,proc,ATTR_OWNER,owner_buf) < 0 ) {
			dprintf(D_ALWAYS,"ERROR: cleanup_ckpt_files(): cannot determine owner for job %d.%d\n",cluster,proc);
		} else {
			owner = owner_buf.Value();
		}
	}

		/* Remove any checkpoint files.  If for some reason we do 
		 * not know the owner, don't bother sending to the ckpt
		 * server.
		 */
	GetAttributeInt(cluster,proc,ATTR_JOB_UNIVERSE,&universe);
	if ( universe == CONDOR_UNIVERSE_STANDARD && owner ) {
		char *ckpt_name_mem = gen_ckpt_name(Spool,cluster,proc,0);
		ckpt_name_buf = ckpt_name_mem;
		free(ckpt_name_mem); ckpt_name_mem = NULL;
		ckpt_name = ckpt_name_buf.Value();

		if (GetAttributeString(cluster, proc, ATTR_LAST_CKPT_SERVER,
							   server) == 0) {
			SetCkptServerHost(server.Value());
		} else {
			SetCkptServerHost(NULL); // no ckpt on ckpt server
		}

		RemoveLocalOrRemoteFile(owner,Name,ckpt_name);

		ckpt_name_buf += ".tmp";
		ckpt_name = ckpt_name_buf.Value();

		RemoveLocalOrRemoteFile(owner,Name,ckpt_name);
	}

	ClassAd * ad = GetJobAd(cluster, proc);
	if(ad) {
		SpooledJobFiles::removeJobSpoolDirectory(ad);
		FreeJobAd(ad);
	}
}


unsigned int pidHash(const int &pid)
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

		// set defaults for rounding attributes for autoclustering
		// only set these values if nothing is specified in condor_config.
	MyString tmpstr;
	tmpstr.formatstr("SCHEDD_ROUND_ATTR_%s",ATTR_EXECUTABLE_SIZE);
	tmp = param(tmpstr.Value());
	if ( !tmp ) {
		config_insert(tmpstr.Value(),"25%");	// round up to 25% of magnitude
	} else {
		free(tmp);
	}
	tmpstr.formatstr("SCHEDD_ROUND_ATTR_%s",ATTR_IMAGE_SIZE);
	tmp = param(tmpstr.Value());
	if ( !tmp ) {
		config_insert(tmpstr.Value(),"25%");	// round up to 25% of magnitude
	} else {
		free(tmp);
	}
	tmpstr.formatstr("SCHEDD_ROUND_ATTR_%s",ATTR_DISK_USAGE);
	tmp = param(tmpstr.Value());
	if ( !tmp ) {
		config_insert(tmpstr.Value(),"25%");	// round up to 25% of magnitude
	} else {
		free(tmp);
	}
	// round ATTR_NUM_CKPTS because our default expressions
	// in the startd for ATTR_IS_VALID_CHECKPOINT_PLATFORM references
	// it (thus by default it is significant), and further references it
	// essentially as a bool.  so by default, lets round it.
	tmpstr.formatstr("SCHEDD_ROUND_ATTR_%s",ATTR_NUM_CKPTS);
	tmp = param(tmpstr.Value());
	if ( !tmp ) {
		config_insert(tmpstr.Value(),"4");	// round up to next 10000
	} else {
		free(tmp);
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

	if( Mail ) free( Mail );
	if( ! (Mail=param("MAIL")) ) {
		EXCEPT( "MAIL not specified in config file\n" );
	}	

		// UidDomain will always be defined, since config() will put
		// in my_full_hostname() if it's not defined in the file.
		// See if the value of this changes, since if so, we've got
		// work to do...
	char* oldUidDomain = UidDomain;
	UidDomain = param( "UID_DOMAIN" );
	if( oldUidDomain ) {
			// We had an old version, so see if we have a new value
		if( strcmp(UidDomain,oldUidDomain) ) {
				// They're different!  So, now we've got to go through
				// the whole job queue and replace ATTR_USER for all
				// the ads with a new value that's got the new
				// UidDomain in it.  Luckily, we shouldn't have to do
				// this very often. :)
			dprintf( D_FULLDEBUG, "UID_DOMAIN has changed.  "
					 "Inserting new ATTR_USER into all classads.\n" );
			WalkJobQueue((int(*)(ClassAd *)) fixAttrUser );
			dirtyJobQueue();
		}
			// we're done with the old version, so don't leak memory 
		free( oldUidDomain );
	}

		////////////////////////////////////////////////////////////////////
		// Grab all the optional parameters from the config file.
		////////////////////////////////////////////////////////////////////

	if( schedd_name_in_config ) {
		tmp = param( "SCHEDD_NAME" );
		delete [] Name;
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

	dprintf( D_FULLDEBUG, "Using name: %s\n", Name );

		// Put SCHEDD_NAME in the environment, so the shadow can use
		// it.  (Since the schedd's name may have been set on the
		// command line, the shadow can't compute the schedd's name on
		// its own.)  
		// Only put in in the env if it is not already there, so 
		// we don't leak memory without reason.		

#define SCHEDD_NAME_LHS "SCHEDD_NAME"
	
	if ( NameInEnv == NULL || strcmp(NameInEnv,Name) ) {
		free( NameInEnv );
		NameInEnv = strdup( Name );
		if ( SetEnv( SCHEDD_NAME_LHS, NameInEnv ) == FALSE ) {
			dprintf(D_ALWAYS, "SetEnv(%s=%s) failed!\n", SCHEDD_NAME_LHS,
					NameInEnv);
		}
	}


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
			WalkJobQueue((int(*)(ClassAd*))::updateSchedDInterval);
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
		// We don't use SHADOW_SIZE_ESTIMATE here, because until 7.4,
		// that was explicitly set to 1800k in the default config file.
	int default_max_jobs_running = sysapi_phys_memory_raw_no_param()*4096/400;

		// Under Linux (not sure about other OSes), the default TCP
		// ephemeral port range is 32768-61000.  Each shadow needs 2
		// ports, sometimes 3, and depending on how fast shadows are
		// finishing, there will be some ports in CLOSE_WAIT, so the
		// following is a conservative upper bound on how many shadows
		// we can run.  Would be nice to check the ephemeral port
		// range directly.
	if( default_max_jobs_running > 10000) {
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

		// Limit number of simultaenous connection attempts to startds.
		// This avoids the schedd getting so busy authenticating with
		// startds that it can't keep up with shadows.
		// note: the special value 0 means 'unlimited'
	max_pending_startd_contacts = param_integer( "MAX_PENDING_STARTD_CONTACTS", 0, 0 );

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
#if defined (ADD_TARGET_SCOPING)
		ExprTree *tmp_expr2 = AddTargetRefs( tmp_expr, TargetJobAttrs );
		this->StartLocalUniverse = strdup( ExprTreeToString( tmp_expr2 ) );
		delete tmp_expr2;
#else
		this->StartLocalUniverse = tmp;
		tmp = NULL;
#endif
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
#if defined (ADD_TARGET_SCOPING)
		ExprTree *tmp_expr2 = AddTargetRefs( tmp_expr, TargetJobAttrs );
		this->StartSchedulerUniverse = strdup( ExprTreeToString( tmp_expr2 ) );
		delete tmp_expr2;
#else
		this->StartSchedulerUniverse = tmp;
		tmp = NULL;
#endif
		delete tmp_expr;
	} else {
		// Default Expression
		this->StartSchedulerUniverse = strdup( "TotalSchedulerJobsRunning < 200" );
		dprintf( D_FULLDEBUG, "Using default expression for "
				 "START_SCHEDULER_UNIVERSE: %s\n", this->StartSchedulerUniverse );
	}
	free( tmp );

	MaxJobsSubmitted = param_integer("MAX_JOBS_SUBMITTED",INT_MAX);
	
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

	/* Initialize the hash tables to size MaxJobsRunning * 1.2 */
		// Someday, we might want to actually resize these hashtables
		// on reconfig if MaxJobsRunning changes size, but we don't
		// have the code for that and it's not too important.
	if (matches == NULL) {
	matches = new HashTable <HashKey, match_rec *> ((int)(MaxJobsRunning*1.2),
													hashFunction);
	matchesByJobID =
		new HashTable<PROC_ID, match_rec *>((int)(MaxJobsRunning*1.2),
											hashFuncPROC_ID,
											rejectDuplicateKeys);
	shadowsByPid = new HashTable <int, shadow_rec *>((int)(MaxJobsRunning*1.2),
													  pidHash);
	shadowsByProcID =
		new HashTable<PROC_ID, shadow_rec *>((int)(MaxJobsRunning*1.2),
											 hashFuncPROC_ID);
	resourcesByProcID = 
		new HashTable<PROC_ID, ClassAd *>((int)(MaxJobsRunning*1.2),
											 hashFuncPROC_ID,
											 updateDuplicateKeys);
	}

	if ( spoolJobFileWorkers == NULL ) {
		spoolJobFileWorkers = 
			new HashTable <int, ExtArray<PROC_ID> *>(5, pidHash);
	}

	char *flock_collector_hosts, *flock_negotiator_hosts;
	flock_collector_hosts = param( "FLOCK_COLLECTOR_HOSTS" );
	flock_negotiator_hosts = param( "FLOCK_NEGOTIATOR_HOSTS" );

	if( flock_collector_hosts ) {
		if( FlockCollectors ) {
			delete FlockCollectors;
		}
		FlockCollectors = new DaemonList();
		FlockCollectors->init( DT_COLLECTOR, flock_collector_hosts );
		MaxFlockLevel = FlockCollectors->number();

		if( FlockNegotiators ) {
			delete FlockNegotiators;
		}
		FlockNegotiators = new DaemonList();
		FlockNegotiators->init( DT_NEGOTIATOR, flock_negotiator_hosts, flock_collector_hosts );
		if( FlockCollectors->number() != FlockNegotiators->number() ) {
			dprintf(D_ALWAYS, "FLOCK_COLLECTOR_HOSTS and "
					"FLOCK_NEGOTIATOR_HOSTS lists are not the same size."
					"Flocking disabled.\n");
			MaxFlockLevel = 0;
		}
	}
	if (flock_collector_hosts) free(flock_collector_hosts);
	if (flock_negotiator_hosts) free(flock_negotiator_hosts);

	// fetch all params that start with SCHEDD_COLLECT_STATS_FOR_ and
	// use them to define other scheduler stats pools.  the value of this
	// param should be a classad expression that evaluates agains the job ad
	// to a boolean.
	//
	{
		Regex re; int err = 0; const char * pszMsg = 0;
		ASSERT(re.compile("schedd_collect_stats_(by|for)_(.+)", &pszMsg, &err, PCRE_CASELESS));
		
		OtherPoolStats.DisableAll();

		ExtArray<const char *> names;
		if (param_names_matching(re, names)) {

			for (int ii = 0; ii < names.length(); ++ii) {

				//dprintf(D_FULLDEBUG, "Found %s\n", names[ii]);
				const MyString name = names[ii];
				char * filter = param(names[ii]);
				if ( ! filter) {
					dprintf(D_ALWAYS, "Ignoring param '%s' : value is empty\n", names[ii]);
					continue;
				}

				// the pool prefix will be the first submatch of the regex of the param name.
				// unfortunately it's been lowercased by the time we get here, so we can't
				// let the user choose the case, just capitalize it and use it as the prefix
				ExtArray<MyString> groups(3);
				if (re.match(name, &groups)) {
					MyString byorfor = groups[1]; // this will by "by" or "for"
					MyString other = groups[2]; // this will be lowercase
					if (isdigit(other[0])) {
						// can't start atributes with a digit, start with _ instead
						other.formatstr("_%s", groups[2].Value());
					} else {
						other.setChar(0, toupper(other[0])); // capitalize it.
					}

					// for 'by' type stats, we also allow an expiration.
					time_t lifetime = 0;
					const int one_week = 60*60*24*7; // 60sec*60min*24hr*7day
					bool by = (MATCH == strcasecmp(byorfor.Value(), "by"));
					if (by) {
						MyString expires_name;
						expires_name.formatstr("schedd_expire_stats_by_%s", other.Value());
						lifetime = (time_t)param_integer(expires_name.Value(), one_week);
					}

					dprintf(D_FULLDEBUG, "Collecting stats %s '%s' life=%" PRId64 " trigger is %s\n", 
					        byorfor.Value(), other.Value(), (int64_t)lifetime, filter);
					OtherPoolStats.Enable(other.Value(), filter, by, lifetime);
				}
				free(filter);
			}
		}
		names.truncate(0);

		OtherPoolStats.RemoveDisabled();
		OtherPoolStats.Reconfig();
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
												(int)( MaxJobsRunning * 1.2 ),
												hashFuncPROC_ID,
												updateDuplicateKeys );
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

	MaxExceptions = param_integer("MAX_SHADOW_EXCEPTIONS", 5);

	PeriodicExprInterval.setMinInterval( param_integer("PERIODIC_EXPR_INTERVAL", 60) );

	PeriodicExprInterval.setMaxInterval( param_integer("MAX_PERIODIC_EXPR_INTERVAL", 1200) );

	PeriodicExprInterval.setTimeslice( param_double("PERIODIC_EXPR_TIMESLICE", 0.01,0,1) );

	RequestClaimTimeout = param_integer("REQUEST_CLAIM_TIMEOUT",60*30);

#ifdef HAVE_EXT_POSTGRESQL

	/* See if QUILL is configured for this schedd */
	if (param_boolean("QUILL_ENABLED", false) == false) {
		quill_enabled = FALSE;
	} else {
		quill_enabled = TRUE;
	}

	/* only force definition of these attributes if I have to */
	if (quill_enabled == TRUE) {

		/* set up whether or not the quill daemon is remotely queryable */
		if (param_boolean("QUILL_IS_REMOTELY_QUERYABLE", true) == true) {
			quill_is_remotely_queryable = TRUE;
		} else {
			quill_is_remotely_queryable = FALSE;
		}

		/* set up a required quill_name */
		tmp = param("QUILL_NAME");
		if (!tmp) {
			EXCEPT( "No QUILL_NAME specified in config file" );
		}
		if (quill_name != NULL) {
			free(quill_name);
			quill_name = NULL;
		}
		quill_name = strdup(tmp);
		free(tmp);
		tmp = NULL;

		/* set up a required database ip address quill needs to use */
		tmp = param("QUILL_DB_IP_ADDR");
		if (!tmp) {
			EXCEPT( "No QUILL_DB_IP_ADDR specified in config file" );
		}
		if (quill_db_ip_addr != NULL) {
			free(quill_db_ip_addr);
			quill_db_ip_addr = NULL;
		}
		quill_db_ip_addr = strdup(tmp);
		free(tmp);
		tmp = NULL;

		/* Set up the name of the required database ip address */
		tmp = param("QUILL_DB_NAME");
		if (!tmp) {
			EXCEPT( "No QUILL_DB_NAME specified in config file" );
		}
		if (quill_db_name != NULL) {
			free(quill_db_name);
			quill_db_name = NULL;
		}
		quill_db_name = strdup(tmp);
		free(tmp);
		tmp = NULL;

		/* learn the required password field to access the database */
		tmp = param("QUILL_DB_QUERY_PASSWORD");
		if (!tmp) {
			EXCEPT( "No QUILL_DB_QUERY_PASSWORD specified in config file" );
		}
		if (quill_db_query_password != NULL) {
			free(quill_db_query_password);
			quill_db_query_password = NULL;
		}
		quill_db_query_password = strdup(tmp);
		free(tmp);
		tmp = NULL;
	}
#endif

	int int_val = param_integer( "JOB_IS_FINISHED_INTERVAL", 0, 0 );
	job_is_finished_queue.setPeriod( int_val );	

	JobStopDelay = param_integer( "JOB_STOP_DELAY", 0, 0 );
	stop_job_queue.setPeriod( JobStopDelay );

	JobStopCount = param_integer( "JOB_STOP_COUNT", 1, 1 );
	stop_job_queue.setCountPerInterval( JobStopCount );

		////////////////////////////////////////////////////////////////////
		// Initialize the queue managment code
		////////////////////////////////////////////////////////////////////

	InitQmgmt();


		//////////////////////////////////////////////////////////////
		// Initialize our classad
		//////////////////////////////////////////////////////////////
	if( m_adBase ) delete m_adBase;
	if( m_adSchedd ) delete m_adSchedd;
	m_adBase = new ClassAd();

    // first put attributes into the Base ad that we want to
    // share between the Scheduler AD and the Submitter Ad
    //
	SetTargetTypeName(*m_adBase, "");
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
    m_adSchedd = new ClassAd(*m_adBase);
	SetMyTypeName(*m_adSchedd, SCHEDD_ADTYPE);
	m_adSchedd->Assign(ATTR_NAME, Name);

	// This is foul, but a SCHEDD_ADTYPE _MUST_ have a NUM_USERS attribute
	// (see condor_classad/classad.C
	// Since we don't know how many there are yet, just say 0, it will get
	// fixed in count_job() -Erik 12/18/2006
	m_adSchedd->Assign(ATTR_NUM_USERS, 0);

#ifdef HAVE_EXT_POSTGRESQL
	// Put the quill stuff into the add as well
	if (quill_enabled == TRUE) {
		m_adSchedd->Assign( ATTR_QUILL_ENABLED, true ); 

		m_adSchedd->Assign( ATTR_QUILL_NAME, quill_name ); 

		m_adSchedd->Assign( ATTR_QUILL_DB_NAME, quill_db_name ); 

		MyString expr;
		expr.formatstr( "%s = \"<%s>\"", ATTR_QUILL_DB_IP_ADDR,
					  quill_db_ip_addr ); 
		m_adSchedd->Insert( expr.Value() );

		m_adSchedd->Assign( ATTR_QUILL_DB_QUERY_PASSWORD, quill_db_query_password); 

		m_adSchedd->Assign( ATTR_QUILL_IS_REMOTELY_QUERYABLE, 
					  quill_is_remotely_queryable == TRUE ? true : false );

	} else {

		m_adSchedd->Assign( ATTR_QUILL_ENABLED, false );
	}
#endif

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
		// Note: BindAnyCommandPort() is in daemon core
		if ( !BindAnyCommandPort(shadowCommandrsock,shadowCommandssock)) {
			EXCEPT("Failed to bind Shadow Command socket");
		}
		if ( !shadowCommandrsock->listen() ) {
			EXCEPT("Failed to post a listen on Shadow Command socket");
		}
		daemonCore->Register_Command_Socket( (Stream*)shadowCommandrsock );
		daemonCore->Register_Command_Socket( (Stream*)shadowCommandssock );

		MyShadowSockName = strdup( shadowCommandrsock->get_sinful() );

		sent_shadow_failure_email = FALSE;
	}
		
		// initialize our ShadowMgr, too.
	shadow_mgr.init();

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
		MyString temp;
		temp.formatstr("string(%s)",expr);
		free(expr);
		expr = temp.StrDup();
		ParseClassAdRvalExpr(temp.Value(),m_parsed_gridman_selection_expr);	
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

	MyString job_machine_attrs_str;
	param(job_machine_attrs_str,"SYSTEM_JOB_MACHINE_ATTRS");
	m_job_machine_attrs.clearAll();
	m_job_machine_attrs.initializeFromString( job_machine_attrs_str.Value() );

	m_job_machine_attrs_history_length = param_integer("SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH",1,0);

	first_time_in_init = false;
}

void
Scheduler::Register()
{
	 // message handlers for schedd commands
	 daemonCore->Register_CommandWithPayload( NEGOTIATE_WITH_SIGATTRS, 
		 "NEGOTIATE_WITH_SIGATTRS", 
		 (CommandHandlercpp)&Scheduler::negotiate, "negotiate", 
		 this, NEGOTIATOR );
	 daemonCore->Register_CommandWithPayload( NEGOTIATE, 
		 "NEGOTIATE", 
		 (CommandHandlercpp)&Scheduler::negotiate, "negotiate", 
		 this, NEGOTIATOR );
	 daemonCore->Register_Command( RESCHEDULE, "RESCHEDULE", 
			(CommandHandlercpp)&Scheduler::reschedule_negotiator, 
			"reschedule_negotiator", this, WRITE);
	 daemonCore->Register_CommandWithPayload(ACT_ON_JOBS, "ACT_ON_JOBS", 
			(CommandHandlercpp)&Scheduler::actOnJobs, 
			"actOnJobs", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(SPOOL_JOB_FILES, "SPOOL_JOB_FILES", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(TRANSFER_DATA, "TRANSFER_DATA", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(SPOOL_JOB_FILES_WITH_PERMS,
			"SPOOL_JOB_FILES_WITH_PERMS", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(TRANSFER_DATA_WITH_PERMS,
			"TRANSFER_DATA_WITH_PERMS", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(UPDATE_GSI_CRED,"UPDATE_GSI_CRED",
			(CommandHandlercpp)&Scheduler::updateGSICred,
			"updateGSICred", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(DELEGATE_GSI_CRED_SCHEDD,
			"DELEGATE_GSI_CRED_SCHEDD",
			(CommandHandlercpp)&Scheduler::updateGSICred,
			"updateGSICred", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(REQUEST_SANDBOX_LOCATION,
			"REQUEST_SANDBOX_LOCATION",
			(CommandHandlercpp)&Scheduler::requestSandboxLocation,
			"requestSandboxLocation", this, WRITE, D_COMMAND,
			true /*force authentication*/);
	 daemonCore->Register_CommandWithPayload(RECYCLE_SHADOW,
			"RECYCLE_SHADOW",
			(CommandHandlercpp)&Scheduler::RecycleShadow,
			"RecycleShadow", this, DAEMON, D_COMMAND,
			true /*force authentication*/);

		 // Commands used by the startd are registered at READ
		 // level rather than something like DAEMON or WRITE in order
		 // to reduce the level of authority that the schedd must
		 // grant the startd.  In order for these commands to
		 // succeed, the startd must present the secret claim id,
		 // so it is deemed safe to open these commands up to READ
		 // access.
	daemonCore->Register_CommandWithPayload(RELEASE_CLAIM, "RELEASE_CLAIM", 
			(CommandHandlercpp)&Scheduler::release_claim, 
			"release_claim", this, READ);
	daemonCore->Register_CommandWithPayload( ALIVE, "ALIVE", 
			(CommandHandlercpp)&Scheduler::receive_startd_alive,
			"receive_startd_alive", this, READ,
			D_PROTOCOL ); 

	// Command handler for testing file access.  I set this as WRITE as we
	// don't want people snooping the permissions on our machine.
	daemonCore->Register_CommandWithPayload( ATTEMPT_ACCESS, "ATTEMPT_ACCESS",
								  (CommandHandler)&attempt_access_handler,
								  "attempt_access_handler", NULL, WRITE,
								  D_FULLDEBUG );
#ifdef WIN32
	// Command handler for stashing credentials.
	daemonCore->Register_CommandWithPayload( STORE_CRED, "STORE_CRED",
								(CommandHandler)&store_cred_handler,
								"cred_access_handler", NULL, WRITE,
								D_FULLDEBUG );
#endif

    // command handler in support of condor_status -direct query of our ads
    //
	daemonCore->Register_CommandWithPayload(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
                                (CommandHandlercpp)&Scheduler::command_query_ads,
                                 "command_query_ads", this, READ);
	daemonCore->Register_CommandWithPayload(QUERY_SUBMITTOR_ADS,"QUERY_SUBMITTOR_ADS",
                                (CommandHandlercpp)&Scheduler::command_query_ads,
                                 "command_query_ads", this, READ);

	// Note: The QMGMT READ/WRITE commands have the same command handler.
	// This is ok, because authorization to do write operations is verified
	// internally in the command handler.
	daemonCore->Register_CommandWithPayload( QMGMT_READ_CMD, "QMGMT_READ_CMD",
								  (CommandHandler)&handle_q,
								  "handle_q", NULL, READ, D_FULLDEBUG );

	// This command always requires authentication.  Therefore, it is
	// more efficient to force authentication when establishing the
	// security session than to possibly create an unauthenticated
	// security session that has to be authenticated every time in
	// the command handler.
	daemonCore->Register_CommandWithPayload( QMGMT_WRITE_CMD, "QMGMT_WRITE_CMD",
								  (CommandHandler)&handle_q,
								  "handle_q", NULL, WRITE, D_FULLDEBUG,
								  true /* force authentication */ );

	daemonCore->Register_Command( DUMP_STATE, "DUMP_STATE",
								  (CommandHandlercpp)&Scheduler::dumpState,
								  "dumpState", this, READ  );

	daemonCore->Register_CommandWithPayload( GET_MYPROXY_PASSWORD, "GET_MYPROXY_PASSWORD",
								  (CommandHandler)&get_myproxy_password_handler,
								  "get_myproxy_password", NULL, WRITE, D_FULLDEBUG  );


	daemonCore->Register_CommandWithPayload( GET_JOB_CONNECT_INFO, "GET_JOB_CONNECT_INFO",
								  (CommandHandlercpp)&Scheduler::get_job_connect_info_handler,
								  "get_job_connect_info", this, WRITE,
								  D_COMMAND, true /*force authentication*/);

	daemonCore->Register_CommandWithPayload( CLEAR_DIRTY_JOB_ATTRS, "CLEAR_DIRTY_JOB_ATTRS",
								  (CommandHandlercpp)&Scheduler::clear_dirty_job_attrs_handler,
								  "clear_dirty_job_attrs_handler", this, WRITE );


	 // These commands are for a startd reporting directly to the schedd sans negotiation
	daemonCore->Register_CommandWithPayload(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
        						  (CommandHandlercpp)&Scheduler::receive_startd_update,
								  "receive_startd_update",this,ADVERTISE_STARTD_PERM);

	daemonCore->Register_CommandWithPayload(INVALIDATE_STARTD_ADS,"INVALIDATE_STARTD_ADS",
        						  (CommandHandlercpp)&Scheduler::receive_startd_invalidate,
								  "receive_startd_invalidate",this,ADVERTISE_STARTD_PERM);


	 // reaper
	shadowReaperId = daemonCore->Register_Reaper(
		"reaper",
		(ReaperHandlercpp)&Scheduler::child_exit,
		"child_exit", this );

	// register all the timers
	RegisterTimers();

	// Now is a good time to instantiate the GridUniverse
	_gridlogic = new GridUniverseLogic;

	// Initialize the Transfer Daemon Manager's handlers as well
	m_tdman.register_handlers();

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


extern "C" {
int
prio_compar(prio_rec* a, prio_rec* b)
{
	 /* compare submitted job preprio's: higher values have more priority */
	 /* Typically used to prioritize entire DAG jobs over other DAG jobs */
	 if (a->pre_job_prio1 > INT_MIN && b->pre_job_prio1 > INT_MIN ) { 
	      if( a->pre_job_prio1 < b->pre_job_prio1 ) {
		  return 1;
              }
	      if( a->pre_job_prio1 > b->pre_job_prio1 ) {
		  return -1;
	      }
	 }
		 
	 if( a->pre_job_prio2 > INT_MIN && b->pre_job_prio2 > INT_MIN ) {
	      if( a->pre_job_prio2 < b->pre_job_prio2 ) {
		  return 1;
	      }
	      if( a->pre_job_prio2 > b->pre_job_prio2 ) {
		  return -1;
	      }
	 }
	 
	 /* compare job priorities: higher values have more priority */
	 if( a->job_prio < b->job_prio ) {
		  return 1;
	 }
	 if( a->job_prio > b->job_prio ) {
		  return -1;
	 }
	 
	 /* compare submitted job postprio's: higher values have more priority */
	 /* Typically used to prioritize entire DAG jobs over other DAG jobs */
	 if( a->post_job_prio1 > INT_MIN && b->post_job_prio1 > INT_MIN ) {
	      if( a->post_job_prio1 < b->post_job_prio1 ) {
		  return 1;
	      }
	      if( a->post_job_prio1 > b->post_job_prio1 ) {
		  return -1;
	      }
	 }
	 
	 if( a->post_job_prio2 > INT_MIN && b->post_job_prio2 > INT_MIN ) {
	      if( a->post_job_prio2 < b->post_job_prio2 ) {
		  return 1;
	      }
	      if( a->post_job_prio2 > b->post_job_prio2 ) {
		  return -1;
	      }
	 }
	      
	 /* here,updown priority and job_priority are both equal */

	 /* check for job submit times */
	 if( a->qdate < b->qdate ) {
		  return -1;
	 }
	 if( a->qdate > b->qdate ) {
		  return 1;
	 }

	 /* go in order of cluster id */
	if ( a->id.cluster < b->id.cluster )
		return -1;
	if ( a->id.cluster > b->id.cluster )
		return 1;

	/* finally, go in order of the proc id */
	if ( a->id.proc < b->id.proc )
		return -1;
	if ( a->id.proc > b->id.proc )
		return 1;

	/* give up! very unlikely we'd ever get here */
	return 0;
}
} // end of extern


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
	if ( autocluster.config() ) {
		WalkJobQueue( (int(*)(ClassAd *))clear_autocluster_id );
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
Scheduler::attempt_shutdown()
{
	if ( numShadows ) {
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
	if(  ( numShadows == 0 ) &&
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

	shadow_rec *rec;
	int sig;
	shadowsByPid->startIterations();
	while( shadowsByPid->iterate(rec) == 1 ) {
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

		// Since this is just sending a bunch of UDP updates, we can
		// still invalidate our classads, even on a fast shutdown.
	invalidate_ads();

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
	ScheddPluginManager::Shutdown();
	ClassAdLogPluginManager::Shutdown();
#endif
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

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
	ScheddPluginManager::Shutdown();
	ClassAdLogPluginManager::Shutdown();
#endif
#endif

	dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::invalidate_ads()
{
	int i;
	MyString line;

		// The ClassAd we need to use is totally different from the
		// regular one, so just create a temporary one
	ClassAd * cad = new ClassAd;
    SetMyTypeName( *cad, QUERY_ADTYPE );
    SetTargetTypeName( *cad, SCHEDD_ADTYPE );

        // Invalidate the schedd ad
    line.formatstr( "%s = TARGET.%s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, Name );
    cad->Insert( line.Value() );
	cad->Assign( ATTR_NAME, Name );
	cad->Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr() );


		// Update collectors
	daemonCore->sendUpdates(INVALIDATE_SCHEDD_ADS, cad, NULL, false);

	if (N_Owners == 0) return;	// no submitter ads to invalidate

		// Invalidate all our submittor ads.

	cad->Assign( ATTR_SCHEDD_NAME, Name );
	cad->Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr() );

	for( i=0; i<N_Owners; i++ ) {
		daemonCore->sendUpdates(INVALIDATE_SUBMITTOR_ADS, cad, NULL, false);
		MyString owner;
		owner.formatstr("%s@%s", Owners[i].Name, UidDomain);
		cad->Assign( ATTR_NAME, owner.Value() );

		line.formatstr( "%s = TARGET.%s == \"%s\" && TARGET.%s == \"%s\"",
					  ATTR_REQUIREMENTS,
					  ATTR_SCHEDD_NAME, Name,
					  ATTR_NAME, owner.Value() );
		cad->Insert( line.Value() );

		Daemon* d;
		if( FlockCollectors && FlockLevel > 0 ) {
			int level;
			for( level=1, FlockCollectors->rewind();
				 level <= FlockLevel && FlockCollectors->next(d); level++ ) {
				((DCCollector*)d)->sendUpdate( INVALIDATE_SUBMITTOR_ADS, cad, NULL, false );
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
Scheduler::sendReschedule()
{
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

	Stream::stream_type st = negotiator->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
	msg->setStreamType(st);
	msg->setTimeout(NEGOTIATOR_CONTACT_TIMEOUT);

	// since we may be sending reschedule periodically, make sure they do
	// not pile up
	msg->setDeadlineTimeout( 300 );

	negotiator->sendMsg( msg.get() );

	Daemon* d;
	if( FlockNegotiators ) {
		FlockNegotiators->rewind();
		FlockNegotiators->next( d );
		for( int i=0; d && i<FlockLevel; FlockNegotiators->next(d), i++ ) {
			st = d->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
			negotiator = new Daemon( *d );
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
		// The machine ad will be passed as the RIGHT ad during
		// matchmaking (i.e. in the call to IsAMatch()), so
		// optimize it accordingly.
	std::string error_msg;
	if( !classad::MatchClassAd::OptimizeRightAdForMatchmaking( ad, &error_msg ) ) {
		MyString name;
		ad->LookupString(ATTR_NAME,name);
		dprintf(D_ALWAYS,
				"Failed to optimize machine ad %s for matchmaking: %s\n",	
			name.Value(),
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
	if( matches->lookup( HashKey( id ), tempRec ) == 0 ) {
		char const *pubid = tempRec->publicClaimId();
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

	if( matches->insert( HashKey( id ), rec ) != 0 ) {
		dprintf( D_ALWAYS, "match \"%s\" insert failed\n",
				 id ? id : "(null)" );
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

	ClassAd *job_ad = GetJobAd(jobId->cluster,jobId->proc);
	if( job_ad && rec->my_match_ad ) {
		float new_startd_rank = 0;
		if( rec->my_match_ad->EvalFloat(ATTR_RANK, job_ad, new_startd_rank) ) {
			rec->my_match_ad->Assign(ATTR_CURRENT_RANK, new_startd_rank);
		}
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

	HashKey key(id);
	if( matches->lookup(key, rec) != 0 ) {
			// Couldn't find it, return failure
		return -1;
	}

	return DelMrec( rec );
}


int
Scheduler::DelMrec(match_rec* match)
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

	HashKey key(match->claimId());
	matches->remove(key);

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
			        match->auth_hole_id->Value());
		}
		delete match->auth_hole_id;
	}

		// Remove this match from the associated shadowRec.
	if (match->shadowRec)
		match->shadowRec->match = NULL;
	delete match;
	
	numMatches--; 
	return 0;
}

shadow_rec*
Scheduler::FindSrecByPid(int pid)
{
	shadow_rec *rec;
	if (shadowsByPid->lookup(pid, rec) < 0)
		return NULL;
	return rec;
}

shadow_rec*
Scheduler::FindSrecByProcID(PROC_ID proc)
{
	shadow_rec *rec;
	if (shadowsByProcID->lookup(proc, rec) < 0)
		return NULL;
	return rec;
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
	matches->lookup(claim_id, rec);
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
		SetMrecJobID(mrec,mrec->origcluster,-1);
		if( mrec->is_dedicated ) {
			deallocMatchRec( mrec );
		}
	}
}

int
Scheduler::AlreadyMatched(PROC_ID* id)
{
	int universe;

	if ((id->cluster == -1) && (id->proc == -1)) {
		return FALSE;
	}

	if (GetAttributeInt(id->cluster, id->proc,
						ATTR_JOB_UNIVERSE, &universe) < 0) {
		dprintf(D_FULLDEBUG, "GetAttributeInt() failed\n");
		return FALSE;
	}

	if ( (universe == CONDOR_UNIVERSE_MPI) ||
		 (universe == CONDOR_UNIVERSE_GRID) ||
		 (universe == CONDOR_UNIVERSE_PARALLEL) )
		return FALSE;

	if( FindMrecByJobID(*id) ) {
			// It is possible for there to be a match rec but no shadow rec,
			// if the job is waiting in the runnable job queue before the
			// shadow is launched.
		return TRUE;
	}
	if( FindSrecByProcID(*id) ) {
			// It is possible for there to be a shadow rec but no match rec,
			// if the match was deleted but the shadow has not yet gone away.
		return TRUE;
	}
	return FALSE;
}

/*
 * go through match reords and send alive messages to all the startds.
 */

bool
sendAlive( match_rec* mrec )
{
	classy_counted_ptr<DCStartd> startd = new DCStartd( mrec->description(),NULL,mrec->peer,mrec->claimId() );
	classy_counted_ptr<DCClaimIdMsg> msg = new DCClaimIdMsg( ALIVE, mrec->claimId() );

	msg->setSuccessDebugLevel(D_PROTOCOL);
	msg->setTimeout( STARTD_CONTACT_TIMEOUT );
	// since we send these messages periodically, we do not want
	// any single attempt to hang around forever and potentially pile up
	msg->setDeadlineTimeout( 300 );
	Stream::stream_type st = startd->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
	msg->setStreamType( st );
	msg->setSecSessionId( mrec->secSessionId() );

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
Scheduler::receive_startd_alive(int cmd, Stream *s)
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
			job_ad = GetJobAd(match->cluster, match->proc, false);
			if (job_ad) {
				job_ad->Assign(ATTR_LAST_JOB_LEASE_RENEWAL, (int)time(0));
			}
		}
	} else {
		ret_value = -1;
		ClaimIdParser idp( claim_id );
		dprintf(D_ALWAYS, "Received startd keepalive for unknown claimid %s\n",
			idp.publicClaimId() );
	}

	s->encode();
	s->code(ret_value);
	s->end_of_message();

	if (claim_id) free(claim_id);
	return TRUE;
}

void
Scheduler::sendAlives()
{
	match_rec	*mrec;
	int		  	numsent=0;

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

	int now = (int)time(0);
	BeginTransaction();
	matches->startIterations();
	while (matches->iterate(mrec) == 1) {
		if( mrec->status == M_ACTIVE ) {
			int renew_time;
			if ( mrec->m_startd_sends_alives ) {
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
	CommitTransaction();

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
			int last_lease_renewal = -1;
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
				jobExitCode( srec->job_id, JOB_SHOULD_REQUEUE );
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
		dprintf( D_FAILURE|D_ALWAYS, 
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
		// Copied from dumpState()
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
	cad->Assign( "JobsStarted", JobsStarted );
	cad->Assign( "SwapSpace", SwapSpace );
	cad->Assign( "ShadowSizeEstimate", ShadowSizeEstimate );
	cad->Assign( "SwapSpaceExhausted", SwapSpaceExhausted );
	cad->Assign( "ReservedSwap", ReservedSwap );
	cad->Assign( "JobsIdle", JobsIdle );
	cad->Assign( "JobsRunning", JobsRunning );
	cad->Assign( "BadCluster", BadCluster );
	cad->Assign( "BadProc", BadProc );
	cad->Assign( "N_Owners", N_Owners );
	cad->Assign( "NegotiationRequestTime", (int)NegotiationRequestTime  );
	cad->Assign( "ExitWhenDone", ExitWhenDone );
	cad->Assign( "StartJobTimer", StartJobTimer );
	if ( CondorAdministrator ) {
		cad->Assign( "CondorAdministrator", CondorAdministrator );
	}
	cad->Assign( "AccountantName", AccountantName );
	cad->Assign( "UidDomain", UidDomain );
	cad->Assign( "MaxFlockLevel", MaxFlockLevel );
	cad->Assign( "FlockLevel", FlockLevel );
	cad->Assign( "MaxExceptions", MaxExceptions );
	
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
	ClassAd input;
	ClassAd reply;
	PROC_ID jobid;
	MyString error_msg;
	ClassAd *jobad;
	int job_status = -1;
	match_rec *mrec = NULL;
	MyString job_claimid_buf;
	char const *job_claimid = NULL;
	char const *match_sec_session_id = NULL;
	int universe = -1;
	MyString startd_name;
	MyString starter_addr;
	MyString starter_claim_id;
	MyString job_owner_session_info;
	MyString starter_version;
	bool retry_is_sensible = false;
	bool job_is_suitable = false;
	ClassAd starter_ad;
	int ltimeout = 20;

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

	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,
				"Failed to receive input ClassAd for GET_JOB_CONNECT_INFO\n");
		return FALSE;
	}

	if( !input.LookupInteger(ATTR_CLUSTER_ID,jobid.cluster) ||
		!input.LookupInteger(ATTR_PROC_ID,jobid.proc) ) {
		error_msg.formatstr("Job id missing from GET_JOB_CONNECT_INFO request");
		goto error_wrapup;
	}

	dprintf(D_AUDIT, *sock, "GET_JOB_CONNECT_INFO for job %d.%d\n", jobid.cluster, jobid.proc );

	input.LookupString(ATTR_SESSION_INFO,job_owner_session_info);

	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if( !jobad ) {
		error_msg.formatstr("No such job: %d.%d", jobid.cluster, jobid.proc);
		goto error_wrapup;
	}

	if( !OwnerCheck2(jobad,sock->getOwner()) ) {
		error_msg.formatstr("%s is not authorized for access to the starter for job %d.%d",
						  sock->getOwner(), jobid.cluster, jobid.proc);
		goto error_wrapup;
	}

	jobad->LookupInteger(ATTR_JOB_STATUS,job_status);
	jobad->LookupInteger(ATTR_JOB_UNIVERSE,universe);

	job_is_suitable = false;
	switch( universe ) {
	case CONDOR_UNIVERSE_STANDARD:
	case CONDOR_UNIVERSE_GRID:
	case CONDOR_UNIVERSE_SCHEDULER:
		break; // these universes not supported
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
	{
		MyString claim_ids;
		MyString remote_hosts_string;
		int subproc = -1;
		if( jobad->LookupString(ATTR_CLAIM_IDS,claim_ids) &&
			jobad->LookupString(ATTR_ALL_REMOTE_HOSTS,remote_hosts_string) ) {
			StringList claim_idlist(claim_ids.Value(),",");
			StringList remote_hosts(remote_hosts_string.Value(),",");
			input.LookupInteger(ATTR_SUB_PROC_ID,subproc);
			if( claim_idlist.number() == 1 && subproc == -1 ) {
				subproc = 0;
			}
			if( subproc == -1 || subproc >= claim_idlist.number() ) {
				error_msg.formatstr("This is a parallel job.  Please specify job %d.%d.X where X is an integer from 0 to %d.",jobid.cluster,jobid.proc,claim_idlist.number()-1);
				goto error_wrapup;
			}
			else {
				claim_idlist.rewind();
				remote_hosts.rewind();
				for(int sp=0;sp<subproc;sp++) {
					claim_idlist.next();
					remote_hosts.next();
				}
				mrec = dedicated_scheduler.FindMrecByClaimID(claim_idlist.next());
				startd_name = remote_hosts.next();
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
			starter_addr = daemonCore->InfoCommandSinfulString( srec->pid );
			if( starter_addr.IsEmpty() ) {
				retry_is_sensible = true;
				break;
			}
			starter_ad.Assign(ATTR_STARTER_IP_ADDR,starter_addr);
			jobad->LookupString(ATTR_CLAIM_ID,job_claimid_buf);
			job_claimid = job_claimid_buf.Value();
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
		error_msg.formatstr("Job %d.%d is denied by SCHEDD_ENABLE_SSH_TO_JOB.",
						  jobid.cluster,jobid.proc);
		goto error_wrapup;
	}


	if( !job_is_suitable )
	{
		if( !retry_is_sensible ) {
				// this must be a job universe that we don't support
			error_msg.formatstr("Job %d.%d does not support remote access.",
							  jobid.cluster,jobid.proc);
		}
		else {
			error_msg.formatstr("Job %d.%d is not running.",
							  jobid.cluster,jobid.proc);
		}
		goto error_wrapup;
	}

	if( mrec ) { // locate starter by calling startd
		MyString global_job_id;
		MyString startd_addr = mrec->peer;

		DCStartd startd(startd_name.Value(),NULL,startd_addr.Value(),mrec->secSessionId() );

		jobad->LookupString(ATTR_GLOBAL_JOB_ID,global_job_id);

		if( !startd.locateStarter(global_job_id.Value(),mrec->claimId(),daemonCore->publicNetworkIpAddr(),&starter_ad,ltimeout) )
		{
			error_msg = "Failed to get address of starter for this job";
			goto error_wrapup;
		}
		job_claimid = mrec->claimId();
		match_sec_session_id = mrec->secSessionId();
	}

		// now connect to the starter and create a security session for
		// our client to use
	{
		starter_ad.LookupString(ATTR_STARTER_IP_ADDR,starter_addr);

		DCStarter starter;
		if( !starter.initFromClassAd(&starter_ad) ) {
			error_msg = "Failed to read address of starter for this job";
			goto error_wrapup;
		}

		if( !starter.createJobOwnerSecSession(ltimeout,job_claimid,match_sec_session_id,job_owner_session_info.Value(),starter_claim_id,error_msg,starter_version,starter_addr) ) {
			goto error_wrapup; // error_msg already set
		}
	}

	reply.Assign(ATTR_RESULT,true);
	reply.Assign(ATTR_STARTER_IP_ADDR,starter_addr.Value());
	reply.Assign(ATTR_CLAIM_ID,starter_claim_id.Value());
	reply.Assign(ATTR_VERSION,starter_version.Value());
	reply.Assign(ATTR_REMOTE_HOST,startd_name.Value());
	if( !putClassAd(s, reply) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,
				"Failed to send response to GET_JOB_CONNECT_INFO\n");
	}

	dprintf(D_FULLDEBUG,"Produced connect info for %s job %d.%d startd %s.\n",
			sock->getFullyQualifiedUser(), jobid.cluster, jobid.proc,
			starter_addr.Value() );

	return TRUE;

 error_wrapup:
	dprintf(D_AUDIT|D_FAILURE, *sock, "GET_JOB_CONNECT_INFO failed: %s\n",error_msg.Value() );
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

int
Scheduler::dumpState(int, Stream* s) {

	dprintf ( D_FULLDEBUG, "Dumping state for Squawk\n" );

		//
		// The new publish() method will stuff all the attributes
		// that we used to set in this method
		//
	ClassAd job_ad;
	this->publish( &job_ad );
	
		//
		// These items we want to keep in here because they're
		// not needed for the general info produced by publish()
		//
	job_ad.Assign( "leaseAliveInterval", leaseAliveInterval );
	job_ad.Assign( "alive_interval", alive_interval );
	job_ad.Assign( "startjobsid", startjobsid );
	job_ad.Assign( "timeoutid", timeoutid );
	job_ad.Assign( "Mail", Mail );
	
	int cmd = 0;
	s->code( cmd );
	s->end_of_message();

	s->encode();
	
	putClassAd( s, job_ad );

	return TRUE;
}

int
fixAttrUser( ClassAd *job )
{
	int nice_user = 0;
	MyString owner;
	MyString user;
	
	if( ! job->LookupString(ATTR_OWNER, owner) ) {
			// No ATTR_OWNER!
		return 0;
	}
		// if it's not there, nice_user will remain 0
	job->LookupInteger( ATTR_NICE_USER, nice_user );

	user.formatstr( "%s%s@%s",
			 (nice_user) ? "nice-user." : "", owner.Value(),
			 scheduler.uidDomain() );  
	job->Assign( ATTR_USER, user );
	return 0;
}


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
	MyString value;
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
							   value.Value() ); 

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
	int value;
	MyString new_value;
	int rval;

	if( GetAttributeInt(job_id.cluster, job_id.proc, old_attr, &value) < 0 ) {
		if( verbose ) {
			dprintf( D_FULLDEBUG, "No %s found for job %d.%d\n",
					 old_attr, job_id.cluster, job_id.proc );
		}
		return false;
	}
	
	new_value += value;

	rval = SetAttribute( job_id.cluster, job_id.proc, new_attr,
						 new_value.Value() ); 

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

	// Abort the job now
	abort_job_myself( job_id, JA_REMOVE_JOBS, true, true );
	dprintf( D_ALWAYS, "Job %d.%d aborted: %s\n", cluster, proc, reason );

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
			CommitTransaction();
		} else {
			AbortTransaction();
		}
	}

		// If we successfully removed the job, remove any jobs that
		// match is OtherJobRemoveRequirements attribute, if it has one.
	if ( result ) {
		// Ignoring return value because we're not sure what to do
		// with it.
		(void)removeOtherJobs( cluster, proc );
	}

	return result;
}

bool
abortJobsByConstraint( const char *constraint,
					   const char *reason,
					   bool use_transaction )
{
	bool result = true;

	ExtArray<PROC_ID> jobs;
	int job_count;

	dprintf(D_FULLDEBUG, "abortJobsByConstraint: '%s'\n", constraint);

	if ( use_transaction ) {
		BeginTransaction();
	}

	job_count = 0;
	ClassAd *ad = GetNextJobByConstraint(constraint, 1);
	while ( ad ) {
		if (!ad->LookupInteger(ATTR_CLUSTER_ID, jobs[job_count].cluster) ||
			!ad->LookupInteger(ATTR_PROC_ID, jobs[job_count].proc)) {

			result = false;
			job_count = 0;
			break;
		}

		dprintf(D_FULLDEBUG, "remove by constraint matched: %d.%d\n",
				jobs[job_count].cluster, jobs[job_count].proc);

		job_count++;

		ad = GetNextJobByConstraint(constraint, 0);
	}

	job_count--;
	ExtArray<PROC_ID> removedJobs;
	int removedJobCount = 0;
	while ( job_count >= 0 ) {
		dprintf(D_FULLDEBUG, "removing: %d.%d\n",
				jobs[job_count].cluster, jobs[job_count].proc);

		bool tmpResult = abortJobRaw(jobs[job_count].cluster,
									   jobs[job_count].proc,
									   reason);
		if ( tmpResult ) {
			removedJobs[removedJobCount].cluster = jobs[job_count].cluster;
			removedJobs[removedJobCount].proc =  jobs[job_count].proc;
			removedJobCount++;
		}
		result = result && tmpResult;
		job_count--;
	}

	if ( use_transaction ) {
		if ( result ) {
			CommitTransaction();
		} else {
			AbortTransaction();
		}
	}

		//
		// Remove "other" jobs that need to be removed as a result of
		// the OtherJobRemoveRequirements exppression(s) in the job(s)
		// that have just been removed.  Note that this must be done
		// *after* the transaction is committed.
		//
	removedJobCount--;
	while ( removedJobCount >= 0 ) {
		// Ignoring return value because we're not sure what to do
		// with it.
		(void)removeOtherJobs(
					removedJobs[removedJobCount].cluster,
					removedJobs[removedJobCount].proc );
		removedJobCount--;
	}

	return result;
}



/*
Hold a job by stopping the shadow, changing the job state,
writing to the user log, and updating the job queue.
Does not start or end a transaction.
*/

static bool
holdJobRaw( int cluster, int proc, const char* reason,
			int reason_code, int reason_subcode,
		 bool notify_shadow, bool email_user,
		 bool email_admin, bool system_hold )
{
	int status;
	PROC_ID tmp_id;
	tmp_id.cluster = cluster;
	tmp_id.proc = proc;
	int system_holds = 0;

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
		dprintf( D_ALWAYS, "Job %d.%d is already on hold\n",
				 cluster, proc );
		return false;
	}

	if ( system_hold ) {
		GetAttributeInt(cluster, proc, ATTR_NUM_SYSTEM_HOLDS, &system_holds);
	}

	if( reason ) {
		MyString fixed_reason;
		if( reason[0] == '"' ) {
			fixed_reason += reason;
		} else {
			fixed_reason += '"';
			fixed_reason += reason;
			fixed_reason += '"';
		}
		if( SetAttribute(cluster, proc, ATTR_HOLD_REASON, 
						 fixed_reason.Value()) < 0 ) {
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
						(int)time(0)) < 0 ) {
		dprintf( D_ALWAYS, "WARNING: Failed to set %s for job %d.%d\n",
				 ATTR_ENTERED_CURRENT_STATUS, cluster, proc );
	}

	if( SetAttributeInt(cluster, proc, ATTR_LAST_SUSPENSION_TIME, 0) < 0 ) {
		dprintf( D_ALWAYS, "WARNING: Failed to set %s for job %d.%d\n",
				 ATTR_LAST_SUSPENSION_TIME, cluster, proc );
	}

	if ( system_hold ) {
		system_holds++;
		SetAttributeInt(cluster, proc, ATTR_NUM_SYSTEM_HOLDS, system_holds);
	}

	dprintf( D_ALWAYS, "Job %d.%d put on hold: %s\n", cluster, proc,
			 reason );

	// replacing this with the call to enqueueActOnJobMyself
	// in holdJob AFTER the transaction; otherwise the job status
	// doesn't get properly updated for some reason
	//abort_job_myself( tmp_id, JA_HOLD_JOBS, true, notify_shadow );
        if(!notify_shadow)	
	{
		dprintf( D_ALWAYS, "notify_shadow set to false but will still notify- this should not be optional\n");
	}

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
		 bool use_transaction, bool notify_shadow, bool email_user,
		 bool email_admin, bool system_hold, bool write_to_user_log )
{
	bool result;

	if(use_transaction) {
		BeginTransaction();
	}

	result = holdJobRaw(cluster,proc,reason,reason_code,reason_subcode,notify_shadow,email_user,email_admin,system_hold);

	if(use_transaction) {
		if(result) {
			CommitTransaction();
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
		scheduler.enqueueActOnJobMyself(id,JA_HOLD_JOBS, true, write_to_user_log);
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
	int status;
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
		MyString fixed_reason;
		if( reason[0] == '"' ) {
			fixed_reason += reason;
		} else {
			fixed_reason += '"';
			fixed_reason += reason;
			fixed_reason += '"';
		}
		if( SetAttribute(cluster, proc, ATTR_RELEASE_REASON, 
						 fixed_reason.Value()) < 0 ) {
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
						(int)time(0)) < 0 ) {
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
			CommitTransaction();
		} else {
			AbortTransaction();
		}
	}

	scheduler.needReschedule();

	return result;
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
	MyString job_managed;
	if( ! ad->LookupString(ATTR_JOB_MANAGED, job_managed) ) {
		return false;
	}
	return job_managed == MANAGED_EXTERNAL;
}

bool jobManagedDone(ClassAd * ad)
{
	ASSERT(ad);
	MyString job_managed;
	if( ! ad->LookupString(ATTR_JOB_MANAGED, job_managed) ) {
		return false;
	}
	return job_managed == MANAGED_DONE;
}


bool 
Scheduler::claimLocalStartd()
{
	Daemon startd(DT_STARTD, NULL, NULL);
	char *startd_addr = NULL;	// local startd sinful string
	int slot_id;
	int number_of_claims = 0;
	char claim_id[155];	
	MyString slot_state;
	char job_owner[150];

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
				20 * 60);
	if ( time(NULL) - NegotiationRequestTime < claimlocal_interval ) {
			// we have negotiated recently, no need to calim the local startd
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
		char *file_name = startdClaimIdFile(slot_id);
		if (!file_name) continue;
			// now open it as user condor and read out the claim
		claim_id[0] = '\0';	// so we notice if we fail to read
			// note: claim file written w/ condor priv by the startd
		priv_state old_priv = set_condor_priv(); 
		FILE* fp=safe_fopen_wrapper_follow(file_name,"r");
		if ( fp ) {
			if (fscanf(fp,"%150s\n",claim_id) != 1) {
				dprintf(D_ALWAYS, "Failed to fscanf claim_id from file %s\n", file_name);
				continue;
			}
			fclose(fp);
		}
		set_priv(old_priv);	// switch our priv state back
		free(file_name);
		claim_id[150] = '\0';	// make certain it is null terminated
			// if we failed to get the claim, move on
		if ( !claim_id[0] ) {
			dprintf(D_ALWAYS,"Failed to read startd claim id from file %s\n",
				file_name);
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
		jobad->LookupString(ATTR_OWNER,job_owner,sizeof(job_owner));
		ASSERT(job_owner[0]);

		match_rec* mrec = AddMrec( claim_id, startd_addr, &matching_jobid, machine_ad,
						job_owner,	// special Owner name
						NULL	// optional negotiator name
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
						new ContactStartdArgs(claim_id, startd_addr, false);
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
 * Adds a job to our list of CronTab jobs
 * We will check to see if the job has already been added and
 * whether it defines a valid CronTab schedule before adding it
 * to our table
 * 
 * @param jobAd - the new job to be added to the cronTabs table
 **/
void
Scheduler::addCronTabClassAd( ClassAd *jobAd )
{
	if ( NULL == m_adSchedd ) return;
	CronTab *cronTab = NULL;
	PROC_ID id;
	jobAd->LookupInteger( ATTR_CLUSTER_ID, id.cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, id.proc );
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
	if ( cluster_id < 0 ||
		 this->cronTabClusterIds.IsMember( cluster_id ) ) return;
	if ( this->cronTabClusterIds.enqueue( cluster_id ) < 0 ) {
		dprintf( D_FULLDEBUG,
				 "Failed to add cluster %d to the cron jobs list\n", cluster_id );
	}
	return;
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
	int cluster_id;
	CronTab *cronTab = NULL;
	ClassAd *jobAd = NULL;
	
		//
		// Loop through all the cluster_ids that we have stored
		// For each cluster, we will inspect the job ads of all its
		// procs to see if they have defined crontab information
		//
	while ( this->cronTabClusterIds.dequeue( cluster_id ) >= 0 ) {
		int init = 1;
		while ( ( jobAd = GetNextJobByCluster( cluster_id, init ) ) ) {
			PROC_ID id;
			jobAd->LookupInteger( ATTR_CLUSTER_ID, id.cluster );
			jobAd->LookupInteger( ATTR_PROC_ID, id.proc );
				//
				// Simple safety check
				//
			ASSERT( id.cluster == cluster_id );
				//
				// If this job hasn't been added to our cron job table
				// and if it needs to be, we wil added to our list
				//
			if ( this->cronTabs->lookup( id, cronTab ) < 0 &&
				 CronTab::needsCronTab( jobAd ) ) {
				this->cronTabs->insert( id, NULL );
			}		
			init = 0;
		} // WHILE
	} // WHILE
	return;
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
	if ( status == RUNNING || status == TRANSFERRING_OUTPUT ) {
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
	MyString error;
		//
		// See if we can get the cached scheduler object 
		//
	CronTab *cronTab = NULL;
	this->cronTabs->lookup( id, cronTab );
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
				this->cronTabs->insert( id, cronTab );
			} else {
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
		int deferralTime = 0;
		jobAd->EvalInteger( ATTR_DEFERRAL_TIME, NULL, deferralTime );
			//
			// Now look to see if they also have a DeferralWindow
			//
		int deferralWindow = 0;
		if ( jobAd->LookupExpr( ATTR_DEFERRAL_WINDOW ) != NULL ) {
			jobAd->EvalInteger( ATTR_DEFERRAL_WINDOW, NULL, deferralWindow );
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
		long runTime = cronTab->nextRunTime( );

		dprintf( D_FULLDEBUG, "Calculating next execution time for Job %d.%d = %ld\n",
				 id.cluster, id.proc, runTime );
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
			jobAd->Assign( ATTR_DEFERRAL_TIME,	(int)runTime );	
					
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
		MyString reason( "Invalid cron schedule parameters: " );
		if ( cronTab != NULL ) {
			reason += cronTab->getError();
		} else {
			reason += error;
		}
			//
			// Throw the job on hold. For this call we want to:
			// 	use_transaction - true
			//	notify_shadow	- false
			//	email_user		- true
			//	email_admin		- false
			//	system_hold		- false
			//
		holdJob( id.cluster, id.proc, reason.Value(),
				 CONDOR_HOLD_CODE_InvalidCronSettings, 0,
				 true, false, true, false, false );
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
				srec->preempted = TRUE;
			}
		}
		return DCSignalMsg::messageSent(messenger,sock);
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
	int value;
	MyString iwd;

	ASSERT(ad);

	if (!ad->EvalBool(ATTR_WANT_SCHEDD_COMPLETION_VISA, NULL, value) ||
	    !value)
	{
		if (!ad->EvalBool(ATTR_JOB_SANDBOX_JOBAD, NULL, value) ||
		    !value)
		{
			return;
		}
	}

	if (!ad->LookupString(ATTR_JOB_IWD, iwd)) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "WriteCompletionVisa ERROR: Job contained no IWD\n");
		return;
	}

	prev_priv_state = set_user_priv_from_ad(*ad);
	classad_visa_write(ad,
	                   get_mySubSystem()->getName(),
	                   daemonCore->InfoCommandSinfulString(),
	                   iwd.Value(),
	                   NULL);
	set_priv(prev_priv_state);
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
	if( !mrec || !mrec->user ||
		(srec->universe != CONDOR_UNIVERSE_VANILLA &&
		 srec->universe != CONDOR_UNIVERSE_JAVA &&
		 srec->universe != CONDOR_UNIVERSE_VM) )
	{
		stream->encode();
		stream->put((int)0);
		return FALSE;
	}

		// verify that whoever is running this command is either the
		// queue super user or the owner of the claim
	char const *cmd_user = sock->getOwner();
	std::string match_owner;
	char const *at_sign = strchr(mrec->user,'@');
	if( at_sign ) {
		match_owner.append(mrec->user,at_sign-mrec->user);
	}
	else {
		match_owner = mrec->user;
	}

	if( !OwnerCheck2(NULL,cmd_user,match_owner.c_str()) ) {
		dprintf(D_ALWAYS,
				"RecycleShadow() called by %s failed authorization check!\n",
				cmd_user ? cmd_user : "(unauthenticated)");
		return FALSE;
	}

		// Now handle the exit reason specified for the existing job.
	if( prev_job_id.cluster != -1 ) {
		dprintf(D_ALWAYS,
			"Shadow pid %d for job %d.%d reports job exit reason %d.\n",
			shadow_pid, prev_job_id.cluster, prev_job_id.proc,
			previous_job_exit_reason );

		jobExitCode( prev_job_id, previous_job_exit_reason );
		srec->exit_already_handled = true;
	}

		// The standard universe shadow never calls this function,
		// and the shadow that does call this function is not capable of
		// running standard universe jobs, so if the job we are trying
		// to run next is standard universe, tell this shadow we are
		// out of work.
	const bool accept_std_univ = false;

	if (mrec->keep_while_idle) {
		mrec->idle_timer_deadline = time(NULL) + mrec->keep_while_idle;
	}

	if( !FindRunnableJobForClaim(mrec,accept_std_univ) ) {
		stream->put((int)0);
		stream->end_of_message();
		return TRUE;
	}

	new_job_id.cluster = mrec->cluster;
	new_job_id.proc = mrec->proc;

	dprintf(D_ALWAYS,
			"Shadow pid %d switching to job %d.%d.\n",
			shadow_pid, new_job_id.cluster, new_job_id.proc );

    stats.Tick();
    stats.ShadowsRecycled += 1;
    stats.ShadowsRunning = numShadows;
	OtherPoolStats.Tick();

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
		new_ad = GetJobAd(new_job_id.cluster, new_job_id.proc ,true, true);
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
	MyString owner;
	MyString domain;
	ClassAd *job_ad = GetJobAd(job_id.cluster,job_id.proc);

	if ( ! job_ad ) {
		return -1;
	}

	job_ad->LookupString(ATTR_OWNER,owner);
	job_ad->LookupString(ATTR_NT_DOMAIN,domain);
	UserIdentity userident(owner.Value(),domain.Value(),job_ad);
	return GridUniverseLogic::FindGManagerPid(userident.username().Value(),
                                        userident.auxid().Value(), 0, 0);
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

	char *claim_id = 0;
	privateAd->LookupString(ATTR_CAPABILITY, &claim_id);
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
		sn->scheduler_handleMatch(jobid,claim_id, *machineAd, name);
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

	int create_process_opts = 0; // Nothing odd

	  // The arguments for our startd
	ArgList args;
	args.AppendArg("condor_startd");
	args.AppendArg("-f"); // The startd is daemon-core, so run in the "foreground"
	args.AppendArg("-local-name"); // This is the local startd, not the vanilla one
	args.AppendArg("LOCALSTARTD");

	Env env;
	env.Import(); // copy schedd's environment
	env.SetEnv("_condor_STARTD_LOG", "$(LOG)/LocalStartLog");
	env.SetEnv("_condor_EXECUTE", "$(SPOOL)/local_univ_execute");

	// Force start expression to be START_LOCAL_UNIVERSE
	char *localStartExpr = 0;
	localStartExpr = param("START_LOCAL_UNIVERSE");
	std::string localConstraint = "(JobUniverse == 12) && ";
	localConstraint += localStartExpr;
	env.SetEnv("_condor_START", localConstraint);
	free(localStartExpr);


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

	m_local_startd_pid = daemonCore->Create_Process(	path.c_str(),
										args,
										PRIV_ROOT,
										rid, 
	                                  	1, /* is_dc */
										&env, 
										NULL, 
										NULL,
										NULL, 
	                                  	NULL,  /* stdin/stdout/stderr */
										NULL, 
										0,    /* niceness */
									  	NULL,
										create_process_opts);

	dprintf(D_ALWAYS, "Launched startd for local jobs with pid %d\n", m_local_startd_pid);
	return TRUE;
}

