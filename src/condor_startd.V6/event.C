#include <sys/types.h>
#include <sys/wait.h>

#include "condor_debug.h"
#include "condor_types.h"
#include "condor_attributes.h"
#include "condor_expressions.h"
#include "condor_mach_status.h"
#include "sched.h"

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

extern int update_interval;
extern int polling_interval;
extern int capab_timeout;
extern char *collector_host;
extern char *alt_collector_host;
extern int coll_sock;

extern ClassAd* template_ClassAd;

extern ClassAd *resource_timeout_classad (resource_info_t *);
extern ClassAd *resource_update_classad (resource_info_t *);

extern "C" int event_timeout(int sig);

volatile int want_reconfig = 0;

static char *_FileName_ = __FILE__;

void update_central_mgr(void);

static int 
eval_timeout_state(resource_info_t* rip)
{
	int tmp;
	int want_vacate, want_suspend;

	resource_timeout_classad(rip);		// calculates dynamic info
										// needed at every timeout and
										// updates classad
	if( rip->r_state == SYSTEM ) {
		resmgr_changestate(rip->r_rid, NO_JOB);
	}
	if( rip->r_state == NO_JOB ) {
		if(CondorPendingJobs::AreTherePendingJobs(rip->r_rid)) {
			dprintf(D_ALWAYS,"Identified a pending job\n");
			if( MatchInfo(rip,CondorPendingJobs::CapabString(rip->r_rid)) ) {
				dprintf(D_ALWAYS,"Match Info FAILED!!!\n");
			} else {
				Sock* sock=CondorPendingJobs::RetSock(rip->r_rid);
				struct sockaddr_in* from =
					CondorPendingJobs::RespondAddr(rip->r_rid);
				CondorPendingJobs::Status status = 
					CondorPendingJobs::Client(rip->r_rid,rip->r_client);
				CondorPendingJobs::Status status1 =
					CondorPendingJobs::ScheddInterval(rip->r_rid,
													  rip->r_interval);
				if( (status!=CondorPendingJobs::eOK) ||
					(status1!=CondorPendingJobs::eOK) ) {
					dprintf(D_ALWAYS,"Simulated Request Service FAILED!!!\n");
				} else {
 					dprintf(D_ALWAYS,"Simulated Request Service succeeded!\n");
				}
				CondorPendingJobs::ClientMachine(rip->r_rid,
												 rip->r_clientmachine);
				command_startjob(sock,from,rip->r_rid, true);
			}
		}
	}

#if 0
		// This is no reason to EXCEPT.  We could have just
		// relinquished the match but the starter may not have exited
		// yet, so we still have a valid r_pid.
	if( rip->r_pid != NO_PID && !rip->r_jobclassad ) {
		EXCEPT("resource allocated, but no job classad.");
	}
#endif

	if( !rip->r_jobclassad ) {
		return 0;
	}
 
	if( rip->r_universe == VANILLA ) {
		if( (rip->r_classad)->EvalBool("WANT_SUSPEND_VANILLA",
									   rip->r_jobclassad,want_suspend ) == 0) {
			want_suspend = 1;
		}
		if( (rip->r_classad)->EvalBool("WANT_VACATE_VANILLA",
									   rip->r_jobclassad,want_vacate ) == 0) {
			want_vacate = 1;
		}
	} else {
		if( (rip->r_classad)->EvalBool("WANT_SUSPEND",
									   rip->r_jobclassad,want_suspend ) == 0) {
			want_suspend = 1;
		}
		if( (rip->r_classad)->EvalBool("WANT_VACATE",
									   rip->r_jobclassad,want_vacate ) == 0) {
			want_vacate = 1;
		}
	}

	if( (rip->r_state == CHECKPOINTING) ||
		 ((rip->r_state == JOB_RUNNING) && (!want_suspend) &&
		  (!want_vacate) ) ||
		 ((rip->r_state == SUSPENDED) && (!want_vacate)) ) {
		if( rip->r_universe == VANILLA ) {
			if( ((rip->r_classad)->EvalBool("KILL_VANILLA",
											rip->r_jobclassad,tmp))==0 ) {
				if( ((rip->r_classad)->EvalBool("KILL",rip->r_classad,tmp))==0 ) {
					EXCEPT("Can't evaluate KILL");
				}
			}
		} else {
			if( ((rip->r_classad)->EvalBool("KILL",rip->r_jobclassad,tmp))==0 )	{
				EXCEPT("Can't evaluate KILL");
			}
		}
		if( tmp ) {
			event_killall(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if( (rip->r_state == SUSPENDED) ||
		 ((rip->r_state == JOB_RUNNING) && (!want_suspend) && (want_vacate)) ) {
		if( rip->r_universe == VANILLA ) {
			if( ((rip->r_classad)->EvalBool("VACATE_VANILLA",
											 rip->r_jobclassad,tmp))==0 ) {
				if( ((rip->r_classad)->EvalBool("VACATE",
												rip->r_jobclassad,tmp))==0 ) {
					EXCEPT("Can't evaluate VACATE");
				}
			}
		} else {
			if( ((rip->r_classad)->EvalBool("VACATE",
											rip->r_jobclassad,tmp))==0 ) {
				EXCEPT("Can't evaluate VACATE");
			}
		}
		if( tmp ) {
			event_vacate(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if( (rip->r_state == JOB_RUNNING) && want_suspend ) {
		if( rip->r_universe == VANILLA ) {
			if( ((rip->r_classad)->EvalBool("SUSPEND_VANILLA",
											rip->r_jobclassad,tmp))==0 ) {
				if( ((rip->r_classad)->EvalBool("SUSPEND",
												rip->r_jobclassad,tmp))==0 ) {
					EXCEPT("Can't evaluate SUSPEND");
				}
			}
		} else {
			if( ((rip->r_classad)->EvalBool("SUSPEND",
										   rip->r_jobclassad,tmp))==0 ) {
				EXCEPT("Can't evaluate SUSPEND");
			}
		}
		if( tmp ) {
			event_suspend(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if( rip->r_state == SUSPENDED ) {
		if( rip->r_universe == VANILLA ) {
			if( ((rip->r_classad)->EvalBool("CONTINUE_VANILLA",
											rip->r_jobclassad,tmp))==0 ) {
				if( ((rip->r_classad)->EvalBool("CONTINUE",
											 rip->r_jobclassad,tmp))==0 ) {
					EXCEPT("Can't evaluate CONTINUE");
				}
			}
		} else {
			if( ((rip->r_classad)->EvalBool("CONTINUE",
											rip->r_jobclassad,tmp))==0 ) {
				EXCEPT("Can't evaluate CONTINUE");
			}
		}
		if( tmp ) {
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
			rip->r_timed_out = 1;
			resource_free( rip->r_rid );
		}
	} else if (rip->r_capab) {
		if (time(NULL) - rip->r_captime > capab_timeout) {
			dprintf(D_ALWAYS, "Capability (%s) timed out\n",
					rip->r_capab);
			rip->r_timed_out = 1;
			resource_free( rip->r_rid );
		}
	}
	return 0;
}

static int 
eval_update_state(resource_info_t* rip)
{
		// On update, do everything we need to do at every timeout
	eval_timeout_state(rip);
		// Plus, figure out things we only need during an update
	resource_update_classad(rip);
}


// Returns the absolute time that the next timeout should happen.
int
event_timeout(int sig)
{
	static int last_update = 0;
	static int last_timeout = 0;

	int next_timeout = 0;
	int now = (int)time((time_t *)0);

	if( now - last_update >= update_interval ) {
		resmgr_walk(eval_update_state);  // Compute statistics and
										 // evaluate state for
										 // updating the collector. 
		resmgr_walk(check_claims);
		update_central_mgr();
		last_update = (int)time((time_t *)0);
	} else {
		resmgr_walk(eval_timeout_state);
		resmgr_walk(check_claims);
	}

	if( resmgr_resourceinuse() ) {
		next_timeout = polling_interval;
	} else {
		next_timeout = update_interval;
	}
	now = (int)time((time_t *)0);
	last_timeout = now;
	return( next_timeout + now );
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
	kill( getpid(), SIGKILL );
}

void event_sigquit(int sig)
{
	dprintf(D_ALWAYS, "Killed by SIGQUIT\n");
	kill( getpid(), SIGKILL );
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
		CondorPrefExps::RestoreReqEx(rip->r_classad);
		return;
	}
  
	// A job is running, we need to modify the Requirements expression of 
	// our ad suitably

	ClassAd* job = rip->r_jobclassad;
	ClassAd* mc = rip->r_classad;

	CondorPrefExps::ModifyReqEx(mc,job);
}

static int send_resource_classad(resource_info_t* rip)
{
	ClassAd *cp;

	AccountForPrefExps(rip);
	cp = rip->r_classad;
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
	resmgr_walk(send_resource_classad);
}
