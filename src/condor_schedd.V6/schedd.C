/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "sched.h"
#include "dedicated_scheduler.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "proc.h"
#include "exit.h"
#include "condor_collector.h"
#include "scheduler.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_adtypes.h"
#include "condor_string.h"
#include "condor_email.h"
#include "environ.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"
#include "renice_self.h"
#include "user_log.c++.h"
#include "access.h"
#include "internet.h"
#include "condor_ckpt_name.h"
#include "../condor_ckpt_server/server_interface.h"
#include "generic_query.h"
#include "directory.h"
#include "condor_ver_info.h"
#include "grid_universe.h"
#include "globus_utils.h"
#include "env.h"

#define DEFAULT_SHADOW_SIZE 125

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
shadow_rec * add_shadow_rec(int, PROC_ID*, match_rec*, int);
bool service_this_universe(int);

int	WallClockCkptInterval = 0;

#ifdef CARMI_OPS
struct shadow_rec *find_shadow_by_cluster( PROC_ID * );
#endif

#ifdef WIN32	 // on unix, this is in instantiate.C
bool operator==(const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}
#endif

int
dc_reconfig()
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGHUP );
	return TRUE;
}


match_rec::match_rec(char* i, char* p, PROC_ID* id, ClassAd *match, 
					 char *the_user, char *my_pool)
{
	strcpy(this->id, i);
	strcpy(peer, p);
	origcluster = cluster = id->cluster;
	proc = id->proc;
	status = M_UNCLAIMED;
	entered_current_status = (int)time(0);
	shadowRec = NULL;
	alive_countdown = 0;
	num_exceptions = 0;
	if( match ) {
		my_match_ad = new ClassAd( *match );
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


ContactStartdArgs::ContactStartdArgs( char* the_capab, char* the_owner,  
									  char* the_sinful, PROC_ID the_id, 
									  ClassAd* match, char* the_pool, 
									  bool is_dedicated ) 
{
	csa_capability = strdup( the_capab );
	csa_owner = strdup( the_owner );
	csa_sinful = strdup( the_sinful );
	csa_id.cluster = the_id.cluster;
	csa_id.proc = the_id.proc;
	if( match ) {
		csa_match_ad = new ClassAd( *match );
	} else {
		csa_match_ad = NULL;
	}
	if( the_pool ) {
		csa_pool = strdup( the_pool );
	} else {
		csa_pool = NULL;
	}
	csa_is_dedicated = is_dedicated;
}


ContactStartdArgs::~ContactStartdArgs()
{
	free( csa_capability );
	free( csa_owner );
	free( csa_sinful );
	if( csa_pool ) {
		free( csa_pool );
	}
	if( csa_match_ad ) {
		delete( csa_match_ad );
	}
}


Scheduler::Scheduler()
{
	ad = NULL;
	MySockName = NULL;
	MyShadowSockName = NULL;
	shadowCommandrsock = NULL;
	shadowCommandssock = NULL;
	SchedDInterval = 0;
	QueueCleanInterval = 0; JobStartDelay = 0;
	MaxJobsRunning = 0;
	NegotiateAllJobsInCluster = false;
	JobsStarted = 0;
	JobsIdle = 0;
	JobsRunning = 0;
	JobsHeld = 0;
	JobsFlocked = 0;
	JobsRemoved = 0;
	SchedUniverseJobsIdle = 0;
	SchedUniverseJobsRunning = 0;
	ReservedSwap = 0;
	SwapSpace = 0;

	ShadowSizeEstimate = 0;

	N_RejectedClusters = 0;
	N_Owners = 0;
	LastTimeout = time(NULL);
	CondorViewHost = NULL;
	Collector = NULL;
	Negotiator = NULL;
	Shadow = NULL;
	CondorAdministrator = NULL;
	Mail = NULL;
	filename = NULL;
	alive_interval = 0;
	ExitWhenDone = FALSE;
	matches = NULL;
	shadowsByPid = NULL;

	shadowsByProcID = NULL;
	numMatches = 0;
	numShadows = 0;
	IdleSchedUniverseJobIDs = NULL;
	FlockCollectors = FlockNegotiators = FlockViewServers = NULL;
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
	sent_shadow_failure_email = FALSE;
	ManageBandwidth = false;
	RejectedClusters.setFiller(0);
	RejectedClusters.resize(MAX_REJECTED_CLUSTERS);
	_gridlogic = NULL;
}


Scheduler::~Scheduler()
{
	delete ad;
	if (MySockName)
		free(MySockName);
	if (MyShadowSockName)
		free(MyShadowSockName);
	if ( shadowCommandrsock ) {
		if( daemonCore ) {
			daemonCore->Cancel_Socket(shadowCommandrsock);
		}
		delete shadowCommandrsock;
	}
	if ( shadowCommandssock ) {
		if( daemonCore ) {
			daemonCore->Cancel_Socket(shadowCommandssock);
		}
		delete shadowCommandssock;
	}
	if (Collector)
		delete(Collector);
	if (Negotiator)
		delete(Negotiator);
	if (CondorViewHost)
		free(CondorViewHost);
	if (Shadow)
		free(Shadow);
	if (CondorAdministrator)
		free(CondorAdministrator);
	if (Mail)
		free(Mail);
	delete []filename;
	if (matches) {
		matches->startIterations();
		match_rec *rec;
		HashKey cap;
		while (matches->iterate(cap, rec) == 1) {
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
	if (shadowsByProcID) {
		delete shadowsByProcID;
	}
	if (FlockCollectors) delete FlockCollectors;
	if (FlockNegotiators) delete FlockNegotiators;
	if (FlockViewServers) delete FlockViewServers;
	if ( checkContactQueue_tid != -1 && daemonCore ) {
		daemonCore->Cancel_Timer(checkContactQueue_tid);
	}

	int i;
	for( i=0; i<N_Owners; i++) {
		if( Owners[i].Name ) { 
			free( Owners[i].Name );
		}
	}

	if (_gridlogic)
		delete _gridlogic;
}

void
Scheduler::timeout()
{
	count_jobs();

	clean_shadow_recs();	

	/* Call preempt() if we are running more than max jobs; however, do not
	 * call preempt() here if we are shutting down.  When shutting down, we have
	 * a timer which is progressively preempting just one job at a time.
	 */

	int real_jobs = numShadows - SchedUniverseJobsRunning;
	if( (real_jobs > MaxJobsRunning) && (!ExitWhenDone) ) {
		dprintf( D_ALWAYS, 
				 "Preempting %d jobs due to MAX_JOBS_RUNNING change\n",
				 (real_jobs - MaxJobsRunning) );
		preempt( real_jobs - MaxJobsRunning );
	}

	/* Reset our timer */
	daemonCore->Reset_Timer(timeoutid,SchedDInterval);
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
	 OwnerData OldOwners[MAX_NUM_OWNERS];
	 memcpy(OldOwners, Owners, N_Owners*sizeof(OwnerData));
	 int Old_N_Owners=N_Owners;

	N_Owners = 0;
	JobsRunning = 0;
	JobsIdle = 0;
	JobsHeld = 0;
	JobsFlocked = 0;
	JobsRemoved = 0;
	SchedUniverseJobsIdle = 0;
	SchedUniverseJobsRunning = 0;

	// clear owner table contents
	time_t current_time = time(0);
	for ( i=0; i<MAX_NUM_OWNERS; i++) {
		Owners[i].Name = NULL;
		Owners[i].JobsRunning = 0;
		Owners[i].JobsIdle = 0;
		Owners[i].JobsHeld = 0;
		Owners[i].JobsFlocked = 0;
		Owners[i].FlockLevel = 0;
		Owners[i].OldFlockLevel = 0;
		Owners[i].GlobusJobs = 0;
		Owners[i].GlobusUnsubmittedJobs = 0;
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
	sprintf(tmp, "%s = %d", ATTR_TOTAL_HELD_JOBS, JobsHeld);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_FLOCKED_JOBS, JobsFlocked);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_REMOVED_JOBS, JobsRemoved);
	ad->Insert (tmp);

		// Tell negotiator to send us the startd ad
	sprintf(tmp, "%s = True", ATTR_WANT_RESOURCE_AD );
	ad->InsertOrUpdate(tmp);

		// Port doesn't matter, since we've got the sinful string. 
	updateCentralMgr( UPDATE_SCHEDD_AD, ad, Collector->addr(), 0 ); 
	dprintf( D_FULLDEBUG, 
			 "Sent HEART BEAT ad to central mgr: Number of submittors=%d\n",
			 N_Owners );

	// send the schedd ad to our flock collectors too, so we will
	// appear in condor_q -global and condor_status -schedd
	if (FlockCollectors) {
		FlockCollectors->rewind();
		char *host = FlockCollectors->next();
		for (i=0; host && i < FlockLevel; i++) {
			updateCentralMgr( UPDATE_SCHEDD_AD, ad, host,
							  COLLECTOR_UDP_COMM_PORT );
			host = FlockCollectors->next();
		}
	}

	// The per user queue ads should not have NumUsers in them --- only
	// the schedd ad should.  In addition, they should not have
	// TotalRunningJobs and TotalIdleJobs
	ad->Delete (ATTR_NUM_USERS);
	ad->Delete (ATTR_TOTAL_RUNNING_JOBS);
	ad->Delete (ATTR_TOTAL_IDLE_JOBS);
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

		  // Port doesn't matter, since we've got the sinful string. 
	  updateCentralMgr( UPDATE_SUBMITTOR_AD, ad, Collector->addr(), 0 ); 

	  dprintf( D_ALWAYS, "Sent ad to central manager for %s@%s\n", 
				Owners[i].Name, UidDomain );

	  // condor view uses the acct port - because the accountant today is not
	  // an independant daemon. In the future condor view will be the
	  // accountant

		  // The CondorViewHost MAY BE NULL!!!!!  It's optional
		  // whether you define it or not.  This will cause a seg
		  // fault if we assume it's defined and use it.  
		  // -Derek Wright 11/4/98 
	  if( CondorViewHost && CondorViewHost[0] != '\0' ) {
		  updateCentralMgr( UPDATE_SUBMITTOR_AD, ad, CondorViewHost, 
							CONDOR_VIEW_PORT );
	  }
	}

	// update collector and condor-view server of the pools with which
	// we are flocking, if any
	if (FlockCollectors && FlockNegotiators) {
		FlockCollectors->rewind();
		FlockNegotiators->rewind();
		if (FlockViewServers) FlockViewServers->rewind();
		for (int flock_level = 1;
			 flock_level <= MaxFlockLevel; flock_level++) {
			char *flock_collector = FlockCollectors->next();
			char *flock_negotiator = FlockNegotiators->next();
			char *flock_view_server =
				(FlockViewServers) ? FlockViewServers->next() : NULL;
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
					!strcmp(rec->pool, flock_negotiator)) {
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
				updateCentralMgr( UPDATE_SUBMITTOR_AD, ad,
								  flock_collector,
								  COLLECTOR_UDP_COMM_PORT ); 
				if (flock_view_server && flock_view_server[0] != '\0') {
					updateCentralMgr( UPDATE_SUBMITTOR_AD, ad, 
									  flock_view_server,
									  CONDOR_VIEW_PORT );
				}
			}
		}
	}

	for (i=0; i < N_Owners; i++) {
		Owners[i].OldFlockLevel = Owners[i].FlockLevel;
	}

	 // Tell our GridUniverseLogic class what we've seen in terms
	 // of Globus Jobs per owner.
	for (i=0; i < N_Owners; i++) {
		if ( Owners[i].GlobusJobs > 0 ) {
			GridUniverseLogic::JobCountUpdate(Owners[i].Name,
				Owners[i].GlobusJobs,Owners[i].GlobusUnsubmittedJobs);
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
		FREE( OldOwners[i].Name );

		  // If k < N_Owners, we found this OldOwner in the current
		  // Owners table, therefore, we don't want to send the
		  // submittor ad with 0 jobs, so we continue to the next
		  // entry in the OldOwner table.
		if (k<N_Owners) continue;

	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  dprintf (D_ALWAYS, "Sent owner (0 jobs) ad to central manager\n");
		  // Port doesn't matter, since we've got the sinful string. 
	  updateCentralMgr( UPDATE_SUBMITTOR_AD, ad, Collector->addr(), 0 ); 

	  // also update all of the flock hosts
	  char *host;
	  int i;
	
	  if (FlockCollectors) {
		 for (i=1, FlockCollectors->rewind();
			  i <= OldOwners[i].OldFlockLevel &&
				  (host = FlockCollectors->next()); i++) {
			 updateCentralMgr( UPDATE_SUBMITTOR_AD, ad, host,
							   COLLECTOR_UDP_COMM_PORT );
		 }
	  }
	}

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
count( ClassAd *job )
{
	int		status;
	int		niceUser;
	char 	buf[100];
	char 	buf2[100];
	char*	owner;
	int		cur_hosts;
	int		max_hosts;
	int		universe;
	int		globus_status;

	if (job->LookupInteger(ATTR_JOB_STATUS, status) < 0) {
		dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				ATTR_JOB_STATUS);
		return 0;
	}
	if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) < 0) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) < 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) < 0) {
		universe = STANDARD;
	}
	
	// calculate owner for per submittor information.
	if (job->LookupString(ATTR_OWNER, buf) < 0) {
		dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				ATTR_OWNER);
		return 0;
	}
	owner = buf;
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

	// insert owner even if REMOVED or HELD for condor_q -{global|sub}
	int OwnerNum = scheduler.insert_owner( owner );

	if ( !service_this_universe(universe)  ) 
	{
		if ( universe == SCHED_UNIVERSE ) {
			// don't count REMOVED or HELD jobs
			if (status == IDLE || status == UNEXPANDED || status == RUNNING) {
				scheduler.SchedUniverseJobsRunning += cur_hosts;
				scheduler.SchedUniverseJobsIdle += (max_hosts - cur_hosts);
			}
		}
		if ( universe == GLOBUS_UNIVERSE ) {
			// for Globus, count jobs in G_UNSUBMITTED state by owner.
			// later we make certain there is a grid manager daemon
			// per owner.
			scheduler.Owners[OwnerNum].GlobusJobs++;
			globus_status = G_UNSUBMITTED;
			job->LookupInteger(ATTR_GLOBUS_STATUS, globus_status);
			if ( globus_status == G_UNSUBMITTED ) 
				scheduler.Owners[OwnerNum].GlobusUnsubmittedJobs++;
		}
			// We want to record the cluster id of all idle MPI jobs  
		if( universe == MPI && status == IDLE ) {
			int cluster = 0;
			job->LookupInteger( ATTR_CLUSTER_ID, cluster );
			dedicated_scheduler.addDedicatedCluster( cluster );
		}

		// bailout now, since all the crud below is only for jobs
		// which the schedd needs to service
		return 0;
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
service_this_universe(int universe)
{
	switch (universe) {
		case GLOBUS_UNIVERSE:
		case MPI:
		case SCHED_UNIVERSE:
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
	if ( N_Owners == MAX_NUM_OWNERS ) {
		EXCEPT( "Reached MAX_NUM_OWNERS" );
	}
	return i;
}

void
Scheduler::updateCentralMgr( int command, ClassAd* ca, char *host,
							 int port ) 
{
	// If the host we're given is NULL, just return, don't seg fault. 
	if( !host ) {
		return;
	}
	// If the ClassAd we're given is NULL, just return, don't seg fault. 
	if( !ca ) {
		return;
	}

	SafeSock sock;
	sock.timeout(NEGOTIATOR_CONTACT_TIMEOUT);
	sock.encode();
	if( !sock.connect(host, port) ||
		!sock.put(command) ||
		!ca->put(sock) ||
		!sock.end_of_message() ) {
		dprintf( D_ALWAYS, "failed to update central manager (%s)!\n",
				 host );
	}
}

static int IsSchedulerUniverse(shadow_rec* srec);

extern "C" {
void
abort_job_myself(PROC_ID job_id)
{
	shadow_rec *srec;
	int mode;

	// First check if there is a shadow assiciated with this process.
	// If so, send it SIGUSR,
	// but do _not_ call DestroyProc - we'll do that via the reaper
	// after the job has exited (and reported its final status to us).
	//
	// If there is no shadow, then simply call DestroyProc() (if we
	// are removing the job).

    dprintf ( D_FULLDEBUG, "abort_job_myself: %d.%d\n", 
              job_id.cluster, job_id.proc );

	if ( GetAttributeInt(job_id.cluster, job_id.proc, 
                         ATTR_JOB_STATUS, &mode) == -1 ) {
        dprintf ( D_ALWAYS, "tried to abort %d.%d; not found.\n", 
                  job_id.cluster, job_id.proc );
        return;
    }

	int job_universe = STANDARD;
	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_UNIVERSE,
					&job_universe);
	if (job_universe == PVM) {
		job_id.proc = 0;		// PVM shadow is always associated with proc 0
	}
	if (job_universe == GLOBUS_UNIVERSE) {
		// tell grid manager about the jobs removal, then return.
		char owner[_POSIX_PATH_MAX];
		owner[0] = '\0';
		GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);
		GridUniverseLogic::JobRemoved(owner);
		return;
	}

	if ((srec = scheduler.FindSrecByProcID(job_id)) != NULL) {

		// if we have already preempted this shadow, we're done.
		if ( srec->preempted ) {
			return;
		}

		// PVM jobs may not have a match  -Bin
		if (! IsSchedulerUniverse(srec)) {
            
                /* if there is a match printout the info */
			if (srec->match) {
				dprintf( D_FULLDEBUG,
                         "Found shadow record for job %d.%d, host = %s\n",
                         job_id.cluster, job_id.proc, srec->match->peer);
			} else {
				dprintf( D_FULLDEBUG, "This job does not have a match -- "
						 "It may be a PVM job.\n");
                dprintf(D_FULLDEBUG, "Found shadow record for job %d.%d\n",
                        job_id.cluster, job_id.proc);
            }
        
			if ( daemonCore->Send_Signal(srec->pid,DC_SIGUSR1)==FALSE) {
				dprintf(D_ALWAYS,
						"Error in sending SIGUSR1 to pid %d errno=%d\n",
						srec->pid, errno);            
			} else {
				dprintf(D_FULLDEBUG, "Sent SIGUSR1 to Shadow Pid %d\n",
						srec->pid);
				srec->preempted = TRUE;
			}

            
        } else {  // Scheduler universe job
            
            dprintf( D_FULLDEBUG,
                     "Found record for scheduler universe job %d.%d\n",
                     job_id.cluster, job_id.proc);
            
			char owner[_POSIX_PATH_MAX];
			owner[0] = '\0';
			GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);
			init_user_ids(owner);
			int kill_sig = DC_SIGTERM;
				// First, try ATTR_REMOVE_KILL_SIG
			if( GetAttributeInt(job_id.cluster, job_id.proc,
								ATTR_REMOVE_KILL_SIG, &kill_sig) < 0 ) {
					// Fall back on the regular ATTR_KILL_SIG
				GetAttributeInt(job_id.cluster, job_id.proc,
								ATTR_KILL_SIG, &kill_sig);
			}
			dprintf( D_FULLDEBUG,
					 "Sending remove signal (%d) to scheduler universe job"
					 " pid=%d owner=%s\n", kill_sig, srec->pid, owner );
			priv_state priv = set_user_priv();

				// TODO!!!!
				// This is really broken, since DC::Send_Signal()
				// assumes you're giving it a DC_SIGXXX brand of
				// signal number.  So, what users define in their
				// submit files isn't what they're really going to
				// get!  This is evil.  We need to either do the
				// appropriate translation, store everything as
				// strings, or come up with a better solution. 
				// -Derek Wright 3/23/01
			if ( daemonCore->Send_Signal( srec->pid, kill_sig ) == TRUE ) {
				// successfully sent signal
				srec->preempted = TRUE;
			}
			set_priv(priv);
		}

		if (mode == REMOVED) {
			srec->removed = TRUE;
		}
        
    } else {
		// We did not find a shadow for this job; just remove it.
		if (mode == REMOVED) {
			if (!scheduler.WriteAbortToUserLog(job_id)) {
				dprintf(D_ALWAYS,"Failed to write abort to the user log\n");
			}
			DestroyProc(job_id.cluster,job_id.proc);
		}
	}
}

} /* End of extern "C" */


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
	char logfilename[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];

	GetAttributeString(job_id.cluster, job_id.proc, ATTR_JOB_IWD, iwd);
	if ( tmp[0] == '/' || tmp[0]=='\\' || (tmp[1]==':' && tmp[2]=='\\') ) {
		strcpy(logfilename, tmp);
	} else {
		sprintf(logfilename, "%s/%s", iwd, tmp);
	}

	owner[0] = '\0';
	GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);

	dprintf( D_FULLDEBUG, 
			 "Writing record to user logfile=%s owner=%s\n",
			 logfilename, owner );

	UserLog* ULog=new UserLog();
	ULog->initialize(owner, logfilename, job_id.cluster, job_id.proc, 0);
	return ULog;
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
	int status = ULog->writeEvent(&event);
	delete ULog;

	if (!status) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_ABORTED event\n" );
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
	event.coreFile[0] = '\0';
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
			abort_job_myself(job_id);
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

				 abort_job_myself(job_id);

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
   we can proceed or false if we can't start another shadow.
