#include <sys/types.h>
#include <sys/wait.h>

#include "condor_debug.h"
#include "condor_types.h"
#include "condor_attributes.h"
#include "condor_expressions.h"
#include "condor_mach_status.h"
#include "sched.h"

#include "cdefs.h"
#include "event.h"
#include "state.h"
#include "resource.h"
#include "resmgr.h"

#include "CondorPrefExps.h"
#include "CondorPendingJobs.h"

/*
 * Handle async events coming in to the startd.
 */

int command_startjob(Sock *, struct sockaddr_in *, resource_id_t,
					 bool JobAlreadyGot=false);
int command_reqservice(Sock *, struct sockaddr_in *, resource_id_t); 
int MatchInfo(resource_info_t* rip, char* str);

extern int polls_per_update;
extern int last_timeout;
extern int capab_timeout;
extern char *collector_host;
extern char *alt_collector_host;
extern int coll_sock;

extern ClassAd* template_ClassAd;

extern ClassAd *resource_context (resource_info_t *);

extern "C" void event_timeout(int sig);

volatile int want_reconfig = 0;
int polls = 0;

// owner_state is true when the startd isn't running any jobs and
// the is not available to run any jobs (i.e., is busy) --
// the idea is that we should watch the machine status less closely
// when in owner state (calculate load average less often, etc.)
// to be less of a burden on the machine owner
bool owner_state = true;

static char *_FileName_ = __FILE__;

void update_central_mgr(void);

