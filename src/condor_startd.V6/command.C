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
#include "CondorPrefExps.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_attributes.h"

#include "resource.h"
#include "resmgr.h"
#include "CondorPendingJobs.h"

static char *_FileName_ = __FILE__;

int Last_X_Event;

extern "C" EXPR *build_expr __P((char *, ELEM *));
//extern "C" int resource_allocate(resource_id_t rid, int njobs, int ntasks);
extern "C" resource_info_t *resmgr_getbyrid(resource_id_t rid);
extern "C" ClassAd *resmgr_context(resource_id_t rid);
extern "C" int create_port(int *sock);
extern "C" int reply(Sock *sock, int answer);

extern char *AlternateStarter[10];
extern char *PrimaryStarter;
extern char *Starter;
extern int polls;
extern int capab_timeout;

extern "C" int event_checkpoint(resource_id_t, job_id_t, task_id_t);

int command_startjob(Sock *, struct sockaddr_in *, resource_id_t,bool JobAlreadyGot=false);
static int command_block __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_unblock __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_suspend __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_continue __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_kill	__P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_vacate __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_matchinfo __P((Sock *, struct sockaddr_in *, resource_id_t));
int command_reqservice __P((Sock *, struct sockaddr_in *, resource_id_t));
int MatchInfo __P((resource_info_t*,char*));
static int command_relservice __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_x_event __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_alive __P((Sock *, struct sockaddr_in *, resource_id_t));
static int command_pcheckpoint	__P((Sock *, struct sockaddr_in *, resource_id_t));

/*
 * Functions to handle commands from incoming requests.
 */

int
command_main(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	int cmd;

	dprintf(D_ALWAYS, "command_main(%x, %x, %x)\n", 
		sock, from, rid);
	
	sock->decode();
	if (!sock->code(cmd)) {
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
		return command_startjob(sock, from, rid);
	case START_FRGN_JOB:
		Starter = PrimaryStarter;
		dprintf(D_ALWAYS, "starter = '%s'\n", Starter);
		return command_startjob(sock, from, rid);
	case CKPT_FRGN_JOB:
		return command_vacate(sock, from, rid);
	case PCKPT_FRGN_JOB:
		return command_pcheckpoint(sock, from, rid);
	case KILL_FRGN_JOB:
		return command_kill(sock, from, rid);
	case X_EVENT_NOTIFICATION:
		return command_x_event(sock, from, rid);
	case MATCH_INFO:
		dprintf (D_PROTOCOL, "## 4. Match information command\n");
		return command_matchinfo(sock, from, rid);
	case REQUEST_SERVICE:
		dprintf (D_PROTOCOL, "## 5. Request service command\n");
		return command_reqservice(sock, from, rid);
	case RELINQUISH_SERVICE:
		dprintf (D_PROTOCOL, "## 7. Relinquish service command\n");
		return command_relservice(sock, from, rid);
	case ALIVE:
		dprintf (D_PROTOCOL, "## 6. Alive command\n");
		return command_alive(sock, from, rid);
	case REQ_NEW_PROC:
	default:
		dprintf(D_ALWAYS, "Can't handle this.\n");
	}
}

#define ABORT \
		if (rip->r_jobcontext) {			 \
			{delete (rip->r_jobcontext);}    \
			rip->r_jobcontext = NULL;		\
		}									 \
		if (rip->r_clientmachine) {		 \
			free(rip->r_clientmachine);	 \
			rip->r_clientmachine = NULL;	 \
		}					 \
		return -1;
/*
 * Start a job. Allocate the resource and use resource_activate to
 * get things going.
 */
int
command_startjob(Sock *sock,struct sockaddr_in* from, resource_id_t rid,
		 bool JobRecdEarlier=false)
{

	int start = 1,job_reqs = 1, capability_verify = 1;
	ClassAd	*job_context = NULL, *MachineContext;
	char tmp[80];
	char *check_string = NULL;
	PORTS ports;
	int sock_1, sock_2;
	int fd_1, fd_2;
	struct sockaddr_in frm;
	int len = sizeof frm;
	struct hostent *hp;
	char official_name[MAXHOSTLEN], *ptr = official_name, *ClientMachine;
	StartdRec stRec;
	start_info_t ji;	/* XXXX */
	resource_name_t rname;
	resource_info_t *rip;
	char RemoteUser[80], JobId[80];
	int universe, job_cluster, job_proc;

	dprintf(D_ALWAYS, "command_startjob called.\n");

	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "startjob: unknown resource.\n");
		CondorPendingJobs::DequeueJob(); // When did we queue the job???
		return -1;
	}

	rip->r_starter = Starter;
	
