#include <sys/types.h>
#include <sys/wait.h>

#include "condor_debug.h"
#include "condor_types.h"
#include "condor_expressions.h"
#include "dgram_io_handle.h"
#include "sched.h"

#include "cdefs.h"
#include "event.h"
#include "state.h"
#include "resource.h"
#include "resmgr.h"

/*
 * Handle async events coming in to the startd.
 */

extern int polls_per_update;
extern int last_timeout;
extern int capab_timeout;
extern char *alt_collector_host;
extern CONTEXT *template_context;
extern DGRAM_IO_HANDLE coll_handle, alt_coll_handle;

extern CONTEXT *resource_context __P((resource_info_t *));
extern EXPR *build_expr();

volatile int want_reconfig;
int polls;

static char *_FileName_ = __FILE__;

static void update_central_mgr __P((void));

static int
eval_state(rip)
	resource_info_t *rip;
{
	int start = FALSE, tmp;

	resource_context(rip);
	if (rip->r_state == SYSTEM)
		resmgr_changestate(rip->r_rid, NO_JOB);

	if (rip->r_state == NO_JOB)
		evaluate_bool("START", &start, rip->r_context,
			      template_context);

	if (rip->r_pid != NO_PID && !rip->r_jobcontext) {
		EXCEPT("resource allocated, but no job context.\n");
	}

	if (!rip->r_jobcontext)
		return 0;

	if (rip->r_state == CHECKPOINTING) {
		if (rip->r_universe == VANILLA) {
			if (evaluate_bool("KILL_VANILLA", &tmp, rip->r_context,
					  rip->r_jobcontext) < 0 &&
			    evaluate_bool("KILL", &tmp, rip->r_context,
					  rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate KILL\n");
			}
		} else {
			if (evaluate_bool("KILL", &tmp, rip->r_context,
					rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate KILL\n");
			}
		}
		if (tmp) {
			event_killall(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if (rip->r_state == SUSPENDED) {
		if (rip->r_universe == VANILLA) {
			if (evaluate_bool("VACATE_VANILLA", &tmp,rip->r_context,
					  rip->r_jobcontext) < 0 &&
			    evaluate_bool("VACATE", &tmp, rip->r_context,
					  rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate VACATE\n");
			}
		} else {
			if (evaluate_bool("VACATE", &tmp, rip->r_context,
					rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate VACATE\n");
			}
		}
		if (tmp) {
			event_vacate(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if (rip->r_state == JOB_RUNNING) {
		if (rip->r_universe == VANILLA) {
			if (evaluate_bool("SUSPEND_VANILLA",&tmp,rip->r_context,
					  rip->r_jobcontext) < 0 &&
			    evaluate_bool("SUSPEND", &tmp, rip->r_context,
					  rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate SUSPEND\n");
			}
		} else {
			if (evaluate_bool("SUSPEND", &tmp, rip->r_context,
					rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate SUSPEND\n");
			}
		}
		if (tmp) {
			if (rip->r_claimed)
				vacate_client(rip->r_rid);
			event_suspend(rip->r_rid, NO_JID, NO_TID);
			return 0;
		}
	}

	if (rip->r_state == SUSPENDED) {
		if (rip->r_universe == VANILLA) {
			if (evaluate_bool("CONTINUE_VANILLA", &tmp,
					  rip->r_context, rip->r_jobcontext) < 0
			    && evaluate_bool("CONTINUE", &tmp, rip->r_context,
					  rip->r_jobcontext) < 0)
			{
				EXCEPT("Can't evaluate CONTINUE\n");
			}
		} else {
			if (evaluate_bool("CONTINUE", &tmp, rip->r_context,
					rip->r_jobcontext) < 0)
			{
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
check_claims(rip)
	resource_info_t *rip;
{
	if (rip->r_claimed == TRUE) {
		if ((time(NULL) - rip->r_receivetime) > 2 * rip->r_interval) {
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
			free(rip->r_capab);
			free(rip->r_client);
			rip->r_capab = NULL;
			rip->r_client = NULL;
		}
	}
	return 0;
}

void
event_timeout(sig)
	int sig;
{
	resmgr_walk(eval_state);
	resmgr_walk(check_claims);
	if (polls == 0)
		update_central_mgr();
	if (++polls >= polls_per_update)
		polls = 0;
	last_timeout = (int)time((time_t *)0);
}

void
event_sigchld(sig)
	int sig;
{
	int pid, status;
	resource_id_t rid;
	resource_info_t *rip;
	ELEM tmp;

	while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0 ) {
		if (WIFSTOPPED(status)) {
			dprintf(D_ALWAYS, "pid %d stopped.\n", pid);
			continue;
		}
		rip = resmgr_getbypid(pid);
		if (rip == NULL) {
			EXCEPT("SIGCHLD from job on non-existing resource??");
		}
		if (WIFSIGNALED(status))
			dprintf(D_ALWAYS, "pid %d died on signal %d\n",
				pid, WTERMSIG(status));
		else
			dprintf(D_ALWAYS, "pid %d exitex with status %d\n",
				pid, WEXITSTATUS(status));
		tmp.type = STRING;
		rip->r_user = "";
		tmp.s_val = rip->r_user;
		store_stmt(build_expr("RemoteUser", &tmp), rip->r_context);
		resmgr_changestate(rip->r_rid, NO_JOB);
		event_exited(rip->r_rid, NO_JID, NO_TID);
	}
}

void
event_sighup(sig)
	int sig;
{
	want_reconfig = 1;
}

void
event_sigterm(sig)
	int sig;
{
	dprintf(D_ALWAYS, "Got SIGTERM. Freeing resources and exiting.\n");
	resmgr_vacateall();
}

void
event_sigint(sig)	/* sigint_handler */
	int sig;
{
	dprintf(D_ALWAYS, "Killed by SIGINT\n");
	exit(0);
}

static int
send_resource_context(rip)
	resource_info_t *rip;
{
	CONTEXT *cp;
	ELEM tmp;

	dprintf(D_ALWAYS, "send_resource_context called.\n");
	cp = resource_context(rip);
	dprintf(D_ALWAYS, "context len %d\n", cp->len);
#ifdef old
	send_context_to_machine(&coll_handle, STARTD_INFO, cp);
#else
	send_classad_to_negotiator(&coll_handle, cp);
#endif
	if (alt_collector_host)
#ifdef old
		send_context_to_machine(&alt_coll_handle, STARTD_INFO, cp);
#else
		send_classad_to_negotiator(&alt_coll_handle, cp);
#endif
	return 0;
}

static void
update_central_mgr()
{
	dprintf(D_ALWAYS, "update_central_manager called.\n");
	resmgr_walk(send_resource_context);
}
