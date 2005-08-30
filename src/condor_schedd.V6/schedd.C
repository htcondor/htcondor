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
#include "classad_helpers.h"
#include "condor_adtypes.h"
#include "condor_string.h"
#include "condor_email.h"
#include "environ.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "get_daemon_name.h"
#include "renice_self.h"
#include "user_log.c++.h"
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


#define DEFAULT_SHADOW_SIZE 125
#define DEFAULT_JOB_START_COUNT 1

#define SUCCESS 1
#define CANT_RUN 0

extern char *gen_ckpt_name();

extern GridUniverseLogic* _gridlogic;

#include "condor_qmgr.h"
#include "qmgmt.h"

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

extern int get_job_prio(ClassAd *ad);
extern void	FindRunnableJob(PROC_ID & jobid, const ClassAd* my_match_ad, 
					 char * user);
extern int Runnable(PROC_ID*);
extern int Runnable(ClassAd*);
extern bool	operator==( PROC_ID, PROC_ID );

extern char* Spool;
extern char * Name;
static char * NameInEnv = NULL;
extern char * JobHistoryFileName;
extern char * mySubSystem;

extern FILE *DebugFP;
extern char *DebugFile;
extern char *DebugLock;

extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;

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
shadow_rec * add_shadow_rec( int, PROC_ID*, int, match_rec*, int );
bool service_this_universe(int, ClassAd*);
bool jobIsSandboxed( ClassAd* ad );
bool getSandbox( int cluster, int proc, MyString & path );
bool sandboxHasRightOwner( int cluster, int proc, ClassAd* job_ad );
bool jobPrepNeedsThread( int cluster, int proc );
bool jobCleanupNeedsThread( int cluster, int proc );
bool jobExternallyManaged(ClassAd * ad);

int	WallClockCkptInterval = 0;
static bool gridman_per_job = false;
int STARTD_CONTACT_TIMEOUT = 45;

#ifdef CARMI_OPS
struct shadow_rec *find_shadow_by_cluster( PROC_ID * );
#endif

struct job_data_transfer_t {
	int mode;
	char *peer_version;
	ExtArray<PROC_ID> *jobs;
};

#ifdef WIN32	 // on unix, this is in instantiate.C
bool operator==(const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}
#endif

int
dc_reconfig()
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGHUP );
	return TRUE;
}


match_rec::match_rec( char* claim_id, char* p, PROC_ID* job_id, 
					  const ClassAd *match, char *the_user, char *my_pool )
{
	id = strdup( claim_id );
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
	allocated = false;
	scheduled = false;
}


match_rec::~match_rec()
{
	if( id ) {
		free( id );
	}
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
}


void
match_rec::setStatus( int stat )
{
	status = stat;
	entered_current_status = (int)time(0);
}


ContactStartdArgs::ContactStartdArgs( char* the_claim_id, char* sinful, bool is_dedicated ) 
{
	csa_claim_id = strdup( the_claim_id );
	csa_sinful = strdup( sinful );
	csa_is_dedicated = is_dedicated;
}


ContactStartdArgs::~ContactStartdArgs()
{
	free( csa_claim_id );
	free( csa_sinful );
}


Scheduler::Scheduler() :
	job_is_finished_queue( "job_is_finished_queue", 1,
						   &CondorID::ServiceDataCompare )
{
	ad = NULL;
	MySockName = NULL;
	MyShadowSockName = NULL;
	shadowCommandrsock = NULL;
	shadowCommandssock = NULL;
	SchedDInterval = 0;
	SchedDMinInterval = 0;
	QueueCleanInterval = 0; JobStartDelay = 0;
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

	ShadowSizeEstimate = 0;

	N_RejectedClusters = 0;
	N_Owners = 0;
	LastTimeout = time(NULL);

	Collectors = NULL;
		//gotiator = NULL;
	CondorAdministrator = NULL;
	Mail = NULL;
	filename = NULL;
	alive_interval = 0;
	leaseAliveInterval = 500000;	// init to a nice big number
	aliveid = -1;
	ExitWhenDone = FALSE;
	matches = NULL;
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

	startJobsDelayBit = FALSE;
	num_reg_contacts = 0;
#ifndef WIN32
	MAX_STARTD_CONTACTS = getdtablesize() - 20;  // save 20 fds...
		// Just in case getdtablesize() returns <= 20, we've got to
		// let the schedd try at least 1 connection at a time.
		// Derek, Todd, Pete K. 1/19/01
	if( MAX_STARTD_CONTACTS < 1 ) {
		MAX_STARTD_CONTACTS = 1;
	}
#else
	// on Windows NT, it appears you can open up zillions of sockets
	MAX_STARTD_CONTACTS = 2000;
#endif
	checkContactQueue_tid = -1;
	checkReconnectQueue_tid = -1;

	job_is_finished_queue.
		registerHandlercpp( (ServiceDataHandlercpp)
							&Scheduler::jobIsFinishedHandler, this );

	sent_shadow_failure_email = FALSE;
	ManageBandwidth = false;
	RejectedClusters.setFiller(0);
	RejectedClusters.resize(MAX_REJECTED_CLUSTERS);
	_gridlogic = NULL;

	CronMgr = NULL;
}


