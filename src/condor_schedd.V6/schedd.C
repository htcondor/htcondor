/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
** Modified by : Dhaval N. Shah
**               University of Wisconsin, Computer Sciences Dept.
** Uses <IP:PORT> rather than hostnames
**
** Modified by : Cai Weiru
**		Organized most functions and variables into Scheduler class
** 
** Cleaned up for V6 by Derek Wright 7/8/97
** 
*/ 

#define _POSIX_SOURCE
#include "condor_common.h"

#include <netdb.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "sched.h"
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
#include "url_condor.h"
#include "condor_updown.h"
#include "condor_uid.h"

#if defined(NDBM)
#include <ndbm.h>
#else NDBM
#include "ndbm_fake.h"
#endif NDBM

#if defined(HPUX9) || defined(Solaris)
#include "fake_flock.h"
#endif

#if defined(Solaris)
#include <fcntl.h>
#include <unistd.h>
#endif

#define DEFAULT_SHADOW_SIZE 125

#define SUCCESS 1
#define CANT_RUN 0

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
extern char *gen_ckpt_name();
extern void	Init();
extern int getJobAd(int cluster_id, int proc_id, ClassAd *new_ad);

#include "condor_qmgr.h"

#define MAX_SCHED_UNIVERSE_RECS 16

extern "C"
{
    void	upDown_ClearUserInfo(void);
    void 	upDown_UpdateUserInfo(const char* , const int);
    int 	upDown_GetNoOfActiveUsers(void);
    int 	upDown_GetUserPriority(const char* , int* );
    void 	upDown_UpdatePriority(void);
    void 	upDown_SetParameters(const int , const int);
    void 	upDown_ReadFromFile(const char* );
    void 	upDown_WriteToFile(const char*);
    void 	upDown_Display(void);
    void 	dprintf(int, char*...);
	int	 	calc_virt_memory();
	int	 	getdtablesize();
	void 	_EXCEPT_(char*...);
	char* 	gen_ckpt_name(char*, int, int, int);
	int	 	do_connect(const char*, const char*, u_int);
#if defined(HPUX9)
	int	 	gethostname(char*, unsigned int);
#else
	int	 	gethostname(char*, int);
#endif
	int		send_context_to_machine(DGRAM_IO_HANDLE*, int, CONTEXT*);
	int	 	boolean(char*, char*);
	char*	param(char*);
	void	block_signal(int);
	void	unblock_signal(int);
	void	handle_q(ReliSock*, struct sockaddr_in*);
	void	fill_dgram_io_handle(DGRAM_IO_HANDLE*, char*, int, int);
	int		udp_unconnect();
	char*	sin_to_string(struct sockaddr_in*);
	EXPR*	build_expr(const char*, ELEM*);
	void	get_inet_address(struct in_addr*);
	int		prio_compar(prio_rec*, prio_rec*);
}

extern	int		send_classad_to_machine(DGRAM_IO_HANDLE*, int, const ClassAd*);
extern	int		get_job_prio(int, int);
extern	void	FindRunnableJob(int, int&);
extern	int		Runnable(PROC_ID*);

extern	char*		myName;
extern	char*		Spool;
extern	char*		CondorAdministrator;
extern	Scheduler*	sched;
extern	char		Name[];
extern	int			ScheddName;

// shadow records and priority records
shadow_rec      ShadowRecs[MAX_SHADOW_RECS];
int             NShadowRecs;
extern prio_rec	*PrioRec;
extern int		N_PrioRecs;
PROC_ID			IdleSchedUniverseJobIDs[MAX_SCHED_UNIVERSE_RECS];
int				NSchedUniverseJobIDs;
extern int		grow_prio_recs(int);

void	check_zombie(int, PROC_ID*);
void	send_vacate(Mrec*, int);
#if defined(__STDC__)
shadow_rec*     find_shadow_rec(PROC_ID*);
shadow_rec*     add_shadow_rec(int, PROC_ID*, Mrec*, int);
#else
shadow_rec*     find_shadow_rec();
#endif

#ifdef CARMI_OPS
struct shadow_rec *find_shadow_by_cluster( PROC_ID * );
#endif

Mrec::Mrec(char* i, char* p, PROC_ID* id)
{
    strcpy(this->id, i);
    strcpy(peer, p);
    cluster = id->cluster;
    proc = id->proc;
    status = M_INACTIVE;
	agentPid = -1;
	shadowRec = NULL;
	alive_countdown = 0;
}

Scheduler::Scheduler()
{
    ad = NULL;
    MySockName = NULL;
    SchedDInterval = 0;
    MaxJobStarts = 0;
    MaxJobsRunning = 0;
    JobsStarted = 0;
    JobsRunning = 0;
    ReservedSwap = 0;
    ShadowSizeEstimate = 0;
    ScheddScalingFactor = 0;
    ScheddHeavyUserTime = 0;
    Step = 0;
    ScheddHeavyUserPriority = 0;
	LastTimeout = time(NULL);
    CollectorHost = NULL;
    NegotiatorHost = NULL;
    Shadow = NULL;
    CondorAdministrator = NULL;
    Mail = NULL;
    filename = NULL;
	rec = new Mrec*[MAXMATCHES];
	aliveFrequency = 0;
	aliveInterval = 0;
	port = 0;
	ExitWhenDone = 0;
}

Scheduler::~Scheduler()
{
	int		i;
    delete ad;
    if (MySockName)
        FREE(MySockName);
    delete []CollectorHost;
    delete []NegotiatorHost;
    delete []Shadow;
    delete []CondorAdministrator;
    delete []Mail;
    delete []filename;
	for(i = 0; i < nMrec; i++)
	{
		delete rec[i];
    }		
    delete []rec;
    nMrec = 0;
}