//	let ReliSock allocate the memory
//	check_string = (char *)malloc(SIZE_OF_CAPABILITY_STRING * sizeof(char));

	if(JobRecdEarlier) dprintf(D_ALWAYS,"Job Ad already recd\n");
	else dprintf(D_ALWAYS,"Job Ad yet to be recd\n");
	if(!JobRecdEarlier)
	{
	  if (!sock->code(check_string)) {
	    free(check_string);
	    dprintf(D_ALWAYS, "Can't receive capability from shadow\n");
	    if(CondorPendingJobs::AreTherePendingJobs(rid)) CondorPendingJobs::DequeueJob();
	    return -1;
	  }
	  dprintf(D_ALWAYS, "capability %s received from shadow\n", check_string);

	  if ((hp = gethostbyaddr((char *)&from->sin_addr,
				  sizeof(struct in_addr), from->sin_family)) == NULL) {
	    dprintf(D_FULLDEBUG, "gethostbyaddr failed. errno= %d\n",errno);
	    if(CondorPendingJobs::AreTherePendingJobs(rid)) CondorPendingJobs::DequeueJob();
	    EXCEPT("Can't find host name");
	  }

	  if(CondorPendingJobs::AreTherePendingJobs(rid)) 
	  {
	    if(rip->r_state==JOB_RUNNING)
	      dprintf(D_ALWAYS,"start_job request when a job is already running\n");
	    else
	    {
	      dprintf(D_ALWAYS,"Something wrong..Pending job when there is no job running: current state is %d\n", rip->r_state);
	      CondorPendingJobs::DequeueJob();
	      (void)reply(sock, NOT_OK);
	      ABORT;
	    }
	    // store client machine
	    CondorPendingJobs::SetClientMachine(rid,hp->h_name);
	    dprintf(D_ALWAYS, "Got start_job_request from %s\n",hp->h_name);
	  }
	  else  
	  {  
	    rip->r_clientmachine = strdup(hp->h_name);
	    dprintf(D_ALWAYS, "Got start_job_request from %s\n",
		    rip->r_clientmachine);
	  }

	  /* TODO: Use resource_param */
	  MachineContext = resmgr_context(rid);

	  /* Read in job facts and booleans */
	  job_context = new ClassAd;
	  if (!job_context->get(*sock)) {
	    dprintf( D_ALWAYS, "Can't receive job classad from shadow\n");
	    CondorPendingJobs::DequeueJob();
		// we should delete job_context here, right?
	    return -1;
	  }
	  if (!sock->eom()) {
	    dprintf(D_ALWAYS, "Can't receive job classad from shadow\n");
	    CondorPendingJobs::DequeueJob();
		// we should delete job_context here, right?
	    return -1;
	  }
	}
	else
	{
	  job_context = CondorPendingJobs::JobAd(rid);
	  CondorPendingJobs::DequeueJob();
	  if(rip->r_state==JOB_RUNNING)
	  {
	    dprintf(D_ALWAYS,"State==JOB_RUNNING when JOb is already recd\n");
	    reply(sock,NOT_OK);
	    return -1;
	  }
	  if(!job_context)
	  {
	    dprintf(D_ALWAYS,"Couldnt get job Ad!!\n");
	    reply(sock,NOT_OK);
	    return -1;
	  }
	}
	dprintf(D_JOB, "JOB_CONTEXT:\n");
	if (DebugFlags & D_JOB) {
	  job_context->fPrint(stdout);
	}
	dprintf(D_JOB, "\n");
	  
	dprintf(D_MACHINE, "MACHINE_CONTEXT:\n");
	if (DebugFlags & D_MACHINE) {
	  job_context->fPrint(stdout);
	}
	dprintf(D_MACHINE, "\n");
	
	if ((rip->r_state != NO_JOB)&&(rip->r_state!=JOB_RUNNING)) {
	  dprintf(D_ALWAYS, "State != NO_JOB && State!=JOB_RUNNING\n");
	  if(CondorPendingJobs::AreTherePendingJobs(rid)) CondorPendingJobs::DequeueJob();
	  (void)reply(sock, NOT_OK);
	  ABORT;
	}
	else if((rip->r_state==NO_JOB)&&(CondorPendingJobs::AreTherePendingJobs(rid)))
	  {
	    dprintf(D_ALWAYS, "State == NO_JOB && there are pending jobs\n");
	    CondorPendingJobs::DequeueJob();
	    (void)reply(sock, NOT_OK);
	    ABORT;
	  }

	  /*
	   * See if machine and job meet each other's requirements, if so
	   * start the job and tell shadow, otherwise refuse and clean up
	   */
	  if(MachineContext->EvalBool("START",job_context,start) == 0)
	  {
	    start = 0;
	  }
	  /* fvdl XXX sometimes fails somehow */
	
	  if(job_context->EvalBool("Requirements",MachineContext,job_reqs) == 0)
	  {
	    job_reqs = 0;
	  }
	  job_context->EvalString(ATTR_OWNER,job_context,RemoteUser);
	  dprintf(D_ALWAYS, "Remote job owner is %s\n", RemoteUser);
	  job_context->EvalInteger(ATTR_CLUSTER_ID,job_context,job_cluster);
	  job_context->EvalInteger(ATTR_PROC_ID,job_context,job_proc);
	  sprintf(JobId, "%d.%d", job_cluster, job_proc);
	  dprintf(D_ALWAYS, "Remote job ID is %s\n", JobId);
	  
	  // check whether any PrefExp satisfies this job
	  tTier JobTier = -1;
	  CondorPrefExps::MatchingPrefExp(MachineContext,job_context,JobTier);
	  dprintf(D_ALWAYS,"Job satisfies Pref Tier %d\n",JobTier);
	  
	  if(CondorPendingJobs::AreTherePendingJobs(rid)) 
	  {
	    if((!CondorPendingJobs::CapabString(rid))||(strcmp(CondorPendingJobs::CapabString(rid),check_string)))
	    {
	      capability_verify = 0;
	    }
	  }
	  else  
	  /* fvdl XXX sometimes bad timeouts for capabilities */
	    if (rip->r_capab == NULL || strcmp(rip->r_capab, check_string)) {
	      if(rip->r_capab)
	      {
		dprintf(D_ALWAYS,"Capability check failed: r_capab=%s != check_string=%s\n",rip->r_capab,check_string);
	      }
	      else dprintf(D_ALWAYS,"r_capab is NULL!!!\n");
	      capability_verify = 0;
	    }
	  free(check_string);
	  if (!start || !job_reqs || !capability_verify) {
	    dprintf( D_ALWAYS, "start = %d, job_reqs = %d, capability_verify = %d\n"
		    , start, job_reqs, capability_verify );
	    if(CondorPendingJobs::AreTherePendingJobs(rid)) CondorPendingJobs::DequeueJob();
	    (void)reply(sock, NOT_OK);
	    ABORT;
	  }

	  /* We could have been asked to run an alternate version of the
	   * starter which is not listed in our config file.  If so, it
	   * would show up as NULL here
	   */
	  if (rip->r_starter == NULL) {
	    dprintf(D_ALWAYS, "Alternate starter not found in config file\n");
	    CondorPendingJobs::DequeueJob();
	    (void)reply(sock, NOT_OK);
	    ABORT;
	  }


	  if(rip->r_state==JOB_RUNNING)
	  {
	    ClassAd* job = rip->r_jobcontext; // current job
	    if(!job)
	    {
	      dprintf(D_ALWAYS,"Current job is NULL!!\n");
	      ABORT;
	    }
	    tTier CurJobTier = -1;
	    CondorPrefExps::MatchingPrefExp(MachineContext,job,CurJobTier);
	    dprintf(D_ALWAYS,"Current Job satisfies Pref Tier %d\n",CurJobTier);
	    
	    if((JobTier<CurJobTier)&&(CurJobTier>=0))
	    {
	      // Higher priority job
	      CondorPendingJobs::StoreRespondInfo(sock,from,rid);
	      CondorPendingJobs::RecordJob(rid,job);

	      event_vacate(rid, NO_JID, NO_TID);	    

	      // dont reply to shadow
	      return 0;
	    }
	    else
	      {
		dprintf(D_ALWAYS,"Current job is of higher or equal priority!\n");
		(void)reply(sock, NOT_OK);
		CondorPendingJobs::DequeueJob();
		return -1;
	      }
	  }

	if(!CondorPendingJobs::AreTherePendingJobs(rid)) 
	{
	  if (resource_allocate(rid, 1, 1) < 0) {
	    ABORT;
	  }
	}
	else
	{
	  dprintf(D_ALWAYS,"There shld be no pending jobs when this routine is called\n");
	  CondorPendingJobs::DequeueJob();
	  return -1;
	}
	if (!(rip = resmgr_getbyrid(rid))) 
	  {
	    dprintf(D_ALWAYS, "startjob: unknown resource.\n");
	    return -1;
	  }
	resmgr_changestate(rid, JOB_RUNNING);
	polls = 0;
	
	sprintf(tmp,"ClientMachine=\"%s\"",rip->r_clientmachine);
	(rip->r_context)->Insert(tmp);
	
	rip->r_user = strdup(RemoteUser);
	sprintf(tmp,"RemoteUser=\"%s\"",rip->r_user);
	(rip->r_context)->Insert(tmp);
	
	sprintf(tmp,"JobId=\"%s\"",JobId);
	(rip->r_context)->Insert(tmp);
	
	if (!reply(sock,OK)) {
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
	
	sock->encode();
	if (!sock->code(stRec)) {
	  (void)close(sock_1);
	  (void)close(sock_2);
	  return -1;
	}

	if (!sock->eom()) {
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

	if (!rip->r_jobcontext || ((rip->r_jobcontext)->EvalInteger("UNIVERSE",rip->r_context,universe)==0))
	{
	  universe = STANDARD;
	  dprintf(D_ALWAYS,
		  "Default universe (%d) since not in context \n", 
		  universe);
	} 
	else 
	{
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

	sprintf(tmp,"%s=%d",ATTR_JOB_UNIVERSE, universe);
	(rip->r_context)->Insert(tmp);

	ji.ji_hname = hp->h_name;
	ji.ji_sock1 = fd_1;
	ji.ji_sock2 = fd_2;
	resource_activate(rid, 1, NULL, 1, NULL, &ji); /* XXX */

	/* change_states( JOB_RUNNING ); TODO? */
	/* Polls = 0; TODO ?*/

	/* PREEMPTION : dhruba */
	sprintf(tmp,"ClientMachine=\"%s\"",rip->r_clientmachine);
	(rip->r_context)->Insert(tmp);

	int tempo = (int)time((time_t *)0);
	sprintf(tmp,"JobStart=%d",tempo);
	(rip->r_context)->Insert(tmp);

	sprintf(tmp,"LastPeriodicCkpt=%d",tempo);
	(rip->r_context)->Insert(tmp);

	sprintf(tmp,"RemoteUser=\"%s\"",rip->r_user);
	(rip->r_context)->Insert(tmp);

	event_timeout();
	return 0;
}

/*
 * Block a resource (XXX or the whole startd?)
 */
static int
command_block(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	return 0;
}

/*
 * Unblock a resource (XXX or the whole startd?)
 */
static int
command_unblock(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	dprintf(D_ALWAYS, "command_unblock called.\n");
	return 0;
}

static int
command_suspend(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	dprintf(D_ALWAYS, "command_suspend called.\n");
	return 0;
}

static int
command_continue(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	dprintf(D_ALWAYS, "command_continue called.\n");
	return 0;
}

static int
command_pcheckpoint(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	resource_info_t *rip;
	char tmp[80];

	dprintf(D_ALWAYS, "pcheckpoint called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;
	event_pcheckpoint(rid, NO_JID, NO_TID);

	sprintf(tmp,"LastPeriodicCkpt=%d",(int)time((time_t *)0));
	(rip->r_context)->Insert(tmp);

	return 0;
}

static int
command_kill(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	dprintf(D_ALWAYS, "command_kill called.\n");
	return event_killjob(rid, NO_JID, NO_TID);
}

static int
command_vacate(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	dprintf(D_ALWAYS, "command_vacate called.\n");
	event_vacate(rid, NO_JID, NO_TID);
	return 0;
}

// This routine is called _AFTER_ a _BONAFIDE_ capability string 
// is recd from the negotiator
int 
MatchInfo(resource_info_t* rip, char* str)
{

  /*
   * If magic_string (the capability string) is not NULL, throw away
   * the newly received capability. An assumption is made here that
   * magic_string will be NULL if there is no waiting to be claimed.
   * We can make this assumption because we initialize magic_string
   * to NULL and already set it to NULL when a timeout on a capability
   * occur. Weiru
   */
  if (rip->r_capab) {
    dprintf(D_PROTOCOL | D_FULLDEBUG,
	    "Capability (%s) has %d seconds before timing out\n",
	    rip->r_capab,
	    capab_timeout - (time(NULL) - rip->r_captime));
    dprintf(D_ALWAYS, "Throw match (%s) away.\n", str);
    free(str);
    return -1;
  }

  if (rip->r_claimed == TRUE) {
    dprintf(D_ALWAYS, "Already claimed, ignore match (%s)\n", str);
    free(str);
    return -1;
  }
  
  
  rip->r_capab = str;
  rip->r_claimed = TRUE;
  dprintf(D_ALWAYS, "Received match %s\n", rip->r_capab);
  rip->r_captime = time(NULL);
  return 0;
}

static int
command_matchinfo(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
  resource_info_t *rip = NULL;
  char *str = NULL;
  char* str1 = NULL;

  dprintf(D_ALWAYS, "command_matchinfo called\n");
  if (!(rip = resmgr_getbyrid(rid))) {
    dprintf(D_ALWAYS, "matchinfo: bad resource.\n");
	sock->eom();
    return -1;
  }

//  let Sock allocate the string
//  str = (char *)(malloc(sizeof(char)*SIZE_OF_CAPABILITY_STRING));
  
  /* receive the capability from the negotiator */
  /* XXXXXX THIS SUCKS. should check from where it's coming. */
  if (!sock->code(str)) {
    dprintf(D_ALWAYS, "Failed to get mag_string from negotiator\n");
    free(str);
	sock->eom();
    return -1;
  }
  sock->eom();
  
  str1 = (char *)strchr(str,'#');
  *str1 = '\0';

  if(rip->r_state!=NO_JOB)
  {
    ClassAd* job = rip->r_jobcontext; // current job
    if(!job)
    {
      dprintf(D_ALWAYS,"Current job is NULL!!\n");
      return -1;
    }

    if(CondorPendingJobs::AreTherePendingJobs(rid)) 
    {
      dprintf(D_ALWAYS,"Possible higher preferred job seen when one such job is already pending!\n");
      return -1;
    }

    // stash the capability for this job away

    CondorPendingJobs::AddJob(rid,str);
    return 0;
  }
  
  return MatchInfo(rip,str);
}


int
command_reqservice(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	char *check_string = NULL;
	int cmd;
	resource_info_t *rip;

	dprintf(D_ALWAYS, "command_reqservice called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	if (!sock->code(check_string)) {
		free(check_string);
		dprintf(D_ALWAYS, "Can't receive cap from schedd-agent\n");
		CondorPendingJobs::DequeueJob();
		return -1;
	} else {
		dprintf(D_ALWAYS,
		  "Received capability from schedd agent (%s)\n", check_string);
		if (!CondorPendingJobs::AreTherePendingJobs(rid)&&(rip->r_capab != NULL && !strcmp(rip->r_capab, check_string)))
		{
			rip->r_claimed = TRUE;
			cmd = OK;
			dprintf(D_ALWAYS, "Request accepted.\n");
		} 
		else if(CondorPendingJobs::AreTherePendingJobs(rid))
		{
		  char* PendingJobCapab = CondorPendingJobs::CapabString(rid);

		  dprintf(D_PROTOCOL, "## 5. Request service on a job that has been stashed away\n");
		  if(PendingJobCapab && !strcmp(PendingJobCapab,check_string))
		  {
		    rip->r_claimed = TRUE;
		    cmd = OK;
		    dprintf(D_ALWAYS, "Request accepted for job on hold.\n");
		  } 
		  else
		  {
		    cmd = NOT_OK;
		    if(PendingJobCapab)
		    {
		      dprintf(D_ALWAYS,"Request rejected, cap is (%s)\n",PendingJobCapab);
		    }
		    else
		    {
		      dprintf(D_ALWAYS, "Request rejected, cap is (null)\n");
		      CondorPendingJobs::DequeueJob();
		    }
		  }
		}
		else {
			cmd = NOT_OK;
			if (rip->r_capab != NULL) {
				dprintf(D_ALWAYS, "Request rejected, cap is (%s)\n",
					rip->r_capab);
			} else {
				dprintf(D_ALWAYS, "Request rejected, cap is (null)\n");
			}
			CondorPendingJobs::DequeueJob();
		}

		sock->end_of_message();
		sock->encode();
		if (!sock->code(cmd)) {
			dprintf(D_ALWAYS, "Failed to send cmd to agent.\n");
			return -1;
		}

		sock->end_of_message();
		sock->decode();

		if (cmd == OK) {
		  // Grab the schedd string and alive interval and
		  // stash em away if you have a pending job

		  if(CondorPendingJobs::AreTherePendingJobs(rid))
		  {
		    int Interval=0;
			if (!sock->code(rip->r_client)) {
		      dprintf(D_ALWAYS, "Can't receive schedd string.\n");
		      return -1;
		    }

			if (!sock->code(rip->r_interval)) {
		      dprintf(D_ALWAYS, "Can't receive alive interval\n");
		      return -1;
		    }
		    else
		      dprintf(D_ALWAYS,"Alive interval = %d\n",Interval);
		    CondorPendingJobs::SetClient(rid,rip->r_client);
		    CondorPendingJobs::SetAliveInterval(rid,Interval);
			sock->end_of_message();
		  }
		  else
		  {
			if (!sock->code(rip->r_client)) {
		      dprintf(D_ALWAYS, "Can't receive schedd string.\n");
		      return -1;
		    }

			if (!sock->code(rip->r_interval)) {
		      dprintf(D_ALWAYS, "Can't receive alive interval\n");
		      return -1;
		    }
			sock->end_of_message();
		  }
		    
		}
	      }
	return 0;
}

static int
command_relservice(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	resource_info_t *rip;
	char *recv_string = NULL;

	dprintf(D_ALWAYS, "command_relservice called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	if (rip->r_claimed == TRUE) {
//		let Sock allocate string
//		recv_string = (char *)malloc(SIZE_OF_CAPABILITY_STRING);
		if (sock->code(recv_string)) {
			dprintf(D_ALWAYS, "received %s, magic %s\n",
				recv_string, rip->r_capab);
			if (rip->r_capab && !strcmp(rip->r_capab, recv_string)){
				dprintf(D_ALWAYS, "Match %s terminated.\n",
					rip->r_capab);
				free(rip->r_capab);
				free(rip->r_client);
				free(rip->r_user);
				rip->r_capab = NULL;
				rip->r_client = NULL;
				rip->r_user = NULL;
				rip->r_claimed = FALSE;
			}
		} else
			dprintf(D_ALWAYS, "Couldn't receive cap for relserv\n");
		free(recv_string);
	}
			
	return 0;
}

static int
command_x_event(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	dprintf(D_ALWAYS, "command_x_event called.\n");
	Last_X_Event = time((time_t *)0);
	event_timeout();
	return 0;
}

static int
command_alive(Sock *sock, struct sockaddr_in* from, resource_id_t rid)
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "command_alive called.\n");
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	rip->r_receivetime = time(NULL);
	return 0;
}
