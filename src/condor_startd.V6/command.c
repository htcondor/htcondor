#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "sched.h"
#include "expr.h"
#include "world.h"	/* XXX */
#include "proc.h"
#include "cdefs.h"
#include "condor_debug.h"
#include "condor_expressions.h"

#include "resource.h"
#include "resmgr.h"

static char *_FileName_ = __FILE__;
static char *Starter;

int Last_X_Event;

extern char *AlternateStarter[10];
extern char *PrimaryStarter;
extern char *Starter;
extern int polls;
extern int capab_timeout;

extern EXPR *build_expr __P((char *, ELEM *));
extern int event_checkpoint  __P((resource_id_t, job_id_t, task_id_t));

static int command_startjob __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_block __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_unblock __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_suspend __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_continue __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_kill	__P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_vacate __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_matchinfo __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_reqservice __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_relservice __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_x_event __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_alive __P((XDR *, struct sockaddr_in *, resource_id_t));
static int command_pcheckpoint	__P((XDR *, struct sockaddr_in *, resource_id_t));

/*
 * Functions to handle commands from incoming requests.
 */

int
command_main(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	int cmd;

	dprintf(D_ALWAYS, "command_main(%x, %x, %x)\n", 
		xdrs, from, rid);
	
	xdrs->x_op = XDR_DECODE;
	if (!xdr_int(xdrs,&cmd)) {
		dprintf(D_ALWAYS, "Can't read command\n");
		return -1;
	}

	dprintf(D_ALWAYS, "Got command %d.\n", cmd);

	/* XXX how to get resource */

	switch (cmd) {
	case STARTER_0:
	case STARTER_1:
	case STARTER_2:
	case STARTER_3:
	case STARTER_4:
	case STARTER_5:
	case STARTER_6:
	case STARTER_7:
	case STARTER_8:
	case STARTER_9:
		/* TODO: get alt starter, set for resource */
		Starter = AlternateStarter[cmd - SCHED_VERS - ALT_STARTER_BASE];
		dprintf(D_ALWAYS, "alt starter = '%s'\n", Starter);
		return command_startjob(xdrs, from, rid);
	case START_FRGN_JOB:
		Starter = PrimaryStarter;
		dprintf(D_ALWAYS, "starter = '%s'\n", Starter);
		return command_startjob(xdrs, from, rid);
	case CKPT_FRGN_JOB:
		return command_vacate(xdrs, from, rid);
	case PCKPT_FRGN_JOB:
		return command_pcheckpoint(xdrs, from, rid);
	case KILL_FRGN_JOB:
		return command_kill(xdrs, from, rid);
	case X_EVENT_NOTIFICATION:
		return command_x_event(xdrs, from, rid);
	case MATCH_INFO:
		return command_matchinfo(xdrs, from, rid);
	case REQUEST_SERVICE:
		return command_reqservice(xdrs, from, rid);
	case RELINQUISH_SERVICE:
		return command_relservice(xdrs, from, rid);
	case ALIVE:
		return command_alive(xdrs, from, rid);
	case REQ_NEW_PROC:
	default:
		dprintf(D_ALWAYS, "Can't handle this.\n");
	}
}

#define ABORT \
		if (rip->r_jobcontext)			 \
			free_context(rip->r_jobcontext); \
		if (rip->r_clientmachine) {		 \
			free(rip->r_clientmachine);	 \
			rip->r_clientmachine = NULL;	 \
		}					 \
		return;
/*
 * Start a job. Allocate the resource and use resource_activate to
 * get things going.
 */