Scheduler::~Scheduler()
{
	delete ad;
	if (MySockName)
		free(MySockName);
	if (MyShadowSockName)
		free(MyShadowSockName);
	if( LocalUnivExecuteDir ) {
		free( LocalUnivExecuteDir );
	}

	if ( CronMgr ) {
		delete CronMgr;
		CronMgr = NULL;
	}

		// we used to cancel and delete the shadowCommand*socks here,
		// but now that we're calling Cancel_And_Close_All_Sockets(),
		// they're already getting cleaned up, so if we do it again,
		// we'll seg fault.


	if ( Collectors ) {
		delete( Collectors );
	}
	if (CondorAdministrator)
		free(CondorAdministrator);
	if (Mail)
		free(Mail);
	delete []filename;
	if (matches) {
		matches->startIterations();
		match_rec *rec;
		HashKey id;
		while (matches->iterate(id, rec) == 1) {
			delete rec;
		}
		delete matches;
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
		ClassAd* ad;
		while (resourcesByProcID->iterate(ad) == 1) {
			if (ad) delete ad;
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

	if (_gridlogic)
		delete _gridlogic;
}

void
Scheduler::timeout()
{
	static bool min_interval_timer_set = false;
	static time_t next_timeout = 0;
	time_t right_now;

	right_now = time(NULL);
	if ( right_now < next_timeout ) {
		if (!min_interval_timer_set) {
			daemonCore->Reset_Timer(timeoutid,next_timeout - right_now,1);
				// The line below fixes the "scheduler universe five-
				// minute delay" bug (Gnats PR 532).  wenger 2005-08-30.
			daemonCore->Reset_Timer(startjobsid,
						next_timeout - right_now + 1, 1);
			min_interval_timer_set = true;
		}
		return;
	}

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
	}

	/* Reset our timer */
	daemonCore->Reset_Timer(timeoutid,SchedDInterval);
	min_interval_timer_set = false;
	next_timeout = right_now + SchedDMinInterval;
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
					dprintf(D_ALWAYS,"timed out requesting claim from %s\n",rec->peer);
					send_vacate(rec, RELEASE_CLAIM);
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
		Owners[i].GlobusJobs = 0;
		Owners[i].GlobusUnmanagedJobs = 0;
		Owners[i].NegotiationTimestamp = current_time;
	}

		// Clear out the DedicatedScheduler's list of idle dedicated
		// job cluster ids, since we're about to re-create it.
	dedicated_scheduler.clearDedicatedClusters();

	WalkJobQueue((int(*)(ClassAd *)) count );

	if( dedicated_scheduler.hasDedicatedClusters() ) {
			// We found some dedicated clusters to service.  Wake up
			// the DedicatedScheduler class in a few seconds to deal
			// with them.
		dedicated_scheduler.handleDedicatedJobTimer( 2 );
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
				 SchedDInterval*2) && (Owners[i].FlockLevel < MaxFlockLevel)) {
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
	ad->InsertOrUpdate(tmp);
	
	sprintf(tmp, "%s = %d", ATTR_MAX_JOBS_RUNNING, MaxJobsRunning);
	ad->InsertOrUpdate(tmp);
	
	 sprintf(tmp, "%s = \"%s\"", ATTR_NAME, Name);
	 ad->InsertOrUpdate(tmp);

	 sprintf(tmp, "%s = %d", ATTR_VIRTUAL_MEMORY, SwapSpace );
	 ad->InsertOrUpdate(tmp);

	// The schedd's ad should not have idle and running job counts --- 
	// only the per user queue ads should.  Instead, the schedd has
	// TotalIdleJobs and TotalRunningJobs.
	ad->Delete (ATTR_IDLE_JOBS);
	ad->Delete (ATTR_RUNNING_JOBS);
	ad->Delete (ATTR_HELD_JOBS);
	ad->Delete (ATTR_FLOCKED_JOBS);
	// The schedd's ad doesn't need ATTR_SCHEDD_NAME either.
	ad->Delete (ATTR_SCHEDD_NAME);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_IDLE_JOBS, JobsIdle);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_RUNNING_JOBS, JobsRunning);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_JOB_ADS, JobsTotalAds);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_HELD_JOBS, JobsHeld);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_FLOCKED_JOBS, JobsFlocked);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_REMOVED_JOBS, JobsRemoved);
	ad->Insert (tmp);

    daemonCore->monitor_data.ExportData(ad);
	extra_ads.Publish( ad );
    
	if ( param_boolean("ENABLE_SOAP", false) ) {
			// If we can support the SOAP API let's let the world know!
		sprintf(tmp, "%s = True", ATTR_HAS_SOAP_API);
		ad->Insert(tmp);
	}


        // Tell negotiator to send us the startd ad
	sprintf(tmp, "%s = True", ATTR_WANT_RESOURCE_AD );
	ad->InsertOrUpdate(tmp);

		// Update collectors
	int num_updates = Collectors->sendUpdates ( UPDATE_SCHEDD_AD, ad );
	dprintf( D_FULLDEBUG, 
			 "Sent HEART BEAT ad to %d collectors. Number of submittors=%d\n",
			 num_updates,
				 N_Owners );   

	// send the schedd ad to our flock collectors too, so we will
	// appear in condor_q -global and condor_status -schedd
	if( FlockCollectors ) {
		FlockCollectors->rewind();
		Daemon* d;
		DCCollector* col;
		FlockCollectors->next(d);
		for( i=0; d && i < FlockLevel; i++ ) {
			col = (DCCollector*)d;
			col->sendUpdate( UPDATE_SCHEDD_AD, ad );
			FlockCollectors->next( d );
		}
	}

	// The per user queue ads should not have NumUsers in them --- only
	// the schedd ad should.  In addition, they should not have
	// TotalRunningJobs and TotalIdleJobs
	ad->Delete (ATTR_NUM_USERS);
	ad->Delete (ATTR_TOTAL_RUNNING_JOBS);
	ad->Delete (ATTR_TOTAL_IDLE_JOBS);
	ad->Delete (ATTR_TOTAL_JOB_ADS);
	ad->Delete (ATTR_TOTAL_HELD_JOBS);
	ad->Delete (ATTR_TOTAL_FLOCKED_JOBS);
	ad->Delete (ATTR_TOTAL_REMOVED_JOBS);
	sprintf(tmp, "%s = \"%s\"", ATTR_SCHEDD_NAME, Name);
	ad->InsertOrUpdate(tmp);


	for ( i=0; i<N_Owners; i++) {
	  sprintf(tmp, "%s = %d", ATTR_RUNNING_JOBS, Owners[i].JobsRunning);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS, Owners[i].JobsIdle);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_HELD_JOBS, Owners[i].JobsHeld);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_FLOCKED_JOBS, Owners[i].JobsFlocked);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, Owners[i].Name, UidDomain);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  dprintf( D_ALWAYS, "Sent ad to central manager for %s@%s\n", 
			   Owners[i].Name, UidDomain );

		// Update collectors
	  int num_updates = Collectors->sendUpdates( UPDATE_SUBMITTOR_AD, ad );
	  dprintf( D_ALWAYS, "Sent ad to %d collectors for %s@%s\n", 
				 num_updates,
				 Owners[i].Name, UidDomain );
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
			match_rec *rec;
			while(matches->iterate(rec) == 1) {
				char *at_sign = strchr(rec->user, '@');
				if (at_sign) *at_sign = '\0';
				int OwnerNum = insert_owner( rec->user );
				if (at_sign) *at_sign = '@';
				if (rec->shadowRec && rec->pool &&
					!strcmp(rec->pool, flock_neg->pool())) {
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
				ad->InsertOrUpdate(tmp);
				sprintf(tmp, "%s = %d", ATTR_RUNNING_JOBS,
						Owners[i].JobsRunning);
				ad->InsertOrUpdate(tmp);
				sprintf(tmp, "%s = %d", ATTR_FLOCKED_JOBS,
						Owners[i].JobsFlocked);
				ad->InsertOrUpdate(tmp);
				sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, Owners[i].Name,
						UidDomain);
				ad->InsertOrUpdate(tmp);
				flock_col->sendUpdate( UPDATE_SUBMITTOR_AD, ad );
			}
		}
	}

	for (i=0; i < N_Owners; i++) {
		Owners[i].OldFlockLevel = Owners[i].FlockLevel;
	}

	 // Tell our GridUniverseLogic class what we've seen in terms
	 // of Globus Jobs per owner.
	 // Don't bother if we are starting a gridmanager per job.
	if ( !gridman_per_job ) {
		for (i=0; i < N_Owners; i++) {
			if ( Owners[i].GlobusJobs > 0 ) {
				GridUniverseLogic::JobCountUpdate(Owners[i].Name, 
						Owners[i].Domain,NULL, NULL, 0, 0, 
						Owners[i].GlobusJobs,Owners[i].GlobusUnmanagedJobs);
			}
		}
	}

	 // send info about deleted owners
	 // put 0 for idle & running jobs

	sprintf(tmp, "%s = 0", ATTR_RUNNING_JOBS);
	ad->InsertOrUpdate(tmp);
	sprintf(tmp, "%s = 0", ATTR_IDLE_JOBS);
	ad->InsertOrUpdate(tmp);

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
	  ad->InsertOrUpdate(tmp);

		// Update collectors
	  int num_updates = 
		  Collectors->sendUpdates( UPDATE_SUBMITTOR_AD, ad );
	  dprintf (D_ALWAYS, "Sent owner (0 jobs) ad to %d collectors\n", num_updates);

	  // also update all of the flock hosts
	  int i;
	  Daemon *d;
	  if( FlockCollectors ) {
		  for( i=1, FlockCollectors->rewind();
			   i <= OldOwners[i].OldFlockLevel &&
				   FlockCollectors->next(d); i++ ) {
			  ((DCCollector*)d)->sendUpdate( UPDATE_SUBMITTOR_AD, ad );
		  }
	  }
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
	char 	buf[_POSIX_PATH_MAX];
	char 	buf2[_POSIX_PATH_MAX];
	char*	owner;
	char 	domain[_POSIX_PATH_MAX];
	int		cur_hosts;
	int		max_hosts;
	int		universe;

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

	// calculate owner for per submittor information.
	buf[0] = '\0';
	job->LookupString(ATTR_ACCOUNTING_GROUP,buf,sizeof(buf));	// TODDCORE
	if ( buf[0] == '\0' ) {
		job->LookupString(ATTR_OWNER,buf,sizeof(buf));
		if ( buf[0] == '\0' ) {	
			dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
					ATTR_OWNER);
			return 0;
		}
	}
	owner = buf;

	// grab the domain too, if it exists
	domain[0] = '\0';
	job->LookupString(ATTR_NT_DOMAIN, domain);
	
	// With NiceUsers, the number of owners is
	// not the same as the number of submittors.  So, we first
	// check if this job is being submitted by a NiceUser, and
	// if so, insert it as a new entry in the "Owner" table
	if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
		strcpy(buf2,NiceUserName);
		strcat(buf2,".");
		strcat(buf2,owner);
		owner=buf2;		
	}

	// increment our count of the number of job ads in the queue
	scheduler.JobsTotalAds++;

	// insert owner even if REMOVED or HELD for condor_q -{global|sub}
	// this function makes its own copies of the memory passed in 
	int OwnerNum = scheduler.insert_owner( owner );

	// make certain gridmanager has a copy of mirrored jobs
	char *mirror_schedd_name = NULL;
	job->LookupString(ATTR_MIRROR_SCHEDD,&mirror_schedd_name);
	if ( mirror_schedd_name ) {
			// We have a mirrored job
		int job_managed = jobExternallyManaged(job);
		bool needs_management = true;
			// if job is held or completed and not managaged, don't worry about it.
		if ( ( status==HELD || status==COMPLETED || status==REMOVED ) &&
			 (job_managed==0 ) ) 
		{
			needs_management = false;
		}
		if ( needs_management ) {
				// note: get owner again cuz we don't want accounting group here.
			char owner[_POSIX_PATH_MAX];
			owner[0] = '\0';	
			job->LookupString(ATTR_OWNER,owner);	
			GridUniverseLogic::JobCountUpdate(owner,domain,mirror_schedd_name,ATTR_MIRROR_SCHEDD,
				0, 0, 1, job_managed ? 0 : 1);
		}
		free(mirror_schedd_name);
		mirror_schedd_name = NULL;
	}


	if ( (universe != CONDOR_UNIVERSE_GLOBUS) &&	// handle Globus below...
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

	if ( universe == CONDOR_UNIVERSE_GLOBUS ) {
		// for Globus, count jobs in UNSUBMITTED state by owner.
		// later we make certain there is a grid manager daemon
		// per owner.
		int needs_management = 0;
		int real_status = status;
		bool want_service = service_this_universe(universe,job);
		int job_managed = jobExternallyManaged(job);
		// if job is not already being managed : if we want matchmaking 
		// for this job, but we have not found a 
		// match yet, consider it "held" for purposes of the logic here.  we
		// have no need to tell the gridmanager to deal with it until we've
		// first found a match.
		if ( (job_managed == 0) && (want_service && cur_hosts == 0) ) {
			status = HELD;
		}
		// if status is REMOVED, but the job contact string is not null,
		// then consider the job IDLE for purposes of the logic here.  after all,
		// the gridmanager needs to be around to finish the task of removing the job.
		if ( status == REMOVED ) {
			char contact_string[20];
			strncpy(contact_string,NULL_JOB_CONTACT,sizeof(contact_string));
			job->LookupString(ATTR_GLOBUS_CONTACT_STRING,contact_string,
								sizeof(contact_string));
			if ( strncmp(contact_string,NULL_JOB_CONTACT,
							sizeof(contact_string)-1) ||
				 job->LookupString(ATTR_REMOTE_JOB_ID,contact_string,
								   sizeof(contact_string)-1) != 0 )
			{
				// looks like the job's globus contact string is still valid,
				// so there is still a job submitted remotely somewhere.
				// fire up the gridmanager to try and really clean it up!
				status = IDLE;
			}
		}

		// Don't count HELD jobs that aren't externally (gridmanager) managed
		if ( status != HELD || job_managed != 0 ) 
		{
			needs_management = 1;
			scheduler.Owners[OwnerNum].GlobusJobs++;
		}
		if ( status != HELD && job_managed == 0 ) 
		{
			scheduler.Owners[OwnerNum].GlobusUnmanagedJobs++;
		}
		if ( gridman_per_job ) {
			int cluster = 0;
			int proc = 0;
			job->LookupInteger(ATTR_CLUSTER_ID, cluster);
			job->LookupInteger(ATTR_PROC_ID, proc);
			GridUniverseLogic::JobCountUpdate(owner,domain,NULL,NULL,
					cluster, proc, needs_management, job_managed ? 0 : 1);
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
		case CONDOR_UNIVERSE_GLOBUS:
			{
				// If this Globus job is already being managed, then the schedd
				// should leave it alone... the gridmanager is dealing with it.
				int job_managed = jobExternallyManaged(job);
				if ( job_managed ) {
					return false;
				}			
				// Now if not managed, if the GlobusScheduler or RemoteSchedd 
				// has a "$$", then  this job is at least _matchable_, so 
				// return true, else false.
				const char * ads_to_check[] = { ATTR_GLOBUS_RESOURCE,
												 ATTR_REMOTE_SCHEDD };
				for (unsigned int i = 0; 
					     i < sizeof(ads_to_check)/sizeof(ads_to_check[0]);
					     i++) {
					char resource[500];
					resource[0] = '\0';
					job->LookupString(ads_to_check[i], resource);
					if ( strstr(resource,"$$") ) {
						return true;
					}
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
Scheduler::insert_owner(char* owner)
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
handle_mirror_job_notification(ClassAd *job_ad, int mode, PROC_ID & job_id)
{
		// Handle Mirrored Job removal/completion/hold.  
		// For a mirrored job, we want to notify the gridmanager if it is being
		// managed or if there is a remote job id, and then do whatever
		// else we would usually do.
	if (!job_ad) return;
	char *mirror_schedd_name = NULL;
	job_ad->LookupString(ATTR_MIRROR_SCHEDD,&mirror_schedd_name);
	if ( mirror_schedd_name ) {
			// We have a mirrored job
		int job_managed = jobExternallyManaged(job_ad);
			// If job_managed is true, then notify the gridmanager.
			// Special case: if job_managed is false, but the job is being removed
			// still has a mirror job id,
			// then consider the job still "managed" so
			// that the gridmanager will be notified.  
		if (!job_managed && mode==REMOVED ) {
			char tmp_str[2];
			tmp_str[0] = '\0';
			job_ad->LookupString(ATTR_MIRROR_JOB_ID,tmp_str,sizeof(tmp_str));
			if ( tmp_str[0] )
			{
				// looks like the mirror job id is still valid,
				// so there is still a job submitted remotely somewhere.
				// fire up the gridmanager to try and really clean it up!
				job_managed = 1;
			}
		}
		if ( job_managed  ) {
			char owner[_POSIX_PATH_MAX];
			char domain[_POSIX_PATH_MAX];
			owner[0] = '\0';
			domain[0] = '\0';
			job_ad->LookupString(ATTR_OWNER,owner);
			job_ad->LookupString(ATTR_NT_DOMAIN,domain);
			if ( gridman_per_job ) {
				GridUniverseLogic::JobRemoved(owner,domain,mirror_schedd_name,
					ATTR_MIRROR_SCHEDD,job_id.cluster,job_id.proc);
			} else {
				GridUniverseLogic::JobRemoved(owner,domain,mirror_schedd_name,
					ATTR_MIRROR_SCHEDD,0,0);
			}
		}
		free(mirror_schedd_name);
		mirror_schedd_name = NULL;
	}
}

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


		// Handle Mirror Job
	handle_mirror_job_notification(job_ad, mode, job_id);

		// Handle Globus Universe
	if (job_universe == CONDOR_UNIVERSE_GLOBUS) {
		int job_managed = jobExternallyManaged(job_ad);
			// If job_managed is true, then notify the gridmanager and return.
			// If job_managed is false, we will fall through the code at the
			// bottom of this function will handle the operation.
			// Special case: if job_managed is false, but the job being removed
			// and the jobmanager contact string is still valid, 
			// then consider the job still "managed" so
			// that the gridmanager will be notified.  
			// If the job contact string is still valid, that means there is
			// still a job remotely submitted that has not been removed.  When
			// the gridmanager confirms a job has been removed, it will set
			// ATTR_GLOBUS_CONTACT_STRING to be NULL_JOB_CONTACT.
		if (!job_managed && mode==REMOVED ) {
			char contact_string[20];
			strncpy(contact_string,NULL_JOB_CONTACT,sizeof(contact_string));
			job_ad->LookupString(ATTR_GLOBUS_CONTACT_STRING,contact_string,
								sizeof(contact_string));
			if ( strncmp(contact_string,NULL_JOB_CONTACT,
							sizeof(contact_string)-1) ||
				 job_ad->LookupString(ATTR_REMOTE_JOB_ID,contact_string,
								   sizeof(contact_string)-1) != 0 )
			{
				// looks like the job's globus contact string is still valid,
				// so there is still a job submitted remotely somewhere.
				// fire up the gridmanager to try and really clean it up!
				job_managed = 1;
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
			char owner[_POSIX_PATH_MAX];
			char domain[_POSIX_PATH_MAX];
			owner[0] = '\0';
			domain[0] = '\0';
			job_ad->LookupString(ATTR_OWNER,owner);
			job_ad->LookupString(ATTR_NT_DOMAIN,domain);
			if ( gridman_per_job ) {
				GridUniverseLogic::JobRemoved(owner,domain,NULL,NULL,job_id.cluster,job_id.proc);
			} else {
				GridUniverseLogic::JobRemoved(owner,domain,NULL,NULL,0,0);
			}
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
	if ((job_universe != CONDOR_UNIVERSE_GLOBUS) && 
		(srec = scheduler.FindSrecByProcID(job_id)) != NULL) 
	{

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
				handler_sig = DC_SIGHOLD;
				handler_sig_str = "DC_SIGHOLD";
				break;
			case JA_REMOVE_JOBS:
				handler_sig = DC_SIGREMOVE;
				handler_sig_str = "DC_SIGREMOVE";
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
			if( ! daemonCore->Send_Signal(srec->pid,handler_sig) ) {
				dprintf( D_ALWAYS,
						 "Error in sending %s to pid %d errno=%d (%s)\n",
						 handler_sig_str, srec->pid, errno, strerror(errno) );
			} else {
				dprintf( D_FULLDEBUG, "Sent %s to Job Handler Pid %d\n",
						 handler_sig_str, srec->pid );
				srec->preempted = TRUE;
			}


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
			if( ! daemonCore->Send_Signal(srec->pid,shadow_sig) ) {
				dprintf( D_ALWAYS,
						 "Error in sending %s to pid %d errno=%d (%s)\n",
						 shadow_sig_str, srec->pid, errno, strerror(errno) );
			} else {
				dprintf( D_FULLDEBUG, "Sent %s to Shadow Pid %d\n",
						 shadow_sig_str, srec->pid );
				srec->preempted = TRUE;
			}
            
        } else {  // Scheduler universe job
            
            dprintf( D_FULLDEBUG,
                     "Found record for scheduler universe job %d.%d\n",
                     job_id.cluster, job_id.proc);
            
			char owner[_POSIX_PATH_MAX];
			char domain[_POSIX_PATH_MAX];
			owner[0] = '\0';
			job_ad->LookupString(ATTR_OWNER,owner);
			job_ad->LookupString(ATTR_NT_DOMAIN,domain);
			if (! init_user_ids(owner, domain) ) {
				char tmpstr[255];
				dprintf(D_ALWAYS, "init_user_ids() failed - putting job on "
					   "hold.\n");
#ifdef WIN32
				snprintf(tmpstr, 255,
					   	"Bad or missing credential for user: %s", owner);
#else
				snprintf(tmpstr, 255, "Unable to switch to user: %s", owner);
#endif
				holdJob(job_id.cluster, job_id.proc, tmpstr, 
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
					 srec->pid, owner );
			priv_state priv = set_user_priv();

			if( daemonCore->Send_Signal(srec->pid, kill_sig) ) {
				// successfully sent signal
				srec->preempted = TRUE;
			}
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

	if( univ==CONDOR_UNIVERSE_SCHEDULER || univ==CONDOR_UNIVERSE_LOCAL ) {
		return 1;
	} else if(univ==CONDOR_UNIVERSE_GLOBUS) {
		if(managed) {
			return 0;
		} else {
			return 1;
		}
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

/* Return a MyString describing the policy's FiringExpressions. */
static MyString
ExplainPolicyDecision(UserPolicy & policy, ClassAd * jobad)
{
	ASSERT(jobad);
	const char *firing_expr = policy.FiringExpression();
	if( ! firing_expr ) {
		return "Unknown user policy expression";
	}
	MyString reason;

	ExprTree *tree, *rhs = NULL;
	tree = jobad->Lookup( firing_expr );

	// Get a formatted expression string
	char* exprString = NULL;
	if( tree && (rhs=tree->RArg()) ) {
		rhs->PrintToNewStr( &exprString );
	}

	// Format up the log entry
	reason.sprintf( "The %s expression '%s' evaluated to ",
					firing_expr,
					exprString ? exprString : "" );

	// Free up the buffer from PrintToNewStr()
	if ( exprString ) {
		free( exprString );
	}

	// Get a string for it's value
	int firing_value = policy.FiringExpressionValue();
	switch( firing_value ) {
	case 0:
		reason += "FALSE";
		break;
	case 1:
		reason += "TRUE";
		break;
	case -1:
		reason += "UNDEFINED";
		break;
	default:
		EXCEPT( "Unrecognized FiringExpressionValue: %d", 
				firing_value ); 
		break;
	}

	return reason;
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

	UserPolicy policy;
	policy.Init(jobad);

	action = policy.AnalyzePolicy(PERIODIC_ONLY);

	// Build a "reason" string for logging
	MyString reason = ExplainPolicyDecision(policy,jobad);

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

	return 1;
}

/*
For all of the jobs in the queue, evaluate the 
periodic user policy expressions.
*/

void
Scheduler::PeriodicExprHandler( void )
{
	WalkJobQueue(PeriodicExprEval);
}


bool
jobPrepNeedsThread( int cluster, int proc )
{
#ifdef WIN32
	// we never want to run in a thread on Win32, since
	// some of the stuff we do in the JobPrep thread
	// is NOT thread safe!!
	return false;
#endif 
		/*
		  make sure we can switch uids.  if not, we're not going to be
		  able to chown() the sandbox, nor switch to a dynamic user,
		  so there's no sense forking.

		  WARNING: if we ever add anything to the job prep code
		  (aboutToSpawnJobHandler()) that doesn't require root/admin
		  privledges, we need to change this!
		*/
	if( ! can_switch_ids() ) {
		return false;
	}

		// then, see if the job has a sandbox at all (either
		// explicitly or b/c of ON_EXIT_OR_EVICT)  

	ClassAd * job_ad = GetJobAd( cluster, proc );
	if( ! job_ad ) {
			// job is already gone, guess we don't need a thread. ;)
		return false;
	}
	if( ! jobIsSandboxed(job_ad) ) {
		FreeJobAd( job_ad );
		return false;
	}
	bool rval = false;
	if( ! sandboxHasRightOwner(cluster, proc, job_ad) ) {
		rval = true;
	}
	FreeJobAd( job_ad );
	return rval;
}


bool
jobCleanupNeedsThread( int cluster, int proc )
{

#ifdef WIN32
	// we never want to run this in a thread on Win32, 
	// since much of what we do in here is NOT thread safe.
	return false;
#endif
		/*
		  make sure we can switch uids.  if not, we're not going to be
		  able to chown() the sandbox, so there's no sense forking.

		  WARNING: if we ever add anything to the job cleanup code
		  (jobIsFinished()) that doesn't require root/admin
		  privledges, we need to change this!
		*/
	if( ! can_switch_ids() ) {
		return false;
	}

		// the cleanup case is different from the start-up case, since
		// we don't want to test the "HasRightOwner" stuff at all.  we
		// always want the cleanup code to chown() back to condor,
		// either so the schedd can read the files out for a
		// condor_transfer_data, or so that it can successfully blow
		// away the spool directories as PRIV_CONDOR.
	ClassAd * job_ad = GetJobAd( cluster, proc );
	if( ! job_ad ) {
			// job is already gone, guess we don't need a thread. ;)
		return false;
	}
	bool rval = false;
	if( jobIsSandboxed(job_ad) ) {
		rval = true;
	}
	FreeJobAd(job_ad);
	return rval;
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
	ad->LookupInteger( ATTR_STAGE_IN_START, stage_in_start );
	if( stage_in_start > 0 ) {
		return true;
	}

	int univ = CONDOR_UNIVERSE_VANILLA;
	ad->LookupInteger( ATTR_JOB_UNIVERSE, univ );
	switch( univ ) {
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_STANDARD:
	case CONDOR_UNIVERSE_PVM:
		return false;
		break;

	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
		return true;
		break;

	default:
		dprintf( D_ALWAYS,
				 "ERROR: unknown universe (%d) in jobIsSandboxed()\n", univ );
		break;
	}
	return true;
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

// #    error "directory chowning on Win32.  Do we need it?"

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

	case CONDOR_UNIVERSE_GLOBUS:
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

	if( jobIsSandboxed(job_ad) ) {
		MyString sandbox;
		if( getSandbox(cluster, proc, sandbox) ) {
			uid_t src_uid = 0;
			uid_t dst_uid = get_condor_uid();
			gid_t dst_gid = get_condor_gid();

			MyString owner;
			job_ad->LookupString( ATTR_OWNER, owner );

			passwd_cache* p_cache = pcache();
			if( p_cache->get_user_uid( owner.Value(), src_uid ) ) {
				if( ! recursive_chown(sandbox.Value(), src_uid,
									  dst_uid, dst_gid, true) )
				{
					dprintf( D_ALWAYS, "(%d.%d) Failed to chown %s from "
							 "%d to %d.%d.  User may run into permissions "
							 "problems when fetching sandbox.\n", 
							 cluster, proc, sandbox.Value(),
							 src_uid, dst_uid, dst_gid );
				}
			} else {
				dprintf( D_ALWAYS, "(%d.%d) Failed to find UID and GID "
						 "for user %s.  Cannot chown \"%s\".  User may "
						 "run into permissions problems when fetching "
						 "job sandbox.\n", cluster, proc, owner.Value(),
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
					 (int)time(NULL) );
	return DestroyProc( cluster, proc );
}


// Initialize a UserLog object for a given job and return a pointer to
// the UserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a UserLog, so you must check for NULL before
// using the pointer you get back.
UserLog*
Scheduler::InitializeUserLog( PROC_ID job_id ) 
{
	char tmp[_POSIX_PATH_MAX];
	
	if( GetAttributeString(job_id.cluster, job_id.proc, ATTR_ULOG_FILE,
							tmp) < 0 ) {
			// if there is no userlog file defined, then our work is
			// done...  
		return NULL;
	}
	
	char owner[_POSIX_PATH_MAX];
	char domain[_POSIX_PATH_MAX];
	char logfilename[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
	int use_xml;

	GetAttributeString(job_id.cluster, job_id.proc, ATTR_JOB_IWD, iwd);
	if ( tmp[0] == '/' || tmp[0]=='\\' || (tmp[1]==':' && tmp[2]=='\\') ) {
		strcpy(logfilename, tmp);
	} else {
		sprintf(logfilename, "%s/%s", iwd, tmp);
	}

	owner[0] = '\0';
	domain[0] = '\0';
	GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);
	GetAttributeString(job_id.cluster, job_id.proc, ATTR_NT_DOMAIN, domain);

	dprintf( D_FULLDEBUG, 
			 "Writing record to user logfile=%s owner=%s\n",
			 logfilename, owner );

	UserLog* ULog=new UserLog();
	if (0 <= GetAttributeBool(job_id.cluster, job_id.proc,
							  ATTR_ULOG_USE_XML, &use_xml)
		&& 1 == use_xml) {
		ULog->setUseXML(true);
	} else {
		ULog->setUseXML(false);
	}
	if (ULog->initialize(owner, domain, logfilename, job_id.cluster, job_id.proc, 0)) {
		return ULog;
	} else {
		dprintf ( D_ALWAYS,
				"WARNING: Invalid user log file specified: %s\n", logfilename);
		delete ULog;
		return NULL;
	}
}


bool
Scheduler::WriteAbortToUserLog( PROC_ID job_id )
{
	UserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobAbortedEvent event;

	char* reason = NULL;
	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  ATTR_REMOVE_REASON, &reason) >= 0 ) {
		event.setReason( reason );
	}
		// GetAttributeStringNew always allocates memory, so we free
		// regardless of the return value.
	free( reason );

	int status = ULog->writeEvent(&event);
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_ABORTED event\n" );
		return false;
	}
	return true;
}


bool
Scheduler::WriteHoldToUserLog( PROC_ID job_id )
{
	UserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobHeldEvent event;

	char* reason = NULL;
	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  ATTR_HOLD_REASON, &reason) >= 0 ) {
		event.setReason( reason );
	}
		// GetAttributeStringNew always allocates memory, so we free
		// regardless of the return value.
	free( reason );

	int status = ULog->writeEvent(&event);
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_HELD event\n" );
		return false;
	}
	return true;
}


bool
Scheduler::WriteReleaseToUserLog( PROC_ID job_id )
{
	UserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobReleasedEvent event;

	char* reason = NULL;
	if( GetAttributeStringNew(job_id.cluster, job_id.proc,
							  ATTR_RELEASE_REASON, &reason) >= 0 ) {
		event.setReason( reason );
	}
		// GetAttributeStringNew always allocates memory, so we free
		// regardless of the return value.
	free( reason );

	int status = ULog->writeEvent(&event);
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RELEASED event\n" );
		return false;
	}
	return true;
}


bool
Scheduler::WriteExecuteToUserLog( PROC_ID job_id, const char* sinful )
{
	UserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}

	char* host;
	if( sinful ) {
		host = (char*)sinful;
	} else {
		host = MySockName;
	}

	ExecuteEvent event;
	strcpy( event.executeHost, host );
	int status = ULog->writeEvent(&event);
	delete ULog;
	
	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event\n" );
		return false;
	}
	return true;
}


bool
Scheduler::WriteEvictToUserLog( PROC_ID job_id, bool checkpointed ) 
{
	UserLog* ULog = this->InitializeUserLog( job_id );
	if( ! ULog ) {
			// User didn't want log
		return true;
	}
	JobEvictedEvent event;
	event.checkpointed = checkpointed;
	int status = ULog->writeEvent(&event);
	delete ULog;
	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n" );
		return false;
	}
	return true;
}


bool
Scheduler::WriteTerminateToUserLog( PROC_ID job_id, int status ) 
{
	UserLog* ULog = this->InitializeUserLog( job_id );
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
	int rval = ULog->writeEvent(&event);
	delete ULog;

	if (!rval) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_TERMINATED event\n" );
		return false;
	}
	return true;
}

bool
Scheduler::WriteRequeueToUserLog( PROC_ID job_id, int status, const char * reason ) 
{
	UserLog* ULog = this->InitializeUserLog( job_id );
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
	int rval = ULog->writeEvent(&event);
	delete ULog;

	if (!rval) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED (requeue) event\n" );
		return false;
	}
	return true;
}


int
Scheduler::abort_job(int, Stream* s)
{
	PROC_ID	job_id;
	int nToRemove;

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
		ClassAd *ad;
		static bool already_removing = false;	// must be static!!!
		unsigned int count=1;
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

		sprintf(constraint,"%s == %d || %s == %d",ATTR_JOB_STATUS,REMOVED,
				ATTR_JOB_STATUS,HELD);

		ad = GetNextJobByConstraint(constraint,1);
		if ( ad ) {
			already_removing = true;
		}
		while ( ad ) {
			if ( (ad->LookupInteger(ATTR_CLUSTER_ID,job_id.cluster) == 1) &&
				 (ad->LookupInteger(ATTR_PROC_ID,job_id.proc) == 1) ) {

				 abort_job_myself(job_id, JA_REMOVE_JOBS, false, true );

			}
			FreeJobAd(ad);
			ad = NULL;

			// users typically do a condor_q immediately after a condor_rm.
			// if they condor_rm a lot of jobs, it could take a really long
			// time since we need to twiddle the job_queue.log, move classads
			// to the history file, and maybe write to the user log.  So, we
			// service the command socket here so condor_q does not timeout.
			// However, the command we service may start a new iteration
			// thru the queue.  Thus if we serviced any commands we must
			// start our iteration over from the beginning to make certain
			// our iterator is not corrupt. -Todd <tannenba@cs.wisc.edu>
			if ( (++count % 10 == 0) && daemonCore->ServiceCommandSocket() ) {
				// we just handled a command, restart the iteration
				ad = GetNextJobByConstraint(constraint,1);
			} else {
				// no command handled, iterator still safe - get next ad
				ad = GetNextJobByConstraint(constraint,0);
			}
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
	const char *AttrsToModify[] = { 
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY,
		NULL };		// list must end with a NULL


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


	int i,cluster,proc,index;
	char new_attr_value[500];
	char *buf = NULL;
	char *SpoolSpace = NULL;
		// figure out how many jobs we're dealing with
	int len = (*jobs).getlast() + 1;


		// For each job, modify its ClassAd
	for (i=0; i < len; i++) {
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;

		ClassAd *ad = GetJobAd(cluster,proc);
		if (!ad) {
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
		ad->LookupString(ATTR_JOB_IWD,&buf);
		if ( buf ) {
			sprintf(new_attr_value,"SUBMIT_%s",ATTR_JOB_IWD);
			SetAttributeString(cluster,proc,new_attr_value,buf);
			free(buf);
			buf = NULL;
		}
			// Modify the IWD to point to the spool space			
		SetAttributeString(cluster,proc,ATTR_JOB_IWD,SpoolSpace);

			// Now, for all the attributes listed in 
			// AttrsToModify, change them to be relative to new IWD
			// by taking the basename of all file paths.
		index = -1;
		while ( AttrsToModify[++index] ) {
				// Lookup original value
			if (buf) free(buf);
			buf = NULL;
			ad->LookupString(AttrsToModify[index],&buf);
			if (!buf) {
				// attribute not found, so no need to modify it
				continue;
			}
			if ( nullFile(buf) ) {
				// null file -- no need to modify it
				continue;
			}
				// Create new value - deal with the fact that
				// some of these attributes contain a list of pathnames
			StringList old_paths(buf,",");
			StringList new_paths(NULL,",");
			old_paths.rewind();
			char *old_path_buf;
			bool changed = false;
			const char *base = NULL;
			char new_path_buf[_POSIX_PATH_MAX];
			while ( (old_path_buf=old_paths.next()) ) {
				base = condor_basename(old_path_buf);
				if ( strcmp(base,old_path_buf)!=0 ) {
					snprintf(new_path_buf,_POSIX_PATH_MAX,
						"%s%c%s",SpoolSpace,DIR_DELIM_CHAR,base);
					base = new_path_buf;
					changed = true;
				}
				new_paths.append(base);
			}
			if ( changed ) {
					// Backup original value
				sprintf(new_attr_value,"SUBMIT_%s",AttrsToModify[index]);
				SetAttributeString(cluster,proc,new_attr_value,buf);
					// Store new value
				char *new_value = new_paths.print_to_string();
				ASSERT(new_value);
				SetAttributeString(cluster,proc,AttrsToModify[index],new_value);
				free(new_value);
			}
		}

			// Set ATTR_STAGE_IN_FINISH if not already set.
		int spool_completion_time = 0;
		ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
		if ( !spool_completion_time ) {
			// Note: we used to subtract one from the time here, because
			// we were worried about making certain we'd transfer back 
			// output files that changed even of the job ran for less than
			// one second.  The problem is we were also transferring back
			// input files.  Doo!! So now instead, we sleep for one second in
			// the forked child to solve these issues.
			SetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,now);
		}

			// And now release the job.
		releaseJob(cluster,proc,"Data files spooled",false,false,false,false);
		CommitTransaction();
	}

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
		// than one second.
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
		rsock->encode();
		if ( !rsock->code(JobAdsArrayLen) || !rsock->eom() ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to send JobAdsArrayLen (%d) \n",
					 JobAdsArrayLen );
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
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

			// Create a file transfer object, with schedd as the server
		result = ftrans.SimpleInit(ad, true, true, rsock);
		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
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

			if(universe == CONDOR_UNIVERSE_GLOBUS) {
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

	if( ! rsock->isAuthenticated() ) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! rsock->authenticate(methods.Value(), &errstack) ) {
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

	if ( mode == SPOOL_JOB_FILES_WITH_PERMS || mode == TRANSFER_DATA_WITH_PERMS ) {
		peer_version = NULL;
		if ( !rsock->code(peer_version) ) {
			dprintf( D_ALWAYS,
					 "spoolJobFiles(): failed to read peer_version\n" );
			refuse(s);
			return FALSE;
		}
			// At this point, we are responsible for deallocating
			// peer_version with free()
	}

	if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
			// read the number of jobs involved
//		if ( !rsock->code(JobAdsArrayLen) || JobAdsArrayLen <= 0 ) {
		if ( !rsock->code(JobAdsArrayLen) ) {
				dprintf( D_ALWAYS, "spoolJobFiles(): "
						 "failed to read JobAdsArrayLen (%d)\n",JobAdsArrayLen );
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
		dprintf(D_FULLDEBUG,"spoolJobFiles(): read JobAdsArrayLen - %d\n",JobAdsArrayLen);

		if (JobAdsArrayLen <= 0) {
			refuse(s);
			return FALSE;
		}
	}

	if ( mode == TRANSFER_DATA || mode == TRANSFER_DATA_WITH_PERMS ) {
			// read constraint string
		if ( !rsock->code(constraint_string) || constraint_string == NULL ) {
				dprintf( D_ALWAYS, "spoolJobFiles(): "
						 "failed to read constraint string\n" );
				refuse(s);
				return FALSE;
		}
	}

	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	setQSock(rsock);	// so OwnerCheck() will work

	time_t now = time(NULL);

	if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
		for (i=0; i<JobAdsArrayLen; i++) {
			rsock->code(a_job);
				// Only add jobs to our list if the caller has permission to do so;
				// cuz only the owner of a job (or queue super user) is allowed
				// to transfer data to/from a job.
			if (OwnerCheck(a_job.cluster,a_job.proc)) {
				(*jobs)[i] = a_job;
				SetAttributeInt(a_job.cluster,a_job.proc,ATTR_STAGE_IN_START,now);
			}
		}
	}

	if ( mode == TRANSFER_DATA || mode == TRANSFER_DATA_WITH_PERMS ) {
		JobAdsArrayLen = 0;
		ClassAd * tmp_ad = GetNextJobByConstraint(constraint_string,1);
		while (tmp_ad) {
			if ( tmp_ad->LookupInteger(ATTR_CLUSTER_ID,a_job.cluster) &&
				 tmp_ad->LookupInteger(ATTR_PROC_ID,a_job.proc) &&
				 OwnerCheck(a_job.cluster, a_job.proc) )
			{
				(*jobs)[JobAdsArrayLen++] = a_job;
			}
			tmp_ad = GetNextJobByConstraint(constraint_string,0);
		}
		if (constraint_string) free(constraint_string);
			// Now set ATTR_STAGE_OUT_START
		for (i=0; i<JobAdsArrayLen; i++) {
				// TODO --- maybe put this in a transaction?
			SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,ATTR_STAGE_OUT_START,now);
		}

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

	if ( mode == SPOOL_JOB_FILES || mode == SPOOL_JOB_FILES_WITH_PERMS ) {
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
	}

	if ( mode == TRANSFER_DATA || mode == TRANSFER_DATA_WITH_PERMS ) {
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
Scheduler::updateGSICred(int, Stream* s)
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

	if( ! rsock->isAuthenticated() ) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! rsock->authenticate(methods.Value(), &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD", SCHEDD_ERR_UPDATE_GSI_CRED_FAILED,
					"Failure to update GSI cred - Authentication failed" );
			dprintf( D_ALWAYS, "updateGSICred() aborting: %s\n",
					 errstack.getFullText() );
			refuse( s );
			return FALSE;
		}
	}	


		// read the job id from the client
	rsock->decode();
	if ( !rsock->code(jobid) || !rsock->eom() ) {
			dprintf( D_ALWAYS, "updateGSICred(): "
					 "failed to read job id\n" );
			refuse(s);
			return FALSE;
	}
	dprintf(D_FULLDEBUG,"updateGSICred(): read job id %d.%d\n",
		jobid.cluster,jobid.proc);
	jobad = GetJobAd(jobid.cluster,jobid.proc);
	if ( !jobad ) {
		dprintf( D_ALWAYS, "updateGSICred(): failed, "
				 "job %d.%d not found\n", jobid.cluster, jobid.proc );
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
		dprintf( D_ALWAYS, "updateGSICred(): failed, "
				 "user %s not authorized to edit job %d.%d\n", 
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
		dprintf( D_ALWAYS, "updateGSICred(): failed, "
			 "job %d.%d does not contain a gsi credential in SPOOL\n", 
			 jobid.cluster, jobid.proc );
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

		// Decode the proxy off the wire, and store into the
		// file temp_proxy_path, which is known to be in the SPOOL dir
	rsock->decode();
	filesize_t size = 0;
	if ( rsock->get_file(&size,temp_proxy_path.Value()) < 0 ) {
			// transfer failed
		reply = 0;	// reply of 0 means failure
	} else {
			// transfer worked, now rename the file to final_proxy_path
		if ( rotate_file(temp_proxy_path.Value(),
						 final_proxy_path.Value()) < 0 ) 
		{
				// the rename failed!!?!?!
			dprintf( D_ALWAYS, "updateGSICred(): failed, "
				 "job %d.%d  - could not rename file\n",
				 jobid.cluster,jobid.proc);
			reply = 0;
		} else {
			reply = 1;	// reply of 1 means success
		}
	}

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
	char status_str[16];
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
	if( ! rsock->isAuthenticated() ) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! rsock->authenticate(methods.Value(), &errstack) ) {
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
		sprintf( status_str, "%d", HELD );
		reason_attr_name = ATTR_HOLD_REASON;
		break;
	case JA_RELEASE_JOBS:
		sprintf( status_str, "%d", IDLE );
		reason_attr_name = ATTR_RELEASE_REASON;
		break;
	case JA_REMOVE_JOBS:
		sprintf( status_str, "%d", REMOVED );
		reason_attr_name = ATTR_REMOVE_REASON;
		break;
	case JA_REMOVE_X_JOBS:
		sprintf( status_str, "%d", REMOVED );
		reason_attr_name = ATTR_REMOVE_REASON;
		break;
	case JA_VACATE_JOBS:
	case JA_VACATE_FAST_JOBS:
			// no status_str needed.  also, we're not touching
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
			return false;
		}
		rhs->PrintToNewStr( &tmp );

			// we want to tack on another clause to make sure we're
			// not doing something invalid
		switch( action ) {
		case JA_REMOVE_JOBS:
				// Don't remove removed jobs
			sprintf( buf, "(%s!=%d) && (", ATTR_JOB_STATUS, REMOVED );
			break;
		case JA_REMOVE_X_JOBS:
				// only allow forced removal of previously "removed" jobs
				// including jobs on hold that will go to the removed state
				// upon release.
			sprintf( buf, "((%s==%d) || (%s==%d && %s=?=%d)) && (", 
				ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, HELD,
				ATTR_JOB_STATUS_ON_RELEASE,REMOVED);
			break;
		case JA_HOLD_JOBS:
				// Don't hold held jobs
			sprintf( buf, "(%s!=%d) && (", ATTR_JOB_STATUS, HELD );
			break;
		case JA_RELEASE_JOBS:
				// Only release held jobs
			sprintf( buf, "(%s==%d) && (", ATTR_JOB_STATUS, HELD );
			break;
		case JA_VACATE_JOBS:
		case JA_VACATE_FAST_JOBS:
				// Only vacate running jobs
			sprintf( buf, "(%s==%d) && (", ATTR_JOB_STATUS, RUNNING );
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
		sprintf( constraint, "%s%s)", buf, tmp );
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
	char time_str[32];
	sprintf( time_str, "%d", now );

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

		ClassAd* ad;
		ad = GetNextJobByConstraint( constraint, 1 );
		while( ad ) {
			if(	ad->LookupInteger(ATTR_CLUSTER_ID,tmp_id.cluster) &&
				ad->LookupInteger(ATTR_PROC_ID,tmp_id.proc) ) 
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
					if( !OwnerCheck(ad, rsock->getOwner()) ) {
						results.record( tmp_id, AR_PERMISSION_DENIED );
						ad = GetNextJobByConstraint( constraint, 0 );
						continue;
					}
					results.record( tmp_id, AR_SUCCESS );
					jobs[num_matches] = tmp_id;
					num_matches++;
					ad = GetNextJobByConstraint( constraint, 0 );
					continue;
				}

				if( action == JA_RELEASE_JOBS ) {
					int on_release_status = IDLE;
					GetAttributeInt(tmp_id.cluster, tmp_id.proc, 
							ATTR_JOB_STATUS_ON_RELEASE, &on_release_status);
					sprintf( status_str, "%d", on_release_status );
				}
				if( action == JA_REMOVE_X_JOBS ) {
					if( SetAttribute( tmp_id.cluster, tmp_id.proc,
									  ATTR_JOB_LEAVE_IN_QUEUE, "False" ) < 0 ) {
						results.record( tmp_id, AR_PERMISSION_DENIED );
						continue;
					}
				}
				if( SetAttribute(tmp_id.cluster, tmp_id.proc,
								 ATTR_JOB_STATUS, status_str) < 0 ) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					ad = GetNextJobByConstraint( constraint, 0 );
					continue;
				}
				if( reason ) {
					if( SetAttribute(tmp_id.cluster, tmp_id.proc,
									 reason_attr_name, reason) < 0 ) {
							// TODO: record failure in response ad?
					}
				}
				SetAttribute( tmp_id.cluster, tmp_id.proc,
							  ATTR_ENTERED_CURRENT_STATUS, time_str );
				fixReasonAttrs( tmp_id, action );
				results.record( tmp_id, AR_SUCCESS );
				jobs[num_matches] = tmp_id;
				num_matches++;
			} 
			ad = GetNextJobByConstraint( constraint, 0 );
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
				sprintf( status_str, "%d", on_release_status );
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
				ad = GetJobAd( tmp_id.cluster, tmp_id.proc, false );
				if( ! ad ) {
					EXCEPT( "impossible: GetJobAd(%d.%d) returned false "
							"yet GetAttributeInt(%s) returned success",
							tmp_id.cluster, tmp_id.proc, ATTR_JOB_STATUS );
				}
				if( !OwnerCheck(ad, rsock->getOwner()) ) {
					results.record( tmp_id, AR_PERMISSION_DENIED );
					continue;
				}
				results.record( tmp_id, AR_SUCCESS );
				jobs[num_matches] = tmp_id;
				num_matches++;
				continue;
			}
			if( SetAttribute(tmp_id.cluster, tmp_id.proc,
							 ATTR_JOB_STATUS, status_str) < 0 ) {
				results.record( tmp_id, AR_PERMISSION_DENIED );
				continue;
			}
			if( reason ) {
				SetAttribute( tmp_id.cluster, tmp_id.proc,
							  reason_attr_name, reason );
					// TODO: deal w/ failure here, too?
			}
			SetAttribute( tmp_id.cluster, tmp_id.proc,
						  ATTR_ENTERED_CURRENT_STATUS, time_str );
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
	sprintf( buf, "%s = %d", ATTR_JOB_ACTION, action );
	response_ad->Insert( buf );

		// Set a single attribute which says if the action succeeded
		// on at least one job or if it was a total failure
	sprintf( buf, "%s = %d", ATTR_ACTION_RESULT, num_matches ? 1:0 );
	response_ad->Insert( buf );

		// Finally, let them know if the user running this command is
		// a queue super user here
	sprintf( buf, "%s = %s", ATTR_IS_QUEUE_SUPER_USER,
			 isQueueSuperUser(rsock->getOwner()) ? "True" : "False" );
	response_ad->Insert( buf );
	
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

	if( needs_transaction ) {
		CommitTransaction();
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
	switch( action ) {
	case JA_HOLD_JOBS:
	case JA_REMOVE_JOBS:
	case JA_VACATE_JOBS:
	case JA_VACATE_FAST_JOBS:
		 for( i=0; i<num_matches; i++ ) {
 			if( i % 10 == 0 ) {
 				daemonCore->ServiceCommandSocket();
 			}
 			abort_job_myself( jobs[i], action, true, notify );
		}

		break;

	case JA_RELEASE_JOBS:
		for( i=0; i<num_matches; i++ ) {
			WriteReleaseToUserLog( jobs[i] );		
		}
		break;

	case JA_REMOVE_X_JOBS:
		for( i=0; i<num_matches; i++ ) {		
			if( !scheduler.WriteAbortToUserLog( jobs[i] ) ) {
				dprintf( D_ALWAYS, 
						 "Failed to write abort event to the user log\n" ); 
			}
			DestroyProc( jobs[i].cluster, jobs[i].proc );
		}
		break;

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
	int command;
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

	// since this is the socket from the negotiator, the only command that can
	// come in at this point is NEGOTIATE.  If we get something else, something
	// goofy in going on.
	if (command != NEGOTIATE)
	{
		dprintf(D_ALWAYS,
				"Negotiator command was %d (not NEGOTIATE) --- aborting\n", command);
		return (!(KEEP_STREAM));
	}

	rval = negotiate(NEGOTIATE, stream);
	return rval;	
}

int
Scheduler::delayedNegotiatorHandler(Stream *stream)
{
	int rval;

	rval = negotiate(NEGOTIATE, stream);
	return rval;
}

int
Scheduler::doNegotiate (int i, Stream *s)
{
	if ( daemonCore->InServiceCommandSocket() == TRUE ) {
		// We are currently in the middle of a negotiate with a
		// different negotiator.  Stash this request to handle it later.
		dprintf(D_FULLDEBUG,"Received Negotiate command while negotiating; stashing for later\n");
		daemonCore->Register_Socket(s,"<Another-Negotiator-Socket>",
			(SocketHandlercpp)&Scheduler::delayedNegotiatorHandler,
			"delayedNegotiatorHandler()", this, ALLOW);
		return KEEP_STREAM;
	}

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
Scheduler::negotiate(int, Stream* s)
{
	int		i;
	int		op;
	PROC_ID	id;
	char*	claim_id = NULL;			// claim_id for each match made
	char*	host = NULL;
	char*	sinful = NULL;
	char*	tmp;
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
	int		serviced_other_commands = 0;	
	int		owner_num;
	int		JobsRejected = 0;
	Sock*	sock = (Sock*)s;
	ContactStartdArgs * args;
	match_rec *mrec;
	ClassAd* my_match_ad;
	ClassAd* ad;
	char buffer[1024];


	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

	// Set timeout on socket
	s->timeout( param_integer("NEGOTIATOR_TIMEOUT",20) );

	// BIOTECH
	bool	biotech = false;
	char *bio_param = param("BIOTECH");
	if (bio_param) {
		free(bio_param);
		biotech = true;
	}
		

	if (FlockNegotiators) {
		// first, check if this is our local negotiator
		struct in_addr endpoint_addr = (sock->endpoint())->sin_addr;
		struct hostent *hent;
		bool match = false;
		Daemon negotiator (DT_NEGOTIATOR);
		char *negotiator_hostname = negotiator.fullHostname();
		if (!negotiator_hostname) {
			dprintf(D_ALWAYS, "Negotiator hostname lookup failed!");
			return (!(KEEP_STREAM));
		}
		hent = gethostbyname(negotiator_hostname);
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
				hent = gethostbyname(neg_host->fullHostname());
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
					"Aborting negotiation.\n", sin_to_string(sock->endpoint()));
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
	CurNumActiveShadows = numShadows + RunnableJobQueue.Length();

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
	s->decode();
	if (!s->code(ownerptr)) {
		dprintf( D_ALWAYS, "Can't receive request from manager\n" );
		return (!(KEEP_STREAM));
	}
	if (!s->end_of_message()) {
		dprintf( D_ALWAYS, "Can't receive request from manager\n" );
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
		return dedicated_scheduler.negotiate( s, negotiator_name );
	}

		// If we got this far, we're negotiating for a regular user,
		// so go ahead and do our expensive setup operations.

	autocluster.mark();
	N_PrioRecs = 0;

	WalkJobQueue( (int(*)(ClassAd *))job_prio );

	if( !shadow_prio_recs_consistent() ) {
		mail_problem_message();
	}

		// N_PrioRecs might be 0, if we have no jobs to run at the
		// moment.  If so, we don't want to call qsort(), since that's
		// bad.  We can still try to find the owner in the Owners
		// array, since that's not that expensive, and we need it for
		// all the flocking logic at the end of this function.
		// Discovered by Derek Wright and insure-- on 2/28/01
	if( N_PrioRecs ) {
		qsort( (char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]),
			   (int(*)(const void*, const void*))prio_compar );
	}
	jobs = N_PrioRecs;
	autocluster.sweep();

	N_RejectedClusters = 0;
	JobsStarted = 0;

	// find owner in the Owners array
	char *at_sign = strchr(owner, '@');
	if (at_sign) *at_sign = '\0';
	for (owner_num = 0;
		 owner_num < N_Owners && strcmp(Owners[owner_num].Name, owner);
		 owner_num++);
	if (owner_num == N_Owners) {
		dprintf(D_ALWAYS, "Can't find owner %s in Owners array!\n", owner);
		jobs = N_PrioRecs = 0;
	} else if (Owners[owner_num].FlockLevel < which_negotiator) {
		dprintf(D_FULLDEBUG,
				"This user is no longer flocking with this negotiator.\n");
		jobs = N_PrioRecs = 0;
	} else if (Owners[owner_num].FlockLevel == which_negotiator) {
		Owners[owner_num].NegotiationTimestamp = time(0);
	}
	if (at_sign) *at_sign = '@';

	/* Try jobs in priority order */
	for( i=0; i < N_PrioRecs; i++ ) {
	
		// BIOTECH
		if ( JobsRejected > 0 && biotech ) {
			continue;
		}

		char tmpstr[200];
		sprintf(tmpstr,"%s@%s",PrioRec[i].owner,UidDomain);
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

		serviced_other_commands += daemonCore->ServiceCommandSocket();

		id = PrioRec[i].id;
		if( cluster_rejected(PrioRec[i].auto_cluster_id) ) {
			continue;
		}

		if ( serviced_other_commands ) {
			// we have run some other schedd command, like condor_rm or condor_q,
			// while negotiating.  check and make certain the job is still
			// runnable, since things may have changed since we built the 
			// prio_rec array (like, perhaps condor_hold or condor_rm was done).
			if ( Runnable(&id) == FALSE ) {
				continue;
			}
		}


		ad = GetJobAd( id.cluster, id.proc );
		if (!ad) {
			dprintf(D_ALWAYS,"Can't get job ad %d.%d\n",
					id.cluster, id.proc );
			continue;
		}	

		cur_hosts = 0;
		ad->LookupInteger(ATTR_CURRENT_HOSTS,cur_hosts);
		max_hosts = 1;
		ad->LookupInteger(ATTR_MAX_HOSTS,max_hosts);
		job_universe = 0;
		ad->LookupInteger(ATTR_JOB_UNIVERSE,job_universe);

		for (host_cnt = cur_hosts; host_cnt < max_hosts;) {

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
					 sprintf(buffer,"%s = \"%s\"",
						 ATTR_LAST_REJ_MATCH_REASON, diagnostic_message);
					 ad->Insert(buffer);
					 free(diagnostic_message);
				 }
					 // don't break: fall through to REJECTED case
				 case REJECTED:
						// Always negotiate for all PVM job classes! 
					if ( job_universe != CONDOR_UNIVERSE_PVM && !NegotiateAllJobsInCluster ) {
						mark_cluster_rejected( cur_cluster );
					}
					host_cnt = max_hosts + 1;
					JobsRejected++;
					sprintf(buffer,"%s = %d",
									ATTR_LAST_REJ_MATCH_TIME,(int)time(0));
					ad->Insert(buffer);
					break;
				case SEND_JOB_INFO: {
						// The Negotiator wants us to send it a job. 
						// First, make sure we could start another
						// shadow without violating some limit.
					if ( service_this_universe( job_universe,ad ) ) {
						if( ! canSpawnShadow(JobsStarted, jobs) ) {
								// We can't start another shadow.  Tell
								// the negotiator we're done.
							if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
									// We failed to talk to the CM, so
									// close the connection.
								dprintf( D_ALWAYS, 
										 "Can't send NO_MORE_JOBS to mgr\n" ); 
								return( !(KEEP_STREAM) );
							} else {
									// Communication worked, keep the
									// connection stashed for later.
								return KEEP_STREAM;
							}
						}
					}	// end of if service_this_universe()

						// If we got this far, we can spawn another
						// shadow, so keep going w/ our regular work. 

					/* Send a job description */
					s->encode();
					if( !s->put(JOB_INFO) ) {
						dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
						return (!(KEEP_STREAM));
					}

					// Figure out if this request would result in another 
					// shadow process if matched.  If non-PVM, the answer
					// is always yes.  If PVM, perhaps yes or no.  If
					// Globus, then no.
					shadow_num_increment = 1;
					if(job_universe == CONDOR_UNIVERSE_GLOBUS) {
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

					// request match diagnostics
					sprintf (buffer, "%s = True", ATTR_WANT_MATCH_DIAGNOSTICS);
					ad->Insert (buffer);

					// Send the ad to the negotiator
					if( !ad->put(*s) ) {
						dprintf( D_ALWAYS,
								"Can't send job ad to mgr\n" );
						// FreeJobAd(ad);
						s->end_of_message();
						return (!(KEEP_STREAM));
					}
					// FreeJobAd(ad);
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
				case PERMISSION_AND_AD:
					/*
					 * If things are cool, contact the startd.
					 * But... of the claim_id is the string "null", that means
					 * the resource does not support the claiming protocol.
					 */
					dprintf ( D_FULLDEBUG, "In case PERMISSION\n" );

					sprintf(buffer,"%s = %d",
									ATTR_LAST_MATCH_TIME,(int)time(0));
					ad->Insert(buffer);

					if( !s->get(claim_id) ) {
						dprintf( D_ALWAYS,
								"Can't receive ClaimId from mgr\n" );
						return (!(KEEP_STREAM));
					}
					my_match_ad = NULL;
					if ( op == PERMISSION_AND_AD ) {
						// get startd ad from negotiator as well
						my_match_ad = new ClassAd();
						if( !my_match_ad->initFromStream(*s) ) {
							dprintf( D_ALWAYS,
								"Can't get my match ad from mgr\n" );
							delete my_match_ad;
							FREE( claim_id );
							return (!(KEEP_STREAM));
						}
					}
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't receive eom from mgr\n" );
						if (my_match_ad)
							delete my_match_ad;
						FREE( claim_id );
						return (!(KEEP_STREAM));
					}
						// claim_id is in the form
						// "<xxx.xxx.xxx.xxx:xxxx>#xxxxxxx" 
						// where everything upto the # is the sinful
						// string of the startd

					dprintf( D_PROTOCOL, 
							 "## 4. Received ClaimId %s\n", claim_id );

					if ( my_match_ad ) {
						dprintf(D_PROTOCOL,"Received match ad\n");

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
						int c = -1;
						int p = -1;
						int list_len = 0;
						ad->LookupInteger(ATTR_LAST_MATCH_LIST_LENGTH,list_len);
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
								sprintf(attr_buf,"%s%d",
									ATTR_LAST_MATCH_LIST_PREFIX,list_index);
								ad->LookupString(attr_buf,&last_match);
								if ( c == -1 ) {
									ad->LookupInteger(ATTR_CLUSTER_ID, c);
									ad->LookupInteger(ATTR_PROC_ID, p);
									ASSERT( c != -1 );
									ASSERT( p != -1 );
									BeginTransaction();
								}
								SetAttributeString(c,p,attr_buf,curr_match);
								free(curr_match);
							}
							if (last_match) free(last_match);
						}
							// Increment ATTR_NUM_MATCHES
						int num_matches = 0;
						ad->LookupInteger(ATTR_NUM_MATCHES,num_matches);
						num_matches++;
							// If a transaction is already open, may as well
							// use it to store ATTR_NUM_MATCHES.  If not,
							// don't bother --- just update in RAM.
						if ( c != -1 && p != -1 ) {
							SetAttributeInt(c,p,ATTR_NUM_MATCHES,num_matches);
							CommitTransaction();
						} else {
							char tbuf[200];
							sprintf(tbuf,"%s=%d",
										ATTR_NUM_MATCHES,num_matches);
							ad->Insert(tbuf);
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
						sprintf (buffer, "%s = True", ATTR_JOB_MATCHED);
						ad->Insert (buffer);
						sprintf (buffer, "%s = 1", ATTR_CURRENT_HOSTS);
						ad->Insert(buffer);
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

						// First pull out the sinful string from ClaimId,
						// so we know whom to contact to claim.
					sinful = strdup( claim_id );
					tmp = strchr( sinful, '#');
					if( tmp ) {
						*tmp = '\0';
					} else {
						dprintf( D_ALWAYS, "Can't find '#' in ClaimId!\n" );
							// What else should we do here?
						FREE( sinful );
						FREE( claim_id );
						sinful = NULL;
						claim_id = NULL;
						if( my_match_ad ) {
							delete my_match_ad;
							my_match_ad = NULL;
						}
						break;
					}
						// sinful should now point to the sinful string
						// of the startd we were matched with.

					mrec = AddMrec( claim_id, sinful, &id, my_match_ad, owner, negotiator_name );

					/* Here we don't want to call contactStartd directly
					   because we do not want to block the negotiator for 
					   this, and because we want to minimize the possibility
					   that the startd will have to block/wait for the 
					   negotiation cycle to finish before it can finish
					   the claim protocol.  So...we enqueue the
					   args for a later call.  (The later call will be
					   made from the startdContactSockHandler) */
					args = new ContactStartdArgs( claim_id, sinful, false );

						// Now that the info is stored in the above
						// object, we can deallocate all of our
						// strings and other memory.
					free( sinful );
					sinful = NULL;
					free( claim_id );
					claim_id = NULL;
					if( my_match_ad ) {
						delete my_match_ad;
						my_match_ad = NULL;
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
Scheduler::vacate_service(int, Stream *sock)
{
	char	*claim_id = NULL;

	dprintf( D_ALWAYS, "Got VACATE_SERVICE from %s\n", 
			 sin_to_string(((Sock*)sock)->endpoint()) );

	if (!sock->code(claim_id)) {
		dprintf (D_ALWAYS, "Failed to get ClaimId\n");
		return;
	}
	if( DelMrec(claim_id) < 0 ) {
			// We couldn't find this match in our table, perhaps it's
			// from a dedicated resource.
		dedicated_scheduler.DelMrec( claim_id );
	}
	FREE (claim_id);
	dprintf (D_PROTOCOL, "## 7(*)  Completed vacate_service\n");
	return;
}


void
Scheduler::contactStartd( ContactStartdArgs* args ) 
{
	if( args->isDedicated() ) {
			// If this was a match for the dedicated scheduler, let it
			// handle it from here.
		dedicated_scheduler.contactStartd( args );
		return;
	}

	dprintf( D_FULLDEBUG, "In Scheduler::contactStartd()\n" );

	HashKey mrec_key(args->claimId());
	match_rec *mrec = NULL;
	matches->lookup(mrec_key,mrec);

	if(!mrec) {
		// The match must have gotten deleted during the time this
		// operation was queued.
		dprintf( D_FULLDEBUG, "no match record found for %s", args->claimId() );
		return;
	}

    dprintf( D_FULLDEBUG, "%s %s %s %d.%d\n", mrec->id, 
			 mrec->user, mrec->peer, mrec->cluster,
			 mrec->proc ); 

	ClassAd *jobAd = GetJobAd( mrec->cluster, mrec->proc );
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "failed to find job %d.%d\n", mrec->cluster,
				 mrec->proc ); 
		DelMrec( mrec );
		return;
	}

	if( ! claimStartd(mrec, jobAd, false) ) {
		DelMrec( mrec );
		return;
	}
}


bool
claimStartd( match_rec* mrec, ClassAd* job_ad, bool is_dedicated )
{
	DCStartd matched_startd ( mrec->peer, NULL );
	Sock* sock = NULL;

	dprintf( D_PROTOCOL, "Requesting resource from %s ...\n",
			 mrec->peer ); 

	if (!(sock = matched_startd.reliSock( STARTD_CONTACT_TIMEOUT, NULL, true ))) {
		dprintf( D_FAILURE|D_ALWAYS, "Couldn't initiate connection to %s\n",
		         mrec->peer );
		return false;
	}

	mrec->setStatus( M_CONNECTING );

	char to_startd[256];
	sprintf ( to_startd, "to startd %s", mrec->peer );

	if( is_dedicated ) {
		daemonCore->
			Register_Socket( sock, "<Startd Contact Socket>",
			  (SocketHandlercpp)&DedicatedScheduler::startdContactConnectHandler,
			  to_startd, &dedicated_scheduler, ALLOW );
	} else {
		daemonCore->
			Register_Socket( sock, "<Startd Contact Socket>",
			  (SocketHandlercpp)&Scheduler::startdContactConnectHandler,
			  to_startd, &scheduler, ALLOW );
	}
	daemonCore->Register_DataPtr( strdup(mrec->id) );

	return true;
}


bool
claimStartdConnected( Sock *sock, match_rec* mrec, ClassAd* job_ad, bool is_dedicated )
{
	DCStartd matched_startd ( mrec->peer, NULL );

	if (!sock) {
		dprintf( D_FAILURE|D_ALWAYS, "NULL sock when connecting to startd %s\n",
				 mrec->peer );
		return false;
	}
	if (!sock->is_connected()) {
		dprintf( D_FAILURE|D_ALWAYS, "Failed to connect to startd %s\n",
				 mrec->peer );
		return false;
	}

	if (!matched_startd.startCommand(REQUEST_CLAIM, sock,
	                                 STARTD_CONTACT_TIMEOUT )) {
		dprintf( D_FAILURE|D_ALWAYS, "Couldn't send REQUEST_CLAIM to startd at %s\n",
				 mrec->peer );
		return false;
	}

	sock->encode();

	if( !sock->put( mrec->id ) ) {
		dprintf( D_ALWAYS, "Couldn't send ClaimId to startd.\n" );	
		return false;
	}

	if( !job_ad->put( *sock ) ) {
		dprintf( D_ALWAYS, "Couldn't send job classad to startd.\n" );	
		return false;
	}	

	char startd_version[150];
	startd_version[0] = '\0';
	if ( mrec->my_match_ad  &&
		 mrec->my_match_ad->LookupString(ATTR_VERSION,startd_version) ) 
	{	
		CondorVersionInfo ver(startd_version,"STARTD");

		if( ver.built_since_version(6,1,11) &&
			ver.built_since_date(1,28,2000) ) {
				// We are talking to a startd which understands sending the
				// post 6.1.11 change to the claim protocol.
			if( !sock->put( scheduler.dcSockSinful() ) ) {
				dprintf( D_ALWAYS, "Couldn't send schedd string to startd.\n" ); 
				return false;
			}
			if( !sock->snd_int(scheduler.aliveInterval(), FALSE) ) {
				dprintf(D_ALWAYS, "Couldn't send alive_interval to startd.\n");
				return false;
			}
			mrec->sent_alive_interval = true;
		}
	}

	if ( !mrec->sent_alive_interval ) {
		dprintf( D_FULLDEBUG, "Startd expects pre-v6.1.11 claim protocol\n" );
	}

	if( !sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Couldn't send eom to startd.\n" );	
		return false;
	}

	mrec->setStatus( M_STARTD_CONTACT_LIMBO );

	char to_startd[256];
	sprintf ( to_startd, "to startd %s", mrec->peer );

	daemonCore->Cancel_Socket( sock ); //Allow us to re-register this socket.
	if( is_dedicated ) {
		daemonCore->
			Register_Socket( sock, "<Startd Contact Socket>",
			  (SocketHandlercpp)&DedicatedScheduler::startdContactSockHandler,
			  to_startd, &dedicated_scheduler, ALLOW );
	} else {
		daemonCore->
			Register_Socket( sock, "<Startd Contact Socket>",
			  (SocketHandlercpp)&Scheduler::startdContactSockHandler,
			  to_startd, &scheduler, ALLOW );
	}
	daemonCore->Register_DataPtr( strdup(mrec->id) );

	scheduler.addRegContact();

	dprintf ( D_FULLDEBUG, "Registered startd contact socket.\n" );

	return true;
}

int
Scheduler::startdContactConnectHandler( Stream *sock )
{
	dprintf ( D_FULLDEBUG, "In Scheduler::startdContactConnectHandler\n" );

		// fetch the match record.  the daemon core DataPtr specifies
		// the id of the match (a.k.a. the ClaimId).  use this id to
		// pull out the actual mrec from our hashtable.
	char *id = (char *) daemonCore->GetDataPtr();
	match_rec *mrec = NULL;
	if ( id ) {
		HashKey key(id);
		matches->lookup(key, mrec);
		free(id); 	// it was allocated with strdup() when
					// Register_DataPtr was called   
	}

	if ( !mrec ) {
		// The match record must have been deleted.  Nothing left to do, close
		// up shop.
		dprintf ( D_FULLDEBUG, "startdContactConnectHandler(): mrec not found\n" );
		checkContactQueue();
		return FALSE;
	}

	dprintf ( D_FULLDEBUG, "Got mrec data pointer %p\n", mrec );

	ClassAd *jobAd = GetJobAd( mrec->cluster, mrec->proc );
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "failed to find job %d.%d\n", mrec->cluster,
				 mrec->proc ); 
		DelMrec( mrec );
		return FALSE;
	}

	if(!claimStartdConnected( (Sock *)sock, mrec, jobAd, false )) {
		DelMrec( mrec );
		return FALSE;
	}

	// The stream will be closed when we get a callback
	// in startdContactSockHandler.  Keep it open for now.
	return KEEP_STREAM;
}

/* note new BAILOUT def:
   before each bad return we check to see if there's a pending call
   in the contact queue. */
#define BAILOUT               \
		DelMrec( mrec );      \
		checkContactQueue();  \
		return FALSE;

int
Scheduler::startdContactSockHandler( Stream *sock )
{
		// all return values are non - KEEP_STREAM.  
		// Therefore, DaemonCore will cancel this socket at the
		// end of this function, which is exactly what we want!

	int reply;

	dprintf ( D_FULLDEBUG, "In Scheduler::startdContactSockHandler\n" );

		// since all possible returns from here result in the socket being
		// cancelled, we begin by decrementing the # of contacts.
	delRegContact();

		// fetch the match record.  the daemon core DataPtr specifies
		// the id of the match (a.k.a. the ClaimId).  use this id to
		// pull out the actual mrec from our hashtable.
	char *id = (char *) daemonCore->GetDataPtr();
	match_rec *mrec = NULL;
	if ( id ) {
		HashKey key(id);
		matches->lookup(key, mrec);
		free(id); 	// it was allocated with strdup() when
					// Register_DataPtr was called   
	}

	if ( !mrec ) {
		// The match record must have been deleted.  Nothing left to do, close
		// up shop.
		dprintf ( D_FULLDEBUG, "startdContactSockHandler(): mrec not found\n" );
		checkContactQueue();
		return FALSE;
	}
	
	dprintf ( D_FULLDEBUG, "Got mrec data pointer %p\n", mrec );

	mrec->setStatus( M_CLAIMED ); // we assume things will work out.

	// Now, we set the timeout on the socket to 1 second.  Since we 
	// were called by as a Register_Socket callback, this should not 
	// block if things are working as expected.  
	// However, if the Startd wigged out and sent a 
	// partial int or some such, we cannot afford to block. -Todd 3/2000
	sock->timeout(1);

 	if( !sock->rcv_int(reply, TRUE) ) {
		dprintf( D_ALWAYS, "Response problem from startd on %s (match %s).\n", mrec->peer, mrec->id );	
		BAILOUT;
	}

	if( reply == OK ) {
		dprintf (D_PROTOCOL, "(Request was accepted)\n");
		if ( !mrec->sent_alive_interval ) {
	 		sock->encode();
			if( !sock->code(MySockName) ) {
				dprintf( D_ALWAYS, "Couldn't send schedd string to startd.\n");
				BAILOUT;
			}
			if( !sock->snd_int(alive_interval, TRUE) ) {
				dprintf( D_ALWAYS, "Couldn't send aliveInterval to startd.\n");
				BAILOUT;
			}
		}
	} else if( reply == NOT_OK ) {
		dprintf( D_PROTOCOL, "(Request was NOT accepted)\n" );
		BAILOUT;
	} else {
		dprintf( D_ALWAYS, "Unknown reply from startd.\n");
		BAILOUT;
	}

	checkContactQueue();

		// we want to set a timer to go off in 2 seconds that will
		// do a StartJobs().  However, we don't want to set this
		// *every* time we get here.  We check startJobDelayBit, if
		// it is TRUE then we don't reset the timer because we were here
		// less than 2 seconds ago.  
	if ( startJobsDelayBit == FALSE ) {
		daemonCore->Reset_Timer(startjobsid, 2);
		startJobsDelayBit = TRUE;
        dprintf ( D_FULLDEBUG, "Timer set...\n" );
	}

	return TRUE;
}
#undef BAILOUT


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
	return true;
}


void
Scheduler::checkContactQueue() 
{
	ContactStartdArgs *args;

		// clear out the timer tid, since we made it here.
	checkContactQueue_tid = -1;

		// Contact startds as long as (a) there are still entries in our
		// queue, and (b) we have not exceeded MAX_STARTD_CONTACTS, which
		// ensures we do not run ourselves out of socket descriptors.
	while( (num_reg_contacts < MAX_STARTD_CONTACTS) &&
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
	ClassAd *ad = NULL, *scan = NULL;
	CondorQuery query(STARTD_AD);
	ClassAdList ads;
	MyString constraint;
	char* remote_host = NULL;
	char* remote_pool = NULL;

		// clear out the timer tid, since we made it here.
	checkReconnectQueue_tid = -1;

		// First, sweep through the job id's we care about, and
		// construct a single query that will grab all the ads 
	jobsToReconnect.Rewind();
	while( jobsToReconnect.Next(job) ) {
			// there's a pending registration in the queue:
		dprintf( D_FULLDEBUG, "In checkReconnectQueue(), job: %d.%d\n", 
				 job.cluster, job.proc );

			// First, see if this job was flocked to a remote pool,
			// and if so, blow up since we don't really handle this
			// level of complexity just yet... 
		if( GetAttributeStringNew(job.cluster, job.proc, ATTR_REMOTE_POOL,
								  &remote_pool) >= 0 ) {
			dprintf( D_ALWAYS, "Can't reconnect job %d.%d to a flocked pool"
					 " (%s) yet\n", job.cluster, job.proc, remote_pool );
			free( remote_pool );
			mark_job_stopped(&job);
			continue;
		}
		free( remote_pool );
		remote_pool = NULL;

		
			// Now, grab the name of the startd ad we need to query
			// for from the job ad.  it's living in ATTR_REMOTE_HOST
		GetAttributeStringNew( job.cluster, job.proc, ATTR_REMOTE_HOST,
							   &remote_host );
		constraint = ATTR_NAME;	
		constraint += "==\"";
		constraint += remote_host;
		constraint += '"';
		free( remote_host );
		remote_host = NULL;
		query.addORConstraint( constraint.Value() );
	}

	if (Collectors->query (query, ads) != Q_OK) {
		dprintf( D_ALWAYS, "ERROR: failed to query collectors\n");
			// TODO! deal violently with this failure. ;)
		jobsToReconnect.Rewind();
		while( jobsToReconnect.Next(job) ) {
			dprintf( D_ALWAYS,
					 "Can't find ClassAd for %d.%d, marking as idle\n",
					 job.cluster, job.proc );
			mark_job_stopped(&job);
		}
	}

	char* job_id = NULL;
	PROC_ID tmp_job;
	ads.Open();
	while( (scan = ads.Next()) ) {
			// make sure this ad is a startd that's running one of our
			// jobs. 
		scan->LookupString( ATTR_JOB_ID, &job_id );
		if( ! job_id ) {
			dprintf( D_ALWAYS, "ClassAd from query has no %s, ignoring\n", 
					 ATTR_JOB_ID );
			continue;
		}
		tmp_job = getProcByString( job_id );
		free( job_id );
		job_id = NULL;
		
			// Now, find it from our list, remove it, and create the
			// necessary records...
		jobsToReconnect.Rewind();
		while( jobsToReconnect.Next(job) ) {
			if( job.cluster == tmp_job.cluster && job.proc == tmp_job.proc ) {
				jobsToReconnect.DeleteCurrent();
					// Normally, we'd have to make a copy, since the ClassAdList
					// object on our stack will destroy everything.
					// However, makeReconnectRecords() will make its own copy
					// of the ad whenever it needs to do so.  
				ad = new ClassAd( *scan );
				makeReconnectRecords( &job, ad );
				break;
			}
		}
	}

		// now, make sure all our jobs are gone from the list.  if
		// not, "We have a problem, Houston..."
	if( ! jobsToReconnect.IsEmpty() ) {
		dprintf( D_ALWAYS, "ERROR: couldn't find ClassAds for some "
				 "disconnected jobs!\n" );
		jobsToReconnect.Rewind();
		while( jobsToReconnect.Next(job) ) {
			dprintf( D_ALWAYS, "Missing match ClassAd for job %d.%d\n",
					 job.cluster, job.proc );
				// TODO handle this error better!
			mark_job_stopped(&job);
		}
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

	// NOTE: match_ad could be deallocated when this function returns,
	// so if we need to keep it around, we must make our own copy of it.

	if( GetAttributeStringNew(cluster, proc, ATTR_OWNER, &owner) < 0 ) {
			// we've got big trouble, just give up.
		free( owner );
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				 ATTR_OWNER, cluster, proc );
		mark_job_stopped( job );
		return;
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_CLAIM_ID, 
							  &claim_id) < 0 ) {
		free( claim_id );
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				 ATTR_CLAIM_ID, cluster, proc );
		mark_job_stopped( job );
		free( owner );
		return;
	}
	if( GetAttributeStringNew(cluster, proc, ATTR_REMOTE_HOST, 
							  &startd_name) < 0 ) {
		free( startd_name );
		dprintf( D_ALWAYS, "WARNING: %s no longer in job queue for %d.%d\n", 
				 ATTR_REMOTE_HOST, cluster, proc );
		mark_job_stopped( job );
		free( claim_id );
		free( owner );
		return;
	}
	
	int universe;
	GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &universe );

	startd_addr = getAddrFromClaimId( claim_id );
	if( GetAttributeStringNew(cluster, proc, ATTR_REMOTE_POOL,
							  &pool) < 0 ) {
		free( pool );
		pool = NULL;
	}

	UserLog* ULog = this->InitializeUserLog( *job );
	if ( ULog ) {
		JobDisconnectedEvent event;
		const char* txt = "Local schedd and job shadow died, "
			"schedd now running again";
		event.setDisconnectReason( txt );
		event.setStartdAddr( startd_addr );
		event.setStartdName( startd_name );

		if( !ULog->writeEvent(&event) ) {
			dprintf( D_ALWAYS, "Unable to log ULOG_JOB_DISCONNECTED event\n" );
		}
		delete ULog;
		ULog = NULL;
	}

	dprintf( D_FULLDEBUG, "Adding match record for disconnected job %d.%d "
			 "(owner: %s)\n", cluster, proc, owner );
	dprintf( D_FULLDEBUG, "ClaimId: %s\n", claim_id );
	if( pool ) {
		dprintf( D_FULLDEBUG, "Pool: %s (via flocking)\n", pool );
	}
		// note: AddMrec will makes its own copy of match_ad
	match_rec *mrec = AddMrec( claim_id, startd_addr, job, match_ad, 
							   owner, pool );
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

		// the match_rec also needs to point to the srec...
	mrec->shadowRec = srec;

		// finally, enqueue this job in our RunnableJob queue.
	addRunnableJob( srec );
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

	// don't count REMOVED or HELD jobs
	if( max_hosts > cur_hosts &&
		(status == IDLE || status == UNEXPANDED || status == RUNNING) )
	{
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
	PROC_ID id;
	match_rec *rec;
	bool ReactivatingMatch;
    
        /* Todd also added this; watch for conflict! */
    startJobsDelayBit = FALSE;

		/* If we are trying to exit, don't start any new jobs! */
	if ( ExitWhenDone ) {
		return;
	}

	dprintf(D_FULLDEBUG, "-------- Begin starting jobs --------\n");
	startJobsDelayBit = FALSE;
	matches->startIterations();
	while(matches->iterate(rec) == 1) {
		switch(rec->status) {
		case M_UNCLAIMED:
			dprintf(D_FULLDEBUG, "match (%s) unclaimed\n", rec->id);
			continue;
		case M_CONNECTING:
			dprintf(D_FULLDEBUG, "match (%s) waiting for connection\n", rec->id);
			continue;
		case M_STARTD_CONTACT_LIMBO:
			dprintf ( D_FULLDEBUG, "match (%s) waiting for startd contact\n", 
					  rec->id );
			continue;
		case M_ACTIVE:
		case M_CLAIMED:
			if ( rec->shadowRec ) {
				dprintf(D_FULLDEBUG, "match (%s) already running a job\n",
						rec->id);
				continue;
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
		if(!Runnable(&id))
		// find the job in the cluster with the highest priority
		{
			FindRunnableJob(id,rec->my_match_ad,rec->user);
		}
		if(id.proc < 0)
		// no more jobs to run
		{
			dprintf(D_ALWAYS,
					"match (%s) out of jobs (cluster id %d); relinquishing\n",
					rec->id, id.cluster);
			Relinquish(rec);
			DelMrec(rec);
			continue;
		}

#ifdef WANT_NETMAN
		if (ManageBandwidth && ReactivatingMatch) {
			RequestBandwidth(id.cluster, id.proc, rec);
		}
#endif

		if(!(rec->shadowRec = StartJob(rec, &id))) {
                
		// Start job failed. Throw away the match. The reason being that we
		// don't want to keep a match around and pay for it if it's not
		// functioning and we don't know why. We might as well get another
		// match.

			dprintf(D_ALWAYS,"Failed to start job %s; relinquishing\n",
					rec->id);
			Relinquish(rec);
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
			continue;
		}
		dprintf(D_FULLDEBUG, "Match (%s) - running %d.%d\n",rec->id,id.cluster,
				id.proc);
			// If we're reusing a match to start another job, then cluster
			// and proc may have changed, so we keep them up-to-date here.
			// This is important for Scheduler::AlreadyMatched().
		rec->cluster = id.cluster;
		rec->proc = id.proc;
			// Now that the shadow has spawned, consider this match "ACTIVE"
		rec->setStatus( M_ACTIVE );
	}
	if( LocalUniverseJobsIdle > 0 || SchedUniverseJobsIdle > 0 ) {
		StartLocalJobs();
	}

	/* Reset the our Timer */
	daemonCore->Reset_Timer(startjobsid,SchedDInterval);

	dprintf(D_FULLDEBUG, "-------- Done starting jobs --------\n");
}


#ifdef WANT_NETMAN
void
Scheduler::RequestBandwidth(int cluster, int proc, match_rec *rec)
{
	ClassAd request;
	char buf[256], source[100], dest[100], user[200], *str;
	int executablesize = 0, universe = CONDOR_UNIVERSE_VANILLA, vm=1;

	GetAttributeString( cluster, proc, ATTR_USER, user );	// TODDCORE 
	sprintf(buf, "%s = \"%s\"", ATTR_USER, user );
	request.Insert(buf);
	sprintf(buf, "%s = 1", ATTR_FORCE);
	request.Insert(buf);
	strcpy(dest, rec->peer+1);
	str = strchr(dest, ':');
	*str = '\0';
	sprintf(buf, "%s = \"%s\"", ATTR_DESTINATION, dest);
	request.Insert(buf);
	GetAttributeInt(cluster, proc, ATTR_EXECUTABLE_SIZE, &executablesize);
	sprintf(buf, "%s = \"%s\"", ATTR_REMOTE_HOST, rec->peer);
	request.Insert(buf);
	if (rec->my_match_ad) {
		rec->my_match_ad->LookupInteger(ATTR_VIRTUAL_MACHINE_ID, vm);
	}
	sprintf(buf, "%s = %d", ATTR_VIRTUAL_MACHINE_ID, vm);
	request.Insert(buf);
    
    SafeSock sock;
	sock.timeout(NEGOTIATOR_CONTACT_TIMEOUT);
	Daemon negotiator (DT_NEGOTIATOR);
	if (!sock.connect(negotiator.addr())) {
		dprintf(D_FAILURE|D_ALWAYS, "Couldn't connect to negotiator!\n");
		return;
	}

	negotiator.startCommand(REQUEST_NETWORK, &sock);

	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe);
	float cputime = 1.0;
	GetAttributeFloat(cluster, proc, ATTR_JOB_REMOTE_USER_CPU, &cputime);
	if (universe == CONDOR_UNIVERSE_STANDARD && cputime > 0.0) {
		int ckptsize;
		GetAttributeInt(cluster, proc, ATTR_IMAGE_SIZE, &ckptsize);
		ckptsize -= executablesize;	// imagesize = ckptsize + executablesize
		sprintf(buf, "%s = %f", ATTR_REQUESTED_CAPACITY,
				(float)ckptsize*1024.0);
		request.Insert(buf);
		if ((GetAttributeString(cluster, proc,
                                ATTR_LAST_CKPT_SERVER, source)) == 0) {
			struct hostent *hp = gethostbyname(source);
			if (!hp) {
				dprintf(D_FAILURE|D_ALWAYS, "DNS lookup for %s %s failed!\n",
						ATTR_LAST_CKPT_SERVER, source);
				return;
			}
			sprintf(buf, "%s = \"%s\"", ATTR_SOURCE,
					inet_ntoa(*((struct in_addr *)hp->h_addr)));
		} else {
			sprintf(buf, "%s = \"%s\"", ATTR_SOURCE,
					inet_ntoa(*(my_sin_addr())));
		}
		sprintf(buf, "%s = \"CheckpointRestart\"", ATTR_TRANSFER_TYPE);
		request.Insert(buf);
		request.Insert(buf);
		sock.put(2);
		request.put(sock);
	} else {
		sock.put(1);
	}

	sprintf(buf, "%s = \"InitialCheckpoint\"", ATTR_TRANSFER_TYPE);
	request.Insert(buf);
	sprintf(buf, "%s = %f", ATTR_REQUESTED_CAPACITY,
			(float)executablesize*1024.0);
	request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_SOURCE, inet_ntoa(*(my_sin_addr())));
	request.Insert(buf);
	request.put(sock);
	sock.end_of_message();
}
#endif

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

			// if we got this far, we're definitely starting the job,
			// so deal with the aboutToSpawnJobHandler hook...
		callAboutToSpawnJobHandler( cluster, proc, srec );

		int universe = srec->universe;
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
	char	args[_POSIX_ARG_MAX];

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
		delete( shadow_obj );
		return;
	}

	if ( sh_reads_file ) {
		if( sh_is_dc ) { 
			sprintf( args, "condor_shadow -f %d.%d%s%s -", 
					 job_id->cluster, job_id->proc, 
					 wants_reconnect ? " --reconnect " : " ",
					 MyShadowSockName ); 
		} else {
			sprintf( args, "condor_shadow %s %s %s %d %d -", 
					 MyShadowSockName, mrec->peer, mrec->id,
					 job_id->cluster, job_id->proc );
		}
	} else {
			// CRUFT: pre-6.7.0 shadows...
		if ( sh_is_dc ) {
			sprintf( args, "condor_shadow -f %s %s %s %d %d",
					 MyShadowSockName, mrec->peer, mrec->id, 
					 job_id->cluster, job_id->proc );
		} else {
				// standard universe shadow 
			sprintf( args, "condor_shadow %s %s %s %d %d", 
					 MyShadowSockName, mrec->peer, mrec->id,
					 job_id->cluster, job_id->proc );
		}
	}

	rval = spawnJobHandlerRaw( srec, shadow_path, args, NULL, "shadow",
							   sh_is_dc, sh_reads_file );

	free( shadow_path );

	if( ! rval ) {
		mark_job_stopped(job_id);
		RemoveShadowRecFromMrec(srec);
		delete srec;
		return;
	}

	dprintf( D_ALWAYS, "Started shadow for job %d.%d on \"%s\", "
			 "(shadow pid = %d)\n", job_id->cluster, job_id->proc,
			 mrec->peer, srec->pid );

		// If this is a reconnect shadow, update the mrec with some
		// important info.  This usually happens in StartJobs(), but
		// in the case of reconnect, we don't go through that code. 
	if( wants_reconnect ) {
			// Now that the shadow is alive, the match is "ACTIVE"
		mrec->setStatus( M_ACTIVE );
		mrec->cluster = job_id->cluster;
		mrec->proc = job_id->proc;
		dprintf(D_FULLDEBUG, "Match (%s) - running %d.%d\n", mrec->id,
				mrec->cluster, mrec->proc );

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
Scheduler::tryNextJob( void )
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
			/* 
			   if there are no more jobs in the queue, this is a great
			   time to update the central manager since things are
			   pretty "stable".  this way, "condor_status -sub" and
			   other views such as "condor_status -run" will be more
			   likely to match
			*/
		timeout();
		StartJobs();
	}
}