*/
bool
Scheduler::canSpawnShadow( int started_jobs, int total_jobs )
{
	int idle_jobs = total_jobs - started_jobs;

		// First, check if we have reached our maximum # of shadows 
	if( CurNumActiveShadows >= MaxJobsRunning ) {
		dprintf( D_ALWAYS, "Reached MAX_JOBS_RUNNING, %d jobs matched, "
				 "%d jobs idle\n", started_jobs, idle_jobs ); 
		return false;
	}

	if( ReservedSwap == 0 ) {
			// We're not supposed to care about swap space at all, so
			// none of the rest of the checks matter at all.
		return true;
	}

		// Now, see if we ran out of swap space already.
	if( SwapSpaceExhausted ) {
		dprintf( D_ALWAYS, "Swap Space Exhausted, %d jobs matched, "
				 "%d jobs idle\n", started_jobs, idle_jobs ); 
		return false;
	}

	if( ShadowSizeEstimate && started_jobs >= MaxShadowsForSwap ) {
		dprintf( D_ALWAYS, "Swap Space Estimate Reached, %d jobs "
				 "matched, %d jobs idle\n", started_jobs, idle_jobs ); 
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
	char*	capability = NULL;			// capability for each match made
	char*	host = NULL;
	char*	sinful = NULL;
	char*	tmp;
	char	temp[512];
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
	int		serviced_other_commands = 0;	
	int		owner_num;
	int		JobsRejected = 0;
	Sock*	sock = (Sock*)s;
	ContactStartdArgs * args;
	ClassAd* my_match_ad;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

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
		char *negotiator_hostname = Negotiator->fullHostname();
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
			for (n=1, FlockNegotiators->rewind();
				 !match && (host = FlockNegotiators->next());
				 n++) {
				hent = gethostbyname(host);
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
		if( cluster_rejected(id.cluster) ) {
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

		if (GetAttributeInt(PrioRec[i].id.cluster, PrioRec[i].id.proc,
							ATTR_CURRENT_HOSTS, &cur_hosts) < 0) {
			cur_hosts = 0;
		}

		if (GetAttributeInt(PrioRec[i].id.cluster, PrioRec[i].id.proc,
							ATTR_MAX_HOSTS, &max_hosts) < 0) {
			max_hosts = 1;
		}

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
					 SetAttributeString(id.cluster, id.proc,
										ATTR_LAST_REJ_MATCH_REASON,
										diagnostic_message);
					 free(diagnostic_message);
				 }
					 // don't break: fall through to REJECTED case
				 case REJECTED:
					job_universe = 0;
					GetAttributeInt(id.cluster, id.proc, ATTR_JOB_UNIVERSE,
									&job_universe);
						// Always negotiate for all PVM job classes! 
					if ( job_universe != PVM && !NegotiateAllJobsInCluster ) {
						mark_cluster_rejected( cur_cluster );
					}
					host_cnt = max_hosts + 1;
					JobsRejected++;
					SetAttributeInt(id.cluster, id.proc,
									ATTR_LAST_REJ_MATCH_TIME, (int)time(0));
					break;
				case SEND_JOB_INFO: {
						// The Negotiator wants us to send it a job. 
						// First, make sure we could start another
						// shadow without violating some limit.
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
						// If we got this far, we can spawn another
						// shadow, so keep going w/ our regular work. 

					/* Send a job description */
					s->encode();
					if( !s->put(JOB_INFO) ) {
						dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
						return (!(KEEP_STREAM));
					}
					ClassAd *ad;
					ad = GetJobAd( id.cluster, id.proc );
					if (!ad) {
						dprintf(D_ALWAYS,"Can't get job ad %d.%d\n",
								id.cluster, id.proc );
						return (!(KEEP_STREAM));
					}	

					// Figure out if this request would result in another 
					// shadow process if matched.  If non-PVM, the answer
					// is always yes.  If PVM, perhaps yes or no.
					shadow_num_increment = 1;
					job_universe = 0;
					ad->LookupInteger(ATTR_JOB_UNIVERSE, job_universe);
					if( job_universe == PVM ) {
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
					sprintf (temp, "%s = True", ATTR_WANT_MATCH_DIAGNOSTICS);
					ad->Insert (temp);

					// Send the ad to the negotiator
					if( !ad->put(*s) ) {
						dprintf( D_ALWAYS,
								"Can't send job ad to mgr\n" );
						FreeJobAd(ad);
						s->end_of_message();
						return (!(KEEP_STREAM));
					}
					FreeJobAd(ad);
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't send job eom to mgr\n" );
						return (!(KEEP_STREAM));
					}
					dprintf( D_FULLDEBUG,
							"Sent job %d.%d\n", id.cluster, id.proc );
					cur_cluster = id.cluster;
					break;
				}
				case PERMISSION:
				case PERMISSION_AND_AD:
					/*
					 * If things are cool, contact the startd.
					 */
					dprintf ( D_FULLDEBUG, "In case PERMISSION\n" );

					SetAttributeInt(id.cluster, id.proc,
									ATTR_LAST_MATCH_TIME, (int)time(0));

					if( !s->get(capability) ) {
						dprintf( D_ALWAYS,
								"Can't receive capability from mgr\n" );
						return (!(KEEP_STREAM));
					}
					my_match_ad = NULL;
					if ( op == PERMISSION_AND_AD ) {
						// get startd ad from negotiator as well
						my_match_ad = new ClassAd();
						if( !my_match_ad->get(*s) ) {
							dprintf( D_ALWAYS,
								"Can't get my match ad from mgr\n" );
							delete my_match_ad;
							FREE( capability );
							return (!(KEEP_STREAM));
						}
					}
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't receive eom from mgr\n" );
						if (my_match_ad)
							delete my_match_ad;
						FREE( capability );
						return (!(KEEP_STREAM));
					}
						// capability is in the form
						// "<xxx.xxx.xxx.xxx:xxxx>#xxxxxxx" 
						// where everything upto the # is the sinful
						// string of the startd

					dprintf( D_PROTOCOL, 
							 "## 4. Received capability %s\n", capability );

					if ( my_match_ad ) {
						dprintf(D_PROTOCOL,"Received match ad\n");
					}

						// Pull out the sinful string.
					sinful = strdup( capability );
					tmp = strchr( sinful, '#');
					if( tmp ) {
						*tmp = '\0';
					} else {
						dprintf( D_ALWAYS, "Can't find '#' in capability!\n" );
							// What else should we do here?
						FREE( sinful );
						FREE( capability );
						sinful = NULL;
						capability = NULL;
						if( my_match_ad ) {
							delete my_match_ad;
						}
						break;
					}
						// sinful should now point to the sinful string
						// of the startd we were matched with.

					// CLAIMING LOGIC

					/* Here we don't want to call contactStartd directly
					   because we do not want to block the negotiator for 
					   this, and because we want to minimize the possibility
					   that the startd will have to block/wait for the 
					   negotiation cycle to finish before it can finish
					   the claim protocol.  So...we enqueue the
					   args for a later call.  (The later call will be
					   made from the startdContactSockHandler) */
					args = new ContactStartdArgs( capability, owner,
												  sinful, id,
												  my_match_ad,
												  negotiator_name, 
												  false );

						// Now that the info is stored in the above
						// object, we can deallocate all of our
						// strings and other memory.
					free( sinful );
					sinful = NULL;
					free( capability );
					capability = NULL;
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
	char	*capability = NULL;

	dprintf( D_ALWAYS, "Got VACATE_SERVICE from %s\n", 
			 sin_to_string(((Sock*)sock)->endpoint()) );

	if (!sock->code(capability)) {
		dprintf (D_ALWAYS, "Failed to get capability\n");
		return;
	}
	if( DelMrec(capability) < 0 ) {
			// We couldn't find this match in our table, perhaps it's
			// from a dedicated resource.
		dedicated_scheduler.DelMrec( capability );
	}
	FREE (capability);
	dprintf (D_PROTOCOL, "## 7(*)  Completed vacate_service\n");
	return;
}


bool
Scheduler::contactStartd( ContactStartdArgs* args ) 
{
	if( args->isDedicated() ) {
			// If this was a match for the dedicated scheduler, let it
			// handle it from here, since we don't want to generate a
			// match_rec for it or anything like that.
		return dedicated_scheduler.contactStartd( args );
	}

	dprintf( D_FULLDEBUG, "In Scheduler::contactStartd()\n" );

    dprintf( D_FULLDEBUG, "%s %s %s %d.%d\n", args->capability(), 
			 args->owner(), args->sinful(), args->cluster(),
			 args->proc() ); 

	match_rec* mrec;   // match record pointer
	PROC_ID id;

	id.cluster = args->cluster();
	id.proc = args->proc();

	mrec = AddMrec( args->capability(), args->sinful(), &id,
					args->matchAd(), args->owner(), args->pool() ); 
	if( ! mrec ) {
        return false;
	}
	ClassAd *jobAd = GetJobAd( id.cluster, id.proc );
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "failed to find job %d.%d\n", id.cluster,
				 id.proc ); 
		DelMrec( mrec );
		return false;
	}

	if( ! claimStartd(mrec, jobAd, false) ) {
		DelMrec( mrec );
		return false;
	}
	return true;
}


