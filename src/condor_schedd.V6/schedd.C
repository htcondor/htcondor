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
#include "environ.h"
#include "condor_updown.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"
#include "renice_self.h"
#include "user_log.c++.h"
#include "access.h"
#include "internet.h"
#include "condor_ckpt_name.h"
#include "../condor_ckpt_server/server_interface.h"

#define DEFAULT_SHADOW_SIZE 125

#define SUCCESS 1
#define CANT_RUN 0

static char *_FileName_ = __FILE__;	/* Used by EXCEPT (see except.h)	  */
extern char *gen_ckpt_name();

#include "condor_qmgr.h"

extern "C"
{
/*	int SetCkptServerHost(const char *host);
	int RemoveLocalOrRemoteFile(const char *, const char *);
	int FileExists(const char *, const char *);
	char* gen_ckpt_name(char*, int, int, int);
	int getdtablesize();
*/
	int calc_virt_memory();
	void handle_q(Service *, int, Stream *sock);
	int prio_compar(prio_rec*, prio_rec*);
}

extern int get_job_prio(ClassAd *ad);
extern void	FindRunnableJob(int, int&);
extern int Runnable(PROC_ID*);
extern bool	operator==( PROC_ID, PROC_ID );

extern char* Spool;
extern char * Name;
extern char * JobHistoryFileName;
extern char * mySubSystem;

extern FILE *DebugFP;
extern char *DebugFile;
extern char *DebugLock;

extern Scheduler scheduler;

// priority records
extern prio_rec *PrioRec;
extern int N_PrioRecs;
extern int grow_prio_recs(int);

void cleanup_ckpt_files(int , int , char*);
void send_vacate(match_rec*, int);
void mark_job_stopped(PROC_ID*);
shadow_rec * find_shadow_rec(PROC_ID*);
shadow_rec * add_shadow_rec(int, PROC_ID*, match_rec*, int);


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


match_rec::match_rec(char* i, char* p, PROC_ID* id)
{
	strcpy(this->id, i);
	strcpy(peer, p);
	cluster = id->cluster;
	proc = id->proc;
	status = M_INACTIVE;
	agentPid = -1;
	shadowRec = NULL;
	alive_countdown = 0;
	num_exceptions = 0;
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
	JobsStarted = 0;
	JobsRunning = 0;
	ReservedSwap = 0;
	SwapSpace = 0;

	ShadowSizeEstimate = 0;

	LastTimeout = time(NULL);
	CondorViewHost = NULL;
	CollectorHost = NULL;
	NegotiatorHost = NULL;
	Shadow = NULL;
	CondorAdministrator = NULL;
	Mail = NULL;
	filename = NULL;
	aliveInterval = 0;
	ExitWhenDone = FALSE;
	matches = NULL;
	shadowsByPid = NULL;

	shadowsByProcID = NULL;
	numMatches = 0;
	numShadows = 0;
	IdleSchedUniverseJobIDs = NULL;
	FlockHosts = NULL;
	FlockLevel = 0;
	MaxFlockLevel = 0;
	StartJobTimer=-1;
	timeoutid = -1;
	startjobsid = -1;
}

Scheduler::~Scheduler()
{
	delete ad;
	if (MySockName)
		free(MySockName);
	if (MyShadowSockName)
		free(MyShadowSockName);
	if (shadowCommandrsock && daemonCore) {
		daemonCore->Cancel_Socket(shadowCommandrsock);
		delete shadowCommandrsock;
	}
	if (shadowCommandssock && daemonCore) {
		daemonCore->Cancel_Socket(shadowCommandssock);
		delete shadowCommandssock;
	}
	if (CondorViewHost)
		free(CondorViewHost);
	if (CollectorHost)
		free(CollectorHost);
	if (NegotiatorHost)
		free(NegotiatorHost);
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
	if (FlockHosts) {
		delete FlockHosts;
	}
}

void
Scheduler::timeout()
{
	count_jobs();

	/* Neither of these calls (reaper or clean_shadow_recs) should be needed! */
#ifndef WIN32
	reaper( 0 );
#endif
	clean_shadow_recs();	

	if( (numShadows-SchedUniverseJobsRunning) > MaxJobsRunning ) {
		dprintf(D_ALWAYS,"Preempting %d jobs due to MAX_JOBS_RUNNING change\n",
			numShadows-SchedUniverseJobsRunning);
		preempt( numShadows - MaxJobsRunning );
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
	int		i;
	int		prio_compar();
	char	tmp[512];

	static int OldFlockLevel = 0;

	 // copy owner data to old-owners table
	 OwnerData OldOwners[MAX_NUM_OWNERS];
	 int Old_N_Owners=N_Owners;
	for ( i=0; i<N_Owners; i++) {
		OldOwners[i].Name = Owners[i].Name;
	}

	N_Owners = 0;
	JobsRunning = 0;
	JobsIdle = 0;
	SchedUniverseJobsIdle = 0;
	SchedUniverseJobsRunning = 0;

	// clear owner table contents
	for ( i=0; i<MAX_NUM_OWNERS; i++) {
		Owners[i].Name = NULL;
		Owners[i].JobsRunning = 0;
		Owners[i].JobsIdle = 0;
	}

	WalkJobQueue((int(*)(ClassAd *)) count );

	dprintf( D_FULLDEBUG, "JobsRunning = %d\n", JobsRunning );
	dprintf( D_FULLDEBUG, "JobsIdle = %d\n", JobsIdle );
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
	sprintf(tmp, "%s = %d", ATTR_TOTAL_IDLE_JOBS, JobsIdle);
	ad->Insert (tmp);
	sprintf(tmp, "%s = %d", ATTR_TOTAL_RUNNING_JOBS, JobsRunning);
	ad->Insert (tmp);

	update_central_mgr(UPDATE_SCHEDD_AD, CollectorHost,
						COLLECTOR_UDP_COMM_PORT); // always send
	dprintf( D_FULLDEBUG, 
			 "Sent HEART BEAT ad to central mgr: Number of submittors=%d\n",
			 N_Owners );

	// The per user queue ads should not have NumUsers in them --- only
	// the schedd ad should.  In addition, they should not have TotalRunningJobs
	// and TotalIdleJobs
	ad->Delete (ATTR_NUM_USERS);
	ad->Delete (ATTR_TOTAL_RUNNING_JOBS);
	ad->Delete (ATTR_TOTAL_IDLE_JOBS);

	// when all jobs are finished, reset FlockLevel
	if (N_Owners == 0) {
		if (FlockLevel > 0) {
			dprintf(D_ALWAYS,
					"all jobs are finished; flock level reset to 0\n");
			FlockLevel = 0;
		}
	}

	for ( i=0; i<N_Owners; i++) {

		// TODO: set running job counts with flocking in mind

	  sprintf(tmp, "%s = %d", ATTR_RUNNING_JOBS, Owners[i].JobsRunning);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS, Owners[i].JobsIdle);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, Owners[i].Name, UidDomain);
	  dprintf (D_FULLDEBUG, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  update_central_mgr(UPDATE_SUBMITTOR_AD, CollectorHost,
						 COLLECTOR_UDP_COMM_PORT);

	  dprintf( D_ALWAYS, "Sent ad to central manager for %s@%s\n", 
				Owners[i].Name, UidDomain );

	  // condor view uses the acct port - because the accountant today is not
	  // an independant daemon. In the future condor view will be the
	  // accountant

		  // The CondorViewHost MAY BE NULL!!!!!  It's optional
		  // whether you define it or not.  This will cause a seg
		  // fault if we assume it's defined and use it.  
		  // -Derek Wright 11/4/98 
	  if( CondorViewHost ) {
		  update_central_mgr( UPDATE_SUBMITTOR_AD, CondorViewHost,
							  CONDOR_VIEW_PORT );
	  }

	  // Request matches from other pools if FlockLevel > 0
	  if (FlockHosts && FlockLevel > 0) {
		  char *host;
		  int i;
		  for (i=1, FlockHosts->rewind();
				i <= FlockLevel && (host = FlockHosts->next()); i++) {
			  update_central_mgr(UPDATE_SUBMITTOR_AD, host,
								 COLLECTOR_UDP_COMM_PORT);
		  }
	  }

	  // Cancel requests when FlockLevel decreases
	  if (OldFlockLevel > FlockLevel) {
		  char *host;
		  int i;
		  sprintf(tmp, "%s = 0", ATTR_RUNNING_JOBS);
		  ad->InsertOrUpdate(tmp);
		  sprintf(tmp, "%s = 0", ATTR_IDLE_JOBS);
		  ad->InsertOrUpdate(tmp);
		  for (i=1, FlockHosts->rewind();
				i <= OldFlockLevel && (host = FlockHosts->next()); i++) {
			  if (i > FlockLevel) {
				  update_central_mgr(UPDATE_SUBMITTOR_AD, host,
									 COLLECTOR_UDP_COMM_PORT);
			  }
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
	// This is fone by looking at the old owners list and searching for owners
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
	  update_central_mgr(UPDATE_SUBMITTOR_AD, CollectorHost,
						 COLLECTOR_UDP_COMM_PORT);

	  // also update all of the flock hosts
	  char *host;
	  int i;
	
	  if (FlockHosts) {
		 for (i=1, FlockHosts->rewind();
			  i <= OldFlockLevel && (host = FlockHosts->next()); i++) {
			 update_central_mgr(UPDATE_SUBMITTOR_AD, host,
			  					COLLECTOR_UDP_COMM_PORT);
		 }
	  }
	}

	OldFlockLevel = FlockLevel;

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

	job->LookupInteger(ATTR_JOB_STATUS, status);
	job->LookupString(ATTR_OWNER, buf);
	owner = buf;
	if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) < 0) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) < 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}
	if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) < 0) {
		universe = STANDARD;
	}
	
	if (universe == SCHED_UNIVERSE) {
		// don't count DELETED or HELD jobs
		if (status == IDLE || status == UNEXPANDED || status == RUNNING) {
			scheduler.SchedUniverseJobsRunning += cur_hosts;
			scheduler.SchedUniverseJobsIdle += (max_hosts - cur_hosts);
		}
	} else {
		// don't count DELETED or HELD jobs
		if (status == IDLE || status == UNEXPANDED || status == RUNNING) {
			scheduler.JobsRunning += cur_hosts;
			scheduler.JobsIdle += (max_hosts - cur_hosts);

			// Per owner info (This is actually *submittor*
			// information.  With NiceUser's, the number of owners is
			// not the same as the number of submittors.  So, we first
			// check if this job is being submitted by a NiceUser, and
			// if so, insert it as a new entry in the "Owner" table
			if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
				strcpy(buf2,NiceUserName);
				strcat(buf2,".");
				strcat(buf2,owner);
				owner=buf2;		
			}

			int OwnerNum = scheduler.insert_owner( owner );
			scheduler.Owners[OwnerNum].JobsRunning += cur_hosts;
			scheduler.Owners[OwnerNum].JobsIdle += (max_hosts - cur_hosts);
		}
	}
	return 0;
}