bool
Scheduler::spawnJobHandlerRaw( shadow_rec* srec, const char* path, 
							   const char* args, const char* env, 
							   const char* name, bool is_dc, bool wants_pipe )
{
	int pid = -1;
	PROC_ID* job_id = &srec->job_id;

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
    char *nice_config = param( "SHADOW_RENICE_INCREMENT" );
    int niceness = 0;
    if( nice_config ) {
        niceness = atoi( nice_config );
        free( nice_config );
        nice_config = NULL;
    }

	int rid = 1;
	if( ( srec->universe == CONDOR_UNIVERSE_MPI) ||
		( srec->universe == CONDOR_UNIVERSE_PARALLEL)) {
		rid = dedicated_scheduler.rid;
		}
	
	/* For now, we should create the handler as PRIV_ROOT so it can do
	   priv switching between PRIV_USER (for handling syscalls, moving
	   files, etc), and PRIV_CONDOR (for writing to log files).
	   Someday, hopefully soon, we'll fix this and spawn the
	   shadow/handler with PRIV_USER_FINAL... */
	pid = daemonCore->Create_Process( path, args, PRIV_ROOT, rid, 
									  is_dc, env, NULL, FALSE, NULL, 
									  std_fds_p, niceness );

	if( pid == FALSE ) {
		dprintf( D_FAILURE|D_ALWAYS, "spawnJobHandlerRaw: "
				 "CreateProcess(%s, %s) failed\n", path, args );
		if( wants_pipe ) {
			for( int i = 0; i < 2; i++ ) {
				if( pipe_fds[i] >= 0 ) {
					daemonCore->Close_Pipe( pipe_fds[i] );
				}
			}
		}
		return false;
	} 

		// if it worked, store the pid in our shadow record, and add
		// this srec to our various tables.  this ensures we have
		// ATTR_REMOTE_HOST set in the job ad by the time we give it
		// to the shadow...
	srec->pid=pid;
	add_shadow_rec(srec);

		// finally, now that the handler has been spawned, we need to
		// do some things with the pipe (if there is one):
	if( wants_pipe ) {
			// 1) close our copy of the read end of the pipe, so we
			// don't leak it.  we have to use DC::Close_Pipe() for
			// this, not just close(), so things work on windoze.
		daemonCore->Close_Pipe( pipe_fds[0] );

			// 2) dump out the job ad to the write end, since the
			// handler is now alive and can read from the pipe.  we
			// want to use "true" for the last argument so that we
			// expand $$ expressions within the job ad to pull things
			// out of the startd ad before we give it to the handler. 
		ClassAd* ad = GetJobAd( job_id->cluster, job_id->proc, true );
		FILE* fp = fdopen( pipe_fds[1], "w" );
		if ( ad ) {
			ad->fPrint( fp );

			// Note that ONLY when GetJobAd() is called with expStartdAd==true
			// do we want to delete the result.
			delete ad;
		} else {
			// This should never happen
			EXCEPT( "Impossible: GetJobAd() returned NULL for %d.%d but that" 
				"job is already known to exist",
				 job_id->cluster, job_id->proc );
		}

			// since this is probably a DC pipe that we have to close
			// with Close_Pipe(), we can't call fclose() on it.  so,
			// unless we call fflush(), we won't get any output. :(
		if( fflush(fp) < 0 ) {
			dprintf( D_ALWAYS,
					 "writeJobAd: fflush() failed: %s (errno %d)\n",
					 strerror(errno), errno );
		}
			// TODO: if this is an MPI job, we should really write all
			// the match info (ClaimIds, sinful strings and machine
			// ads) to the pipe before we close it, but that's just a
			// performance optimization, not a correctness issue.

			// Now that all the data is written to the pipe, we can
			// safely close the other end, too.  
		daemonCore->Close_Pipe( fp );
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

	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);

	// add job to run queue
	shadow_rec* srec=add_shadow_rec( 0, job_id, univ, mrec, -1 );
	addRunnableJob( srec );
	return srec;
}