#ifdef BAILOUT   /* this is kinda hack-like, but we need to do it a LOT. */
#undef BAILOUT
#endif
#define BAILOUT       \
        delete sock;  \
        return false;

bool
claimStartd( match_rec* mrec, ClassAd* job_ad, bool is_dedicated )
{

	ReliSock *sock = new ReliSock();

	sock->timeout( STARTD_CONTACT_TIMEOUT );

	if ( !sock->connect(mrec->peer, 0) ) {
		dprintf( D_ALWAYS, "Couldn't connect to startd at %s\n",
				 mrec->peer );
		BAILOUT;
	}

	dprintf( D_PROTOCOL, "Requesting resource from %s ...\n",
			 mrec->peer ); 

	sock->encode();

	if( !sock->put( REQUEST_CLAIM ) ) {
		dprintf( D_ALWAYS, "Couldn't send command to startd.\n" );	
		BAILOUT;
	}

	if( !sock->put( mrec->id ) ) {
		dprintf( D_ALWAYS, "Couldn't send capability to startd.\n" );	
		BAILOUT;
	}

	if( !job_ad->put( *sock ) ) {
		dprintf( D_ALWAYS, "Couldn't send job classad to startd.\n" );	
		BAILOUT;
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
				BAILOUT;
			}
			if( !sock->snd_int(scheduler.aliveInterval(), FALSE) ) {
				dprintf(D_ALWAYS, "Couldn't send alive_interval to startd.\n");
				BAILOUT;
			}
			mrec->sent_alive_interval = true;
		}
	}

	if ( !mrec->sent_alive_interval ) {
		dprintf( D_FULLDEBUG, "Startd expects pre-v6.1.11 claim protocol\n" );
	}

	if( !sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Couldn't send eom to startd.\n" );	
		BAILOUT;
	}

	mrec->setStatus( M_STARTD_CONTACT_LIMBO );

	char to_startd[256];
	sprintf ( to_startd, "to startd %s", mrec->peer );

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
#undef BAILOUT


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

		// fetch the match record.  the daemon core DataPtr specifies the
		// id of the match (which is really the startd capability).  use
		// this id to pull out the actual mrec from our hashtable.
	char *id = (char *) daemonCore->GetDataPtr();
	match_rec *mrec = NULL;
	if ( id ) {
		HashKey key(id);
		matches->lookup(key, mrec);
		free(id);	// it was allocated with strdup() when Register_DataPtr was called
	}

	if ( !mrec ) {
		// The match record must have been deleted.  Nothing left to do, close
		// up shop.
		dprintf ( D_FULLDEBUG, "startdContactSockHandler(): mrec not found\n" );
		checkContactQueue();
		return FALSE;
	}
	
	dprintf ( D_FULLDEBUG, "Got mrec data pointer %x\n", mrec );

	mrec->setStatus( M_CLAIMED ); // we assume things will work out.

	// Now, we set the timeout on the socket to 1 second.  Since we 
	// were called by as a Register_Socket callback, this should not 
	// block if things are working as expected.  
	// However, if the Startd wigged out and sent a 
	// partial int or some such, we cannot afford to block. -Todd 3/2000
	sock->timeout(1);

 	if( !sock->rcv_int(reply, TRUE) ) {
		dprintf( D_ALWAYS, "Response problem from startd.\n" );	
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
		dprintf( D_FULLDEBUG, "In checkContactQueue(), args = %x, "
				 "host=%s\n", args, args->sinful() ); 
		contactStartd( args );
		delete args;
	}
}


