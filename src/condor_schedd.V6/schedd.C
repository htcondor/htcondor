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
*/ 

#define _POSIX_SOURCE
#include "condor_common.h"

#include <signal.h>
#include <sys/signal.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "condor_common.h"
#include "condor_xdr.h"
#include "sched.h"
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "proc.h"
#include "exit.h"
#include "dgram_io_handle.h"
#include "condor_collector.h"
#include "scheduler.h"
#include "condor_attributes.h"

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
#define SHADOW_SIZE_INCR	75

#define SUCCESS 1
#define CANT_RUN 0

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
extern char *gen_ckpt_name();
extern void	Init();

#include "condor_qmgr.h"

#define MAX_PRIO_REC 2048
#define MAX_SHADOW_RECS 512

extern "C"
{
    void upDown_ClearUserInfo(void);
    void upDown_UpdateUserInfo(const char* , const int);
    int upDown_GetNoOfActiveUsers(void);
    int upDown_GetUserPriority(const char* , int* );
    void upDown_UpdatePriority(void);
    void upDown_SetParameters(const int , const int);
    void upDown_ReadFromFile(const char* );
    void upDown_WriteToFile(const char*);
    void upDown_Display(void);
    void dprintf(int, char*...);
	int	 xdr_proc_id(XDR*, PROC_ID*);
	int	 calc_virt_memory();
	int	 rcv_int(XDR*, int*, int);
	int	 snd_int(XDR*, int, int);
	int	 snd_string(XDR*, char*, int);
	int	 snd_context(XDR*, CONTEXT*, int);
	int	 rcv_string(XDR*, char**, int);
	int	 getdtablesize();
	int	 set_root_euid(char*, int);
	void _EXCEPT_(char*...);
	char* gen_ckpt_name(char*, int, int, int);
	int	 do_connect(char*, char*, u_int);
	int	 gethostname(char*, int);
	int		send_context_to_machine(DGRAM_IO_HANDLE*, int, CONTEXT*);
	int	 boolean(char*, char*);
	char*	param(char*);
	void	block_signal(int);
	void	unblock_signal(int);
	void	config(char*, CONTEXT*);
	void	handle_q(XDR*, struct sockaddr_in*);
	void	fill_dgram_io_handle(DGRAM_IO_HANDLE*, char*, int, int);
	int		udp_unconnect();
	char*	sin_to_string(struct sockaddr_in*);
	EXPR*	build_expr(char*, ELEM*);
	void	get_inet_address(struct in_addr*);
	int		prio_compar(prio_rec*, prio_rec*);
}

extern	int	send_classad_to_machine(DGRAM_IO_HANDLE*, int, const ClassAd*);
extern	int	get_job_prio(int, int);
extern	void	FindRunnableJob(int, int&);

extern	char*		myName;
extern	char*		Spool;
extern	char*		CondorAdministrator;
extern	Scheduler*	sched;
extern	CONTEXT*	MachineContext;

// shadow records and priority records
shadow_rec      ShadowRecs[MAX_SHADOW_RECS];
int             NShadowRecs;
extern	prio_rec        PrioRec[];
extern	int             N_PrioRecs;

void	check_zombie(int, PROC_ID*);
void	send_vacate(Mrec*, int);
#if defined(__STDC__)
shadow_rec*     find_shadow_rec(PROC_ID*);
shadow_rec*     add_shadow_rec(int, PROC_ID*, Mrec*, int);
#else
shadow_rec*     find_shadow_rec();
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
    memset((char*)&CollectorHandle, 0, sizeof(CollectorHandle));
	rec = new Mrec*[MAXMATCHES];
	aliveFrequency = 0;
	aliveInterval = 0;
}