shadow_rec*
Scheduler::start_pvm(match_rec* mrec, PROC_ID *job_id)
{

#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	char			args[128];
	char			cluster[10], proc_str[10];
	int				pid;
	int				shadow_fd;
	char			out_buf[1000];
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

	(void)sprintf( cluster, "%d", job_id->cluster );
	(void)sprintf( proc_str, "%d", job_id->proc );
	
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
	    sprintf( args, "condor_shadow.pvm %s", MyShadowSockName );

		int fds[3];
		fds[0] = pipes[0];  // the effect is to dup the pipe to stdin in child.
	    fds[1] = fds[2] = -1;
		dprintf( D_ALWAYS, "About to Create_Process( %s, %s, ... )\n", 
				 shadow_path, args );
		
		pid = daemonCore->Create_Process( shadow_path, args, PRIV_ROOT, 
										  1, FALSE, NULL, NULL, FALSE, 
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

    sprintf(out_buf, "%d %d %d\n", job_id->cluster, job_id->proc, 1);

		// Warning: the pvm shadow may close this pipe during a
		// graceful shutdown.  We should consider than an indication
		// that the shadow doesn't want any more machines.  We should
		// not kill the shadow if it closes the pipe, though, since it
		// has some useful cleanup to do (i.e., so we can't return
		// NULL here when the pipe is closed, since our caller will
		// consider that a fatal error for the shadow).
	
	dprintf( D_ALWAYS, "First Line: %s", out_buf );
	write(shadow_fd, out_buf, strlen(out_buf));

	sprintf(out_buf, "%s %s %d %s\n", mrec->peer, mrec->id, old_proc,
			hostname);
	dprintf( D_ALWAYS, "sending %s", out_buf);
	write(shadow_fd, out_buf, strlen(out_buf));
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
	MyString starter_args;
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

	starter_args = "condor_starter -f -job-cluster ";
	starter_args += job_id->cluster;
	starter_args += " -job-proc ";
	starter_args += job_id->proc;
	starter_args += " -job-input-ad - -schedd-addr ";
	starter_args += MyShadowSockName;

	dprintf( D_FULLDEBUG, "About to spawn %s %s\n", 
			 starter_path, starter_args.Value() );

	BeginTransaction();
	mark_job_running( job_id );
	CommitTransaction();

	MyString starter_env;
	starter_env.sprintf( "_%s_EXECUTE=%s", myDistro->Get(),
						 LocalUnivExecuteDir );
	
	rval = spawnJobHandlerRaw( srec, starter_path, starter_args.Value(),
							   starter_env.Value(), "starter", true, true );

	free( starter_path );
	starter_path = NULL;

	if( ! rval ) {
		dprintf( D_ALWAYS|D_FAILURE, "Can't spawn local starter for "
				 "job %d.%d\n", job_id->cluster, job_id->proc );
		BeginTransaction();
		mark_job_stopped( job_id );
		CommitTransaction();
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

	char	a_out_name[_POSIX_PATH_MAX];
	char	input[_POSIX_PATH_MAX];
	char	output[_POSIX_PATH_MAX];
	char	error[_POSIX_PATH_MAX];
	char	*env = NULL;
	char	job_args[_POSIX_ARG_MAX];
	char	args[_POSIX_ARG_MAX];
	char	owner[_POSIX_PATH_MAX], iwd[_POSIX_PATH_MAX];
	char	domain[_POSIX_PATH_MAX];
	int		pid;
	StatInfo* filestat;
	bool is_executable;

	is_executable = false;
	
	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
		job_id->cluster, job_id->proc );
	
	// make sure file is executable
	strcpy(a_out_name, gen_ckpt_name(Spool, job_id->cluster, ICKPT, 0));
	errno = 0;
#ifndef WIN32
	if( chmod(a_out_name, 0755) < 0 ) { 
#else
// WIN32
// We can't change execute permissions on NT with chmod 
// (we'd have to change the file extension to .exe, .com, .bat or .cmd)
// So instead, just check if it is executable. 
	filestat = new StatInfo(a_out_name);
	is_executable = false;
	if ( filestat ) {
		is_executable = filestat->IsExecutable();
		delete filestat;
		filestat = NULL;
	}
	if ( !is_executable ) {
#endif
		// If the chmod failed, it could be because the user submitted
		// the job with copy_to_spool = false and therefore it is not
		// in the SPOOL directory.  So check where the 
		// ClassAd says the executable is.
		ClassAd *userJob = GetJobAd(job_id->cluster,job_id->proc);
		a_out_name[0] = '\0';
		userJob->LookupString(ATTR_JOB_CMD,a_out_name,sizeof(a_out_name));
		FreeJobAd(userJob);
		if (a_out_name[0]=='\0') {
			holdJob(job_id->cluster, job_id->proc, 
				"Executable unknown - not specified in job ad!",
				false, false, true, false, false );
			return NULL;
		}
		
		// at least make certain the file is still there, and that
		// it is exectuable by at least user or group or other.
		filestat = new StatInfo(a_out_name);
		is_executable = false;
		if ( filestat ) {
			is_executable = filestat->IsExecutable();
			delete filestat;
		}
		if ( !is_executable ) {
			char tmpstr[255];
			snprintf(tmpstr, 255, "File '%s' is missing or not executable", a_out_name);
			holdJob(job_id->cluster, job_id->proc, tmpstr,
					false, false, true, false, false);
			return NULL;
		}
	}
	
	if (GetAttributeString(job_id->cluster, job_id->proc, 
		ATTR_OWNER, owner) < 0) {
		dprintf(D_FULLDEBUG, "Scheduler::start_sched_universe_job"
			"--setting owner to \"nobody\"\n" );
		sprintf(owner, "nobody");
	}
	// get the nt domain too, if we have it
	GetAttributeString(job_id->cluster, job_id->proc, ATTR_NT_DOMAIN, domain);

	if (stricmp(owner, "root") == 0 ) {
		dprintf(D_ALWAYS, "Aborting job %d.%d.  Tried to start as root.\n",
			job_id->cluster, job_id->proc);
		return NULL;
	}
	
	if (! init_user_ids(owner, domain) ) {
		char tmpstr[255];
#ifdef WIN32
		snprintf(tmpstr, 255, "Bad or missing credential for user: %s", owner);
#else
		snprintf(tmpstr, 255, "Unable to switch to user: %s", owner);
#endif
		holdJob(job_id->cluster, job_id->proc, tmpstr,
				false, false, true, false, false);
		return NULL;
	}
	
	// Get std(in|out|err)
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_INPUT,
		input) < 0) {
		sprintf(input, NULL_FILE); 
		
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_OUTPUT,
		output) < 0) {
		sprintf(output, NULL_FILE);
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ERROR,
		error) < 0) {
		sprintf(error, NULL_FILE);
	}
	
	priv_state priv = set_user_priv(); // need user's privs...
	
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_IWD,
		iwd) < 0) {
#ifndef WIN32		
		sprintf(iwd, "/tmp");
#else
		// try to get the temp dir, otherwise just use the root directory
		char* tempdir = getenv("TEMP");
		sprintf(iwd, "%s", ((tempdir) ? tempdir : "\\") );		
		
#endif
	}
	
	//change to IWD before opening files, easier than prepending 
	//IWD if not absolute pathnames
	char tmpCwd[_POSIX_PATH_MAX];
	char *p_tmpCwd = tmpCwd;
	p_tmpCwd = getcwd( p_tmpCwd, _POSIX_PATH_MAX );
	chdir(iwd);
	
	// now open future in|out|err files
	int inouterr[3];
	bool cannot_open_files = false;
	