int
find_idle_sched_universe_jobs( ClassAd *job )
{
	int	status;
	int	cur_hosts;
	int	max_hosts;
	int	universe;
	PROC_ID id;

	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);

	if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) != 1) {
		universe = STANDARD;
	}

	if (universe != SCHED_UNIVERSE) return 0;

	job->LookupInteger(ATTR_JOB_STATUS, status);
	if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) != 1) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) != 1) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}

	// don't count REMOVED or HELD jobs
	if (max_hosts > cur_hosts &&
		(status == IDLE || status == UNEXPANDED || status == RUNNING)) {
		dprintf(D_FULLDEBUG,"Found idle scheduler universe job %d.%d\n",id.cluster,id.proc);
		scheduler.start_sched_universe_job(&id);
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
		if( rec->status == M_UNCLAIMED ) {
			dprintf(D_FULLDEBUG, "match (%s) unclaimed\n", rec->id);
			continue;
		}
		if ( rec->status == M_STARTD_CONTACT_LIMBO ) {
			dprintf ( D_FULLDEBUG, "match (%s) waiting for startd contact\n", 
					  rec->id );
			continue;
		}
		if ( rec->shadowRec ) {
			dprintf(D_FULLDEBUG, "match (%s) already running a job\n",
					rec->id);
			continue;
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
	if (SchedUniverseJobsIdle > 0) {
		StartSchedUniverseJobs();
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
	int executablesize = 0, universe = VANILLA, vm=1;

	GetAttributeString( cluster, proc, ATTR_USER, user );
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
	if (!sock.connect(Negotiator->addr())) {
		dprintf(D_ALWAYS, "Couldn't connect to negotiator!\n");
		return;
	}
	sock.put(REQUEST_NETWORK);

	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe);
	float cputime = 1.0;
	GetAttributeFloat(cluster, proc, ATTR_JOB_REMOTE_USER_CPU, &cputime);
	if (universe == STANDARD && cputime > 0.0) {
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
				dprintf(D_ALWAYS, "DNS lookup for %s %s failed!\n",
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
Scheduler::StartSchedUniverseJobs()
{
	WalkJobQueue( (int(*)(ClassAd *))find_idle_sched_universe_jobs );
}

shadow_rec*
Scheduler::StartJob(match_rec* mrec, PROC_ID* job_id)
{
	int		universe;
	int		rval;

	rval = GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE, 
							&universe);
	if (universe == PVM) {
		return start_pvm(mrec, job_id);
	} else {
		if (rval < 0) {
			dprintf(D_ALWAYS, "Couldn't find %s Attribute for job "
					"(%d.%d) assuming standard.\n",	ATTR_JOB_UNIVERSE,
					job_id->cluster, job_id->proc);
		}
		return start_std(mrec, job_id);
	}
	return NULL;
}

//-----------------------------------------------------------------
// Start Job Handler
//-----------------------------------------------------------------

void Scheduler::StartJobHandler() {

	 StartJobTimer=-1;
	PROC_ID* job_id=NULL;
	match_rec* mrec=NULL;
	shadow_rec* srec;

	// get job from runnable job queue
	while(!mrec) {	
		 if (RunnableJobQueue.dequeue(srec)) return;
		job_id=&srec->job_id;
		mrec=srec->match;
		if (!mrec) {
			dprintf(D_ALWAYS,
				"match for job %d.%d was deleted - not forking a shadow\n",
				job_id->cluster,job_id->proc);
			mark_job_stopped(job_id);
			RemoveShadowRecFromMrec(srec);
			delete srec;
		}
	}


	//-------------------------------
	// Actually fork the shadow
	//-------------------------------

	int		pid;
	int sh_is_dc;

#ifdef WANT_DC_PM
	char	args[_POSIX_ARG_MAX];

	if (Shadow) free(Shadow);
#ifdef WIN32
	Shadow = param("SHADOW");
	sh_is_dc = TRUE;
#else
 	char opsys_buf[100], *match_opsys=opsys_buf;       
 	match_opsys[0] = '\0';
 	if (mrec->my_match_ad) {
 		mrec->my_match_ad->LookupString(ATTR_OPSYS,match_opsys);
	}

	if ( strincmp(match_opsys,"winnt",5) == MATCH ) {
		Shadow = param("SHADOW_NT");
		if( ! Shadow ) {
			Shadow = param("SHADOWNT");
			if( Shadow ) {
				dprintf( D_ALWAYS, 
						 "WARNING: \"SHADOWNT\" is depricated. "
						 "Use \"SHADOW_NT\" in your config file, instead\n" );
			}
		}
		sh_is_dc = TRUE;
		if ( !Shadow ) {
			EXCEPT("Parameter SHADOW_NT not defined!");
		}
	} else {
		Shadow = param("SHADOW");
		sh_is_dc = FALSE;
	}
#endif

	char *shadow_is_dc;
	shadow_is_dc = param( "SHADOW_IS_DC" );

	if (shadow_is_dc) {
		if ( (shadow_is_dc[0] == 'T') || (shadow_is_dc[0] == 't') ) {  
			sh_is_dc = TRUE;
		} else {
			sh_is_dc = FALSE;
		}
		free( shadow_is_dc );
	}

	char ipc_dir[_POSIX_PATH_MAX];
	ipc_dir[0] = '\0';
	char *send_ad_via_file = param( "IPC_VIA_FILES_DIR" );
	if ( send_ad_via_file ) {
		strcat(ipc_dir,send_ad_via_file);
		char our_ipc_filename[100];
		sprintf(our_ipc_filename,"%cpid-%u-%d.%d",DIR_DELIM_CHAR,
			daemonCore->getpid(),job_id->cluster,job_id->proc);
		strcat(ipc_dir,our_ipc_filename);
		free(send_ad_via_file);

		// now write out the job classad to the file
		ClassAd *job_to_write = GetJobAd(job_id->cluster,job_id->proc);

		priv_state priv = set_condor_priv();
		FILE *fp_ipc = fopen(ipc_dir,"w");
		if ( fp_ipc == NULL ) {
			ipc_dir[0] = '\0';
		} else {
			if ( job_to_write->fPrint(fp_ipc) == FALSE ) {
				ipc_dir[0] = '\0';
			}
			fclose(fp_ipc);
		}
		set_priv(priv);
		if ( ipc_dir[0] == '\0' ) {
			dprintf(D_ALWAYS,
				"Unable to write classad into file %s, using socket\n",
				our_ipc_filename);
		}
	}

	if ( sh_is_dc ) {
		sprintf(args, "condor_shadow -f %s %s %s %d %d", MyShadowSockName, 
				mrec->peer, mrec->id, job_id->cluster, job_id->proc);
	} else {
		sprintf(args, "condor_shadow %s %s %s %d %d %s", MyShadowSockName, 
				mrec->peer, mrec->id, job_id->cluster, job_id->proc, ipc_dir);
	}

        /* Get shadow's nice increment: */
    char *shad_nice = param( "SHADOW_RENICE_INCREMENT" );
    int niceness = 0;
    if ( shad_nice ) {
        niceness = atoi( shad_nice );
        free( shad_nice );
    }

	/* Print out the current priv state, for debugging. */
	priv_state current_priv = set_root_priv();
	set_priv(current_priv);

	dprintf(D_FULLDEBUG,
		"About to Create_Process(%s, ...).  Current priv state: %d\n",
		Shadow, current_priv);

	/* We should create the Shadow as PRIV_ROOT so it can read it if
	   it needs too.  This used to be PRIV_UNKNOWN, which was determined
	   to be definately not what we wanted.  -Ballard 2/3/00 */
	pid = daemonCore->Create_Process(Shadow, args, PRIV_ROOT, 1, 
                                     sh_is_dc, NULL, NULL, FALSE, NULL, NULL, 
                                     niceness );

	if (pid == FALSE) {
		dprintf( D_ALWAYS, "CreateProcess(%s, %s) failed\n", Shadow, args );
		pid = -1;
	} 
	free(Shadow);
	Shadow = NULL;
#else
	// here we are not using WANT_DC_PM, so prepare an argv[] array for
	// exec and do an actual honest to goodness fork().
	(void)sprintf( cluster, "%d", job_id->cluster );
	(void)sprintf( proc, "%d", job_id->proc );
	argv[0] = "condor_shadow";
	argv[1] = MyShadowSockName;
	argv[2] = mrec->peer;
	argv[3] = mrec->id;
	argv[4] = cluster;
	argv[5] = proc;
	argv[6] = 0;

	pid = fork();
#endif

	switch(pid) {
		case -1:	/* error */
#ifndef WANT_DC_PM
			if( errno == ENOMEM ) {
				dprintf(D_ALWAYS, "fork() failed, due to lack of swap space\n");
				swap_space_exhausted();
			} else {
				dprintf(D_ALWAYS, "fork() failed, errno = %d\n" );
			}
#endif
			mark_job_stopped(job_id);
			RemoveShadowRecFromMrec(srec);
			delete srec;
			break;

#ifndef WANT_DC_PM
		case 0:		/* the child */
			
			// Renice 
			renice_shadow();

			(void)close( 0 );
#	ifdef CARMI_OPS
			if ((retval = dup2( pipes[0], 0 )) < 0)
			  {
				 dprintf(D_ALWAYS, "Dup2 returns %d\n", retval);  
				 EXCEPT("Could not dup pipe to stdin\n");
			  }
				  else
				 dprintf(D_ALWAYS, " Duped 0 to %d \n", pipes[0]);
#	endif		
			// Close descriptors we do not want inherited by the shadow
			lim = getdtablesize();
			for( int i=3; i<lim; i++ ) {
				(void)close( i );
			}

			Shadow = param("SHADOW");
			(void)execve( Shadow, argv, environ );
			dprintf( D_ALWAYS, "exec(%s) failed, errno = %d\n", Shadow, errno);
			if (errno == ENOMEM) {
				exit( JOB_NO_MEM );
			} else {
				exit( JOB_EXEC_FAILED );
			}
			break;
#endif // of ifndef WANT_DC_PM

		default:	/* the parent */
			dprintf( D_ALWAYS, "Started shadow for job %d.%d on \"%s\", (shadow pid = %d)\n",
				job_id->cluster, job_id->proc, mrec->peer, pid );
			srec->pid=pid;
			add_shadow_rec(srec);
	}

	 // Re-set timer if there are any jobs left in the queue
	 if (!RunnableJobQueue.IsEmpty()) {
		  StartJobTimer=daemonCore->Register_Timer(JobStartDelay,
						(Eventcpp)&Scheduler::StartJobHandler,"start_job", this);
	 } else {
		// if there are no more jobs in the queue, this is a great time to
		// update the central manager since things are pretty "stable".
		// this way, "condor_status -sub" and other views such as 
		// "condor_status -run" will be more likely to match
		timeout();
	}
}

shadow_rec*
Scheduler::start_std(match_rec* mrec , PROC_ID* job_id)
{

	dprintf( D_FULLDEBUG, "Scheduler::start_std - job=%d.%d on %s\n",
			job_id->cluster, job_id->proc, mrec->peer);

	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);

	// add job to run queue

	dprintf(D_FULLDEBUG,"Queueing job %d.%d in runnable job queue\n",
			job_id->cluster,job_id->proc);
	shadow_rec* srec=add_shadow_rec(0,job_id, mrec,-1);
	if (RunnableJobQueue.enqueue(srec)) {
		EXCEPT("Cannot put job into run queue\n");
	}
	if (StartJobTimer<0) {
		StartJobTimer = daemonCore->Register_Timer(
				0, (Eventcpp)&Scheduler::StartJobHandler,"start_job", this);
	 }

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

		if (Shadow) free( Shadow );
		Shadow = param("SHADOW_PVM");
		if ( !Shadow ) {
			Shadow = param("SHADOW_CARMI");
			if ( !Shadow ) {
				dprintf ( D_ALWAYS, "Neither SHADOW_PVM nor SHADOW_CARMI "
						  "defined in config file\n" );
				return NULL;
			}
		}
	    sprintf ( args, "condor_shadow.pvm %s", MyShadowSockName );

		int fds[3];
		fds[0] = pipes[0];  // the effect is to dup the pipe to stdin in child.
	    fds[1] = fds[2] = -1;
		dprintf ( D_ALWAYS, "About to Create_Process( %s, %s, ... )\n", 
				  Shadow, args );
		
		pid = daemonCore->Create_Process( Shadow, args, PRIV_UNKNOWN, 
										  1, FALSE, NULL, NULL, FALSE, 
										  NULL, fds );

		if ( !pid ) {
			dprintf ( D_ALWAYS, "Problem with Create_Process!\n" );
			close(pipes[0]);
			return NULL;
		}

		dprintf ( D_ALWAYS, "In parent, shadow pid = %d\n", pid );

		close(pipes[0]);
		mark_job_running(job_id);
		srp = add_shadow_rec( pid, job_id, mrec, pipes[1] );
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
Scheduler::start_sched_universe_job(PROC_ID* job_id)
{
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */

#ifndef WANT_DC_PM
	dprintf ( D_ALWAYS, "Starting sched universe jobs no longer "
			  "supported without WANT_DC_PM\n" );
	return NULL;
#else

	char	a_out_name[_POSIX_PATH_MAX];
	char	input[_POSIX_PATH_MAX];
	char	output[_POSIX_PATH_MAX];
	char	error[_POSIX_PATH_MAX];
	char	env[ATTRLIST_MAX_EXPRESSION];		// fixed size is bad here!!
	char	job_args[_POSIX_ARG_MAX];
	char	args[_POSIX_ARG_MAX];
	char	owner[20], iwd[_POSIX_PATH_MAX];
	int		pid;

	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
			job_id->cluster, job_id->proc );

	// make sure file is executable
	strcpy(a_out_name, gen_ckpt_name(Spool, job_id->cluster, ICKPT, 0));
	errno = 0;
	if( chmod(a_out_name, 0755) < 0 ) {
		dprintf( D_ALWAYS, "ERROR: Can't chmod(%s, 0755), errno: %d\n",
				 a_out_name, errno );
		dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
				 job_id->cluster, job_id->proc );
		SetAttributeInt( job_id->cluster, job_id->proc,
						 ATTR_JOB_STATUS, HELD );
		ClassAd *jobAd = GetJobAd( job_id->cluster, job_id->proc );
		char buf[256];
		sprintf( buf, "Your job (%d.%d) is on hold", job_id->cluster,
				 job_id->proc ); 
		FILE* email = email_user_open( jobAd, buf );
		if( ! email ) {
			return NULL;
		}
		fprintf( email, "Condor failed to start your scheduluer universe " );
		fprintf( email, "job (%d.%d).\n", job_id->cluster, job_id->proc );
		fprintf( email, "The initial executable for the job,\n\"%s\"\n",
				 a_out_name );
		fprintf( email, "is not executable or does not exist.\n" );
		fprintf( email, "\nPlease correct this problem and release your "
				 "job with:\n" );
		fprintf( email, "\"condor_release %d.%d\"\n\n", job_id->cluster,
				 job_id->proc ); 
		email_close ( email );
		return NULL;
	}

	if (GetAttributeString(job_id->cluster, job_id->proc, 
						   ATTR_OWNER, owner) < 0) {
		dprintf(D_FULLDEBUG, "Scheduler::start_sched_universe_job"
				"--setting owner to \"nobody\"\n" );
		sprintf(owner, "nobody");
	}
	if (stricmp(owner, "root") == 0 ) {
		dprintf(D_ALWAYS, "Aborting job %d.%d.  Tried to start as root.\n",
				job_id->cluster, job_id->proc);
		return NULL;
	}

	init_user_ids(owner);

		// Get std(in|out|err)
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_INPUT,
							input) < 0) {
		sprintf(input, "/dev/null");
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_OUTPUT,
							output) < 0) {
		sprintf(output, "/dev/null");
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ERROR,
							error) < 0) {
		sprintf(error, "/dev/null");
	}

	priv_state priv = set_user_priv(); // need user's privs...

	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_IWD,
							iwd) < 0) {
		sprintf(iwd, "/tmp");
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
	if ((inouterr[0] = open(input, O_RDONLY, 0)) < 0) {
		dprintf ( D_ALWAYS, "Open of %s failed, errno %d\n", input, errno );
		cannot_open_files = true;
	}
	if ((inouterr[1] = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0)) < 0) {
		dprintf ( D_ALWAYS, "Open of %s failed, errno %d\n", output, errno );
		cannot_open_files = true;
	}
	if ((inouterr[2] = open(error, O_WRONLY | O_CREAT | O_TRUNC, 0)) < 0) {
		dprintf ( D_ALWAYS, "Open of %s failed, errno %d\n", error, errno );
		cannot_open_files = true;
	}

	//change back to whence we came
	if ( p_tmpCwd ) {
		chdir( p_tmpCwd );
	}

	set_priv( priv );  // back to regular privs...

	if ( cannot_open_files ) {
		return NULL;
	}

	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ENVIRONMENT,
							env) < 0) {
		env[0] = '\0';
	}

	// stick a CONDOR_ID environment variable in job's environment
	char condor_id_string[64];
	sprintf( condor_id_string, "%d.%d", job_id->cluster, job_id->proc );
	AppendEnvVariable( env, "CONDOR_ID", condor_id_string );

	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ARGUMENTS,
							args) < 0) {
		args[0] = '\0';
	}

		// Don't use a_out_name for argv[0], use
		// "condor_scheduniv_exec.cluster.proc" instead. 
	sprintf(job_args, "condor_scheduniv_exec.%d.%d %s",
			job_id->cluster, job_id->proc, args);

	pid = daemonCore->Create_Process( a_out_name, job_args, PRIV_USER_FINAL, 
								1, FALSE, env, iwd, FALSE, NULL, inouterr );

		// now close those open fds - we don't want them here.
	for ( int i=0 ; i<3 ; i++ ) {
		if ( close( inouterr[i] ) == -1 ) {
			dprintf ( D_ALWAYS, "FD closing problem, errno = %d\n", errno );
		}
	}

	if ( pid <= 0 ) {
		dprintf ( D_ALWAYS, "Create_Process problems!\n" );
		return NULL;
	}

	dprintf ( D_ALWAYS, "Successfully created sched universe process\n" );
	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
	WriteExecuteToUserLog( *job_id );
	return add_shadow_rec(pid, job_id, NULL, -1);