Scheduler::~Scheduler()
{
	int		i;

    delete ad;
    delete MySockName;
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

void Scheduler::timeout()
{
	char		tmpExpr[200];

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

	update_central_mgr();

		/* Neither of these should be needed! */
	reaper( 0, 0, 0 );
	clean_shadow_recs();

	if( NShadowRecs > MaxJobsRunning ) {
		preempt( NShadowRecs - MaxJobsRunning );
	}
	unblock_signal(SIGCHLD);
}

/*
** Examine the job queue to determine how many CONDOR jobs we currently have
** running, and how many individual users own them.
*/
Scheduler::count_jobs()
{
	char	queue[MAXPATHLEN];
	int		i;
	int		prio_compar();
/*
	char	tmp[200];
*/
	ELEM	tmp;
	ExprTree*	tree;

	N_Owners = 0;
	JobsRunning = 0;
	JobsIdle = 0;

	WalkJobQueue((int(*)(int, int)) count );

	dprintf( D_FULLDEBUG, "JobsRunning = %d\n", JobsRunning );
	dprintf( D_FULLDEBUG, "JobsIdle = %d\n", JobsIdle );
	dprintf( D_FULLDEBUG, "N_Owners = %d\n", N_Owners );
	dprintf( D_FULLDEBUG, "MaxJobsRunning = %d\n", MaxJobsRunning );

	for ( i=0; i<N_Owners; i++) {
		FREE( Owners[i] );
	}

    tmp.type = INT;
    tmp.val.integer_val = JobsRunning;
    store_stmt( build_expr("Running",&tmp), MachineContext );

    tmp.val.integer_val = JobsIdle;
    store_stmt( build_expr("Idle",&tmp), MachineContext );

    tmp.val.integer_val = N_Owners;
    store_stmt( build_expr("Users",&tmp), MachineContext );

    tmp.val.integer_val = MaxJobsRunning;
    store_stmt( build_expr("MAX_JOBS_RUNNING",&tmp), MachineContext );

	/*
	sprintf(tmp, "RunningJobs = %d", JobsRunning);
	ad->Insert(tmp);

	sprintf(tmp, "IdleJobs = %d", JobsIdle);
	ad->Insert(tmp);

	sprintf(tmp, "Users = %d", N_Owners);
	ad->Insert(tmp);

	sprintf(tmp, "MAX_JOBS_RUNNING = %d", MaxJobsRunning);
	ad->Insert(tmp);
	*/
}

prio_comrpar(struct prio_rec* a, struct prio_rec* b )
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

count( int cluster, int proc )
{
	int		status;
	char 	buf[100];
	char*	owner;
	int		cur_hosts;
	int		max_hosts;

	GetAttributeInt(cluster, proc, "Status", &status);
	GetAttributeString(cluster, proc, "Owner", buf);
	owner = buf;
	if (GetAttributeInt(cluster, proc, "CurrentHosts", &cur_hosts) < 0) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (GetAttributeInt(cluster, proc, "MaxHosts", &max_hosts) < 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}

	sched->JobsRunning += cur_hosts;
	sched->JobsIdle += (max_hosts - cur_hosts);

	sched->insert_owner( owner );

#define UpDownRunning	1 /* these shud be same as up_down.h */
#define UpDownIdle	2
#define	Error		-1

	if ( status == RUNNING ) {	/* UPDOWN */
		upDown_UpdateUserInfo(owner,UpDownRunning);
	} else if ( (status == IDLE)||(status == UNEXPANDED)) {
		upDown_UpdateUserInfo(owner,UpDownIdle);
	}
}

void Scheduler::insert_owner(char* owner)
{
	int		i;
	for ( i=0; i<N_Owners; i++ ) {
		if( strcmp(Owners[i],owner) == MATCH ) {
			return;
		}
	}
	Owners[i] = strdup( owner );
	N_Owners +=1;
}

void Scheduler::update_central_mgr()
{
	int		cmd = SCHEDD_INFO;
	
	send_context_to_machine(&CollectorHandle, cmd, MachineContext);
}


void Scheduler::abort_job(XDR* xdrs, struct sockaddr_in*)
{
	PROC_ID	job_id;
	char	*host;
	int		i;


	xdrs->x_op = XDR_DECODE;
	if( !xdr_proc_id(xdrs,&job_id) ) {
		dprintf( D_ALWAYS, "abort_job() can't read job_id\n" );
		return;
	}

	for( i=0; i<NShadowRecs; i++ ) {
		if( ShadowRecs[i].job_id.cluster == job_id.cluster &&
			(ShadowRecs[i].job_id.proc == job_id.proc || job_id.proc == -1)) {

			host = ShadowRecs[i].match->peer;
			dprintf( D_ALWAYS,
				"Found shadow record for job %d.%d, host = %s\n",
				job_id.cluster, job_id.proc, host );
			/* send_kill_command( host ); */
            /* change for condor flocking */
            if ( kill( ShadowRecs[i].pid, SIGUSR1) == -1 )
                dprintf(D_ALWAYS,"Error in sending SIGUSR1 to %d errno = %d\n",

                            ShadowRecs[i].pid, errno);
            else dprintf(D_ALWAYS, "Send SIGUSR1 to Shadow Pid %d\n",
                            ShadowRecs[i].pid);

		}
	}
}
#if 0
send_kill_command( host )
char	*host;
{
	XDR		xdr, *xdrs = NULL;
	int		sock = -1;
	int		cmd;

      /* Connect to the startd on the serving host */
    if( (sock = do_connect(host, "condor_startd", START_PORT)) < 0 ) {
        dprintf( D_ALWAYS, "Can't connect to startd on %s\n", host );
        return;
    }
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;

    cmd = KILL_FRGN_JOB;
    if( !xdr_int(xdrs, &cmd) ) {
		dprintf( D_ALWAYS, "Can't send KILL_FRGN_JOB cmd to schedd on %s\n",
			host );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}
		
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		dprintf( D_ALWAYS, "Can't send xdr end_of_record to schedd on %s\n",
			host );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}

    dprintf( D_ALWAYS, "Sent KILL_FRGN_JOB command to startd on %s\n", host );
	xdr_destroy( xdrs );
	close( sock );
}
#endif 

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
void Scheduler::negotiate(XDR* xdrs, struct sockaddr_in*)
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

	SwapSpace = calc_virt_memory();

	N_PrioRecs = 0;

	WalkJobQueue( (int(*)(int, int))job_prio );

	if( !shadow_prio_recs_consistent() ) {
		mail_problem_message();
	}

	qsort((char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]),
		  (int(*)(const void*, const void*))prio_compar);
	jobs = N_PrioRecs;

	dprintf(D_UPDOWN,"USER\t\tPID\tUPDOWN_PRIO\tJOB_PRIO\tSTATUS\tQDATE\n");
	for( i=0; i<N_PrioRecs; i++ ) {
		dprintf( D_UPDOWN, "%s\t:%d.%d\t%d\t\t%d\t\t%d\t%s\n",
				PrioRec[i].owner, PrioRec[i].id.cluster, PrioRec[i].id.proc, 
				PrioRec[i].prio ,PrioRec[i].job_prio, PrioRec[i].status,
				ctime( (time_t*) &(PrioRec[i].qdate)));
	}

	N_RejectedClusters = 0;
	SwapSpaceExhausted = FALSE;
	JobsStarted = 0;
	if( ShadowSizeEstimate == 0 ) {
		ShadowSizeEstimate = DEFAULT_SHADOW_SIZE;
	}
	start_limit_for_swap = (SwapSpace - ReservedSwap) / ShadowSizeEstimate;

	dprintf( D_FULLDEBUG, "*** SwapSpace = %d\n", SwapSpace );
	dprintf( D_FULLDEBUG, "*** ReservedSwap = %d\n", ReservedSwap );
	dprintf( D_FULLDEBUG, "*** Shadow Size Estimate = %d\n",ShadowSizeEstimate);
	dprintf( D_FULLDEBUG, "*** Start Limit For Swap = %d\n",
													start_limit_for_swap);

		/* Try jobs in priority order */
	for( i=0; i < N_PrioRecs; i++ ) {

		if(AlreadyMatched(&PrioRec[i].id))
		{
			continue;
		}

		id = PrioRec[i].id;
		if( cluster_rejected(id.cluster) ) {
			continue;
		}

		if (GetAttributeInt(PrioRec[i].id.cluster, PrioRec[i].id.proc,
							"CurrentHosts", &cur_hosts) < 0) {
			cur_hosts = 0;
		}

		if (GetAttributeInt(PrioRec[i].id.cluster, PrioRec[i].id.proc,
						"MaxHosts", &max_hosts) < 0) {
			max_hosts = 1;
		}

		for (host_cnt = cur_hosts; host_cnt < max_hosts;) {

			/* Wait for manager to request job info */
			if( !rcv_int(xdrs,&op,TRUE) ) {
				dprintf( D_ALWAYS, "Can't receive request from manager\n" );
				RETURN;
			}
			
			switch( op ) {
			    case REJECTED:
				    mark_cluster_rejected( cur_cluster );
					host_cnt = max_hosts + 1;
					break;
				case SEND_JOB_INFO:
					if( JobsStarted >= MaxJobStarts ) {
						if( !snd_int(xdrs,NO_MORE_JOBS,TRUE) ) {
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
						if( !snd_int(xdrs,NO_MORE_JOBS,TRUE) ) {
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
						if( !snd_int(xdrs,NO_MORE_JOBS,TRUE) ) {
							dprintf( D_ALWAYS, 
									"Can't send NO_MORE_JOBS to mgr\n" );
							RETURN;
						}
						dprintf( D_ALWAYS,
					"Swap Space Exhausted, %d jobs matched, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						RETURN;
					}
					if( JobsStarted >= start_limit_for_swap ) {
						if( !snd_int(xdrs,NO_MORE_JOBS,TRUE) ) {
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
					context = build_context( &id);
					if( !snd_int(xdrs,JOB_INFO,FALSE) ) {
						dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
						RETURN;
					}
					if( !snd_context(xdrs,context,TRUE) ) {
						dprintf( D_ALWAYS,
								"1.Can't send job_context to mgr\n" );
						RETURN;
					}
					free_context( context );
					context = NULL;
					dprintf( D_FULLDEBUG,
							"Sent job %d.%d\n", id.cluster, id.proc );
					cur_cluster = id.cluster;
					break;
				case PERMISSION:
					/*
					 * Weiru
					 * When we receive permission, we add a match record, fork
					 * a child called agent to claim our matched resource.
					 * Originally there is a limit on how many
					 * jobs in total can be run concurrently from this schedd,
					 * and a limit on how many jobs this schedd can start in
					 * one negotiation cycle. Now these limits are  adopted to
					 * apply to match records instead.
					 */
					if( !rcv_string(xdrs,&match,TRUE) ) {
						dprintf( D_ALWAYS,
								"Can't receive host name from mgr\n" );
						RETURN;
					}
					// match is in the form "<xxx.xxx.xxx.xxx:xxxx> xxxxxxxx#xx"
					dprintf(D_ALWAYS, "Received match %s\n", match);
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
	if( !snd_int(xdrs,NO_MORE_JOBS,TRUE) ) {
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

CONTEXT* Scheduler::build_context(PROC_ID* id)
{
	char			req_buf[1000], *requirements;
	char			pref_buf[1000], *preferences;
	char			own_buf[1000], *owner;
	CONTEXT*		job_context;
	char			line[1024];
	int				image_size;
	int				disk;

	GetAttributeExpr(id->cluster, id->proc, "Requirements", req_buf);
	requirements = strchr(req_buf, '=');
	requirements++;
	GetAttributeExpr(id->cluster, id->proc, "Preferences", pref_buf);
	preferences = strchr(pref_buf, '=');
	preferences++;
	GetAttributeString(id->cluster, id->proc, "Owner", own_buf);
	owner = own_buf;
	GetAttributeInt(id->cluster, id->proc, "Image_size", &image_size);
	/* Very bad hack! */
	disk = 10000;

	job_context = create_context();

	(void)sprintf( line,
		"JOB_REQUIREMENTS = (%s) && (Disk >= %d) && (VirtualMemory >= %d)",
		requirements, disk, image_size  );
	store_stmt( scan(line), job_context );

	if( preferences && preferences[0] ) {
		(void)sprintf( line, "JOB_PREFERENCES = %s", preferences );
	} else {
		(void)sprintf( line, "JOB_PREFERENCES = T" );
	}
	store_stmt( scan(line), job_context );

	(void)sprintf( line, "Owner = \"%s\"", owner );
	store_stmt( scan(line), job_context );

	return job_context;
}

/*
 * Weiru
 * This function does two things.
 * 1. add a match record
 * 2. fork an agent to contact the startd
 * Returns 1 if successful (not the result of agent's negotiation), 0 if failed.
 */
Scheduler::permission(char* id, char* server, PROC_ID* jobId)
{
	Mrec*		mrec;						// match record pointer
	int			pid;
	int			lim = getdtablesize();		// size of descriptor table
	int			i;

	mrec = AddMrec(id, server, jobId);
	if(!mrec)
	{
		return 0;
	}
	switch((pid = fork()))
	{
		case -1:	/* error */

            if(errno == ENOMEM)
            {
                dprintf(D_ALWAYS, "fork() failed, due to lack of swap space\n");
                swap_space_exhausted();
            }
            else
            {
                dprintf(D_ALWAYS, "fork() failed, errno = %d\n", errno);
            }
			DelMrec(mrec);
			return 0;

        case 0:     /* the child */

            close(0);
            for(i = 3; i < lim; i++)
            {
                close(i);
            }
            Agent(server, id, MySockName, aliveFrequency);

        default:    /* the parent */

            mrec->agentPid = pid;
			return 1;
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
 */
void Scheduler::StartJobs()
{
	int		i;							// iterate through match records
	PROC_ID	id;

	dprintf(D_ALWAYS, ">>>>>>>>>>>> Start jobs >>>>>>>>>>>>\n");
	for(i = 0; i < nMrec; i++)
	{
		dprintf(D_FULLDEBUG, "match (%s) ", rec[i]->id);
		if(rec[i]->status == M_INACTIVE)
		{
			dprintf(D_FULLDEBUG | D_NOHEADER, "inactive\n");
			continue;
		}
		if(rec[i]->shadowRec)
		{
			dprintf(D_FULLDEBUG | D_NOHEADER, "already running a job\n");
			continue;
		}

		// This is the case we want to try and start a job.
		id.cluster = rec[i]->cluster;
		if(rec[i]->proc >= 0)
		// we already have a candidate
		{
			id.proc = rec[i]->proc;
		}
		else
		// find the job in the cluster with the highest priority
		{
			dprintf(D_FULLDEBUG | D_NOHEADER, "\n");
			FindRunnableJob(rec[i]->cluster, id.proc);
		}
		if(id.proc < 0)
		// no more jobs to run
		{
			dprintf(D_FULLDEBUG | D_NOHEADER, "out of jobs\n");
			Relinquish(rec[i]);
			DelMrec(rec[i]);
			i--;
			continue;
		}
		dprintf(D_FULLDEBUG | D_NOHEADER, "running %d.%d\n", id.cluster, id.proc);
		if(!(rec[i]->shadowRec = StartJob(rec[i], &id)))
		// Start job failed. Throw away the match. The reason being that we
		// don't want to keep a match around and pay for it if it's not
		// functioning and we don't know why. We might as well get another
		// match.
		{
			dprintf(D_FULLDEBUG, "Starting job failed, relinquish resource\n");
			Relinquish(rec[i]);
			DelMrec(rec[i]);
			i--;
			continue;
		}
	}
	dprintf(D_ALWAYS, ">>>>>>>>>>>> Done starting jobs >>>>>>>>>>>>\n");
}





shadow_rec* Scheduler::StartJob(Mrec* mrec, PROC_ID* job_id)
{
	int		universe;
	int		rval;

	rval = GetAttributeInt(job_id->cluster, job_id->proc, "Universe", 
						   &universe);
	if (universe == PVM) {
		return start_pvm(mrec, job_id);
	} else {
		if (rval < 0) {
			dprintf(D_ALWAYS, 
		"Couldn't find Universe Attribute for job (%d.%d) assuming stanard.\n",
					job_id->cluster, job_id->proc);
		}
		return start_std(mrec, job_id);
	}
	return NULL;
}

shadow_rec* Scheduler::start_std(Mrec* mrec , PROC_ID* job_id)
{
	char	*argv[6];
	char	cluster[10], proc[10];
	int		pid;
	int		i, lim;
	int		parent_id;

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s\n",
			job_id->cluster, job_id->proc, mrec->peer);


	(void)sprintf( cluster, "%d", job_id->cluster );
	(void)sprintf( proc, "%d", job_id->proc );
	argv[0] = "condor_shadow";
	argv[1] = mrec->peer;
	argv[2] = mrec->id;
	argv[3] = cluster;
	argv[4] = proc;
	argv[5] = 0;

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
			for( i=3; i<lim; i++ ) {
				(void)close( i );
			}
#ifdef NFSFIX
	/* Must be condor to write to log files. */
			if (getuid()==0) {
				set_root_euid(__FILE__,__LINE__); 
			}

#endif NFSFIX
			Shadow = param("SHADOW");
			(void)execve( Shadow, argv, environ );
			if (errno == ENOMEM) {
				kill( parent_id, SIGUSR1 );
			}
			exit( JOB_NO_MEM );
			break;
		default:	/* the parent */
			dprintf( D_ALWAYS, "Running %d.%d on \"%s\", (shadow pid = %d)\n",
				job_id->cluster, job_id->proc, mrec->peer, pid );
			return add_shadow_rec( pid, job_id, mrec, -1 );
	}
	return NULL;
}


shadow_rec* Scheduler::start_pvm(Mrec* mrec, PROC_ID *job_id)
{
	char			*argv[8];
	char			cluster[10], proc_str[10];
	int				pid;
	int				i, lim;
	int				parent_id;
	int				ClientUid;
	int				shadow_fd;
	int				new_shadow;
	char			class_name[50];
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
				argv[1] = "-d";
				argv[2] = MySockName;
				argv[3] = 0;
				
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
#if 0 /* def NFSFIX */
				/* Must be condor to write to log files. */
				set_root_euid(__FILE__,__LINE__);
#endif
				(void)execve( Shadow, argv, environ );
				dprintf( D_ALWAYS, "exec failed\n");
				kill( parent_id, SIGUSR1 );
				exit( JOB_NO_MEM );
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
	sprintf(out_buf, "%s %d\n", mrec->peer, job_id->proc);
	dprintf( D_ALWAYS, "sending %s", out_buf);
	write(shadow_fd, out_buf, strlen(out_buf));
	return srp;
}


void Scheduler::display_shadow_recs()
{
	int		i;
	struct shadow_rec *r;

	dprintf( D_FULLDEBUG, "\n");
	dprintf( D_FULLDEBUG, "..................\n" );
	dprintf( D_FULLDEBUG, ".. Shadow Recs (%d)\n", NShadowRecs );
	for( i=0; i<NShadowRecs; i++ ) {
		r = &ShadowRecs[i];
		dprintf(
			D_FULLDEBUG,
			".. %d: %d, %d.%d, %s, %s\n",
			i,
			r->pid,
			r->job_id.cluster,
			r->job_id.proc,
			r->preempted ? "T" : "F" ,
			r->match->peer
		);
	}
	dprintf( D_FULLDEBUG, "..................\n\n" );
}

struct shadow_rec *add_shadow_rec( int pid, PROC_ID* job_id, Mrec* mrec, int fd )
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
	return &(ShadowRecs[NShadowRecs - 1]);
}

void Scheduler::delete_shadow_rec(int pid)
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


void Scheduler::mark_job_running(PROC_ID* job_id)
{
#if DBM_QUEUE
#if defined(NEW_PROC)
	GENERIC_PROC	buf;
	PROC			*proc  = (PROC *)&buf;
#else
	char			buf[1024];
	PROC			*proc  = (PROC *)buf;
#endif
#else
#endif
	int status;
	int orig_max;

	GetAttributeInt(job_id->cluster, job_id->proc, "Status", &status);
	GetAttributeInt(job_id->cluster, job_id->proc, "MaxHosts", &orig_max);
	SetAttributeInt(job_id->cluster, job_id->proc, "OrigMaxHosts", orig_max);

	if( status == RUNNING ) {
		EXCEPT( "Trying to run job %d.%d, but already marked RUNNING!",
			job_id->cluster, job_id->proc );
	}

	status = RUNNING;

	SetAttributeInt(job_id->cluster, job_id->proc, "Status", status);

	/*
	** dprintf( D_FULLDEBUG, "Marked job %d.%d as RUNNING\n",
	**								proc->id.cluster, proc->id.proc );
	*/
}

/*
** Mark a job as stopped, (Idle or Unexpanded).  Note: this routine assumes
** that the DBM pointer (q) is already locked for writing.
*/
void Scheduler::mark_job_stopped(PROC_ID* job_id)
{
	int		status;
	int		orig_max;
	int		had_orig;
	char	ckpt_name[MAXPATHLEN];

	GetAttributeInt(job_id->cluster, job_id->proc, "Status", &status);
	had_orig = GetAttributeInt(job_id->cluster, job_id->proc, 
							   "OrigMaxHosts", &orig_max);

	if( status != RUNNING ) {
		EXCEPT( "Trying to stop job %d.%d, but not marked RUNNING!",
			job_id->cluster, job_id->proc );
	}
#if 0
	(void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
								Spool, job_id->cluster, job_id->proc  );
#else
	strcpy(ckpt_name, gen_ckpt_name(Spool,job_id->cluster,job_id->proc,0) );
#endif
	if( access(ckpt_name,F_OK) != 0 ) {
		status = UNEXPANDED;
	} else {
		status = IDLE;
	}

	SetAttributeInt(job_id->cluster, job_id->proc, "Status", status);
	SetAttributeInt(job_id->cluster, job_id->proc, "CurrentHosts", 0);
	if (had_orig >= 0) {
		SetAttributeInt(job_id->cluster, job_id->proc, "MaxHosts", orig_max);
	}
	
	dprintf( D_FULLDEBUG, "Marked job %d.%d as %s\n", job_id->cluster,
			job_id->proc, status==IDLE?"IDLE":"UNEXPANDED" );
	
}


void Scheduler::mark_cluster_rejected(int cluster)
{
	int		i;

	for( i=0; i<N_RejectedClusters; i++ ) {
		if( RejectedClusters[i] == cluster ) {
			return;
		}
	}
	RejectedClusters[ N_RejectedClusters++ ] = cluster;
}

int Scheduler::cluster_rejected(int cluster)
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
int Scheduler::is_alive(int pid)
{
#if defined(ULTRIX43)
    int stat;				/* on ULTRIX, the schedd needs to seteuid */

	if (getuid()==0) {
		set_root_euid();      /* to root , otherwise it is unable to    */
	}

    stat = kill(pid,0);     /* send a signal to the shadow  : dhruba  */
    set_condor_euid();
    if ( stat == 0 ) return 1;
    else return 0;
#else
	return( kill(pid,0) == 0 );
#endif 
}

void Scheduler::clean_shadow_recs()
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

void Scheduler::preempt(int n)
{
	int		i;

	dprintf( D_ALWAYS, "Called preempt( %d )\n", n );
	for( i=NShadowRecs-1; n > 0 && i >= 0; n--, i-- ) {
		if( is_alive(ShadowRecs[i].pid) ) {
			if( ShadowRecs[i].preempted ) {
				send_vacate( ShadowRecs[i].match, KILL_FRGN_JOB );
			} else {
				send_vacate( ShadowRecs[i].match, CKPT_FRGN_JOB );
				ShadowRecs[i].preempted = TRUE;
			}
		} else {
			dprintf( D_ALWAYS,
			"Have ShadowRec for pid %d, but is dead!\n", ShadowRecs[i].pid );
			dprintf( D_ALWAYS, "Deleting now...\n" );
			delete_shadow_rec( ShadowRecs[i].pid );
		}
	}
}

#define RETURN \
	if( xdrs ) { \
		xdr_destroy( xdrs ); \
	} \
	if( sock >= 0 ) { \
		close( sock ); \
	}
void send_vacate(Mrec* match,int cmd)
{
	int		sock = -1;
	XDR		xdr, *xdrs = NULL;

	dprintf( D_ALWAYS, "Called send_vacate( %s, %d )\n", match->peer, cmd );
        /* Connect to the specified host */
    if( (sock = do_connect(match->peer, "condor_startd", START_PORT)) < 0 ) {
        dprintf( D_ALWAYS, "Can't connect to startd on %s\n", match->peer);
		RETURN;
    }
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;

    if( !xdr_int(xdrs, &cmd) ) {
        dprintf( D_ALWAYS, "Can't initialize xdr sock to %s\n", match->peer);
		RETURN;
	}

    if( !snd_string(xdrs, match->id, FALSE) ) {
        dprintf( D_ALWAYS, "Can't initialize xdr sock to %s\n", match->peer);
		RETURN;
	}

    if( !xdrrec_endofrecord(xdrs,TRUE) ) {
        dprintf( D_ALWAYS, "Can't send xdr EOF to %s\n", match->peer);
		RETURN;
	}

	if( cmd == CKPT_FRGN_JOB ) {
		dprintf( D_ALWAYS, "Sent CKPT_FRGN_JOB to startd on %s\n", match->peer);
	} else {
		dprintf( D_ALWAYS, "Sent KILL_FRGN_JOB to startd on %s\n", match->peer);
	}
	RETURN;

}
#undef RETURN

void Scheduler::swap_space_exhausted()
{
	int		old_estimate = ShadowSizeEstimate;

	SwapSpaceExhausted = TRUE;

	ShadowSizeEstimate = (SwapSpace - ReservedSwap) / JobsStarted;
	if( ShadowSizeEstimate <= old_estimate ) {
		ShadowSizeEstimate = old_estimate + SHADOW_SIZE_INCR;
	}

	dprintf( D_FULLDEBUG, "*** Revising Shadow Size Esitimate ***\n" );
	dprintf( D_FULLDEBUG, "*** SwapSpace was %d, JobsStarted was %d\n",
													SwapSpace, JobsStarted );
	dprintf( D_FULLDEBUG, "*** New ShadowSizeEstimate: %dK\n",
													ShadowSizeEstimate );

}

/*
  We maintain two tables which should be consistent, return TRUE if they
  are, and FALSE otherwise.  The tables are the ShadowRecs, a list
  of currently running jobs, and PrioRec a list of currently runnable
  jobs.  We will say they are consistent if none of the currently
  runnable jobs are already listed as running jobs.
*/
int Scheduler::shadow_prio_recs_consistent()
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
			GetAttributeInt(BadCluster, BadProc, "Status", &status);
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
struct shadow_rec* find_shadow_rec(PROC_ID* id)
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

void Scheduler::mail_problem_message()
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

/*
** Allow child processes to die a decent death, don't keep them
** hanging around as <defunct>.
**
** NOTE: This signal handler calls routines which will attempt to lock
** the job queue.  Be very careful it is not called when the lock is
** already held, or deadlock will occur!
*/
void Scheduler::reaper(int sig, int code, struct sigcontext* scp)
{
    pid_t   pid;
    int     status;
	Mrec*	mrec;

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
		if((mrec = FindMrecByPid(pid)))
		{
			if(WIFEXITED(status))
            {
                dprintf(D_FULLDEBUG, "agent pid %d exited with status %d\n",
                        pid, WEXITSTATUS(status) );
                if(WEXITSTATUS(status) == EXITSTATUS_NOTOK)
                {
                    dprintf(D_ALWAYS, "capability rejected by startd\n");
                    DelMrec(mrec);
                }
                else
                {
                    dprintf(D_ALWAYS, "Agent contacting startd successful\n");
					mrec->status = M_ACTIVE;
                }
            }
            else if(WIFSIGNALED(status))
            {
                dprintf(D_ALWAYS, "Agent pid %d died with signal %d\n",
                        pid, WTERMSIG(status));
                DelMrec(mrec);
            }
		}
        else
        {
            if( WIFEXITED(status) ) {
                dprintf( D_FULLDEBUG, "Shadow pid %d exited with status %d\n",
                                                pid, WEXITSTATUS(status) );
                if( WEXITSTATUS(status) == JOB_NO_MEM ) {
                    swap_space_exhausted();
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
	unblock_signal(SIGALRM);
}

void Scheduler::kill_zombie(int pid, PROC_ID* job_id )
{
    char    ckpt_name[MAXPATHLEN];

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
void Scheduler::check_zombie(int pid, PROC_ID* job_id)
{
    char    queue[MAXPATHLEN];

#if DBM_QUEUE
    PROC    proc;
#endif
    int     status;

    dprintf( D_ALWAYS, "Entered check_zombie( %d, 0x%x )\n", pid, job_id );

    if (GetAttributeInt(job_id->cluster, job_id->proc, "Status", &status) < 0){
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

#if DBM_QUEUE
    CLOSE_JOB_QUEUE( Q );
#endif
    dprintf( D_ALWAYS, "Exited check_zombie( %d, 0x%x )\n", pid, job_id );
}

void Scheduler::cleanup_ckpt_files(int pid, PROC_ID* job_id)
{
    char    ckpt_name[MAXPATHLEN];

        /* Remove any checkpoint file */
#if defined(V2)
    (void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
                                Spool, job_id->cluster, job_id->proc  );
#else
    strcpy(ckpt_name, gen_ckpt_name(Spool,job_id->cluster,job_id->proc,0) );
#endif
    (void)unlink( ckpt_name );

        /* Remove any temporary checkpoint file */
    (void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d.tmp",
                                Spool, job_id->cluster, job_id->proc  );
    (void)unlink( ckpt_name );
}

void Scheduler::SetClassAd(ClassAd* a)
{
	if(ad)
	{
		delete ad;
	}
	ad = a;
}

// initialize the configuration parameters
void Scheduler::Init()
{
	char*					tmp;
	struct sockaddr_in		addr;
	struct hostent*			host = gethostent();
	char					tmpExpr[200];

	// set the name of the schedd
	memset((char*)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SCHED_PORT);
	get_inet_address(&(addr.sin_addr));
	MySockName = sin_to_string(&addr);
	dprintf(D_FULLDEBUG, "MySockName = %s\n", MySockName);
	if (MySockName !=0){
		sprintf(tmpExpr, "SCHEDD_IP_ADDR = %s", MySockName);
		ad->Insert(tmpExpr);
		ELEM	tmp;

		tmp.type = STRING;
		tmp.val.string_val = MySockName;
		store_stmt(build_expr("SCHEDD_IP_ADDR", &tmp), MachineContext);
	}

	CollectorHost = param( "COLLECTOR_HOST" );
    if( CollectorHost == NULL ) {
        EXCEPT( "No Collector host specified in config file\n" );
    }
	UdpSock = udp_unconnect();
	fill_dgram_io_handle(&CollectorHandle, CollectorHost, UdpSock, COLLECTOR_UDP_PORT);

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

	/* I don't think this is necessary. dprintf_config uses D_ALWAYS by default
	 * Weiru
    if( (tmp=param("SCHEDD_DEBUG")) == NULL ) {
        EXCEPT( "\"SCHEDD_DEBUG\" not specified" );
    }
    free( tmp );
	*/

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
        ShadowSizeEstimate =  128;          /* 128 K bytes */
    } else {
        ShadowSizeEstimate = atoi( tmp );   /* Value specified in kilobytes */
        free( tmp );
    }

    if( (CondorAdministrator = param("CONDOR_ADMIN")) == NULL ) {
        EXCEPT( "CONDOR_ADMIN not specified in config file" );
    }

    /* parameters for UPDOWN alogorithm */
    tmp = param( "SCHEDD_SCALING_FACTOR" );
    if( tmp == NULL ) ScheddScalingFactor = 60;
     else ScheddScalingFactor = atoi( tmp );
    if ( ScheddScalingFactor == 0 )
          ScheddScalingFactor = 60;

    tmp = param( "SCHEDD_HEAVY_USER_TIME" );
    if( tmp == NULL )  ScheddHeavyUserTime = 120;
     else ScheddHeavyUserTime = atoi( tmp );

    Step                    =  SchedDInterval/ScheddScalingFactor;
    ScheddHeavyUserPriority = Step * ScheddHeavyUserTime;

    upDown_SetParameters(Step,ScheddHeavyUserPriority); /* UPDOWN */

    /* update upDown objct from disk file */
    /* ignore errors              */
	filename = new char[MAXPATHLEN];
    sprintf(filename,"%s/%s",Spool, "UserPrio");
    upDown_ReadFromFile(filename);                /* UPDOWN */

    if( (Mail=param("MAIL")) == NULL ) {
        EXCEPT( "MAIL not specified in config file\n" );
    }

	tmp = param("ALIVE_FREQUENCY");
    if(!tmp)
    {
		aliveFrequency = SchedDInterval;
    }
    else
    {
        aliveFrequency = atoi(tmp);
        free(tmp);
	}
	if(aliveFrequency < SchedDInterval)
	{
		aliveInterval = 1;
	}
	else
	{
		aliveInterval = aliveFrequency / SchedDInterval;
	}
}

void Scheduler::Register(DaemonCore* core)
{
	core->Register(this, NEGOTIATE, (void*)negotiate, REQUEST);
	core->Register(this, RESCHEDULE, (void*)reschedule_negotiator, REQUEST);
	core->Register(this, 10, (void*)timeout, SchedDInterval, TIMER);
	core->Register(this, SIGCHLD, (void*)reaper, SIGNAL);
	core->Register(NULL, QMGMT_CMD, (void*)handle_q, REQUEST);
	core->Register(this, SIGINT, (void*)sigint_handler, SIGNAL);
	core->Register(this, SIGHUP, (void*)sighup_handler, SIGNAL);
	core->Register(this, SIGPIPE, (void*)SIG_IGN, SIGNAL);
	core->Register(this, (void*)StartJobs);
	core->Register(this, (void*)send_alive);
}

extern "C" {
int prio_compar(prio_rec* a, prio_rec* b)
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

void Scheduler::sigint_handler()
{
	dprintf( D_ALWAYS, "Killed by SIGINT\n" );
	exit(0);
}

void Scheduler::sighup_handler()
{
	ClassAd*	ad;
	CONTEXT*	MachineContext;

    dprintf( D_ALWAYS, "Re reading config file\n" );

    MachineContext = create_context();
    config( myName, MachineContext );
	ad = new ClassAd(MachineContext);
	SetClassAd(ad);

	Init();
	::Init();
    timeout();
}

void Scheduler::reschedule_negotiator(XDR*, struct sockaddr_in*)
{
    int     sock = -1;
    int     cmd;
    XDR     xdr, *xdrs = NULL;

    dprintf( D_ALWAYS, "Called reschedule_negotiator()\n" );

	timeout();							// update the central manager now

        /* Connect to the negotiator */
    if( (sock=do_connect(NegotiatorHost,"condor_negotiator",NEGOTIATOR_PORT))
                                                                    < 0 ) {
        dprintf( D_ALWAYS, "Can't connect to CONDOR negotiator\n" );
        return;
    }
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;

    cmd = RESCHEDULE;
    (void)xdr_int( xdrs, &cmd );
    (void)xdrrec_endofrecord( xdrs, TRUE );

    xdr_destroy( xdrs );
    (void)close( sock );
    return;
}

Mrec* Scheduler::AddMrec(char* id, char* peer, PROC_ID* jobId)
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

int Scheduler::DelMrec(char* id)
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
			delete rec[i];
			nMrec--; 
			rec[i] = rec[nMrec];
			rec[nMrec] = NULL;
		}
	}
}

int Scheduler::DelMrec(Mrec* match)
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
}

int Scheduler::MarkDel(char* id)
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
}

void Scheduler::Agent(char* server, char* capability, char* name, int aliveFrequency)
{
    int     sock;                               /* socket to startd */
    XDR     xdr, *xdrs;                         /* stream to startd */
    int     attempt;                            /* # of attempts */
    int     reply;                              /* reply from the startd */
    int     flag = TRUE;

    for(attempt = 1; attempt <= 3; attempt++)
    {
        flag = TRUE;
        if((sock = do_connect(server, "condor_startd", 0)) < 0)
        {
            flag = FALSE;
            xdr_destroy(xdrs);
            close(sock);
            sleep(1);
            continue;
        }
        xdrs = xdr_Init(&sock, &xdr);
        if(!snd_int(xdrs, REQUEST_SERVICE, FALSE))
        {
            flag = FALSE;
            xdr_destroy(xdrs);
            close(sock);
            sleep(1);
            continue;
        }
        if(!snd_string(xdrs, capability, TRUE))
        {
            flag = FALSE;
            xdr_destroy(xdrs);
            close(sock);
            sleep(1);
            continue;
        }
        if(!rcv_int(xdrs, &reply, TRUE))
        {
            flag = FALSE;
            xdr_destroy(xdrs);
            close(sock);
            sleep(1);
            continue;
        }
        if(reply == ACCEPTED)
        {
            if(!snd_string(xdrs, name, FALSE))
            {
                flag = FALSE;
                xdr_destroy(xdrs);
                close(sock);
                sleep(1);
                continue;
            }
            if(!snd_int(xdrs, aliveFrequency, TRUE))
            {
                flag = FALSE;
                xdr_destroy(xdrs);
                close(sock);
                sleep(1);
                continue;
            }
            dprintf(D_ALWAYS, "alive frequency %d secs\n", aliveFrequency);
            xdr_destroy(xdrs);
            close(sock);
            break;
        }
		else
		{
			flag = FALSE;
		}
        xdr_destroy(xdrs);
        close(sock);
    }
    if(flag == TRUE)
    {
        exit(EXITSTATUS_OK);
    }
    else
    {
        exit(EXITSTATUS_NOTOK);
    }
}

Mrec* Scheduler::FindMrecByPid(int pid)
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

/*
 * Weiru
 * Inform the startd and the accountant of the relinquish of the resource.
 */
void Scheduler::Relinquish(Mrec* mrec)
{
	int     sock;
    XDR     xdr, *xdrs;
	int		flag = FALSE;

	// inform the startd
    if((sock = do_connect(mrec->peer, "condor_startd", 0)) < 0)
    {
        dprintf(D_ALWAYS, "Can't relinquish startd. Match record is:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
    }
	else
	{
		xdrs = xdr_Init(&sock, &xdr);

		if(!snd_int(xdrs, RELINQUISH_SERVICE, FALSE))
		{
			dprintf(D_ALWAYS, "Can't relinquish startd. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
			xdr_destroy(xdrs);
			close(sock);
		}
		else if(!snd_string(xdrs, mrec->id, TRUE))
		{
			dprintf(D_ALWAYS, "Can't relinquish startd. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
			xdr_destroy(xdrs);
			close(sock);
		}
		else
		{
			xdr_destroy(xdrs);
			close(sock);
			dprintf(D_FULLDEBUG, "Relinquished startd. Match record is:\n");
			dprintf(D_FULLDEBUG, "%s\t%s\n", mrec->id,	mrec->peer);
			flag = TRUE;
		}
	}

	// inform the accountant
	if(!AccountantName)
	{
		dprintf(D_FULLDEBUG, "No accountant to relinquish\n");
	}
	else if((sock = do_connect(AccountantName, "condor_accountant", 0)) < 0)
    {
        dprintf(D_ALWAYS, "Can't relinquish accountant. Match record is:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
    }
	else
	{
		xdrs = xdr_Init(&sock, &xdr);

		if(!snd_int(xdrs, RELINQUISH_SERVICE, FALSE))
		{
			dprintf(D_ALWAYS, "Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
			xdr_destroy(xdrs);
			close(sock);
		}
		else if(!snd_string(xdrs, MySockName, FALSE))
		{
			dprintf(D_ALWAYS, "Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
			xdr_destroy(xdrs);
			close(sock);
		}
		else if(!snd_string(xdrs, mrec->id, FALSE))
		{
			dprintf(D_ALWAYS, "Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
			xdr_destroy(xdrs);
			close(sock);
		}
		else if(!snd_string(xdrs, mrec->peer, TRUE))
		// This is not necessary to send except for being an extra checking
		// because capability uniquely identifies a match.
		{
			dprintf(D_ALWAYS, "Can't relinquish accountant. Match record is:\n");
			dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
			xdr_destroy(xdrs);
			close(sock);
		}
		else
		{
			xdr_destroy(xdrs);
			close(sock);
			dprintf(D_FULLDEBUG, "Relinquished accountant. Match record is:\n");
			dprintf(D_FULLDEBUG, "%s\t%s\n", mrec->id,	mrec->peer);
			flag = TRUE;
		}
	}
	if(flag)
	{
		dprintf(D_ALWAYS, "Relinquished match:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
	}
	else
	{
		dprintf(D_ALWAYS, "Couldn't relinquish match:\n");
		dprintf(D_ALWAYS, "%s\t%s\n", mrec->id,	mrec->peer);
	}
}

void Scheduler::RemoveShadowRecFromMrec(shadow_rec* shadow)
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

int Scheduler::AlreadyMatched(PROC_ID* id)
{
	int			i;

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
void Scheduler::send_alive()
{
    int     sock;
    XDR     xdr, *xdrs;
    int     i;

	if(aliveFrequency == 0)
	/* no need to send alive message */
	{
		return;
	}
    dprintf(D_FULLDEBUG, "send alive messages to startds\n");
    for(i = 0; i < nMrec; i++)
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
            if((sock = do_connect(rec[i]->peer, "condor_startd", 0)) < 0)
            /* can't connect to startd */
            {
                dprintf(D_FULLDEBUG, "\tCan't connect to %s\n",
                        rec[i]->peer);
                continue;
            }
            xdrs = xdr_Init(&sock, &xdr);
            if(!snd_int(xdrs, ALIVE, TRUE))
            {
                dprintf(D_FULLDEBUG, "\tCan't send alive message to %d\n",
                        rec[i]->peer);
                xdr_destroy(xdrs);
                close(sock);
                continue;
            }
            dprintf(D_FULLDEBUG, "\tSent alive message to %s\n",
                    rec[i]->peer);
            xdr_destroy(xdrs);
            close(sock);
        }
    }
    dprintf(D_FULLDEBUG, "done sending alive messages to startds\n");
}

void job_prio(int cluster, int proc)
{
	sched->JobsRunning += get_job_prio(cluster, proc);
}
