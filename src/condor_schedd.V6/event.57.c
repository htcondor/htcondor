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
#include "condor_xdr.h"
#include "sched.h"
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "expr.h"
#include "proc.h"
#include "exit.h"
#include "dgram_io_handle.h"
#include "prio_rec.h"

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
extern char	*Spool;
extern char	*Shadow;
extern char	**environ;
extern char *gen_ckpt_name();
extern char *Mail;

CONTEXT		*create_context(), *build_context();
bool_t 		xdr_int();
XDR			*xdr_Udp_Init(), *xdr_Init();
ELEM		*create_elem();
EXPR		*create_expr();
char		*strdup();
int			prio_compar();
EXPR		*build_expr(), *scan();

extern		char	filename[];/* store UpDown Object */ /*UPDOWN*/
extern		time_t  LastTimeout; /* time when timeout was called */
extern int Step, ScheddHeavyUserPriority; /* for UPDOWN algorithm */
extern int 	ScheddScalingFactor;

extern CONTEXT	*MachineContext;
extern char		*MgrHost;
extern int		UdpSock;
extern DGRAM_IO_HANDLE  CollectorHandle;
extern int		MaxJobStarts;
extern int		MaxJobsRunning;
extern int		JobsRunning;
extern char*    MySockName1; /* dhaval */

#if DBM_QUEUE
DBM		*Q, *OpenJobQueue();
#else
#include "condor_qmgr.h"
#endif

int		JobsIdle;
char	*Owners[1024];
int		N_Owners;
struct sockaddr_in	From;
int		Len;

extern 	prio_rec		PrioRec[];
extern	int				N_PrioRecs;

#define MAX_SHADOW_RECS 512
struct shadow_rec {
	int			pid;
	PROC_ID		job_id;
	char		*host;
	int			preempted;
	int			conn_fd;
} ShadowRecs[ MAX_SHADOW_RECS ];
int	NShadowRecs;

#if defined(__STDC__)
struct shadow_rec *find_shadow_rec( PROC_ID * );
struct shadow_rec *add_shadow_rec( int, PROC_ID *, char *, int );
#else
struct shadow_rec *find_shadow_rec();
#endif

int		RejectedClusters[ 1024 ];
int		N_RejectedClusters;

int	JobsStarted;		/* Number of jobs started last negotiating session */
int	SwapSpace;			/* Swap space available at beginning of last session */
int	ShadowSizeEstimate;	/* Estimate of swap needed to run a single job */
int	SwapSpaceExhausted;	/* True if a job had died due to lack of swap space */
int	ReservedSwap;		/* Try to keep this much swap for non-condor users */

extern char *param( char * );

timeout()
{
	ELEM tmp;
	/* the step size in the UPDOWN algo depends on the time */
	/* interval of sampling			                */
	Step = ( time((time_t*)0) - LastTimeout) /ScheddScalingFactor;
	upDown_SetParameters(Step, ScheddHeavyUserPriority);

	upDown_ClearUserInfo();	/* UPDOWN */
	count_jobs();
	upDown_UpdatePriority(); /* UPDOWN */
	upDown_Display();        /* UPDOWN */
	upDown_WriteToFile(filename);    /* UPDOWN */
	if (MySockName1!=0){  /* dhaval */
		tmp.type = STRING;
		tmp.s_val = MySockName1;
		store_stmt( build_expr("SCHEDD_IP_ADDR", &tmp), MachineContext);
	}

	update_central_mgr();

		/* Neither of these should be needed! */
	reaper( 0, 0, 0 );
	clean_shadow_recs();

	if( NShadowRecs > MaxJobsRunning ) {
		preempt( NShadowRecs - MaxJobsRunning );
	}
}