#endif /* of WANT_DC_PM code */
#else
	return NULL;
#endif /* !defined(WIN32) */
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
Scheduler::add_shadow_rec( int pid, PROC_ID* job_id, match_rec* mrec, int fd )
{
	shadow_rec *new_rec = new shadow_rec;

	new_rec->pid = pid;
	new_rec->job_id = *job_id;
	new_rec->match = mrec;
	new_rec->preempted = FALSE;
	new_rec->removed = FALSE;
	new_rec->conn_fd = fd;
    new_rec->sinfulString = 
        strnewp( daemonCore->InfoCommandSinfulString( pid ) );
	
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
	int current_time = (int)time(NULL);
	int job_start_date = 0;
	SetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE, current_time);
	if (GetAttributeInt(cluster, proc,
						ATTR_JOB_START_DATE, &job_start_date) < 0) {
		// this is the first time the job has ever run, so set JobStartDate
		SetAttributeInt(cluster, proc, ATTR_JOB_START_DATE, current_time);
	}
}

struct shadow_rec *
Scheduler::add_shadow_rec( shadow_rec* new_rec )
{
	numShadows++;
	shadowsByPid->insert(new_rec->pid, new_rec);
	shadowsByProcID->insert(new_rec->job_id, new_rec);

	if ( new_rec->match ) {
		struct sockaddr_in sin;
		if ( new_rec->match->peer && new_rec->match->peer[0] &&
			 string_to_sin(new_rec->match->peer, &sin) == 1) {
			// make local copy of static hostname buffer
			char *tmp, *hostname;
			if( (tmp = sin_to_hostname(&sin, NULL)) ) {
				hostname = strdup( tmp );
				SetAttributeString( new_rec->job_id.cluster, 
									new_rec->job_id.proc,
									ATTR_REMOTE_HOST, hostname );
				free(hostname);
			} else {
					// Error looking up host info...
					// Just use the sinful string
				SetAttributeString( new_rec->job_id.cluster, 
									new_rec->job_id.proc,
									ATTR_REMOTE_HOST, 
									new_rec->match->peer );
			}
		}
		if ( new_rec->match->pool ) {
			SetAttributeString(new_rec->job_id.cluster, new_rec->job_id.proc,
							   ATTR_REMOTE_POOL, new_rec->match->pool);
		}
		if (new_rec->match->my_match_ad) {
			int vm = 1;
			new_rec->match->my_match_ad->LookupInteger(ATTR_VIRTUAL_MACHINE_ID,
													   vm);
			SetAttributeInt( new_rec->job_id.cluster, new_rec->job_id.proc,
							 ATTR_REMOTE_VIRTUAL_MACHINE_ID, vm );
		}
	}
	int universe = STANDARD;
	GetAttributeInt(new_rec->job_id.cluster, new_rec->job_id.proc,
					ATTR_JOB_UNIVERSE, &universe);
	if (universe == PVM) {
		ClassAd *ad;
		ad = GetNextJob(1);
		while (ad != NULL) {
			PROC_ID tmp_id;
			ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == new_rec->job_id.cluster) {
				ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				add_shadow_birthdate(tmp_id.cluster, tmp_id.proc);
			}
			ad = GetNextJob(0);
		}
	} else {
		add_shadow_birthdate(new_rec->job_id.cluster, new_rec->job_id.proc);
	}

	dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
			new_rec->pid, new_rec->job_id.cluster, new_rec->job_id.proc );
	scheduler.display_shadow_recs();
	return new_rec;
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
		BeginTransaction();
		SetAttributeFloat(cluster, proc,
						  ATTR_JOB_REMOTE_WALL_CLOCK,accum_time);
		DeleteAttribute(cluster, proc, ATTR_JOB_WALL_CLOCK_CKPT);
		CommitTransaction();
	}
}