void
Scheduler::timeout()
{
	block_signal(SIGCHLD);
	/* the step size in the UPDOWN algo depends on the time */
	/* interval of sampling			                */
	Step = ( time((time_t*)0) - LastTimeout) /ScheddScalingFactor;
	upDown_SetParameters(Step, ScheddHeavyUserPriority);

	upDown_ClearUserInfo();	/* UPDOWN */
	count_jobs();
	upDown_UpdatePriority(); /* UPDOWN */
	upDown_Display();        /* UPDOWN */
	upDown_WriteToFile(filename);    /* UPDOWN */

	//	update_central_mgr();

		/* Neither of these should be needed! */
	reaper( 0, 0, 0 );
	clean_shadow_recs();

	if( (NShadowRecs-SchedUniverseJobsRunning) > MaxJobsRunning ) {
		preempt( NShadowRecs - MaxJobsRunning );
	}
	unblock_signal(SIGCHLD);
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

	N_Owners = 0;
	JobsRunning = 0;
	JobsIdle = 0;
	SchedUniverseJobsIdle = 0;
	SchedUniverseJobsRunning = 0;

	// clear out the table ... (WHY? --RR)
	for ( i=0; i<MAX_NUM_OWNERS; i++) {
		if (Owners[i].Name) FREE( Owners[i].Name );
		Owners[i].Name = NULL;
		Owners[i].JobsRunning = 0;
		Owners[i].JobsIdle = 0;
	}

	WalkJobQueue((int(*)(int, int)) count );

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
    dprintf (D_ALWAYS, "Changed attribute: %s\n", tmp);
    ad->InsertOrUpdate(tmp);
	ad->Delete (ATTR_IDLE_JOBS);
	update_central_mgr();  // Send even if no owners
	dprintf (D_ALWAYS, "Sent HEART BEAT ad to central mgr: N_Owners=%d\n",N_Owners);

	for ( i=0; i<N_Owners; i++) {

	  sprintf(tmp, "%s = %d", ATTR_RUNNING_JOBS, Owners[i].JobsRunning);
	  dprintf (D_ALWAYS, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS, Owners[i].JobsIdle);
	  dprintf (D_ALWAYS, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  sprintf(tmp, "%s = \"%s@%s\"", ATTR_NAME, Owners[i].Name, UidDomain);
	  dprintf (D_ALWAYS, "Changed attribute: %s\n", tmp);
	  ad->InsertOrUpdate(tmp);

	  update_central_mgr();
	}

	return 0;
}

int
count( int cluster, int proc )
{
	int		status;
	char 	buf[100];
	char*	owner;
	int		cur_hosts;
	int		max_hosts;
	int		universe;

	GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status);
	GetAttributeString(cluster, proc, ATTR_OWNER, buf);
	owner = buf;
	if (GetAttributeInt(cluster, proc, ATTR_CURRENT_HOSTS, &cur_hosts) < 0) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (GetAttributeInt(cluster, proc, ATTR_MAX_HOSTS, &max_hosts) < 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}
	if (GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe) < 0) {
		universe = STANDARD;
	}
	
	if (universe == SCHED_UNIVERSE) {
		sched->SchedUniverseJobsRunning += cur_hosts;
		sched->SchedUniverseJobsIdle += (max_hosts - cur_hosts);
	} else {
		sched->JobsRunning += cur_hosts;
		sched->JobsIdle += (max_hosts - cur_hosts);

		// Per owner info
		int OwnerNum = sched->insert_owner( owner );
		sched->Owners[OwnerNum].JobsRunning += cur_hosts;
		sched->Owners[OwnerNum].JobsIdle += (max_hosts - cur_hosts);

		if ( status == RUNNING ) {	/* UPDOWN */
			upDown_UpdateUserInfo(owner,UpDownRunning);
		} else if ( (status == IDLE)||(status == UNEXPANDED)) {
			upDown_UpdateUserInfo(owner,UpDownIdle);
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
Scheduler::update_central_mgr()
{
	SafeSock	sock(CollectorHost, COLLECTOR_UDP_COMM_PORT);

	sock.attach_to_file_desc(UdpSock);
	sock.encode();
	sock.put(UPDATE_SCHEDD_AD);
	ad->put(sock);
	sock.end_of_message();
}

extern "C" {
void
abort_job_myself(PROC_ID job_id)
{
	char	*host;
	int		i;

	for( i=0; i<NShadowRecs; i++ ) {
		if( ShadowRecs[i].job_id.cluster == job_id.cluster &&
		   (ShadowRecs[i].job_id.proc == job_id.proc ||
			job_id.proc == -1)) {
			if (ShadowRecs[i].match) {
				host = ShadowRecs[i].match->peer;
				dprintf( D_ALWAYS,
						"Found shadow record for job %d.%d, host = %s\n",
						job_id.cluster, job_id.proc, host );
				/* send_kill_command( host ); */
				/* change for condor flocking */
				if ( kill( ShadowRecs[i].pid, SIGUSR1) == -1 )
					dprintf(D_ALWAYS,
							"Error in sending SIGUSR1 to %d errno = %d\n",
							ShadowRecs[i].pid, errno);
				else dprintf(D_ALWAYS, "Send SIGUSR1 to Shadow Pid %d\n",
							 ShadowRecs[i].pid);
			} else {
				dprintf( D_ALWAYS,
						"Found record for scheduler universe job %d.%d\n",
						job_id.cluster, job_id.proc, host );
				kill( ShadowRecs[i].pid, SIGKILL );
			}
		}
	}
}
} /* End of extern "C" */

void
Scheduler::abort_job(ReliSock* s, struct sockaddr_in*)
{
	PROC_ID	job_id;
	s->decode();
	if( !s->code(job_id) ) {
		dprintf( D_ALWAYS, "abort_job() can't read job_id\n" );
		return;
	}
	abort_job_myself(job_id);
}

#define RETURN \
	if( context ) { \
		free_context( context ); \
	} \
	unblock_signal(SIGCHLD); \
	return

/*
** The negotiator wants to give us permission to run a job on some
** server.  We must negotiate to try and match one of our jobs with a
** server which is capable of running it.  NOTE: We must keep job queue
** locked during this operation.
*/
void
Scheduler::negotiate(ReliSock* s, struct sockaddr_in*)
{
	int		i;
	int		op;
	CONTEXT	*context = NULL;
	PROC_ID	id;
	char	*match= NULL;				// each match info received from cmgr
	char*	capability;					// capability for each match made
	int		jobs;						// # of jobs that CAN be negotiated
	int		cur_cluster = -1;
	int		start_limit_for_swap;
	int		cur_hosts;
	int		max_hosts;
	int		host_cnt;
	int		perm_rval;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

	dprintf (D_PROTOCOL, "## 2. Negotiating with CM\n");

	SwapSpace = calc_virt_memory();

	N_PrioRecs = 0;

	WalkJobQueue( (int(*)(int, int))job_prio );

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
	}

		/* Try jobs in priority order */
	for( i=0; i < N_PrioRecs; i++ ) {

		if(AlreadyMatched(&PrioRec[i].id))
		{
			dprintf( D_FULLDEBUG, "Job already matched\n");
			continue;
		}

		id = PrioRec[i].id;
		if( cluster_rejected(id.cluster) ) {
			continue;
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
			if( !s->rcv_int(op,TRUE) ) {
				dprintf( D_ALWAYS, "Can't receive request from manager\n" );
				RETURN;
			}
			
			switch( op ) {
			    case REJECTED:
				    mark_cluster_rejected( cur_cluster );
					host_cnt = max_hosts + 1;
					break;
				case SEND_JOB_INFO: {
					if( JobsStarted >= MaxJobStarts ) {
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS,
									"Can't send NO_MORE_JOBS to mgr\n" );
							RETURN;
						}
						dprintf( D_ALWAYS,
					"Reached MAX_JOB_STARTS - %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						RETURN;
					}
					/* Really, we're trying to make sure we don't have too
					   many shadows running, so compare here against
					   NShadowRecs rather than JobsRunning as in the past. */
					if( NShadowRecs >= MaxJobsRunning ) {
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							RETURN;
						}
						dprintf( D_ALWAYS,
				"Reached MAX_JOBS_RUNNING, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						RETURN;
					}
					if( SwapSpaceExhausted ) {
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							RETURN;
						}
						dprintf( D_ALWAYS,
					"Swap Space Exhausted, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						RETURN;
					}
					if( ShadowSizeEstimate && JobsStarted >= start_limit_for_swap ) { 
						if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							RETURN;
						}
						dprintf( D_ALWAYS,
				"Swap Space Estimate Reached, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						RETURN;
					}
					
					/* Send a job description */
					if( !s->snd_int(JOB_INFO,FALSE) ) {
						dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
						RETURN;
					}
					ClassAd ad;
					getJobAd( id.cluster, id.proc, &ad );
					if( !ad.put(*s) ) {
						dprintf( D_ALWAYS,
								"Can't send job ad to mgr\n" );
						RETURN;
					}
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't send job ad to mgr\n" );
						RETURN;
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
					if( !s->get(match) ) {
						dprintf( D_ALWAYS,
								"Can't receive host name from mgr\n" );
						RETURN;
					}
					if( !s->end_of_message() ) {
						dprintf( D_ALWAYS,
								"Can't receive host name from mgr\n" );
						RETURN;
					}
					// match is in the form "<xxx.xxx.xxx.xxx:xxxx> xxxxxxxx#xx"
					dprintf(D_PROTOCOL, "## 4. Received match %s\n", match);
					capability = strchr(match, '#');
					if(capability)
					{
						*capability = '\0';
					}
					capability = strchr(match, ' ');
					*capability = '\0';
					capability++;
					perm_rval = permission(capability, match, &id);
					FREE( match );
					match = NULL;
					JobsStarted += perm_rval;
					host_cnt++;
					break;
				case END_NEGOTIATE:
					dprintf( D_ALWAYS, "Lost priority - %d jobs matched\n",
							JobsStarted );
					RETURN;
				default:
					dprintf( D_ALWAYS, "Got unexpected request (%d)\n", op );
					RETURN;
			}
		}
	}

		/* Out of jobs */
	if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
		dprintf( D_ALWAYS, "Can't send NO_MORE_JOBS to mgr\n" );
		RETURN;
	}
	if( JobsStarted < jobs ) {
		dprintf( D_ALWAYS,
		"Out of servers - %d jobs matched, %d jobs idle\n",
							JobsStarted, jobs - JobsStarted );
	} else {
		dprintf( D_ALWAYS,
		"Out of jobs - %d jobs matched, %d jobs idle\n",
							JobsStarted, jobs - JobsStarted );
	}
	RETURN;
}
#undef RETURN