/*
** Examine the job queue to determine how many CONDOR jobs we currently have
** running, and how many individual users own them.
*/
count_jobs()
{
	char	queue[MAXPATHLEN];
	int		i;
	int		count();
	ELEM	tmp;
	int		prio_compar();

	N_Owners = 0;
	JobsRunning = 0;
	JobsIdle = 0;

#if DBM_QUEUE
	(void)sprintf( queue, "%s/job_queue", Spool );
	if( (Q=OpenJobQueue(queue,O_RDONLY,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue );
	}

	LOCK_JOB_QUEUE( Q, READER );

	ScanJobQueue( Q, count );

	CLOSE_JOB_QUEUE( Q );
#else
	WalkJobQueue( count );
#endif

	dprintf( D_FULLDEBUG, "JobsRunning = %d\n", JobsRunning );
	dprintf( D_FULLDEBUG, "JobsIdle = %d\n", JobsIdle );
	dprintf( D_FULLDEBUG, "N_Owners = %d\n", N_Owners );
	dprintf( D_FULLDEBUG, "MaxJobsRunning = %d\n", MaxJobsRunning );

	for ( i=0; i<N_Owners; i++) {
		FREE( Owners[i] );
	}

	tmp.type = INT;
	tmp.i_val = JobsRunning;
	store_stmt( build_expr("Running",&tmp), MachineContext );

	tmp.i_val = JobsIdle;
	store_stmt( build_expr("Idle",&tmp), MachineContext );

	tmp.i_val = N_Owners;
	store_stmt( build_expr("Users",&tmp), MachineContext );

	tmp.i_val = MaxJobsRunning;
	store_stmt( build_expr("MAX_JOBS_RUNNING",&tmp), MachineContext );

}

prio_compar( a, b )
prio_rec		*a;
prio_rec		*b;
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

#if DBM_QUEUE
count( proc )
PROC	*proc;
#else
count( cluster, proc )
int cluster;
int proc;
#endif
{
	int status;
	char buf[100];
	char *owner;
	int		cur_hosts;
	int		max_hosts;

#if DBM_QUEUE
	status = proc->status;
	owner = proc->owner;
	if( status == RUNNING ) {
		cur_hosts = 1;
	} else {
		cur_hosts = 0;
	}

	if( status == IDLE || status == UNEXPANDED ) {
		max_hosts = 1;
	}

#else
	GetAttributeInt(cluster, proc, "Status", &status);
	GetAttributeString(cluster, proc, "Owner", buf);
	owner = buf;
	if (GetAttributeInt(cluster, proc, "CurrentHosts", &cur_hosts) < 0) {
		cur_hosts = ((status == RUNNING) ? 1 : 0);
	}
	if (GetAttributeInt(cluster, proc, "MaxHosts", &max_hosts) < 0) {
		max_hosts = ((status == IDLE || status == UNEXPANDED) ? 1 : 0);
	}
#endif

	JobsRunning += cur_hosts;
	JobsIdle += (max_hosts - cur_hosts);

	insert_owner( owner );

#define UpDownRunning	1 /* these shud be same as up_down.h */
#define UpDownIdle	2
#define	Error		-1

	if ( status == RUNNING ) {	/* UPDOWN */
		upDown_UpdateUserInfo(owner,UpDownRunning);
	} else if ( (status == IDLE)||(status == UNEXPANDED)) {
		upDown_UpdateUserInfo(owner,UpDownIdle);
	}
}


#if DBM_QUEUE
job_prio( proc )
PROC	*proc;
#else
job_prio( cluster, proc )
int cluster;
int proc;
#endif
{
	int	prio;
	int	job_prio;
	int	status;
	int	job_status;
	struct shadow_rec *srp;
	PROC_ID	id;
	int		q_date;
	char	buf[100], *owner;
	int		cur_hosts;
	int		max_hosts;

#if DBM_QUEUE
	job_status = proc->status;
	job_prio = proc->prio;
	id = proc->id;
	q_date = proc->q_date;
	owner = proc->owner;
	if( status == RUNNING ) {
		cur_hosts = 1;
	} else {
		cur_hosts = 0;
	}

	if( status == IDLE || status == UNEXPANDED ) {
		max_hosts = 1;
	}
#else
	GetAttributeInt(cluster, proc, "Status", &job_status);
	GetAttributeInt(cluster, proc, "Prio", &job_prio);
	GetAttributeInt(cluster, proc, "Q_date", &q_date);
	GetAttributeString(cluster, proc, "Owner", buf);
	owner = buf;
	id.cluster = cluster;
	id.proc = proc;
	if (GetAttributeInt(cluster, proc, "CurrentHosts", &cur_hosts) < 0) {
		cur_hosts = ((job_status == RUNNING) ? 1 : 0);
	}
	if (GetAttributeInt(cluster, proc, "MaxHosts", &max_hosts) < 0) {
		max_hosts = ((job_status == IDLE || job_status == UNEXPANDED) ? 1 : 0);
	}
#endif

	JobsRunning += cur_hosts;
	if (cur_hosts >= max_hosts) {
		return;
	}

	/* No longer do this since a shadow can be running, and still want more
	   machines. */
#if 0
	if( (srp=find_shadow_rec(&id)) ) {
		dprintf( D_ALWAYS,
			"Job %d.%d marked UNEXPANDED or IDLE, but shadow %d not exited\n",
			id.cluster, id.proc, srp->pid
		);
		return;
	}
#endif

	prio = upDown_GetUserPriority(owner,&status); /* UPDOWN */
	if ( status  == Error )
	{
		dprintf(D_UPDOWN,"GetUserPriority returned error\n");
		dprintf(D_UPDOWN,"ERROR : ERROR \n");
/* 		EXCEPT("Can't evaluate \"PRIO\""); */
		dprintf( D_ALWAYS, 
				"job_prio: Can't find user priority for %s, assuming 0\n",
				owner );
		prio = 0;
	}

	PrioRec[N_PrioRecs].id 		= id;
	PrioRec[N_PrioRecs].prio 	= prio;
	PrioRec[N_PrioRecs].job_prio 	= job_prio;
	PrioRec[N_PrioRecs].status	= job_status;
	PrioRec[N_PrioRecs].qdate	= q_date;
	if ( DebugFlags & D_UPDOWN )
	{
		PrioRec[N_PrioRecs].owner= (char *)malloc(strlen(owner) + 1 );
		strcpy( PrioRec[N_PrioRecs].owner, owner);
	}
	N_PrioRecs += 1;
}

insert_owner( owner )
char	*owner;
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

update_central_mgr()
{
	int		cmd = SCHEDD_INFO;
	
	send_context_to_machine(&CollectorHandle, cmd, MachineContext);
}


abort_job( xdrs )
XDR		*xdrs;
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

			host = ShadowRecs[i].host;
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
	if( q ) { \
		/* DBM_QUEUE DOESN'T NEED CLOSE_JOB_QUEUE( q ); */ \
	} \
	return

/*
** The negotiator wants to give us permission to run a job on some
** server.  We must negotiate to try and match one of our jobs with a
** server which is capable of running it.  NOTE: We must keep job queue
** locked during this operation.
*/
negotiate( xdrs )
XDR		*xdrs;
{
	char	queue[MAXPATHLEN];
	int		i;
	int		op;
	CONTEXT	*context = NULL;
	PROC_ID	id;
	char	*host = NULL;
	DBM		*q = NULL;
	int		jobs;
	int		cur_cluster = -1;
	int		start_limit_for_swap;
	int		cur_hosts;
	int		max_hosts;
	int		host_cnt;
	int		perm_rval;

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered negotiate\n" );

/*	SwapSpace = calc_virt_memory(); */
	SwapSpace = 10000;

	N_PrioRecs = 0;
	JobsRunning = 0;

#if DBM_QUEUE
		/* Open and lock the job queue */
	(void)sprintf( queue, "%s/job_queue", Spool );
	if( (q=OpenJobQueue(queue,O_RDWR,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue );
	}

	LOCK_JOB_QUEUE( q, WRITER );

		/* Prioritize the jobs */
	ScanJobQueue( q, job_prio );
#else
	WalkJobQueue( job_prio );
#endif

	if( !shadow_prio_recs_consistent() ) {
		mail_problem_message();
	}

	qsort( (char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]), prio_compar );
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
					"Reached MAX_JOB_STARTS - %d jobs started, %d jobs idle\n",
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
				"Reached MAX_JOBS_RUNNING, %d jobs started, %d jobs idle\n",
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
					"Swap Space Exhausted, %d jobs started, %d jobs idle\n",
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
				"Swap Space Estimate Reached, %d jobs started, %d jobs idle\n",
								JobsStarted, jobs - JobsStarted );
						RETURN;
					}
					
					
					
					/* Send a job description */
					context = build_context( &id, q );
					display_context( context );
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
					if( !rcv_string(xdrs,&host,TRUE) ) {
						dprintf( D_ALWAYS,
								"Can't receive host name from mgr\n" );
						RETURN;
					}
					perm_rval = permission( host, &id, q );
					FREE( host );
					host = NULL;
					JobsStarted += perm_rval;
					JobsRunning += perm_rval;
					host_cnt++;
					break;
				case END_NEGOTIATE:
					dprintf( D_ALWAYS, "Lost priority - %d jobs started\n",
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
		"Out of servers - %d jobs started, %d jobs idle\n",
							JobsStarted, jobs - JobsStarted );
	} else {
		dprintf( D_ALWAYS,
		"Out of jobs - %d jobs started, %d jobs idle\n",
							JobsStarted, jobs - JobsStarted );
	}
	/*
	sleep( 2 );
	*/
	RETURN;

}
#undef RETURN

