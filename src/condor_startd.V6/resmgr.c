#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "condor_types.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "sched.h"

#include "cdefs.h"
#include "event.h"
#include "state.h"
#include "resource.h"
#include "resmgr.h"

extern CONTEXT *template_context;
static char *_FileName_;

extern EXPR *build_expr();

/*
 * Higher level routines to manage a collection of resources.
 * XXX - DON'T do this using lineair searches..
 */

static resource_info_t *resources;
static int nresources;
static fd_set cur_fds;

static int resmgr_vacateone	__P((resource_info_t *));
static int resmgr_addfd		__P((resource_info_t *));
static int resmgr_command	__P((resource_info_t *));

int
resmgr_init()
{
	int nres, i, index;
	resource_name_t *rnames;
	resource_id_t rid;
	resource_param_t param;

	// collect all resources that were listed in the config file
	// and their number
	resource_names(&rnames, &nres);
	dprintf(D_ALWAYS, "resmgr_init: %d resources.\n", nres);
	resources = (resource_info_t *)calloc(nres, (sizeof (resource_info_t)));
	for (i = index = 0; i < nres; i++) {
		dprintf(D_ALWAYS, "opening resource '%s'\n", rnames[i]);
		if (resource_open(rnames[i], &rid) < 0)
			continue;
		resources[index].r_rid = rid;
		dprintf(D_ALWAYS, "resource %d has id '%s'\n", index,
			resources[index].r_rid);
		resources[index].r_state = NO_JOB;
		if (resource_params(rid, NO_JID, NO_TID,
		    &resources[index].r_param, NULL) < 0)
			continue;
		resources[index].r_name = rnames[i];
		resources[index].r_pid = NO_PID;
		resources[index].r_claimed = FALSE;
		resources[index].r_context = create_context();
		resources[index].r_jobcontext = NULL;
		resources[index].r_capab = NULL;
		resources[index].r_interval = 0;
		resources[index].r_receivetime = 0;
		resources[index].r_universe = STANDARD;
		dprintf(D_ALWAYS, "create_context returned %x\n",
			resources[index].r_context);
		resources[index].r_port = create_port(&resources[index].r_sock);
		resource_initcontext(&resources[index]);
		/* XXX following must be last in this initialization */
		resource_context(&resources[index]);
		index++;
	}
	nresources = index;
	dprintf(D_ALWAYS, "nresources set to %d\n", nresources);
	return 0;
}

int
resmgr_setsocks(fds)
	fd_set *fds;
{
	FD_ZERO(&cur_fds);
	resmgr_walk(resmgr_addfd);
	*fds = cur_fds;
	return 0;
}

int
resmgr_call(fds)
	fd_set *fds;
{
	dprintf(D_ALWAYS, "resmgr_call()\n");
	cur_fds = *fds;
	return resmgr_walk(resmgr_command);
}

int
resmgr_add(rid, rinfop)
	resource_id_t rid;
	resource_info_t *rinfop;
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (resource_isused(resources[i])) {
			resources[i] = *rinfop;
			break;
		}
	}
	return i == nresources ? -1 : 0;
}

int
resmgr_del(rid)
	resource_id_t rid;
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (!resource_isused(resources[i]))
			continue;
		if (!resource_ridcmp(resources[i].r_rid, rid)) {
			resource_markunused(resources[i]);
			break;
		}
	}
	return i == nresources ? -1 : 0;
}


resource_info_t *
resmgr_getbyrid(rid)
	resource_id_t rid;
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (!resource_ridcmp(resources[i].r_rid, rid))
			return &resources[i];
	}
	return (resource_info_t *)0;
}

resource_info_t *
resmgr_getbyname(rname)
	resource_name_t rname;	
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (!resource_rnamecmp(resources[i].r_name, rname))
			return &resources[i];
	}
	return (resource_info_t *)0;
}

resource_info_t *
resmgr_getbypid(pid)
	int pid;
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (resources[i].r_pid == pid)
			return &resources[i];
	}
	return (resource_info_t *)0;
}

int
resmgr_walk(func)
	int (*func) __P((resource_info_t *));
{
	int i;

	for (i = 0; i < nresources; i++)
		func(&resources[i]);
	return 0;
}

int
resmgr_vacateall(void)
{
	return resmgr_walk(resmgr_vacateone);
}

static int
resmgr_vacateone(rinfop)
	resource_info_t *rinfop;
{
	resource_id_t rid;

	rid = rinfop->r_rid;
	return resource_event(rid, NO_JID, NO_TID, EV_VACATE);
}

static int
resmgr_addfd(rip)
	resource_info_t *rip;
{
	FD_SET(rip->r_sock, &cur_fds);
	return 0;
}

static int
resmgr_command(rinfop)
	resource_info_t *rinfop;
{
	dprintf(D_ALWAYS, "resmgr_command(%x)\n", rinfop);
	if (FD_ISSET(rinfop->r_sock, &cur_fds)) {
		call_incoming(rinfop->r_sock, SOCK_STREAM, rinfop->r_rid);
		FD_CLR(rinfop->r_sock , &cur_fds);
	}
	dprintf(D_ALWAYS, "resmgr_command(%x) exiting\n", rinfop);
	return 0;
}

CONTEXT *
resmgr_context(rid)
	resource_id_t rid;
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return NULL;

	return rip->r_context;
}

char *
state_to_string(state)
	int state;
{
	switch (state) {
	case NO_JOB:
		return "NoJob";
	case JOB_RUNNING:
		return "Running";
	case KILLED:
		return "Killed";
	case CHECKPOINTING:
		return "Checkpointing";
	case SUSPENDED:
		return "Suspended";
	case BLOCKED:
		return "Blocked";
	case SYSTEM:
		return "System";
	}
	return "Unknown";
}

void
resmgr_changestate(rid, new_state)
	resource_id_t rid;
	int new_state;
{
	CONTEXT *cp;
	ELEM tmp;
	char *name;
	int start = FALSE;
	int claimed;
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return;

	cp = rip->r_context;
	claimed = rip->r_claimed;

	switch (new_state) {
	case NO_JOB:
		evaluate_bool("START", &start, rip->r_context,template_context);
		if(start && claimed == FALSE ) {
			set_machine_status(M_IDLE);
		} else {
			set_machine_status(USER_BUSY);
		}
		insert_context("ClientMachine", "", cp);
		break;
	case JOB_RUNNING:
		set_machine_status(JOB_RUNNING);
		break;
	case KILLED:
		set_machine_status(KILLED);
		break;
	case CHECKPOINTING:
		set_machine_status(CHECKPOINTING);
		break;
	case SUSPENDED:
		set_machine_status(SUSPENDED);
		break;
	case BLOCKED:
		set_machine_status(BLOCKED);
		break;
	case SYSTEM:
		set_machine_status(SYSTEM);
		break;
	default:
		EXCEPT("Change states, unknown state (%d)", new_state);
	}

	name = state_to_string(new_state);
	rip->r_state = new_state;

	tmp.type = STRING;
	tmp.s_val = name;
	store_stmt(build_expr("State",&tmp), cp);

	tmp.type = INT;
	tmp.i_val = (int)time( (time_t *)0 );
	store_stmt(build_expr("EnteredCurrentState",&tmp), cp);
}
