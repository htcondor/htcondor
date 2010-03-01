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
#include "condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "proc.h"
#include "exit.h"
#include "condor_collector.h"
#include "scheduler.h"
#include "condor_attributes.h"
#include "condor_parameters.h"
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "classad_helpers.h"
#include "condor_adtypes.h"
#include "condor_string.h"
#include "condor_email.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "get_daemon_name.h"
#include "renice_self.h"
#include "write_user_log.h"
#include "access.h"
#include "internet.h"
#include "condor_ckpt_name.h"
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
#include "condor_classad_namedlist.h"
#include "schedd_cronmgr.h"
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

#if HAVE_DLOPEN
#include "ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#endif

#define DEFAULT_SHADOW_SIZE 800
#define DEFAULT_JOB_START_COUNT 1

#define SUCCESS 1
#define CANT_RUN 0

extern char *gen_ckpt_name();

extern GridUniverseLogic* _gridlogic;

#include "condor_qmgr.h"
#include "qmgmt.h"
#include "condor_vm_universe_types.h"

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
int fixAttrUser( ClassAd *job );
shadow_rec * find_shadow_rec(PROC_ID*);
bool service_this_universe(int, ClassAd*);
bool jobIsSandboxed( ClassAd* ad );
bool getSandbox( int cluster, int proc, MyString & path );
bool sandboxHasRightOwner( int cluster, int proc, ClassAd* job_ad );
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

unsigned int UserIdentity::HashFcn(const UserIdentity & index)
{
	return index.m_username.Hash() + index.m_domain.Hash() + index.m_auxid.Hash();
}

UserIdentity::UserIdentity(const char *user, const char *domainname, 
						   const ClassAd *ad):
	m_username(user), 
	m_domain(domainname),
	m_auxid("")
{
	ExprTree *tree = (ExprTree *) scheduler.getGridParsedSelectionExpr();
	EvalResult val;
	if ( ad && tree && 
		 tree->EvalTree(ad,&val) && val.type==LX_STRING && val.s )
	{
		m_auxid = val.s;
	}
}

struct job_data_transfer_t {
	int mode;
	char *peer_version;
	ExtArray<PROC_ID> *jobs;
};

int
dc_reconfig()
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGHUP );
	return TRUE;
}

match_rec::match_rec( char* claim_id, char* p, PROC_ID* job_id, 
					  const ClassAd *match, char *the_user, char *my_pool,
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
		if( DebugFlags && D_MACHINE ) {
			dprintf( D_MACHINE, "*** ClassAd of Matched Resource ***\n" );
			my_match_ad->dPrint( D_MACHINE );
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
	if( DebugFlags & D_FULLDEBUG ) {
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
	}
}


void
match_rec::setStatus( int stat )
{
	status = stat;
	entered_current_status = (int)time(0);
	if( status == M_CLAIMED ||
		status == M_STARTD_CONTACT_LIMBO ) {
			// We may have successfully claimed this startd, so we need to
			// release it later.
		needs_release_claim = true;
	}
}


ContactStartdArgs::ContactStartdArgs( char* the_claim_id, char* sinfulstr, bool is_dedicated ) 
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
	GridJobOwners(USER_HASH_SIZE, UserIdentity::HashFcn, updateDuplicateKeys),
	stop_job_queue( "stop_job_queue" ),
	act_on_job_myself_queue( "act_on_job_myself_queue" ),
	job_is_finished_queue( "job_is_finished_queue", 1 )
{
	m_ad = NULL;
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
	m_need_reschedule = false;
	m_send_reschedule_timer = -1;

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
	startd_sends_alives = false;
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

#ifdef WANT_QUILL
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

	CronMgr = NULL;

	jobThrottleNextJobDelay = 0;
#ifdef WANT_QUILL
	prevLHF = 0;
#endif
}


Scheduler::~Scheduler()
{
	delete m_ad;
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

	if ( CronMgr ) {
		delete CronMgr;
		CronMgr = NULL;
	}

		// we used to cancel and delete the shadowCommand*socks here,
		// but now that we're calling Cancel_And_Close_All_Sockets(),
		// they're already getting cleaned up, so if we do it again,
		// we'll seg fault.

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
	daemonCore->Reset_Timer(timeoutid,time_to_next_run);
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
** Examine the job queue to determine how many CONDOR jobs we currently have
** running, and how many individual users own them.
*/
int
Scheduler::count_jobs()
{
	int		i, j;
	int		prio_compar();
	char	tmp[512];

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

	// clear owner table contents
	time_t current_time = time(0);
	for ( i = 0; i < Owners.getsize(); i++) {
		Owners[i].Name = NULL;
		Owners[i].Domain = NULL;
		Owners[i].JobsRunning = 0;
		Owners[i].JobsIdle = 0;
		Owners[i].JobsHeld = 0;
		Owners[i].JobsFlocked = 0;
		Owners[i].FlockLevel = 0;
		Owners[i].OldFlockLevel = 0;
		Owners[i].NegotiationTimestamp = current_time;
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
				Owners[i].FlockLevel++;
				Owners[i].NegotiationTimestamp = current_time;
				dprintf(D_ALWAYS,
						"Increasing flock level for %s to %d.\n",
						Owners[i].Name, Owners[i].FlockLevel);
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
	
	sprintf(tmp, "%s = %d", ATTR_NUM_USERS, N_Owners);
	m_ad->InsertOrUpdate(tmp);
	
	sprintf(tmp, "%s = %d", ATTR_MAX_JOBS_RUNNING, MaxJobsRunning);
	m_ad->InsertOrUpdate(tmp);
	
	sprintf(tmp, "%s = %s", ATTR_START_LOCAL_UNIVERSE,
							this->StartLocalUniverse );
	m_ad->InsertOrUpdate(tmp);
	
	sprintf(tmp, "%s = %s", ATTR_START_SCHEDULER_UNIVERSE,
							this->StartSchedulerUniverse );
	m_ad->InsertOrUpdate(tmp);
	
	
	 sprintf(tmp, "%s = \"%s\"", ATTR_NAME, Name);
	 m_ad->InsertOrUpdate(tmp);

	 m_ad->Assign( ATTR_SCHEDD_IP_ADDR, daemonCore->publicNetworkIpAddr() );

	 sprintf(tmp, "%s = %d", ATTR_VIRTUAL_MEMORY, SwapSpace );
	 m_ad->InsertOrUpdate(tmp);

	// The schedd's ad should not have idle and running job counts --- 
	// only the per user queue ads should.  Instead, the schedd has
	// TotalIdleJobs and TotalRunningJobs.
	m_ad->Delete (ATTR_IDLE_JOBS);
	m_ad->Delete (ATTR_RUNNING_JOBS);
	m_ad->Delete (ATTR_HELD_JOBS);
	m_ad->Delete (ATTR_FLOCKED_JOBS);
	// The schedd's ad doesn't need ATTR_SCHEDD_NAME either.
	m_ad->Delete (ATTR_SCHEDD_NAME);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_IDLE_JOBS, JobsIdle);
	m_ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_RUNNING_JOBS, JobsRunning);
	m_ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_JOB_ADS, JobsTotalAds);
	m_ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_HELD_JOBS, JobsHeld);
	m_ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_FLOCKED_JOBS, JobsFlocked);
	m_ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_REMOVED_JOBS, JobsRemoved);
	m_ad->Insert (tmp);

	m_ad->Assign(ATTR_TOTAL_LOCAL_IDLE_JOBS, LocalUniverseJobsIdle);
	m_ad->Assign(ATTR_TOTAL_LOCAL_RUNNING_JOBS, LocalUniverseJobsRunning);
	
	m_ad->Assign(ATTR_TOTAL_SCHEDULER_IDLE_JOBS, SchedUniverseJobsIdle);
	m_ad->Assign(ATTR_TOTAL_SCHEDULER_RUNNING_JOBS, SchedUniverseJobsRunning);

	m_ad->Assign(ATTR_SCHEDD_SWAP_EXHAUSTED, (bool)SwapSpaceExhausted);

    daemonCore->publish(m_ad);
    daemonCore->monitor_data.ExportData(m_ad);
	extra_ads.Publish( m_ad );
    
	if ( param_boolean("ENABLE_SOAP", false) ) {
			// If we can support the SOAP API let's let the world know!
		sprintf(tmp, "%s = True", ATTR_HAS_SOAP_API);
		m_ad->Insert(tmp);
	}

	sprintf(tmp, "%s = %d", ATTR_JOB_QUEUE_BIRTHDATE,
			 (int)GetOriginalJobQueueBirthdate());
	m_ad->Insert (tmp);

        // Tell negotiator to send us the startd ad
		// As of 7.1.3, the negotiator no longer pays attention to this
		// attribute; it _always_ sends the resource request ad.
		// For backward compatibility with older negotiators, we still set it.
	sprintf(tmp, "%s = True", ATTR_WANT_RESOURCE_AD );
	m_ad->InsertOrUpdate(tmp);

	daemonCore->UpdateLocalAd(m_ad);

		// log classad into sql log so that it can be updated to DB
#ifdef WANT_QUILL
	FILESQL::daemonAdInsert(m_ad, "ScheddAd", FILEObj, prevLHF);
#endif

#if HAVE_DLOPEN
	ScheddPluginManager::Update(UPDATE_SCHEDD_AD, m_ad);
#endif
	
		// Update collectors
	int num_updates = daemonCore->sendUpdates(UPDATE_SCHEDD_AD, m_ad, NULL, true);
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
			col->sendUpdate( UPDATE_SCHEDD_AD, m_ad, NULL, true );
			FlockCollectors->next( d );
		}
	}

	// The per user queue ads should not have NumUsers in them --- only
	// the schedd ad should.  In addition, they should not have
	// TotalRunningJobs and TotalIdleJobs
	m_ad->Delete (ATTR_NUM_USERS);
	m_ad->Delete (ATTR_TOTAL_RUNNING_JOBS);
	m_ad->Delete (ATTR_TOTAL_IDLE_JOBS);
	m_ad->Delete (ATTR_TOTAL_JOB_ADS);
	m_ad->Delete (ATTR_TOTAL_HELD_JOBS);
	m_ad->Delete (ATTR_TOTAL_FLOCKED_JOBS);
	m_ad->Delete (ATTR_TOTAL_REMOVED_JOBS);
	m_ad->Delete (ATTR_TOTAL_LOCAL_IDLE_JOBS);
	m_ad->Delete (ATTR_TOTAL_LOCAL_RUNNING_JOBS);
	m_ad->Delete (ATTR_TOTAL_SCHEDULER_IDLE_JOBS);
	m_ad->Delete (ATTR_TOTAL_SCHEDULER_RUNNING_JOBS);

	sprintf(tmp, "%s = \"%s\"", ATTR_SCHEDD_NAME, Name);
	m_ad->InsertOrUpdate(tmp);

	m_ad->SetMyTypeName( SUBMITTER_ADTYPE );

	for ( i=0; i<N_Owners; i++) {
	  sprintf(tmp, "%s = %d", ATTR_RUNNING_JOBS, Owners[i].JobsRunning);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  m_ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS, Owners[i].JobsIdle);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  m_ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_HELD_JOBS, Owners[i].JobsHeld);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  m_ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_FLOCKED_JOBS, Owners[i].JobsFlocked);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  m_ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, Owners[i].Name, UidDomain);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  m_ad->InsertOrUpdate(tmp);

	  dprintf( D_ALWAYS, "Sent ad to central manager for %s@%s\n", 
			   Owners[i].Name, UidDomain );

#if HAVE_DLOPEN
	  ScheddPluginManager::Update(UPDATE_SUBMITTOR_AD, m_ad);
#endif
		// Update collectors
	  num_updates = daemonCore->sendUpdates(UPDATE_SUBMITTOR_AD, m_ad, NULL, true);
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
				if (Owners[i].FlockLevel >= flock_level) {
					sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS,
							Owners[i].JobsIdle);
				} else if (Owners[i].OldFlockLevel >= flock_level ||
						   Owners[i].JobsRunning > 0) {
					sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS, 0);
				} else {
					// if we're no longer flocking with this pool and
					// we're not running jobs in the pool, then don't send
					// an update
					continue;
				}
				m_ad->InsertOrUpdate(tmp);
				sprintf(tmp, "%s = %d", ATTR_RUNNING_JOBS,
						Owners[i].JobsRunning);
				m_ad->InsertOrUpdate(tmp);
				sprintf(tmp, "%s = %d", ATTR_FLOCKED_JOBS,
						Owners[i].JobsFlocked);
				m_ad->InsertOrUpdate(tmp);
				sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, Owners[i].Name,
						UidDomain);
				m_ad->InsertOrUpdate(tmp);
				flock_col->sendUpdate( UPDATE_SUBMITTOR_AD, m_ad, NULL, true );
			}
		}
	}

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


	 // send info about deleted owners
	 // put 0 for idle & running jobs

	sprintf(tmp, "%s = 0", ATTR_RUNNING_JOBS);
	m_ad->InsertOrUpdate(tmp);
	sprintf(tmp, "%s = 0", ATTR_IDLE_JOBS);
	m_ad->InsertOrUpdate(tmp);

 	// send ads for owner that don't have jobs idle
	// This is done by looking at the old owners list and searching for owners
	// that are not in the current list (the current list has only owners w/ idle jobs)
	for ( i=0; i<Old_N_Owners; i++) {

	  sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, OldOwners[i].Name, UidDomain);

		// check that the old name is not in the new list
		int k;
		for(k=0; k<N_Owners;k++) {
		  if (!strcmp(OldOwners[i].Name,Owners[k].Name)) break;
		}
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

	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  m_ad->InsertOrUpdate(tmp);

		// Update collectors
	  int num_udates = 
		  daemonCore->sendUpdates(UPDATE_SUBMITTOR_AD, m_ad, NULL, true);
	  dprintf(D_ALWAYS, "Sent owner (0 jobs) ad to %d collectors\n",
			  num_udates);

	  // also update all of the flock hosts
	  Daemon *da;
	  if( FlockCollectors ) {
		  int flock_level;
		  for( flock_level=1, FlockCollectors->rewind();
			   flock_level <= OldOwners[i].OldFlockLevel &&
				   FlockCollectors->next(da); flock_level++ ) {
			  ((DCCollector*)da)->sendUpdate( UPDATE_SUBMITTOR_AD, m_ad, NULL, true );
		  }
	  }
	}

	m_ad->SetMyTypeName( SCHEDD_ADTYPE );

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

/* 
 * renice_shadow() will nice the shadow if specified in the
 * condor_config.  the value of SHADOW_RENICE_INCREMENT will be added
 * to the current process priority (the higher the priority number,
 * the less CPU will be allocated).  renice_shadow() is meant to be
 * called by the child process after a fork() and before an exec().
 * it returns the value added to the priority, or 0 if the priority
 * did not change.  renice_shadow() now just calls renice_self() from
 * the C++ util that actually does the work, since other parts of
 * Condor might need to be reniced (namely, the user job).  -Derek
 * Wright, <wright@cs.wisc.edu> 4/14/98
 */