static int
command_startjob(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{

	int start = 1,job_reqs = 1, capability_verify = 1;
	CONTEXT	*job_context = NULL, *MachineContext;
	ELEM tmp;
	char *check_string = NULL;
	PORTS ports;
	int sock_1, sock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;
	struct hostent *hp, *gethostbyaddr();
	char official_name[MAXHOSTLEN], *ptr = official_name, *ClientMachine;
	StartdRec stRec;
	start_info_t ji;	/* XXXX */
	resource_name_t rname;
	resource_info_t *rip;
	char *RemoteUser, *JobId;
	int universe;

	dprintf(D_ALWAYS, "command_startjob called.\n");

	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "startjob: unknown resource.\n");
		return -1;
	}

	rip->r_starter = Starter;
	
	check_string = (char *)malloc(SIZE_OF_CAPABILITY_STRING * sizeof(char));

	if (!xdr_string(xdrs, &check_string, SIZE_OF_CAPABILITY_STRING)) {
		free(check_string);
		dprintf(D_ALWAYS, "Can't receive capability from schedd\n");
		return;
	}
	dprintf(D_ALWAYS, "capability %s received from shadow\n", check_string);

	if ((hp = gethostbyaddr((char *)&from->sin_addr,
		sizeof(struct in_addr), from->sin_family)) == NULL) {
		dprintf(D_FULLDEBUG, "gethostbyaddr failed. errno= %d\n",errno);
		EXCEPT("Can't find host name");
	}

	rip->r_clientmachine = strdup(hp->h_name);

	dprintf(D_ALWAYS, "Got start_job_request from %s\n",
		rip->r_clientmachine);
	/* TODO: Use resource_param */
	MachineContext = resmgr_context(rid);

	/* Read in job facts and booleans */
	job_context = create_context();
	if (!xdr_context(xdrs,job_context)) {
		dprintf( D_ALWAYS, "Can't receive context from shadow\n");
		return;
	}
	if (!xdrrec_skiprecord(xdrs)) {
		dprintf(D_ALWAYS, "Can't receive context from shadow\n");
		return;
	}

	dprintf(D_JOB, "JOB_CONTEXT:\n");
	if (DebugFlags & D_JOB) {
		display_context(job_context);
	}
	dprintf(D_JOB, "\n");

	dprintf(D_MACHINE, "MACHINE_CONTEXT:\n");
	if (DebugFlags & D_MACHINE) {
		display_context(MachineContext);
	}
	dprintf(D_MACHINE, "\n");

	if (rip->r_state != NO_JOB) {
		dprintf(D_ALWAYS, "State != NO_JOB\n");
		(void)reply(xdrs, NOT_OK);
		ABORT;
	}

	/*
	 * See if machine and job meet each other's requirements, if so
	 * start the job and tell shadow, otherwise refuse and clean up
	 */
	if (evaluate_bool("START",&start,MachineContext,job_context) < 0) {
		start = 0;
	}
	/* fvdl XXX sometimes fails somehow */
	if (evaluate_bool("JOB_REQUIREMENTS",&job_reqs,MachineContext,job_context) < 0) {
		job_reqs = 0;
	}
	evaluate_string("Owner", &RemoteUser, job_context, job_context);
	dprintf(D_ALWAYS, "Remote job owner is %s\n", RemoteUser);
	evaluate_string("JobId", &JobId, job_context, job_context);
	dprintf(D_ALWAYS, "Remote job owner is %s\n", RemoteUser);
	/* fvdl XXX sometimes bad timeouts for capabilities */
	if (rip->r_capab == NULL || strcmp(rip->r_capab, check_string)) {
		capability_verify = 0;
	}
	free(check_string);
	if (!start || !job_reqs || !capability_verify) {
		dprintf( D_ALWAYS, "start = %d, job_reqs = %d, capability_verify = %d\n"
				, start, job_reqs, capability_verify );
		(void)reply(xdrs, NOT_OK);
		ABORT;
	}

	/* We could have been asked to run an alternate version of the
	 * starter which is not listed in our config file.  If so, it
	 * would show up as NULL here
	 */
	if (rip->r_starter == NULL) {
		dprintf(D_ALWAYS, "Alternate starter not found in config file\n");
		(void)reply(xdrs, NOT_OK);
		ABORT;
	}

	if (resource_allocate(rid, 1, 1) < 0) {
		ABORT;
	}

	resmgr_changestate(rid, JOB_RUNNING);
	polls = 0;
	insert_context("ClientMachine", rip->r_clientmachine, rip->r_context);

	tmp.type = STRING;
	tmp.s_val = RemoteUser;
	store_stmt( build_expr("RemoteUser", &tmp), rip->r_context);
	rip->r_user = RemoteUser;

	tmp.type = STRING;
	tmp.s_val = JobId;
	store_stmt(build_expr("JobId", &tmp), rip->r_context);

	if (!reply(xdrs,OK)) {
		ABORT;
	}
	rip->r_jobcontext = job_context;

	stRec.version_num = VERSION_FOR_FLOCK;
	stRec.ports.port1 = create_port(&sock_1);
	stRec.ports.port2 = create_port(&sock_2);

	/* send name of server machine: dhruba */ 
	if (get_machine_name(official_name) == -1) {
		dprintf(D_ALWAYS,"Error in get_machine_name\n");
		(void)close(sock_1);
		(void)close(sock_2);
		return -1;
	}
	/* fvdl XXX fails for machines with multiple interfaces */
	stRec.server_name = (char *)strdup(official_name);

	/* send ip_address of server machine: dhruba */ 
	if (get_inet_address(&stRec.ip_addr) == -1) {
		dprintf(D_ALWAYS,"Error in get_machine_name\n");
		(void)close(sock_1);
		(void)close(sock_2);
		return -1;
	}

	xdrs->x_op = XDR_ENCODE;
	if (!xdr_startd_info(xdrs,&stRec)) {
		(void)close(sock_1);
		(void)close(sock_2);
		return -1;
	}

	if (!xdrrec_endofrecord(xdrs,TRUE)) {
		(void)close(sock_1);
		(void)close(sock_2);
		return -1;
	}

	/* Wait for connection to port1 */
	len = sizeof frm;
	memset((char *)&frm,0, sizeof frm);
	while((fd_1=tcp_accept_timeout(sock_1,(struct sockaddr *)&frm,&len,150)) < 0 ) {
		if (fd_1 != -3) {  /* tcp_accept_timeout returns -3 on EINTR */
			if (fd_1 == -2) {
				dprintf(D_ALWAYS,"accept timed out\n");
				(void)close(sock_1);
				(void)close(sock_2);
				return -1;
			} else {
				dprintf(D_ALWAYS,"tcp_accept_timeout returns %d, errno=%d\n",
						fd_1, errno);
				(void)close(sock_1);
				(void)close(sock_2);
				return -1;
			}
		}
	}
	(void)close(sock_1);

	/* Wait for connection to port2 */
	len = sizeof frm;
	memset((char *)&frm,0,sizeof frm);
	while((fd_2 = tcp_accept_timeout(sock_2,(struct sockaddr *)&frm,&len,150)) < 0 ) {
		if (fd_2 != -3) {  /* tcp_accept_timeout returns -3 on EINTR */
			if (fd_2 == -2) {
				dprintf(D_ALWAYS,"accept timed out\n");
				(void)close(sock_2);
				return -1;
			} else {
				dprintf(D_ALWAYS,"tcp_accept_timeout returns %d, errno=%d\n",
						fd_2, errno);
				(void)close(sock_2);
				return -1;
			}
		}
	}
	(void)close(sock_2);

	/* Now find out what host we're talking to */
	if ((hp = gethostbyaddr((char *)&frm.sin_addr,sizeof(struct in_addr),
		    frm.sin_family)) == NULL) {
		dprintf( D_ALWAYS, "Can't find host name\n" );
		return -1;
	}

	if (!rip->r_jobcontext || evaluate_int("UNIVERSE", &universe,
			rip->r_jobcontext, rip->r_context) == -1) {
		universe = STANDARD;
		dprintf(D_ALWAYS,
			"Default universe (%d) since not in context \n", 
				universe);
	} else {
		dprintf(D_ALWAYS, "Got universe (%d) from JobContext\n",
			universe);
	}

	if (universe == VANILLA) {
		dprintf(D_ALWAYS,"Startd using *_VANILLA control params\n");
	} else {
		dprintf(D_ALWAYS,"Startd using standard control params\n");
	}

	rip->r_universe = universe;

	/*
	 * Put the JobUniverse into the machine context so that the negotiator
	 * can take it into account for preemption (i.e. dont preempt vanilla
	 * jobs).
	 *
	 * TODO: store universe in resource parameters; extract one level up.
	 */
	tmp.type = INT;
	tmp.i_val = universe;
	store_stmt(build_expr("JobUniverse",&tmp), rip->r_context);

	ji.ji_hname = hp->h_name;
	ji.ji_sock1 = fd_1;
	ji.ji_sock2 = fd_2;
	resource_activate(rid, 1, NULL, 1, NULL, &ji); /* XXX */

	/* change_states( JOB_RUNNING ); TODO? */
	/* Polls = 0; TODO ?*/

	/* PREEMPTION : dhruba */
	insert_context("ClientMachine", rip->r_clientmachine, rip->r_context);
	tmp.type = INT;
	tmp.i_val = (int)time((time_t *)0);
	store_stmt( build_expr("JobStart",&tmp), rip->r_context);
	store_stmt( build_expr("LastPeriodicCkpt",&tmp), rip->r_context);
	tmp.type = STRING;
	tmp.s_val = rip->r_user;
	store_stmt(build_expr("RemoteUser", &tmp), rip->r_context);
	event_timeout();
	return 0;
}