void
Scheduler::delete_shadow_rec(int pid)
{
	shadow_rec *rec;

	dprintf( D_FULLDEBUG, "Entered delete_shadow_rec( %d )\n", pid );

	if (shadowsByPid->lookup(pid, rec) == 0) {
		dprintf(D_FULLDEBUG,
				"Deleting shadow rec for PID %d, job (%d.%d)\n",
				pid, rec->job_id.cluster, rec->job_id.proc );

		int universe = STANDARD;
		GetAttributeInt(rec->job_id.cluster, rec->job_id.proc,
						ATTR_JOB_UNIVERSE, &universe);
		if (universe == PVM) {
			ClassAd *ad;
			ad = GetNextJob(1);
			while (ad != NULL) {
				PROC_ID tmp_id;
				ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
				if (tmp_id.cluster == rec->job_id.cluster) {
					ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
					update_remote_wall_clock(tmp_id.cluster, tmp_id.proc);
				}
				ad = GetNextJob(0);
			}
		} else {
			update_remote_wall_clock(rec->job_id.cluster, rec->job_id.proc);
		}

		char last_host[256];
		last_host[0] = '\0';
		GetAttributeString(rec->job_id.cluster,rec->job_id.proc,
			ATTR_REMOTE_HOST,last_host);
		DeleteAttribute( rec->job_id.cluster,rec->job_id.proc, 
			ATTR_REMOTE_HOST );
		if ( last_host[0] ) {
			SetAttributeString( rec->job_id.cluster,rec->job_id.proc, 
							ATTR_LAST_REMOTE_HOST, last_host );
		}
		DeleteAttribute( rec->job_id.cluster, rec->job_id.proc,
						 ATTR_REMOTE_POOL );
		DeleteAttribute( rec->job_id.cluster,rec->job_id.proc, 
			ATTR_REMOTE_VIRTUAL_MACHINE_ID );

		if (rec->removed) {
			if (!WriteAbortToUserLog(rec->job_id)) {
				dprintf(D_ALWAYS,"Failed to write abort to the user log\n");
			}
		}
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
        if ( rec->sinfulString ) 
            delete [] rec->sinfulString;

		delete rec;
		numShadows -= 1;
		if( ExitWhenDone && numShadows == 0 ) {
			schedd_exit();
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

	// Also clear out ATTR_SHADOW_BIRTHDATE.  We'll set it to be the
	// current time when we actually start the shadow (in add_shadow_rec), 
	// since that is more accurate (esp if JOB_START_DELAY is large or we
	// just got lots of resources from the central manager).
	// By setting it to zero, we prevent condor_q from getting confused by
	// seeing a job status of running and an old (or non-existant) 
	// ATTR_SHADOW_BIRTHDATE; this could result in condor_q temporarily
	// displaying huge run times until the shadow is started. 
	// -Todd <tannenba@cs.wisc.edu>
	SetAttributeInt(job_id->cluster, job_id->proc,
		ATTR_SHADOW_BIRTHDATE, 0);
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

	// if job isn't RUNNING, then our work is already done
	if (status == RUNNING) {

		if ( GetAttributeString(job_id->cluster, job_id->proc, 
				ATTR_OWNER, owner) < 0 )
		{
			dprintf(D_FULLDEBUG,"mark_job_stopped: setting owner to \"nobody\"\n");
			strcpy(owner,"nobody");
		}

		SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, IDLE);
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
	int universe = STANDARD;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE,
					&universe);
	if (universe == PVM) {
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
	int universe = STANDARD;
	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_UNIVERSE,
					&universe);
	if (universe == PVM) {
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

	if ( N_RejectedClusters + 1 == MAX_REJECTED_CLUSTERS ) {
		EXCEPT("Reached MAX_REJECTED_CLUSTERS");
	}
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
			dprintf( D_ALWAYS,
			"Cleaning up ShadowRec for pid %d\n", rec->pid );
			delete_shadow_rec( rec->pid );
		}
	}
	dprintf( D_FULLDEBUG, "============ End clean_shadow_recs =============\n" );
}

void
Scheduler::preempt(int n)
{
	shadow_rec *rec;
	bool preempt_all;

	dprintf( D_ALWAYS, "Called preempt( %d )\n", n );
	if (n >= numShadows-SchedUniverseJobsRunning) {	// preempt all
		preempt_all = true;
	} else {
		preempt_all = false;
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
			int universe;
			GetAttributeInt(rec->job_id.cluster, rec->job_id.proc, 
							ATTR_JOB_UNIVERSE,&universe);
			if (universe == PVM) {
				if ( !rec->preempted ) {
					daemonCore->Send_Signal( rec->pid, DC_SIGTERM );
				} else {
					if ( ExitWhenDone ) {
						n++;
					}
				}
				rec->preempted = TRUE;
				n--;
            }
			else if (rec->match) {
				if( !rec->preempted ) {
					send_vacate( rec->match, CKPT_FRGN_JOB );
				} else {
					if ( ExitWhenDone ) {
						n++;
					}
				}
				rec->preempted = TRUE;
				n--;
			} 
			else if (preempt_all) {
				if ( !rec->preempted ) {
					// This is the scheduler universe job case.  We get here
					// because scheduler universe jobs don't have associated
					// match records.  We check to make sure this is in fact
					// a scheduler universe job before we send the signal
					// beceause it could also be a shadow for which the
					// claim was relinquished (by the startd).
					if (IsSchedulerUniverse(rec)) {
						int kill_sig = DC_SIGTERM;
						GetAttributeInt(rec->job_id.cluster, rec->job_id.proc, 
										ATTR_KILL_SIG,&kill_sig);
						daemonCore->Send_Signal( rec->pid, kill_sig );
					}
				} else {
					if ( ExitWhenDone ) {
						n++;
					}
				}
				rec->preempted = TRUE;
				n--;
			}
		}
	}
}