CONTEXT	*
build_context( id, q )
DBM		*q;
PROC_ID	*id;
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
	char			req_buf[1000], *requirements;
	char			pref_buf[1000], *preferences;
	char			own_buf[1000], *owner;
#endif
	CONTEXT	*job_context;
	char	line[1024];
	int		image_size;
	int		disk;

#if DBM_QUEUE
	proc->id = *id;
	(void)FetchProc( q, proc );
	requirements = proc->requirements;
	preferences = proc->preferences;
	owner = proc->owner;
	image_size = proc->image_size;
	disk = calc_disk_needed(proc);
#else
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
#endif

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

#if DBM_QUEUE
	xdr_free_proc( proc );
#endif
	return job_context;
}


permission( char *server, PROC_ID *job_id, DBM *q )
{
	int		universe;
	int		rval;

	rval = GetAttributeInt(job_id->cluster, job_id->proc, "Universe", 
						   &universe);
	if (universe == PVM) {
		rval = permission_pvm(server, job_id);
	} else {
		if (rval < 0) {
			dprintf(D_ALWAYS, 
		"Couldn't find Universe Attribute for job (%d.%d) assuming stanard.\n",
					job_id->cluster, job_id->proc);
		}
		rval = permission_std( server, job_id, q);
	}

	return rval;
}