int
Scheduler::insert_owner(char* owner)
{
	int		i;
	for ( i=0; i<N_Owners; i++ ) {
		if( strcmp(Owners[i].Name,owner) == MATCH ) {
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
Scheduler::update_central_mgr(int command, char *host, int port)
{
	// If the host we're given is NULL, just return, don't seg fault. 
	if( !host ) {
		return;
	}
	SafeSock	sock(host, port);
	sock.timeout( 30 );
	sock.encode();
	if( !sock.put(command) ||
		!ad->put(sock) ||
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

	GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_STATUS, &mode);

	if ((srec = scheduler.FindSrecByProcID(job_id)) != NULL) {

		// PVM jobs may not have a match  -Bin
		if (! IsSchedulerUniverse(srec)) {

			/* if there is a match printout the info */
			if (srec->match)
				dprintf( D_ALWAYS,
					"Found shadow record for job %d.%d, host = %s\n",
					job_id.cluster, job_id.proc, srec->match->peer);
			else 
				dprintf( D_ALWAYS,
					"This job does not have a match -- It may be a PVM job.\nFound shadow record for job %d.%d\n",
					job_id.cluster, job_id.proc);
		  
#ifdef WANT_DC_PM
			if ( daemonCore->Send_Signal( srec->pid, DC_SIGUSR1 ) == FALSE )
#else
			if ( kill( srec->pid, SIGUSR1) == -1 )
#endif
				dprintf(D_ALWAYS,
					"Error in sending SIGUSR1 to %d errno = %d\n",
					srec->pid, errno);
			else 
				dprintf(D_ALWAYS, "Sent SIGUSR1 to Shadow Pid %d\n",
					srec->pid);

		} else {  // Scheduler universe job

			dprintf( D_ALWAYS,
				"Found record for scheduler universe job %d.%d\n",
				job_id.cluster, job_id.proc);

			char owner[_POSIX_PATH_MAX];
			GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);
			dprintf(D_FULLDEBUG,"Sending SIGUSR1 to scheduler universe job pid=%d owner=%s\n",srec->pid,owner);
			init_user_ids(owner);
			priv_state priv = set_user_priv();
#ifdef WANT_DC_PM
			daemonCore->Send_Signal( srec->pid, DC_SIGUSR1 );
#else
			kill( srec->pid, SIGUSR1 );
#endif
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


// Write to userlog
bool Scheduler::WriteAbortToUserLog(PROC_ID job_id)
{
	char tmp[_POSIX_PATH_MAX];

	 if (GetAttributeString(job_id.cluster, job_id.proc, ATTR_ULOG_FILE,
							tmp) < 0) {
		// if there is no userlog file defined, then our work is done...
		return true;
	}
	
	char owner[_POSIX_PATH_MAX];
	char logfilename[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];

	 GetAttributeString(job_id.cluster, job_id.proc, ATTR_JOB_IWD, iwd);
	if (tmp[0] == '/') {
		strcpy(logfilename, tmp);
	} else {
		sprintf(logfilename, "%s/%s", iwd, tmp);
	}

	GetAttributeString(job_id.cluster, job_id.proc, ATTR_OWNER, owner);

	dprintf(D_FULLDEBUG,"Writing abort record to user logfile=%s owner=%s\n",logfilename,owner);

	 UserLog* ULog=new UserLog();
	ULog->initialize(owner, logfilename, job_id.cluster, job_id.proc, 0);

	JobAbortedEvent event;
	int status=ULog->writeEvent(&event);

	delete ULog;

	if (!status) return false;
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
		char constraint[120];

		// This could take a long time if the queue is large; do the
		// end_of_message first so condor_rm does not timeout. We do not
		// need any more info off of the socket anyway.
		s->end_of_message();

		dprintf(D_FULLDEBUG,"abort_job: asked to abort all status REMOVED/HELD jobs\n");

		sprintf(constraint,"%s == %d || %s == %d",ATTR_JOB_STATUS,REMOVED,
				ATTR_JOB_STATUS,HELD);

		ad = GetNextJobByConstraint(constraint,1);
		while ( ad ) {
			if ( (ad->LookupInteger(ATTR_CLUSTER_ID,job_id.cluster) == 1) &&
				 (ad->LookupInteger(ATTR_PROC_ID,job_id.proc) == 1) ) {

				 abort_job_myself(job_id);

			}
			FreeJobAd(ad);
			ad = NULL;
			ad = GetNextJobByConstraint(constraint,0);
		}
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
			(SocketHandlercpp)delayedNegotiatorHandler,
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
								(SocketHandlercpp)negotiatorSocketHandler,
								"<Negotiator Command>",
								this, ALLOW);
	}
	return rval;
}


/*
** The negotiator wants to give us permission to run a job on some
** server.  We must negotiate to try and match one of our jobs with a
** server which is capable of running it.  NOTE: We must keep job queue
** locked during this operation.
*/
int
Scheduler::negotiate(int, Stream* s)
{
	int		i;
	int		op;
	PROC_ID	id;
	char*	capability = NULL;			// capability for each match made
	char*	host = NULL;
	char*	tmp;
	int		jobs;						// # of jobs that CAN be negotiated
	int		cur_cluster = -1;
	int		start_limit_for_swap;
	int		cur_hosts;
	int		max_hosts;
	int		host_cnt;
	int		perm_rval;
	int		curr_num_active_shadows;
	int		shadow_num_increment;
	int		job_universe;
	int		which_negotiator = 0; 		// >0 implies flocking
	int		serviced_other_commands = 0;	

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

	if (FlockHosts) {
		// first, check if this is our local negotiator
		struct in_addr endpoint_addr = (s->endpoint())->sin_addr;
		struct hostent *hent;
		char *addr;
		bool match = false;
		hent = gethostbyname(NegotiatorHost);
		if (hent->h_addrtype == AF_INET) {
			for (int a=0; !match && (addr = hent->h_addr_list[a]); a++) {
				if (memcmp(addr, &endpoint_addr, sizeof(struct in_addr)) == 0){
					match = true;
				}
			}
		}
		// if it isn't our local negotiator, check the FlockHosts list
		if (!match) {
			char *host;
			int n;
			for (n=1, FlockHosts->rewind();
				 !match && (host = FlockHosts->next());
				 n++) {
				hent = gethostbyname(host);
				if (hent->h_addrtype == AF_INET) {
					for (int a=0;
						 !match && (addr = hent->h_addr_list[a]);
						 a++) {
						if (memcmp(addr, &endpoint_addr,
									sizeof(struct in_addr)) == 0){
							match = true;
							which_negotiator = n;
						}
					}
				}
			}
		}
		if (!match) {
			dprintf(D_ALWAYS, "Unknown negotiator (%s).  "
					"Aborting negotiation.\n", sin_to_string(s->endpoint()));
			return (!(KEEP_STREAM));
		}
	}

	if (which_negotiator > FlockLevel) {
		dprintf( D_ALWAYS, "Aborting connection with negotiator %s due to "
				 "insufficient flock level.\n", sin_to_string(s->endpoint()));
		return (!(KEEP_STREAM));
	}

	dprintf (D_PROTOCOL, "## 2. Negotiating with CM\n");

 	/* if ReservedSwap is 0, then we are not supposed to make any
 	 * swap check, so we can avoid the expensive calc_virt_memory 
 	 * calculation -Todd, 9/97 */
 	if ( ReservedSwap != 0 ) {
 		SwapSpace = calc_virt_memory();
 	} else {
 		SwapSpace = INT_MAX;
 	}

	// figure out the number of active shadows. we do this by
	// adding the number of existing shadows + the number of shadows
	// queued up to run in the future.
	curr_num_active_shadows = numShadows + RunnableJobQueue.Length();

	N_PrioRecs = 0;

	WalkJobQueue( (int(*)(ClassAd *))job_prio );

	if( !shadow_prio_recs_consistent() ) {
		mail_problem_message();
	}

	qsort((char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]),
		  (int(*)(const void*, const void*))prio_compar);
	jobs = N_PrioRecs;

	N_RejectedClusters = 0;
	SwapSpaceExhausted = FALSE;
	JobsStarted = 0;
	if( ShadowSizeEstimate ) {
		start_limit_for_swap = (SwapSpace - ReservedSwap) / ShadowSizeEstimate;
		dprintf( D_FULLDEBUG, "*** SwapSpace = %d\n", SwapSpace );
		dprintf( D_FULLDEBUG, "*** ReservedSwap = %d\n", ReservedSwap );
		dprintf( D_FULLDEBUG, "*** Shadow Size Estimate = %d\n",ShadowSizeEstimate);
		dprintf( D_FULLDEBUG, "*** Start Limit For Swap = %d\n",
				 									start_limit_for_swap);
		dprintf( D_FULLDEBUG, "*** Current num of active shadows = %d\n",
													curr_num_active_shadows);
	}

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
	dprintf (D_ALWAYS, "Negotiating for owner: %s\n", owner);
	//-----------------------------------------------


	/* Try jobs in priority order */
	for( i=0; i < N_PrioRecs; i++ ) {

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
				// PERMISSION, which also sends the capability for the
				// match.  Do the end_of_message here, except for
				// PERMISSON.
			if( op != PERMISSION ) {
				if( !s->end_of_message() ) {
					dprintf( D_ALWAYS, "Can't receive eom from manager\n" );
					return (!(KEEP_STREAM));
				}
			}

			switch( op ) {
				 case REJECTED:
					 mark_cluster_rejected( cur_cluster );
					host_cnt = max_hosts + 1;
					break;
				case SEND_JOB_INFO: {
					/* Really, we're trying to make sure we don't have too
						many shadows running, so compare here against
						curr_num_active_shadows rather than JobsRunning 
						as in the past.  Furthermore, we do not want to use
						just numShadows, because this does not take into account
						the shadows which have been enqueued (slow shadow start) 
					 */
					if( curr_num_active_shadows >= MaxJobsRunning ) {
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							return (!(KEEP_STREAM));
						}
						dprintf( D_ALWAYS,
				"Reached MAX_JOBS_RUNNING, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						return KEEP_STREAM;
					}
					if( SwapSpaceExhausted && (ReservedSwap != 0) ) {
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							return (!(KEEP_STREAM));
						}
						dprintf( D_ALWAYS,
					"Swap Space Exhausted, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						return KEEP_STREAM;
					}
					if( ShadowSizeEstimate && JobsStarted >= start_limit_for_swap && ReservedSwap != 0) { 
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							return (!(KEEP_STREAM));
						}
						dprintf( D_ALWAYS,
				"Swap Space Estimate Reached, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						return (KEEP_STREAM);
					}
					
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
					if ( job_universe == PVM ) {
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
					/*
					 * Weiru
					 * When we receive permission, we add a match record, fork
					 * a child called agent to claim our matched resource.
					 * Originally there is a limit on how many
					 * jobs in total can be run concurrently from this schedd,
					 * and a limit on how many jobs this schedd can start in
					 * one negotiation cycle. Now these limits are adopted to
					 * apply to match records instead.
					 */
					if( !s->get(capability) ) {
						dprintf( D_ALWAYS,
								"Can't receive capability from mgr\n" );
						return (!(KEEP_STREAM));
					}
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't receive eom from mgr\n" );
						return (!(KEEP_STREAM));
					}
						// capability is in the form
						// "<xxx.xxx.xxx.xxx:xxxx>#xxxxxxx" 
						// where everything upto the # is the sinful
						// string of the startd

					dprintf( D_PROTOCOL, 
							 "## 4. Received capability %s\n", capability );

						// Pull out the sinful string.
					host = strdup( capability );
					tmp = strchr( host, '#');
					if( tmp ) {
						*tmp = '\0';
					} else {
						dprintf( D_ALWAYS, "Can't find '#' in capability!\n" );
							// What else should we do here?
						FREE( host );
						FREE( capability );
						host = NULL;
						capability = NULL;
						break;
					}
						// host should now point to the sinful string
						// of the startd we were matched with.
					perm_rval = permission(capability, owner, host, &id);
					FREE( host );
					FREE( capability );
					host = NULL;
					capability = NULL;
					JobsStarted += perm_rval;
					host_cnt++;
					curr_num_active_shadows += (perm_rval * shadow_num_increment);
					break;
				case END_NEGOTIATE:
					dprintf( D_ALWAYS, "Lost priority - %d jobs matched\n",
							JobsStarted );
#ifdef WIN32
					if( ! ExitWhenDone ) {
						if (JobsStarted) StartJobs();
					}
#endif
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

		/* Out of jobs */
	if( JobsStarted < jobs ) {
		dprintf( D_ALWAYS,
		"Out of servers - %d jobs matched, %d jobs idle\n",
							JobsStarted, jobs - JobsStarted );

		// We are unsatisfied in this pool.  Flock with less desirable pools.
		if (FlockLevel < MaxFlockLevel && FlockLevel == which_negotiator) { 
			FlockLevel++;
			dprintf(D_ALWAYS, "Increasing flock level to %d.\n",
					FlockLevel);
		}
	} else {
		// We are out of jobs.  Stop flocking with less desirable pools.
		FlockLevel = which_negotiator;

		dprintf( D_ALWAYS,
		"Out of jobs - %d jobs matched, %d jobs idle, flock level = %d\n",
							JobsStarted, jobs - JobsStarted, FlockLevel );
	}

#ifdef WIN32
		// If we're not trying to shutdown, now an agent
		// has claimed a resource, we should try to
		// activate all our claims and start jobs on them.
		// On Unix, this is done in the repear, but on WIN32
		// we do not fork the agent, so do it here.
	if( ! ExitWhenDone ) {
		if (JobsStarted) StartJobs();
	}
#endif

	return KEEP_STREAM;
}


void
Scheduler::vacate_service(int, Stream *sock)
{
	char	*capability = NULL;

	dprintf( D_ALWAYS, "Got VACATE_SERVICE from %s\n", 
			 sin_to_string(sock->endpoint()) );

	if (!sock->code(capability)) {
		dprintf (D_ALWAYS, "Failed to get capability\n");
		return;
	}
	DelMrec (capability);
	FREE (capability);
	dprintf (D_PROTOCOL, "## 7(*)  Completed vacate_service\n");
	return;
}


/*
 * Weiru
 * This function does two things.
 * 1. add a match record
 * 2. fork an agent to contact the startd
 * Returns 1 if successful (not the result of agent's negotiation), 0 if failed.
 */
int
Scheduler::permission(char* id, char *user, char* server, PROC_ID* jobId)
{
	match_rec*		mrec;						// match record pointer
#if 0
	int			lim = getdtablesize();		// size of descriptor table
	int			i;
#endif

	mrec = AddMrec(id, server, jobId);
	if(!mrec) {
		return 0;
	}
	ClassAd *jobAd = GetJobAd(jobId->cluster, jobId->proc);
	if (!jobAd) {
		dprintf(D_ALWAYS, "failed to find job %d.%d\n", 
				jobId->cluster, jobId->proc);
		return 0;
	}

	// The above GetJobAd() does different things if we are a qmgmt client
	// or server.  In the client, it allocates memory for a new ad and therefore
	// must be deleted when we are done.  On the server side, the server being
	// the schedd where we now are, GetJobAd() does _not_ allocate any new memory
	// and instead simply returns a pointer to the ad which is already in memory.
	// We must not delete this ad (now pointed to by jobAd), which is why
	// FreeJobAd() is a no-op on the server side.  So, we make a copy of the ad
	// into jobAdCopy for the agent (which must be deleted).  This is so the 
	// Agent can run in a thread without the need of a semaphore to protect
	// the in-memory queue. -Todd 11/98
	// ClassAd *jobAdCopy = new ClassAd(*jobAd);	// make a copy for Agent

#ifdef WIN32

	// Note: this is going to tie up the negotiator until we do it in a new thread
	switch (Agent(server, id, MySockName, user, aliveInterval, jobAd)) {
	case EXITSTATUS_OK:
		dprintf(D_ALWAYS, "Agent contacting startd successful\n");
		mrec->status = M_ACTIVE;
		// delete jobAdCopy;
		return 1;
		break;
	default:
		dprintf(D_ALWAYS, "capability rejected by startd\n");
		DelMrec(mrec);
		// delete jobAdCopy;
		return 0;
		break;
	}

#else

	int	pid;
	switch((pid = fork())) 	{
		 case -1:	/* error */

				if(errno == ENOMEM) {
					 dprintf(D_ALWAYS, "fork() failed, due to lack of swap space\n");
					 swap_space_exhausted();
				} else {
					 dprintf(D_ALWAYS, "fork() failed, errno = %d\n", errno);
				}
			DelMrec(mrec);
			FreeJobAd(jobAd);
			// delete jobAdCopy;
			return 0;

		  case 0:	  /* the child */

				close(0);

			/* don't do these closes here because we still need the
				DebugLock fd in dprintf.c.  -Jim B. */
#if 0
				for(i = 3; i < lim; i++) {
					 close(i);
				}
#endif

				exit( Agent(server, id, MySockName, user, aliveInterval, jobAd) );
			

		  default:	 /* the parent */

			dprintf(D_FULLDEBUG,"Forked Agent with pid=%d for job=%d.%d\n",pid,jobId->cluster,jobId->proc);
				mrec->agentPid = pid;
			FreeJobAd(jobAd);
			// delete jobAdCopy;	// THREAD SHARED MEMORY WARNING
			return 1;
	}
#endif

	return 0;	/* should never reach here */
}


int
find_idle_sched_universe_jobs( ClassAd *job )
{
	int	status;
	int	cur_hosts;
	int	max_hosts;
	int	universe;
	PROC_ID id;
	bool already_found = false;

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

	// don't count DELETED or HELD jobs
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

	dprintf(D_FULLDEBUG, "-------- Begin starting jobs --------\n");
	matches->startIterations();
	while(matches->iterate(rec) == 1) {
		if(rec->status == M_INACTIVE)
		{
			dprintf(D_FULLDEBUG, "match (%s) inactive\n", rec->id);
			continue;
		}
		if(rec->shadowRec)
		{
			dprintf(D_FULLDEBUG, "match (%s) already running a job\n",
					rec->id);
			continue;
		}

		// This is the case we want to try and start a job.
		id.cluster = rec->cluster;
		id.proc = rec->proc; 
		if(!Runnable(&id))
		// find the job in the cluster with the highest priority
		{
			FindRunnableJob(rec->cluster, id.proc);
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
		if(!(rec->shadowRec = StartJob(rec, &id)))
		// Start job failed. Throw away the match. The reason being that we
		// don't want to keep a match around and pay for it if it's not
		// functioning and we don't know why. We might as well get another
		// match.
		{
			dprintf(D_ALWAYS,"Failed to start job %s; relinquishing\n",
						rec->id);
			Relinquish(rec);
			DelMrec(rec);
			continue;
		}
		dprintf(D_FULLDEBUG, "Match (%s) - running %d.%d\n",rec->id,id.cluster,
				id.proc);
	}
	if (SchedUniverseJobsIdle > 0) {
		StartSchedUniverseJobs();
	}

	/* Reset the our Timer */
	daemonCore->Reset_Timer(startjobsid,SchedDInterval);

	dprintf(D_FULLDEBUG, "-------- Done starting jobs --------\n");
}

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
			dprintf(D_ALWAYS,"match for job %d.%d was deleted - not forking a shadow\n",job_id->cluster,job_id->proc);
			mark_job_stopped(job_id);
			delete srec;
		}
	}

	//-------------------------------
	// Actually fork the shadow
	//-------------------------------

	char	*argv[7];
	char	cluster[10], proc[10];
	int		pid;
	int		lim;

#ifdef CARMI_OPS
	struct shadow_rec *srp;
	char out_buf[80];
	int pipes[2];
	srp = find_shadow_by_cluster( job_id );
	
	if (srp != NULL) /* mean that the shadow exists */
	{
		if (!find_shadow_rec( job_id )) /* means the shadow rec for this
							process does not exist */
			add_shadow_rec( srp->pid, job_id, server, srp->conn_fd);
		  /* next write out the server cluster and proc on conn_fd */
		dprintf( D_ALWAYS, "Sending job %d.%d to shadow pid %d\n", 
			job_id->cluster, job_id->proc, srp->pid);
		 sprintf(out_buf, "%s %d %d\n", server, job_id->cluster, 
			job_id->proc);
		dprintf( D_ALWAYS, "sending %s", out_buf);
		if (write(srp->conn_fd, out_buf, strlen(out_buf)) <= 0)
			dprintf(D_ALWAYS, "error in write %d \n", errno);
		else
			dprintf(D_ALWAYS, "done writting to %d \n", srp->conn_fd);
		return 1;	
	}
	
	 /* now the case when the shadow is absent */
	socketpair(AF_UNIX, SOCK_STREAM, 0, pipes);
	dprintf(D_ALWAYS, "Got the socket pair %d %d \n", pipes[0], pipes[1]);
#endif  // of CARMI_OPS

#ifdef WANT_DC_PM
	char	args[_POSIX_ARG_MAX];

	sprintf(args, "condor_shadow %s %s %s %d %d", MyShadowSockName, mrec->peer,
			mrec->id, job_id->cluster, job_id->proc);
	if (Shadow) free(Shadow);
	Shadow = param("SHADOW");
	pid = daemonCore->Create_Process(Shadow, args);
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
	dprintf(D_FULLDEBUG,"Queueing job %d.%d in runnable job queue\n",job_id->cluster,job_id->proc);
	shadow_rec* srec=add_shadow_rec(0,job_id, mrec,-1);
	if (RunnableJobQueue.enqueue(srec)) {
		EXCEPT("Cannot put job into run queue\n");
	}
	if (StartJobTimer<0) {
		StartJobTimer=daemonCore->Register_Timer(0,
						(Eventcpp)&Scheduler::StartJobHandler,"start_job", this);
	 }

	return srec;
}


shadow_rec*
Scheduler::start_pvm(match_rec* mrec, PROC_ID *job_id)
{

#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	char			*argv[8];
	char			cluster[10], proc_str[10];
	int				pid;
	int				i, lim;
	int				shadow_fd;
	char			out_buf[80];
	char			**ptr;
	struct shadow_rec *srp;
	int				c;							// current hosts
	int	 old_proc;  // the class in the cluster.  needed by the multi_shadow -Bin

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s...\n",
			job_id->cluster, job_id->proc, mrec->peer);
	
	if(GetAttributeInt(job_id->cluster, job_id->proc, "CurrentHosts", &c) < 0)
	{
		c = 1;
	}
	else
	{
		c++;
	}
	SetAttributeInt(job_id->cluster, job_id->proc, "CurrentHosts", c);

	dprintf(D_ALWAYS, "old_proc = %d\n", old_proc);
	// need to pass to the multi-shadow later. -Bin
	old_proc = job_id->proc;  
	
	/* For PVM/CARMI, all procs in a cluster are considered part of the same
		job, so just clear out the proc number */
	job_id->proc = 0;

	(void)sprintf( cluster, "%d", job_id->cluster );
	(void)sprintf( proc_str, "%d", job_id->proc );
	argv[0] = "condor_shadow.carmi";
	
	/* See if this job is already running */
	srp = find_shadow_rec(job_id);


	if (srp == 0) {
		int pipes[2];
		socketpair(AF_UNIX, SOCK_STREAM, 0, pipes);
		switch( (pid=fork()) ) {
			 case -1:	/* error */
				 if( errno == ENOMEM ) {
					dprintf(D_ALWAYS, 
							"fork() failed, due to lack of swap space\n");
					swap_space_exhausted();
				} else {
					dprintf(D_ALWAYS, "fork() failed, errno = %d\n" );
				}
				mark_job_stopped(job_id);
				break;
			case 0:		/* the child */
				Shadow = param("SHADOW_PVM");
				if (Shadow == 0) {
					Shadow = param("SHADOW_CARMI");
					if( !Shadow ) {
						dprintf( D_ALWAYS, 
								 "Neither SHADOW_PVM nor SHADOW_CARMI defined in config file" );
						return NULL;
					}
				}
				argv[1] = MyShadowSockName;
				argv[2] = 0;
				
				renice_shadow();

				dprintf( D_ALWAYS, "About to call: %s ", Shadow );
				for( ptr = argv; *ptr; ptr++ ) {
					dprintf( D_ALWAYS | D_NOHEADER, "%s ", *ptr );
				}
				dprintf( D_ALWAYS | D_NOHEADER, "\n" );
				
				lim = getdtablesize();
	
				(void)close( 0 );
				if (dup2( pipes[0], 0 ) < 0) {
					EXCEPT("Could not dup pipe to stdin\n");
				}
				for( i=3; i<lim; i++ ) {
					(void)close( i );
				}

				(void)execve( Shadow, argv, environ );
				dprintf( D_ALWAYS, "exec(%s) failed, errno = %d\n", Shadow, errno);
				if( errno == ENOMEM ) {
					exit( JOB_NO_MEM );
				} else {
					exit ( JOB_EXEC_FAILED );
				}
				break;
			default:	/* the parent */
				dprintf( D_ALWAYS, "Forked shadow... (shadow pid = %d)\n",
						pid );
				close(pipes[0]);
				srp = add_shadow_rec( pid, job_id, mrec, pipes[1] );
				shadow_fd = pipes[1];
				dprintf( D_ALWAYS, "shadow_fd = %d\n", shadow_fd);
				mark_job_running(job_id);
			}
	} else {
		shadow_fd = srp->conn_fd;
		dprintf( D_ALWAYS, "Existing shadow connected on fd %d\n", shadow_fd);
	}
	
	 dprintf( D_ALWAYS, "Sending job %d.%d to shadow pid %d\n", 
			job_id->cluster, job_id->proc, srp->pid);

	 sprintf(out_buf, "%d %d %d\n", job_id->cluster, job_id->proc, 1);
	
	dprintf( D_ALWAYS, "First Line: %s", out_buf );
	write(shadow_fd, out_buf, strlen(out_buf));

	sprintf(out_buf, "%s %s %d\n", mrec->peer, mrec->id, old_proc);
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
	char 	a_out_name[_POSIX_PATH_MAX];
	char 	input[_POSIX_PATH_MAX];
	char 	output[_POSIX_PATH_MAX];
	char 	error[_POSIX_PATH_MAX];
	char	env[ATTRLIST_MAX_EXPRESSION];		// fixed size is bad here!!
	char		job_args[_POSIX_ARG_MAX];
	char	args[_POSIX_ARG_MAX];
	char	owner[20], iwd[_POSIX_PATH_MAX];
	Environ	env_obj;
	char	**envp;
	char	*argv[_POSIX_ARG_MAX];
	int		fd, pid, argc;
	struct passwd	*pwd;

	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
			job_id->cluster, job_id->proc );

	strcpy(a_out_name, gen_ckpt_name(Spool, job_id->cluster, ICKPT, 0));

	// make sure file is executable
	if (chmod(a_out_name, 0755) < 0) {
		EXCEPT("chmod(%s, 0755)", a_out_name);
	}

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
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ENVIRONMENT,
							env) < 0) {
		sprintf(env, "");
	}
	env_obj.add_string(env);
	envp = env_obj.get_vector();
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ARGUMENTS,
							args) < 0) {
		sprintf(args, "");
	}
	sprintf(job_args, "%s %s", a_out_name, args);
	mkargv(&argc, argv, job_args);
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_OWNER,
							owner) < 0) {
		dprintf(D_FULLDEBUG,"Scheduler::start_sched_universe_job--setting owner"
				" to \"nobody\"\n" );
		sprintf(owner, "nobody");
	}
	if (strcmp(owner, "root") == MATCH) {
		dprintf(D_ALWAYS, "aborting job %d.%d start as root\n",
				job_id->cluster, job_id->proc);
		return NULL;
	}
	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_IWD,
							iwd) < 0) {
		sprintf(owner, "/tmp");
	}

	if ((pid = fork()) < 0) {
		dprintf(D_ALWAYS, "failed for fork for sched universe job start!\n");
		return NULL;
	}

	if (pid == 0) {		// the child

		// set dprintf messages to stderr in case something goes wrong,
		// since we can't access the SchedLog when running with user_priv

		DebugFP = stderr;
		DebugFile = NULL;
		DebugLock = NULL;

		init_user_ids(owner);
		set_user_priv_final();

		if (chdir(iwd) < 0) {
			EXCEPT("chdir(%s)", iwd);
		}
		if ((fd = open(input, O_RDONLY, 0)) < 0) {
			EXCEPT("open(%s)", input);
		}
		if (dup2(fd, 0) < 0) {
			EXCEPT("dup2(%d, 0)", fd);
		}
		if (close(fd) < 0) {
			EXCEPT("close(%d)", fd);
		}
		if ((fd = open(output, O_WRONLY, 0)) < 0) {
			EXCEPT("open(%s)", output);
		}
		if (dup2(fd, 1) < 0) {
			EXCEPT("dup2(%d, 1)", fd);
		}
		if (close(fd) < 0) {
			EXCEPT("close(%d)", fd);
		}
		if ((fd = open(error, O_WRONLY, 0)) < 0) {
			EXCEPT("open(%s)", error);
		}
		if (dup2(fd, 2) < 0) {
			EXCEPT("dup2(%d, 2)", fd);
		}
		if (close(fd) < 0) {
			EXCEPT("close(%d)", fd);
		}

		errno = 0;
		execve( a_out_name, argv, envp );
		dprintf( D_ALWAYS, "exec(%s) failed, errno = %d\n", a_out_name,
				errno );
		if( errno == ENOMEM ) {
			exit( JOB_NO_MEM );
		} else {
			exit( JOB_EXEC_FAILED );
		}
	}

	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, RUNNING);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
	return add_shadow_rec(pid, job_id, NULL, -1);
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
	
	if (pid) add_shadow_rec(new_rec);
	return new_rec;
}