int
renice_shadow()
{
#ifdef WIN32
	return 0;
#else 
	return renice_self( "SHADOW_RENICE_INCREMENT" ); 
#endif
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
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) == 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) == 0) {
		universe = CONDOR_UNIVERSE_STANDARD;
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
		owner_buf2.sprintf("%s.%s",NiceUserName,owner);
		owner=owner_buf2.Value();
	}

	// increment our count of the number of job ads in the queue
	scheduler.JobsTotalAds++;

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
			// don't count REMOVED or HELD jobs
			if (status == IDLE || status == UNEXPANDED || status == RUNNING) {
				scheduler.SchedUniverseJobsRunning += cur_hosts;
				scheduler.SchedUniverseJobsIdle += (max_hosts - cur_hosts);
			}
		}
		if( universe == CONDOR_UNIVERSE_LOCAL ) 
		{
			// don't count REMOVED or HELD jobs
			if (status == IDLE || status == UNEXPANDED || status == RUNNING) {
				scheduler.LocalUniverseJobsRunning += cur_hosts;
				scheduler.LocalUniverseJobsIdle += (max_hosts - cur_hosts);
			}
		}
			// We want to record the cluster id of all idle MPI and parallel
		    // jobs

		if( (universe == CONDOR_UNIVERSE_MPI ||
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
		int needs_management = 0;
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
			char job_id[20];
			if ( job->LookupString( ATTR_GRID_JOB_ID, job_id,
									sizeof(job_id) ) )
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

	if (status == IDLE || status == UNEXPANDED || status == RUNNING) {
		scheduler.JobsRunning += cur_hosts;
		scheduler.JobsIdle += (max_hosts - cur_hosts);
		scheduler.Owners[OwnerNum].JobsIdle += (max_hosts - cur_hosts);
			// Don't update scheduler.Owners[OwnerNum].JobsRunning here.
			// We do it in Scheduler::count_jobs().
	} else if (status == HELD) {
		scheduler.JobsHeld++;
		scheduler.Owners[OwnerNum].JobsHeld++;
	} else if (status == REMOVED) {
		scheduler.JobsRemoved++;
	}

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
		case CONDOR_UNIVERSE_LOCAL:
			return false;
		default:
			return true;
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
			char jobID[20];
			if ( job_ad->LookupString( ATTR_GRID_JOB_ID, jobID,
									   sizeof(jobID) ) )
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

	if( (job_universe == CONDOR_UNIVERSE_PVM) || 
		(job_universe == CONDOR_UNIVERSE_MPI) || 
		(job_universe == CONDOR_UNIVERSE_PARALLEL) ) {
		job_id.proc = 0;		// PVM and MPI shadow is always associated with proc 0
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
			return;
		}

		// if we have already preempted this shadow, we're done.
		if ( srec->preempted ) {
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

			int handler_sig;
			const char* handler_sig_str;
			switch( action ) {
			case JA_HOLD_JOBS:
				handler_sig = SIGUSR1;
				handler_sig_str = "SIGUSR1";
				break;
			case JA_REMOVE_JOBS:
				handler_sig = SIGUSR1;
				handler_sig_str = "SIGUSR1";
				break;
			case JA_VACATE_JOBS:
				handler_sig = DC_SIGSOFTKILL;
				handler_sig_str = "DC_SIGSOFTKILL";
				break;
			case JA_VACATE_FAST_JOBS:
				handler_sig = DC_SIGHARDKILL;
				handler_sig_str = "DC_SIGHARDKILL";
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
				dprintf( D_FULLDEBUG, "This job does not have a match -- "
						 "It may be a PVM job.\n");
            }
			int shadow_sig;
			const char* shadow_sig_str;
			switch( action ) {
			case JA_HOLD_JOBS:
					// for now, use the same as remove
			case JA_REMOVE_JOBS:
				shadow_sig = SIGUSR1;
				shadow_sig_str = "SIGUSR1";
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
				msg.sprintf("Bad or missing credential for user: %s", owner.Value());
#else
				msg.sprintf("Unable to switch to user: %s", owner.Value());
#endif
				holdJob(job_id.cluster, job_id.proc, msg.Value(), 
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
		}

		if (mode == REMOVED) {
			srec->removed = TRUE;
		}

		return;
    }

	// If we made it here, we did not find a shadow or other job manager 
	// process for this job.  Just handle the operation ourselves.
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

	return;
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
		MyString hold_reason;
		jobad->LookupString(ATTR_HOLD_REASON,hold_reason);
		if( hold_reason == "Spooling input data files" ) {
			int cluster = -1, proc = -1;
			jobad->LookupInteger(ATTR_CLUSTER_ID, cluster);
			jobad->LookupInteger(ATTR_PROC_ID, proc);
			dprintf(D_FULLDEBUG,"Skipping periodic expressions for job %d.%d, because hold reason is '%s'\n",cluster,proc,hold_reason.Value());
			return 0;
		}
	}

	if( univ==CONDOR_UNIVERSE_SCHEDULER || univ==CONDOR_UNIVERSE_LOCAL ) {
		return 1;
	} else if(univ==CONDOR_UNIVERSE_GRID) {
		return 1;
	} else {
		switch(status) {
			case UNEXPANDED:
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

	PROC_ID job_id;
	job_id.cluster = cluster;
	job_id.proc = proc;

	UserPolicy policy;
	policy.Init(jobad);

	action = policy.AnalyzePolicy(PERIODIC_ONLY);

	// Build a "reason" string for logging
	MyString reason = policy.FiringReason();
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


// returns true if the sandbox already exists *and* is owned by the
// right owner, false if either of those conditions isn't true. 
bool
sandboxHasRightOwner( int cluster, int proc, ClassAd* job_ad )
{
	bool rval = true;  // we'll return false if needed...

	if( ! job_ad ) {
			// job is already gone, guess we don't need a thread. ;)
		return false;
	}

	MyString sandbox;
	if( ! getSandbox(cluster, proc, sandbox) ) {
		EXCEPT( "getSandbox(%d.%d) returned FALSE!", cluster, proc );
	}

	StatInfo si( sandbox.Value() );
	if( si.Error() == SINoFile ) {
			// sandbox doesn't yet exist, we'll need to create it
		FreeJobAd( job_ad );
		return false;
	}
	
		// if we got this far, we know the sandbox already exists for
		// this cluster/proc.  if we're not WIN32, check the owner.

#ifndef WIN32
		// check the owner of the sandbox vs. what's in the job ad
	uid_t sandbox_uid = si.GetOwner();
	passwd_cache* p_cache = pcache();
	uid_t job_uid;
	char* job_owner = NULL;
	job_ad->LookupString( ATTR_OWNER, &job_owner );
	if( ! job_owner ) {
		EXCEPT( "No %s for job %d.%d!", ATTR_OWNER, cluster, proc );
	}
	if( ! p_cache->get_user_uid(job_owner, job_uid) ) {
			// failed to find uid for this owner, badness.
		dprintf( D_ALWAYS, "Failed to find uid for user %s (job %d.%d), "
				 "job sandbox ownership will probably be broken\n", 
				 job_owner, cluster, proc );
		free( job_owner );
		FreeJobAd( job_ad );
		return false;
	}
	free( job_owner );
	job_owner = NULL;

		// now that we have the right uids, see if they match.
	rval = (job_uid == sandbox_uid);
	
#endif /* WIN32 */

	FreeJobAd( job_ad );
	return rval;
}


/*
  we want to be a little bit careful about this.  we don't want to go
  messing with the sandbox if a) we've got a partial (messed up)
  sandbox that hasn't finished being transfered yet or if b)
  condor_submit (or the SOAP submit interface, whatever) initializes
  these variables to 0.  we want a real value for stage_in_finish, and
  we want to make sure it's later than stage_in_start.  todd gets the
  credit for making this smarter, even though derek made the changes.

  however, todd gets the blame for breaking the grid universe case,
  since these values are being checked in the job spooling thread
  *BEFORE* stage_in_finish is ever initialized.  :)  so, we're going
  to ignore the "partially transfered" sandbox logic and just make
  sure we've got a real value for stage_in_start (which we do, even in
  grid universe, since the parent sets that before spawning the job
  spooling thread).  --derek 2005-03-30 

  and, if the job is using file transfer, we're going to create and
  use both the regular sandbox and the tmp sandbox, so we need to
  handle those cases, too.  the new shadow initializes a FileTransfer
  object no matter what the job classad says, so in fact, the only way
  we would *not* have a transfer sandbox is if we're a standard or PVM
  universe job...  --derek 2005-04-21
*/
bool
jobIsSandboxed( ClassAd * ad )
{
	ASSERT(ad);
	int stage_in_start = 0;
	int never_create_sandbox_expr = 0;
	// In the past, we created sandboxes (or not) based on the
	// universe in which a job is executing.  Now, we create a
	// sandbox only if we are in a universe that ordinarily
	// requires a sandbox AND if create_sandbox is true; note that
	// create_sandbox may be set to false by other attributes in
	// the job ad (see below).
	bool create_sandbox = true;
	
	ad->LookupInteger( ATTR_STAGE_IN_START, stage_in_start );
	if( stage_in_start > 0 ) {
		return true;
	}

	// 
	if( ad->EvalBool( ATTR_NEVER_CREATE_JOB_SANDBOX, NULL, never_create_sandbox_expr ) &&
	    never_create_sandbox_expr == TRUE ) {
	  // As this function stands now, we could return false here.
	  // But if the sandbox logic becomes more complicated in the
	  // future --- notably, if there might be a case in which
	  // we'd want to always create a sandbox even if
	  // ATTR_NEVER_CREATE_JOB_SANDBOX were set --- then we'd want
	  // to be sure to ensure that we weren't in such a case.

	  create_sandbox = false;
	}

	int univ = CONDOR_UNIVERSE_VANILLA;
	ad->LookupInteger( ATTR_JOB_UNIVERSE, univ );
	switch( univ ) {
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_STANDARD:
	case CONDOR_UNIVERSE_PVM:
	case CONDOR_UNIVERSE_GRID:
		return false;
		break;

	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_VM:
	  // True by default for jobs in these universes, but false if
	  // ATTR_NEVER_CREATE_JOB_SANDBOX is set in the job ad.
		return create_sandbox;
		break;

	default:
		dprintf( D_ALWAYS,
				 "ERROR: unknown universe (%d) in jobIsSandboxed()\n", univ );
		break;
	}
	return false;
}


bool
getSandbox( int cluster, int proc, MyString & path )
{
	const char * sandbox = gen_ckpt_name(Spool, cluster, proc, 0);
	if( ! sandbox ) {
		return false;
	}
	path = sandbox;
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

		/*
		  make sure we can switch uids.  if not, there's nothing to
		  do, so we should exit right away.

		  WARNING: if we ever add anything to this function that
		  doesn't require root/admin privledges, we'll also need to
		  change jobPrepNeedsThread()!
		*/
	if( ! can_switch_ids() ) {
		return TRUE;
	}


	// claim dynamic accounts here
	// NOTE: we only want to claim a dynamic account once, however,
	// this function is called *every* time we're about to spawn a job
	// handler.  so, this is the spot to be careful about this issue.

	ClassAd * job_ad = GetJobAd( cluster, proc );
	ASSERT( job_ad ); // No job ad?
	if( ! jobIsSandboxed(job_ad) ) {
			// nothing more to do...
		FreeJobAd( job_ad );
		return TRUE;
	}

	MyString sandbox;
	if( ! getSandbox(cluster, proc, sandbox) ) {
		dprintf( D_ALWAYS, "Failed to find sandbox for job %d.%d. "
				 "Cannot chown sandbox to user. Job may run into "
				 "permissions problems when it starts.\n", cluster, proc );
		FreeJobAd( job_ad );
		return FALSE;
	}

	MyString sandbox_tmp = sandbox.Value();
	sandbox_tmp += ".tmp";

#ifndef WIN32
	uid_t sandbox_uid;
	uid_t sandbox_tmp_uid;
#endif

	bool mkdir_rval = true;

		// if we got this far, we know we'll need a sandbox.  if it
		// doesn't yet exist, we have to create it as PRIV_CONDOR so
		// that spool can still be chmod 755...
	StatInfo si( sandbox.Value() );
	if( si.Error() == SINoFile ) {
		priv_state saved_priv = set_condor_priv();
		if( (mkdir(sandbox.Value(),0777) < 0) ) {
				// mkdir can return 17 = EEXIST (dirname exists) or 2
				// = ENOENT (path not found)
			dprintf( D_FULLDEBUG, "ERROR in aboutToSpawnJobHandler(): "
					 "mkdir(%s) failed: %s (errno: %d)\n",
					 sandbox.Value(), strerror(errno), errno );
			mkdir_rval = false;
		}
#ifndef WIN32
		sandbox_uid = get_condor_uid();
#endif
		set_priv( saved_priv );
	} else { 
#ifndef WIN32
			// sandbox already exists, check owner
	sandbox_uid = si.GetOwner();
#endif
	}

	StatInfo si_tmp( sandbox_tmp.Value() );
	if( si_tmp.Error() == SINoFile ) {
		priv_state saved_priv = set_condor_priv();
		if( (mkdir(sandbox_tmp.Value(),0777) < 0) ) {
				// mkdir can return 17 = EEXIST (dirname exists) or 2
				// = ENOENT (path not found)
			dprintf( D_FULLDEBUG, "ERROR in aboutToSpawnJobHandler(): "
					 "mkdir(%s) failed: %s (errno: %d)\n",
					 sandbox_tmp.Value(), strerror(errno), errno );
			mkdir_rval = false;
		}
#ifndef WIN32
		sandbox_tmp_uid = get_condor_uid();
#endif
		set_priv( saved_priv );
	} else { 
#ifndef WIN32
			// sandbox already exists, check owner
	sandbox_tmp_uid = si_tmp.GetOwner();
#endif
	}

	if( ! mkdir_rval ) {
		return FALSE;
	}

#ifndef WIN32

	MyString owner;
	job_ad->LookupString( ATTR_OWNER, owner );

	uid_t src_uid = get_condor_uid();
	uid_t dst_uid;
	gid_t dst_gid;
	passwd_cache* p_cache = pcache();
	if( ! p_cache->get_user_ids(owner.Value(), dst_uid, dst_gid) ) {
		dprintf( D_ALWAYS, "(%d.%d) Failed to find UID and GID for "
				 "user %s. Cannot chown %s to user. Job may run "
				 "into permissions problems when it starts.\n", 
				 cluster, proc, owner.Value(), sandbox.Value() );
		FreeJobAd( job_ad );
		return FALSE;
	}

	if( (sandbox_uid != dst_uid) && 
		!recursive_chown(sandbox.Value(),src_uid,dst_uid,dst_gid,true) )
	{
		dprintf( D_ALWAYS, "(%d.%d) Failed to chown %s from %d to %d.%d. "
				 "Job may run into permissions problems when it starts.\n",
				 cluster, proc, sandbox.Value(), src_uid, dst_uid, dst_gid );
	}

	if( (sandbox_tmp_uid != dst_uid) && 
		!recursive_chown(sandbox_tmp.Value(),src_uid,dst_uid,dst_gid,true) )
	{
		dprintf( D_ALWAYS, "(%d.%d) Failed to chown %s from %d to %d.%d. "
				 "Job may run into permissions problems when it starts.\n",
				 cluster, proc, sandbox_tmp.Value(), src_uid, dst_uid, 
				 dst_gid );
	}
	FreeJobAd(job_ad);
	job_ad = 0;
#else	/* WIN32 */

	MyString owner;
	job_ad->LookupString(ATTR_OWNER, owner);

	MyString nt_domain;

	job_ad->LookupString(ATTR_NT_DOMAIN, nt_domain);



	if (!recursive_chown(sandbox.Value(), owner.Value(), nt_domain.Value())) {

		dprintf( D_ALWAYS, "(%d.%d) Failed to chown %s from to %d\\%d. "

		         "Job may run into permissions problems when it starts.\n",

		         cluster, proc, sandbox.Value(), nt_domain.Value(), owner.Value() );

	}



	if (!recursive_chown(sandbox_tmp.Value(), owner.Value(), nt_domain.Value())) {

		dprintf( D_ALWAYS, "(%d.%d) Failed to chown %s from to %d\\%d. "

		         "Job may run into permissions problems when it starts.\n",

		         cluster, proc, sandbox.Value(), nt_domain.Value(), owner.Value() );

	}


#endif
	return 0;
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
		scheduler.spawnLocalStarter( srec );
		return true;
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
	int want_flush = 1;

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

			filename_template.sprintf( "%s/.condor_nfs_sync_XXXXXX",
									   iwd.Value() );
			sync_filename = strdup( filename_template.Value() );
			sync_fd = condor_mkstemp( sync_filename );
			if ( sync_fd >= 0 ) {
				close( sync_fd );
				unlink( sync_filename );
			}

			free( sync_filename );

			set_priv( priv );
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

	if( jobIsSandboxed(job_ad) ) {
		MyString sandbox;
		if( getSandbox(cluster, proc, sandbox) ) {
			uid_t src_uid = 0;
			uid_t dst_uid = get_condor_uid();
			gid_t dst_gid = get_condor_gid();

			MyString jobOwner;
			job_ad->LookupString( ATTR_OWNER, jobOwner );

			passwd_cache* p_cache = pcache();
			if( p_cache->get_user_uid( jobOwner.Value(), src_uid ) ) {
				if( ! recursive_chown(sandbox.Value(), src_uid,
									  dst_uid, dst_gid, true) )
				{
					dprintf( D_FULLDEBUG, "(%d.%d) Failed to chown %s from "
							 "%d to %d.%d.  User may run into permissions "
							 "problems when fetching sandbox.\n", 
							 cluster, proc, sandbox.Value(),
							 src_uid, dst_uid, dst_gid );
				}
			} else {
				dprintf( D_ALWAYS, "(%d.%d) Failed to find UID and GID "
						 "for user %s.  Cannot chown \"%s\".  User may "
						 "run into permissions problems when fetching "
						 "job sandbox.\n", cluster, proc, jobOwner.Value(),
						 sandbox.Value() );
			}
		} else {
			dprintf( D_ALWAYS, "(%d.%d) Failed to find sandbox for this "
					 "job.  Cannot chown sandbox to user.  User may run "
					 "into permissions problems when fetching sandbox.\n",
					 cluster, proc );
		}
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


// Initialize a WriteUserLog object for a given job and return a pointer to
// the WriteUserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a WriteUserLog, so you must check for NULL before
// using the pointer you get back.
WriteUserLog*
Scheduler::InitializeUserLog( PROC_ID job_id ) 
{
	MyString logfilename;
	ClassAd *ad = GetJobAd(job_id.cluster,job_id.proc);
	if( getPathToUserLog(ad, logfilename)==false )
	{			
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

	dprintf( D_FULLDEBUG, 
			 "Writing record to user logfile=%s owner=%s\n",
			 logfilename.Value(), owner.Value() );

	WriteUserLog* ULog=new WriteUserLog();
	if (0 <= GetAttributeBool(job_id.cluster, job_id.proc,
							  ATTR_ULOG_USE_XML, &use_xml)
		&& 1 == use_xml) {
		ULog->setUseXML(true);
	} else {
		ULog->setUseXML(false);
	}
	ULog->setCreatorName( Name );
	if (ULog->initialize(owner.Value(), domain.Value(), logfilename.Value(), job_id.cluster, job_id.proc, 0, gjid.Value())) {
		return ULog;
	} else {
		dprintf ( D_ALWAYS,
				"WARNING: Invalid user log file specified: %s\n", logfilename.Value());
		delete ULog;
		return NULL;
	}
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
	strcpy( event.executeHost, host );
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


int
Scheduler::abort_job(int, Stream* s)
{
	PROC_ID	job_id;
	int nToRemove = -1;

	// First grab the number of jobs to remove/hold
	if ( !s->code(nToRemove) ) {
		dprintf(D_ALWAYS,"abort_job() can't read job count\n");
		return FALSE;
	}

	if ( nToRemove > 0 ) {
		// We are being told how many and which jobs to abort

		dprintf(D_FULLDEBUG,"abort_job: asked to abort %d jobs\n",nToRemove);

		while ( nToRemove > 0 ) {
			if( !s->code(job_id) ) {
				dprintf( D_ALWAYS, "abort_job() can't read job_id #%d\n",
					nToRemove);
				return FALSE;
			}
			abort_job_myself(job_id, JA_REMOVE_JOBS, false, true );
			nToRemove--;
		}
		s->end_of_message();
	} else {
		// We are being told to scan the queue ourselves and abort
		// any jobs which have a status = REMOVED or HELD
		ClassAd *job_ad;
		static bool already_removing = false;	// must be static!!!
		char constraint[120];

		// This could take a long time if the queue is large; do the
		// end_of_message first so condor_rm does not timeout. We do not
		// need any more info off of the socket anyway.
		s->end_of_message();

		dprintf(D_FULLDEBUG,"abort_job: asked to abort all status REMOVED/HELD jobs\n");

		// if already_removing is true, it means the user sent a second condor_rm
		// command before the first condor_rm command completed, and we are
		// already in the below job scan/removal loop in a different stack frame.
		// so we should just return here.
		if ( already_removing ) {
			return TRUE;
		}

		snprintf(constraint,120,"%s == %d || %s == %d",ATTR_JOB_STATUS,REMOVED,
				 ATTR_JOB_STATUS,HELD);

		job_ad = GetNextJobByConstraint(constraint,1);
		if ( job_ad ) {
			already_removing = true;
		}
		while ( job_ad ) {
			if ( (job_ad->LookupInteger(ATTR_CLUSTER_ID,job_id.cluster) == 1) &&
				 (job_ad->LookupInteger(ATTR_PROC_ID,job_id.proc) == 1) ) {

				 abort_job_myself(job_id, JA_REMOVE_JOBS, false, true );

			}
			FreeJobAd(job_ad);

			job_ad = GetNextJobByConstraint(constraint,0);
		}
		already_removing = false;
	}

	return TRUE;
}

int
Scheduler::transferJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs;
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

	if (exit_status == FALSE) {
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
	ExtArray<PROC_ID> *jobs;
		// These three lists must be kept in sync!
	static const int ATTR_ARRAY_SIZE = 8;
	static const char *AttrsToModify[ATTR_ARRAY_SIZE] = { 
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY };
	static const bool AttrIsList[ATTR_ARRAY_SIZE] = {
		false,
		false,
		false,
		false,
		true,
		true,
		false,
		false };
	static const char *AttrXferBool[ATTR_ARRAY_SIZE] = {
		ATTR_TRANSFER_EXECUTABLE,
		ATTR_TRANSFER_INPUT,
		ATTR_TRANSFER_OUTPUT,
		ATTR_TRANSFER_ERROR,
		NULL,
		NULL,
		NULL,
		NULL };

	dprintf(D_FULLDEBUG,"spoolJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

	time_t now = time(NULL);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,"ERROR - JobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (exit_status == FALSE) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		delete jobs;
		return FALSE;
	}


	int jobIndex,cluster,proc,attrIndex;
	char new_attr_value[500];
	char *buf = NULL;
	ExprTree *expr = NULL;
	char *SpoolSpace = NULL;
		// figure out how many jobs we're dealing with
	int len = (*jobs).getlast() + 1;


		// For each job, modify its ClassAd
	for (jobIndex = 0; jobIndex < len; jobIndex++) {
		cluster = (*jobs)[jobIndex].cluster;
		proc = (*jobs)[jobIndex].proc;

		ClassAd *job_ad = GetJobAd(cluster,proc);
		if (!job_ad) {
			// didn't find this job ad, must've been removed?
			// just go to the next one
			continue;
		}
		if ( SpoolSpace ) free(SpoolSpace);
		SpoolSpace = strdup( gen_ckpt_name(Spool,cluster,proc,0) );
		ASSERT(SpoolSpace);

		BeginTransaction();

			// Backup the original IWD at submit time
		if (buf) free(buf);
		buf = NULL;
		job_ad->LookupString(ATTR_JOB_IWD,&buf);
		if ( buf ) {
			snprintf(new_attr_value,500,"SUBMIT_%s",ATTR_JOB_IWD);
			SetAttributeString(cluster,proc,new_attr_value,buf);
			free(buf);
			buf = NULL;
		}
			// Modify the IWD to point to the spool space			
		SetAttributeString(cluster,proc,ATTR_JOB_IWD,SpoolSpace);

			// Backup the original TRANSFER_OUTPUT_REMAPS at submit time
		expr = job_ad->Lookup(ATTR_TRANSFER_OUTPUT_REMAPS);
		snprintf(new_attr_value,500,"SUBMIT_%s",ATTR_TRANSFER_OUTPUT_REMAPS);
		if ( expr ) {
			char *remap_buf = NULL;
			ASSERT( expr->RArg() );
			expr->RArg()->PrintToNewStr(&remap_buf);
			ASSERT(remap_buf);
			SetAttribute(cluster,proc,new_attr_value,remap_buf);
			free(remap_buf);
		}
		else if(job_ad->Lookup(new_attr_value)) {
				// SUBMIT_TransferOutputRemaps is defined, but
				// TransferOutputRemaps is not; disable the former,
				// so that when somebody fetches the sandbox, nothing
				// gets remapped.
			SetAttribute(cluster,proc,new_attr_value,"Undefined");
		}
			// Set TRANSFER_OUTPUT_REMAPS to Undefined so that we don't
			// do remaps when the job's output files come back into the
			// spool space. We only want to remap when the submitter
			// retrieves the files.
		SetAttribute(cluster,proc,ATTR_TRANSFER_OUTPUT_REMAPS,"Undefined");

			// Now, for all the attributes listed in 
			// AttrsToModify, change them to be relative to new IWD
			// by taking the basename of all file paths.
		for ( attrIndex = 0; attrIndex < ATTR_ARRAY_SIZE; attrIndex++ ) {
				// Lookup original value
			bool xfer_it;
			if (buf) free(buf);
			buf = NULL;
			job_ad->LookupString(AttrsToModify[attrIndex],&buf);
			if (!buf) {
				// attribute not found, so no need to modify it
				continue;
			}
			if ( nullFile(buf) ) {
				// null file -- no need to modify it
				continue;
			}
			if ( AttrXferBool[attrIndex] &&
				 job_ad->LookupBool( AttrXferBool[attrIndex], xfer_it ) && !xfer_it ) {
					// ad says not to transfer this file, so no need
					// to modify it
				continue;
			}
				// Create new value - deal with the fact that
				// some of these attributes contain a list of pathnames
			StringList old_paths(NULL,",");
			StringList new_paths(NULL,",");
			if ( AttrIsList[attrIndex] ) {
				old_paths.initializeFromString(buf);
			} else {
				old_paths.insert(buf);
			}
			old_paths.rewind();
			char *old_path_buf;
			bool changed = false;
			const char *base = NULL;
			MyString new_path_buf;
			while ( (old_path_buf=old_paths.next()) ) {
				base = condor_basename(old_path_buf);
				if ( strcmp(base,old_path_buf)!=0 ) {
					new_path_buf.sprintf(
						"%s%c%s",SpoolSpace,DIR_DELIM_CHAR,base);
					base = new_path_buf.Value();
					changed = true;
				}
				new_paths.append(base);
			}
			if ( changed ) {
					// Backup original value
				snprintf(new_attr_value,500,"SUBMIT_%s",AttrsToModify[attrIndex]);
				SetAttributeString(cluster,proc,new_attr_value,buf);
					// Store new value
				char *new_value = new_paths.print_to_string();
				ASSERT(new_value);
				SetAttributeString(cluster,proc,AttrsToModify[attrIndex],new_value);
				free(new_value);
			}
		}

			// Set ATTR_STAGE_IN_FINISH if not already set.
		int spool_completion_time = 0;
		job_ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
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
	if (SpoolSpace) free(SpoolSpace);
	if (buf) free(buf);
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
		if ( !rsock->code(JobAdsArrayLen) || !rsock->eom() ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
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
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
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
		ad->dPrint(D_ALWAYS);

			// Create a file transfer object, with schedd as the server.
			// If we're receiving files, don't create a file catalog in
			// the FileTransfer object. The submitter's IWD is probably not
			// valid on this machine and we won't use the catalog anyway.
		result = ftrans.SimpleInit(ad, true, true, rsock, PRIV_UNKNOWN,
								   (mode == TRANSFER_DATA ||
									mode == TRANSFER_DATA_WITH_PERMS));
		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
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
		} else {
			// send sandbox out of the schedd
			rsock->encode();
			// first send the classad for the job
			result = ad->put(*rsock);
			if (!result) {
				dprintf(D_ALWAYS, "generalJobFilesWorkerThread(): "
					"failed to send job ad for job %d.%d \n",
					cluster,proc );
			} else {
				rsock->eom();
				// and then upload the files
				result = ftrans.UploadFiles();
			}
		}

		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to transfer files for job %d.%d \n",
					 cluster,proc );
			s->timeout( 10 ); // avoid hanging due to huge timeout
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
	}	
		
		
	rsock->eom();

	int answer;
	if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
		rsock->encode();
		answer = OK;
	} else {
		rsock->decode();
		answer = -1;
	}
	rsock->code(answer);
	rsock->eom();
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

	if (answer == OK ) {
		return TRUE;
	} else {
		return FALSE;
	}
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
			dprintf( D_ALWAYS, "spoolJobFiles() aborting: %s\n",
					 errstack.getFullText() );
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
				dprintf( D_ALWAYS,
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
					dprintf( D_ALWAYS, "spoolJobFiles(): "
						 	"failed to read JobAdsArrayLen (%d)\n",
							JobAdsArrayLen );
					refuse(s);
					return FALSE;
			}
			if ( JobAdsArrayLen <= 0 ) {
				dprintf( D_ALWAYS, "spoolJobFiles(): "
					 	"read bad JobAdsArrayLen value %d\n", JobAdsArrayLen );
				refuse(s);
				return FALSE;
			}
			rsock->eom();
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
					dprintf( D_ALWAYS, "spoolJobFiles(): "
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

						// Must not allow stagein to happen more than
						// once, because this could screw up
						// subsequent operations, such as rewriting of
						// paths in the ClassAd and the job being in
						// the middle of using the files.
					int finish_time;
					if( GetAttributeInt(a_job.cluster,a_job.proc,
					    ATTR_STAGE_IN_FINISH,&finish_time) >= 0 ) {
						dprintf( D_ALWAYS, "spoolJobFiles(): cannot allow"
						         " stagein for job %d.%d, because stagein"
						         " already finished for this job.\n",
						         a_job.cluster, a_job.proc);
						unsetQSock();
						return FALSE;
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

	rsock->eom();

		// DaemonCore will free the thread_arg for us when the thread
		// exits, but we need to free anything pointed to by
		// job_data_transfer_t ourselves. generalJobFilesWorkerThread()
		// will free 'peer_version' and our reaper will free 'jobs' (the
		// reaper needs 'jobs' for some of its work).
	job_data_transfer_t *thread_arg = (job_data_transfer_t *)malloc( sizeof(job_data_transfer_t) );
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
			dprintf( D_ALWAYS, "updateGSICred(%d) aborting: %s\n", cmd,
					 errstack.getFullText() );
			refuse( s );
			return FALSE;
		}
	}	


		// read the job id from the client
	rsock->decode();
	if ( !rsock->code(jobid) || !rsock->eom() ) {
			dprintf( D_ALWAYS, "updateGSICred(%d): "
					 "failed to read job id\n", cmd );
			refuse(s);
			return FALSE;
	}
	dprintf(D_FULLDEBUG,"updateGSICred(%d): read job id %d.%d\n",
		cmd,jobid.cluster,jobid.proc);
	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if ( !jobad ) {
		dprintf( D_ALWAYS, "updateGSICred(%d): failed, "
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
		dprintf( D_ALWAYS, "updateGSICred(%d): failed, "
				 "user %s not authorized to edit job %d.%d\n", cmd,
				 rsock->getFullyQualifiedUser(),jobid.cluster, jobid.proc );
		refuse(s);
		return FALSE;
	}

		// Make certain this job has a x509 proxy, and that this 
		// proxy is sitting in the SPOOL directory
	char* SpoolSpace = strdup(gen_ckpt_name(Spool,jobid.cluster,jobid.proc,0));
	ASSERT(SpoolSpace);
	char *proxy_path = NULL;
	jobad->LookupString(ATTR_X509_USER_PROXY,&proxy_path);
	if ( !proxy_path || strncmp(SpoolSpace,proxy_path,strlen(SpoolSpace)) ) {
		dprintf( D_ALWAYS, "updateGSICred(%d): failed, "
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
		dprintf( D_ALWAYS, "updateGSICred(%d): failed, "
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
		EXCEPT( "No %s for job %d.%d!", ATTR_OWNER, jobid.cluster,
				jobid.proc );
	}
	if ( !p_cache->get_user_uid( job_owner, job_uid ) ) {
			// Failed to find uid for this owner, badness.
		dprintf( D_ALWAYS, "Failed to find uid for user %s (job %d.%d)\n",
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
			dprintf( D_ALWAYS, "init_user_ids() failed for user %s!\n",
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
		}
	}

#ifndef WIN32
		// Now switch back to our old priv state
	set_priv( priv );
#endif

		// Send our reply back to the client
	rsock->encode();
	rsock->code(reply);
	rsock->eom();

	dprintf(D_ALWAYS,"Refresh GSI cred for job %d.%d %s\n",
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
			dprintf( D_ALWAYS, "actOnJobs() aborting: %s\n",
					 errstack.getFullText() );
			refuse( s );
			return FALSE;
		}
	}

		// read the command ClassAd + EOM
	if( ! (command_ad.initFromStream(*rsock) && rsock->eom()) ) {
		dprintf( D_ALWAYS, "Can't read command ad from tool\n" );
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
							 JA_VACATE_JOBS, or JA_VACATE_FAST_JOBS
		   ATTR_ACTION_RESULT_TYPE - either AR_TOTALS or AR_LONG
		   and one of:
		   ATTR_ACTION_CONSTRAINT - a string with a ClassAd constraint 
		   ATTR_ACTION_IDS - a string with a comma seperated list of
		                     job ids to act on

		   In addition, it might also include:
		   ATTR_NOTIFY_JOB_SCHEDULER (true or false)
		   and one of: ATTR_REMOVE_REASON, ATTR_RELEASE_REASON, or
		               ATTR_HOLD_REASON

		*/
	if( ! command_ad.LookupInteger(ATTR_JOB_ACTION, action_num) ) {
		dprintf( D_ALWAYS, 
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
	case JA_VACATE_JOBS:
	case JA_VACATE_FAST_JOBS:
			// no new_status needed.  also, we're not touching
			// anything in the job queue, so we don't need a
			// transaction, either...
		needs_transaction = false;
		break;
	default:
		dprintf( D_ALWAYS, "actOnJobs(): ClassAd contains invalid "
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
			EXCEPT( "Out of memory!" );
		}
		sprintf( reason, "\"%s (by user %s)\"", tmp, owner );
		free( tmp );
		tmp = NULL;
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
	ExprTree *tree, *rhs;
	tree = command_ad.Lookup(ATTR_ACTION_CONSTRAINT);
	if( tree ) {
		rhs = tree->RArg();
		if( ! rhs ) {
				// TODO: deal with this kind of error
			free(reason);
			return false;
		}
		rhs->PrintToNewStr( &tmp );

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
				// Only release held jobs
			snprintf( buf, 256, "(%s==%d) && (", ATTR_JOB_STATUS, HELD );
			break;
		case JA_VACATE_JOBS:
		case JA_VACATE_FAST_JOBS:
				// Only vacate running jobs
			snprintf( buf, 256, "(%s==%d) && (", ATTR_JOB_STATUS, RUNNING );
			break;
		default:
			EXCEPT( "impossible: unknown action (%d) in actOnJobs() after "
					"it was already recognized", action_num );
		}
		int size = strlen(buf) + strlen(tmp) + 3;
		constraint = (char*) malloc( size * sizeof(char) );
		if( ! constraint ) {
			EXCEPT( "Out of memory!" );
		}
			// we need to terminate the ()'s after their constraint
		snprintf( constraint, size, "%s%s)", buf, tmp );
		free( tmp );
	} else {
		constraint = NULL;
	}
	tmp = NULL;
	if( command_ad.LookupString(ATTR_ACTION_IDS, &tmp) ) {
		if( constraint ) {
			dprintf( D_ALWAYS, "actOnJobs(): "
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

		ClassAd* job_ad;
		job_ad = GetNextJobByConstraint( constraint, 1 );
		for( job_ad = GetNextJobByConstraint( constraint, 1 );
		     job_ad;
		     job_ad = GetNextJobByConstraint( constraint, 0 ))
		{
			if(	job_ad->LookupInteger(ATTR_CLUSTER_ID,tmp_id.cluster) &&
				job_ad->LookupInteger(ATTR_PROC_ID,tmp_id.proc) ) 
			{
				if( action == JA_VACATE_JOBS || 
					action == JA_VACATE_FAST_JOBS ) 
				{
						// vacate is a special case, since we're not
						// trying to modify the job in the queue at
						// all, so we just need to make sure we're
						// authorized to vacate this job, and if so,
						// record that we found this job_id and we're
						// done.
					if( !OwnerCheck(m_ad, rsock->getOwner()) ) {
						results.record( tmp_id, AR_PERMISSION_DENIED );
						continue;
					}
					results.record( tmp_id, AR_SUCCESS );
					jobs[num_matches] = tmp_id;
					num_matches++;
					continue;
				}

				if( action == JA_RELEASE_JOBS ) {
					new_status = IDLE;
					GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
							ATTR_JOB_STATUS_ON_RELEASE, &new_status);
				}
				if( action == JA_REMOVE_X_JOBS ) {
					if( SetAttribute( tmp_id.cluster, tmp_id.proc,
									  ATTR_JOB_LEAVE_IN_QUEUE, "False" ) < 0 ) {
						results.record( tmp_id, AR_PERMISSION_DENIED );
						continue;
					}
				}
				if( action == JA_HOLD_JOBS ) {
					int old_status = IDLE;
					GetAttributeInt( tmp_id.cluster, tmp_id.proc,
									 ATTR_JOB_STATUS, &old_status );
					if ( old_status == REMOVED &&
						 SetAttributeInt( tmp_id.cluster, tmp_id.proc,
										  ATTR_JOB_STATUS_ON_RELEASE,
										  old_status ) < 0 ) {
						results.record( tmp_id, AR_PERMISSION_DENIED );
						continue;
					}
					if( SetAttributeInt( tmp_id.cluster, tmp_id.proc,
										 ATTR_HOLD_REASON_CODE,
										 CONDOR_HOLD_CODE_UserRequest ) < 0 ) {
						results.record( tmp_id, AR_PERMISSION_DENIED );
						continue;
					}
				}
				if( SetAttributeInt(tmp_id.cluster, tmp_id.proc,
									ATTR_JOB_STATUS, new_status) < 0 ) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					continue;
				}
				if( reason ) {
					if( SetAttribute(tmp_id.cluster, tmp_id.proc,
									 reason_attr_name, reason) < 0 ) {
							// TODO: record failure in response ad?
					}
				}
				SetAttributeInt( tmp_id.cluster, tmp_id.proc,
								 ATTR_ENTERED_CURRENT_STATUS, now );
				fixReasonAttrs( tmp_id, action );
				results.record( tmp_id, AR_SUCCESS );
				jobs[num_matches] = tmp_id;
				num_matches++;
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

				// Check to make sure the job's status makes sense for
				// the command we're trying to perform
			int status;
			int on_release_status = IDLE;
			if( GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
								ATTR_JOB_STATUS, &status) < 0 ) {
				results.record( tmp_id, AR_NOT_FOUND );
				continue;
			}
			switch( action ) {
			case JA_VACATE_JOBS:
			case JA_VACATE_FAST_JOBS:
				if( status != RUNNING ) {
					results.record( tmp_id, AR_BAD_STATUS );
					continue;
				}
				break;
			case JA_RELEASE_JOBS:
				if( status != HELD ) {
					results.record( tmp_id, AR_BAD_STATUS );
					continue;
				}
				GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
							ATTR_JOB_STATUS_ON_RELEASE, &on_release_status);
				new_status = on_release_status;
				break;
			case JA_REMOVE_JOBS:
				if( status == REMOVED ) {
					results.record( tmp_id, AR_ALREADY_DONE );
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
					continue;
				}
					// set LeaveJobInQueue to false...
				if( SetAttribute( tmp_id.cluster, tmp_id.proc,
								  ATTR_JOB_LEAVE_IN_QUEUE, "False" ) < 0 ) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					continue;
				}
				break;
			case JA_HOLD_JOBS:
				if( status == HELD ) {
					results.record( tmp_id, AR_ALREADY_DONE );
					continue;
				}
				if( SetAttributeInt( tmp_id.cluster, tmp_id.proc,
									 ATTR_HOLD_REASON_CODE,
									 CONDOR_HOLD_CODE_UserRequest ) < 0 ) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					continue;
				}
				break;
			default:
				EXCEPT( "impossible: unknown action (%d) in actOnJobs() "
						"after it was already recognized", action_num );
			}

				// Ok, we're happy, do the deed.
			if( action == JA_VACATE_JOBS || action == JA_VACATE_FAST_JOBS ) {
					// vacate is a special case, since we're not
					// trying to modify the job in the queue at
					// all, so we just need to make sure we're
					// authorized to vacate this job, and if so,
					// record that we found this job_id and we're
					// done.
				ClassAd *cad = GetJobAd( tmp_id.cluster, tmp_id.proc, false );
				if( ! cad ) {
					EXCEPT( "impossible: GetJobAd(%d.%d) returned false "
							"yet GetAttributeInt(%s) returned success",
							tmp_id.cluster, tmp_id.proc, ATTR_JOB_STATUS );
				}
				if( !OwnerCheck(cad, rsock->getOwner()) ) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					continue;
				}
				results.record( tmp_id, AR_SUCCESS );
				jobs[num_matches] = tmp_id;
				num_matches++;
				continue;
			}
			if( SetAttributeInt(tmp_id.cluster, tmp_id.proc,
								ATTR_JOB_STATUS, new_status) < 0 ) {
				results.record( tmp_id, AR_PERMISSION_DENIED );
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
			jobs[num_matches] = tmp_id;
			num_matches++;
		}
	}

	if( reason ) { free( reason ); }

	ClassAd* response_ad;

	response_ad = results.publishResults();

		// Let them know what action we performed in the reply
	response_ad->Assign( ATTR_JOB_ACTION, action );

		// Set a single attribute which says if the action succeeded
		// on at least one job or if it was a total failure
	response_ad->Assign( ATTR_ACTION_RESULT, num_matches ? 1:0 );

		// Finally, let them know if the user running this command is
		// a queue super user here
	response_ad->Assign( ATTR_IS_QUEUE_SUPER_USER,
			 isQueueSuperUser(rsock->getOwner()) ? true : false );
	
	rsock->encode();
	if( ! (response_ad->put(*rsock) && rsock->eom()) ) {
			// Failed to send reply, the client might be dead, so
			// abort our transaction.
		dprintf( D_ALWAYS, 
				 "actOnJobs: couldn't send results to client: aborting\n" );
		if( needs_transaction ) {
			AbortTransaction();
		}
		unsetQSock();
		return FALSE;
	}

	if( num_matches == 0 ) {
			// We didn't do anything, so we want to bail out now...
		dprintf( D_FULLDEBUG, 
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
	if( ! (rsock->code(reply) && rsock->eom() && reply == OK) ) {
			// we couldn't get the reply, or they told us to bail
		dprintf( D_ALWAYS, "actOnJobs: client not responding: aborting\n" );
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
	rsock->eom();

		// Now that we know the events are logged and commited to
		// the queue, we can do the final actions for these jobs,
		// like killing shadows if needed...
	for( i=0; i<num_matches; i++ ) {
		enqueueActOnJobMyself( jobs[i], action, notify );
	}
	return TRUE;
}

class ActOnJobRec: public ServiceData {
public:
	ActOnJobRec(PROC_ID job_id, JobAction action, bool notify):
		m_job_id(job_id.cluster,job_id.proc,-1), m_action(action), m_notify(notify) {}

	CondorID m_job_id;
	JobAction m_action;
	bool m_notify;

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
Scheduler::enqueueActOnJobMyself( PROC_ID job_id, JobAction action, bool notify )
{
	ActOnJobRec *act_rec = new ActOnJobRec( job_id, action, notify );
	bool stopping_job = false;

	if( action == JA_HOLD_JOBS ||
		action == JA_REMOVE_JOBS ||
		action == JA_VACATE_JOBS ||
		action == JA_VACATE_FAST_JOBS )
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

int
Scheduler::actOnJobMyselfHandler( ServiceData* data )
{
	ActOnJobRec *act_rec = (ActOnJobRec *)data;

	JobAction action = act_rec->m_action;
	bool notify = act_rec->m_notify;
	PROC_ID job_id;
	job_id.cluster = act_rec->m_job_id._cluster;
	job_id.proc = act_rec->m_job_id._proc;

	delete act_rec;

	switch( action ) {
	case JA_HOLD_JOBS:
	case JA_REMOVE_JOBS:
	case JA_VACATE_JOBS:
	case JA_VACATE_FAST_JOBS: {
		abort_job_myself( job_id, action, true, notify );		
#ifdef WIN32
			/*	This is a small patch so when DAGMan jobs are removed
				on Win32, jobs submitted by the DAGMan are removed as well.
				This patch is small and acceptable for the 6.8 stable series,
				but for v6.9 and beyond we should remove this patch and have things
				work on Win32 the same way they work on Unix.  However, doing this
				was deemed to much code churning for a stable series, thus this
				simpler but temporary patch.  -Todd 8/2006.
			*/
		int job_universe = CONDOR_UNIVERSE_MIN;
		GetAttributeInt(job_id.cluster, job_id.proc, 
						ATTR_JOB_UNIVERSE, &job_universe);
		if ( job_universe == CONDOR_UNIVERSE_SCHEDULER ) {
			MyString constraint;
			constraint.sprintf( "%s == %d", ATTR_DAGMAN_JOB_ID,
								job_id.cluster );
			abortJobsByConstraint(constraint.Value(),
				"removed because controlling DAGMan was removed",
				true);
		}
#endif
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
		break;
    }
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
	s->eom();
}


int
Scheduler::negotiatorSocketHandler (Stream *stream)
{
	int command = -1;
	int rval;

	dprintf (D_ALWAYS, "Activity on stashed negotiator socket\n");

	// attempt to read a command off the stream
	stream->decode();
	if (!stream->code(command))
	{
		dprintf (D_ALWAYS, "Socket activated, but could not read command\n");
		dprintf (D_ALWAYS, "(Negotiator probably invalidated cached socket)\n");
		return (!KEEP_STREAM);
	}

	rval = negotiate(command, stream);
	return rval;	
}

int
Scheduler::doNegotiate (int i, Stream *s)
{
	int rval = negotiate(i, s);
	if (rval == KEEP_STREAM)
	{
		dprintf (D_FULLDEBUG,
				 "Stashing socket to negotiator for future reuse\n");
		daemonCore->
				Register_Socket(s, "<Negotiator Socket>", 
				(SocketHandlercpp)&Scheduler::negotiatorSocketHandler,
				"<Negotiator Command>",
				this, ALLOW);
	}
	return rval;
}


/* 
   Helper function used by both DedicatedScheduler::negotiate() and
   Scheduler::negotiate().  This checks all the various reasons why we
   might not be able to or want to start another shadow
   (MAX_JOBS_RUNNING, swap space problems, etc), and returns true if
   we can proceed or false if we can't start another shadow.  */
bool
Scheduler::canSpawnShadow( int started_jobs, int total_jobs )
{
	int idle_jobs = total_jobs - started_jobs;

		// First, check if we have reached our maximum # of shadows 
	if( CurNumActiveShadows >= MaxJobsRunning ) {
		dprintf( D_ALWAYS, "Reached MAX_JOBS_RUNNING: no more can run, %d jobs matched, "
				 "%d jobs idle\n", started_jobs, idle_jobs ); 
		return false;
	}

	if( ReservedSwap == 0 ) {
			// We're not supposed to care about swap space at all, so
			// none of the rest of the checks matter at all.
		return true;
	}

		// Now, see if we ran out of swap space already.
	if( SwapSpaceExhausted) {
		dprintf( D_ALWAYS, "Swap space exhausted! No more jobs can be run!\n" );
        dprintf( D_ALWAYS, "    Solution: get more swap space, or set RESERVED_SWAP = 0\n" );
        dprintf( D_ALWAYS, "    %d jobs matched, %d jobs idle\n", started_jobs, idle_jobs ); 
		return false;
	}

	if( ShadowSizeEstimate && started_jobs >= MaxShadowsForSwap ) {
		dprintf( D_ALWAYS, "Swap space estimate reached! No more jobs can be run!\n" );
        dprintf( D_ALWAYS, "    Solution: get more swap space, or set RESERVED_SWAP = 0\n" );
        dprintf( D_ALWAYS, "    %d jobs matched, %d jobs idle\n", started_jobs, idle_jobs ); 
		return false;
	}
	
		// We made it.  Everything's cool.
	return true;
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
	int		i;
	int		op = -1;
	PROC_ID	id;
	char*	claim_id = NULL;			// claim_id for each match made
	char*	host = NULL;
	char*	sinful = NULL;
	int		jobs;						// # of jobs that CAN be negotiated
	int		cur_cluster = -1;
	int		cur_hosts;
	int		max_hosts;
	int		host_cnt;
	int		perm_rval;
	int		shadow_num_increment;
	int		job_universe;
	int		which_negotiator = 0; 		// >0 implies flocking
	char*	negotiator_name = NULL;	// hostname of negotiator when flocking
	Daemon*	neg_host = NULL;	
	int		owner_num;
	int		JobsRejected = 0;
	Sock*	sock = (Sock*)s;
	ContactStartdArgs * args = NULL;
	match_rec *mrec;
	ClassAd* my_match_ad;
	ClassAd* cad;
	bool cant_spawn_shadow = false;
	bool skip_negotiation = false;
	match_rec *pre_existing_match = NULL;
	int num_pre_existing_matches_received = 0;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

	// since this is the socket from the negotiator, the only command that can
	// come in at this point is NEGOTIATE.  If we get something else, something
	// goofy in going on.
	if (command != NEGOTIATE && command != NEGOTIATE_WITH_SIGATTRS)
	{
		dprintf(D_ALWAYS,
				"Negotiator command was %d (not NEGOTIATE) --- aborting\n", command);
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

	// BIOTECH
	bool	biotech = false;
	char *bio_param = param("BIOTECH");
	if (bio_param) {
		free(bio_param);
		biotech = true;
	}
	
		//
		// CronTab Jobs
		//
	this->calculateCronTabSchedules();		

	if (FlockNegotiators) {
		// first, check if this is our local negotiator
		struct in_addr endpoint_addr = (sock->peer_addr())->sin_addr;
		struct hostent *hent;
		bool match = false;
		Daemon negotiator (DT_NEGOTIATOR);
		char *negotiator_hostname = negotiator.fullHostname();
		if (!negotiator_hostname) {
			dprintf(D_ALWAYS, "Negotiator hostname lookup failed!\n");
			return (!(KEEP_STREAM));
		}
		hent = condor_gethostbyname(negotiator_hostname);
		if (!hent) {
			dprintf(D_ALWAYS, "gethostbyname for local negotiator (%s) failed!"
					"  Aborting negotiation.\n", negotiator_hostname);
			return (!(KEEP_STREAM));
		}
		char *addr;
		if (hent->h_addrtype == AF_INET) {
			for (int a=0; !match && (addr = hent->h_addr_list[a]); a++) {
				if (memcmp(addr, &endpoint_addr, sizeof(struct in_addr)) == 0){
					match = true;
				}
			}
		}
		// if it isn't our local negotiator, check the FlockNegotiators list.
		if (!match) {
			int n;
			for( n=1, FlockNegotiators->rewind();
				 !match && FlockNegotiators->next(neg_host); n++) {
				hent = condor_gethostbyname(neg_host->fullHostname());
				if (hent && hent->h_addrtype == AF_INET) {
					for (int a=0;
						 !match && (addr = hent->h_addr_list[a]);
						 a++) {
						if (memcmp(addr, &endpoint_addr,
									sizeof(struct in_addr)) == 0){
							match = true;
							which_negotiator = n;
							negotiator_name = host;
						}
					}
				}
			}
		}
		if (!match) {
			dprintf(D_ALWAYS, "Unknown negotiator (%s).  "
					"Aborting negotiation.\n", sock->peer_ip_str());
			return (!(KEEP_STREAM));
		}
	}

	dprintf (D_PROTOCOL, "## 2. Negotiating with CM\n");

 	/* if ReservedSwap is 0, then we are not supposed to make any
 	 * swap check, so we can avoid the expensive sysapi_swap_space
 	 * calculation -Todd, 9/97 */
 	if ( ReservedSwap != 0 ) {
 		SwapSpace = sysapi_swap_space();
 	} else {
 		SwapSpace = INT_MAX;
 	}

	// figure out the number of active shadows. we do this by
	// adding the number of existing shadows + the number of shadows
	// queued up to run in the future.
	CurNumActiveShadows = numShadows + RunnableJobQueue.Length() + num_pending_startd_contacts + startdContactQueue.Length();

	SwapSpaceExhausted = FALSE;
	if( ShadowSizeEstimate ) {
		MaxShadowsForSwap = (SwapSpace - ReservedSwap) / ShadowSizeEstimate;
		dprintf( D_FULLDEBUG, "*** SwapSpace = %d\n", SwapSpace );
		dprintf( D_FULLDEBUG, "*** ReservedSwap = %d\n", ReservedSwap );
		dprintf( D_FULLDEBUG, "*** Shadow Size Estimate = %d\n",
				 ShadowSizeEstimate );
		dprintf( D_FULLDEBUG, "*** Start Limit For Swap = %d\n",
				 MaxShadowsForSwap );
		dprintf( D_FULLDEBUG, "*** Current num of active shadows = %d\n",
				 CurNumActiveShadows );
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
	s->decode();
	if (!s->get(ownerptr,sizeof(owner))) {
		dprintf( D_ALWAYS, "Can't receive owner from manager\n" );
		return (!(KEEP_STREAM));
	}
	if ( command == NEGOTIATE_WITH_SIGATTRS ) {
		if (!s->code(sig_attrs_from_cm)) {	// result is mallec-ed!
			dprintf( D_ALWAYS, "Can't receive sig attrs from manager\n" );
			return (!(KEEP_STREAM));
		}

	}
	if (!s->end_of_message()) {
		dprintf( D_ALWAYS, "Can't receive owner/EOM from manager\n" );
		return (!(KEEP_STREAM));
	}
	if (negotiator_name) {
		dprintf (D_ALWAYS, "Negotiating with %s for owner: %s\n",
				 negotiator_name, owner);
	} else {
		dprintf (D_ALWAYS, "Negotiating for owner: %s\n", owner);
	}
	//-----------------------------------------------

		// See if the negotiator wants to talk to the dedicated
		// scheduler

	if( ! strcmp(owner, dedicated_scheduler.name()) ) {
			// Just let the DedicatedScheduler class do its thing. 
		if (sig_attrs_from_cm) {
			free(sig_attrs_from_cm);
		}
		return dedicated_scheduler.negotiate( s, negotiator_name );
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

	bool prio_rec_array_is_stale = !BuildPrioRecArray();
	jobs = N_PrioRecs;

	JobsStarted = 0;

	// find owner in the Owners array
	char *at_sign = strchr(owner, '@');
	if (at_sign) *at_sign = '\0';
	for (owner_num = 0;
		 owner_num < N_Owners && strcmp(Owners[owner_num].Name, owner);
		 owner_num++);
	if (owner_num == N_Owners) {
		dprintf(D_ALWAYS, "Can't find owner %s in Owners array!\n", owner);
		jobs = 0;
		skip_negotiation = true;
	} else if (Owners[owner_num].FlockLevel < which_negotiator) {
		dprintf(D_FULLDEBUG,
				"This user is no longer flocking with this negotiator.\n");
		jobs = 0;
		skip_negotiation = true;
	} else if (Owners[owner_num].FlockLevel == which_negotiator) {
		Owners[owner_num].NegotiationTimestamp = time(0);
	}
	if (at_sign) *at_sign = '@';

	/* Try jobs in priority order */
	for( i=0; i < N_PrioRecs && !skip_negotiation; i++ ) {
	
		// BIOTECH
		if ( JobsRejected > 0 && biotech ) {
			continue;
		}

		char tmpstr[200];
		snprintf(tmpstr,200,"%s@%s",PrioRec[i].owner,UidDomain);
		if(strcmp(owner,tmpstr)!=0)
		{
			dprintf( D_FULLDEBUG, "Job %d.%d skipped ---  belongs to %s\n", 
				PrioRec[i].id.cluster, PrioRec[i].id.proc, tmpstr);
			jobs--;
			continue;
		}

		if(AlreadyMatched(&PrioRec[i].id))
		{
			dprintf( D_FULLDEBUG, "Job already matched\n");
			continue;
		}

		id = PrioRec[i].id;

		int junk; // don't care about the value
		if ( PrioRecAutoClusterRejected->lookup( PrioRec[i].auto_cluster_id, junk ) == 0 ) {
				// We have already failed to match a job from this same
				// autocluster with this machine.  Skip it.
			continue;
		}

		if ( prio_rec_array_is_stale ) {
			// check and make certain the job is still
			// runnable, since things may have changed since we built the 
			// prio_rec array (like, perhaps condor_hold or condor_rm was done).
			if ( Runnable(&id) == FALSE ) {
				continue;
			}
		}


		cad = GetJobAd( id.cluster, id.proc );
		if (!cad) {
			dprintf(D_ALWAYS,"Can't get job ad %d.%d\n",
					id.cluster, id.proc );
			continue;
		}	

		cur_hosts = 0;
		cad->LookupInteger(ATTR_CURRENT_HOSTS,cur_hosts);
		max_hosts = 1;
		cad->LookupInteger(ATTR_MAX_HOSTS,max_hosts);
		job_universe = 0;
		cad->LookupInteger(ATTR_JOB_UNIVERSE,job_universe);

			// Figure out if this request would result in another shadow
			// process if matched.
			// If Grid, the answer is no.
			// If PVM, perhaps yes or no.
			// Otherwise, always yes.
		shadow_num_increment = 1;
		if(job_universe == CONDOR_UNIVERSE_GRID) {
			shadow_num_increment = 0;
		}
		if( job_universe == CONDOR_UNIVERSE_PVM ) {
			PROC_ID temp_id;

				// For PVM jobs, the shadow record is keyed based
				// upon cluster number only - so set proc to 0.
			temp_id.cluster = id.cluster;
			temp_id.proc = 0;

			if ( find_shadow_rec(&temp_id) != NULL ) {
					// A shadow already exists for this PVM job, so
					// if we get a match we will not get a new shadow.
				shadow_num_increment = 0;
			}
		}					

			// Next, make sure we could start another
			// shadow without violating some limit.
		if ( shadow_num_increment ) {
			if ( cant_spawn_shadow ) {
					// We can't start another shadow.
					// Continue on to the next job.
				continue;
			}
			if( ! canSpawnShadow(JobsStarted, jobs) ) {
					// We can't start another shadow.
					// Continue on to the next job.
					// Once canSpawnShadow() returns false, save the result
					// and don't call it again for the rest of this
					// negotiation cycle. Otherwise, it will spam the log
					// with the same message for every shadow-based job
					// left in PrioRec.
				cant_spawn_shadow = true;
				continue;
			}
		}

		for (host_cnt = cur_hosts; host_cnt < max_hosts && !skip_negotiation;) {

			/* Wait for manager to request job info */

			s->decode();
			if( !s->code(op) ) {
				dprintf( D_ALWAYS, "Can't receive request from manager\n" );
				s->end_of_message();
				return (!(KEEP_STREAM));
			}
				// All commands from CM during the negotiation cycle
				// just send the command followed by eom, except
				// PERMISSION, PERMISSION_AND_AD, and REJECTED_WITH_REASON
				// Do the end_of_message here, except for
				// those commands.
			if( (op != PERMISSION) && (op != PERMISSION_AND_AD) &&
				(op != REJECTED_WITH_REASON) ) {
				if( !s->end_of_message() ) {
					dprintf( D_ALWAYS, "Can't receive eom from manager\n" );
					return (!(KEEP_STREAM));
				}
			}

			switch( op ) {
				 case REJECTED_WITH_REASON: {
					 char *diagnostic_message = NULL;
					 if( !s->code(diagnostic_message) ||
						 !s->end_of_message() ) {
						 dprintf( D_ALWAYS,
								  "Can't receive request from manager\n" );
						 return (!(KEEP_STREAM));
					 }
					 dprintf(D_FULLDEBUG, "Job %d.%d rejected: %s\n",
							 id.cluster, id.proc, diagnostic_message);
					 SetAttributeString(
						id.cluster,id.proc,
						ATTR_LAST_REJ_MATCH_REASON,
						diagnostic_message,NONDURABLE);
					 free(diagnostic_message);
				 }
					 // don't break: fall through to REJECTED case
				 case REJECTED:
						// Always negotiate for all PVM job classes! 
					if ( job_universe != CONDOR_UNIVERSE_PVM && !NegotiateAllJobsInCluster ) {
						PrioRecAutoClusterRejected->insert( cur_cluster, 1 );
					}
					host_cnt = max_hosts + 1;
					JobsRejected++;
					SetAttributeInt(
						id.cluster,id.proc,
						ATTR_LAST_REJ_MATCH_TIME,(int)time(0),NONDURABLE);
					break;
				case SEND_JOB_INFO: {
						// The Negotiator wants us to send it a job. 

					/* Send a job description */
					s->encode();
					if( !s->put(JOB_INFO) ) {
						dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
						return (!(KEEP_STREAM));
					}

					// request match diagnostics
					cad->Assign(ATTR_WANT_MATCH_DIAGNOSTICS, true);

					// Send the ad to the negotiator
					if( !cad->put(*s) ) {
						dprintf( D_ALWAYS,
								"Can't send job ad to mgr\n" );
						// FreeJobAd(cad);
						s->end_of_message();
						return (!(KEEP_STREAM));
					}
					// FreeJobAd(cad);
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't send job eom to mgr\n" );
						return (!(KEEP_STREAM));
					}
					cur_cluster = PrioRec[i].auto_cluster_id;
					dprintf( D_FULLDEBUG,
							"Sent job %d.%d (autocluster=%d)\n", id.cluster, 
							id.proc, cur_cluster );
					break;
				}
				case PERMISSION:
						// No negotiator since 7.1.3 should ever send this
						// command, and older ones should not send it either,
						// since we advertise WantResAd=True.
					dprintf( D_ALWAYS, "Negotiator sent PERMISSION rather than expected PERMISSION_AND_AD!  Aborting.\n");
					return !(KEEP_STREAM);
					break;
				case PERMISSION_AND_AD:
					/*
					 * If things are cool, contact the startd.
					 * But... of the claim_id is the string "null", that means
					 * the resource does not support the claiming protocol.
					 */
					dprintf ( D_FULLDEBUG, "In case PERMISSION_AND_AD\n" );

					SetAttributeInt(
						id.cluster,id.proc,
						ATTR_LAST_MATCH_TIME,(int)time(0),NONDURABLE);

					if( !s->get_secret(claim_id) ) {
						dprintf( D_ALWAYS,
								"Can't receive ClaimId from mgr\n" );
						return (!(KEEP_STREAM));
					}
					my_match_ad = NULL;

					// get startd ad from negotiator as well
					my_match_ad = new ClassAd();
					if( !my_match_ad->initFromStream(*s) ) {
						dprintf( D_ALWAYS,
							"Can't get my match ad from mgr\n" );
						delete my_match_ad;
						FREE( claim_id );
						return (!(KEEP_STREAM));
					}
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't receive eom from mgr\n" );
						if (my_match_ad)
							delete my_match_ad;
						FREE( claim_id );
						return (!(KEEP_STREAM));
					}

					{
						ClaimIdParser idp(claim_id);
						dprintf( D_PROTOCOL, 
						         "## 4. Received ClaimId %s\n", idp.publicClaimId() );
					}

					{

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
						bool in_transaction = false;
						cad->LookupInteger(ATTR_LAST_MATCH_LIST_LENGTH,list_len);
						if ( list_len > 0 ) {								
							int list_index;
							char attr_buf[100];
							char *last_match = NULL;
							my_match_ad->LookupString(ATTR_NAME,&last_match);
							for (list_index=0; 
								 last_match && (list_index < list_len); 
								 list_index++) 
							{
								char *curr_match = last_match;
								last_match = NULL;
								snprintf(attr_buf,100,"%s%d",
									ATTR_LAST_MATCH_LIST_PREFIX,list_index);
								cad->LookupString(attr_buf,&last_match);
								if( !in_transaction ) {
									in_transaction = true;
									BeginTransaction();
								}
								SetAttributeString(
									id.cluster,id.proc,
									attr_buf,curr_match);
								free(curr_match);
							}
							if (last_match) free(last_match);
						}
							// Increment ATTR_NUM_MATCHES
						int num_matches = 0;
						cad->LookupInteger(ATTR_NUM_MATCHES,num_matches);
						num_matches++;

						SetAttributeInt(
							id.cluster,id.proc,
							ATTR_NUM_MATCHES,num_matches,NONDURABLE);

						if( in_transaction ) {
							in_transaction = false;
							CommitTransaction(NONDURABLE);
						}
					}

					if ( stricmp(claim_id,"null") == 0 ) {
						// No ClaimId given by the matchmaker.  This means
						// the resource we were matched with does not support
						// the claiming protocol.
						//
						// So, set the matched attribute in our classad to be true,
						// and store the my_match_ad (if exists) in a hashtable.
						if ( my_match_ad ) {
							ClassAd *tmp_ad = NULL;
							resourcesByProcID->lookup(id,tmp_ad);
							if ( tmp_ad ) delete tmp_ad;
							resourcesByProcID->insert(id,my_match_ad);
								// set my_match_ad to NULL, since we stashed it
								// in our hashtable -- i.e. dont deallocate!!
							my_match_ad = NULL;	
						} else {
							EXCEPT("Negotiator messed up - gave null ClaimId & no match ad");
						}
						// Update matched attribute in job ad
						SetAttribute(
							id.cluster,id.proc,
							ATTR_JOB_MATCHED,"True",NONDURABLE);
						SetAttributeInt(
							id.cluster,id.proc,
							ATTR_CURRENT_HOSTS,1,NONDURABLE);
						// Break before we fall into the Claiming Logic section below...
						FREE( claim_id );
						claim_id = NULL;
						JobsStarted += 1;
						host_cnt++;
						break;
					}

					/////////////////////////////////////////////
					////// CLAIMING LOGIC  
					/////////////////////////////////////////////

					{
						Daemon startd(my_match_ad,DT_STARTD,NULL);
						if( !startd.addr() ) {
							dprintf( D_ALWAYS, "Can't find address of startd in match ad:\n" );
							my_match_ad->dPrint(D_ALWAYS);
							delete my_match_ad;
							my_match_ad = NULL;
							break;
						}
						sinful = strdup( startd.addr() );
					}

						// sinful should now point to the sinful string
						// of the startd we were matched with.
					pre_existing_match = NULL;
					mrec = AddMrec( claim_id, sinful, &id, my_match_ad,
									owner,negotiator_name,&pre_existing_match);

						/* if AddMrec returns NULL, it means we can't
						   use that match.  in that case, we'll skip
						   over the attempt to stash the info, go
						   straight to deallocating memory we used in
						   this protocol, and break out of this case

						   One reason this can happen is that the
						   negotiator handed us a claim id that we
						   already have.  If that happens a lot, it
						   probably means the negotiator has a lot of
						   stale info about the startds.  One
						   pathalogical case where that can happen is
						   when the negotiatior is configured to not
						   send match info to the startds, so the
						   startds keep advertising the same claimid
						   until they get the claim request from the
						   schedd.  If this schedd is behind on
						   requesting claims and it starts negotiating
						   for more jobs, only to receive a lot of
						   stale claim ids, it will have wasted more
						   than the usual amount of time in
						   negotiation and so may fall even further
						   behind in requesting claims.

						   Therefore, if we get more than a handfull
						   of claimids for startds we have not yet
						   requested, abort this round of negotiation.
						*/

					if( !mrec && pre_existing_match &&
						(pre_existing_match->status==M_UNCLAIMED ||
						 pre_existing_match->status==M_STARTD_CONTACT_LIMBO))
					{
						if( ++num_pre_existing_matches_received > 10 ) {
							dprintf(D_ALWAYS,"Too many pre-existing matches "
							        "received (due to stale info in "
							        "negotiator), so ending this round of "
							        "negotiation.\n");
							skip_negotiation = true;
						}
					}

						// clear this out so we know if we use it...
					args = NULL;

					if( mrec ) {
							/*
							  Here we don't want to call contactStartd
							  directly because we do not want to block
							  the negotiator for this, and because we
							  want to minimize the possibility that
							  the startd will have to block/wait for
							  the negotiation cycle to finish before
							  it can finish the claim protocol.
							  So...we enqueue the args for a later
							  call.  (The later call will be made from
							  the startdContactSockHandler)
							*/
						args = new ContactStartdArgs( claim_id, sinful,
													  false );
					}

						/*
						  either we already saved all this info in the
						  ContactStartdArgs object, or we're going to
						  throw away the match.  either way, we're
						  done with all this memory and need to
						  deallocate it now.
						*/
					free( sinful );
					sinful = NULL;
					free( claim_id );
					claim_id = NULL;
					if( my_match_ad ) {
						delete my_match_ad;
						my_match_ad = NULL;
					}

					if( ! mrec ) {
						ASSERT( ! args );
							// we couldn't use this match, so we're done.
						break;
					}

					if( enqueueStartdContact(args) ) {
						perm_rval = 1;	// happiness
					} else {
						perm_rval = 0;	// failure
						delete( args );
					}
					JobsStarted += perm_rval;
					addActiveShadows( perm_rval * shadow_num_increment ); 
					host_cnt++;

					/////////////////////////////////////////////
					////// END OF CLAIMING LOGIC  
					/////////////////////////////////////////////

					break;

				case END_NEGOTIATE:
					dprintf( D_ALWAYS, "Lost priority - %d jobs matched\n",
							JobsStarted );

					// We are unsatisfied in this pool.  Flock with
					// less desirable pools.  Note that the current
					// negotiator may "spin the pie" and negotiate with
					// us again shortly.  If that happens, and we end up
					// starting all of our jobs in the local pool, then
					// we'll set FlockLevel back to its original value below.
					if (Owners[owner_num].FlockLevel < MaxFlockLevel &&
						Owners[owner_num].FlockLevel == which_negotiator) { 
						Owners[owner_num].FlockLevel++;
						Owners[owner_num].NegotiationTimestamp = time(0);
						dprintf(D_ALWAYS,
								"Increasing flock level for %s to %d.\n",
								owner, Owners[owner_num].FlockLevel);
						if (JobsStarted == 0) {
							timeout(); // flock immediately
						}
					}

					return KEEP_STREAM;
				default:
					dprintf( D_ALWAYS, "Got unexpected request (%d)\n", op );
					return (!(KEEP_STREAM));
			}
		}
	}

		// We've broken out of our loops here because we're out of
		// jobs.  The Negotiator has either asked us for one more job
		// or has told us to END_NEGOTIATE.  In either case, we need
		// to pull this command off the wire and flush it down the
		// bit-bucket before we stash this socket for future use.  If
		// we got told SEND_JOB_INFO, we need to tell the negotiator
		// we have NO_MORE_JOBS.	-Derek Wright 1/8/98
	s->decode();
	if( !s->code(op) || !s->end_of_message() ) {
		dprintf( D_ALWAYS, "Error: Can't read command from negotiator.\n" );
		return (!(KEEP_STREAM));
	}		
	switch( op ) {
	case SEND_JOB_INFO: 
		if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
			dprintf( D_ALWAYS, "Can't send NO_MORE_JOBS to mgr\n" );
			return (!(KEEP_STREAM));
		}
		break;
	case END_NEGOTIATE:
		break;
	default: 
		dprintf( D_ALWAYS, 
				 "Got unexpected command (%d) from negotiator.\n", op );
		break;
	}

		// It addition to checking to see if we started all of our
		// jobs, check also if any jobs were rejected.  This handles
		// the case when we requested more than one CPU for a job and
		// failed to get all the CPUs we wanted (i.e., PVM jobs).
	if( JobsStarted < jobs || JobsRejected > 0 ) {
		dprintf( D_ALWAYS,
		"Out of servers - %d jobs matched, %d jobs idle, %d jobs rejected\n",
							JobsStarted, jobs - JobsStarted, JobsRejected );

		// We are unsatisfied in this pool.  Flock with less desirable pools.
		if (Owners[owner_num].FlockLevel < MaxFlockLevel &&
			Owners[owner_num].FlockLevel == which_negotiator) { 
			Owners[owner_num].FlockLevel++;
			Owners[owner_num].NegotiationTimestamp = time(0);
			dprintf(D_ALWAYS, "Increasing flock level for %s to %d.\n",
					owner, Owners[owner_num].FlockLevel);
			if (JobsStarted == 0) {
				timeout(); // flock immediately
			}
		}
	} else {
		// We are out of jobs.  Stop flocking with less desirable pools.
		if (Owners[owner_num].FlockLevel >= which_negotiator) {
			Owners[owner_num].FlockLevel = which_negotiator;
			Owners[owner_num].NegotiationTimestamp = time(0);
		}

		dprintf( D_ALWAYS,
		"Out of jobs - %d jobs matched, %d jobs idle, flock level = %d\n",
		 JobsStarted, jobs - JobsStarted, Owners[owner_num].FlockLevel );

		timeout();				// invalidate our ads immediately
	}

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
	description.sprintf( "%s %d.%d", mrec->description(),
						 mrec->cluster, mrec->proc ); 

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
		char const *reason = "find/expand";
		if( !mrec->is_dedicated ) {
			if( GetJobAd( mrec->cluster, mrec->proc, false ) ) {
				reason = "expand";
			}
			else {
				reason = "find";
			}
		}
		dprintf( D_ALWAYS,
				 "Failed to %s job %d.%d when starting to request claim %s\n",
				 reason, mrec->cluster, mrec->proc,
				 mrec->description() ); 
		DelMrec( mrec );
		return;
	}

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

	// now that we've completed authentication (if enabled), punch a hole
	// in our DAEMON authorization level for the execute machine user/IP
	// (if we're flocking, which is why we check match->pool)
	//
	if ((match->auth_hole_id == NULL)) {
		match->auth_hole_id = new MyString;
		ASSERT(match->auth_hole_id != NULL);
		if (msg->startd_fqu() && *msg->startd_fqu()) {
			match->auth_hole_id->sprintf("%s/%s",
			                            msg->startd_fqu(),
			                            msg->startd_ip_addr());
		}
		else {
			*match->auth_hole_id = msg->startd_ip_addr();
		}
		IpVerify* ipv = daemonCore->getSecMan()->getIpVerify();
		if (!ipv->PunchHole(DAEMON, *match->auth_hole_id)) {
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

	if( match->is_dedicated ) {
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

	if( GetAttributeStringNew(cluster, proc, ATTR_OWNER, &owner) < 0 ) {
			// we've got big trouble, just give up.
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				 ATTR_OWNER, cluster, proc );
		mark_job_stopped( job );
		return;
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

		if( !ULog->writeEvent(&event,GetJobAd(cluster,proc)) ) {
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

		// if we need to punch an authorization hole in our DAEMON
		// level for this StartD (to support flocking), do it now
	if (startd_principal != NULL) {
		mrec->auth_hole_id = new MyString(startd_principal);
		ASSERT(mrec->auth_hole_id != NULL);
		free(startd_principal);
		IpVerify* ipv = daemonCore->getIpVerify();
		if (!ipv->PunchHole(DAEMON, *mrec->auth_hole_id)) {
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
	if ( job->Lookup( ATTR_SCHEDD_INTERVAL ) ) {
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

	if (job->LookupInteger(ATTR_JOB_UNIVERSE, univ) != 1) {
		univ = CONDOR_UNIVERSE_STANDARD;
	}

	if( univ != CONDOR_UNIVERSE_LOCAL && univ != CONDOR_UNIVERSE_SCHEDULER ) {
		return 0;
	}

	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);
	job->LookupInteger(ATTR_JOB_STATUS, status);

	if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) != 1) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) != 1) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}
	
		//
		// Before evaluating whether we can run this job, first make 
		// sure its even eligible to run
		// We do not count REMOVED or HELD jobs
		//
	if ( max_hosts > cur_hosts &&
		(status == IDLE || status == UNEXPANDED || status == RUNNING) ) {
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
		if ( scheddAd.Lookup( universeExp ) != NULL ) {
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
			char *exp = scheddAd.sPrintExpr( NULL, false, universeExp );
			if ( exp ) {
				dprintf( D_FULLDEBUG, "Failed expression '%s'\n", exp );
				free( exp );
			}
			return ( 0 );
		}
			//
			// Job Requirements Evaluation
			//
		if ( job->Lookup( ATTR_REQUIREMENTS ) != NULL ) {
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
			char *exp = job->sPrintExpr( NULL, false, ATTR_REQUIREMENTS );
			if ( exp ) {
				dprintf( D_FULLDEBUG, "Failed expression '%s'\n", exp );
				free( exp );
			}
			// This is too verbose.
			//dprintf(D_FULLDEBUG,"Schedd ad that failed to match:\n");
			//scheddAd.dPrint(D_FULLDEBUG);
			//dprintf(D_FULLDEBUG,"Job ad that failed to match:\n");
			//job->dPrint(D_FULLDEBUG);
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

	/* Reset our Timer */
	daemonCore->Reset_Timer(startjobsid,(int)SchedDInterval.getDefaultInterval());

	dprintf(D_FULLDEBUG, "-------- Done starting jobs --------\n");
}

void
Scheduler::StartJob(match_rec *rec)
{
	PROC_ID id;
	bool ReactivatingMatch;

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
	ReactivatingMatch = (id.proc == -1);
	if(!Runnable(&id)) {
			// find the job in the cluster with the highest priority
		id.proc = -1;
		if( rec->my_match_ad ) {
			FindRunnableJob(id,rec->my_match_ad,rec->user);
		}
	}
	if(id.proc < 0) {
			// no more jobs to run
		dprintf(D_ALWAYS,
				"match (%s) out of jobs; relinquishing\n",
				rec->description() );
		DelMrec(rec);
		return;
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
		// If we're reusing a match to start another job, then cluster
		// and proc may have changed, so we keep them up-to-date here.
		// This is important for Scheduler::AlreadyMatched(), and also
		// for when we receive an alive command from the startd.
	SetMrecJobID(rec,id);
		// Now that the shadow has spawned, consider this match "ACTIVE"
	rec->setStatus( M_ACTIVE );
}


void
Scheduler::StartLocalJobs()
{
	WalkJobQueue( (int(*)(ClassAd *))find_idle_local_jobs );
}

shadow_rec*
Scheduler::StartJob(match_rec* mrec, PROC_ID* job_id)
{
	int		universe;
	int		rval;

	rval = GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE, 
							&universe);
	if (universe == CONDOR_UNIVERSE_PVM) {
		return start_pvm(mrec, job_id);
	} else {
		if (rval < 0) {
			dprintf(D_ALWAYS, "Couldn't find %s Attribute for job "
					"(%d.%d) assuming standard.\n",	ATTR_JOB_UNIVERSE,
					job_id->cluster, job_id->proc);
		}
		return start_std( mrec, job_id, universe );
	}
	return NULL;
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
	case UNEXPANDED:
	case IDLE:
	case RUNNING:
			// these are the cases we expect.  if it's local
			// universe, it'll still be IDLE.  if it's not local,
			// it'll already be marked as RUNNING...  just break
			// out of the switch and carry on.
		return true;
		break;

	case REMOVED:
	case HELD:
		dprintf( D_FULLDEBUG,
				 "Job %d.%d was %s while waiting to start\n",
				 cluster, proc, getJobStatusString(status) );
		return false;
		break;

	case COMPLETED:
	case SUBMISSION_ERR:
		EXCEPT( "IMPOSSIBLE: status for job %d.%d is %s "
				"but we're trying to start a shadow for it!", 
				cluster, proc, getJobStatusString(status) );
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

	bool old_resource = true;
	bool nt_resource = false;
 	char* match_opsys = NULL;
 	char* match_version = NULL;

		// Until we're restorting the match ClassAd on reconnected, we
		// wouldn't know if the startd we want to talk to supports the
		// DC shadow or not.  so, for now, we can just assume that if
		// we're trying to reconnect, it *must* be a DC shadow/starter 
	if( wants_reconnect ) {
		old_resource = false;
	}

 	if( mrec->my_match_ad ) {
 		mrec->my_match_ad->LookupString( ATTR_OPSYS, &match_opsys );
		mrec->my_match_ad->LookupString( ATTR_VERSION, &match_version );
	}
	if( match_version ) {
		CondorVersionInfo ver_info( match_version );
		if( ver_info.built_since_version(6, 3, 3) ) {
			old_resource = false;
		}
		free( match_version );
		match_version = NULL;
	}

	if( match_opsys ) {
		if( strincmp(match_opsys,"winnt",5) == MATCH ) {
			nt_resource = true;
		}
		free( match_opsys );
		match_opsys = NULL;
	}
	
	if( nt_resource ) {
		shadow_obj = shadow_mgr.findShadow( ATTR_IS_DAEMON_CORE );
		if( ! shadow_obj ) {
			dprintf( D_ALWAYS, "Trying to run a job on a Windows "
					 "resource but you do not have a condor_shadow "
					 "that will work, aborting.\n" );
			noShadowForJob( srec, NO_SHADOW_WIN32 );
			return;
		}
	}

	if( ! shadow_obj ) {
		switch( universe ) {
		case CONDOR_UNIVERSE_PVM:
			EXCEPT( "Trying to spawn a PVM job with StartJobHandler(), "
					"not start_pvm()!" );
			break;
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
			if( old_resource ) {
				shadow_obj = shadow_mgr.findShadow( ATTR_HAS_OLD_VANILLA );
				if( ! shadow_obj ) {
					dprintf( D_ALWAYS, "Trying to run a VANILLA job on a "
							 "pre-6.3.3 resource, but you do not have a "
							 "condor_shadow that will work, aborting.\n" );
					noShadowForJob( srec, NO_SHADOW_OLD_VANILLA );
					return;
				}
			} else {
				shadow_obj = shadow_mgr.findShadow( ATTR_IS_DAEMON_CORE ); 
				if( ! shadow_obj ) {
					dprintf( D_ALWAYS, "Trying to run a VANILLA job on a "
							 "6.3.3 or later resource, but you do not have "
							 "condor_shadow that will work, aborting.\n" );
					noShadowForJob( srec, NO_SHADOW_DC_VANILLA );
					return;
				}
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

	if (universe == CONDOR_UNIVERSE_STANDARD 
		&& !shadow_obj->builtSinceVersion(6, 8, 5)
		&& !shadow_obj->builtSinceDate(5, 15, 2007)) {
		dprintf(D_ALWAYS, "Your version of the condor_shadow is older than "
			  "6.8.5, is incompatible with this version of the condor_schedd, "
			  "and will not be able to run jobs.  "
			  "Please upgrade your condor_shadow.  Aborting.\n");
		noShadowForJob(srec, NO_SHADOW_PRE_6_8_5_STD);
		delete( shadow_obj );
		free( shadow_path );
		return;
	}

	if ( shadow_obj ) {
		delete( shadow_obj );
		shadow_obj = NULL;
	}

#endif /* ! WIN32 */

	if( wants_reconnect && !(sh_is_dc && sh_reads_file) ) {
		dprintf( D_ALWAYS, "Trying to reconnect but you do not have a "
				 "condor_shadow that will work, aborting.\n" );
		noShadowForJob( srec, NO_SHADOW_RECONNECT );
		delete( shadow_obj );
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

			case CONDOR_UNIVERSE_PVM:
				/* no transferd for this universe */
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
			argbuf.sprintf("%d.%d",job_id->cluster,job_id->proc);
			args.AppendArg(argbuf.Value());

			if(wants_reconnect) {
				args.AppendArg("--reconnect");
			}

			// pass the public ip/port of the schedd (used w/ reconnect)
			// We need this even if we are not currently in reconnect mode,
			// because the shadow may go into reconnect mode at any time.
			argbuf.sprintf("--schedd=%s", daemonCore->publicNetworkIpAddr());
			args.AppendArg(argbuf.Value());

			if( m_xfer_queue_contact.Length() ) {
				argbuf.sprintf("--xfer-queue=%s",m_xfer_queue_contact.Value());
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


	rval = spawnJobHandlerRaw( srec, shadow_path, args, NULL, "shadow",
							   sh_is_dc, sh_reads_file );

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
	if( (universe == CONDOR_UNIVERSE_MPI ) ||
	    (universe == CONDOR_UNIVERSE_PARALLEL) ){
		dedicated_scheduler.shadowSpawned( srec );
	}
}


void
Scheduler::setNextJobDelay( ClassAd const *job_ad, ClassAd const *machine_ad ) {
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
							(Eventcpp)&Scheduler::StartJobHandler,
							"start_job", this ); 
	} else {
		StartJobs();
	}
}


bool
Scheduler::spawnJobHandlerRaw( shadow_rec* srec, const char* path, 
							   ArgList const &args, Env const *env, 
							   const char* name, bool is_dc, bool wants_pipe )
{
	int pid = -1;
	PROC_ID* job_id = &srec->job_id;
	ClassAd* job_ad = NULL;

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
			envname.sprintf("_%s_USERID_MAP",myDistro->Get());
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


	/* For now, we should create the handler as PRIV_ROOT so it can do
	   priv switching between PRIV_USER (for handling syscalls, moving
	   files, etc), and PRIV_CONDOR (for writing to log files).
	   Someday, hopefully soon, we'll fix this and spawn the
	   shadow/handler with PRIV_USER_FINAL... */
	pid = daemonCore->Create_Process( path, args, PRIV_ROOT, rid, 
	                                  is_dc, env, NULL, NULL, NULL, 
	                                  std_fds_p, NULL, niceness );
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
		job_ad->sPrint(ad_str);
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
		if( srec && srec->match ) {
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
	static bool notify_pre_6_8_5_std = true;

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
	static char pre_6_8_5_std_reason [] = 
		"No condor_shadow installed that is at least version 6.8.5";

	PROC_ID job_id;
	char* hold_reason;
	bool* notify_admin;

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
	case NO_SHADOW_PRE_6_8_5_STD:
		hold_reason = pre_6_8_5_std_reason;
		notify_admin = &notify_pre_6_8_5_std;
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

	BeginTransaction();
	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
		// nothing that has been written in this transaction needs to
		// be immediately synced to disk
	CommitTransaction( NONDURABLE );

	// add job to run queue
	shadow_rec* srec=add_shadow_rec( 0, job_id, univ, mrec, -1 );
	addRunnableJob( srec );
	return srec;
}


shadow_rec*
Scheduler::start_pvm(match_rec* mrec, PROC_ID *job_id)
{

#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	ArgList         args;
	int				pid;
	int				shadow_fd;
	MyString		out_buf;
	struct shadow_rec *srp;
	int	 c;     	// current hosts
	int	 old_proc;  // the class in the cluster.  
                    // needed by the multi_shadow -Bin
	char			hostname[MAXHOSTNAMELEN];
	Shadow*			shadow_obj;
	char* 			shadow_path;

	mrec->my_match_ad->LookupString(ATTR_NAME, hostname);

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s...\n",
			job_id->cluster, job_id->proc, hostname);
	
	if(GetAttributeInt(job_id->cluster,job_id->proc,ATTR_CURRENT_HOSTS,&c)<0){
		c = 1;
	} else {
		c++;
	}
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, c);

	old_proc = job_id->proc;  
	
	/* For PVM/CARMI, all procs in a cluster are considered part of the same
		job, so just clear out the proc number */
	job_id->proc = 0;

	/* See if this job is already running */
	srp = find_shadow_rec(job_id);

	if (srp == 0) {
		int pipes[2];
		socketpair(AF_UNIX, SOCK_STREAM, 0, pipes);

		shadow_obj = shadow_mgr.findShadow( ATTR_HAS_PVM );
		if( ! shadow_obj ) {
			dprintf( D_ALWAYS, "ERROR: Can't find a shadow with %s -- "
					 "can't spawn PVM jobs, aborting\n", ATTR_HAS_PVM );
			RemoveShadowRecFromMrec(srp);
			holdJob( job_id->cluster, job_id->proc, 
					 "No condor_shadow installed that supports PVM jobs", 
                     true, true, true, true );
			delete srp;
			return NULL;
		}
		shadow_path = shadow_obj->path();
		args.AppendArg("condor_shadow.pvm");
		args.AppendArg(MyShadowSockName);

		int fds[3];
		fds[0] = pipes[0];  // the effect is to dup the pipe to stdin in child.
	    fds[1] = fds[2] = -1;
		{
			MyString args_string;
			args.GetArgsStringForDisplay(&args_string);
			dprintf( D_ALWAYS, "About to Create_Process( %s, %s, ... )\n", 
				 shadow_path, args_string.Value() );
		}
		
		pid = daemonCore->Create_Process( shadow_path, args, PRIV_ROOT, 
										  shadowReaperId,
										  FALSE, NULL, NULL, NULL, 
										  NULL, fds );

		delete( shadow_obj );

		if ( !pid ) {
			dprintf ( D_FAILURE|D_ALWAYS, "Problem with Create_Process!\n" );
			close(pipes[0]);
			return NULL;
		}

		dprintf ( D_ALWAYS, "In parent, shadow pid = %d\n", pid );

		close(pipes[0]);
		mark_job_running(job_id);
		srp = add_shadow_rec( pid, job_id, CONDOR_UNIVERSE_PVM, mrec,
							  pipes[1] );
		shadow_fd = pipes[1];
		dprintf( D_ALWAYS, "shadow_fd = %d\n", shadow_fd);		
	} else {
		shadow_fd = srp->conn_fd;
		dprintf( D_ALWAYS, "Existing shadow connected on fd %d\n", shadow_fd);
	}
	
    dprintf( D_ALWAYS, "Sending job %d.%d to shadow pid %d\n", 
             job_id->cluster, job_id->proc, srp->pid);

    out_buf.sprintf("%d %d %d\n", job_id->cluster, job_id->proc, 1);

		// Warning: the pvm shadow may close this pipe during a
		// graceful shutdown.  We should consider than an indication
		// that the shadow doesn't want any more machines.  We should
		// not kill the shadow if it closes the pipe, though, since it
		// has some useful cleanup to do (i.e., so we can't return
		// NULL here when the pipe is closed, since our caller will
		// consider that a fatal error for the shadow).
	
	dprintf( D_ALWAYS, "First Line: %s", out_buf.Value() );
	write(shadow_fd, out_buf.Value(), out_buf.Length());

	out_buf.sprintf("%s %s %d %s\n", mrec->peer, mrec->claimId(), old_proc,
					hostname);
	dprintf( D_ALWAYS, "sending %s %s %d %s",
	         mrec->peer, mrec->publicClaimId(), old_proc, hostname);
	write(shadow_fd, out_buf.Value(), out_buf.Length());
	return srp;
#else
	return NULL;
#endif /* !defined(WIN32) */
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
							(Eventcpp)&Scheduler::StartJobHandler,
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
	header.sprintf("(%d.%d) ",job_id->cluster,job_id->proc);
	starter_args.AppendArg(header.Value());

	starter_args.AppendArg("-job-input-ad");
	starter_args.AppendArg("-");
	starter_args.AppendArg("-schedd-addr");
	starter_args.AppendArg(MyShadowSockName);

	if(DebugFlags & D_FULLDEBUG) {
		MyString argstring;
		starter_args.GetArgsStringForDisplay(&argstring);
		dprintf( D_FULLDEBUG, "About to spawn %s %s\n", 
				 starter_path, argstring.Value() );
	}

	BeginTransaction();
	mark_job_running( job_id );

		// add CLAIM_ID to this job ad so schedd can be authorized by
		// starter by virtue of this shared secret (e.g. for
		// CREATE_JOB_OWNER_SEC_SESSION
	char *public_part = Condor_Crypt_Base::randomHexKey();
	char *private_part = Condor_Crypt_Base::randomHexKey();
	ClaimIdParser cidp(public_part,NULL,private_part);
	SetAttributeString( job_id->cluster, job_id->proc, ATTR_CLAIM_ID, cidp.claimId() );
	free( public_part );
	free( private_part );

	CommitTransaction();

	Env starter_env;
	MyString execute_env;
	execute_env.sprintf( "_%s_EXECUTE", myDistro->Get());
	starter_env.SetEnv(execute_env.Value(),LocalUnivExecuteDir);
	
	rval = spawnJobHandlerRaw( srec, starter_path, starter_args,
							   &starter_env, "starter", true, true );

	free( starter_path );
	starter_path = NULL;

	if( ! rval ) {
		dprintf( D_ALWAYS|D_FAILURE, "Can't spawn local starter for "
				 "job %d.%d\n", job_id->cluster, job_id->proc );
		BeginTransaction();
		mark_job_stopped( job_id );
		CommitTransaction();
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
		dir_name.sprintf( "%s%c%s", tmp, DIR_DELIM_CHAR,
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
			Directory exec_dir( &exec_statinfo );
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

	is_executable = false;

	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
		job_id->cluster, job_id->proc );

	userJob = GetJobAd(job_id->cluster,job_id->proc);
	ASSERT(userJob);

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
	if (stricmp(owner.Value(), "root") == 0 ) {
		dprintf(D_ALWAYS, "Aborting job %d.%d.  Tried to start as root.\n",
			job_id->cluster, job_id->proc);
		goto wrapup;
	}

	// switch to the user in question to make some checks about what I'm 
	// about to execute and then to execute.

	if (! init_user_ids(owner.Value(), domain.Value()) ) {
		MyString tmpstr;
#ifdef WIN32
		tmpstr.sprintf("Bad or missing credential for user: %s", owner.Value());
#else
		tmpstr.sprintf("Unable to switch to user: %s", owner.Value());
#endif
		holdJob(job_id->cluster, job_id->proc, tmpstr.Value(),
				false, false, true, false, false);
		goto wrapup;
	}

	priv = set_user_priv(); // need user's privs...

	// Here we are going to look into the spool directory which contains the
	// user's executables as the user. Be aware that even though the spooled
	// executable probably is owned by Condor in most circumstances, we
	// must ensure the user can at least execute it.

	a_out_name = gen_ckpt_name(Spool, job_id->cluster, ICKPT, 0);
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
				false, false, true, false, false );
			goto wrapup;
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
			tmpstr.sprintf( "File '%s' is missing or not executable", a_out_name.Value() );
			set_priv( priv );  // back to regular privs...
			holdJob(job_id->cluster, job_id->proc, tmpstr.Value(),
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
	
	//change to IWD before opening files, easier than prepending 
	//IWD if not absolute pathnames
	condor_getcwd(tmpCwd);
	chdir(iwd.Value());
	
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
	
	if ((inouterr[0] = safe_open_wrapper(input.Value(), O_RDONLY, S_IREAD)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", input.Value(), errno );
		cannot_open_files = true;
	}
	if ((inouterr[1] = safe_open_wrapper(output.Value(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", output.Value(), errno );
		cannot_open_files = true;
	}
	if ((inouterr[2] = safe_open_wrapper(error.Value(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", error.Value(), errno );
		cannot_open_files = true;
	}
	
	//change back to whence we came
	if ( tmpCwd.Length() ) {
		chdir( tmpCwd.Value() );
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
	argbuf.sprintf("condor_scheduniv_exec.%d.%d",job_id->cluster,job_id->proc);
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
	BeginTransaction();
	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
	CommitTransaction();
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
	if(userJob) {
		FreeJobAd(userJob);
	}
	return retval;
}

void
Scheduler::display_shadow_recs()
{
	struct shadow_rec *r;

	if( !(DebugFlags & D_FULLDEBUG) ) {
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
		// need to make sure this gets set immediately
		SetAttributeString(new_rec->job_id.cluster, new_rec->job_id.proc,
						   ATTR_REMOTE_POOL, new_rec->match->pool);
	}
	return new_rec;
}

static void
add_shadow_birthdate(int cluster, int proc, bool is_reconnect = false)
{
	dprintf( D_ALWAYS, "Starting add_shadow_birthdate(%d.%d)\n",
			 cluster, proc );
	int current_time = (int)time(NULL);
	int job_start_date = 0;
	SetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE, current_time);
	if (GetAttributeInt(cluster, proc,
						ATTR_JOB_START_DATE, &job_start_date) < 0) {
		// this is the first time the job has ever run, so set JobStartDate
		SetAttributeInt(cluster, proc, ATTR_JOB_START_DATE, current_time);
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
			if( stricmp(vmtype.Value(), CONDOR_VM_UNIVERSE_VMWARE ) == 0 ) {
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
		}
		if( ! have_remote_host ) {
				// CRUFT
			dprintf( D_ALWAYS, "ERROR: add_shadow_rec() doesn't have %s "
					 "for of remote resource for setting %s, using "
					 "inferior alternatives!\n", ATTR_NAME, 
					 ATTR_REMOTE_HOST );
			struct sockaddr_in sin;
			if( mrec->peer && mrec->peer[0] && 
				string_to_sin(mrec->peer, &sin) == 1 ) {
					// make local copy of static hostname buffer
				char *tmp, *hostname;
				if( (tmp = sin_to_hostname(&sin, NULL)) ) {
					hostname = strdup( tmp );
					SetAttributeString( cluster, proc, ATTR_REMOTE_HOST,
										hostname );
					dprintf( D_FULLDEBUG, "Used inverse DNS lookup (%s)\n",
							 hostname );
					free(hostname);
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
	if (new_rec->universe == CONDOR_UNIVERSE_PVM) {
		ClassAd *job_ad;
		job_ad = GetNextJob(1);
		while (job_ad != NULL) {
			PROC_ID tmp_id;
			job_ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == cluster) {
				job_ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				add_shadow_birthdate(tmp_id.cluster, tmp_id.proc,
									 new_rec->is_reconnect);
			}
			job_ad = GetNextJob(0);
		}
	} else {
		add_shadow_birthdate( cluster, proc, new_rec->is_reconnect );
	}
	CommitTransaction();
	if( new_rec->pid ) {
		dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
				 new_rec->pid, cluster, proc );
		scheduler.display_shadow_recs();
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
	scheduler.display_shadow_recs();
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
		if (status == RUNNING) {
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
		accum_time += (float)( time(NULL) - bday );
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

	if( rec->universe == CONDOR_UNIVERSE_PVM ) {
		ClassAd *cad;
		cad = GetNextJob(1);
		while (cad != NULL) {
			PROC_ID tmp_id;
			cad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == cluster) {
				cad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				update_remote_wall_clock(tmp_id.cluster, tmp_id.proc);
			}
			cad = GetNextJob(0);
		}
	} else {
		if( pid ) {
				// we only need to update this if we spawned a shadow.
			update_remote_wall_clock(cluster, proc);
		}
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
		display_shadow_recs();
	}

	dprintf( D_FULLDEBUG, "Exited delete_shadow_rec( %d )\n", pid );
	return;
}

/*
** Mark a job as running.  Do not call directly.  Call the non-underscore
** version below instead.
*/
void
_mark_job_running(PROC_ID* job_id)
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
}

/*
** Mark a job as stopped, (Idle or Unexpanded).  Do not call directly.  
** Call the non-underscore version below instead.
*/
void
_mark_job_stopped(PROC_ID* job_id)
{
	int		status;
	int		orig_max;
	int		had_orig;

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
	if (status == RUNNING) {

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


/* 
** Wrapper for _mark_job_running so we mark the whole cluster as running
** for pvm jobs.
*/
void
mark_job_running(PROC_ID* job_id)
{
	int universe = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE,
					&universe);
	if( universe == CONDOR_UNIVERSE_PVM ) {
		ClassAd *ad;
		ad = GetNextJob(1);
		while (ad != NULL) {
			PROC_ID tmp_id;
			ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == job_id->cluster) {
				ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				_mark_job_running(&tmp_id);
			}
			ad = GetNextJob(0);
		}
	} else {
		_mark_job_running(job_id);
	}
}

/* PVM jobs may have many procs (job classes) in a cluster.  We should
   mark all of them stopped when the job stops. */
void
mark_job_stopped(PROC_ID* job_id)
{
	int universe = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE,
					&universe);
	if( (universe == CONDOR_UNIVERSE_PVM) || 
        (universe == CONDOR_UNIVERSE_MPI) ||
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
			case CONDOR_UNIVERSE_PVM:
				dprintf( D_ALWAYS, "Sending SIGTERM to shadow for PVM job "
						 "%d.%d (pid: %d)\n", cluster, proc, rec->pid );
				sendSignalToShadow(rec->pid,SIGTERM,rec->job_id);
				break;

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
						send_vacate( rec->match, CKPT_FRGN_JOB );
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
						   universe other than PVM, local, and
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
	classy_counted_ptr<DCStartd> startd = new DCStartd( match->peer );
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
			if (status != RUNNING &&
				universe!=CONDOR_UNIVERSE_PVM &&
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

	dprintf( D_ALWAYS, "Mailing administrator (%s)\n", CondorAdministrator );

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

void
Scheduler::NotifyUser(shadow_rec* srec, char* msg, int status, int JobStatus)
{
	int notification;
	MyString owner;
	MyString subject;
	MyString cmd;
	MyString args;

	if (GetAttributeInt(srec->job_id.cluster, srec->job_id.proc,
						ATTR_JOB_NOTIFICATION, &notification) < 0) {
		dprintf(D_ALWAYS, "GetAttributeInt() failed "
				"-- presumably job was just removed\n");
		return;
	}

	switch(notification) {
	case NOTIFY_NEVER:
		return;
	case NOTIFY_ALWAYS:
		break;
	case NOTIFY_COMPLETE:
		if (JobStatus == COMPLETED) {
			break;
		} else {
			return;
		}
	case NOTIFY_ERROR:
		if( (JobStatus == REMOVED) ) {
			break;
		} else {
			return;
		}
	default:
		dprintf(D_ALWAYS, "Condor Job %d.%d has a notification of %d\n",
				srec->job_id.cluster, srec->job_id.proc, notification );
	}

	if (GetAttributeString(srec->job_id.cluster, srec->job_id.proc,
						   ATTR_NOTIFY_USER, owner) < 0) {
		if (GetAttributeString(srec->job_id.cluster, srec->job_id.proc,
							   ATTR_OWNER, owner) < 0) {
			EXCEPT("GetAttributeString(%d, %d, \"%s\")",
					srec->job_id.cluster,
					srec->job_id.proc, ATTR_OWNER);
		}
	}

	if (GetAttributeString(srec->job_id.cluster, srec->job_id.proc, 
							ATTR_JOB_CMD,
							cmd) < 0) {
		EXCEPT("GetAttributeString(%d, %d, \"cmd\")", srec->job_id.cluster,
				srec->job_id.proc);
	}
	{
		ClassAd *job_ad = GetJobAd(srec->job_id.cluster,srec->job_id.proc);
		if(!job_ad) {
			EXCEPT("Failed to get job ad when sending notification.");
		}
		ArgList::GetArgsStringForDisplay(job_ad,&args);
	}

	// Send mail to user
	subject.sprintf("Condor Job %d.%d", srec->job_id.cluster, 
					srec->job_id.proc);
	dprintf( D_FULLDEBUG, "Unknown user notification selection\n");
	dprintf( D_FULLDEBUG, "\tNotify user with subject: %s\n",subject.Value());

	FILE* mailer = email_open(owner.Value(), subject.Value());
	if( mailer ) {
		fprintf( mailer, "Your condor job %s%d.\n\n", msg, status );
		fprintf( mailer, "Job: %s %s\n", cmd.Value(), args.Value() );
		email_close( mailer );
	}

/*
	sprintf(url, "mailto:%s", owner);
	if ((fd = open_url(url, O_WRONLY)) < 0) {
		EXCEPT("condor_open_mailto_url(%s, %d, 0)", owner, O_WRONLY, 0);
	}

//	sprintf(subject, "From: Condor\n");
//	write(fd, subject, strlen(subject));
//	sprintf(subject, "To: %s\n", owner);
//	write(fd, subject, strlen(subject));
	sprintf(subject, "Subject: Condor Job %d.%d\n\n", srec->job_id.cluster,
			srec->job_id.proc);
	write(fd, subject, strlen(subject));
	sprintf(subject, "Your condor job\n\t");
	write(fd, subject, strlen(subject));
	write(fd, cmd.Value(), cmd.Length());
	write(fd, " ", 1);
	write(fd, args, strlen(args));
	write(fd, "\n", 1);
	write(fd, msg, strlen(msg));
	sprintf(subject, "%d.\n", status);
	write(fd, subject, strlen(subject));
	close(fd);
*/

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
** Wrapper for setting the job status to deal with PVM jobs, which can 
** contain multiple procs.
*/
void
set_job_status(int cluster, int proc, int status)
{
	int universe = CONDOR_UNIVERSE_STANDARD;
	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe);

	BeginTransaction();

	if( ( universe == CONDOR_UNIVERSE_PVM) || 
		( universe == CONDOR_UNIVERSE_MPI) ||
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
		char* name = NULL;
			//
			// Local Universe
			//
		if( IsLocalUniverse(srec) ) {
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
		// activate all our claims and start jobs on them.
	if( ! ExitWhenDone && StartJobsFlag ) {
		this->StartJobs();
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
	
		// We get the name of the daemon that had a problem for 
		// nice log messages...
	MyString daemon_name;
	if ( srec != NULL ) {
		daemon_name = ( IsLocalUniverse( srec ) ? "Local Starter" : "Shadow" );
	}

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

		case JOB_EXEC_FAILED:
				//
				// The calling function will make sure that
				// we don't try to start new jobs
				//
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
			if( !srec->removed && srec->match ) {
				DelMrec(srec->match);
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
				// no break, fall through and do the action

		case JOB_NO_CKPT_FILE:
		case JOB_KILLED:
				// If the job isn't being HELD, we'll remove it
			if ( q_status != HELD ) {
				set_job_status( job_id.cluster, job_id.proc, REMOVED );
			}
			break;

		case JOB_EXITED_AND_CLAIM_CLOSING:
			if( srec->match ) {
					// startd is not accepting more jobs on this claim
				srec->match->needs_release_claim = false;
				DelMrec(srec->match);
			}
				// no break, fall through
		case JOB_EXITED:
			dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
				// no break, fall through and do the action

		case JOB_COREDUMPED:
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
			break;
		}

		case DPRINTF_ERROR:
			dprintf( D_ALWAYS,
					 "ERROR: %s had fatal error writing its log file\n",
					 daemon_name.Value() );
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
			break;
	} // SWITCH
	
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
						ATTR_NUM_SHADOW_EXCEPTIONS, num_excepts);

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
	ClassAd * job_ad = GetJobAd( job_id.cluster, job_id.proc );
	ASSERT( job_ad ); // No job ad?
	{
		UserPolicy policy;
		policy.Init(job_ad);
		action = policy.AnalyzePolicy(PERIODIC_THEN_EXIT);
		reason = policy.FiringReason();
		if ( reason == "" ) {
			reason = "Unknown user policy expression";
		}
	}
	FreeJobAd(job_ad);
	job_ad = NULL;


	switch(action) {
		case REMOVE_FROM_QUEUE:
			scheduler_univ_job_leave_queue(job_id, status, srec);
			break;

		case STAYS_IN_QUEUE:
			set_job_status( job_id.cluster,	job_id.proc, IDLE ); 
			WriteRequeueToUserLog(job_id, status, reason.Value());
			break;

		case HOLD_IN_QUEUE:
			holdJob(job_id.cluster, job_id.proc, reason.Value(),
				true,false,false,false,false);
			break;

		case RELEASE_FROM_HOLD:
			dprintf(D_ALWAYS,
				"(%d.%d) Job exited.  User policy attempted to release "
				"job, but it wasn't on hold.  Allowing job to exit queue.\n", 
				job_id.cluster, job_id.proc);
			scheduler_univ_job_leave_queue(job_id, status, srec);
			break;

		case UNDEFINED_EVAL:
			dprintf( D_ALWAYS,
				"(%d.%d) Problem parsing user policy for job: %s.  "
				"Putting job on hold.\n",
				 job_id.cluster, job_id.proc, reason.Value());
			holdJob(job_id.cluster, job_id.proc, reason.Value(),
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
				true,false,false,false,true);
			break;
	}

}


void
Scheduler::scheduler_univ_job_leave_queue(PROC_ID job_id, int status, shadow_rec * srec)
{
	ASSERT(srec);
	set_job_status( job_id.cluster,	job_id.proc, COMPLETED ); 
	WriteTerminateToUserLog( job_id, status );
	if(WIFEXITED(status)) {
		NotifyUser( srec, "exited with status ",
					WEXITSTATUS(status), COMPLETED );
	} else { // signal
		NotifyUser( srec, "was killed by signal ",
					WTERMSIG(status), COMPLETED );
	}
}

void
Scheduler::kill_zombie(int, PROC_ID* job_id )
{
#if 0
		// This always happens now, no need for a dprintf() 
		// Derek 3/13/98
	 dprintf( D_ALWAYS,
		  "Shadow %d died, and left job %d.%d marked RUNNING\n",
		  pid, job_id->cluster, job_id->proc );
#endif

	 mark_job_stopped( job_id );
}

/*
** The shadow running this job has died.  If things went right, the job
** has been marked as idle, unexpanded, or completed as appropriate.
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
		dprintf(D_ALWAYS,"ERROR fetching job status in check_zombie !\n");
		return;
	}

	dprintf( D_FULLDEBUG, "Entered check_zombie( %d, 0x%p, st=%d )\n", 
			 pid, job_id, status );

	// set cur-hosts to zero
	SetAttributeInt( job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 0, NONDURABLE ); 

	switch( status ) {
	case RUNNING: {
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

/*
**  Starting with version 6.2.0, we store checkpoints on the checkpoint
**  server using "owner@scheddName" instead of just "owner".  If the job
**  was submitted before this change, we need to check to see if its
**  checkpoint was stored using the old naming scheme.
*/
bool
JobPreCkptServerScheddNameChange(int cluster, int proc)
{
	char *job_version = NULL;
	
	if (GetAttributeStringNew(cluster, proc, ATTR_VERSION, &job_version) >= 0) {
		CondorVersionInfo ver(job_version, "JOB");
		free(job_version);
		if (ver.built_since_version(6,2,0) &&
			ver.built_since_date(11,16,2000)) {
			return false;
		}
	}
	return true;				// default to version compat. mode
}

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

	// only need to contact the ckpt server for standard universe jobs
	GetAttributeInt(cluster,proc,ATTR_JOB_UNIVERSE,&universe);
	if (universe == CONDOR_UNIVERSE_STANDARD) {
		if (GetAttributeString(cluster, proc, ATTR_LAST_CKPT_SERVER,
							   server) == 0) {
			SetCkptServerHost(server.Value());
		} else {
			SetCkptServerHost(NULL); // no ckpt on ckpt server
		}
	}

		  /* Remove any checkpoint files.  If for some reason we do 
		 * not know the owner, don't bother sending to the ckpt
		 * server.
		 */
	ckpt_name_buf = gen_ckpt_name(Spool,cluster,proc,0);
	ckpt_name = ckpt_name_buf.Value();
	if ( owner ) {
		if ( IsDirectory(ckpt_name) ) {
			if ( param_boolean("KEEP_OUTPUT_SANDBOX",false) ) {
				// Blow away only the input files from the job's spool.
				// The user is responsible for garbage collection of the
				// rest of the sandbox.  Good luck.  
				FileTransfer sandbox;
				ClassAd *ad = GetJobAd(cluster,proc);
				if ( ad ) {
					sandbox.SimpleInit(ad,
								false,  // want_check_perms
								true,	// is_server
								NULL,	// sock_to_use
								PRIV_CONDOR		// priv_state to use
								);
					sandbox.RemoveInputFiles(ckpt_name);
				}
			} else {
				// Blow away entire sandbox.  This is the default.
				{
					// Must put this in braces so the Directory object
					// destructor is called, which will free the iterator
					// handle.  If we didn't do this, the below rmdir 
					// would fail.
					Directory ckpt_dir(ckpt_name);
					ckpt_dir.Remove_Entire_Directory();
				}
				rmdir(ckpt_name);
			}
		} else {
			if (universe == CONDOR_UNIVERSE_STANDARD) {
				RemoveLocalOrRemoteFile(owner,Name,ckpt_name);
				if (JobPreCkptServerScheddNameChange(cluster, proc)) {
					RemoveLocalOrRemoteFile(owner,NULL,ckpt_name);
				}
			} else {
				unlink(ckpt_name);
			}
		}
	} else
		unlink(ckpt_name);

		  /* Remove any temporary checkpoint files */
	ckpt_name_buf += ".tmp";
	ckpt_name = ckpt_name_buf.Value();
	if ( owner ) {
		if ( IsDirectory(ckpt_name) ) {
			{
				// Must put this in braces so the Directory object
				// destructor is called, which will free the iterator
				// handle.  If we didn't do this, the below rmdir 
				// would fail.
				Directory ckpt_dir(ckpt_name);
				ckpt_dir.Remove_Entire_Directory();
			}
			rmdir(ckpt_name);
		} else {
			if (universe == CONDOR_UNIVERSE_STANDARD) {
				RemoveLocalOrRemoteFile(owner,Name,ckpt_name);
				if (JobPreCkptServerScheddNameChange(cluster, proc)) {
					RemoveLocalOrRemoteFile(owner,NULL,ckpt_name);
				}
			} else {
				unlink(ckpt_name);
			}
		}
	} else
		unlink(ckpt_name);
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

		// set defaults for rounding attributes for autoclustering
		// only set these values if nothing is specified in condor_config.
	MyString tmpstr;
	tmpstr.sprintf("SCHEDD_ROUND_ATTR_%s",ATTR_EXECUTABLE_SIZE);
	tmp = param(tmpstr.Value());
	if ( !tmp ) {
		config_insert(tmpstr.Value(),"25%");	// round up to 25% of magnitude
	} else {
		free(tmp);
	}
	tmpstr.sprintf("SCHEDD_ROUND_ATTR_%s",ATTR_IMAGE_SIZE);
	tmp = param(tmpstr.Value());
	if ( !tmp ) {
		config_insert(tmpstr.Value(),"25%");	// round up to 25% of magnitude
	} else {
		free(tmp);
	}
	tmpstr.sprintf("SCHEDD_ROUND_ATTR_%s",ATTR_DISK_USAGE);
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
	tmpstr.sprintf("SCHEDD_ROUND_ATTR_%s",ATTR_NUM_CKPTS);
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
	if ( !first_time_in_init && this->SchedDInterval.getMaxInterval() != orig_SchedDInterval ) {
			// 
			// This will only update the job's that have the old
			// ScheddInterval attribute defined
			//
		WalkJobQueue( (int(*)(ClassAd *))::updateSchedDInterval );
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
	int default_max_jobs_running = sysapi_phys_memory_raw_no_param()*0.8*1024/800;

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
	if ( this->StartLocalUniverse ) {
		free( this->StartLocalUniverse );
	}
	tmp = param( "START_LOCAL_UNIVERSE" );
	if ( ! tmp ) {
			//
			// Default Expression: TRUE
			//
		this->StartLocalUniverse = strdup( "TotalLocalJobsRunning < 200" );
	} else {
			//
			// Use what they had in the config file
			// Should I be checking this first??
			//
		this->StartLocalUniverse = tmp;
		tmp = NULL;
	}

		//
		// Start Scheduler Universe Expression
		// This will be added into the requirements expression for
		// the schedd to know whether we can start a scheduler job 
		// 
	if ( this->StartSchedulerUniverse ) {
		free( this->StartSchedulerUniverse );
	}
	tmp = param( "START_SCHEDULER_UNIVERSE" );
	if ( ! tmp ) {
			//
			// Default Expression: TRUE
			//
		this->StartSchedulerUniverse = strdup( "TotalSchedulerJobsRunning < 200" );
	} else {
			//
			// Use what they had in the config file
			// Should I be checking this first??
			//
		this->StartSchedulerUniverse = tmp;
		tmp = NULL;
	}

	MaxJobsSubmitted = param_integer("MAX_JOBS_SUBMITTED",INT_MAX);
	
	tmp = param( "NEGOTIATE_ALL_JOBS_IN_CLUSTER" );
	if( !tmp || tmp[0] == 'f' || tmp[0] == 'F' ) {
		NegotiateAllJobsInCluster = false;
	} else {
		NegotiateAllJobsInCluster = true;
	}
	if( tmp ) free( tmp );

	STARTD_CONTACT_TIMEOUT = param_integer("STARTD_CONTACT_TIMEOUT",45);

		// Decide the directory we should use for the execute
		// directory for local universe starters.  Create it if it
		// doesn't exist, fix the permissions (1777 on UNIX), and, if
		// it's the first time we've hit this method (on startup, not
		// reconfig), we remove any subdirectories that might have
		// been left due to starter crashes, etc.
	initLocalStarterDir();

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
	if (!flock_collector_hosts) { // backward compatibility
		flock_collector_hosts = param( "FLOCK_HOSTS" );
	}
	flock_negotiator_hosts = param( "FLOCK_NEGOTIATOR_HOSTS" );
	if (!flock_negotiator_hosts) { // backward compatibility
		flock_negotiator_hosts = param( "FLOCK_HOSTS" );
	}
	if( flock_collector_hosts && flock_negotiator_hosts ) {
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
	} else {
		MaxFlockLevel = 0;
		if (!flock_collector_hosts && flock_negotiator_hosts) {
			dprintf(D_ALWAYS, "FLOCK_NEGOTIATOR_HOSTS defined but "
					"FLOCK_COLLECTOR_HOSTS undefined.  Flocking disabled.\n");
		} else if (!flock_negotiator_hosts && flock_collector_hosts) {
			dprintf(D_ALWAYS, "FLOCK_COLLECTOR_HOSTS defined but "
					"FLOCK_NEGOTIATOR_HOSTS undefined.  Flocking disabled.\n");
		}
	}
	if (flock_collector_hosts) free(flock_collector_hosts);
	if (flock_negotiator_hosts) free(flock_negotiator_hosts);

	/* default 5 megabytes */
	ReservedSwap = param_integer( "RESERVED_SWAP", 0 );
	ReservedSwap *= 1024;

	/* Value specified in kilobytes */
	ShadowSizeEstimate = param_integer( "SHADOW_SIZE_ESTIMATE",DEFAULT_SHADOW_SIZE );

	startd_sends_alives = param_boolean("STARTD_SENDS_ALIVES",false);

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

#ifdef WANT_QUILL

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
	if( m_ad ) delete m_ad;
	m_ad = new ClassAd();

	m_ad->SetMyTypeName(SCHEDD_ADTYPE);
	m_ad->SetTargetTypeName("");

    daemonCore->publish(m_ad);

		// Throw name and machine into the classad.
	m_ad->Assign( ATTR_NAME, Name );

	// This is foul, but a SCHEDD_ADTYPE _MUST_ have a NUM_USERS attribute
	// (see condor_classad/classad.C
	// Since we don't know how many there are yet, just say 0, it will get
	// fixed in count_job() -Erik 12/18/2006
	m_ad->Assign(ATTR_NUM_USERS, 0);

#ifdef WANT_QUILL
	// Put the quill stuff into the add as well
	if (quill_enabled == TRUE) {
		m_ad->Assign( ATTR_QUILL_ENABLED, true ); 

		m_ad->Assign( ATTR_QUILL_NAME, quill_name ); 

		m_ad->Assign( ATTR_QUILL_DB_NAME, quill_db_name ); 

		MyString expr;
		expr.sprintf( "%s = \"<%s>\"", ATTR_QUILL_DB_IP_ADDR,
					  quill_db_ip_addr ); 
		m_ad->Insert( expr.Value() );

		m_ad->Assign( ATTR_QUILL_DB_QUERY_PASSWORD, quill_db_query_password); 

		m_ad->Assign( ATTR_QUILL_IS_REMOTELY_QUERYABLE, 
					  quill_is_remotely_queryable == TRUE ? true : false );

	} else {

		m_ad->Assign( ATTR_QUILL_ENABLED, false );
	}
#endif

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
	if ( ! CronMgr ) {
		CronMgr = new ScheddCronMgr( );
		CronMgr->Initialize( );
	}

	m_xfer_queue_mgr.InitAndReconfig();
	m_xfer_queue_mgr.GetContactInfo(MyShadowSockName,m_xfer_queue_contact);

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
		temp.sprintf("string(%s)",expr);
		free(expr);
		expr = temp.StrDup();
		Parse(temp.Value(),m_parsed_gridman_selection_expr);	
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

	first_time_in_init = false;
}

void
Scheduler::Register()
{
	 // message handlers for schedd commands
	 daemonCore->Register_Command( NEGOTIATE, "NEGOTIATE", 
		 (CommandHandlercpp)&Scheduler::doNegotiate, "doNegotiate", 
		 this, NEGOTIATOR );
	 daemonCore->Register_Command( NEGOTIATE_WITH_SIGATTRS, 
		 "NEGOTIATE_WITH_SIGATTRS", 
		 (CommandHandlercpp)&Scheduler::doNegotiate, "doNegotiate", 
		 this, NEGOTIATOR );
	 daemonCore->Register_Command( RESCHEDULE, "RESCHEDULE", 
			(CommandHandlercpp)&Scheduler::reschedule_negotiator, 
			"reschedule_negotiator", this, WRITE);
	 daemonCore->Register_Command( RECONFIG, "RECONFIG", 
			(CommandHandler)&dc_reconfig, "reconfig", 0, OWNER );
	 daemonCore->Register_Command(RELEASE_CLAIM, "RELEASE_CLAIM", 
			(CommandHandlercpp)&Scheduler::release_claim, 
			"release_claim", this, WRITE);
	 daemonCore->Register_Command(KILL_FRGN_JOB, "KILL_FRGN_JOB", 
			(CommandHandlercpp)&Scheduler::abort_job, 
			"abort_job", this, WRITE);
	 daemonCore->Register_Command(ACT_ON_JOBS, "ACT_ON_JOBS", 
			(CommandHandlercpp)&Scheduler::actOnJobs, 
			"actOnJobs", this, WRITE);
	 daemonCore->Register_Command(SPOOL_JOB_FILES, "SPOOL_JOB_FILES", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE);
	 daemonCore->Register_Command(TRANSFER_DATA, "TRANSFER_DATA", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE);
	 daemonCore->Register_Command(SPOOL_JOB_FILES_WITH_PERMS,
			"SPOOL_JOB_FILES_WITH_PERMS", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE);
	 daemonCore->Register_Command(TRANSFER_DATA_WITH_PERMS,
			"TRANSFER_DATA_WITH_PERMS", 
			(CommandHandlercpp)&Scheduler::spoolJobFiles, 
			"spoolJobFiles", this, WRITE);
	 daemonCore->Register_Command(UPDATE_GSI_CRED,"UPDATE_GSI_CRED",
			(CommandHandlercpp)&Scheduler::updateGSICred,
			"updateGSICred", this, WRITE);
	 daemonCore->Register_Command(DELEGATE_GSI_CRED_SCHEDD,
			"DELEGATE_GSI_CRED_SCHEDD",
			(CommandHandlercpp)&Scheduler::updateGSICred,
			"updateGSICred", this, WRITE);
	 daemonCore->Register_Command(REQUEST_SANDBOX_LOCATION,
			"REQUEST_SANDBOX_LOCATION",
			(CommandHandlercpp)&Scheduler::requestSandboxLocation,
			"requestSandboxLocation", this, WRITE);
	daemonCore->Register_Command( ALIVE, "ALIVE", 
			(CommandHandlercpp)&Scheduler::receive_startd_alive,
			"receive_startd_alive", this, DAEMON,
			D_PROTOCOL ); 

	// Command handler for testing file access.  I set this as WRITE as we
	// don't want people snooping the permissions on our machine.
	daemonCore->Register_Command( ATTEMPT_ACCESS, "ATTEMPT_ACCESS",
								  (CommandHandler)&attempt_access_handler,
								  "attempt_access_handler", NULL, WRITE,
								  D_FULLDEBUG );
#ifdef WIN32
	// Command handler for stashing credentials.
	daemonCore->Register_Command( STORE_CRED, "STORE_CRED",
								(CommandHandler)&store_cred_handler,
								"cred_access_handler", NULL, WRITE,
								D_FULLDEBUG );
#endif


	// handler for queue management commands
	// Note: We make QMGMT_CMD a READ command.  The command handler
	// itself calls daemonCore->Verify() to check for WRITE access if
	// someone tries to modify the queue.
	daemonCore->Register_Command( QMGMT_CMD, "QMGMT_CMD",
								  (CommandHandler)&handle_q,
								  "handle_q", NULL, READ, D_FULLDEBUG );

	daemonCore->Register_Command( DUMP_STATE, "DUMP_STATE",
								  (CommandHandlercpp)&Scheduler::dumpState,
								  "dumpState", this, READ  );

	daemonCore->Register_Command( GET_MYPROXY_PASSWORD, "GET_MYPROXY_PASSWORD",
								  (CommandHandler)&get_myproxy_password_handler,
								  "get_myproxy_password", NULL, WRITE, D_FULLDEBUG  );


	daemonCore->Register_Command( GET_JOB_CONNECT_INFO, "GET_JOB_CONNECT_INFO",
								  (CommandHandlercpp)&Scheduler::get_job_connect_info_handler,
								  "get_job_connect_info", this, WRITE );

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

	// clear previous timers
	if (timeoutid >= 0) {
		daemonCore->Cancel_Timer(timeoutid);
	}
	if (startjobsid >= 0) {
		daemonCore->Cancel_Timer(startjobsid);
	}
	if (aliveid >= 0) {
		daemonCore->Cancel_Timer(aliveid);
	}
	if (periodicid>=0) {
		daemonCore->Cancel_Timer(periodicid);
	}

	 // timer handlers
	timeoutid = daemonCore->Register_Timer(10,
		(Eventcpp)&Scheduler::timeout,"timeout",this);
	startjobsid = daemonCore->Register_Timer(10,
		(Eventcpp)&Scheduler::StartJobs,"StartJobs",this);
	aliveid = daemonCore->Register_Timer(10, alive_interval,
		(Eventcpp)&Scheduler::sendAlives,"sendAlives", this);
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
            (Event)&CleanJobQueue,"CleanJobQueue");
    }
    oldQueueCleanInterval = QueueCleanInterval;

	if (WallClockCkptInterval) {
		if( wallclocktid != -1 ) {
			daemonCore->Reset_Timer_Period(wallclocktid,WallClockCkptInterval);
		}
		else {
			wallclocktid = daemonCore->Register_Timer(WallClockCkptInterval,
												  WallClockCkptInterval,
												  (Event)&CkptWallClock,
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
			(Eventcpp)&Scheduler::PeriodicExprHandler,"PeriodicExpr",this);
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
	 /* compare job priorities: higher values have more priority */
	 if( a->job_prio < b->job_prio ) {
		  return 1;
	 }
	 if( a->job_prio > b->job_prio ) {
		  return -1;
	 }

	 /* here,updown priority and job_priority are both equal */
	 /* check existence of checkpoint files */
	 if (( a->status == UNEXPANDED) && ( b->status != UNEXPANDED))
		  return ( 1);
	 if (( a->status != UNEXPANDED) && ( b->status == UNEXPANDED))
		  return (-1);

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

		// The SetMaxHistoricalLogs is initialized in main_init(), we just need to check here
		// for changes.  
	int max_saved_rotations = param_integer( "MAX_JOB_QUEUE_LOG_ROTATIONS", DEFAULT_MAX_JOB_QUEUE_LOG_ROTATIONS );
	SetMaxHistoricalLogs(max_saved_rotations);
}

// NOTE: this is likely unreachable now, and may be removed
void
Scheduler::update_local_ad_file() 
{
	daemonCore->UpdateLocalAd(m_ad);
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

	if ( CronMgr && ( ! CronMgr->ShutdownOk() ) ) {
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
		 ( CronMgr && ( CronMgr->ShutdownOk() ) )  ) {
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
	if( CronMgr ) {
		CronMgr->Shutdown( false );
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
	if( CronMgr ) {
		CronMgr->Shutdown( true );
	}

		// Since this is just sending a bunch of UDP updates, we can
		// still invalidate our classads, even on a fast shutdown.
	invalidate_ads();

	int num_closed = daemonCore->Cancel_And_Close_All_Sockets();
		// now that these have been canceled and deleted, we should
		// set these to NULL so that we don't try to use them again.
	shadowCommandrsock = NULL;
	shadowCommandssock = NULL;
	dprintf( D_FULLDEBUG, "Canceled/Closed %d socket(s) at shutdown\n",
			 num_closed ); 

#if HAVE_DLOPEN
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
	if( CronMgr ) {
		dprintf( D_ALWAYS, "Deleting CronMgr\n" );
		CronMgr->Shutdown( true );
		delete CronMgr;
		CronMgr = NULL;
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

	int num_closed = daemonCore->Cancel_And_Close_All_Sockets();
		// now that these have been canceled and deleted, we should
		// set these to NULL so that we don't try to use them again.
	shadowCommandrsock = NULL;
	shadowCommandssock = NULL;
	dprintf( D_FULLDEBUG, "Canceled/Closed %d socket(s) at shutdown\n",
			 num_closed ); 

#if HAVE_DLOPEN
	ScheddPluginManager::Shutdown();
	ClassAdLogPluginManager::Shutdown();
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
		// regular one, so just delete it and start over again.
	delete m_ad;
	m_ad = new ClassAd;
    m_ad->SetMyTypeName( QUERY_ADTYPE );
    m_ad->SetTargetTypeName( SCHEDD_ADTYPE );

        // Invalidate the schedd ad
    line.sprintf( "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, Name );
    m_ad->Insert( line.Value() );


		// Update collectors
	daemonCore->sendUpdates(INVALIDATE_SCHEDD_ADS, m_ad, NULL, false);

	if (N_Owners == 0) return;	// no submitter ads to invalidate

		// Invalidate all our submittor ads.
	line.sprintf( "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_SCHEDD_NAME,
				  Name );
    m_ad->InsertOrUpdate( line.Value() );

	daemonCore->sendUpdates(INVALIDATE_SUBMITTOR_ADS, m_ad, NULL, false);


	Daemon* d;
	if( FlockCollectors && FlockLevel > 0 ) {
		for( i=1, FlockCollectors->rewind();
			 i <= FlockLevel && FlockCollectors->next(d); i++ ) {
			((DCCollector*)d)->sendUpdate( INVALIDATE_SUBMITTOR_ADS, m_ad, NULL, false );
		}
	}
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
	timeout();
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


match_rec*
Scheduler::AddMrec(char* id, char* peer, PROC_ID* jobId, const ClassAd* my_match_ad,
				   char *user, char *pool, match_rec **pre_existing)
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

	return rec;
}

// All deletions of match records _MUST_ go through DelMrec() to ensure
// proper cleanup.
int
Scheduler::DelMrec(char const* id)
{
	match_rec *rec;
	HashKey key(id);

	if(!id)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not deleted\n");
		return -1;
	}

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
		if (!ipv->FillHole(DAEMON, *match->auth_hole_id)) {
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
	bool		found = false;
	match_rec	*rec;

	matches->startIterations();
	while (matches->iterate(rec) == 1) {
		if(rec->shadowRec == shadow) {
			rec->shadowRec = NULL;
				// re-associate match with the original job cluster
			SetMrecJobID(rec,rec->origcluster,-1);
			found = true;
		}
	}
	if( !found ) {
			// Try the dedicated scheduler
		found = dedicated_scheduler.removeShadowRecFromMrec( shadow );
	}
	if( ! found ) {
		dprintf( D_FULLDEBUG, "Shadow does not have a match record, "
				 "so did not remove it from the match\n");
	}
}

int
Scheduler::AlreadyMatched(PROC_ID* id)
{
	int universe;

	if (GetAttributeInt(id->cluster, id->proc,
						ATTR_JOB_UNIVERSE, &universe) < 0) {
		dprintf(D_FULLDEBUG, "GetAttributeInt() failed\n");
		return FALSE;
	}

	if ( (universe == CONDOR_UNIVERSE_PVM) ||
		 (universe == CONDOR_UNIVERSE_MPI) ||
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
	classy_counted_ptr<DCStartd> startd = new DCStartd( mrec->peer );
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
	s->get_secret(claim_id);	// must free this; CEDAR will malloc cuz claimid=NULL
	if ( !s->end_of_message() ) {
		if (claim_id) free(claim_id);
		return FALSE;
	}
	
	if ( claim_id ) {
		matches->lookup( HashKey(claim_id), match );
		free(claim_id);
	}

	if ( match ) {
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
	}

	s->encode();
	s->code(ret_value);
	s->end_of_message();

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
			if ( startd_sends_alives ) {
				// if the startd sends alives, then the ATTR_LAST_JOB_LEASE_RENEWAL
				// is updated someplace else in RAM only when we receive a keepalive
				// ping from the startd.  So here
				// we just want to read it out of RAM and set it via SetAttributeInt
				// so it is written to disk.  Doing things this way allows us
				// to update the queue persistently all in one transaction, even
				// if startds are sending updates asynchronously.  -Todd Tannenbaum 
				GetAttributeInt(mrec->cluster,mrec->proc,
								ATTR_LAST_JOB_LEASE_RENEWAL,&now);
			}
			SetAttributeInt( mrec->cluster, mrec->proc, 
							 ATTR_LAST_JOB_LEASE_RENEWAL, now ); 
		}
	}
	CommitTransaction();

	if ( !startd_sends_alives ) {
		matches->startIterations();
		while (matches->iterate(mrec) == 1) {
			if( mrec->status == M_ACTIVE || mrec->status == M_CLAIMED ) {
				if( sendAlive( mrec ) ) {
					numsent++;
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
	cad->Assign( "CondorAdministrator", CondorAdministrator );
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
	
	unsigned long phys_mem = sysapi_phys_memory( );
	cad->Assign( ATTR_MEMORY, (int)phys_mem );
	
	unsigned long disk_space = sysapi_disk_space( this->LocalUnivExecuteDir );
	cad->Assign( ATTR_DISK, (int)disk_space );
	
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
	int timeout = 20;

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
					 errstack.getFullText() );
			return FALSE;
		}
	}

	if( !input.initFromStream(*s) || !s->eom() ) {
		dprintf(D_ALWAYS,
				"Failed to receive input ClassAd for GET_JOB_CONNECT_INFO\n");
		return FALSE;
	}

	if( !input.LookupInteger(ATTR_CLUSTER_ID,jobid.cluster) ||
		!input.LookupInteger(ATTR_PROC_ID,jobid.proc) ) {
		error_msg.sprintf("Job id missing from GET_JOB_CONNECT_INFO request");
		goto error_wrapup;
	}

	input.LookupString(ATTR_SESSION_INFO,job_owner_session_info);

	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if( !jobad ) {
		error_msg.sprintf("No such job: %d.%d", jobid.cluster, jobid.proc);
		goto error_wrapup;
	}

	if( !OwnerCheck2(jobad,sock->getOwner()) ) {
		error_msg.sprintf("%s is not authorized for access to the starter for job %d.%d",
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
	case CONDOR_UNIVERSE_PVM:
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
				error_msg.sprintf("This is a parallel job.  Please specify job %d.%d.X where X is an integer from 0 to %d.",jobid.cluster,jobid.proc,claim_idlist.number()-1);
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
		else if (job_status != RUNNING) {
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
			startd_name = my_full_hostname();
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
		else if (job_status != RUNNING) {
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
		error_msg.sprintf("Job %d.%d is denied by SCHEDD_ENABLE_SSH_TO_JOB.",
						  jobid.cluster,jobid.proc);
		goto error_wrapup;
	}


	if( !job_is_suitable )
	{
		if( !retry_is_sensible ) {
				// this must be a job universe that we don't support
			error_msg.sprintf("Job %d.%d does not support remote access.",
							  jobid.cluster,jobid.proc);
		}
		else {
			error_msg.sprintf("Job %d.%d is not running.",
							  jobid.cluster,jobid.proc);
		}
		goto error_wrapup;
	}

	if( mrec ) { // locate starter by calling startd
		MyString global_job_id;
		MyString startd_addr = mrec->peer;

		DCStartd startd(startd_name.Value(),NULL,startd_addr.Value(),mrec->secSessionId() );

		jobad->LookupString(ATTR_GLOBAL_JOB_ID,global_job_id);

		if( !startd.locateStarter(global_job_id.Value(),mrec->claimId(),daemonCore->publicNetworkIpAddr(),&starter_ad,timeout) )
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

		if( !starter.createJobOwnerSecSession(timeout,job_claimid,match_sec_session_id,job_owner_session_info.Value(),starter_claim_id,error_msg,starter_version,starter_addr) ) {
			goto error_wrapup; // error_msg already set
		}
	}

	reply.Assign(ATTR_RESULT,true);
	reply.Assign(ATTR_STARTER_IP_ADDR,starter_addr.Value());
	reply.Assign(ATTR_CLAIM_ID,starter_claim_id.Value());
	reply.Assign(ATTR_VERSION,starter_version.Value());
	reply.Assign(ATTR_REMOTE_HOST,startd_name.Value());
	if( !reply.put(*s) || !s->eom() ) {
		dprintf(D_ALWAYS,
				"Failed to send response to GET_JOB_CONNECT_INFO\n");
	}

	dprintf(D_FULLDEBUG,"Produced connect info for %s job %d.%d startd %s.\n",
			sock->getFullyQualifiedUser(), jobid.cluster, jobid.proc,
			starter_addr.Value() );

	return TRUE;

 error_wrapup:
	dprintf(D_ALWAYS,"GET_JOB_CONNECT_INFO failed: %s\n",error_msg.Value());
	reply.Assign(ATTR_RESULT,false);
	reply.Assign(ATTR_ERROR_STRING,error_msg);
	if( retry_is_sensible ) {
		reply.Assign(ATTR_RETRY,retry_is_sensible);
	}
	if( !reply.put(*s) || !s->eom() ) {
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
	s->eom();

	s->encode();
	
	job_ad.put( *s );

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

	user.sprintf( "%s%s@%s",
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

	default:
		return;
	}
}


bool
moveStrAttr( PROC_ID job_id, const char* old_attr, const char* new_attr,
			 bool verbose )
{
	char* value = NULL;
	MyString new_value;
	int rval;

	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  old_attr, &value) < 0 ) { 
		if( verbose ) {
			dprintf( D_FULLDEBUG, "No %s found for job %d.%d\n",
					 old_attr, job_id.cluster, job_id.proc );
		}
		return false;
	}
	
	new_value += '"';
	new_value += value;
	new_value += '"';
	free( value );
	value = NULL;

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

		dprintf(D_FULLDEBUG, "remove by constrain matched: %d.%d\n",
				jobs[job_count].cluster, jobs[job_count].proc);

		job_count++;

		ad = GetNextJobByConstraint(constraint, 0);
	}

	job_count--;
	while ( job_count >= 0 ) {
		dprintf(D_FULLDEBUG, "removing: %d.%d\n",
				jobs[job_count].cluster, jobs[job_count].proc);

		result = result && abortJobRaw(jobs[job_count].cluster,
									   jobs[job_count].proc,
									   reason);

		job_count--;
	}

	if ( use_transaction ) {
		if ( result ) {
			CommitTransaction();
		} else {
			AbortTransaction();
		}
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
		 bool notify_shadow, bool email_user,
		 bool email_admin, bool system_hold )
{
	int status;
	PROC_ID tmp_id;
	tmp_id.cluster = cluster;
	tmp_id.proc = proc;
	int system_holds = 0;

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

	abort_job_myself( tmp_id, JA_HOLD_JOBS, true, notify_shadow );

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

		MyString msg_buf;
		msg_buf.sprintf( "Condor job %d.%d has been put on hold.\n%s\n"
						 "Please correct this problem and release the "
						 "job with \"condor_release\"\n",
						 cluster, proc, reason );

		MyString msg_subject;
		msg_subject.sprintf( "Condor job %d.%d put on hold", cluster, proc );

		FILE* fp;
		if( email_user ) {
			fp = email_user_open( job_ad, msg_subject.Value() );
			if( fp ) {
				fprintf( fp, msg_buf.Value() );
				email_close( fp );
			}
		}
		FreeJobAd( job_ad );
		if( email_admin ) {
			fp = email_admin_open( msg_subject.Value() );
			if( fp ) {
				fprintf( fp, msg_buf.Value() );
				email_close( fp );
			}
		}			
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
		 bool use_transaction, bool notify_shadow, bool email_user,
		 bool email_admin, bool system_hold )
{
	bool result;

	if(use_transaction) {
		BeginTransaction();
	}

	result = holdJobRaw(cluster,proc,reason,notify_shadow,email_user,email_admin,system_hold);

	if(use_transaction) {
		if(result) {
			CommitTransaction();
		} else {
			AbortTransaction();
		}
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

		MyString msg_buf;
		msg_buf.sprintf( "Condor job %d.%d has been released from being "
						 "on hold.\n%s", cluster, proc, reason );

		MyString msg_subject;
		msg_subject.sprintf( "Condor job %d.%d released from hold state",
							 cluster, proc );

		FILE* fp;
		if( email_user ) {
			fp = email_user_open( job_ad, msg_subject.Value() );
			if( fp ) {
				fprintf( fp, msg_buf.Value() );
				email_close( fp );
			}
		}
		FreeJobAd( job_ad );
		if( email_admin ) {
			fp = email_admin_open( msg_subject.Value() );
			if( fp ) {
				fprintf( fp, msg_buf.Value() );
				email_close( fp );
			}
		}			
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
	int negotiator_interval = param_integer("NEGOTIATOR_INTERVAL",60);
	int claimlocal_interval = param_integer("SCHEDD_ASSUME_NEGOTIATOR_GONE",
				negotiator_interval * 20);
				//,	// default (20 min usually)
				//10 * 60,	// minimum = 10 minutes
				//120 * 60);	// maximum = 120 minutes
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
		"Haven't heard from negotiator, trying to claim local startd\n");

		// Fetch all the slot (machine) ads from the local startd
	CondorError errstack;
	CondorQuery query(STARTD_AD);
	QueryResult q;
	ClassAdList result;
	q = query.fetchAds(result, startd_addr, &errstack);
	if ( q != Q_OK ) {
		dprintf(D_FULLDEBUG,
			"ERROR: could not fetch ads from local startd- %s\n",
			getStrQueryResult(q));
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
		FILE* fp=safe_fopen_wrapper(file_name,"r");
		if ( fp ) {
			fscanf(fp,"%150s\n",claim_id);
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
	if ( m_ad == NULL ) return;
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
	if ( status == RUNNING ) {
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
	if ( ! calculate && jobAd->Lookup( ATTR_DEFERRAL_TIME ) != NULL ) {
			//
			// First get the DeferralTime
			//
		int deferralTime = 0;
		jobAd->EvalInteger( ATTR_DEFERRAL_TIME, NULL, deferralTime );
			//
			// Now look to see if they also have a DeferralWindow
			//
		int deferralWindow = 0;
		if ( jobAd->Lookup( ATTR_DEFERRAL_WINDOW ) != NULL ) {
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
	}

	virtual MessageClosureEnum messageSent(
				DCMessenger *messenger, Sock *sock )
	{
		shadow_rec *srec = scheduler.FindSrecByProcID( m_proc );
		if( srec && srec->pid == thePid() ) {
			srec->preempted = TRUE;
		}
		return DCSignalMsg::messageSent(messenger,sock);
	}

private:
	PROC_ID m_proc;
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