#ifdef WIN32
	
	// submit gives us /dev/null regardless of the platform.
	// normally, the starter would handle this translation,
	// but since we're the schedd, we'll have to do it ourselves.
	// At least for now. --stolley
	
	if (nullFile(input)) {
		sprintf(input, WINDOWS_NULL_FILE);
	}
	if (nullFile(output)) {
		sprintf(output, WINDOWS_NULL_FILE);
	}
	if (nullFile(error)) {
		sprintf(error, WINDOWS_NULL_FILE);
	}
	
#endif
	
	if ((inouterr[0] = open(input, O_RDONLY, S_IREAD)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", input, errno );
		cannot_open_files = true;
	}
	if ((inouterr[1] = open(output, O_WRONLY | O_CREAT | O_TRUNC, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", output, errno );
		cannot_open_files = true;
	}
	if ((inouterr[2] = open(error, O_WRONLY | O_CREAT | O_TRUNC, S_IREAD|S_IWRITE)) < 0) {
		dprintf ( D_FAILURE|D_ALWAYS, "Open of %s failed, errno %d\n", error, errno );
		cannot_open_files = true;
	}
	
	//change back to whence we came
	if ( p_tmpCwd ) {
		chdir( p_tmpCwd );
	}
	
	if ( cannot_open_files ) {
	/* I'll close the opened files in the same priv state I opened them
		in just in case the OS cares about such things. */
		if (inouterr[0] >= 0) {
			if (close(inouterr[0]) == -1) {
				dprintf(D_ALWAYS, 
					"Failed to close input file fd for '%s' because [%d %s]\n",
					input, errno, strerror(errno));
			}
		}
		if (inouterr[1] >= 0) {
			if (close(inouterr[1]) == -1) {
				dprintf(D_ALWAYS,  
					"Failed to close output file fd for '%s' because [%d %s]\n",
					output, errno, strerror(errno));
			}
		}
		if (inouterr[2] >= 0) {
			if (close(inouterr[2]) == -1) {
				dprintf(D_ALWAYS,  
					"Failed to close error file fd for '%s' because [%d %s]\n",
					output, errno, strerror(errno));
			}
		}
		set_priv( priv );  // back to regular privs...
		return NULL;
	}
	
	set_priv( priv );  // back to regular privs...
	
	if (GetAttributeStringNew(job_id->cluster, job_id->proc, ATTR_JOB_ENVIRONMENT,
		&env) < 0) {
		// Note that GetAttributeStringNew always fills in the env variable,
		// even if it's an empty string. We get back -1 if it's empty. 
		// Filling it in with 0 is redundant, I suppose. 
		env[0] = '\0';
	}
	
	// stick a CONDOR_ID environment variable in job's environment
	char condor_id_string[64];
	sprintf( condor_id_string, "%d.%d", job_id->cluster, job_id->proc );
	Env envobject;
	envobject.Merge(env);
	envobject.Put("CONDOR_ID",condor_id_string);
	char *newenv = envobject.getDelimitedString();
	
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ARGUMENTS,
		args) < 0) {
		args[0] = '\0';
	}
	
	// Don't use a_out_name for argv[0], use
	// "condor_scheduniv_exec.cluster.proc" instead. 
    // NOTE: a trailing space with no args causes Create_Process to
    // call the program with a single arg consisting of the null
    // string (''), so we have to avoid that
    sprintf(job_args, "condor_scheduniv_exec.%d.%d%s%s",
		job_id->cluster, job_id->proc, args[0] != '\0' ? " " : "", args );

        // get the job's nice increment
    char *nice_config = param( "SCHED_UNIV_RENICE_INCREMENT" );
    int niceness = 0;
    if( nice_config ) {
        niceness = atoi( nice_config );
        free( nice_config );
        nice_config = NULL;
    }
	
	pid = daemonCore->Create_Process( a_out_name, job_args, PRIV_USER_FINAL, 
								1, FALSE, newenv, iwd, FALSE, NULL, inouterr,
									  niceness );
	
	// now close those open fds - we don't want them here.
	for ( int i=0 ; i<3 ; i++ ) {
		if ( close( inouterr[i] ) == -1 ) {
			dprintf ( D_ALWAYS, "FD closing problem, errno = %d\n", errno );
		}
	}
	
	if (env) {
		free(env);
	}

	if (newenv) {
		delete[] newenv;
	}
	
	if ( pid <= 0 ) {
		dprintf ( D_FAILURE|D_ALWAYS, "Create_Process problems!\n" );
		return NULL;
	}
	
	dprintf ( D_ALWAYS, "Successfully created sched universe process\n" );
	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
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

	return add_shadow_rec( pid, job_id, CONDOR_UNIVERSE_SCHEDULER, NULL, -1 );
}