void
Scheduler::vacate_service(ReliSock *sock, struct sockaddr_in*)
{
	char	*capability = NULL;

	dprintf (D_ALWAYS, "Got VACATE_SERVICE\n");
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
Scheduler::permission(char* id, char* server, PROC_ID* jobId)
{
	Mrec*		mrec;						// match record pointer
	int			pid;
	int			lim = getdtablesize();		// size of descriptor table
	int			i;

	mrec = AddMrec(id, server, jobId);
	if(!mrec) {
		return 0;
	}
	switch((pid = fork())) 	{
	    case -1:	/* error */

            if(errno == ENOMEM) {
                dprintf(D_ALWAYS, "fork() failed, due to lack of swap space\n");
                swap_space_exhausted();
            } else {
                dprintf(D_ALWAYS, "fork() failed, errno = %d\n", errno);
            }
			DelMrec(mrec);
			return 0;

        case 0:     /* the child */

            close(0);
            for(i = 3; i < lim; i++) {
                close(i);
            }
            Agent(server, id, MySockName, aliveFrequency, jobId);
			

        default:    /* the parent */

            mrec->agentPid = pid;
			return 1;
	}
}

int
find_idle_sched_universe_jobs( int cluster, int proc )
{
	int	status;
	int	cur_hosts;
	int	max_hosts;
	int	universe;
	bool already_found = false;

	if (GetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, &universe) < 0) {
		universe = STANDARD;
	}

	if (universe != SCHED_UNIVERSE) return 0;

	GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status);
	if (GetAttributeInt(cluster, proc, ATTR_CURRENT_HOSTS, &cur_hosts) < 0) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (GetAttributeInt(cluster, proc, ATTR_MAX_HOSTS, &max_hosts) < 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}

	if (max_hosts > cur_hosts) {
		for (int i=0; i < NSchedUniverseJobIDs; i++) {
			if (IdleSchedUniverseJobIDs[i].cluster == cluster &&
				IdleSchedUniverseJobIDs[i].proc == proc) {
				already_found = true;
			}
		}
		if (!already_found) {
			IdleSchedUniverseJobIDs[NSchedUniverseJobIDs].cluster = cluster;
			IdleSchedUniverseJobIDs[NSchedUniverseJobIDs].proc = proc;
			NSchedUniverseJobIDs++;
		}
	}

	if (NSchedUniverseJobIDs == MAX_SCHED_UNIVERSE_RECS) return -1;

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
	int		i;							// iterate through match records
	PROC_ID	id;

	dprintf(D_ALWAYS, "-------- Begin starting jobs --------\n");
	for(i = 0; i < nMrec; i++)
	{
		if(rec[i]->status == M_INACTIVE)
		{
			dprintf(D_FULLDEBUG, "match (%s) inactive\n", rec[i]->id);
			continue;
		}
		if(rec[i]->shadowRec)
		{
			dprintf(D_FULLDEBUG, "match (%s) already running a job\n",
					rec[i]->id);
			continue;
		}

		// This is the case we want to try and start a job.
		id.cluster = rec[i]->cluster;
		id.proc = rec[i]->proc; 
		if(!Runnable(&id))
		// find the job in the cluster with the highest priority
		{
			FindRunnableJob(rec[i]->cluster, id.proc);
		}
		if(id.proc < 0)
		// no more jobs to run
		{
			dprintf(D_FULLDEBUG, "match (%s) out of jobs\n", rec[i]->id);
			dprintf(D_ALWAYS,"Out of jobs (proc id %d); relinquishing\n",
								id.proc);
			Relinquish(rec[i]);
			DelMrec(rec[i]);
			i--;
			continue;
		}
		dprintf(D_ALWAYS, "Match (%s) - running %d.%d\n",rec[i]->id,id.cluster,
				id.proc);
		if(!(rec[i]->shadowRec = StartJob(rec[i], &id)))
		// Start job failed. Throw away the match. The reason being that we
		// don't want to keep a match around and pay for it if it's not
		// functioning and we don't know why. We might as well get another
		// match.
		{
			dprintf(D_ALWAYS,"Failed to start job %s; relinquishing\n",
						rec[i]->id);
			Relinquish(rec[i]);
			DelMrec(rec[i]);
			i--;
			continue;
		}
	}
	if (SchedUniverseJobsIdle > 0) {
		StartSchedUniverseJobs();
	}
	dprintf(D_ALWAYS, "-------- Done starting jobs --------\n");
}

void
Scheduler::StartSchedUniverseJobs()
{
	int i;

	WalkJobQueue( (int(*)(int, int))find_idle_sched_universe_jobs );
	for (i = 0; i < NSchedUniverseJobIDs; i++) {
		start_sched_universe_job(IdleSchedUniverseJobIDs+i);
	}
	NSchedUniverseJobIDs = 0;
}

shadow_rec*
Scheduler::StartJob(Mrec* mrec, PROC_ID* job_id)
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

shadow_rec*
Scheduler::start_std(Mrec* mrec , PROC_ID* job_id)
{
	char	*argv[7];
	char	cluster[10], proc[10];
	int		pid;
	int		i, lim;
	int		parent_id;

#ifdef CARMI_OPS
	struct shadow_rec *srp;
	char out_buf[80];
	int pipes[2];
#endif

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s\n",
			job_id->cluster, job_id->proc, mrec->peer);

	(void)sprintf( cluster, "%d", job_id->cluster );
	(void)sprintf( proc, "%d", job_id->proc );
	argv[0] = "condor_shadow";
	argv[1] = MySockName;
	argv[2] = mrec->peer;
	argv[3] = mrec->id;
	argv[4] = cluster;
	argv[5] = proc;
	argv[6] = 0;

#ifdef NOTDEF
	{
	char	**ptr;

	dprintf( D_ALWAYS, "About to call: " );
	for( ptr = argv; *ptr; ptr++ ) {
		dprintf( D_ALWAYS | D_NOHEADER, "%s ", *ptr );
	}
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
	}