void
send_vacate(match_rec* match,int cmd)
{
	//SC2000
	//ReliSock	sock;
	SafeSock	sock;

	dprintf( D_FULLDEBUG, "Called send_vacate( %s, %d )\n", match->peer, cmd );
	 
	sock.timeout(STARTD_CONTACT_TIMEOUT);
	if (!sock.connect(match->peer,START_PORT)) {
		dprintf(D_ALWAYS,"Can't connect to startd at %s\n",match->peer);
		return;
	}
	
	sock.encode();

	 if( !sock.code(cmd) ) {
		  dprintf( D_ALWAYS, "Can't initialize sock to %s\n", match->peer);
		return;
	}

	 if( !sock.put(match->id) ) {
		  dprintf( D_ALWAYS, "Can't initialize sock to %s\n", match->peer);
		return;
	}

	 if( !sock.eom() ) {
		  dprintf( D_ALWAYS, "Can't send EOM to %s\n", match->peer);
		return;
	}

	if( cmd == CKPT_FRGN_JOB ) {
		dprintf( D_ALWAYS, "Sent CKPT_FRGN_JOB to startd on %s\n", match->peer);
	} else {
		dprintf( D_ALWAYS, "Sent KILL_FRGN_JOB to startd on %s\n", match->peer);
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
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_UNIVERSE, &universe);
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_STATUS, &status);
			if (status != RUNNING && universe!=PVM && universe!=MPI) {
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

void
Scheduler::mail_problem_message()
{
#if !defined(WIN32)
	FILE	*mailer;

	dprintf( D_ALWAYS, "Mailing administrator (%s)\n", CondorAdministrator );

	mailer = email_admin_open("CONDOR Problem");
	if (mailer == NULL)
	{
		EXCEPT( "Could not open mailer to admininstrator!" );
	}

	fprintf( mailer, "Problem with condor_schedd %s\n", Name );
	fprintf( mailer, "Job %d.%d is in the runnable job table,\n",
												BadCluster, BadProc );
	fprintf( mailer, "but we already have a shadow record for it.\n" );

	email_close(mailer);
#endif /* !defined(WIN32) */
}

void
Scheduler::NotifyUser(shadow_rec* srec, char* msg, int status, int JobStatus)
{
#if !defined(WIN32)
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
	if (mailer == NULL) {
		EXCEPT("Could not open mail to user!");
	}
	fprintf(mailer, "Your condor job %s%d.\n\n", msg, status);
	fprintf(mailer, "Job: %s %s\n", cmd, args);
	email_close(mailer);

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

#endif
}

// Check scheduler universe
static int IsSchedulerUniverse(shadow_rec* srec)
{
	// dprintf(D_FULLDEBUG,"Scheduler::IsSchedulerUniverse - checking job universe\n");
	if (srec==NULL || srec->match!=NULL) return FALSE;
	int universe=STANDARD;
	GetAttributeInt(srec->job_id.cluster, srec->job_id.proc, ATTR_JOB_UNIVERSE,&universe);
	if (universe!=SCHED_UNIVERSE) return FALSE;
	return TRUE;
}

#ifndef WANT_DC_PM
/*
** Allow child processes to die a decent death, don't keep them
** hanging around as <defunct>.
*/
void
Scheduler::reaper(int sig)
{
	pid_t			pid;
	 int	  		status;

	 if( sig == 0 ) {
		  dprintf( D_FULLDEBUG, "***********  Begin Extra Checking ********\n" );
	 } else {
		  dprintf( D_ALWAYS, "Entered reaper( %d, %d )\n", sig );
	 }

	for(;;) {
		  errno = 0;
		  if( (pid = waitpid(-1,&status,WNOHANG)) <= 0 ) {
				dprintf( D_FULLDEBUG, "waitpid() returned %d, errno = %d\n",
																				pid, errno );
				break;
		  }
		child_exit(pid, status);
	}
	 if( sig == 0 ) {
		  dprintf( D_FULLDEBUG, "***********  End Extra Checking ********\n" );
	 }
}
#endif

/*
** Wrapper for setting the job status to deal with PVM jobs, which can 
** contain multiple procs.
*/
void
set_job_status(int cluster, int proc, int status)
{
	int universe = STANDARD;
	GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe);
	if (universe == PVM) {
		ClassAd *ad;
		ad = GetNextJob(1);
		while (ad != NULL) {
			PROC_ID tmp_id;
			ad->LookupInteger(ATTR_CLUSTER_ID, tmp_id.cluster);
			if (tmp_id.cluster == cluster) {
				ad->LookupInteger(ATTR_PROC_ID, tmp_id.proc);
				SetAttributeInt(tmp_id.cluster, tmp_id.proc, ATTR_JOB_STATUS,
								status);
			}
			ad = GetNextJob(0);
		}
	} else {
		SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, status);
	}
}

void
Scheduler::child_exit(int pid, int status)
{
	shadow_rec*		srec;
	int				StartJobsFlag=TRUE;
	int				q_status;  // status of this job in the queue 

	srec = FindSrecByPid(pid);

	if (IsSchedulerUniverse(srec)) {
		PROC_ID jobId;
		jobId.cluster = srec->job_id.cluster;
		jobId.proc = srec->job_id.proc;

		// scheduler universe process
		if(WIFEXITED(status)) {
			dprintf(D_FULLDEBUG,
					"scheduler universe job (%d.%d) pid %d "
					"exited with status %d\n", srec->job_id.cluster,
					srec->job_id.proc, pid, WEXITSTATUS(status) );
			if (!srec->preempted) {
				NotifyUser(srec, "exited with status ",
						   WEXITSTATUS(status) , COMPLETED);
				SetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
								ATTR_JOB_STATUS, COMPLETED);
				SetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
								ATTR_JOB_EXIT_STATUS, WEXITSTATUS(status) );
				WriteTerminateToUserLog( jobId, status );
			} else {
				WriteEvictToUserLog( jobId );
			}
		} else if(WIFSIGNALED(status)) {
			dprintf(D_ALWAYS,
					"scheduler universe job (%d.%d) pid %d died "
					"with %s\n", srec->job_id.cluster,
					srec->job_id.proc, pid, 
					daemonCore->GetExceptionString(status));
			if (!srec->preempted) {
				NotifyUser(srec, "was killed by signal ",
						   WTERMSIG(status), REMOVED);
				SetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
								ATTR_JOB_STATUS, REMOVED);
				WriteTerminateToUserLog( jobId, status );
			} else {
				WriteEvictToUserLog( jobId );
			}
		}