void
Scheduler::display_shadow_recs()
{
	struct shadow_rec *r;


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
add_shadow_birthdate(int cluster, int proc)
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

	// Update the current start & last start times
	if ( GetAttributeInt(cluster, proc,
						  ATTR_JOB_CURRENT_START_DATE, 
						 &job_start_date) >= 0 ) {
		// It's been run before, so copy the current into last
		SetAttributeInt(cluster, proc, ATTR_JOB_LAST_START_DATE, 
						job_start_date);
	}
	// Update current
	SetAttributeInt(cluster, proc, ATTR_JOB_CURRENT_START_DATE, current_time);

	// Update the job run count
	int count;
	if ( GetAttributeInt(cluster, proc, ATTR_JOB_RUN_COUNT, &count ) < 0 ) {
		count = 0;
	}
	SetAttributeInt(cluster, proc, ATTR_JOB_RUN_COUNT, ++count);
}

struct shadow_rec *
Scheduler::add_shadow_rec( shadow_rec* new_rec )
{
	numShadows++;
	shadowsByPid->insert(new_rec->pid, new_rec);
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

		SetAttributeString( cluster, proc, ATTR_CLAIM_ID, mrec->id );
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
			int vm = 1;
			mrec->my_match_ad->LookupInteger( ATTR_VIRTUAL_MACHINE_ID, vm );
			SetAttributeInt(cluster,proc,ATTR_REMOTE_VIRTUAL_MACHINE_ID,vm);
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
	}
	GetAttributeInt( cluster, proc, ATTR_JOB_UNIVERSE, &new_rec->universe );
	if (new_rec->universe == CONDOR_UNIVERSE_PVM) {
		ClassAd *ad;
		ad = GetNextJob(1);
		while (ad != NULL) {
			PROC_ID tmp_id;
			ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == cluster) {
				ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				add_shadow_birthdate(tmp_id.cluster, tmp_id.proc);
			}
			ad = GetNextJob(0);
		}
	} else {
		add_shadow_birthdate( cluster, proc );
	}
	CommitTransaction();
	dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
			 new_rec->pid, cluster, proc );
	scheduler.display_shadow_recs();
	RecomputeAliveInterval(cluster,proc);
	return new_rec;
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
				SetAttributeInt(cluster, proc, ATTR_JOB_WALL_CLOCK_CKPT,
								run_time);
			}
		}
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

	dprintf( D_FULLDEBUG, "Entered delete_shadow_rec( %d )\n", pid );

	if( shadowsByPid->lookup(pid, rec) == 0 ) {
		int cluster = rec->job_id.cluster;
		int proc = rec->job_id.proc;

		dprintf( D_FULLDEBUG,
				 "Deleting shadow rec for PID %d, job (%d.%d)\n",
				 pid, cluster, proc );

		BeginTransaction();

		int job_status = IDLE;
		GetAttributeInt( cluster, proc, ATTR_JOB_STATUS, &job_status );

		if( rec->universe == CONDOR_UNIVERSE_PVM ) {
			ClassAd *ad;
			ad = GetNextJob(1);
			while (ad != NULL) {
				PROC_ID tmp_id;
				ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
				if (tmp_id.cluster == cluster) {
					ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
					update_remote_wall_clock(tmp_id.cluster, tmp_id.proc);
				}
				ad = GetNextJob(0);
			}
		} else {
			update_remote_wall_clock(cluster, proc);
		}

		char* last_host = NULL;
		GetAttributeStringNew( cluster, proc, ATTR_REMOTE_HOST, &last_host );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_HOST );
		if( last_host ) {
			SetAttributeString( cluster, proc, ATTR_LAST_REMOTE_HOST,
								last_host );
			free( last_host );
			last_host = NULL;
		}

		char* last_claim = NULL;
		GetAttributeStringNew( cluster, proc, ATTR_CLAIM_ID, &last_claim );
		DeleteAttribute( cluster, proc, ATTR_CLAIM_ID );
		if( last_claim ) {
			SetAttributeString( cluster, proc, ATTR_LAST_CLAIM_ID, 
								last_claim );
			free( last_claim );
			last_claim = NULL;
		}

		DeleteAttribute( cluster, proc, ATTR_REMOTE_POOL );
		DeleteAttribute( cluster, proc, ATTR_REMOTE_VIRTUAL_MACHINE_ID );
		DeleteAttribute( cluster, proc, ATTR_SHADOW_BIRTHDATE );

			// we want to commit all of the above changes before we
			// call check_zombie() since it might do it's own
			// transactions of one sort or another...
		CommitTransaction();

		check_zombie( pid, &(rec->job_id) );
			// If the shadow went away, this match is no longer
			// "ACTIVE", it's just "CLAIMED"
		if( rec->match ) {
				// Be careful, since there might not be a match record
				// for this shadow record anymore... 
			rec->match->setStatus( M_CLAIMED );
		}
		RemoveShadowRecFromMrec(rec);
		shadowsByPid->remove(pid);
		shadowsByProcID->remove(rec->job_id);
		if ( rec->conn_fd != -1 )
			close(rec->conn_fd);

		delete rec;
		numShadows -= 1;
		if( ExitWhenDone && numShadows == 0 ) {
			return;
		}
		display_shadow_recs();
		return;
	}
	dprintf( D_FULLDEBUG, "Exited delete_shadow_rec( %d )\n", pid );
}