#endif NOTDEF

	mark_job_running(job_id);
	SetAttributeInt(job_id->cluster, job_id->proc, "CurrentHosts", 1);
	lim = getdtablesize();

	parent_id = getpid();

#ifdef CARMI_OPS
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
#endif

	switch( (pid=fork()) ) {
		case -1:	/* error */
			if( errno == ENOMEM ) {
				dprintf(D_ALWAYS, "fork() failed, due to lack of swap space\n");
				swap_space_exhausted();
			} else {
				dprintf(D_ALWAYS, "fork() failed, errno = %d\n" );
			}
			mark_job_stopped(job_id);
			break;
		case 0:		/* the child */
			(void)close( 0 );
#ifdef CARMI_OPS
			if ((retval = dup2( pipes[0], 0 )) < 0)
			  {
			    dprintf(D_ALWAYS, "Dup2 returns %d\n", retval);  
			    EXCEPT("Could not dup pipe to stdin\n");
			  }
		        else
			    dprintf(D_ALWAYS, " Duped 0 to %d \n", pipes[0]);
#endif		
			for( i=3; i<lim; i++ ) {
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
		default:	/* the parent */
			dprintf( D_ALWAYS, "Running %d.%d on \"%s\", (shadow pid = %d)\n",
				job_id->cluster, job_id->proc, mrec->peer, pid );
#ifdef ASHISH_DEBUG
			close(pipes[0]);
			srp = add_shadow_rec( pid, job_id, server, pipes[1] );
			dprintf( D_ALWAYS, "pipes[1] = %d\n", pipes[1]);
#endif	
			return add_shadow_rec( pid, job_id, mrec, -1 );
	}
	return NULL;
}


shadow_rec*
Scheduler::start_pvm(Mrec* mrec, PROC_ID *job_id)
{
	char			*argv[8];
	char			cluster[10], proc_str[10];
	int				pid;
	int				i, lim;
	int				parent_id;
	int				shadow_fd;
	char			out_buf[80];
	char			**ptr;
	struct shadow_rec *srp;
	int				c;							// current hosts


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
		parent_id = getpid();
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
				}
				argv[1] = MySockName;
				argv[2] = 0;
				
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
	sprintf(out_buf, "%s %s %d\n", mrec->peer, mrec->id, job_id->proc);
	dprintf( D_ALWAYS, "sending %s", out_buf);
	write(shadow_fd, out_buf, strlen(out_buf));
	return srp;
}

shadow_rec*
Scheduler::start_sched_universe_job(PROC_ID* job_id)
{
	char 	a_out_name[_POSIX_PATH_MAX];
	char 	input[_POSIX_PATH_MAX];
	char 	output[_POSIX_PATH_MAX];
	char 	error[_POSIX_PATH_MAX];
	char	env[_POSIX_PATH_MAX];		// fixed size is bad here!!
	char   	job_args[_POSIX_PATH_MAX]; 	// fixed size is bad here!!
	char	args[_POSIX_PATH_MAX];
	char	owner[20], iwd[_POSIX_PATH_MAX];
	Environ	env_obj;
	char	**envp;
	char	*argv[_POSIX_PATH_MAX];		// bad
	int		fd, parent_id, pid, argc;
	struct passwd	*pwd;

	dprintf( D_FULLDEBUG, "Starting sched universe job %d.%d\n",
			job_id->cluster, job_id->proc );

	strcpy(a_out_name, gen_ckpt_name(Spool, job_id->cluster, ICKPT, 0));
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

	parent_id = getpid();

	if ((pid = fork()) < 0) {
		dprintf(D_ALWAYS, "failed for fork for sched universe job start!\n");
		return NULL;
	}

	if (pid == 0) {		// the child

		init_user_ids(owner);
		set_user_priv();

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
		// make sure file is executable
		if (chmod(a_out_name, 0755) < 0) {
			EXCEPT("chmod(%s, 0755)", a_out_name);
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
}

void
Scheduler::display_shadow_recs()
{
	int		i;
	struct shadow_rec *r;

	dprintf( D_FULLDEBUG, "\n");
	dprintf( D_FULLDEBUG, "..................\n" );
	dprintf( D_FULLDEBUG, ".. Shadow Recs (%d)\n", NShadowRecs );
	for( i=0; i<NShadowRecs; i++ ) {
		r = &ShadowRecs[i];
		dprintf(D_FULLDEBUG, ".. %d: %d, %d.%d, %s, %s\n",
				i, r->pid, r->job_id.cluster, r->job_id.proc,
				r->preempted ? "T" : "F" ,
				r->match ? r->match->peer : "localhost");
	}
	dprintf( D_FULLDEBUG, "..................\n\n" );
}

struct shadow_rec *
add_shadow_rec( int pid, PROC_ID* job_id, Mrec* mrec, int fd )
{
	ShadowRecs[ NShadowRecs ].pid = pid;
	ShadowRecs[ NShadowRecs ].job_id = *job_id;
	ShadowRecs[ NShadowRecs ].match = mrec;
	ShadowRecs[ NShadowRecs ].preempted = FALSE;
	ShadowRecs[ NShadowRecs ].conn_fd = fd;
	
	NShadowRecs++;
	dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
			pid, job_id->cluster, job_id->proc );
	sched->display_shadow_recs();
	if ( NShadowRecs == MAX_SHADOW_RECS ) {
		EXCEPT( "Reached MAX_SHADOW_RECS" );
	}
	return &(ShadowRecs[NShadowRecs - 1]);
}

void
Scheduler::delete_shadow_rec(int pid)
{
	int		i;

	dprintf( D_ALWAYS, "Entered delete_shadow_rec( %d )\n", pid );

	for( i=0; i<NShadowRecs; i++ ) {
		if( ShadowRecs[i].pid == pid ) {
			
			dprintf(D_FULLDEBUG,
				"Deleting shadow rec for PID %d, job (%d.%d)\n",
			pid, ShadowRecs[i].job_id.cluster, ShadowRecs[i].job_id.proc );
			
			check_zombie( pid, &(ShadowRecs[i].job_id) );
			RemoveShadowRecFromMrec(&ShadowRecs[i]);
			NShadowRecs -= 1;
			ShadowRecs[i] = ShadowRecs[NShadowRecs];
			display_shadow_recs();
			dprintf( D_ALWAYS, "Exited delete_shadow_rec( %d )\n", pid );
			return;
		}
	}
	/*
	EXCEPT( "Can't find shadow record for process %d\n", pid );
	*/
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

	/*
	** dprintf( D_FULLDEBUG, "Marked job %d.%d as RUNNING\n",
	**								proc->id.cluster, proc->id.proc );
	*/
}

/*
** Mark a job as stopped, (Idle or Unexpanded).  Note: this routine assumes
** that the DBM pointer (q) is already locked for writing.
*/
void
Scheduler::mark_job_stopped(PROC_ID* job_id)
{
	int		status;
	int		orig_max;
	int		had_orig;
	char	ckpt_name[MAXPATHLEN];

	GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, &status);
	had_orig = GetAttributeInt(job_id->cluster, job_id->proc, 
							   "OrigMaxHosts", &orig_max);

	if( status != RUNNING ) {
		EXCEPT( "Trying to stop job %d.%d, but not marked RUNNING!",
			job_id->cluster, job_id->proc );
	}
	strcpy(ckpt_name, gen_ckpt_name(Spool,job_id->cluster,job_id->proc,0) );
	if( access(ckpt_name,F_OK) != 0 ) {
		status = UNEXPANDED;
	} else {
		status = IDLE;
	}

	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS, status);
	SetAttributeInt(job_id->cluster, job_id->proc, ATTR_CURRENT_HOSTS, 0);
	if (had_orig >= 0) {
		SetAttributeInt(job_id->cluster, job_id->proc, ATTR_MAX_HOSTS,
						orig_max);
	}
	
	dprintf( D_FULLDEBUG, "Marked job %d.%d as %s\n", job_id->cluster,
			job_id->proc, status==IDLE?"IDLE":"UNEXPANDED" );
	
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
** Try to send a signal to the given process to determine whether it
** is alive.  We use signal '0' which only checks whether a signal is
** sendable, it doesn't actually send one.
*/
int
Scheduler::is_alive(int pid)
{
	return( kill(pid,0) == 0 );
}