/*
			if( DestroyProc(srec->job_id.cluster, srec->job_id.proc) ) {
				dprintf(D_ALWAYS, "DestroyProc(%d.%d) failed -- "
						"presumably job was already removed\n",
						srec->job_id.cluster,
						srec->job_id.proc);
			}
*/
		delete_shadow_rec( pid );
	} else if (srec) {
		// A real shadow
		if ( daemonCore->Was_Not_Responding(pid) ) {
			// this shadow was killed by daemon core because it was hung.
			// make the schedd treat this like a Shadow Exception so job
			// just goes back into the queue as idle, but if it happens
			// to many times we relinquish the match.
			dprintf(D_FULLDEBUG,
				"Shadow pid %d successfully killed because it was hung.\n"
				,pid);
			status = JOB_EXCEPTION;
		}
		if( GetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
							ATTR_JOB_STATUS, &q_status) < 0 ) {
			EXCEPT( "ERROR no job status for %d.%d in child_exit()!",
					srec->job_id.cluster, srec->job_id.proc );
		}
		if( WIFEXITED(status) ) {			
            dprintf( D_FULLDEBUG, "Shadow pid %d exited with status %d\n",
					 pid, WEXITSTATUS(status) );

			switch( WEXITSTATUS(status) ) {
			case JOB_NO_MEM:
				swap_space_exhausted();
			case JOB_EXEC_FAILED:
				StartJobsFlag=FALSE;
				break;
			case JOB_CKPTED:
			case JOB_NOT_CKPTED:
			case JOB_NOT_STARTED:
				if (!srec->removed) {
					Relinquish(srec->match);
					DelMrec(srec->match);
				}
				break;
			case JOB_SHADOW_USAGE:
				EXCEPT("shadow exited with incorrect usage!\n");
				break;
			case JOB_BAD_STATUS:
				EXCEPT("shadow exited because job status != RUNNING");
				break;
			case JOB_NO_CKPT_FILE:
			case JOB_KILLED:
			case JOB_COREDUMPED:
				if( q_status != HELD ) {
					set_job_status( srec->job_id.cluster,
									srec->job_id.proc, REMOVED ); 
				}
				break;
			case JOB_EXITED:
				dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
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
						 "ERROR: Shadow had fatal error writing to its log file.\n" );
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
			case JOB_EXCEPTION:
				if ( WEXITSTATUS(status) == JOB_EXCEPTION ){
					dprintf( D_ALWAYS,
						"ERROR: Shadow exited with job exception code!\n");
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
						"ERROR: Shadow exited with unknown value!\n");
				}
				// record that we had an exception.  This function will
				// relinquish the match if we get too many
				// exceptions 
				if( !srec->removed ) {
					HadException(srec->match);
				}
				break;
			}
	 	} else if( WIFSIGNALED(status) ) {
			dprintf( D_ALWAYS, "Shadow pid %d died with %s\n",
					 pid, daemonCore->GetExceptionString(status) );
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

	 if (GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS,
						&status) < 0){
		  dprintf(D_ALWAYS,"ERROR fetching job status in check_zombie !\n");
		return;
	 }

	 dprintf( D_FULLDEBUG, "Entered check_zombie( %d, 0x%x, st=%d )\n", pid, job_id, status );

	// set cur-hosts to zero
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 0);

	 switch( status ) {
		  case RUNNING:
				kill_zombie( pid, job_id );
				break;
		  case REMOVED:
		case COMPLETED:
				DestroyProc(job_id->cluster,job_id->proc);
				break;
		  default:
				break;
	 }

	 dprintf( D_FULLDEBUG, "Exited check_zombie( %d, 0x%x )\n", pid, job_id );
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
	int		universe = STANDARD;

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
	if (universe == STANDARD) {
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
			if (universe == STANDARD) {
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
			if (universe == STANDARD) {
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

		////////////////////////////////////////////////////////////////////
		// Grab all the essential parameters we need from the config file.
		////////////////////////////////////////////////////////////////////

	if( Spool ) free( Spool );
	if( !(Spool = param("SPOOL")) ) {
		EXCEPT( "No spool directory specified in config file" );
	}

	if( CondorViewHost ) free( CondorViewHost );
	CondorViewHost = param("CONDOR_VIEW_HOST");

	if( Collector ) delete( Collector );
	Collector = new Daemon( DT_COLLECTOR );

	if( Negotiator ) delete( Negotiator );
	Negotiator = new Daemon( DT_NEGOTIATOR );


	if( Shadow ) free( Shadow );
	if( ! (Shadow = param("SHADOW")) ) {
		EXCEPT( "SHADOW not specified in config file\n" );
	}

	if( CondorAdministrator ) free( CondorAdministrator );
	if( ! (CondorAdministrator = param("CONDOR_ADMIN")) ) {
		EXCEPT( "CONDOR_ADMIN not specified in config file" );
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
		// its own.)  Note that the string we pass to putenv() may
		// become part of the environment, so we can't free it until we
		// replace it with a new value.
	char *OldNameInEnv = NameInEnv;
	NameInEnv = (char *)malloc(strlen("SCHEDD_NAME=")+strlen(Name)+1);
	sprintf(NameInEnv, "SCHEDD_NAME=%s", Name);
	if (putenv(NameInEnv) < 0) {
		dprintf(D_ALWAYS, "putenv(\"%s\") failed!\n", NameInEnv);
	}
	if (OldNameInEnv) free(OldNameInEnv);

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
	
	tmp = param( "MAX_JOBS_RUNNING" );
	if( ! tmp ) {
		MaxJobsRunning = 200;
	} else {
		MaxJobsRunning = atoi( tmp );
		free( tmp );
	}
	
	tmp = param( "NEGOTIATE_ALL_JOBS_IN_CLUSTER" );
	if( !tmp || tmp[0] == 'f' || tmp[0] == 'F' ) {
		NegotiateAllJobsInCluster = false;
	} else {
		NegotiateAllJobsInCluster = true;
	}
	if( tmp ) free( tmp );
	
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
	}

	char *flock_collector_hosts, *flock_negotiator_hosts, *flock_view_servers;
	flock_collector_hosts = param( "FLOCK_COLLECTOR_HOSTS" );
	if (!flock_collector_hosts) { // backward compatibility
		flock_collector_hosts = param( "FLOCK_HOSTS" );
	}
	flock_negotiator_hosts = param( "FLOCK_NEGOTIATOR_HOSTS" );
	if (!flock_negotiator_hosts) { // backward compatibility
		flock_negotiator_hosts = param( "FLOCK_HOSTS" );
	}
	flock_view_servers = param( "FLOCK_VIEW_SERVERS" );
	if (flock_collector_hosts && flock_negotiator_hosts) {
		if (FlockCollectors) delete FlockCollectors;
		FlockCollectors = new StringList( flock_collector_hosts );
		MaxFlockLevel = FlockCollectors->number();
		if (FlockNegotiators) delete FlockNegotiators;
		FlockNegotiators = new StringList( flock_negotiator_hosts );
		if (FlockCollectors->number() != FlockNegotiators->number()) {
			dprintf(D_ALWAYS, "FLOCK_COLLECTOR_HOSTS and "
					"FLOCK_NEGOTIATOR_HOSTS lists are not the same size."
					"Flocking disabled.\n");
			MaxFlockLevel = 0;
		}
		if (flock_view_servers) {
			if (FlockViewServers) delete FlockViewServers;
			FlockViewServers = new StringList( flock_view_servers );
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
	if (flock_view_servers) free(flock_view_servers);

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
	 if(!tmp) {
		alive_interval = 300;
	 } else {
		  alive_interval = atoi(tmp);
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

	// Command handler for testing file access.  I set this as WRITE as we
	// don't want people snooping the permissions on our machine.
	daemonCore->Register_Command( ATTEMPT_ACCESS, "ATTEMPT_ACCESS", 
								  (CommandHandler)&attempt_access_handler, 
								  "attempt_access_handler", NULL, WRITE, 
								  D_FULLDEBUG );

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
	
	 // reaper
#ifdef WANT_DC_PM
	daemonCore->Register_Reaper( "reaper", 
                                 (ReaperHandlercpp)&Scheduler::child_exit,
								 "child_exit", this );
#else
    daemonCore->Register_Signal( DC_SIGCHLD, "SIGCHLD", 
                                 (SignalHandlercpp)&Scheduler::reaper, 
								 "reaper", this );
#endif

	// register all the timers
	RegisterTimers();

	// Now is a good time to instantiate the GridUniverse
	_gridlogic = new GridUniverseLogic;
}

void
Scheduler::RegisterTimers()
{
	static int aliveid = -1, cleanid = -1, wallclocktid = -1;

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
	if (cleanid >= 0) {
		daemonCore->Cancel_Timer(cleanid);
	}

	 // timer handlers
	 timeoutid = daemonCore->Register_Timer(10,
		(Eventcpp)&Scheduler::timeout,"timeout",this);
	startjobsid = daemonCore->Register_Timer(10,
		(Eventcpp)&Scheduler::StartJobs,"StartJobs",this);
	aliveid = daemonCore->Register_Timer(alive_interval, alive_interval,
		(Eventcpp)&Scheduler::sendAlives,"sendAlives", this);
	cleanid = daemonCore->Register_Timer(QueueCleanInterval,
										 QueueCleanInterval,
										 (Event)&CleanJobQueue,
										 "CleanJobQueue");
	if (WallClockCkptInterval) {
		wallclocktid = daemonCore->Register_Timer(WallClockCkptInterval,
												  WallClockCkptInterval,
												  (Event)&CkptWallClock,
												  "CkptWallClock");
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
	timeout();
}

// This function is called by a timer when we are shutting down
void
Scheduler::preempt_one_job()
{
	preempt(1);
}

// Perform graceful shutdown.
void
Scheduler::shutdown_graceful()
{
	if( numShadows == 0 ) {
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
	daemonCore->Register_Timer(0,JobStartDelay,
					(TimerHandlercpp)&Scheduler::preempt_one_job,
					"preempt_one_job()", this );
}

// Perform fast shutdown.
void
Scheduler::shutdown_fast()
{
	shadow_rec *rec;
	shadowsByPid->startIterations();
	while (shadowsByPid->iterate(rec) == 1) {
		daemonCore->Send_Signal( rec->pid, DC_SIGKILL );
		rec->preempted = TRUE;
	}

		// Since this is just sending a bunch of UDP updates, we can
		// still invalidate our classads, even on a fast shutdown.
	invalidate_ads();

	dprintf( D_ALWAYS, "All shadows have been killed, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::schedd_exit()
{
		// write a clean job queue on graceful shutdown so we can
		// quickly recover on restart
	CleanJobQueue();

		// Deallocate the memory in the job queue so we don't think
		// we're leaking anything. 
	DestroyJobQueue();

		// Invalidate our classads at the collector, since we're now
		// gone.  
	invalidate_ads();

	dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::invalidate_ads()
{
	int i;
	char *host;
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
	updateCentralMgr( INVALIDATE_SCHEDD_ADS, ad, Collector->addr(), 0 );

	if (N_Owners == 0) return;	// no submitter ads to invalidate

		// Invalidate all our submittor ads.
	sprintf( line, "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_SCHEDD_NAME,
			 Name );
    ad->InsertOrUpdate( line );
	updateCentralMgr( INVALIDATE_SUBMITTOR_ADS, ad, Collector->addr(), 0 );
	if( FlockCollectors && FlockLevel > 0 ) {
		for( i=1, FlockCollectors->rewind();
			 i <= FlockLevel && (host = FlockCollectors->next()); i++ ) {
			updateCentralMgr( INVALIDATE_SUBMITTOR_ADS, ad, host, 
							  COLLECTOR_UDP_COMM_PORT );
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

	if (SchedUniverseJobsIdle > 0) {
		StartSchedUniverseJobs();
	}

	 return;
}


void
Scheduler::sendReschedule( void )
{
	int cmd = RESCHEDULE;
	SafeSock sock;
	sock.timeout(NEGOTIATOR_CONTACT_TIMEOUT);

	dprintf( D_FULLDEBUG, "Sending RESCHEDULE command to negotiator(s)\n" );

	if( !sock.connect(Negotiator->addr(), 0) ) {
		dprintf( D_ALWAYS, "failed to connect to negotiator %s\n",
				 Negotiator->addr() );
		return;
	}

	sock.encode();
	if( !sock.code(cmd) ) {
		dprintf( D_ALWAYS,
				 "failed to send RESCHEDULE command to negotiator\n" );
		return;
	}
	if( !sock.eom() ) {
		dprintf( D_ALWAYS,
				 "failed to send RESCHEDULE command to negotiator\n" );
		return;
	}
	sock.close();

	if( FlockNegotiators ) {
		FlockNegotiators->rewind();
		char *negotiator = FlockNegotiators->next();
		for( int i=0; negotiator && i < FlockLevel;
			 negotiator = FlockNegotiators->next(), i++ ) {
			if( !sock.connect(negotiator, NEGOTIATOR_PORT) ||
				!sock.code(cmd) || !sock.eom() ) {
				dprintf( D_ALWAYS, "failed to send RESCHEDULE command to %s\n",
						 negotiator );
			}
		}
	}
}


match_rec*
Scheduler::AddMrec(char* id, char* peer, PROC_ID* jobId, ClassAd* my_match_ad,
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
	matches->insert(HashKey(id), rec);
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

	dprintf( D_ALWAYS, "Match record (%s, %d, %d) deleted\n",
			 rec->peer, rec->cluster, rec->proc ); 
	dprintf( D_FULLDEBUG, "Capability of deleted match: %s\n", rec->id );
	
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

	//sock = new ReliSock;
	sock = new SafeSock;
	sock->timeout(STARTD_CONTACT_TIMEOUT);
	sock->encode();
	if(!sock->connect(mrec->peer)) {
		dprintf(D_ALWAYS, "Can't connect to startd %s\n", mrec->peer);
	}
	else if(!sock->put(RELINQUISH_SERVICE))
	{
		dprintf(D_ALWAYS, "Can't relinquish startd. Match record is:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
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
		sock = new ReliSock;
		sock->timeout(NEGOTIATOR_CONTACT_TIMEOUT);
		sock->encode();
		if(!sock->connect(AccountantName)) {
			dprintf(D_ALWAYS,"Can't connect to accountant %s\n",
					AccountantName);
		}
		else if(!sock->put(RELINQUISH_SERVICE))
		{
			dprintf(D_ALWAYS,"Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
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
		// because capability uniquely identifies a match.
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

	if ( (universe == PVM) || (universe == MPI ) ) 
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
	int			alive = ALIVE;
	char		*id = NULL;
	
	dprintf (D_PROTOCOL,"## 6. Sending alive msg to %s\n",mrec->peer);
	id = mrec->id;
	sock.timeout(STARTD_CONTACT_TIMEOUT);
	sock.encode();
	id = mrec->id;
	if( !sock.connect(mrec->peer) ||
		!sock.put(alive) || 
		!sock.code(id) || 
		!sock.end_of_message() ) {
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

	matches->startIterations();
	while (matches->iterate(mrec) == 1) {
		if( mrec->status == M_ACTIVE || mrec->status == M_CLAIMED ) {
			if( sendAlive( mrec ) ) {
				numsent++;
			}
		}
	}
	dprintf( D_PROTOCOL, "## 6. (Done sending alive messages to "
			 "%d startds)\n", numsent );

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
		dprintf( D_ALWAYS, 
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
	intoAd ( ad, "MaxJobsRunning", MaxJobsRunning );
	intoAd ( ad, "JobsStarted", JobsStarted );
	intoAd ( ad, "SwapSpace", SwapSpace );
	intoAd ( ad, "ShadowSizeEstimate", ShadowSizeEstimate );
	intoAd ( ad, "SwapSpaceExhausted", SwapSpaceExhausted );
	intoAd ( ad, "ReservedSwap", ReservedSwap );
	intoAd ( ad, "JobsIdle", JobsIdle );
	intoAd ( ad, "JobsRunning", JobsRunning );
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
	intoAd ( ad, "CondorViewHost", CondorViewHost );
	intoAd ( ad, "Shadow", Shadow );
	intoAd ( ad, "CondorAdministrator", CondorAdministrator );
	intoAd ( ad, "Mail", Mail );
	intoAd ( ad, "filename", filename );
	intoAd ( ad, "AccountantName", AccountantName );
	intoAd ( ad, "UidDomain", UidDomain );
	intoAd ( ad, "MaxFlockLevel", MaxFlockLevel );
	intoAd ( ad, "FlockLevel", FlockLevel );
	intoAd ( ad, "alive_interval", alive_interval );
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