/*
** Mark a job as running.  Do not call directly.  Call the non-underscore
** version below instead.
*/
void
_mark_job_running(PROC_ID* job_id)
{
	int status;
	int orig_max;

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
	char	owner[_POSIX_PATH_MAX];

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

		if ( GetAttributeString(job_id->cluster, job_id->proc, 
				ATTR_OWNER, owner) < 0 )
		{
			dprintf(D_FULLDEBUG,"mark_job_stopped: setting owner to \"nobody\"\n");
			strcpy(owner,"nobody");
		}

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



void
Scheduler::mark_cluster_rejected(int cluster)
{
	int		i;

	for( i=0; i<N_RejectedClusters; i++ ) {
		if( RejectedClusters[i] == cluster ) {
			return;
		}
	}
	RejectedClusters[ N_RejectedClusters++ ] = cluster;
}

int
Scheduler::cluster_rejected(int cluster)
{
	int		i;

	for( i=0; i<N_RejectedClusters; i++ ) {
		if( RejectedClusters[i] == cluster ) {
			return 1;
		}
	}
	return 0;
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

	dprintf( D_ALWAYS, "Called preempt( %d )%s\n", n, 
			 force_sched_jobs ? " forcing scheduler univ preemptions" : "" );

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
				daemonCore->Send_Signal( rec->pid, SIGTERM );
				dprintf( D_ALWAYS, "Sent SIGTERM to shadow for PVM job "
						 "%d.%d (pid: %d)\n", cluster, proc, rec->pid );
				break;

			case CONDOR_UNIVERSE_LOCAL:
				if( ! preempt_sched ) {
					continue;
				}
				daemonCore->Send_Signal( rec->pid, DC_SIGSOFTKILL );
				dprintf( D_ALWAYS, "Sent DC_SIGSOFTKILL to handler for "
						 "local universe job %d.%d (pid: %d)\n", 
						 cluster, proc, rec->pid );
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
				daemonCore->Send_Signal( rec->pid, kill_sig );
				dprintf( D_ALWAYS, "Sent %s to scheduler universe job "
						 "%d.%d (pid: %d)\n", signalName(kill_sig), 
						 cluster, proc, rec->pid );
				break;

			default:
					// all other universes
				if( rec->match ) {
					send_vacate( rec->match, CKPT_FRGN_JOB );
					dprintf( D_ALWAYS, 
							 "Sent vacate command to %s for job %d.%d\n",
							 rec->match->peer, cluster, proc );
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
			}

				// if we're here, we really preempted it, so mark it
				// as preempted, and decrement n so we let this count
				// towards our goal.
			rec->preempted = TRUE;
			n--;
		}
	}

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
    SafeSock	sock;

	dprintf( D_FULLDEBUG, "Called send_vacate( %s, %d )\n", match->peer, cmd );

	sock.timeout(STARTD_CONTACT_TIMEOUT);
	if (!sock.connect(match->peer,START_PORT)) {
		dprintf( D_FAILURE|D_ALWAYS,"Can't connect to startd at %s\n",match->peer);
		return;
	}
 
	DCStartd d( match->peer );
	d.startCommand(cmd, &sock);

	sock.encode();

	if( !sock.put(match->id) ) {
		dprintf( D_ALWAYS, "Can't initialize sock to %s\n", match->peer);
		return;
	}

	if( !sock.eom() ) {
		dprintf( D_ALWAYS, "Can't send EOM to %s\n", match->peer);
		return;
	}

	switch(cmd) {
		case CKPT_FRGN_JOB:
			dprintf( D_ALWAYS, "Sent CKPT_FRGN_JOB to startd on %s\n", match->peer);
			break;
		case KILL_FRGN_JOB:
			dprintf( D_ALWAYS, "Sent KILL_FRGN_JOB to startd on %s\n", match->peer);
			break;
		case RELEASE_CLAIM:
			dprintf( D_ALWAYS, "Sent RELEASE_CLAIM to startd on %s\n", match->peer);
			break;
		default:
			dprintf( D_ALWAYS, "Send %i to startd on %s\n", cmd, match->peer);
	}
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
			sprintf(buf, "%d.%d", p.cluster, proc_index);
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
	char owner[2048], subject[2048];
	char cmd[_POSIX_PATH_MAX], args[_POSIX_ARG_MAX];

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
	if (GetAttributeString(srec->job_id.cluster, srec->job_id.proc,
							ATTR_JOB_ARGUMENTS, args) < 0) {
		EXCEPT("GetAttributeString(%d, %d, \"%s\"", srec->job_id.cluster,
				srec->job_id.proc, ATTR_JOB_ARGUMENTS);
	}

	// Send mail to user
	sprintf(subject, "Condor Job %d.%d", srec->job_id.cluster, 
		srec->job_id.proc);
	dprintf( D_FULLDEBUG, "Unknown user notification selection\n");
	dprintf( D_FULLDEBUG, "\tNotify user with subject: %s\n",subject);

	FILE* mailer = email_open(owner, subject);
	if( mailer ) {
		fprintf( mailer, "Your condor job %s%d.\n\n", msg, status );
		fprintf( mailer, "Job: %s %s\n", cmd, args );
		email_close( mailer );
	}