static int eval_state(resource_info_t* rip)
{
	int tmp;
	int want_vacate, want_suspend;

	resource_context(rip);		// calculates dynamic info and builds classad
	if (rip->r_state == SYSTEM)
		resmgr_changestate(rip->r_rid, NO_JOB);
	if (rip->r_state == NO_JOB)	{
		if ((rip->r_context)->EvalBool(ATTR_REQUIREMENTS, NULL, tmp) == 0) {
			// Requirements do not locally evaluate to FALSE
			// so we're not in owner state
			owner_state = false;
		}
		if (tmp == 1) {		// Requirements locally evaluate to TRUE
			owner_state = false;
		}
		if(CondorPendingJobs::AreTherePendingJobs(rip->r_rid)) {
			dprintf(D_ALWAYS,"Identified a pending job\n");
			if(MatchInfo(rip,CondorPendingJobs::CapabString(rip->r_rid)))
				dprintf(D_ALWAYS,"Match Info FAILED!!!\n");
			else {
				Sock* sock=CondorPendingJobs::RetSock(rip->r_rid);
				struct sockaddr_in* from =
					CondorPendingJobs::RespondAddr(rip->r_rid);
				CondorPendingJobs::Status status = 
					CondorPendingJobs::Client(rip->r_rid,rip->r_client);
				CondorPendingJobs::Status status1 =
					CondorPendingJobs::ScheddInterval(rip->r_rid,
													  rip->r_interval);
	
				if((status!=CondorPendingJobs::eOK) ||
				   (status1!=CondorPendingJobs::eOK))
					dprintf(D_ALWAYS,"Simulated Request Service FAILED!!!\n");
				else
 					dprintf(D_ALWAYS,"Simulated Request Service succeeded!\n");
				CondorPendingJobs::ClientMachine(rip->r_rid,
												 rip->r_clientmachine);
				command_startjob(sock,from,rip->r_rid, true);
			}
		}
	}
	if (rip->r_pid != NO_PID && !rip->r_jobcontext) {
		EXCEPT("resource allocated, but no job context.\n");
	}

	if (!rip->r_jobcontext)
		return 0;

	owner_state = false;		// We're running a job.
 
	if ( rip->r_universe == VANILLA ) {
		if ((rip->r_context)->EvalBool("WANT_SUSPEND_VANILLA",
									   rip->r_jobcontext,want_suspend) == 0) {
			want_suspend = 1;
		}
		if ((rip->r_context)->EvalBool("WANT_VACATE_VANILLA",
									   rip->r_jobcontext,want_vacate) == 0) {
			want_vacate = 1;
		}
	} else {
		if ((rip->r_context)->EvalBool("WANT_SUSPEND",
									   rip->r_jobcontext,want_suspend) == 0) {
			want_suspend = 1;
		}
		if ((rip->r_context)->EvalBool("WANT_VACATE",
									   rip->r_jobcontext,want_vacate) == 0) {
			want_vacate = 1;
		}
	}

	if ( (rip->r_state == CHECKPOINTING) ||
		 ((rip->r_state == JOB_RUNNING) && (!want_suspend) &&
		  (!want_vacate)) ||
		 ((rip->r_state == SUSPENDED) && (!want_vacate)) ) {
		if (rip->r_universe == VANILLA) {
			if(((rip->r_context)->EvalBool("KILL_VANILLA",
										   rip->r_jobcontext,tmp))==0 &&
			   ((rip->r_context)->EvalBool("KILL",rip->r_context,tmp))==0 ) {
				EXCEPT("Can't evaluate KILL\n");
			}
		} else {
			if(((rip->r_context)->EvalBool("KILL",rip->r_jobcontext,tmp))==0)
				{
					EXCEPT("Can't evaluate KILL\n");
				}
		}
		if (tmp) {
			event_killall(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if ( (rip->r_state == SUSPENDED) ||
		 ((rip->r_state == JOB_RUNNING) && (!want_suspend) && (want_vacate))) {
		if (rip->r_universe == VANILLA) {
			if(((((rip->r_context)->EvalBool("VACATE_VANILLA",
											 rip->r_jobcontext,tmp))==0)) &&
			   (((rip->r_context)->EvalBool("VACATE",
											rip->r_jobcontext,tmp))==0)) {
				EXCEPT("Can't evaluate VACATE\n");
			}
		} else {
			if (((rip->r_context)->EvalBool("VACATE",
											rip->r_jobcontext,tmp))==0) {
				EXCEPT("Can't evaluate VACATE\n");
			}
		}
		if (tmp) {
			event_vacate(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if ( (rip->r_state == JOB_RUNNING) && want_suspend ) {
		if (rip->r_universe == VANILLA) {
			if((((rip->r_context)->EvalBool("SUSPEND_VANILLA",
											rip->r_jobcontext,tmp))==0) &&
			   ((rip->r_context)->EvalBool("SUSPEND",
										   rip->r_jobcontext,tmp))==0) {
				EXCEPT("Can't evaluate SUSPEND\n");
			}
		} else {
			if(((rip->r_context)->EvalBool("SUSPEND",
										   rip->r_jobcontext,tmp))==0) {
				EXCEPT("Can't evaluate SUSPEND\n");
			}
		}
		if (tmp) {
#if 0
			if (rip->r_claimed)		/* Why call vacate_client here? */
				vacate_client(rip->r_rid);
#endif
			event_suspend(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if (rip->r_state == SUSPENDED) {
		if (rip->r_universe == VANILLA) {
			if((((rip->r_context)->EvalBool("CONTINUE_VANILLA",
											rip->r_jobcontext,tmp))==0) &&
			   ((rip->r_context)->EvalBool("CONTINUE",
										   rip->r_jobcontext,tmp))==0) {
				EXCEPT("Can't evaluate CONTINUE\n");
			}
		} else {
			if(((rip->r_context)->EvalBool("CONTINUE",
										   rip->r_jobcontext,tmp))==0) {
				EXCEPT("Can't evaluate CONTINUE\n");
			}
		}
		if (tmp) {
			event_continue(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}
	return 0;
}

static int
check_claims(resource_info_t* rip)
{
	if (rip->r_claimed == TRUE) {
		if ((time(NULL) - rip->r_receivetime) > 2 * rip->r_interval) {
			dprintf(D_ALWAYS, "Capability (%s) timed out\n",
					rip->r_capab);
			rip->r_claimed = FALSE;
			free(rip->r_capab);
			free(rip->r_client);
			rip->r_capab = NULL;
			rip->r_client = NULL;	
		}
	} else if (rip->r_capab) {
		if (time(NULL) - rip->r_captime > capab_timeout) {
			dprintf(D_ALWAYS, "Capability (%s) timed out\n",
					rip->r_capab);
			rip->r_claimed = FALSE;
			free(rip->r_capab);
			free(rip->r_client);
			rip->r_capab = NULL;
			rip->r_client = NULL;
		}
	}
	return 0;
}

void
event_timeout(int sig)
{
	owner_state = true;			// default for owner_state is true
	resmgr_walk(eval_state);	// conditionally set to false in eval_state
	resmgr_walk(check_claims);
	if (polls == 0)
		update_central_mgr();
	if (++polls >= polls_per_update)
		polls = 0;
	last_timeout = (int)time((time_t *)0);
	if (owner_state) {
		dprintf(D_FULLDEBUG,"in owner state; using OWNER_POLLING_FREQUENCY\n");
	}
}

void
event_sigchld(int sig)
{
	int pid, status;
	resource_id_t rid;
	resource_info_t *rip;
	char tmp[80];

	while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0 ) {
		if (WIFSTOPPED(status)) {
			dprintf(D_ALWAYS, "pid %d stopped.\n", pid);
			continue;
		}
		rip = resmgr_getbypid(pid);
		if (rip == NULL) {
			return;
		}
		if (WIFSIGNALED(status))
			dprintf(D_ALWAYS, "pid %d died on signal %d\n",
					pid, WTERMSIG(status));
		else
			dprintf(D_ALWAYS, "pid %d exited with status %d\n",
					pid, WEXITSTATUS(status));
#if 0
		// ??? Why update RemoteUser here?  rip->r_user could be NULL!
		sprintf(tmp,"RemoteUser=%s",(rip->r_user));
		(rip->r_context)->Insert(tmp);
#endif
		resmgr_changestate(rip->r_rid, NO_JOB);
		event_exited(rip->r_rid, NO_JID, NO_TID);
	}
}

void
event_sighup(int sig)
{
	want_reconfig = 1;
}

void
event_sigterm(int sig)
{
	dprintf(D_ALWAYS, "Got SIGTERM. Freeing resources and exiting.\n");
	if (resmgr_resourceinuse()) {
		resmgr_vacateall();
		while (resmgr_resourceinuse()) sigpause(0);
	}
	exit(0);
}

void event_sigquit(int sig)
{
	dprintf(D_ALWAYS, "Killed by SIGQUIT\n");
	exit(0);
}

void event_sigint(int sig)	/* sigint_handler */
{
	dprintf(D_ALWAYS, "Killed by SIGINT\n");
	exit(0);
}

void AccountForPrefExps(resource_info_t* rip)
{
	if(rip->r_state!=JOB_RUNNING) {
		// nothing to be done: possibly restore the
		// Requirements expression
		CondorPrefExps::RestoreReqEx(rip->r_context);
		return;
	}
  
	// A job is running, we need to modify the Requirements expression of 
	// our ad suitably

	ClassAd* job = rip->r_jobcontext;
	ClassAd* mc = rip->r_context;

	CondorPrefExps::ModifyReqEx(mc,job);
}

static int send_resource_context(resource_info_t* rip)
{
	ClassAd *cp;

	AccountForPrefExps(rip);
	cp = resource_context(rip);
	send_classad_to_machine(collector_host, COLLECTOR_UDP_COMM_PORT,
							coll_sock, cp);
	if (alt_collector_host)
		send_classad_to_machine(alt_collector_host, COLLECTOR_UDP_COMM_PORT,
								coll_sock, cp);
	return 0;
}

void update_central_mgr()
{
	dprintf(D_ALWAYS, "updating central manager\n");
	resmgr_walk(send_resource_context);
}
