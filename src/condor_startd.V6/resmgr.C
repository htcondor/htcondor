#include "condor_common.h"
#include <sys/socket.h>
#include <netinet/in.h>

#if !defined(LINUX) && !defined(HPUX9)
#include <sys/select.h>
#endif

#include "condor_types.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_mach_status.h"
#include "condor_attributes.h"
#include "sched.h"

#include "event.h"
#include "state.h"
#include "resource.h"
#include "resmgr.h"

extern ClassAd* template_ClassAd;

static char *_FileName_ = __FILE__;

/*
 * Higher level routines to manage a collection of resources.
 * XXX - DON'T do this using lineair searches..
 */

static resource_info_t *resources=NULL;
static int nresources;
static fd_set cur_fds;

static int resmgr_vacateone(resource_info_t *);
static int resmgr_addfd(resource_info_t *);
static int resmgr_command(resource_info_t *);
void update_central_mgr(void);

extern "C" int resmgr_setsocks(fd_set* fds);
extern "C" int resmgr_call(fd_set* fds);
extern "C" void resmgr_changestate(resource_id_t rid, int new_state);

// collect all resources that were listed in the config file
// initialize(including the ClassAd in each resource)
//  and store them in the global resources array. The number
// of such resources (just 1 if RESOURCE_LIST i snot specified
// in the config \file is recorded by nresources.
int resmgr_init()
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
		if (resource_timeout_params(rid, NO_JID, NO_TID,
		    &resources[index].r_param, NULL) < 0)
			continue;
		if (resource_update_params(rid, NO_JID, NO_TID,
		    &resources[index].r_param, NULL) < 0)
			continue;
		resources[index].r_name = rnames[i];
		resources[index].r_pid = NO_PID;
		resources[index].r_claimed = FALSE;
		// C H A N G E -> N Anand
		resources[index].r_classad = new ClassAd(*template_ClassAd);
		resources[index].r_jobclassad = NULL;
		resources[index].r_capab = NULL;
		resources[index].r_interval = 0;
		resources[index].r_receivetime = 0;
		resources[index].r_universe = STANDARD;
		resources[index].r_timed_out = 0;
		dprintf(D_FULLDEBUG, "create_classad returned %x\n",
			resources[index].r_classad);
		resources[index].r_port = create_port(&resources[index].r_sock);
		// CHANGE -> N Anand
		resource_initAd(&resources[index]);
		/* XXX following must be last in this initialization */
		resource_timeout_classad(&resources[index]);
		resource_update_classad(&resources[index]);
		index++;
	}
	nresources = index;
	dprintf(D_ALWAYS, "nresources set to %d\n", nresources);
	return 0;
}

int
resmgr_setsocks(fd_set* fds)
{
	FD_ZERO(&cur_fds);
	resmgr_walk(resmgr_addfd);
	*fds = cur_fds;
	return 0;
}

int
resmgr_call(fd_set* fds)
{
	cur_fds = *fds;
	return resmgr_walk(resmgr_command);
}

int
resmgr_add(resource_id_t rid, resource_info_t* rinfop)
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
resmgr_del(resource_id_t rid)
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
resmgr_getbyrid(resource_id_t rid)
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (!resource_ridcmp(resources[i].r_rid, rid))
			return &resources[i];
	}
	return (resource_info_t *)0;
}

resource_info_t *
resmgr_getbyname(resource_name_t rname)
{
	int i;

	for (i = 0; i < nresources; i++) {
		if( !resource_rnamecmp(resources[i].r_name, rname) ) {
			return &resources[i];
		}
	}
	return (resource_info_t *)0;
}

resource_info_t *
resmgr_getbypid(int pid)
{
	int i;

	for (i = 0; i < nresources; i++) {
		if (resources[i].r_pid == pid) {
			return &resources[i];
		}
	}
	return (resource_info_t *)0;
}

int
resmgr_walk(int (*func)(resource_info_t*))
{
	int i;

	for (i = 0; i < nresources; i++) {
		func(&resources[i]);
	}
	return 0;
}

int
resmgr_vacateall(void)
{
	return resmgr_walk(resmgr_vacateone);
}

bool
resmgr_resourceinuse(void)
{
	int i;
	for (i = 0; i < nresources; i++) {
		if (resources[i].r_state != NO_JOB) {
			return true;
		}
	}
	return false;
}	

static int
resmgr_vacateone(resource_info_t* rinfop)
{
	resource_id_t rid;

	rid = rinfop->r_rid;
	return resource_event(rid, NO_JID, NO_TID, EV_VACATE);
}

static int
resmgr_addfd(resource_info_t* rip)
{
	FD_SET(rip->r_sock, &cur_fds);
	return 0;
}

static int
resmgr_command(resource_info_t* rinfop)
{
	if (FD_ISSET(rinfop->r_sock, &cur_fds)) {
		call_incoming(rinfop->r_sock, SOCK_STREAM, rinfop->r_rid);
		FD_CLR(rinfop->r_sock , &cur_fds);
	}
	return 0;
}

ClassAd*
resmgr_classad(resource_id_t rid)
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return NULL;

	return rip->r_classad;
}

char *
state_to_string(int state)
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
	case CLAIMED:
		return "Claimed";
	}
	return "Unknown";
}

void resmgr_changestate(resource_id_t rid, int new_state)
{
  ClassAd* cp;
  char *name;
  char tmp[80];
  int start = FALSE;
  int claimed;
  resource_info_t *rip;
  
  if (!(rip = resmgr_getbyrid(rid)))
    return;
  
  cp = rip->r_classad;
  claimed = rip->r_claimed;
  
  switch (new_state) {
  case NO_JOB:
	  cp->EvalBool(ATTR_REQUIREMENTS, template_ClassAd, start);
	  if(start && claimed == FALSE ) {
		  set_machine_status(M_IDLE);
	  } else {
		  set_machine_status(USER_BUSY);
	  }
	  cp->Delete("ClientMachine");
	  cp->Delete("RemoteUser");
	  cp->Delete("JobId");
	  cp->Delete("JobStart");
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
  case CLAIMED:
	  set_machine_status(CLAIMED);
	  break;
  default:
	  EXCEPT("Change states, unknown state (%d)", new_state);
  }
  
  name = state_to_string(new_state);
  rip->r_state = new_state;
  
  sprintf(tmp,"State=\"%s\"",name);
  cp->InsertOrUpdate(tmp);
  
  sprintf(tmp,"EnteredCurrentState=%d",(int)time((time_t*)0));
  cp->InsertOrUpdate(tmp);

  update_central_mgr();
}