/*
	sprintf(url, "mailto:%s", owner);
	if ((fd = open_url(url, O_WRONLY, 0)) < 0) {
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
	write(fd, cmd, strlen(cmd));
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
}

void
Scheduler::child_exit(int pid, int status)
{
	shadow_rec*		srec;
	int				StartJobsFlag=TRUE;
	int				q_status;  // status of this job in the queue 
	PROC_ID			job_id;

	srec = FindSrecByPid(pid);
	job_id.cluster = srec->job_id.cluster;
	job_id.proc = srec->job_id.proc;

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
		if( IsLocalUniverse(srec) ) {
			name = "Local starter";
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
		if( GetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
							ATTR_JOB_STATUS, &q_status) < 0 ) {
			EXCEPT( "ERROR no job status for %d.%d in child_exit()!",
					srec->job_id.cluster, srec->job_id.proc );
		}
		if( WIFEXITED(status) ) {			
            dprintf( D_ALWAYS,
					 "%s pid %d for job %d.%d exited with status %d\n",
					 name, pid, srec->job_id.cluster, srec->job_id.proc,
					 WEXITSTATUS(status) );

			switch( WEXITSTATUS(status) ) {
			case JOB_NO_MEM:
				swap_space_exhausted();
			case JOB_EXEC_FAILED:
				StartJobsFlag=FALSE;
				break;
			case JOB_CKPTED:
			case JOB_NOT_CKPTED:
					// we can't have the same value twice in our
					// switch, so we can't really have a valid case
					// for this, but it's the same number as
					// JOB_NOT_CKPTED, so we're safe.
			// case JOB_SHOULD_REQUEUE:  
			case JOB_NOT_STARTED:
				if( !srec->removed && srec->match ) {
					Relinquish(srec->match);
					DelMrec(srec->match);
				}
				break;
			case JOB_SHADOW_USAGE:
				EXCEPT( "%s exited with incorrect usage!\n", name );
				break;
			case JOB_BAD_STATUS:
				EXCEPT( "%s exited because job status != RUNNING", name );
				break;
			case JOB_SHOULD_REMOVE:
				dprintf( D_ALWAYS, "Removing job %d.%d\n",
						 srec->job_id.cluster, srec->job_id.proc );
					// Set this flag in our shadow record so we
					// treat this just like a condor_rm
				srec->removed = true;
					// no break, fall through and do the action
			case JOB_NO_CKPT_FILE:
			case JOB_KILLED:
				if( q_status != HELD ) {
					set_job_status( srec->job_id.cluster,
									srec->job_id.proc, REMOVED ); 
				}
				break;
			case JOB_EXITED:
				dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
			case JOB_COREDUMPED:
				if( q_status != HELD ) {
					set_job_status( srec->job_id.cluster,
									srec->job_id.proc, COMPLETED ); 
				}
				break;
			case JOB_SHOULD_HOLD:
				dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
						 srec->job_id.cluster, srec->job_id.proc );
				set_job_status( srec->job_id.cluster, srec->job_id.proc, 
								HELD );
				break;
			case DPRINTF_ERROR:
				dprintf( D_ALWAYS,
						 "ERROR: %s had fatal error writing its log file\n",
						 name );
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
			case JOB_EXCEPTION:
				if ( WEXITSTATUS(status) == JOB_EXCEPTION ){
					dprintf( D_ALWAYS,
							 "ERROR: %s exited with job exception code!\n",
							 name );
				}
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
			default:
				/* the default case is now a shadow exception in case ANYTHING
					goes wrong with the shadow exit status */
				if ( (WEXITSTATUS(status) != DPRINTF_ERROR) &&
					(WEXITSTATUS(status) != JOB_EXCEPTION) )
				{
					dprintf( D_ALWAYS,
							 "ERROR: %s exited with unknown value!\n",
							 name );
				}
				// record that we had an exception.  This function will
				// relinquish the match if we get too many
				// exceptions 
				if( !srec->removed && srec->match ) {
					HadException(srec->match);
				}
				break;
			}
	 	} else if( WIFSIGNALED(status) ) {
			dprintf( D_FAILURE|D_ALWAYS, "%s pid %d died with %s\n",
					 name, pid, daemonCore->GetExceptionString(status) );
		}
		delete_shadow_rec( pid );
	} else {
		// mrec and srec are NULL - agent dies after deleting match
		StartJobsFlag=FALSE;
	 }  // big if..else if...

		// If we're not trying to shutdown, now that either an agent
		// or a shadow (or both) have exited, we should try to
		// activate all our claims and start jobs on them.
	if( ! ExitWhenDone ) {
		if (StartJobsFlag) StartJobs();
	}
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
		reason = ExplainPolicyDecision(policy,job_ad);
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
	SetAttributeInt( job_id->cluster, job_id->proc,
					 ATTR_CURRENT_HOSTS, 0 ); 

	switch( status ) {
	case RUNNING:
		kill_zombie( pid, job_id );
		break;
	case HELD:
		if( !scheduler.WriteHoldToUserLog(*job_id)) {
			dprintf( D_ALWAYS, 
					 "Failed to write hold event to the user log\n" ); 
		}
		handle_mirror_job_notification(
					GetJobAd(job_id->cluster,job_id->proc),
					status, *job_id);
		break;
	case REMOVED:
		if( !scheduler.WriteAbortToUserLog(*job_id)) {
			dprintf( D_ALWAYS, 
					 "Failed to write abort event to the user log\n" ); 
		}
			// No break, fall through and do the deed...
	case COMPLETED:
		handle_mirror_job_notification(
					GetJobAd(job_id->cluster,job_id->proc),
					status, *job_id);
		DestroyProc( job_id->cluster, job_id->proc );
		break;
	default:
		break;
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
	char job_version[150];
	job_version[0] = '\0';
	
	if (GetAttributeString(cluster, proc, ATTR_VERSION, job_version) == 0) {
		CondorVersionInfo ver(job_version, "JOB");
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
    char	 ckpt_name[MAXPATHLEN];
	char	buf[_POSIX_PATH_MAX];
	char	server[_POSIX_PATH_MAX];
	int		universe = CONDOR_UNIVERSE_STANDARD;

		/* In order to remove from the checkpoint server, we need to know
		 * the owner's name.  If not passed in, look it up now.
  		 */
	if ( owner == NULL ) {
		if ( GetAttributeString(cluster,proc,ATTR_OWNER,buf) < 0 ) {
			dprintf(D_ALWAYS,"ERROR: cleanup_ckpt_files(): cannot determine owner for job %d.%d\n",cluster,proc);
		} else {
			owner = buf;
		}
	}

	// only need to contact the ckpt server for standard universe jobs
	GetAttributeInt(cluster,proc,ATTR_JOB_UNIVERSE,&universe);
	if (universe == CONDOR_UNIVERSE_STANDARD) {
		if (GetAttributeString(cluster, proc, ATTR_LAST_CKPT_SERVER,
							   server) == 0) {
			SetCkptServerHost(server);
		} else {
			SetCkptServerHost(NULL); // no ckpt on ckpt server
		}
	}

		  /* Remove any checkpoint files.  If for some reason we do 
		 * not know the owner, don't bother sending to the ckpt
		 * server.
		 */
	strcpy(ckpt_name, gen_ckpt_name(Spool,cluster,proc,0) );
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
	strcat( ckpt_name, ".tmp");
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


int pidHash(const int &pid, int numBuckets)
{
	return pid % numBuckets;
}


int procIDHash(const PROC_ID &procID, int numBuckets)
{
	return ( (procID.cluster+(procID.proc*19)) % numBuckets );
}

// initialize the configuration parameters and classad.  Since we call
// this again when we reconfigure, we have to be careful not to leak
// memory. 
void
Scheduler::Init()
{
	char*					tmp;
	char					expr[1024];
	static	int				schedd_name_in_config = 0;
	static  bool			first_time_in_init = true;

		////////////////////////////////////////////////////////////////////
		// Grab all the essential parameters we need from the config file.
		////////////////////////////////////////////////////////////////////

	if( Spool ) free( Spool );
	if( !(Spool = param("SPOOL")) ) {
		EXCEPT( "No spool directory specified in config file" );
	}


	if ( Collectors ) delete ( Collectors );
	Collectors = CollectorList::create();

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

#define SCHEDD_NAME_LHS "SCHEDD_NAME="
	
	int lhs_length = strlen(SCHEDD_NAME_LHS);

	if ( NameInEnv == NULL || strcmp(&NameInEnv[lhs_length],Name) ) {
		NameInEnv = (char *)malloc(lhs_length +strlen(Name)+1);
		sprintf(NameInEnv, SCHEDD_NAME_LHS "%s", Name);
		if (putenv(NameInEnv) < 0) {
			dprintf(D_ALWAYS, "putenv(\"%s\") failed!\n", NameInEnv);
		}
	}


	if( AccountantName ) free( AccountantName );
	if( ! (AccountantName = param("ACCOUNTANT_HOST")) ) {
		dprintf( D_FULLDEBUG, "No Accountant host specified in config file\n" );
	}

	if( JobHistoryFileName ) free( JobHistoryFileName );
	if( ! (JobHistoryFileName = param("HISTORY")) ) {
		  dprintf(D_FULLDEBUG, "No history file specified in config file\n" );
	}

	 tmp = param( "SCHEDD_INTERVAL" );
	 if( ! tmp ) {
		  SchedDInterval = 300;
	 } else {
		  SchedDInterval = atoi( tmp );
		  free( tmp );
	 }

	SchedDMinInterval = param_integer("SCHEDD_MIN_INTERVAL",5);

	tmp = param( "QUEUE_CLEAN_INTERVAL" );
	if( ! tmp ) {
		QueueCleanInterval = 24*60*60;	// every 24 hours
	} else {
		QueueCleanInterval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "WALL_CLOCK_CKPT_INTERVAL" );
	if( ! tmp ) {
		WallClockCkptInterval = 60*60;	// every hour
	} else {
		WallClockCkptInterval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "JOB_START_DELAY" );
	if( ! tmp ) {
		JobStartDelay = 2;
	} else {
		JobStartDelay = atoi( tmp );
		free( tmp );
	}
	
	JobStartCount =	param_integer(
						"JOB_START_COUNT",			// name
						DEFAULT_JOB_START_COUNT,	// default value
						DEFAULT_JOB_START_COUNT		// min value
					);

	JobsThisBurst = -1;

	tmp = param( "MAX_JOBS_RUNNING" );
	if( ! tmp ) {
		MaxJobsRunning = 200;
	} else {
		MaxJobsRunning = atoi( tmp );
		free( tmp );
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
	shadowsByPid = new HashTable <int, shadow_rec *>((int)(MaxJobsRunning*1.2),
													  pidHash);
	shadowsByProcID =
		new HashTable<PROC_ID, shadow_rec *>((int)(MaxJobsRunning*1.2),
											 procIDHash);
	resourcesByProcID = 
		new HashTable<PROC_ID, ClassAd *>((int)(MaxJobsRunning*1.2),
											 procIDHash,
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
		FlockNegotiators->init( DT_NEGOTIATOR, flock_negotiator_hosts );
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

	tmp = param( "RESERVED_SWAP" );
	 if( !tmp ) {
		  ReservedSwap = 5 * 1024;				/* 5 megabytes */
	 } else {
		  ReservedSwap = atoi( tmp ) * 1024; /* Value specified in megabytes */
		  free( tmp );
	 }

	tmp = param( "SHADOW_SIZE_ESTIMATE" );
	 if( ! tmp ) {
		  ShadowSizeEstimate = DEFAULT_SHADOW_SIZE;
	 } else {
		  ShadowSizeEstimate = atoi( tmp );	/* Value specified in kilobytes */
		  free( tmp );
	 }

	tmp = param("ALIVE_INTERVAL");
		// Don't allow the user to specify an alive interval larger
		// than leaseAliveInterval, or the startd may start killing off
		// jobs before ATTR_JOB_LEASE_DURATION has passed, thereby screwing
		// up the operation of the disconnected shadow/starter feature.
	 if(!tmp) {
		alive_interval = MIN(leaseAliveInterval,300);
	 } else {
		  alive_interval = MIN(atoi(tmp),leaseAliveInterval);
		  free(tmp);
	}


	tmp = param("MAX_SHADOW_EXCEPTIONS");
	 if(!tmp) {
		MaxExceptions = 5;
	 } else {
		  MaxExceptions = atoi(tmp);
		  free(tmp);
	}

	 tmp = param("MANAGE_BANDWIDTH");
	 if (!tmp) {
		 ManageBandwidth = false;
	 } else {
		 if (tmp[0] == 'T' || tmp[0] == 't') {
			 ManageBandwidth = true;
		 } else {
			 ManageBandwidth = false;
		 }
		 free(tmp);
	 }

	tmp = param("PERIODIC_EXPR_INTERVAL");
	if(!tmp) {
		PeriodicExprInterval = 300;
	} else {
		PeriodicExprInterval = atoi(tmp);
		free(tmp);
		tmp = NULL;
	}

	if ( first_time_in_init ) {	  // cannot be changed on the fly
		tmp = param( "GRIDMANAGER_PER_JOB" );
		if( !tmp || tmp[0] == 'f' || tmp[0] == 'F' ) {
			gridman_per_job = false;
		} else {
			gridman_per_job = true;
		}
		if( tmp ) free( tmp );
	}

	tmp = param("REQUEST_CLAIM_TIMEOUT");
	if(!tmp) {
		RequestClaimTimeout = 60*30; //30 minutes by default
	} else {
		RequestClaimTimeout = atoi(tmp);
		free(tmp);
		tmp = NULL;
	}


	int int_val = param_integer( "JOB_IS_FINISHED_INTERVAL", 0, 0 );
	job_is_finished_queue.setPeriod( int_val );	


		////////////////////////////////////////////////////////////////////
		// Initialize the queue managment code
		////////////////////////////////////////////////////////////////////

	InitQmgmt();


		//////////////////////////////////////////////////////////////
		// Initialize our classad
		//////////////////////////////////////////////////////////////
	if( ad ) delete ad;
	ad = new ClassAd();

	ad->SetMyTypeName(SCHEDD_ADTYPE);
	ad->SetTargetTypeName("");

	config_fill_ad( ad );

		// Throw name and machine into the classad.
	sprintf( expr, "%s = \"%s\"", ATTR_NAME, Name );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() ); 
	ad->Insert(expr);

		// Put in our sinful string.  Note, this is never going to
		// change, so we only need to initialize it once.
	if( ! MySockName ) {
		MySockName = strdup( daemonCore->InfoCommandSinfulString() );
	}
	sprintf( expr, "%s = \"%s\"", ATTR_SCHEDD_IP_ADDR, MySockName );
	ad->Insert(expr);

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

		char nameBuf[50];
		sprintf( nameBuf, "<%s:%d>", inet_ntoa(*(my_sin_addr())),
				 shadowCommandrsock->get_port());
		MyShadowSockName = strdup( nameBuf );

		sent_shadow_failure_email = FALSE;
	}
		
		// initialize our ShadowMgr, too.
	shadow_mgr.init();

		// Startup the cron logic (only do it once, though)
	if ( ! CronMgr ) {
		CronMgr = new ScheddCronMgr( );
		CronMgr->Initialize( );
	}

	first_time_in_init = false;
}

void
Scheduler::Register()
{
	 // message handlers for schedd commands
	 daemonCore->Register_Command( NEGOTIATE, "NEGOTIATE", 
		 (CommandHandlercpp)&Scheduler::doNegotiate, "negotiate", 
		 this, NEGOTIATOR );
	 daemonCore->Register_Command( RESCHEDULE, "RESCHEDULE", 
			(CommandHandlercpp)&Scheduler::reschedule_negotiator, 
			"reschedule_negotiator", this, WRITE);
	 daemonCore->Register_Command( RECONFIG, "RECONFIG", 
			(CommandHandler)&dc_reconfig, "reconfig", 0, OWNER );
	 daemonCore->Register_Command(VACATE_SERVICE, "VACATE_SERVICE", 
			(CommandHandlercpp)&Scheduler::vacate_service, 
			"vacate_service", this, WRITE);
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


	 // reaper
	daemonCore->Register_Reaper( "reaper",
                                 (ReaperHandlercpp)&Scheduler::child_exit,
								 "child_exit", this );

	// register all the timers
	RegisterTimers();

	// Now is a good time to instantiate the GridUniverse
	_gridlogic = new GridUniverseLogic;
}

void
Scheduler::RegisterTimers()
{
	static int cleanid = -1, wallclocktid = -1, periodicid=-1;
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
		wallclocktid = daemonCore->Register_Timer(WallClockCkptInterval,
												  WallClockCkptInterval,
												  (Event)&CkptWallClock,
												  "CkptWallClock");
	} else {
		wallclocktid = -1;
	}

	if (PeriodicExprInterval>0) {
		periodicid = daemonCore->Register_Timer(PeriodicExprInterval,PeriodicExprInterval,
			(Eventcpp)&Scheduler::PeriodicExprHandler,"PeriodicExpr",this);
	} else {
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


void
Scheduler::reconfig()
{
	Init();
	RegisterTimers();			// reset timers
	if ( autocluster.config() ) {
		// clear out auto cluster id attributes
		WalkJobQueue( (int(*)(ClassAd *))clear_autocluster_id );
	}
	timeout();
}

// This function is called by a timer when we are shutting down
void
Scheduler::attempt_shutdown()
{
	if ( numShadows ) {
		preempt(1);
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
	daemonCore->Register_Timer( 0, MAX(JobStartDelay,1), 
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
	shadow_rec *rec;
	int sig;
	shadowsByPid->startIterations();
	while( shadowsByPid->iterate(rec) == 1 ) {
		if(	rec->universe == CONDOR_UNIVERSE_LOCAL ) { 
			sig = DC_SIGHARDKILL;
		} else {
			sig = SIGKILL;
		}
		daemonCore->Send_Signal( rec->pid, sig );
		rec->preempted = TRUE;
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

	dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::invalidate_ads()
{
	int i;
	char line[256];

		// The ClassAd we need to use is totally different from the
		// regular one, so just delete it and start over again.
	delete ad;
	ad = new ClassAd;
    ad->SetMyTypeName( QUERY_ADTYPE );
    ad->SetTargetTypeName( SCHEDD_ADTYPE );

        // Invalidate the schedd ad
    sprintf( line, "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, 
             Name );
    ad->Insert( line );


		// Update collectors
	Collectors->sendUpdates ( INVALIDATE_SCHEDD_ADS, ad );

	if (N_Owners == 0) return;	// no submitter ads to invalidate

		// Invalidate all our submittor ads.
	sprintf( line, "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_SCHEDD_NAME,
			 Name );
    ad->InsertOrUpdate( line );

	Collectors->sendUpdates ( INVALIDATE_SUBMITTOR_ADS, ad );


	Daemon* d;
	if( FlockCollectors && FlockLevel > 0 ) {
		for( i=1, FlockCollectors->rewind();
			 i <= FlockLevel && FlockCollectors->next(d); i++ ) {
			((DCCollector*)d)->sendUpdate( INVALIDATE_SUBMITTOR_ADS, ad );
		}
	}
}


void
Scheduler::reschedule_negotiator(int, Stream *)
{
		// don't bother the negotiator if we are shutting down
	if ( ExitWhenDone ) {
		return;
	}

	timeout();							// update the central manager now

	dprintf( D_ALWAYS, "Called reschedule_negotiator()\n" );

	sendReschedule();

	if( LocalUniverseJobsIdle > 0 || SchedUniverseJobsIdle > 0 ) {
		StartLocalJobs();
	}

	 return;
}


void
Scheduler::sendReschedule( void )
{
	dprintf( D_FULLDEBUG, "Sending RESCHEDULE command to negotiator(s)\n" );

	Daemon negotiator(DT_NEGOTIATOR);
	if( !negotiator.sendCommand(RESCHEDULE, Stream::safe_sock, NEGOTIATOR_CONTACT_TIMEOUT) ) {
		dprintf( D_ALWAYS,
				 "failed to send RESCHEDULE command to negotiator\n" );
		return;
	}

	Daemon* d;
	if( FlockNegotiators ) {
		FlockNegotiators->rewind();
		FlockNegotiators->next( d );
		for( int i=0; d && i<FlockLevel; FlockNegotiators->next(d), i++ ) {
			if( !d->sendCommand(RESCHEDULE, Stream::safe_sock, 
								NEGOTIATOR_CONTACT_TIMEOUT) ) {
				dprintf( D_ALWAYS, 
						 "failed to send RESCHEDULE command to %s: %s\n",
						 d->name(), d->error() );
			}
		}
	}
}


match_rec*
Scheduler::AddMrec(char* id, char* peer, PROC_ID* jobId, const ClassAd* my_match_ad,
				   char *user, char *pool)
{
	match_rec *rec;
	char* addr;

	if(!id || !peer)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not added\n"); 
		return NULL;
	} 
	rec = new match_rec(id, peer, jobId, my_match_ad, user, pool);
	if(!rec)
	{
		EXCEPT("Out of memory!");
	} 

	// spit out a warning and return NULL if we already have this mrec
	match_rec *tempRec;
	if( matches->lookup( HashKey( id ), tempRec ) == 0 ) {
		dprintf( D_ALWAYS,
				 "attempt to add pre-existing match \"%s\" ignored\n",
				 id ? id : "(null)" );
		delete rec;
		return NULL;
	}
	if( matches->insert( HashKey( id ), rec ) != 0 ) {
		dprintf( D_ALWAYS, "match \"%s\" insert failed\n",
				 id ? id : "(null)" );
		delete rec;
		return NULL;
	}
	numMatches++;

		/*
		  Finally, we want to tell daemonCore that we're willing to
		  grant WRITE permission to whatever machine we were matched
		  with.  This greatly simplifies DaemonCore permission stuff
		  for flocking, since submitters don't have to know all the
		  hosts they might possibly run on, all they have to do is
		  trust the central managers of all the pools they're flocking
		  to (which they have to do, already).  Added on 7/13/00 by
		  Derek Wright <wright@cs.wisc.edu>
		*/
	if( (addr = string_to_ipstr(peer)) ) {
		daemonCore->AddAllowHost( addr, WRITE );
		daemonCore->AddAllowHost( addr, DAEMON );
	} else {
		dprintf( D_ALWAYS, "ERROR: Can't convert \"%s\" to an IP address!\n", 
				 peer );
	}
	return rec;
}

int
Scheduler::DelMrec(char* id)
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

	// release the claim on the startd
	send_vacate(rec, RELEASE_CLAIM);

	dprintf( D_ALWAYS, "Match record (%s, %d, %d) deleted\n",
			 rec->peer, rec->cluster, rec->proc ); 
	dprintf( D_FULLDEBUG, "ClaimId of deleted match: %s\n", rec->id );
	
	matches->remove(key);
		// Remove this match from the associated shadowRec.
	if (rec->shadowRec)
		rec->shadowRec->match = NULL;
	delete rec;
	
	numMatches--; 
	return 0;
}


int
Scheduler::DelMrec(match_rec* match)
{
	if(!match)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not deleted\n");
		return -1;
	}
	return DelMrec(match->id);
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

/*
 * Weiru
 * Inform the startd and the accountant of the relinquish of the resource.
 */
void
Scheduler::Relinquish(match_rec* mrec)
{
	// SC2000
	//ReliSock	*sock;
	SafeSock	*sock;
	int				flag = FALSE;

	if (!mrec) {
		dprintf(D_ALWAYS, "Scheduler::Relinquish - mrec is NULL, can't relinquish\n");
		return;
	}

	// inform the startd

	DCStartd d( mrec->peer );
	sock = (SafeSock*)d.startCommand(RELINQUISH_SERVICE, Stream::safe_sock, STARTD_CONTACT_TIMEOUT);

	if(!sock) {
		dprintf(D_ALWAYS, "Can't connect to startd %s\n", mrec->peer);
	}
	else if(!sock->put(mrec->id) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Can't relinquish startd. Match record is:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
	}
	else
	{
		dprintf(D_PROTOCOL,"## 7. Relinquished startd. Match record is:\n");
		dprintf(D_PROTOCOL, "\t%s\t%s\n", mrec->id,	mrec->peer);
		flag = TRUE;
	}
	delete sock;

// SC2000 - ifdef'd to 0 because there is no accountant, so just skip over
// all this stuff for now. Todd / Erik Nov 22 2000
#if 0
	// inform the accountant
	if(!AccountantName)
	{
		dprintf(D_PROTOCOL, "## 7. No accountant to relinquish\n");
	}
	else
	{
		Daemon d (AccountantName);
		sock = d.startCommand (RELINQUISH_SERVICE,
				Stream::reli_sock,
				NEGOTIATOR_CONTACT_TIMEOUT);

		sock->timeout(NEGOTIATOR_CONTACT_TIMEOUT);
		sock->encode();
		if(!sock) {
			dprintf(D_ALWAYS,"Can't connect to accountant %s\n",
					AccountantName);
		}
		else if(!sock->code(MySockName))
		{
			dprintf(D_ALWAYS,"Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
		}
		else if(!sock->put(mrec->id))
		{
			dprintf(D_ALWAYS,"Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
		}
		else if(!sock->put(mrec->peer) || !sock->eom())
		// This is not necessary to send except for being an extra checking
		// because ClaimId uniquely identifies a match.
		{
			dprintf(D_ALWAYS,"Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
		}
		else
		{
			dprintf(D_PROTOCOL,"## 7. Relinquished acntnt. Match record is:\n");
			dprintf(D_PROTOCOL, "\t%s\t%s\n", mrec->id,	mrec->peer);
			flag = TRUE;
		}
		delete sock;
	}
#endif 

	if(flag)
	{
		dprintf(D_PROTOCOL, "## 7. Successfully relinquished match:\n");
		dprintf(D_PROTOCOL, "\t%s\t%s\n", mrec->id,	mrec->peer);
	}
	else
	{
		dprintf(D_ALWAYS, "Couldn't relinquish match:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
	}
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
			rec->cluster = rec->origcluster; 
			rec->proc = -1;
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
	match_rec *rec;

	if (GetAttributeInt(id->cluster, id->proc,
						ATTR_JOB_UNIVERSE, &universe) < 0) {
		dprintf(D_FULLDEBUG, "GetAttributeInt() failed\n");
		return FALSE;
	}

	if ( (universe == CONDOR_UNIVERSE_PVM) ||
		 (universe == CONDOR_UNIVERSE_MPI) ||
		 (universe == CONDOR_UNIVERSE_GLOBUS) ||
		 (universe == CONDOR_UNIVERSE_PARALLEL) )
		return FALSE;

	matches->startIterations();
	while (matches->iterate(rec) == 1) {
		if(rec->cluster == id->cluster && rec->proc == id->proc) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * go through match reords and send alive messages to all the startds.
 */

bool
sendAlive( match_rec* mrec )
{
	SafeSock	sock;
	char		*id = NULL;
	
    sock.timeout(STARTD_CONTACT_TIMEOUT);
	sock.encode();

	DCStartd d( mrec->peer );
	id = mrec->id;

	dprintf (D_PROTOCOL,"## 6. Sending alive msg to %s\n", id);

	if( !sock.connect(mrec->peer) || !d.startCommand ( ALIVE, &sock) ||
	    !sock.code(id) || !sock.end_of_message()) {
			// UDP transport out of buffer space!
		dprintf(D_ALWAYS, "\t(Can't send alive message to %s)\n",
				mrec->peer);
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
			SetAttributeInt( mrec->cluster, mrec->proc, 
							 ATTR_LAST_JOB_LEASE_RENEWAL, now ); 
		}
	}
	CommitTransaction();

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


void
job_prio(ClassAd *ad)
{
	scheduler.JobsRunning += get_job_prio(ad);
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
		Relinquish(mrec);
		DelMrec(mrec);
	}
}


int
Scheduler::dumpState(int, Stream* s) {

	dprintf ( D_FULLDEBUG, "Dumping state for Squawk\n" );

	ClassAd *ad = new ClassAd;
	intoAd ( ad, "MySockName", MySockName );
	intoAd ( ad, "MyShadowSockname", MyShadowSockName );
	intoAd ( ad, "SchedDInterval", SchedDInterval );
	intoAd ( ad, "QueueCleanInterval", QueueCleanInterval );
	intoAd ( ad, "JobStartDelay", JobStartDelay );
	intoAd ( ad, "JobStartCount", JobStartCount );
	intoAd ( ad, "JobsThisBurst", JobsThisBurst );
	intoAd ( ad, "MaxJobsRunning", MaxJobsRunning );
	intoAd ( ad, "MaxJobsSubmitted", MaxJobsSubmitted );
	intoAd ( ad, "JobsStarted", JobsStarted );
	intoAd ( ad, "SwapSpace", SwapSpace );
	intoAd ( ad, "ShadowSizeEstimate", ShadowSizeEstimate );
	intoAd ( ad, "SwapSpaceExhausted", SwapSpaceExhausted );
	intoAd ( ad, "ReservedSwap", ReservedSwap );
	intoAd ( ad, "JobsIdle", JobsIdle );
	intoAd ( ad, "JobsRunning", JobsRunning );
	intoAd ( ad, "LocalUniverseJobsIdle", LocalUniverseJobsIdle );
	intoAd ( ad, "LocalUniverseJobsRunning", LocalUniverseJobsRunning );
	intoAd ( ad, "SchedUniverseJobsIdle", SchedUniverseJobsIdle );
	intoAd ( ad, "SchedUniverseJobsRunning", SchedUniverseJobsRunning );
	intoAd ( ad, "BadCluster", BadCluster );
	intoAd ( ad, "BadProc", BadProc );
	intoAd ( ad, "N_Owners", N_Owners );
	intoAd ( ad, "LastTimeout", LastTimeout  );
	intoAd ( ad, "ExitWhenDone", ExitWhenDone );
	intoAd ( ad, "StartJobTimer", StartJobTimer );
	intoAd ( ad, "timeoutid", timeoutid );
	intoAd ( ad, "startjobsid", startjobsid );
	intoAd ( ad, "startJobsDelayBit", startJobsDelayBit );
	intoAd ( ad, "num_reg_contacts", num_reg_contacts );
	intoAd ( ad, "MAX_STARTD_CONTACTS", MAX_STARTD_CONTACTS );
	intoAd ( ad, "CondorAdministrator", CondorAdministrator );
	intoAd ( ad, "Mail", Mail );
	intoAd ( ad, "filename", filename );
	intoAd ( ad, "AccountantName", AccountantName );
	intoAd ( ad, "UidDomain", UidDomain );
	intoAd ( ad, "MaxFlockLevel", MaxFlockLevel );
	intoAd ( ad, "FlockLevel", FlockLevel );
	intoAd ( ad, "alive_interval", alive_interval );
	intoAd ( ad, "leaseAliveInterval", leaseAliveInterval );
	intoAd ( ad, "MaxExceptions", MaxExceptions );
	
	int cmd;
	s->code( cmd );
	s->eom();

	s->encode();
	
	ad->put( *s );

	delete ad;

	return TRUE;
}

int Scheduler::intoAd( ClassAd *ad, char *lhs, char *rhs ) {
	char tmp[16000];
	
	if ( !lhs || !rhs || !ad ) {
		return FALSE;
	}

	sprintf ( tmp, "%s = \"%s\"", lhs, rhs );
	ad->Insert( tmp );
	
	return TRUE;
}

int Scheduler::intoAd( ClassAd *ad, char *lhs, int rhs ) {
	char tmp[256];
	if ( !lhs || !ad ) {
		return FALSE;
	}

	sprintf ( tmp, "%s = %d", lhs, rhs );
	ad->Insert( tmp );

	return TRUE;
}


int
fixAttrUser( ClassAd *job )
{
	int nice_user = 0;
	char owner[_POSIX_PATH_MAX];
	char user[_POSIX_PATH_MAX];
	owner[0] = '\0';
	
	if( ! job->LookupString(ATTR_OWNER, owner) ) {
			// No ATTR_OWNER!
		return 0;
	}
		// if it's not there, nice_user will remain 0
	job->LookupInteger( ATTR_NICE_USER, nice_user );

	sprintf( user, "%s = \"%s%s@%s\"", ATTR_USER, 
			 (nice_user) ? "nice-user." : "", owner,
			 scheduler.uidDomain() );  
	job->Insert( user );
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
			// how evil, this allocates me a string, even if it failed...
		free( value );
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

	if( reason ) {
		MyString fixed_reason;
		if( reason[0] == '"' ) {
			fixed_reason += reason;
		} else {
			fixed_reason += '"';
			fixed_reason += reason;
			fixed_reason += '"';
		}
		if( SetAttribute(cluster, proc, ATTR_REMOVE_REASON, 
						 fixed_reason.Value()) < 0 ) {
			dprintf( D_ALWAYS, "WARNING: Failed to set %s to \"%s\" for "
					 "job %d.%d\n", ATTR_REMOVE_REASON, reason, cluster,
					 proc );
		}
	}

	if( SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, REMOVED) < 0 ) {
		dprintf(D_ALWAYS,"Couldn't change state of job %d.%d\n",cluster,proc);
		return false;
	}

	// Add the remove reason to the job's attributes
	if ( reason && *reason ) {
		SetAttributeString( cluster, proc, ATTR_REMOVE_REASON, reason );
	}

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
			// MATT: What happens if these fail?
		ad->LookupInteger(ATTR_CLUSTER_ID, jobs[job_count].cluster);
		ad->LookupInteger(ATTR_PROC_ID, jobs[job_count].proc);

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

		char id_buf[64];
		sprintf( id_buf, "%d.%d", cluster, proc );
		MyString msg_buf;
		msg_buf += "Condor job ";
		msg_buf += id_buf;
		msg_buf += " has been put on hold.\n";
		msg_buf += reason;
		msg_buf += "\nPlease correct this problem and release the "
			"job with \"condor_release\"\n";

		MyString msg_subject;
		msg_subject += "Condor job ";
		msg_subject += id_buf;
		msg_subject += " put on hold";

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

		char id_buf[64];
		sprintf( id_buf, "%d.%d", cluster, proc );
		MyString msg_buf;
		msg_buf += "Condor job ";
		msg_buf += id_buf;
		msg_buf += " has been released from being on hold.\n";
		msg_buf += reason;

		MyString msg_subject;
		msg_subject += "Condor job ";
		msg_subject += id_buf;
		msg_subject += " released from hold state";

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

	dprintf( D_FULLDEBUG, "start next job after %d sec, JobsThisBurst %d\n",
			delay, JobsThisBurst);
	return delay;
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
	if( job_is_finished_queue.isMember(id) ) { 
		dprintf( D_FULLDEBUG, "enqueueFinishedJob(): job %d.%d already "
				 "in queue to run jobIsFinished()\n", cluster, proc );
		delete id;
		id = NULL;
		return false;
	}
	dprintf( D_FULLDEBUG, "Job %d.%d is finished\n", cluster, proc );
	return job_is_finished_queue.enqueue( id );
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