/*
 * Block a resource (XXX or the whole startd?)
 */
static int
command_block(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	return 0;
}

/*
 * Unblock a resource (XXX or the whole startd?)
 */
static int
command_unblock(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	dprintf(D_ALWAYS, "command_unblock called.\n");
	return 0;
}

static int
command_suspend(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	dprintf(D_ALWAYS, "command_suspend called.\n");
	return 0;
}

static int
command_continue(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	dprintf(D_ALWAYS, "command_continue called.\n");
	return 0;
}

static int
command_pcheckpoint(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	resource_info_t *rip;
	ELEM tmp;

	dprintf(D_ALWAYS, "pcheckpoint called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;
	event_pcheckpoint(rid, NO_JID, NO_TID);
	tmp.type = INT;
	tmp.i_val = (int)time((time_t *)0);
	store_stmt(build_expr("LastPeriodicCkpt", &tmp), rip->r_context);
	return 0;
}

static int
command_kill(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	dprintf(D_ALWAYS, "command_kill called.\n");
	return event_killjob(rid, NO_JID, NO_TID);
}

static int
command_vacate(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	dprintf(D_ALWAYS, "command_vacate called.\n");
	event_vacate(rid, NO_JID, NO_TID);
	return 0;
}

static int
command_matchinfo(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	resource_info_t *rip;
	char *str,*str1; 

	dprintf(D_ALWAYS, "command_matchinfo called\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "matchinfo: bad resource.\n");
		return -1;
	}
	str = (char *)(malloc(sizeof(char)*SIZE_OF_CAPABILITY_STRING));

	/* receive the capability from the negotiator */
	/* XXXXXX THIS SUCKS. should check from where it's coming. */
	if (!rcv_string(xdrs, &str, TRUE)) {
		dprintf(D_ALWAYS, "Failed to get mag_string from negotiator\n");
		free(str);
		return;
	}

	/*
	 * If magic_string (the capability string) is not NULL, throw away
	 * the newly received capability. An assumption is made here that
	 * magic_string will be NULL if there is no waiting to be claimed.
	 * We can make this assumption because we initialize magic_string
	 * to NULL and already set it to NULL when a timeout on a capability
	 * occur. Weiru
	 */
	if (rip->r_capab) {
		dprintf(D_FULLDEBUG,
			"Capability (%s) has %d seconds before timing out",
				rip->r_capab,
				capab_timeout - (time(NULL) - rip->r_captime));
		dprintf(D_ALWAYS, "Throw match (%s) away.\n", str);
		free(str);
		return;
	}

	if (rip->r_claimed == TRUE) {
		dprintf(D_FULLDEBUG,
			"Already claimed, ignore match (%s)\n", str);
		free(str);
		return;
	}

	str1 = (char *)strchr(str,'#');
	*str1 = '\0';
	rip->r_capab = str;
	dprintf(D_ALWAYS, "Received match %s\n", rip->r_capab);
	rip->r_captime = time(NULL);
	return 0;
}

static int
command_reqservice(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	char *check_string = NULL;
	int cmd;
	resource_info_t *rip;

	dprintf(D_ALWAYS, "command_reqservice called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	if (!rcv_string(xdrs,&check_string,TRUE)) {
		free(check_string);
		dprintf(D_ALWAYS, "Can't receive cap from schedd-agent\n");
		return -1;
	} else {
		dprintf(D_FULLDEBUG,
		  "Received capability from schedd agent (%s)\n", check_string);
		if (rip->r_capab != NULL && !strcmp(rip->r_capab, check_string))
		{
			rip->r_claimed = TRUE;
			xdrs->x_op = XDR_ENCODE;
			cmd = ACCEPTED;
			dprintf(D_ALWAYS, "Request accepted.\n");
		} else {
			cmd = REJECTED;
			if (rip->r_capab != NULL)
				dprintf(D_ALWAYS,
					"Request rejected, cap is (%s)\n",
					rip->r_capab);
			else
				dprintf(D_ALWAYS,
					"Request rejected, cap is (null)\n");
		}

		if (!snd_int(xdrs, cmd, TRUE)) {
			dprintf(D_ALWAYS, "Failed to send cmd to agent.\n");
			return -1;
		}

		if (cmd == ACCEPTED) {
			if (!rcv_string(xdrs,&rip->r_client,FALSE)) {
				dprintf(D_ALWAYS,
					"Can't receive schedd string.\n");
				return;
			}

			if (!rcv_int(xdrs,&rip->r_interval,TRUE)) {
				dprintf(D_ALWAYS,
					"Can't receive alive interval\n");
				return;
			}
		}
	}
	return 0;
}

static int
command_relservice(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	resource_info_t *rip;
	char *recv_string;

	dprintf(D_ALWAYS, "command_relservice called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	if (rip->r_claimed == TRUE) {
		recv_string = (char *)malloc(SIZE_OF_CAPABILITY_STRING);
		if (rcv_string(xdrs,&recv_string,TRUE)) {
			dprintf(D_FULLDEBUG, "received %s, magic %s\n",
				recv_string, rip->r_capab);
			if (rip->r_capab && !strcmp(rip->r_capab, recv_string)){
				dprintf(D_ALWAYS, "Match %s terminated.\n",
					rip->r_capab);
				free(rip->r_capab);
				free(rip->r_client);
				rip->r_capab = NULL;
				rip->r_client = NULL;
				rip->r_claimed = FALSE;
			}
		} else
			dprintf(D_ALWAYS, "Couldn't receive cap for relserv\n");
		free(recv_string);
	}
			
	return 0;
}

static int
command_x_event(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	dprintf(D_ALWAYS, "command_x_event called.\n");
	Last_X_Event = time((time_t *)0);
	event_timeout();
	return 0;
}

static int
command_alive(xdrs, from, rid)
	XDR *xdrs;
	struct sockaddr_in *from;
	resource_id_t rid;
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "command_alive called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	rip->r_receivetime = time(NULL);
	return 0;
}