permission_std( server, job_id, q )
char		*server;
PROC_ID		*job_id;
DBM			*q;
{
	char	*argv[5];
	char	cluster[10], proc[10];
	int		pid;
	int		i, lim;
	int		parent_id;

	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s\n",
										job_id->cluster, job_id->proc, server);


	(void)sprintf( cluster, "%d", job_id->cluster );
	(void)sprintf( proc, "%d", job_id->proc );
	argv[0] = "condor_shadow";
	argv[1] = server;
	argv[2] = cluster;
	argv[3] = proc;
	argv[4] = 0;

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

	mark_job_running( job_id, q );
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
			mark_job_stopped( job_id, q );
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
				job_id->cluster, job_id->proc, server, pid );
			add_shadow_rec( pid, job_id, server, -1 );
			break;
	}
	return 1;
}


permission_pvm(char *server, PROC_ID *job_id)
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
	int				rval = 1;
	char			**ptr;
	struct shadow_rec *srp;


	dprintf( D_FULLDEBUG, "Got permission to run job %d.%d on %s...\n",
			job_id->cluster, job_id->proc, server);
	
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
				mark_job_stopped( job_id, 0 );
				break;
			case 0:		/* the child */
				Shadow = param("SHADOW_PVM");
				if (Shadow == 0) {
					Shadow = param("SHADOW_CARMI");
				}
				argv[1] = "-d";
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
				srp = add_shadow_rec( pid, job_id, server, pipes[1] );
				shadow_fd = pipes[1];
				dprintf( D_ALWAYS, "shadow_fd = %d\n", shadow_fd);
				mark_job_running( job_id, 0 );
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
	sprintf(out_buf, "%s %d\n", server, job_id->proc);
	dprintf( D_ALWAYS, "sending %s", out_buf);
	write(shadow_fd, out_buf, strlen(out_buf));
	return rval;
}


display_shadow_recs()
{
	int		i;
	struct shadow_rec *r;

	dprintf( D_FULLDEBUG, "\n..................\n" );
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
			r->host
		);
	}
	dprintf( D_FULLDEBUG, "..................\n\n" );
}

struct shadow_rec *
add_shadow_rec( pid, job_id, server, fd )
int			pid;
PROC_ID		*job_id;
char		*server;
int			fd;
{
	ShadowRecs[ NShadowRecs ].pid = pid;
	ShadowRecs[ NShadowRecs ].job_id = *job_id;
	ShadowRecs[ NShadowRecs ].host = strdup( server );
	ShadowRecs[ NShadowRecs ].preempted = FALSE;
	ShadowRecs[ NShadowRecs ].conn_fd = fd;
	
	NShadowRecs++;
	dprintf( D_FULLDEBUG, "Added shadow record for PID %d, job (%d.%d)\n",
			pid, job_id->cluster, job_id->proc );
	display_shadow_recs();
	return &(ShadowRecs[NShadowRecs - 1]);
}