void
Scheduler::clean_shadow_recs()
{
	int		i;

	dprintf( D_FULLDEBUG, "============ Begin clean_shadow_recs =============\n" );
	for( i=NShadowRecs-1; i >= 0; i-- ) {
		if( !is_alive(ShadowRecs[i].pid) ) {
			dprintf( D_ALWAYS,
			"Cleaning up ShadowRec for pid %d\n", ShadowRecs[i].pid );
			delete_shadow_rec( ShadowRecs[i].pid );
		}
	}
	dprintf( D_FULLDEBUG, "============ End clean_shadow_recs =============\n" );
}

void
Scheduler::preempt(int n)
{
	int		i;

	dprintf( D_ALWAYS, "Called preempt( %d )\n", n );
	for( i=NShadowRecs-1; n > 0 && i >= 0; n--, i-- ) {
		if( is_alive(ShadowRecs[i].pid) ) {
			if (ShadowRecs[i].match) {
				if( ShadowRecs[i].preempted ) {
					send_vacate( ShadowRecs[i].match, KILL_FRGN_JOB );
				} else {
					send_vacate( ShadowRecs[i].match, CKPT_FRGN_JOB );
					ShadowRecs[i].preempted = TRUE;
				}
			}
		} else {
			dprintf( D_ALWAYS,
			"Have ShadowRec for pid %d, but is dead!\n", ShadowRecs[i].pid );
			dprintf( D_ALWAYS, "Deleting now...\n" );
			delete_shadow_rec( ShadowRecs[i].pid );
		}
	}
}

