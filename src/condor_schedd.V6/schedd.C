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

#define DEFAULT_SHADOW_SIZE 125

#define SUCCESS 1
#define CANT_RUN 0

const int Scheduler::MPIShadowSockTimeout = 60;

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
	shadowRec = NULL;
	alive_countdown = 0;
	num_exceptions = 0;
        // this is a HACK.  True if it's MPI and matched, but can be true
        // before the Mpi shadow starts.  Used so that we don't try to 
        // start the mpi shadow every time through StartJob()...
    isMatchedMPI = FALSE;
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
	Collector = NULL;
	Negotiator = NULL;
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

	reschedule_request_pending = false;
	startJobsDelayBit = FALSE;
	numRegContacts = 0;
#ifndef WIN32
	MAX_STARTD_CONTACTS = getdtablesize() - 20;  // save 20 fds...
#else
	// on Windows NT, it appears you can open up zillions of sockets
	MAX_STARTD_CONTACTS = 2000;
#endif
	checkContactQueue_tid = -1;
    storedMatches = new HashTable < int, ExtArray<match_rec*> *> 
                                                  ( 5, mpiHashFunc );
	sent_shadow_failure_email = FALSE;
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
	if (FlockHosts) {
		delete FlockHosts;
	}
	if ( checkContactQueue_tid != -1 && daemonCore ) {
		daemonCore->Cancel_Timer(checkContactQueue_tid);
	}

        // for the stored mpi matches
    ExtArray<match_rec*> *foo;
    storedMatches->startIterations();
    while( storedMatches->iterate( foo ) )
        delete foo;

    delete storedMatches;
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
	if( ((numShadows-SchedUniverseJobsRunning) > MaxJobsRunning) && 
					(!ExitWhenDone) ) {
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

		// Port doesn't matter, since we've got the sinful string. 
	update_central_mgr( UPDATE_SCHEDD_AD, Collector->addr(), 0 );
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

		  // Port doesn't matter, since we've got the sinful string. 
	  update_central_mgr( UPDATE_SUBMITTOR_AD, Collector->addr(), 0 );

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
				  // Port doesn't matter, since we've got the sinful string. 
			  update_central_mgr( UPDATE_SUBMITTOR_AD, host, 
								  COLLECTOR_UDP_COMM_PORT );
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
				  update_central_mgr( UPDATE_SUBMITTOR_AD, host,
									  COLLECTOR_UDP_COMM_PORT );
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
		  // Port doesn't matter, since we've got the sinful string. 
	  update_central_mgr( UPDATE_SUBMITTOR_AD, Collector->addr(), 0 );

	  // also update all of the flock hosts
	  char *host;
	  int i;
	
	  if (FlockHosts) {
		 for (i=1, FlockHosts->rewind();
			  i <= OldFlockLevel && (host = FlockHosts->next()); i++) {
			 update_central_mgr( UPDATE_SUBMITTOR_AD, host,
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

	if (job->LookupInteger(ATTR_JOB_STATUS, status) < 0) {
		dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				ATTR_JOB_STATUS);
		return 0;
	}
	if (job->LookupString(ATTR_OWNER, buf) < 0) {
		dprintf(D_ALWAYS, "Job has no %s attribute.  Ignoring...\n",
				ATTR_OWNER);
		return 0;
	}
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
Scheduler::update_central_mgr( int command, char *host, int port )
{
	// If the host we're given is NULL, just return, don't seg fault. 
	if( !host ) {
		return;
	}
	SafeSock sock;
	sock.timeout(NEGOTIATOR_CONTACT_TIMEOUT);
	sock.encode();
	if( !sock.connect(host, port) ||
		!sock.put(command) ||
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

    dprintf ( D_FULLDEBUG, "abort_job_myself: %d.%d\n", 
              job_id.cluster, job_id.proc );

	if ( GetAttributeInt(job_id.cluster, job_id.proc, 
                         ATTR_JOB_STATUS, &mode) == -1 ) {
        dprintf ( D_ALWAYS, "tried to abort %d.%d; not found.\n", 
                  job_id.cluster, job_id.proc );
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
			dprintf(D_FULLDEBUG,
				"Sending SIGUSR1 to scheduler universe job"
				" pid=%d owner=%s\n",srec->pid,owner);
			init_user_ids(owner);
			priv_state priv = set_user_priv();
			if ( daemonCore->Send_Signal( srec->pid, DC_SIGUSR1 ) == TRUE ) {
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

	owner[0] = '\0';
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
	Sock*	sock = (Sock*)s;
	contactStartdArgs * args;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

	if (!FlockHosts) {
		reschedule_request_pending = false;
	}

	if (FlockHosts) {
		// first, check if this is our local negotiator
		struct in_addr endpoint_addr = (sock->endpoint())->sin_addr;
		struct hostent *hent;
		char *addr;
		bool match = false;
		hent = gethostbyname(Negotiator->fullHostname());
		if (hent->h_addrtype == AF_INET) {
			for (int a=0; !match && (addr = hent->h_addr_list[a]); a++) {
				if (memcmp(addr, &endpoint_addr, sizeof(struct in_addr)) == 0){
					match = true;
						// since we now know we have heard back from our 
						// local negotiator, we must clear the 
						// reschedule_request_pending flag to
						// permit subsequent reschedule commands to occur.
					reschedule_request_pending = false;
				}
			}
		}
		// if it isn't our local negotiator, check the FlockHosts list.
		// note we do _not_ clear the reschedule_request_pending flag here, since
		// a reschedule event means we want to hear from our local negotiator, not
		// a flocking negotiator.
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
					"Aborting negotiation.\n", sin_to_string(sock->endpoint()));
			return (!(KEEP_STREAM));
		}
	}

	if (which_negotiator > FlockLevel) {
		dprintf( D_ALWAYS, "Aborting connection with negotiator %s due to "
				 "insufficient flock level.\n", sin_to_string(sock->endpoint()));
		return (!(KEEP_STREAM));
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
					if ( ( job_universe == PVM ) || ( job_universe == MPI) ) {
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
					 * If things are cool, contact the startd.
					 */
					dprintf ( D_FULLDEBUG, "In case PERMISSION\n" );

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

					// CLAIMING LOGIC

					/* Here we don't want to call contactStartd directly
					   because we do not want to block the negotiator for 
					   this, and because we want to minimize the possibility
					   that the startd will have to block/wait for the 
					   negotiation cycle to finish before it can finish
					   the claim protocol.  So...we enqueue the
					   args for a later call.  (The later call will be
					   made from the startdContactSockHandler) */
					args = 	new contactStartdArgs;
					dprintf (D_FULLDEBUG, "Queuing contactStartd args=%x, "
							 "startd=%s\n", args, host );

					args->capability = capability;
					args->owner = strdup( owner );
					args->host = host;
					args->id.cluster = id.cluster;
					args->id.proc = id.proc;
					if ( startdContactQueue.enqueue( args ) < 0 ) {
						perm_rval = 0;	// failure
						dprintf(D_ALWAYS,"Failed to enqueue contactStartd "
							"args=%x, startd=%s\n", args, host);
					} else {
						perm_rval = 1;	// happiness
						// if we havn't already done so, register a timer
						// to go off in zero seconds to call checkContactQueue.
						// this will start the process of claiming the startds
						// _after_ we have completed negotiating.
						if ( checkContactQueue_tid == -1 ) {
							checkContactQueue_tid = 
								daemonCore->Register_Timer(
									0,
									(TimerHandlercpp)&Scheduler::checkContactQueue,
									"checkContactQueue",
									this);
						}
					}

					JobsStarted += perm_rval;
					curr_num_active_shadows += 
						(perm_rval * shadow_num_increment);
					host_cnt++;

					host = NULL;
					capability = NULL;
					break;

				case END_NEGOTIATE:
					dprintf( D_ALWAYS, "Lost priority - %d jobs matched\n",
							JobsStarted );
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
	DelMrec (capability);
	FREE (capability);
	dprintf (D_PROTOCOL, "## 7(*)  Completed vacate_service\n");
	return;
}

#ifdef BAILOUT   /* this is kinda hack-like, but we need to do it a LOT. */
#undef BAILOUT
#endif
#define BAILOUT              \
		DelMrec( mrec );     \
        free( capability );  \
        free( user );        \
        free( server );      \
        delete sock;         \
        return 0;

int
Scheduler::contactStartd( char* capability, char *user, 
						  char* server, PROC_ID* jobId)
{
	match_rec* mrec;   // match record pointer
	ReliSock *sock = new ReliSock();

	dprintf ( D_FULLDEBUG, "In Scheduler::contactStartd.\n" );

    dprintf ( D_FULLDEBUG, "%s %s %s %d.%d\n", capability, user, server, 
              jobId->cluster, jobId->proc );

	mrec = AddMrec(capability, server, jobId);
	if(!mrec) {
        free( capability );
        free( user );
        free( server );
        delete sock;
        return 0;
	}
	ClassAd *jobAd = GetJobAd(jobId->cluster, jobId->proc);
	if (!jobAd) {
		dprintf(D_ALWAYS, "failed to find job %d.%d\n", 
				jobId->cluster, jobId->proc);
		BAILOUT;
	}

	// add User = "owner@uiddomain" to ad
    char temp[512];
    sprintf (temp, "%s = \"%s\"", ATTR_USER, user);
    jobAd->Insert (temp);

	sock->timeout( STARTD_CONTACT_TIMEOUT );

	if ( !sock->connect(server, 0) ) {
		dprintf( D_ALWAYS, "Couldn't connect to startd.\n" );
		BAILOUT;
	}

	dprintf (D_PROTOCOL, "Requesting resource from %s ...\n", server);
	sock->encode();

	if( !sock->put( REQUEST_SERVICE ) ) {
		dprintf( D_ALWAYS, "Couldn't send command to startd.\n" );	
		BAILOUT;
	}

	if( !sock->code( capability ) ) {
		dprintf( D_ALWAYS, "Couldn't send capability to startd.\n" );	
		BAILOUT;
	}

	if( !jobAd->put( *sock ) ) {
		dprintf( D_ALWAYS, "Couldn't send job classad to startd.\n" );	
		BAILOUT;
	}	

	if( !sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Couldn't send eom to startd.\n" );	
		BAILOUT;
	}

	mrec->status = M_STARTD_CONTACT_LIMBO;

	char to_startd[256];
	sprintf ( to_startd, "to startd %s", server );
	daemonCore->Register_Socket( sock, "<Startd Contact Socket>",
								 (SocketHandlercpp)startdContactSockHandler,
								 to_startd, this, ALLOW );

	daemonCore->Register_DataPtr( mrec );

	dprintf ( D_FULLDEBUG, "Registered startd contact socket.\n" );
	dprintf ( D_FULLDEBUG, "Set data pointer to %x\n", mrec );

	free( capability );
	free( user );
	free( server );

	numRegContacts++;

	return 1;
}
#undef BAILOUT

/* note new BAILOUT def:
   before each bad return we check to see if there's a pending call
   in the contact queue. */
#define BAILOUT               \
		DelMrec( mrec );      \
		checkContactQueue();  \
		return FALSE;

int Scheduler::startdContactSockHandler( Stream *sock )
{
		// all return values are non - KEEP_STREAM.  
		// Therefore, DaemonCore will cancel this socket at the
		// end of this function, which is exactly what we want!

	int reply;

	dprintf ( D_FULLDEBUG, "In Scheduler::startdContactSockHandler\n" );

		// since all possible returns from here result in the socket being
		// cancelled, we begin by decrementing the # of contacts.
	numRegContacts--;

	match_rec *mrec = (match_rec *) daemonCore->GetDataPtr();

	if ( !mrec ) {
		dprintf ( D_ALWAYS, "deamonCore failed to get data pointer!\n" );
		checkContactQueue();
		return FALSE;
	}
	
	dprintf ( D_FULLDEBUG, "Got mrec data pointer %x\n", mrec );

		/* If someone *tried* to delete mrec since the socket was 
		   registered, they really only set the status to M_DELETE_PENDING.
		   We check for this and delete as necessary */

	if ( mrec->status == M_DELETE_PENDING ) {
		dprintf( D_FULLDEBUG, "Found pending delete in mrec->status\n" );
		mrec->status = M_DELETED; // to tell DelMrec to actually delete it
		BAILOUT;
	}
	else {  // we assume things will work out.
		mrec->status = M_ACTIVE;
	}

 	if( !sock->rcv_int(reply, TRUE) ) {
		dprintf( D_ALWAYS, "Response problem from startd.\n" );	
		BAILOUT;
	}

	if( reply == OK ) {
		dprintf (D_PROTOCOL, "(Request was accepted)\n");
	 	sock->encode();
		if( !sock->code(MySockName) ) {
			dprintf( D_ALWAYS, "Couldn't send schedd string to startd.\n" );
			BAILOUT;
		}
		if( !sock->snd_int(aliveInterval, TRUE) ) {
			dprintf( D_ALWAYS, "Couldn't send aliveInterval to startd.\n" );
			BAILOUT;
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

void
Scheduler::checkContactQueue() 
{
	struct contactStartdArgs * args;

		// clear out the timer tid, since we made it here.
	checkContactQueue_tid = -1;

		// Contact startds as long as (a) there are still entries in our
		// queue, and (b) we have not exceeded MAX_STARTD_CONTACTS, which
		// ensures we do not run ourselves out of socket descriptors.
	while ( (numRegContacts < MAX_STARTD_CONTACTS) &&
			(!startdContactQueue.IsEmpty()) ) {
			// there's a pending registration in the queue:

		startdContactQueue.dequeue ( args );
		dprintf ( D_FULLDEBUG, "In checkContactQueue(), args = %x, host=%s\n", 
				  args, args->host );
		contactStartd( args->capability, args->owner,
							  args->host, &(args->id) );	
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
		if( rec->status == M_INACTIVE ) {
			dprintf(D_FULLDEBUG, "match (%s) inactive\n", rec->id);
			continue;
		}
		if ( rec->status == M_STARTD_CONTACT_LIMBO ) {
			dprintf ( D_FULLDEBUG, "match (%s) waiting for startd contact\n", 
					  rec->id );
			continue;
		}
		if ( rec->status == M_DELETE_PENDING ) {
			dprintf ( D_FULLDEBUG, "match (%s) pending delete...\n", rec->id);
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
                /* We check the universe of the job.  If it's MPI, 
                   it's ok to return null - we just haven't started
                   the shadow yet! */
        {
            int universe;
            if ( GetAttributeInt ( id.cluster, id.proc, ATTR_JOB_UNIVERSE, 
                                   &universe ) < 0 ) {
                universe = STANDARD;
            }
            if ( universe != MPI ) {
                
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
					fprintf(email,"Condor failed to start the " );
					fprintf(email,"condor_shadow.\n\nThis may be a ");
					fprintf(email,"configuration problem or a problem with\n");
					fprintf(email,"permissions on the condor_shadow ");
					fprintf(email,"binary.\n");
					char *schedlog = param ( "SCHEDD_LOG" );
					if ( schedlog ) {
						email_asciifile_tail( email, schedlog, 50 );
						free ( schedlog );
					}
					email_close ( email );
				}
                continue;
            } else {
                dprintf ( D_FULLDEBUG, "Match stored for mpi use\n" );
            }
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
	}
    else if ( universe == MPI ) {
        shadow_rec *mpireturn = NULL;
            // if we haven't tried to start this match before...
        if ( !mrec->isMatchedMPI ) {
            mpireturn = start_mpi( mrec, job_id );
        } else {
            mpireturn = NULL;
        }
        return mpireturn;
	}
	else {
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

	char *shadow_is_dc;
	shadow_is_dc = param( "SHADOW_IS_DC" );
	int sh_is_dc;
#ifdef WIN32
	sh_is_dc = TRUE;
#else
	sh_is_dc = FALSE;
#endif

	if (shadow_is_dc) {
		if ( (shadow_is_dc[0] == 'T') || (shadow_is_dc[0] == 't') ) {  
			sh_is_dc = TRUE;
		} else {
			sh_is_dc = FALSE;
		}
		free( shadow_is_dc );
	}

	if ( sh_is_dc ) {
		sprintf(args, "condor_shadow -f %s %s %s %d %d", MyShadowSockName, 
				mrec->peer, mrec->id, job_id->cluster, job_id->proc);
	} else {
		sprintf(args, "condor_shadow %s %s %s %d %d", MyShadowSockName, 
				mrec->peer, mrec->id, job_id->cluster, job_id->proc);
	}

        /* Get shadow's nice increment: */
    char *shad_nice = param( "SHADOW_RENICE_INCREMENT" );
    int niceness = 0;
    if ( shad_nice ) {
        niceness = atoi( shad_nice );
        free( shad_nice );
    }

	pid = daemonCore->Create_Process(Shadow, args, PRIV_UNKNOWN, 1, 
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
	int				i, lim;
	int				shadow_fd;
	char			out_buf[80];
	char			**ptr;
	struct shadow_rec *srp;
	int	 c;     	// current hosts
	int	 old_proc;  // the class in the cluster.  
                    // needed by the multi_shadow -Bin

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s...\n",
			job_id->cluster, job_id->proc, mrec->peer);
	
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
Scheduler::start_mpi(match_rec* matchRec, PROC_ID *job_id)
{
	char args[128];
	int	 pid;
	struct shadow_rec *srec;
    struct match_rec  *mrec;
	int	currHosts=0;    // current hosts
    int maxHosts=0;     // max hosts needed for a proc.
        // a pointer to our stored matches for this cluster.
    ExtArray<match_rec*> *MpiMatches;
	int i;

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s...\n",
			job_id->cluster, job_id->proc, matchRec->peer);
	
        /* We adhere to the "one mpi job per cluster number" religion.
           We now have the not-fun task of seeing if we *need* 
           the match given to us, incrementing the ATTR_CURRENT_HOSTS 
           for the proper proc if it is, then determining if 
           all the procs are in a ready to start state. */

        /* The first thing we do is find our stored match list: */
    if ( storedMatches->lookup( job_id->cluster, MpiMatches ) == -1 ) {
        dprintf ( D_FULLDEBUG, "No stored matches; assuming new...\n" );
        MpiMatches = new ExtArray<match_rec*>;
        MpiMatches->fill(NULL);
        MpiMatches->truncate(-1);
        storedMatches->insert( job_id->cluster, MpiMatches );
    } else {
        dprintf ( D_FULLDEBUG, "Found stored extarray of matches...\n" );
    }

        // now we get the max and current hosts of this proc to see
        // if we actually need this host. 
    if ( GetAttributeInt( job_id->cluster, job_id->proc, 
                          ATTR_MAX_HOSTS, &maxHosts ) < 0 ) {
        maxHosts = 0;
    }

    if ( GetAttributeInt( job_id->cluster, job_id->proc, 
                          ATTR_CURRENT_HOSTS, &currHosts ) < 0 ) {
        currHosts = 0;
    }

    if ( currHosts < maxHosts ) {
            // we need it; add on to the stored matches:
        dprintf ( D_FULLDEBUG, "curr < max  (%d < %d)\n", 
                  currHosts, maxHosts );
            // update ad:
        SetAttributeInt(job_id->cluster, job_id->proc, 
                        ATTR_CURRENT_HOSTS, ++currHosts );
        matchRec->isMatchedMPI = TRUE;
        dprintf ( D_FULLDEBUG, "MpiMatches->getlast() = %d\n", 
                  MpiMatches->getlast() );
        (*MpiMatches)[MpiMatches->getlast()+1] = matchRec;
        dprintf ( D_FULLDEBUG, "Just appended %s %s %d.%d\n", matchRec->peer, 
                  matchRec->id, job_id->cluster, job_id->proc );
        dprintf ( D_FULLDEBUG, "MpiMatches->getlast() = %d\n", 
                  MpiMatches->getlast() );

            // paranoia check:
        if ( currHosts != MpiMatches->getlast()+1 ) {
            dprintf (D_ALWAYS, "%s not == to #elem in MpiMatches! (%d!=%d)\n", 
                     ATTR_CURRENT_HOSTS, currHosts, MpiMatches->getlast()+1 );
        }
    }
    else {
            // we don't need this match.
        dprintf ( D_FULLDEBUG, "Don't need match!\n" );
        Relinquish( matchRec );
        DelMrec( matchRec );
        return NULL;
    }

        /* Now we walk through all the procs to see if all of them
           are ready to go.  */
    bool startShadow = true;
    int proc = 0;
    maxHosts = currHosts = 0;
    
    while ( startShadow ) {
        if ( GetAttributeInt( job_id->cluster, proc,
                              ATTR_MAX_HOSTS, &maxHosts ) < 0 ) {
            dprintf ( D_FULLDEBUG, "a\n" );
            break;  // hit last proc
        }
        
        if ( GetAttributeInt( job_id->cluster, proc, 
                              ATTR_CURRENT_HOSTS, &currHosts ) < 0 ) {
            dprintf ( D_FULLDEBUG, "b\n" );
            break;  // hit last proc
        }
        
        if ( maxHosts == 0 ) {
            dprintf ( D_FULLDEBUG, "c\n" );
            break;  // hit last proc
        }
        
        if ( maxHosts == currHosts ) {
                // cool, move to next
            dprintf ( D_FULLDEBUG, "cool: (m==c==%d) p=%d\n", maxHosts, proc );
            proc++;
            maxHosts = currHosts = 0;
        }
        else {
                // not cool, don't start.
            dprintf ( D_FULLDEBUG, "no start: m:%d c:%d p:%d\n", 
                      maxHosts, currHosts, proc );
            startShadow = false;
        }
    }

    // proc is now the # of procs going.

    dprintf ( D_FULLDEBUG, "startShadow: %s.  # procs: %d\n", 
              startShadow ? "TRUE":"FALSE", proc );

    if ( startShadow ) {
        
        dprintf ( D_ALWAYS, "Starting MPI shadow, cluster #%d\n", 
                  job_id->cluster );

            // XXX todo: add rec to slowstart Q

        if (Shadow) free(Shadow);
        Shadow = param("SHADOW");

        for ( i=0 ; i<=MpiMatches->getlast() ; i++ ) {
            dprintf ( D_FULLDEBUG, "peer: %s\n", (*MpiMatches)[i]->peer );
        }
        
            // give shadow the very first match from the list:
        mrec = (*MpiMatches)[0];

		sprintf(args, "condor_shadow -f %s %s %s %d %d", MyShadowSockName, 
				mrec->peer, mrec->id, job_id->cluster, 0 );

        dprintf ( D_FULLDEBUG, "args: \"%s\"\n", args );

        pid = daemonCore->Create_Process(Shadow, args);

        free(Shadow);
        Shadow = NULL;

        if (pid == FALSE) {
            dprintf( D_ALWAYS, "CreateProcess() failed!\n" );
            for ( i=0 ; i<=MpiMatches->getlast() ; i++ ) {
                DelMrec( (*MpiMatches)[i] );
            }
            return NULL;
        } 
    
		dprintf ( D_ALWAYS, "In Schedd, mpi shadow pid = %d\n", pid );        
            // The shadow rec always has a proc of 0 now...
        job_id->proc = 0;
		mark_job_running(job_id);
		srec = add_shadow_rec( pid, job_id, mrec, -1);

			// We must set all the match recs to point at this srec.
		for ( i=0 ; i<=MpiMatches->getlast() ; i++ ) {
			(*MpiMatches)[i]->shadowRec = srec;
		}

            // now we have to contact the Shadow and push the other 
            // matches at it.
        if ( !pushMPIMatches( srec->sinfulString, MpiMatches, proc ) ) {
            dprintf( D_ALWAYS, "pushMpiMatches() failed!\n" );
            for ( i=0 ; i<=MpiMatches->getlast() ; i++ ) {
                DelMrec( (*MpiMatches)[i] );
            }
            return NULL;
        }

        return srec;
    }
    
        // this is a *good* case of NULL
	return NULL;
}

int
Scheduler::pushMPIMatches( char *shadow, 
                           ExtArray<match_rec*> *MpiMatches, 
                           int procs ) {
/* shadow is in sinful string format */

/* We're going to push the information on all the matches.  The shadow
   actually *knows* the information for the first match from the argv, 
   so it can ignore the first one.  The format we're sending is:

   #procs (p)
   for each proc: {
      proc num
      number in this proc (n)
      for each n: {
         host
         cap
      }
   }
*/

    dprintf ( D_FULLDEBUG, "In pushMPIMatches %s, %d\n", shadow, procs );

    match_rec *mrec;
    ReliSock s;
    s.timeout( MPIShadowSockTimeout );
    
    if ( !s.connect( shadow ) ) {
        dprintf ( D_ALWAYS, "Failed to contact mpi shadow.\n" );
        return NULL;
    }
    
    int cmd = TAKE_MATCH;
    s.encode();
    if ( !s.code( cmd ) ||
         !s.code( procs ) ) {
        dprintf ( D_ALWAYS, "Failed to push cmd or procs\n" );
        return NULL;
    }
    
    dprintf ( D_PROTOCOL, "Pushed TAKE_MATCH, %d to Mpi Shadow\n", procs);

    char *p = new char[64];
    char *c = new char[64];
    for ( int i=0, matchNum=0 ; i<procs ; i++ ) {
        int inproc = countOfProc( *MpiMatches, i );
        if ( !s.code( i ) ||
             !s.code( inproc ) ) {
            dprintf ( D_ALWAYS, "Failed to push proc num, inproc.\n" );
            return NULL;
        }
        dprintf ( D_FULLDEBUG, "Pushed proc %d, num %d.\n", i, inproc );

        for ( int j=0 ; j<inproc ; j++ ) {
            mrec = (*MpiMatches)[matchNum++];
            strncpy( p, mrec->peer, 63 );
            strncpy( c, mrec->id, 63 );
            if ( !s.code( p ) ||
                 !s.code( c ) ) {
                dprintf ( D_ALWAYS, "Failed to send host, cap to shadow\n" );
                delete [] p;
                delete [] c;
                return NULL;
            }
            dprintf ( D_PROTOCOL, "Pushed %s %s to mpi shadow\n", p, c );
        }
    }
    delete [] p;
    delete [] c;
    
    if ( !s.end_of_message() ) {
        dprintf ( D_ALWAYS, "Failure in sending eom to mpi shadow.\n" );
        return NULL;
    }
    
    if( !s.close() ) {
        dprintf ( D_ALWAYS, "Failure to close mpi shadow sock.\n" );
        return NULL;
    }
    
    return 1;
}

int
Scheduler::countOfProc( ExtArray<match_rec*> matches, int proc ) {
        /* Job: count the number of matches which have proc 'proc' */
    int last = matches.getlast();
    int num = 0;
    for ( int i=0 ; i<=last ; i++ ) {
        if ( matches[i]->proc == proc ) {
            num++;
        }
    }
    dprintf ( D_FULLDEBUG, "%d elements in proc %d\n", num, proc );
    return num;
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
	if (chmod(a_out_name, 0755) < 0) {
		EXCEPT("chmod(%s, 0755)", a_out_name);
	}

	if (GetAttributeString(job_id->cluster, job_id->proc, 
						   ATTR_OWNER, owner) < 0) {
		dprintf(D_FULLDEBUG, "Scheduler::start_sched_universe_job"
				"--setting owner to \"nobody\"\n" );
		sprintf(owner, "nobody");
	}
	if (strcmp(owner, "root") == 0 ) {
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
	if ((inouterr[1] = open(output, O_WRONLY, 0)) < 0) {
		dprintf ( D_ALWAYS, "Open of %s failed, errno %d\n", output, errno );
		cannot_open_files = true;
	}
	if ((inouterr[2] = open(error, O_WRONLY, 0)) < 0) {
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
		sprintf(env, "");
	}

	if (GetAttributeString(job_id->cluster, job_id->proc, ATTR_JOB_ARGUMENTS,
							args) < 0) {
		sprintf(args, "");
	}
	sprintf(job_args, "%s %s", a_out_name, args);

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
	// SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, RUNNING);
	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 1);
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
	
	if (pid) add_shadow_rec(new_rec);
	return new_rec;
}

struct shadow_rec *
Scheduler::add_shadow_rec( shadow_rec* new_rec )
{
	numShadows++;
	shadowsByPid->insert(new_rec->pid, new_rec);
	shadowsByProcID->insert(new_rec->job_id, new_rec);

	if ( new_rec->match && new_rec->match->peer && new_rec->match->peer[0] ) {
		SetAttributeString(new_rec->job_id.cluster, new_rec->job_id.proc,
			ATTR_REMOTE_HOST,new_rec->match->peer);
	}
	int current_time = (int)time(NULL);
	int job_start_date = 0;
	SetAttributeInt(new_rec->job_id.cluster, new_rec->job_id.proc, 
		ATTR_SHADOW_BIRTHDATE, current_time);
	if (GetAttributeInt(new_rec->job_id.cluster, new_rec->job_id.proc,
						ATTR_JOB_START_DATE, &job_start_date) < 0) {
		// this is the first time the job has ever run, so set JobStartDate
		SetAttributeInt(new_rec->job_id.cluster, new_rec->job_id.proc, 
						ATTR_JOB_START_DATE, current_time);
	}

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

		// update ATTR_JOB_REMOTE_WALL_CLOCK.  note: must do this before
		// we call check_zombie below, since check_zombie is where the
		// job actually gets removed from the queue if job completed or deleted
		int bday = 0;
		GetAttributeInt(rec->job_id.cluster,rec->job_id.proc,
			ATTR_SHADOW_BIRTHDATE,&bday);
		if (bday) {
			float accum_time = 0;
			GetAttributeFloat(rec->job_id.cluster,rec->job_id.proc,
				ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
			accum_time += (float)( time(NULL) - bday );
			SetAttributeFloat(rec->job_id.cluster,rec->job_id.proc,
				ATTR_JOB_REMOTE_WALL_CLOCK,accum_time);
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
        if ( rec->sinfulString ) 
            delete [] rec->sinfulString;

            // mpi stored match stuff:
        ExtArray<match_rec*> *foo;
        if( storedMatches->lookup( rec->job_id.cluster, foo ) == 0 ) {
            dprintf (D_FULLDEBUG, "Deleting stored extarray for cluster %d\n", 
                     rec->job_id.cluster );
            storedMatches->remove( rec->job_id.cluster );
            delete foo;
        }

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
** Mark a job as stopped, (Idle or Unexpanded).
*/
void
mark_job_stopped(PROC_ID* job_id)
{
	int		status;
	int		orig_max;
	int		had_orig;
	char	owner[_POSIX_PATH_MAX];

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
			else if ( universe == MPI ) {
                if ( !rec->preempted ) {
                    daemonCore->Send_Signal( rec->pid, DC_SIGQUIT );
				} else {
					if ( ExitWhenDone ) {
						n++;
					}
                }
                rec->preempted = TRUE;
                n--;
			}
			else if (rec->match) {	/* scheduler universe job check (?) */
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
					daemonCore->Send_Signal( rec->pid, DC_SIGTERM );
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
	ReliSock	sock;

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
	shadow_rec*		srec;
	int				StartJobsFlag=TRUE;

	srec = FindSrecByPid(pid);

	if (IsSchedulerUniverse(srec)) {
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
					"with %s\n", srec->job_id.cluster,
					srec->job_id.proc, pid, 
					daemonCore->GetExceptionString(status));
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
						/* See if it's an mpi job here... */
					if ( (srec->match) && (srec->match->isMatchedMPI) ) {
							/* remove all in cluster */
						ExtArray<match_rec*> *matches;
						if ( storedMatches->lookup( srec->match->cluster, 
													matches ) != -1 ) {
							int n = matches->getlast();
							for ( int m=0 ; m <= n ; m++ ) {
								Relinquish( (*matches)[m] );
								DelMrec( (*matches)[m] );
							}
							matches->fill( NULL );
							matches->truncate(-1);
						}
					} else {
							/* Otherwise it's a normal job... */
						Relinquish(srec->match);
						DelMrec(srec->match);
					}
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
			dprintf( D_ALWAYS, "Shadow pid %d died with %s\n",
					 pid, daemonCore->GetExceptionString(status) );
		}
		delete_shadow_rec( pid );
	} else {
		// mrec and srec are NULL - agent dies after deleting match
		StartJobsFlag=FALSE;
	 }  // big if..else if...
	if( ExitWhenDone && numShadows == 0 ) {
		schedd_exit();
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
cleanup_ckpt_files(int cluster, int proc, const char *owner)
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
	if( UidDomain ) free( UidDomain );
	UidDomain = param( "UID_DOMAIN" );

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
				Name = strnewp( my_full_hostname() );
			}
		}
	}

	dprintf( D_FULLDEBUG, "Using name: %s\n", Name );

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
	 timeoutid = daemonCore->Register_Timer(10,
		(Eventcpp)&Scheduler::timeout,"timeout",this);
	startjobsid = daemonCore->Register_Timer(10,
		(Eventcpp)&Scheduler::StartJobs,"StartJobs",this);
	 aliveid = daemonCore->Register_Timer(aliveInterval, aliveInterval,
		(Eventcpp)&Scheduler::send_alive,"send_alive", this);
	cleanid = daemonCore->Register_Timer(QueueCleanInterval,
										 QueueCleanInterval,
										 (Event)&CleanJobQueue,
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
	GenericQuery query;

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
	update_central_mgr( INVALIDATE_SCHEDD_ADS, Collector->addr(), 0 );
	if( FlockHosts && FlockLevel > 0 ) {
		for( i=1, FlockHosts->rewind();
			 i <= FlockLevel && (host = FlockHosts->next()); i++) {
			update_central_mgr( INVALIDATE_SCHEDD_ADS, host, 
								COLLECTOR_UDP_COMM_PORT );
		}
	}

		// Now, we want to invalidate all the submittor ads.  So, go
		// through each submittor and add their Name to a query.
	for( i=0; i<N_Owners; i++ ) {
		sprintf( line, "%s == \"%s@%s\"", ATTR_NAME, Owners[i].Name,
				 UidDomain ); 
		query.addCustom( line );
	}
		// This will overwrite the Requirements expression in ad.
	query.makeQuery( *ad );

	update_central_mgr( INVALIDATE_SUBMITTOR_ADS, Collector->addr(), 0 );
	if( FlockHosts && FlockLevel > 0 ) {
		for( i=1, FlockHosts->rewind();
			 i <= FlockLevel && (host = FlockHosts->next()); i++ ) {
			update_central_mgr( INVALIDATE_SUBMITTOR_ADS, host, 
								COLLECTOR_UDP_COMM_PORT );
		}
	}
}


void
Scheduler::reschedule_negotiator(int, Stream *)
{
	int	  	cmd = RESCHEDULE;


		// don't bother the negotiator if we are shutting down
	if ( ExitWhenDone ) {
		return;
	}

	timeout();							// update the central manager now

		// only send reschedule command to negotiator if there is not a
		// reschedule request already pending.
	if ( !reschedule_request_pending ) {
		dprintf( D_ALWAYS, "Called reschedule_negotiator()\n" );
		SafeSock sock;
		sock.timeout(NEGOTIATOR_CONTACT_TIMEOUT);

		if (!sock.connect(Negotiator->addr(), 0)) {
			dprintf( D_ALWAYS, "failed to connect to negotiator %s\n",
				 Negotiator->addr() );
			return;
		}

		sock.encode();
		if (!sock.code(cmd)) {
			dprintf( D_ALWAYS,
				"failed to send RESCHEDULE command to negotiator\n");
			return;
		}
		if (!sock.eom()) {
			dprintf( D_ALWAYS,
				"failed to send RESCHEDULE command to negotiator\n");
			return;
		}
	} else {
		dprintf(D_ALWAYS,"reschedule requested -- already pending\n");
	}

		// if made it here, we have told to negotiator to contact us
	reschedule_request_pending = true;

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

	sock = new ReliSock;
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
		sock = new SafeSock;
		sock->timeout(STARTD_CONTACT_TIMEOUT);
		sock->encode();
		id = rec->id;
		if( !sock->connect(rec->peer) ||
			!sock->put(alive) || 
			!sock->code(id) || 
			!sock->end_of_message() ) {
				// UDP transport out of buffer space!
			dprintf(D_ALWAYS, "\t(Can't send alive message to %d)\n",
					rec->peer);
			delete sock;
			continue;
		}
			/* TODO: Someday, espcially once the accountant is done, 
               the startd should send a keepalive ACK back to the schedd.  
               If there is no shadow to this machine, and we have not 
               had a startd keepalive ACK in X amount of time, then we 
               should relinquish the match.  Since the accountant is 
               not done and we are in fire mode, leave this 
               for V6.1.  :^) -Todd 9/97
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

int
mpiHashFunc( const int& cluster, int numbuckets ) {
    return cluster % numbuckets;
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
	intoAd ( ad, "numRegContacts", numRegContacts );
	intoAd ( ad, "MAX_STARTD_CONTACTS", MAX_STARTD_CONTACTS );
	intoAd ( ad, "CondorViewHost", CondorViewHost );
	intoAd ( ad, "Shadow", Shadow );
	intoAd ( ad, "CondorAdministrator", CondorAdministrator );
	intoAd ( ad, "Mail", Mail );
	intoAd ( ad, "filename", filename );
	intoAd ( ad, "AccountantName", AccountantName );
	intoAd ( ad, "UidDomain", UidDomain );
	intoAd ( ad, "FlockLevel", FlockLevel );
	intoAd ( ad, "MaxFlockLevel", MaxFlockLevel );
	intoAd ( ad, "aliveInterval", aliveInterval );
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