delete_shadow_rec( pid )
int		pid;
{
	int		i;

	dprintf( D_ALWAYS, "Entered delete_shadow_rec( %d )\n", pid );

	for( i=0; i<NShadowRecs; i++ ) {
		if( ShadowRecs[i].pid == pid ) {
			
			dprintf(D_FULLDEBUG,
				"Deleting shadow rec for PID %d, job (%d.%d)\n",
			pid, ShadowRecs[i].job_id.cluster, ShadowRecs[i].job_id.proc );
			
			check_zombie( pid, &(ShadowRecs[i].job_id) );
			FREE( ShadowRecs[i].host );
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


mark_job_running( job_id, q )
PROC_ID		*job_id;
DBM			*q;
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

#if DBM_QUEUE
	proc->id = *job_id;
	if( FetchProc(q,proc) < 0 ) { 
		EXCEPT( "FetchProc(%d.%d)", proc->id.cluster, proc->id.proc );
	}
	status = proc->status;
#else
	GetAttributeInt(job_id->cluster, job_id->proc, "Status", &status);
	GetAttributeInt(job_id->cluster, job_id->proc, "MaxHosts", &orig_max);
	SetAttributeInt(job_id->cluster, job_id->proc, "OrigMaxHosts", orig_max);
#endif

	if( status == RUNNING ) {
		EXCEPT( "Trying to run job %d.%d, but already marked RUNNING!",
			job_id->cluster, job_id->proc );
	}

	status = RUNNING;

#if DBM_QUEUE
	if( StoreProc(q,proc) < 0 ) {
		EXCEPT( "StoreProc(0x%x,0x%x)", Q, proc );
	}
#else
	SetAttributeInt(job_id->cluster, job_id->proc, "Status", status);
#endif

	/*
	** dprintf( D_FULLDEBUG, "Marked job %d.%d as RUNNING\n",
	**								proc->id.cluster, proc->id.proc );
	*/
}

/*
** Mark a job as stopped, (Idle or Unexpanded).  Note: this routine assumes
** that the DBM pointer (q) is already locked for writing.
*/
mark_job_stopped( job_id, q )
PROC_ID		*job_id;
DBM			*q;
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
	int		status;
	int		orig_max;
	int		had_orig;
	char	ckpt_name[MAXPATHLEN];

#if DBM_QUEUE
	proc->id = *job_id;
	if( FetchProc(q,proc) < 0 ) { 
		EXCEPT( "FetchProc(%d.%d)", proc->id.cluster, proc->id.proc );
	}
	status = proc->status;
#else
	GetAttributeInt(job_id->cluster, job_id->proc, "Status", &status);
	had_orig = GetAttributeInt(job_id->cluster, job_id->proc, 
							   "OrigMaxHosts", &orig_max);
#endif

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

#if DBM_QUEUE
	if( StoreProc(q,proc) < 0 ) {
		EXCEPT( "StoreProc(0x%x,0x%x)", q, proc );
	}
#else
	SetAttributeInt(job_id->cluster, job_id->proc, "Status", status);
	SetAttributeInt(job_id->cluster, job_id->proc, "CurrentHosts", 0);
	if (had_orig >= 0) {
		SetAttributeInt(job_id->cluster, job_id->proc, "MaxHosts", orig_max);
	}
#endif

	
	dprintf( D_FULLDEBUG, "Marked job %d.%d as %s\n", job_id->cluster,
			job_id->proc, status==IDLE?"IDLE":"UNEXPANDED" );
	
}


mark_cluster_rejected( cluster )
int		cluster;
{
	int		i;

	for( i=0; i<N_RejectedClusters; i++ ) {
		if( RejectedClusters[i] == cluster ) {
			return;
		}
	}
	RejectedClusters[ N_RejectedClusters++ ] = cluster;
}

cluster_rejected( cluster )
int		cluster;
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
is_alive( pid )
int		pid;
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

clean_shadow_recs()
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

preempt( n )
int		n;
{
	int		i;

	dprintf( D_ALWAYS, "Called preempt( %d )\n", n );
	for( i=NShadowRecs-1; n > 0 && i >= 0; n--, i-- ) {
		if( is_alive(ShadowRecs[i].pid) ) {
			if( ShadowRecs[i].preempted ) {
				send_vacate( ShadowRecs[i].host, KILL_FRGN_JOB );
			} else {
				send_vacate( ShadowRecs[i].host, CKPT_FRGN_JOB );
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
send_vacate( host, cmd )
char	*host;
int		cmd;
{
	int		sock = -1;
	XDR		xdr, *xdrs = NULL;

	dprintf( D_ALWAYS, "Called send_vacate( %s, %d )\n", host, cmd );
        /* Connect to the specified host */
    if( (sock = do_connect(host, "condor_startd", START_PORT)) < 0 ) {
        dprintf( D_ALWAYS, "Can't connect to startd on %s\n", host );
		RETURN;
    }
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;

    if( !xdr_int(xdrs, &cmd) ) {
        dprintf( D_ALWAYS, "Can't initialize xdr sock to %s\n", host );
		RETURN;
	}

    if( !xdrrec_endofrecord(xdrs,TRUE) ) {
        dprintf( D_ALWAYS, "Can't send xdr EOF to %s\n", host );
		RETURN;
	}

	if( cmd == CKPT_FRGN_JOB ) {
		dprintf( D_ALWAYS, "Sent CKPT_FRGN_JOB to startd on %s\n", host );
	} else {
		dprintf( D_ALWAYS, "Sent KILL_FRGN_JOB to startd on %s\n", host );
	}
	RETURN;

}
#undef RETURN


PROC GlobalqEndMarker;
XDR *Proc_XDRS;

send_all_jobs( xdrs )
XDR *xdrs;
{
	DBM *q = NULL;
	PROC *end = &GlobalqEndMarker;
	char queue[MAXPATHLEN];
	int send_proc();

	dprintf( D_FULLDEBUG, "Schedd received SEND_ALL_JOBS request\n");

#if DBM_QUEUE
	/* Open and lock the job queue */
	(void)sprintf( queue, "%s/job_queue", Spool );
	if( (q=OpenJobQueue(queue,O_RDONLY,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue );
	}

	LOCK_JOB_QUEUE( q, READER );

	xdrs->x_op = XDR_ENCODE;
	Proc_XDRS = xdrs;

	/* This will scan the job Q and send procs across */
	ScanJobQueue( q, send_proc );
	
	CLOSE_JOB_QUEUE(q);
#else
	WalkJobQueue( send_proc );
#endif

#if defined(NEW_PROC)
	end->n_cmds = 0;
	end->n_pipes = 0;

	end->owner = end->env = end->rootdir = end->iwd = 
		end->requirements = end->preferences = "";
	end->version_num = PROC_VERSION;
#else
	end->owner = end->cmd = end->args = end->env = end->in = 
		end->out = end->err = end->rootdir = end->iwd = 
		end->requirements = end->preferences = "";
	end->version_num = PROC_VERSION;
#endif
		
	if( !xdr_proc( xdrs, end ) ) {
		dprintf( D_ALWAYS, "Couldn't send end marker for SEND_ALL_JOBS\n");
		return FALSE;
	}
	
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		dprintf( D_ALWAYS, "Couldn't send end of record for SEND_ALL_JOBS\n");
		return FALSE;
	}

	dprintf( D_FULLDEBUG, "Finished SEND_ALL_JOBS\n");
}

#if DBM_QUEUE
send_proc( proc )
PROC *proc;
#else
send_proc(cluster, proc)
int		cluster;
int		proc;
#endif
{
	XDR *xdrs = Proc_XDRS;	/* Can't pass this routine an arg, so use global */
	PROC	p;
	int		rval;

#if DBM_QUEUE
	if( !xdr_proc( xdrs, proc ) ) {
		return FALSE;
	}
#else
#if 0
	GetProc(cluster, proc, &p);
	rval = xdr_proc(xdrs, p);
	/* GetProc allocates all these things, so we must free them.  Did I miss
	   anything?  I haven't really checked yet. */
	FREE( p.cmd[0] );
	FREE( p.args[0] );
	FREE( p.in[0] );
	FREE( p.out[0] );
	FREE( p.err[0] );
	FREE( p.cmd );
	FREE( p.args );
	FREE( p.in );
	FREE( p.out );
	FREE( p.err );
	FREE( p.exit_status );
	FREE( p.remote_usage );
	FREE( p.rootdir );
	FREE( p.iwd );
	FREE( p.requirements );
	FREE( p.preferences );
#else
	return FALSE;
#endif
#endif
}

send_all_jobs_prioritized( xdrs )
XDR	*xdrs;
{
#if defined(NEW_PROC)
	GENERIC_PROC	buf;
	PROC			*proc  = (PROC *)&buf;
#else
	char			buf[1024];
	PROC			*proc  = (PROC *)buf;
#endif
	char	queue[MAXPATHLEN];
	int		i;
	DBM		*q = NULL;
	PROC *end = &GlobalqEndMarker;
	int all_job_prio();

	dprintf( D_FULLDEBUG, "\n" );
	dprintf( D_FULLDEBUG, "Entered send_all_jobs_prioritized\n" );

	N_PrioRecs = 0;

#if DBM_QUEUE
	/* Open and lock the job queue */
	(void)sprintf( queue, "%s/job_queue", Spool );
	if( (q=OpenJobQueue(queue,O_RDONLY,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue );
	}

	LOCK_JOB_QUEUE( q, READER );

	/* Prioritize the jobs */
	ScanJobQueue( q, all_job_prio );
#else
	WalkJobQueue( all_job_prio );
#endif

	qsort( (char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]), prio_compar );

	/*
	** for( i=0; i<N_PrioRecs; i++ ) {
	** 	dprintf( D_FULLDEBUG, "PrioRec[%d] = %d.%d %3.10f\n",
	** 		i, PrioRec[i].id.cluster, PrioRec[i].id.proc, PrioRec[i].prio );
	** }
	*/

	xdrs->x_op = XDR_ENCODE;

	for( i=0; i < N_PrioRecs; i++ ) {
		proc->id = PrioRec[i].id;
		(void)FetchProc( q, proc );
		display_proc_long( proc );
		if( !xdr_proc( xdrs, proc ) ) {
#if DBM_QUEUE
			CLOSE_JOB_QUEUE(q);
#endif
			return FALSE;
		}
	}
#if DBM_QUEUE
	CLOSE_JOB_QUEUE(q);
#endif

#if defined(NEW_PROC)
	end->n_cmds = 0;
	end->n_pipes = 0;

	end->owner = end->env = end->rootdir = end->iwd = 
		end->requirements = end->preferences = "";
	end->version_num = PROC_VERSION;
#else
	end->owner = end->cmd = end->args = end->env = end->in = 
		end->out = end->err = end->rootdir = end->iwd = 
		end->requirements = end->preferences = "";
	end->version_num = PROC_VERSION;
#endif
		
		
	if( !xdr_proc( xdrs, end ) ) {
		dprintf( D_ALWAYS, "Couldn't send end marker for SEND_ALL_JOBS\n");
		return FALSE;
	}
	
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		dprintf( D_ALWAYS, "Couldn't send end of record for SEND_ALL_JOBS\n");
		return FALSE;
	}

	dprintf( D_FULLDEBUG, "Finished send_all_jobs_prioritized\n");
}

/*
** This is different from "job_prio()" because we want to send all jobs
** regardless of the state they are in.
*/
#if DBM_QUEUE
all_job_prio( proc )		/* UPDOWN */
PROC	*proc;
#else
all_job_prio( cluster, proc )
int		cluster;
int		proc;
#endif
{
	int prio;
	int status;
	int job_prio;
	int job_status;
	PROC_ID	job_id;
	int job_q_date;
	char	own_buf[100], *owner;

#if DBM_QUEUE
	job_prio = proc->prio;
	job_status = proc->status;
	owner = proc->owner;
	job_q_date = proc->q_date;
	job_id = proc->id;
#else
	GetAttributeInt(cluster, proc, "Prio", &job_prio);
	GetAttributeInt(cluster, proc, "Status", &job_status);
	GetAttributeInt(cluster, proc, "Q_date", &job_q_date);
	GetAttributeString(cluster, proc, "Owner", own_buf);
	owner = own_buf;
	job_id.cluster = cluster;
	job_id.proc = proc;
#endif
	prio = upDown_GetUserPriority(owner, &status);
	if ( status == Error ) {
		dprintf( D_ALWAYS, 
				"all_job_prio: Can't find user priority for %s, assuming 0\n",
				owner );
/*		EXCEPT( "Can't evaluate \"PRIO\"" ); */
		prio = 0;
	}
	

	
	PrioRec[N_PrioRecs].id = job_id;
	PrioRec[N_PrioRecs].prio = prio;
	PrioRec[N_PrioRecs].job_prio = job_prio;
	PrioRec[N_PrioRecs].status = job_status;
	PrioRec[N_PrioRecs].qdate = job_q_date;
	N_PrioRecs += 1;

}

swap_space_exhausted()
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
** Several procedures in this program will manipulate the job queue.  To
** avoid conflicts with other processes which will also be maniuplating
** the queue, we utilize advisory file locks.  One procedure in this
** process is a signal handler which will be called asynchronously in
** response to a shadow process's death.  That signal handler will also
** manipulate the job queue, and will also need to obtain a lock before
** doing so.  There is a potential deadlock situation in that if the
** signal handler is called while another part of this process has the
** lock, neither part can complete.  To avoid this we make sure that the
** signal associated with the "dangerous" signal handler can never be
** taken while the lock is already held.  Every part of this program which
** needs to aquire the lock *must* do so by going through this procedure.
** In this way we can ensure that the signal handler will be called only
** at times when we are not already holding the lock.
*/
#if DBM_QUEUE
GuardQueue( q, mode, file, line )
DBM		*q;
int		mode;
char	*file;
int		line;
{
#if defined(HPUX9) || defined(Solaris)
	static sigset_t	oblock;
	sigset_t	nblock;
#else
	static int	oblock;
#endif
	static int	is_locked = 0;

	if( mode & LOCK_UN & LOCK_SH ) {
		EXCEPT( "Bogus lock mode: includes both unlock & shared lock bits" );
	}

	if( mode & LOCK_UN & LOCK_EX ) {
		EXCEPT( "Bogus lock mode: includes both unlock & exclusive lock bits" );
	}

	if( (mode & LOCK_UN) == 0 ) {	/* Obtain the lock */
#if defined(HPUX9) || defined(Solaris)
	 	sigemptyset(&oblock);
	 	sigemptyset(&nblock);
		sigaddset(&nblock,SIGCHLD);
	    sigprocmask(SIG_BLOCK,&nblock,&oblock);
#else			
		oblock = sigblock(sigmask(SIGCHLD));
#endif
		if( is_locked ) {
			dprintf( D_ALWAYS,
							"Attempted to obtain lock, but already locked" );
			sigsetmask( 0 );
			abort();
		}
		is_locked = 1;
		LockJobQueue( q, mode );
		dprintf( D_FULLDEBUG, "Locked queue: mode %d at %d in %s\n",
													mode, line, file );
	} else {						/* Release the lock */
		if( !is_locked ) {
			dprintf( D_ALWAYS,
							"Attempted to release lock, but already unlocked" );
			sigsetmask( 0 );
			abort();
		}
		is_locked = 0;
		CloseJobQueue( q );
		dprintf( D_FULLDEBUG, "Unlocked queue: at %d in %s\n", line, file );
#if defined(HPUX9) || defined(Solaris)
		sigprocmask(SIG_SETMASK,&oblock,NULL);
#else
		sigsetmask( oblock );
#endif
	}
}
#endif /* DBM_QUEUE */


#if 0
#define MAX_PRIO_REC 2048
struct prio_rec {
	PROC_ID		id;
	float		prio;
} PrioRec[MAX_PRIO_REC];
int		N_PrioRecs = 0;

#define MAX_SHADOW_RECS 512
struct shadow_rec {
	int			pid;
	PROC_ID		job_id;
	char		*host;
	int			preempted;
} ShadowRecs[ MAX_SHADOW_RECS ];
int	NShadowRecs;
#endif

int	BadCluster;
int	BadProc;


/*
  We maintain two tables which should be consistent, return TRUE if they
  are, and FALSE otherwise.  The tables are the ShadowRecs, a list
  of currently running jobs, and PrioRec a list of currently runnable
  jobs.  We will say they are consistent if none of the currently
  runnable jobs are already listed as running jobs.
*/
int
shadow_prio_recs_consistent()
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
struct shadow_rec *
find_shadow_rec( id )
PROC_ID	*id;
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

extern char	*CondorAdministrator;

mail_problem_message()
{
	char	cmd[512];
	char	hostname[512];
	FILE	*mailer, *popen();

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