void
send_vacate(Mrec* match,int cmd)
{
	ReliSock	sock(match->peer, START_PORT);

	dprintf( D_ALWAYS, "Called send_vacate( %s, %d )\n", match->peer, cmd );
        /* Connect to the specified host */
	
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
	int		status;

	dprintf( D_ALWAYS, "Checking consistency running and runnable jobs\n" );
	BadCluster = -1;
	BadProc = -1;

	for( i=0; i<N_PrioRecs; i++ ) {
		if( (srp=find_shadow_rec(&PrioRec[i].id)) ) {
			BadCluster = srp->job_id.cluster;
			BadProc = srp->job_id.proc;
			GetAttributeInt(BadCluster, BadProc, ATTR_JOB_STATUS, &status);
			if (status != RUNNING) {
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
find_shadow_rec(PROC_ID* id)
{
	int		i;
	int		my_cluster;
	int		my_proc;

	my_cluster = id->cluster;
	my_proc = id->proc;

	for( i=0; i<NShadowRecs; i++ ) {
		if( my_cluster == ShadowRecs[i].job_id.cluster &&
			my_proc == ShadowRecs[i].job_id.proc ) {
				return &ShadowRecs[i];
		}
	}
	return NULL;
}

#ifdef CARMI_OPS
struct shadow_rec*
find_shadow_by_cluster( PROC_ID *id )
{
	int		i;
	int		my_cluster;

	my_cluster = id->cluster;

	for( i=0; i<NShadowRecs; i++ ) {
		if( my_cluster == ShadowRecs[i].job_id.cluster) {
				return &ShadowRecs[i];
		}
	}
	return NULL;
}
#endif

void
Scheduler::mail_problem_message()
{
	char	cmd[512];
	char	hostname[512];
	FILE	*mailer;

	dprintf( D_ALWAYS, "Mailing administrator (%s)\n", CondorAdministrator );

	if( gethostname(hostname,sizeof(hostname)) < 0 ) {
		EXCEPT( "gethostname(0x%x,%d)", hostname, sizeof(hostname) );
	}

	(void)sprintf( cmd, "%s %s", Mail, CondorAdministrator );
	if( (mailer=popen(cmd,"w")) == NULL ) {
		EXCEPT( "popen(\"%s\",\"w\")", cmd );
	}

	fprintf( mailer, "To: %s\n", CondorAdministrator );
	fprintf( mailer, "Subject: Condor Problem\n" );
	fprintf( mailer, "\n" );

	fprintf( mailer, "Job %d.%d is in the runnable job table,\n",
												BadCluster, BadProc );
	fprintf( mailer, "but we already have a shadow record for it.\n" );

		/* don't use 'pclose()' here, it does its own wait, and messes
	       with our handling of SIGCHLD! */
		/* except on HPUX it is both safe and required */
#if defined(HPUX9)
	pclose( mailer );
#else
	(void)fclose( mailer );
#endif

}

void
NotifyUser(shadow_rec* srec, char* msg, int status)
{
	int fd, notification;
	char owner[20], url[25], buf[80];
	char cmd[_POSIX_PATH_MAX], args[_POSIX_PATH_MAX];

	if (GetAttributeInt(srec->job_id.cluster, srec->job_id.proc,
						"Notification", &notification) < 0) {
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
		break;
	case NOTIFY_ERROR:
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			return;
		break;
	default:
		dprintf(D_ALWAYS, "Condor Job %d.%d has a notification of %d\n",
				srec->job_id.cluster, srec->job_id.proc, notification );
	}

	if (GetAttributeString(srec->job_id.cluster, srec->job_id.proc,
						   "NotifyUser", owner) < 0) {
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

	sprintf(url, "mailto:%s", owner);
	if ((fd = open_url(url, O_WRONLY, 0)) < 0) {
		EXCEPT("condor_open_mailto_url(%s, %d, 0)", owner, O_WRONLY, 0);
	}

	sprintf(buf, "From: Condor\n");
	write(fd, buf, strlen(buf));
	sprintf(buf, "To: %s\n", owner);
	write(fd, buf, strlen(buf));
	sprintf(buf, "Subject: Condor Job %d.%d\n", srec->job_id.cluster,
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
}

/*
** Allow child processes to die a decent death, don't keep them
** hanging around as <defunct>.
**
** NOTE: This signal handler calls routines which will attempt to lock
** the job queue.  Be very careful it is not called when the lock is
** already held, or deadlock will occur!
*/
void
Scheduler::reaper(int sig, int code, struct sigcontext* scp)
{
    pid_t   	pid;
    int     	status;
	Mrec*		mrec;
	shadow_rec*	srec;

	block_signal(SIGALRM);
    if( sig == 0 ) {
        dprintf( D_ALWAYS, "***********  Begin Extra Checking ********\n" );
    } else {
        dprintf( D_ALWAYS, "Entered reaper( %d, %d, 0x%x )\n", sig, code, scp );
    }

    for(;;) {
        if( (pid = waitpid(-1,&status,WNOHANG)) <= 0 ) {
            dprintf( D_FULLDEBUG, "waitpid() returned %d, errno = %d\n",
                                                            pid, errno );
            break;
        }
		if((mrec = FindMrecByPid(pid))) {
			if(WIFEXITED(status)) {
                dprintf(D_FULLDEBUG, "agent pid %d exited with status %d\n",
                        pid, WEXITSTATUS(status) );
                if(WEXITSTATUS(status) == EXITSTATUS_NOTOK) {
                    dprintf(D_ALWAYS, "capability rejected by startd\n");
                    DelMrec(mrec);
                } else {
                    dprintf(D_ALWAYS, "Agent contacting startd successful\n");
					mrec->status = M_ACTIVE;
                }
            } else if(WIFSIGNALED(status)) {
                dprintf(D_ALWAYS, "Agent pid %d died with signal %d\n",
                        pid, WTERMSIG(status));
                DelMrec(mrec);
            }
		} else if ((srec = FindSrecByPid(pid)) != NULL && srec->match == NULL) {
				// Shadow record without a capability... must be a
				// scheduler universe process.
			if(WIFEXITED(status)) {
				dprintf(D_FULLDEBUG,
						"scheduler universe job (%d.%d) pid %d "
						"exited with status %d\n", srec->job_id.cluster,
						srec->job_id.proc, pid, WEXITSTATUS(status) );
				NotifyUser(srec, "exited with status ",
						   WEXITSTATUS(status) );
			} else if(WIFSIGNALED(status)) {
				dprintf(D_ALWAYS,
						"scheduler universe job (%d.%d) pid %d died "
						"with signal %d\n", srec->job_id.cluster,
						srec->job_id.proc, pid, WTERMSIG(status));
				NotifyUser(srec, "was killed by signal ",
						   WTERMSIG(status));
			}
			if( DestroyProc(srec->job_id.cluster, srec->job_id.proc) ) {
				dprintf(D_ALWAYS, "DestroyProc(%d.%d) failed -- "
						"presumably job was already removed\n",
						srec->job_id.cluster,
						srec->job_id.proc);
			}
			delete_shadow_rec( pid );
		} else {
				// Shadow record with capability... must be a real shadow.
			if( WIFEXITED(status) ) {
                dprintf( D_FULLDEBUG, "Shadow pid %d exited with status %d\n",
                                                pid, WEXITSTATUS(status) );
                if( WEXITSTATUS(status) == JOB_NO_MEM ) {
                    swap_space_exhausted();
                } else if( WEXITSTATUS(status) == JOB_NOT_STARTED ) {
						/* unable to start job -- throw away match */
					Relinquish(srec->match);
					DelMrec(srec->match);
				}
            } else if( WIFSIGNALED(status) ) {
                dprintf( D_FULLDEBUG, "Shadow pid %d died with signal %d\n",
                                                pid, WTERMSIG(status) );
            }
			delete_shadow_rec( pid );
        }
    }
    if( sig == 0 ) {
        dprintf( D_ALWAYS, "***********  End Extra Checking ********\n" );
    }
	if( ExitWhenDone && NShadowRecs == 0 ) {
		dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
		kill( getpid(), SIGKILL );
	}
	unblock_signal(SIGALRM);
}

void
Scheduler::kill_zombie(int pid, PROC_ID* job_id )
{
    dprintf( D_ALWAYS,
        "Shadow %d died, and left job %d.%d marked RUNNING\n",
        pid, job_id->cluster, job_id->proc );

    mark_job_stopped( job_id);
}

/*
** The shadow running this job has died.  If things went right, the job
** has been marked as idle, unexpanded, or completed as appropriate.
** However, if the shadow terminated abnormally, the job might still
** be marked as running (a zombie).  Here we check for that conditon,
** and mark the job with the appropriate status.
*/
void
Scheduler::check_zombie(int pid, PROC_ID* job_id)
{
 
    int     status;

    dprintf( D_ALWAYS, "Entered check_zombie( %d, 0x%x )\n", pid, job_id );

    if (GetAttributeInt(job_id->cluster, job_id->proc, ATTR_JOB_STATUS,
						&status) < 0){
        status = REMOVED;
    }

    switch( status ) {
        case RUNNING:
            kill_zombie( pid, job_id );
            break;
        case REMOVED:
            cleanup_ckpt_files( pid, job_id );
            break;
        default:
            break;
    }

    dprintf( D_ALWAYS, "Exited check_zombie( %d, 0x%x )\n", pid, job_id );
}

void
Scheduler::cleanup_ckpt_files(int pid, PROC_ID* job_id)
{
    char    ckpt_name[MAXPATHLEN];

        /* Remove any checkpoint file */

    strcpy(ckpt_name, gen_ckpt_name(Spool,job_id->cluster,job_id->proc,0) );

    (void)unlink( ckpt_name );

        /* Remove any temporary checkpoint file */
    (void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d.tmp",
                                Spool, job_id->cluster, job_id->proc  );
    (void)unlink( ckpt_name );
}

void
Scheduler::SetClassAd(ClassAd* a)
{
	if(ad)
	{
		delete ad;
	}
	ad = a;
}

// initialize the configuration parameters
void
Scheduler::Init()
{
	char*					tmp;
	char					tmpExpr[200];

	// set the name of the schedd
	if(ScheddName) {
		sprintf(tmpExpr, "SCHEDD__%s_PORT", Name); 
		tmp = param(tmpExpr);
		if(tmp) {
			port = atoi(tmp);
			free(tmp);
		}
	} else {
		tmp = param("SCHEDD_PORT");
		if(tmp) {
			port = atoi(tmp);
			free(tmp);
		}
	} 

	CollectorHost = param( "COLLECTOR_HOST" );
    if( CollectorHost == NULL ) {
        EXCEPT( "No Collector host specified in config file\n" );
    }
	UdpSock = udp_unconnect();

	// Assume that we are still using machine names in the configuration file.
	// If we move to ip address and port, this need to be changed.
	AccountantName = param( "ACCOUNTANT_HOST" );
    if( AccountantName == NULL ) {
        dprintf(D_ALWAYS, "No Accountant host specified in config file\n" );
    }

    NegotiatorHost = param( "NEGOTIATOR_HOST" );
    if( NegotiatorHost == NULL ) {
        EXCEPT( "No NegotiatorHost host specified in config file\n" );
    }

    tmp = param( "SCHEDD_INTERVAL" );
    if( tmp == NULL ) {
        SchedDInterval = 120;
    } else {
        SchedDInterval = atoi( tmp );
        free( tmp );
    }

    if( (Shadow=param("SHADOW")) == NULL ) {
        EXCEPT( "SHADOW not specified in config file\n" );
    }

    if( (tmp=param("MAX_JOB_STARTS")) == NULL ) {
        MaxJobStarts = 5;
    } else {
        MaxJobStarts = atoi( tmp );
        free( tmp );
    }

    if( (tmp=param("MAX_JOBS_RUNNING")) == NULL ) {
        MaxJobsRunning = 15;
    } else {
        MaxJobsRunning = atoi( tmp );
        free( tmp );
    }

    if( (tmp=param("RESERVED_SWAP")) == NULL ) {
        ReservedSwap = 5 * 1024;            /* 5 megabytes */
    } else {
        ReservedSwap = atoi( tmp ) * 1024;  /* Value specified in megabytes */
        free( tmp );
    }

    if( (tmp=param("SHADOW_SIZE_ESTIMATE")) == NULL ) {
        ShadowSizeEstimate = DEFAULT_SHADOW_SIZE;
    } else {
        ShadowSizeEstimate = atoi( tmp );   /* Value specified in kilobytes */
        free( tmp );
    }

    if( (CondorAdministrator = param("CONDOR_ADMIN")) == NULL ) {
        EXCEPT( "CONDOR_ADMIN not specified in config file" );
    }

    /* parameters for UPDOWN alogorithm */
	if( (tmp = param( "SCHEDD_SCALING_FACTOR" )) == NULL ) {
		ScheddScalingFactor = 60;
	} else {
		ScheddScalingFactor = atoi( tmp );
		free( tmp );
	}
    if( ScheddScalingFactor == 0 ) {
		ScheddScalingFactor = 60;
	}

    if( (tmp = param( "SCHEDD_HEAVY_USER_TIME" )) == NULL ) {
		ScheddHeavyUserTime = 120;
	} else {
		ScheddHeavyUserTime = atoi( tmp );
		free( tmp );
	}

    UidDomain = param( "UID_DOMAIN" );
    if( NegotiatorHost == NULL ) {
        EXCEPT( "No UID_DOMAIN specified in config file\n" );
    }

    Step                    = SchedDInterval/ScheddScalingFactor;
    ScheddHeavyUserPriority = Step * ScheddHeavyUserTime;

    upDown_SetParameters(Step,ScheddHeavyUserPriority); /* UPDOWN */

    /* update upDown object from disk file */
    /* ignore errors              */
	filename = new char[MAXPATHLEN];
    sprintf(filename,"%s/%s",Spool, "UserPrio");
    upDown_ReadFromFile(filename);                /* UPDOWN */

    if( (Mail=param("MAIL")) == NULL ) {
        EXCEPT( "MAIL not specified in config file\n" );
    }

	tmp = param("ALIVE_FREQUENCY");
    if(!tmp) {
		aliveFrequency = SchedDInterval;
    } else {
        aliveFrequency = atoi(tmp);
        free(tmp);
	}
	if(aliveFrequency < SchedDInterval) {
		aliveInterval = 1;
	} else {
		aliveInterval = aliveFrequency / SchedDInterval;
	}
}

void
Scheduler::Register(DaemonCore* core)
{
	// message handlers for schedd commands
	core->Register(this, NEGOTIATE, (void*)negotiate, REQUEST);
	core->Register(this, RESCHEDULE, (void*)reschedule_negotiator, REQUEST);
	core->Register(this, VACATE_SERVICE, (void*)vacate_service, REQUEST);
	core->Register(this, KILL_FRGN_JOB, (void*)abort_job, REQUEST);

	// handler for queue management commands
	core->Register(NULL, QMGMT_CMD, (void*)handle_q, REQUEST);

	// timer handler
	core->Register(this, 10, (void*)timeout, SchedDInterval, TIMER);

	// signal handlers
	core->Register(this, SIGCHLD, (void*)reaper, SIGNAL);
	core->Register(this, SIGINT, (void*)sigint_handler, SIGNAL);
	core->Register(this, SIGHUP, (void*)sighup_handler, SIGNAL);
	core->Register(this, SIGQUIT, (void*)sigquit_handler, SIGNAL);
	core->Register(this, SIGTERM, (void*)sigterm_handler, SIGNAL);
	// Daemon_core doesn't handle SIG_IGN, properly, just use the
	// system for signal handling in this case. -Derek 6/27/97
	install_sig_handler(SIGPIPE, SIG_IGN);

	// these functions are called after the select call in daemon core's loop
	core->Register(this, (void*)StartJobs, FALSE);	// call anytime we break from select()
	core->Register(this, (void*)send_alive, TRUE);  // call only on a select timeout
}

extern "C" {
int
prio_compar(prio_rec* a, prio_rec* b)
{
    /* compare up down priorities */
    /* lower values mean more priority */
    if( a->prio > b->prio ) {
        return 1;
    }
    if( a->prio < b->prio ) {
        return -1;
    }

    /* here updown priorities are equal */
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


    /* finally, check for job submit times */
    if( a->qdate < b->qdate ) {
        return -1;
    }
    if( a->qdate > b->qdate ) {
        return 1;
    }


    /* give up!!! */
    return 0;
}
} // end of extern

void
Scheduler::sigint_handler()
{
	dprintf( D_ALWAYS, "Killed by SIGINT\n" );
	exit(0);
}

void
Scheduler::sighup_handler()
{
    dprintf( D_ALWAYS, "Re-reading config file\n" );

	config( ad );

	Init();
	::Init();
    timeout();
}


// Perform graceful shutdown.
void
Scheduler::sigterm_handler()
{
	block_signal(SIGCHLD);
    dprintf( D_ALWAYS, "Performing graceful shut down.\n" );
	if( NShadowRecs == 0 ) {
		dprintf( D_ALWAYS, "All shadows are gone, exiting.\n" );
		kill( getpid(), SIGKILL );  
	} else {
			/* 
			   There are shadows running, so set a flag that tells the
			   reaper to exit when all the shadows are gone, and start
			   shutting down shadows.
 		    */
		MaxJobsRunning = 0;
		ExitWhenDone = 1;
		preempt( NShadowRecs );
		unblock_signal(SIGCHLD);
	}
}

// Perform fast shutdown.
void
Scheduler::sigquit_handler()
{
	int i;
	dprintf( D_ALWAYS, "Performing fast shut down.\n" );
	for(i=0; i < NShadowRecs; i++) {
		kill( ShadowRecs[i].pid, SIGKILL );
	}
	dprintf( D_ALWAYS, "All shadows have been killed, exiting.\n" );
	kill( getpid(), SIGKILL );
}


void
Scheduler::reschedule_negotiator(ReliSock*, struct sockaddr_in*)
{
    int     	cmd = RESCHEDULE;
	ReliSock	sock(NegotiatorHost, NEGOTIATOR_PORT);

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

Mrec*
Scheduler::AddMrec(char* id, char* peer, PROC_ID* jobId)
{
	if(nMrec >= MAXMATCHES)
	{
		dprintf(D_ALWAYS, "Max # of matches exceeded --- match not added\n"); 
		return NULL;
	}
	if(!id || !peer)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not added\n"); 
		return NULL;
	} 
	rec[nMrec] = new Mrec(id, peer, jobId);
	if(!rec[nMrec])
	{
		EXCEPT("Out of memory!");
	} 
	nMrec++;
	return rec[nMrec - 1];
}

int
Scheduler::DelMrec(char* id)
{
	if(!id)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not deleted\n");
		return -1;
	}
	for(int i = 0; rec[i]; i++)
	{
		if(strcmp(id, rec[i]->id) == 0)
		{
			dprintf(D_FULLDEBUG, "Match record (%s, %s, %d, %d) deleted\n",
					rec[i]->id, rec[i]->peer, rec[i]->cluster, rec[i]->proc); 
				// Remove this match from the associated shadowRec.
			rec[i]->shadowRec->match = NULL;
			delete rec[i];
			nMrec--; 
			rec[i] = rec[nMrec];
			rec[nMrec] = NULL;
		}
	}
	return 0;
}

int
Scheduler::DelMrec(Mrec* match)
{
	if(!match)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not deleted\n");
		return -1;
	}
	for(int i = 0; rec[i]; i++)
	{
		if(match == rec[i])
		{
			dprintf(D_FULLDEBUG, "Match record (%s, %s, %d) deleted\n",
					rec[i]->id, rec[i]->peer, rec[i]->cluster); 
			delete rec[i];
			nMrec--; 
			rec[i] = rec[nMrec];
			rec[nMrec] = NULL;
		}
	}
	return 0;
}

int
Scheduler::MarkDel(char* id)
{
	if(!id)
	{
		dprintf(D_ALWAYS, "Null parameter --- match not marked deleted\n");
		return -1;
	}
	for(int i = 0; rec[i]; i++)
	{
		if(strcmp(id, rec[i]->id) == 0)
		{
			dprintf(D_FULLDEBUG, "Match record (%s, %s, %d) marked deleted\n",
					rec[i]->id, rec[i]->peer, rec[i]->cluster);
			rec[i]->status = M_DELETED; 
		}
	}
	return 0;
}

void
Scheduler::Agent(char* server, char* capability, 
				 char* name, int aliveFrequency, PROC_ID* jobId) 
{
    int     	reply;                              /* reply from the startd */

	ClassAd jobAd;
	getJobAd( jobId->cluster, jobId->proc, &jobAd );

	ReliSock sock(server, 0);
		
	dprintf (D_PROTOCOL, "## 5. Requesting resource from %s ...\n", server);
	sock.encode();

	if( !sock.put( REQUEST_SERVICE ) ) {
		dprintf( D_ALWAYS, "Couldn't send command to startd.\n" );	
		exit(EXITSTATUS_NOTOK);
	}

	if( !sock.code( capability ) ) {
		dprintf( D_ALWAYS, "Couldn't send capability to startd.\n" );	
		exit(EXITSTATUS_NOTOK);
	}

	if( !jobAd.put( sock ) ) {
		dprintf( D_ALWAYS, "Couldn't send job classad to startd.\n" );	
		exit(EXITSTATUS_NOTOK);
	}
	
	if( !sock.eom() ) {
		dprintf( D_ALWAYS, "Couldn't send eom to startd.\n" );	
		exit(EXITSTATUS_NOTOK);
	}

	if( !sock.rcv_int(reply, TRUE) ) {
		dprintf( D_ALWAYS, "Couldn't receive response from startd.\n" );	
		exit(EXITSTATUS_NOTOK);
	}

	if( reply == OK ) {
		dprintf (D_PROTOCOL, "(Request was accepted)\n");
		sock.encode();
		if( !sock.code(name) ) {
			dprintf( D_ALWAYS, "Couldn't send schedd string to startd.\n" );	
			exit(EXITSTATUS_NOTOK);
		}
		if( !sock.snd_int(aliveFrequency, TRUE) ) {
			dprintf( D_ALWAYS, "Couldn't receive response from startd.\n" );	
			exit(EXITSTATUS_NOTOK);
		}
		dprintf( D_ALWAYS, "alive frequency %d secs\n", aliveFrequency );
	} else if( reply == NOT_OK ) {
		dprintf( D_PROTOCOL, "(Request was NOT accepted)\n" );
		exit(EXITSTATUS_NOTOK);
	} else {
		dprintf( D_ALWAYS, "Unknown reply from startd, agent exiting.\n" ); 
		exit(EXITSTATUS_NOTOK);	
	}
	exit(EXITSTATUS_OK);
}

Mrec*
Scheduler::FindMrecByPid(int pid)
{
	int		i;

	for(i = 0; i < nMrec; i++)
	{
		if(rec[i]->agentPid == pid)
		// found the match record
		{
			return rec[i];
		}
	}
	return NULL;
}

shadow_rec*
Scheduler::FindSrecByPid(int pid)
{
	int		i;

	for(i = 0; i < NShadowRecs; i++)
	{
		if(ShadowRecs[i].pid == pid)
		// found the match record
		{
			return ShadowRecs+i;
		}
	}
	return NULL;
}

/*
 * Weiru
 * Inform the startd and the accountant of the relinquish of the resource.
 */
void
Scheduler::Relinquish(Mrec* mrec)
{
	ReliSock	*sock;
	int	   		flag = FALSE;

	// inform the startd

	sock = new ReliSock(mrec->peer, 0);
	sock->encode();
	if(!sock->put(RELINQUISH_SERVICE))
	{
		dprintf(D_ALWAYS, "Can't relinquish startd. Match record is:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
	}
	else if(!sock->put(mrec->id) || !sock->eom())
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
	int			i;

	for(i = 0; i < nMrec; i++)
	{
		if(rec[i]->shadowRec == shadow)
		{
			rec[i]->shadowRec = NULL;
			rec[i]->proc = -1;
		}
	}
}

int
Scheduler::AlreadyMatched(PROC_ID* id)
{
	int			i;
	int universe;

	if (GetAttributeInt(id->cluster, id->proc, ATTR_JOB_UNIVERSE, &universe) < 0)
		dprintf(D_ALWAYS, "GetAttributeInt() failed\n");

	if (universe == PVM)
		return FALSE;

	for(i = 0; i < nMrec; i++)
	{
		if(rec[i]->cluster == id->cluster && rec[i]->proc == id->proc)
		{
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
    int     	i, j;

	if(aliveFrequency == 0)
	/* no need to send alive message */
	{
		return;
	}
    for(i = 0, j = 0; i < nMrec; i++)
    {
		if(rec[i]->status != M_ACTIVE)
		{
			continue;
		}
        rec[i]->alive_countdown--;
        if(rec[i]->alive_countdown <= 0)
        /* time to send alive message */
        {
            rec[i]->alive_countdown = aliveInterval;
			dprintf (D_PROTOCOL,"## 6. Sending alive msg to %s\n",rec[i]->peer);
			j++;
			sock = new SafeSock(rec[i]->peer, 0);
			sock->encode();
			if( !sock->put(ALIVE) || 
				!sock->code(rec[i]->id) || 
				!sock->end_of_message() ) {
					// UDP transport out of buffer space!
                dprintf(D_ALWAYS, "\t(Can't send alive message to %d)\n",
                        rec[i]->peer);
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
    }
    dprintf(D_PROTOCOL,"## 6. (Done sending alive messages to %d startds)\n",j);
}

void
job_prio(int cluster, int proc)
{
	sched->JobsRunning += get_job_prio(cluster, proc);
}

void
Scheduler::SetSockName(int sock)
{
	struct sockaddr_in		addr;
	int						addrLen; 
	char					tmpExpr[200];

	memset((char*)&addr, 0, sizeof(addr));
	errno = 0;
	addrLen = sizeof(addr); 
	if(getsockname(sock, (struct sockaddr*)&addr, &addrLen) < 0)
	{
		EXCEPT("getservbyname(), errno = %d", errno);
	}
	
	if(port == 0)
	{
		port = ntohs(addr.sin_port);
		dprintf(D_ALWAYS, "System assigned port %d\n", port);
	}
	else
	{
		dprintf(D_ALWAYS, "Configured port %d, ", port);
		dprintf(D_ALWAYS, "System assigned port %d\n", ntohl(addr.sin_port));
	} 
		
	addr.sin_family = AF_INET;
	get_inet_address(&(addr.sin_addr));
	MySockName = strdup(sin_to_string(&addr));
	dprintf(D_FULLDEBUG, "MySockName = %s\n", MySockName);
	if (MySockName !=0){
		sprintf(tmpExpr, "%s = \"%s\"", ATTR_SCHEDD_IP_ADDR, MySockName);
		ad->Insert(tmpExpr);
	}
	else
	{
		EXCEPT("Can't get socket name");
	} 
}	