struct shadow_rec *
Scheduler::add_shadow_rec( shadow_rec* new_rec )
{
	numShadows++;
	shadowsByPid->insert(new_rec->pid, new_rec);
	shadowsByProcID->insert(new_rec->job_id, new_rec);
	dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
			new_rec->pid, new_rec->job_id.cluster, new_rec->job_id.proc );
	scheduler.display_shadow_recs();
	return new_rec;
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

		if (rec->removed) {
			if (!WriteAbortToUserLog(rec->job_id)) {
				dprintf(D_ALWAYS,"Failed to write abort to the user log\n");
			}
		}
		check_zombie( pid, &(rec->job_id) );
		RemoveShadowRecFromMrec(rec);
		shadowsByPid->remove(pid);
		shadowsByProcID->remove(rec->job_id);
		if ( rec->conn_fd != -1 )
			close(rec->conn_fd);
		delete rec;
		numShadows -= 1;
		display_shadow_recs();
		return;
	}
	dprintf( D_FULLDEBUG, "Exited delete_shadow_rec( %d )\n", pid );
}

void
Scheduler::mark_job_running(PROC_ID* job_id)
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

}

/*
** Mark a job as stopped, (Idle or Unexpanded).
*/
void
mark_job_stopped(PROC_ID* job_id)
{
	int		status;
	int		orig_max;
	int		had_orig;
	char	owner[_POSIX_PATH_MAX];
	float 	cpu_time;

	had_orig = GetAttributeInt(job_id->cluster, job_id->proc, 
								ATTR_ORIG_MAX_HOSTS, &orig_max);

	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, &status);

	// if job isn't RUNNING, then our work is already done
	if (status == RUNNING) {

		if ( GetAttributeString(job_id->cluster, job_id->proc, 
				ATTR_OWNER, owner) < 0 )
		{
			dprintf(D_FULLDEBUG,"mark_job_stopped: setting owner to \"nobody\"\n");
			strcpy(owner,"nobody");
		}

		SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, IDLE);
		SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 0);
		if (had_orig >= 0) {
			SetAttributeInt(job_id->cluster, job_id->proc, ATTR_MAX_HOSTS,
							orig_max);
		}
	
		dprintf( D_FULLDEBUG, "Marked job %d.%d as IDLE\n", job_id->cluster,
				 job_id->proc );
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
	while (shadowsByPid->iterate(rec) == 1 && n > 0) {
		if( is_alive(rec) ) {
			int universe;
			GetAttributeInt(rec->job_id.cluster, rec->job_id.proc, 
							ATTR_JOB_UNIVERSE,&universe);
			if (universe == PVM) {
				if ( !rec->preempted ) {
#ifdef WANT_DC_PM
					daemonCore->Send_Signal( rec->pid, DC_SIGTERM );
#else
					kill( rec->pid, SIGTERM );
#endif
				}
				rec->preempted = TRUE;
				n--;
			} else if (rec->match) {	/* scheduler universe job check (?) */
				if( !rec->preempted ) {
					send_vacate( rec->match, CKPT_FRGN_JOB );
				}
				rec->preempted = TRUE;
				n--;
			} else if (preempt_all) {
				if ( !rec->preempted ) {
#ifdef WANT_DC_PM
					daemonCore->Send_Signal( rec->pid, DC_SIGTERM );
#else
					kill( rec->pid, SIGTERM );
#endif
					rec->preempted = TRUE;
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
	ReliSock	sock;

	dprintf( D_FULLDEBUG, "Called send_vacate( %s, %d )\n", match->peer, cmd );
	 
	/* Connect to the specified host with 20 second timeout */
	sock.timeout(20);
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
	int CurHosts, MaxHosts;

	dprintf( D_ALWAYS, "Checking consistency running and runnable jobs\n" );
	BadCluster = -1;
	BadProc = -1;

	for( i=0; i<N_PrioRecs; i++ ) {
		if( (srp=find_shadow_rec(&PrioRec[i].id)) ) {
			BadCluster = srp->job_id.cluster;
			BadProc = srp->job_id.proc;
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_UNIVERSE, &universe);
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_STATUS, &status);
			if (status != RUNNING && universe!=PVM) {
				// display_shadow_recs();
				// dprintf(D_ALWAYS,"shadow_prio_recs_consistent(): PrioRec %d - id = %d.%d, owner = %s\n",i,PrioRec[i].id.cluster,PrioRec[i].id.proc,PrioRec[i].owner);
				dprintf( D_ALWAYS, "Found a consistency problem!!!\n" );
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
	char	cmd[512];
	FILE	*mailer;

	dprintf( D_ALWAYS, "Mailing administrator (%s)\n", CondorAdministrator );

	(void)sprintf( cmd, "%s -s \"CONDOR Problem\" %s", Mail, 
					CondorAdministrator );
	if( (mailer=popen(cmd,"w")) == NULL ) {
		EXCEPT( "popen(\"%s\",\"w\")", cmd );
	}

	fprintf( mailer, "Problem with condor_schedd %s\n", Name );
	fprintf( mailer, "Job %d.%d is in the runnable job table,\n",
												BadCluster, BadProc );
	fprintf( mailer, "but we already have a shadow record for it.\n" );

		/* don't use 'pclose()' here, it does its own wait, and messes
			 with our handling of SIGCHLD! */
		/* except on HPUX it is both safe and required */
#if defined(HPUX)
	pclose( mailer );
#else
	(void)fclose( mailer );
#endif
#endif /* !defined(WIN32) */
}

void
Scheduler::NotifyUser(shadow_rec* srec, char* msg, int status, int JobStatus)
{
#if !defined(WIN32)
	int fd, notification;
	char owner[20], url[25], buf[80];
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

	if (GetAttributeString(srec->job_id.cluster, srec->job_id.proc, "cmd",
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
	sprintf(buf, "%s -s \"Condor Job %d.%d\" %s\n", Mail, srec->job_id.cluster, srec->job_id.proc, owner);
	dprintf( D_FULLDEBUG, "Notify user using cmd: %s\n",buf);
	FILE* mailer = popen( buf, "w" );
	if (!mailer) {
		EXCEPT("cannot execute %s %s\n", cmd, "w");
	}
	fprintf(mailer, "Your condor job\n" );
	fprintf(mailer, "\t%s %s\n", cmd, args );
	fprintf(mailer, "%s%d.\n", msg, status);
	pclose(mailer);

/*
	sprintf(url, "mailto:%s", owner);
	if ((fd = open_url(url, O_WRONLY, 0)) < 0) {
		EXCEPT("condor_open_mailto_url(%s, %d, 0)", owner, O_WRONLY, 0);
	}

//	sprintf(buf, "From: Condor\n");
//	write(fd, buf, strlen(buf));
//	sprintf(buf, "To: %s\n", owner);
//	write(fd, buf, strlen(buf));
	sprintf(buf, "Subject: Condor Job %d.%d\n\n", srec->job_id.cluster,
			srec->job_id.proc);
	write(fd, buf, strlen(buf));
	sprintf(buf, "Your condor job\n\t");
	write(fd, buf, strlen(buf));
	write(fd, cmd, strlen(cmd));
	write(fd, " ", 1);
	write(fd, args, strlen(args));
	write(fd, "\n", 1);
	write(fd, msg, strlen(msg));
	sprintf(buf, "%d.\n", status);
	write(fd, buf, strlen(buf));
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

void
Scheduler::child_exit(int pid, int status)
{
	match_rec*		mrec;
	shadow_rec*		srec;
	 int				StartJobsFlag=TRUE;

	mrec = FindMrecByPid(pid);
	srec = FindSrecByPid(pid);

	if(mrec) {
		if(WIFEXITED(status)) {
			dprintf(D_FULLDEBUG, "agent pid %d exited with status %d\n",
					  pid, WEXITSTATUS(status) );
			if(WEXITSTATUS(status) == EXITSTATUS_NOTOK) {
				dprintf(D_ALWAYS, "capability rejected by startd\n");
				DelMrec(mrec);
			} else {
				dprintf(D_ALWAYS, "Agent contacting startd successful\n");
				mrec->status = M_ACTIVE;
				// be certain to clear out agentPid value, otherwise
				// if this match lives long enough that the PID gets
				// reused by the operating system, we will mistake a 
				// shadow exit for an agent exit!  -Todd Tannenbaum, 3/99
				mrec->agentPid = -1;
			}
		} else if(WIFSIGNALED(status)) {
			dprintf(D_ALWAYS, "Agent pid %d died with signal %d\n",
					  pid, WTERMSIG(status));
			DelMrec(mrec);
		}
	} else if (IsSchedulerUniverse(srec)) {
		// scheduler universe process
		if(WIFEXITED(status)) {
			dprintf(D_FULLDEBUG,
					"scheduler universe job (%d.%d) pid %d "
					"exited with status %d\n", srec->job_id.cluster,
					srec->job_id.proc, pid, WEXITSTATUS(status) );
			NotifyUser(srec, "exited with status ",
					WEXITSTATUS(status) , COMPLETED);
			SetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
							ATTR_JOB_STATUS, COMPLETED);
			SetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
							ATTR_JOB_EXIT_STATUS, WEXITSTATUS(status) );
		} else if(WIFSIGNALED(status)) {
			dprintf(D_ALWAYS,
					"scheduler universe job (%d.%d) pid %d died "
					"with signal %d\n", srec->job_id.cluster,
					srec->job_id.proc, pid, WTERMSIG(status));
			NotifyUser(srec, "was killed by signal ",
						WTERMSIG(status), REMOVED);
			SetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
							ATTR_JOB_STATUS, REMOVED);
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
			case JOB_EXCEPTION:
					// some exception happened in this job --  
					// record that we had one.  This function will
					// relinquish the match if we get too many
					// exceptions 
				if( !srec->removed ) {
						// Only if this job wasn't removed, do we
						// have anything to do. 
					HadException(srec->match);
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
				SetAttributeInt( srec->job_id.cluster, srec->job_id.proc, 
								 ATTR_JOB_STATUS, REMOVED );
				break;
			case JOB_EXITED:
				SetAttributeInt( srec->job_id.cluster, srec->job_id.proc,
								 ATTR_JOB_STATUS, COMPLETED );
				break;
			}
	 	} else if( WIFSIGNALED(status) ) {
			dprintf( D_ALWAYS, "Shadow pid %d died with signal %d\n",
					 pid, WTERMSIG(status) );
		}
		delete_shadow_rec( pid );
	} else {
		// mrec and srec are NULL - agent dies after deleting match
		StartJobsFlag=FALSE;
	 }  // big if..else if...
	if( ExitWhenDone && numShadows == 0 ) {
		dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
		DC_Exit(0);
	}
		// If we're not trying to shutdown, now that either an agent
		// or a shadow (or both) have exited, we should try to
		// activate all our claims and start jobs on them.
	if( ! ExitWhenDone ) {
		if (StartJobsFlag) StartJobs();
	}
}

void
Scheduler::kill_zombie(int pid, PROC_ID* job_id )
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

void
cleanup_ckpt_files(int cluster, int proc, char *owner)
{
	 char	 ckpt_name[MAXPATHLEN];
	char	buf[_POSIX_PATH_MAX];
	char	server[_POSIX_PATH_MAX];
	char	*tmp;

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

	if (GetAttributeString(cluster, proc, ATTR_LAST_CKPT_SERVER,
							server) == 0) {
		SetCkptServerHost(server);
	} else {
		tmp = param("CKPT_SERVER_HOST");
		if (tmp) {
			SetCkptServerHost(tmp);
			free(tmp);
		}
	}

		  /* Remove any checkpoint files.  If for some reason we do 
		 * not know the owner, don't bother sending to the ckpt
		 * server.
		 */
	 strcpy(ckpt_name, gen_ckpt_name(Spool,cluster,proc,0) );
	if ( owner )
		RemoveLocalOrRemoteFile(owner,ckpt_name);
	else
		unlink(ckpt_name);

		  /* Remove any temporary checkpoint files */
	 strcat( ckpt_name, ".tmp");
	if ( owner )
		RemoveLocalOrRemoteFile(owner,ckpt_name);
	else
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

	if( CollectorHost ) free( CollectorHost );
	if( ! (CollectorHost = param("COLLECTOR_HOST")) ) {
		  EXCEPT( "No Collector host specified in config file" );
	 }

	if( NegotiatorHost ) free( NegotiatorHost );
	 if( ! (NegotiatorHost = param("NEGOTIATOR_HOST")) ) {
		  EXCEPT( "No NegotiatorHost host specified in config file" );
	 }

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
	if( UidDomain ) free( UidDomain );
	UidDomain = param( "UID_DOMAIN" );

		////////////////////////////////////////////////////////////////////
		// Grab all the optional parameters from the config file.
		////////////////////////////////////////////////////////////////////

	if( schedd_name_in_config ) {
		free( Name );
		tmp = param( "SCHEDD_NAME" );
		Name = strdup( build_valid_daemon_name(tmp) );
		free( tmp );
	} else {
		if( ! Name ) {
			tmp = param( "SCHEDD_NAME" );
			if( tmp ) {
				Name = strdup( build_valid_daemon_name(tmp) );
				schedd_name_in_config = 1;
				free( tmp );
			} else {
				Name = strdup( my_full_hostname() );
			}
		}
	}

	dprintf( D_FULLDEBUG, "Using name: %s\n", Name );

	if( AccountantName ) free( AccountantName );
	if( ! (AccountantName = param("ACCOUNTANT_HOST")) ) {
		  dprintf(D_FULLDEBUG, "No Accountant host specified in config file\n" );
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

	tmp = param( "FLOCK_HOSTS" );
	if ( tmp && tmp[0] != '\0') {
		if (FlockHosts) delete FlockHosts;
		FlockHosts = new StringList( tmp );
		MaxFlockLevel = FlockHosts->number();
	} else if (FlockHosts) {
		delete FlockHosts;
		FlockHosts = NULL;
		MaxFlockLevel = 0;
	}
	if (tmp) free(tmp);

	tmp = param( "RESERVED_SWAP" );
	 if( !tmp ) {
		  ReservedSwap = 5 * 1024;				/* 5 megabytes */
	 } else {
		  ReservedSwap = atoi( tmp ) * 1024;  /* Value specified in megabytes */
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
		aliveInterval = 300;
	 } else {
		  aliveInterval = atoi(tmp);
		  free(tmp);
	}


	tmp = param("MAX_SHADOW_EXCEPTIONS");
	 if(!tmp) {
		MaxExceptions = 5;
	 } else {
		  MaxExceptions = atoi(tmp);
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
		struct in_addr addr;
		if (get_inet_address(&addr) < 0) {
			EXCEPT("get_inet_address failed");
		}
		sprintf(nameBuf,"<%s:%d>",inet_ntoa(addr),
				shadowCommandrsock->get_port());
		MyShadowSockName = strdup( nameBuf );
	}

}

void
Scheduler::Register()
{
	 // message handlers for schedd commands
	 daemonCore->Register_Command( NEGOTIATE, "NEGOTIATE", 
			(CommandHandlercpp)doNegotiate, "negotiate", this, NEGOTIATOR );
	 daemonCore->Register_Command( RESCHEDULE, "RESCHEDULE", 
			(CommandHandlercpp)reschedule_negotiator, "reschedule_negotiator", 
										 this, WRITE);
	 daemonCore->Register_Command( RECONFIG, "RECONFIG", 
			(CommandHandler)dc_reconfig, "reconfig", 0, OWNER );
	 daemonCore->Register_Command(VACATE_SERVICE, "VACATE_SERVICE", 
			(CommandHandlercpp)vacate_service, "vacate_service", this, WRITE);
	 daemonCore->Register_Command(KILL_FRGN_JOB, "KILL_FRGN_JOB", 
			(CommandHandlercpp)abort_job, "abort_job", this, WRITE);

	// Command handler for testing file access.  I set this as WRITE as we
	// don't want people snooping the permissions on our machine.
	daemonCore->Register_Command( ATTEMPT_ACCESS, "ATTEMPT_ACCESS", 
								  (CommandHandler)attempt_access_handler, 
								  "attempt_access_handler", NULL, WRITE, 
								  D_FULLDEBUG );

	 // handler for queue management commands
	 // Note: This could either be a READ or a WRITE command.  Too bad we have 
	// to lump both together here.
	 daemonCore->Register_Command( QMGMT_CMD, "QMGMT_CMD",
								  (CommandHandler)handle_q, 
								  "handle_q", NULL, READ, D_FULLDEBUG );

	 // reaper
#ifdef WIN32
	daemonCore->Register_Reaper( "reaper", (ReaperHandlercpp)&Scheduler::child_exit,
								 "child_exit", this );
#else
	 daemonCore->Register_Signal( DC_SIGCHLD, "SIGCHLD", (SignalHandlercpp)&Scheduler::reaper, 
								 "reaper", this );
#endif

	RegisterTimers();
}

void
Scheduler::RegisterTimers()
{
	static int aliveid = -1, cleanid = -1;

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
	 timeoutid = daemonCore->Register_Timer(10,(Eventcpp)timeout,
											"timeout",this);
	startjobsid = daemonCore->Register_Timer(10,
											 (Eventcpp)StartJobs,"StartJobs",
											 this);
	 aliveid = daemonCore->Register_Timer(aliveInterval, aliveInterval,
										 (Eventcpp)send_alive, 
										 "send_alive", this);
	cleanid = daemonCore->Register_Timer(QueueCleanInterval,
										 QueueCleanInterval,
										 (Event)CleanJobQueue,
										 "CleanJobQueue");
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

	/* finally, go in order of the proc id 
	 * we already are likely in "cluster" order because of the qdate comparison 
	 * above, but all procs in a cluster have the same qdate, thus the
	 * comparison on proc id here. 
	 */
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


// Perform graceful shutdown.
void
Scheduler::shutdown_graceful()
{
	if( numShadows == 0 ) {
		dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
		DC_Exit(0);
	} else {
			/* 
				There are shadows running, so set a flag that tells the
				reaper to exit when all the shadows are gone, and start
				shutting down shadows.
 			 */
		MaxJobsRunning = 0;
		ExitWhenDone = TRUE;
		preempt( numShadows );
	}
}

// Perform fast shutdown.
void
Scheduler::shutdown_fast()
{
	shadow_rec *rec;
	shadowsByPid->startIterations();
	while (shadowsByPid->iterate(rec) == 1) {
#ifdef WANT_DC_PM
		daemonCore->Send_Signal( rec->pid, DC_SIGKILL );
#else
		kill( rec->pid, SIGKILL );
#endif
	}
	dprintf( D_ALWAYS, "All shadows have been killed, exiting.\n" );
	DC_Exit(0);
}


void
Scheduler::reschedule_negotiator(int, Stream *)
{
	 int	  	cmd = RESCHEDULE;
	ReliSock	sock(NegotiatorHost, NEGOTIATOR_PORT);

	// relsock->end_of_message();

	 dprintf( D_ALWAYS, "Called reschedule_negotiator()\n" );

	timeout();							// update the central manager now

	sock.encode();
	if (!sock.code(cmd)) {
		dprintf( D_ALWAYS,
				"failed to send RESCHEDULE command to negotiator\n");
		return;
	}
	if (!sock.eom()) {
		dprintf( D_ALWAYS,
				"failed to send RESCHEDULE command to negotiator\n");
	}

	if (SchedUniverseJobsIdle > 0) {
		StartSchedUniverseJobs();
	}

	 return;
}

match_rec*
Scheduler::AddMrec(char* id, char* peer, PROC_ID* jobId)
{
	match_rec *rec;

	if(!id || !peer)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not added\n"); 
		return NULL;
	} 
	rec = new match_rec(id, peer, jobId);
	if(!rec)
	{
		EXCEPT("Out of memory!");
	} 
	matches->insert(HashKey(id), rec);
	numMatches++;
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
	if (matches->lookup(key, rec) == 0) {

		dprintf( D_ALWAYS, "Match record (%s, %d, %d) deleted\n",
				 rec->peer, rec->cluster, rec->proc ); 
		dprintf( D_FULLDEBUG, "Capability of deleted match: %s\n", rec->id );

		matches->remove(key);
		// Remove this match from the associated shadowRec.
		if (rec->shadowRec)
			rec->shadowRec->match = NULL;
		delete rec;
		numMatches--; 
	}
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

int
Scheduler::MarkDel(char* id)
{
	match_rec *rec;

	if(!id)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not marked deleted\n");
		return -1;
	}
	if (matches->lookup(HashKey(id), rec) == 0) {
		dprintf(D_FULLDEBUG, "Match record (%s, %s, %d) marked deleted\n",
				rec->id, rec->peer, rec->cluster);
		rec->status = M_DELETED; 
	}
	return 0;
}

// The Agent() function is forked on Unix, so we need to exit our return value,
// but on WIN32 we should just return the value.  In all cases, free jobAd
// whenever we leave the Agent() function.

//#ifdef WIN32
//#define agent_exit(x) delete jobAd; return x
//#else
//#define agent_exit(x) delete jobAd; exit(x)
//#endif

#ifdef WIN32
#define agent_exit(x) return x
#else
#define agent_exit(x) exit(x)
#endif

int
Scheduler::Agent(char* server, char* capability, 
				 char* name, char *user, int aliveInterval, ClassAd* jobAd) 
{
	 int	  	reply;										/* reply from the startd */
	ReliSock 	sock;


	// add User = "owner@uiddomain" to ad
	{
		char temp[512];
		sprintf (temp, "%s = \"%s\"", ATTR_USER, user);
		jobAd->Insert (temp);
	}	

	if ( !sock.connect(server, 0) ) {
		// dprintf( D_ALWAYS, "Couldn't connect to startd.\n" );
		agent_exit(EXITSTATUS_NOTOK);
	}

	// dprintf (D_PROTOCOL, "## 5. Requesting resource from %s ...\n", server);
	sock.encode();

	if( !sock.put( REQUEST_SERVICE ) ) {
		// dprintf( D_ALWAYS, "Couldn't send command to startd.\n" );	
		agent_exit(EXITSTATUS_NOTOK);
	}

	if( !sock.code( capability ) ) {
		// dprintf( D_ALWAYS, "Couldn't send capability to startd.\n" );	
		agent_exit(EXITSTATUS_NOTOK);
	}

	if( !jobAd->put( sock ) ) {
		// dprintf( D_ALWAYS, "Couldn't send job classad to startd.\n" );	
		agent_exit(EXITSTATUS_NOTOK);
	}	
	
	if( !sock.end_of_message() ) {
		// dprintf( D_ALWAYS, "Couldn't send eom to startd.\n" );	
		agent_exit(EXITSTATUS_NOTOK);
	}

	if( !sock.rcv_int(reply, TRUE) ) {
		// dprintf( D_ALWAYS, "Couldn't receive response from startd.\n" );	
		agent_exit(EXITSTATUS_NOTOK);
	}

	if( reply == OK ) {
		// dprintf (D_PROTOCOL, "(Request was accepted)\n");
		sock.encode();
		if( !sock.code(name) ) {
			// dprintf( D_ALWAYS, "Couldn't send schedd string to startd.\n" );	
			agent_exit(EXITSTATUS_NOTOK);
		}
		if( !sock.snd_int(aliveInterval, TRUE) ) {
			// dprintf( D_ALWAYS, "Couldn't receive response from startd.\n" );	
			agent_exit(EXITSTATUS_NOTOK);
		}
	} else if( reply == NOT_OK ) {
		// dprintf( D_PROTOCOL, "(Request was NOT accepted)\n" );
		agent_exit(EXITSTATUS_NOTOK);
	} else {
		// dprintf( D_ALWAYS, "Unknown reply from startd, agent agent_exiting.\n" ); 
		agent_exit(EXITSTATUS_NOTOK);	
	}

	agent_exit(EXITSTATUS_OK);
}

match_rec*
Scheduler::FindMrecByPid(int pid)
{
	match_rec *rec;

	// We only fork an agent on Unix; thus, no need wasting
	// cycles on this if we are on WIN32.
#ifdef WIN32
	return NULL;
#endif

	matches->startIterations();
	while (matches->iterate(rec) == 1) {
		if(rec->agentPid == pid) {
			return rec;
		}
	}
	return NULL;
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
	ReliSock	*sock;
	int				flag = FALSE;

	if (!mrec) {
		dprintf(D_ALWAYS, "Scheduler::Relinquish - mrec is NULL, can't relinquish\n");
		return;
	}

	// inform the startd

	sock = new ReliSock(mrec->peer, 0);
	sock->encode();
	if(!sock->put(RELINQUISH_SERVICE))
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

	// inform the accountant
	if(!AccountantName)
	{
		dprintf(D_PROTOCOL, "## 7. No accountant to relinquish\n");
	}
	else
	{
		sock = new ReliSock(AccountantName, 0);
		sock->encode();
		if(!sock->put(RELINQUISH_SERVICE))
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
Scheduler::RemoveShadowRecFromMrec(shadow_rec* shadow)
{
	bool		found = false;
	match_rec	*rec;

	matches->startIterations();
	while (matches->iterate(rec) == 1) {
		if(rec->shadowRec == shadow) {
			rec->shadowRec = NULL;
			rec->proc = -1;
			found = true;
		}
	}
	if (!found) {
		dprintf(D_FULLDEBUG, "Shadow does not have a match record, so did not remove it from the match\n");
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

	if (universe == PVM)
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
void
Scheduler::send_alive()
{
	SafeSock	*sock;
	 int	  	numsent=0;
	int			alive = ALIVE;
	char		*id = NULL;
	match_rec	*rec;

	matches->startIterations();
	while (matches->iterate(rec) == 1) {
		if(rec->status != M_ACTIVE)	{
			continue;
		}
		dprintf (D_PROTOCOL,"## 6. Sending alive msg to %s\n",rec->peer);
		numsent++;
		id = rec->id;
		sock = new SafeSock(rec->peer, 0);
		sock->encode();
		id = rec->id;
		if( !sock->put(alive) || 
			!sock->code(id) || 
			!sock->end_of_message() ) {
				// UDP transport out of buffer space!
			dprintf(D_ALWAYS, "\t(Can't send alive message to %d)\n",
					rec->peer);
			delete sock;
			continue;
		}
			/* TODO: Someday, espcially once the accountant is done, the startd
				should send a keepalive ACK back to the schedd.  if there is no shadow
				to this machine, and we have not had a startd keepalive ACK in X amount
				of time, then we should relinquish the match.  Since the accountant 
				is not done and we are in fire mode, leave this for V6.1.  :^) -Todd 9/97
				*/
		delete sock;
	 }
	 dprintf(D_PROTOCOL,"## 6. (Done sending alive messages to %d startds)\n",
			numsent);
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
