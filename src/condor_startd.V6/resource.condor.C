#include "condor_common.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <utmp.h>
#include <sys/param.h>

#include "condor_types.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_attributes.h"
#include "condor_mach_status.h"
#include "sched.h"
#include "util_lib_proto.h"
#include "condor_uid.h"

#include "resource.h"
#include "resmgr.h"
#include "state.h"
#include "event.h"

extern int Last_X_Event;
extern char *exec_path;
extern char *Starter;
extern int run_benchmarks;

extern "C" int resource_initAd(resource_info_t* rip);
extern "C" char *param(char*);
extern "C" char *sin_to_string(sockaddr_in*);
extern "C" char *get_host_cell();
extern "C" int event_killall(resource_id_t rid, job_id_t jid, task_id_t tid);
extern "C" int event_pcheckpoint(resource_id_t rid,job_id_t jid,task_id_t tid);
extern "C" int event_killjob(resource_id_t rid, job_id_t jid, task_id_t tid);
extern "C" int event_new_proc_order(resource_id_t rid, job_id_t jid, task_id_t tid);

extern "C" int calc_virt_memory();
extern "C" float calc_load_avg();
extern "C" int dhry_mips();
extern "C" int clinpack_kflops();

char *kbd_dev=NULL;
char *mouse_dev=NULL;

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

static void check_perms(void);
static void cleanup_execute_dir(void);
static int calc_disk(void);
static void calc_idle_time(resource_info_t *, int &, int &);
static int calc_ncpus(void);
static int tty_idle_time(char *);

#if !defined(OSF1)
static int tty_pty_idle_time(void);
#else
extern "C" time_t tty_pty_idle_time();
#endif

static float get_load_avg(void);
static int kill_starter(int, int);
extern "C" int event_vacate(resource_id_t, job_id_t, task_id_t);
static int event_block(resource_id_t, job_id_t, task_id_t);
static int event_unblock(resource_id_t, job_id_t, task_id_t);
static int event_start(resource_id_t, job_id_t, task_id_t);
extern "C" int event_suspend(resource_id_t, job_id_t, task_id_t);
extern "C" int event_continue(resource_id_t, job_id_t, task_id_t);
extern "C" int event_checkpoint(resource_id_t, job_id_t, task_id_t);
extern "C" int event_exited(resource_id_t, job_id_t, task_id_t);
static char *get_full_hostname(void);
static void init_static_info(resource_info_t *);
static int exec_starter(char *, char *, int, int);



int
resource_init()
{
	kbd_dev = param("KBD_DEVICE");
	mouse_dev = param("MOUSE_DEVICE");

	check_perms();
	cleanup_execute_dir();
	return 0;
}

int
resource_names(resource_name_t** rnames, int* nres)
{
	char *rlist, *p, *name;
	char **rn;
	int n = 0, i = 0;
	char hostname[512];
	char **tpp;

	p = rlist = param("RESOURCE_LIST");

	/*
	 * If RESOURCE_LIST is not specified, use the hostname as the
	 * only available resource.
	 */
	if (p == NULL) {
		if (gethostname(hostname, sizeof hostname) < 0)
			return -1;
		tpp = (char **)malloc(sizeof (char *));
		*tpp = strdup(hostname);
		*rnames = tpp;
		*nres = 1;
		return 0;
	}

	do {
		while (*p && (*p == ' ' || *p == '\t'))
			p++;
		if (*p) {
			n++;
			while (*p && *p != ',')
				p++;
			if (*p)
				p++;
		}
	} while (*p);

	rn = (char **)calloc(n, sizeof (char *));
	p = rlist;
	do {
		while (*p && (*p == ' ' || *p == '\t'))
			p++;
		if (*p) {
			name = p;
			while (*p && *p != ',')
				p++;
			if (*p) {
				*p = '\0';
				p++;
			}
			rn[i++] = name;
		}
	} while (*p);

	*rnames = rn;
	*nres = n;

	return 0;
}

int
resource_open(resource_name_t name, resource_id_t* id)
{
	/*
	 * XXX this should check if it's already open, this is kind of
	 * ridiculous.
	 */
	*id = name;
	return 0;
}


/* Re-calculate all dynamic parameters */
int
resource_params(resource_id_t rid, job_id_t jid, task_id_t tid, 
				resource_param_t* old, resource_param_t* New)
{
	resource_timeout_params(rid, jid, tid, old, New);
	resource_update_params(rid, jid, tid, old, New);
}


/* Re-calculate parameters for a given resource for things we need to
   know at every timeout */

int
resource_timeout_params(resource_id_t rid, job_id_t jid, task_id_t tid, 
				resource_param_t* old, resource_param_t* New)
{
	resource_info_t *rip;

	rip = resmgr_getbyrid(rid);
	/*
	 * Can't set any parameters for now.
	 */
	if (New != NULL)
		return -1;

	if (rid != NO_RID) {
		old->res.res_load = get_load_avg();
		calc_idle_time(rip, old->res.res_idle, old->res.res_console_idle);
			/* These are just here as a place holder in case some day
			 we decide to do something in this function if we are not
			 passed a valid Resource ID, but are passed a valid Job or
			 Task ID -- Derek Wright 8/22/97 */
	} else if (jid != NO_JID) {
	} else if (tid != NO_TID) {
	}
	return 0;
}

/* Re-calculate parameters for a given resource for things we only
   need to know when we're updating the central manager */
int
resource_update_params(resource_id_t rid, job_id_t jid, task_id_t tid, 
				resource_param_t* old, resource_param_t* New)
{
	resource_info_t *rip;

	rip = resmgr_getbyrid(rid);
	/*
	 * Can't set any parameters for now.
	 */
	if (New != NULL)
		return -1;

	if (rid != NO_RID) {
		old->res.res_memavail = calc_virt_memory();
		dprintf( D_FULLDEBUG, "Swap space: %d\n", old->res.res_memavail );
		old->res.res_diskavail = calc_disk();
		dprintf( D_FULLDEBUG, "Disk space: %d\n", old->res.res_diskavail );

		/* Perform CPU Benchmarks only if desired in config_file 
		 * _and_ only if we've been idle for quite some time or we've
		 * have not computed them for the first time yet.
		 */
		if ( run_benchmarks ) {
			if ( get_machine_status() == M_IDLE ) {
				(old->res.res_consecutive_idle)++;
			} else {
				old->res.res_consecutive_idle = 0;
			}
			if ((old->res.res_mips == 0) ||
				(old->res.res_consecutive_idle >= 5)) {
				int new_mips_calc = dhry_mips();
				int new_kflops_calc = clinpack_kflops();
				// don't recompute right away
				old->res.res_consecutive_idle = 0;
				if ( old->res.res_mips == 0 ) {
					old->res.res_mips = new_mips_calc;
					old->res.res_kflops = new_kflops_calc;
				} else {
					// compute a weighted average
					old->res.res_mips = (old->res.res_mips * 3 + 
										 new_mips_calc) / 4;
					old->res.res_kflops = (old->res.res_kflops * 3 + 
										   new_kflops_calc) / 4;
				}
				dprintf(D_FULLDEBUG,"recalc:DHRY_MIPS=%d,CLINPACK KFLOPS=%d\n",
						old->res.res_mips, old->res.res_kflops);
			}
		}

			/* These are just here as a place holder in case some day
			 we decide to do something in this function if we are not
			 passed a valid Resource ID, but are passed a valid Job or
			 Task ID -- Derek Wright 8/22/97 */
	} else if (jid != NO_JID) {
	} else if (tid != NO_TID) {
	}
	return 0;
}

int
resource_allocate(resource_id_t rid, int njobs, int ntasks)
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return -1;
	if (rip->r_pid != NO_PID)
		return -1;
	rip->r_pid = getpid();
	return 0;
}

int
resource_activate(resource_id_t rid, int njobs, job_id_t* jobs, 
				  int ntasks, task_id_t* tasks, start_info_t* jobinfo)
{
	int pid;
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	pid = exec_starter(rip->r_starter, jobinfo->ji_hname, 
					   jobinfo->ji_sock1,
					   jobinfo->ji_sock2);

	if (rip->r_pid != getpid()) {
		/*
		 * Huh?
		 */
		dprintf(D_ALWAYS, "Resource allocated by unknown pid.\n");
		exit(1);
	}
	rip->r_pid = pid;
	return 0;
}

int
resource_free(resource_id_t rid)
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	if( rip->r_client ) {
		if( ! rip->r_timed_out ) {
				// If the schedd/startd connection didn't time out,
				// tell the schedd and accountant we're terminating
				// the match
			vacate_client( rid );
		}
		free(rip->r_client);
		rip->r_client = NULL;
	}

	if( rip->r_capab ) {
		free(rip->r_capab);
		rip->r_capab = NULL;
	}

	if( rip->r_clientmachine ) {
		free(rip->r_clientmachine);
		rip->r_clientmachine = NULL;
	}

	if( rip->r_user ) {
		free(rip->r_user);
		rip->r_user = NULL;
	}

	if (rip->r_jobclassad) {
		delete rip->r_jobclassad;
		rip->r_jobclassad = NULL;
	}

	if (rip->r_pid != NO_PID && rip->r_pid != getpid()) {
		kill_starter( rip->r_pid, SIGTSTP );
		resmgr_changestate(rip->r_rid, CHECKPOINTING);
	} else {
		resmgr_changestate(rip->r_rid, NO_JOB);
	}
	
	rip->r_claimed = FALSE;

	return 0;
}


int
resource_event(resource_id_t rid, job_id_t jid, task_id_t tid, event_t ev)
{
	resource_info_t *rip;
  
	if (!(rip = resmgr_getbyrid(rid)))
		return -1;
	switch (ev) 
		{
		case EV_VACATE:
			return event_vacate(rid, jid, tid);
		case EV_BLOCK:
			return event_block(rid, jid, tid);
		case EV_UNBLOCK:
			return event_unblock(rid, jid, tid);
		case EV_START:
			return event_start(rid, jid, tid);
		case EV_SUSPEND:
			return event_suspend(rid, jid, tid);
		case EV_CONTINUE:
			return event_continue(rid, jid, tid);
		case EV_KILL:
			return event_killjob(rid, jid, tid);
		case EV_EXITED:
			return event_exited(rid, jid, tid);
		}
	return 0;
}

int 
resource_initAd(resource_info_t* rip)
{
	init_static_info(rip);

		// Save the orig. requirement expression for later use.
	rip->r_origreqexp = new char[512];
	rip->r_origreqexp[0] = '\0';	// need a NULL cuz PrintToStr does strcat
	((rip->r_classad)->Lookup(ATTR_REQUIREMENTS))->PrintToStr(rip->r_origreqexp);

	return (rip->r_classad)->Insert("State=\"NoJob\"");
}


// Update all dynamic info in the classad associated with the given rip 
ClassAd*
resource_classad(resource_info_t* rip)
{
	resource_timeout_classad( rip );
	return resource_update_classad( rip );
}


//fill in the classAd of rip with dynamic info needed at every timeout
ClassAd* 
resource_timeout_classad(resource_info_t* rip)
{
	resource_param_t *rp;
	ClassAd *cp;

	char line[80];

	rp = &rip->r_param;
	cp = rip->r_classad;
  
	resource_timeout_params(rip->r_rid, NO_JID, NO_TID, rp, NULL);
 
	sprintf(line,"LoadAvg=%f",rp->res.res_load);
	cp->Insert(line);

	sprintf(line,"KeyboardIdle=%d",rp->res.res_idle);
	cp->Insert(line); 
  
	// ConsoleIdle cannot be determined on all platforms; thus, only
	// advertise if it is not -1.
	if ( rp->res.res_console_idle != -1 ) {
		sprintf(line,"ConsoleIdle=%d",rp->res.res_console_idle);
		cp->Insert(line); 
	}
	return cp;
}


//fill in the classAd of rip with dynamic info needed only for updating CM
ClassAd* 
resource_update_classad(resource_info_t* rip)
{
	resource_param_t *rp;
	ClassAd *cp;

	char line[80];

	rp = &rip->r_param;
	cp = rip->r_classad;

	resource_update_params(rip->r_rid, NO_JID, NO_TID, rp, NULL);
 
	sprintf(line,"VirtualMemory=%d",rp->res.res_memavail);
	cp->Insert(line); 

	sprintf(line,"Disk=%d",rp->res.res_diskavail);
	cp->Insert(line); 
  
	// KFLOPS and MIPS are only conditionally computed; thus, only
	// advertise them if we computed them.
	if ( rp->res.res_kflops != 0 ) {
		sprintf(line,"KFLOPS=%d",rp->res.res_kflops);
		cp->Insert(line);
	}
	if ( rp->res.res_mips != 0 ) {
		sprintf(line,"MIPS=%d",rp->res.res_mips);
		cp->Insert(line);
	}

		// Hack for Miron.  Locally evaluate REQUIREMENTS expression.
		// If it evaluates to False, stick that in the classad and set
		// RESOURCE_STATE=Owner.  It it evalutes to True, stick True
		// into classad and set RESOURCE_STATE=Condor.  Only if it
		// evaluates to Undefined do we actually ship out the
		// expression.  Undefined means we should be in Condor state. 
	ClassAd ad;
	int tmp;
	cp->Insert(rip->r_origreqexp);
	if( cp->EvalBool(ATTR_REQUIREMENTS,&ad,tmp) == 0 ) {
			// Evaluation failed for some reason, say we're in
			// Condor state and leave requirements alone.
		sprintf(line,"ResourceState=condor");
		cp->Insert(line);
	} else {
		if( tmp ) {
			sprintf(line,"Requirements=True");
			cp->Insert(line);
			sprintf(line,"ResourceState=condor");
			cp->Insert(line);
		} else {
			sprintf(line,"Requirements=False");
			cp->Insert(line);
			sprintf(line,"ResourceState=owner");
			cp->Insert(line);
		}
	}
	return cp;
}


int
resource_close(resource_id_t rid)
{
	return 0;
}

static void
check_perms()
{
	struct stat st;
	mode_t mode;

	if (stat(exec_path, &st) < 0) {
		EXCEPT("stat exec path");
	}

	if ((st.st_mode & 0777) != 0777) {
		dprintf(D_FULLDEBUG, "Changing permission on %s\n", exec_path);
		mode = st.st_mode | 0777;
		if (chmod(exec_path, mode) < 0) {
			EXCEPT("chmod exec path");
		}
	}
}

static void
cleanup_execute_dir()
{
	char *execute;
	char buf[2048];

	execute = strrchr(exec_path, '/');
	if (execute == NULL) {
		execute = exec_path;
	} else {
		execute++;
	}

	if(strcmp("execute", execute) != 0) {
		EXCEPT("EXECUTE parameter (%s) must end with 'execute'.\n",
			   exec_path);
	}

	sprintf(buf, "/bin/rm -rf %.256s/condor_exec* %.256s/dir_*",
			exec_path, exec_path);
	system(buf);
}

static int
exec_starter(char* starter, char* hostname, int main_sock, int err_sock)
{
	int i;
	int pid;
	int n_fds = getdtablesize();
	int omask;
	ELEM tmp;
	sigset_t set;

	/*
	  omask = sigblock(sigmask(SIGTSTP));
	  */
#if defined(Solaris)
	if (sigprocmask(SIG_SETMASK,0,&set)  == -1)
		{EXCEPT("Error in reading procmask, errno = %d\n", errno);}
	for (i=0; i < MAXSIG; i++) block_signal(i);
#else
	omask = sigblock(-1);
#endif
	if ((pid = fork()) < 0) {
		EXCEPT( "fork" );
	}

	if (pid) {	/* The parent */
		/*
		  (void)sigblock(omask);
		  */
#if defined(Solaris)
		if ( sigprocmask(SIG_SETMASK, &set, 0)  == -1 )
			{EXCEPT("Error in setting procmask, errno = %d\n", errno);}
#else
		(void)sigsetmask(omask);
#endif
		(void)close(main_sock);
		(void)close(err_sock);

		dprintf(D_ALWAYS,
				"exec_starter( %s, %d, %d ) : pid %d\n",
				hostname, main_sock, err_sock, pid);
		dprintf(D_ALWAYS, "execl(%s, \"condor_starter\", %s, 0)\n",
				starter, hostname);
	} else {	/* the child */
		/*
		 * N.B. The child is born with TSTP blocked, so he can be
		 * sure to set up his handler before accepting it.
		 */

		/*
		 * This should not change process groups because the
		 * condor_master daemon may want to do a killpg at some
		 * point...
		 *
		 *	if( setpgrp(0,getpid()) ) {
		 *		EXCEPT( "setpgrp(0, %d)", getpid() );
		 *	}
		 */
			/*
			 * We _DO_ want to create the starter with it's own
			 * process group, since when KILL evaluates to True, we
			 * don't want to kill the startd as well.  The master no
			 * longer kills via a process group to do a quick clean
			 * up, it just sends signals to the startd and schedd and
			 * they, in turn, do whatever quick cleaning needs to be 
			 * done. 
			 */
		if( setsid() < 0 ) {
			EXCEPT( "setsid()" );
		}

		if (dup2(main_sock,0) < 0) {
			EXCEPT("dup2(%d,%d)", main_sock, 0);
		}
		if (dup2(main_sock,1) < 0) {
			EXCEPT("dup2(%d,%d)", main_sock, 1);
		}
		if (dup2(err_sock,2) < 0) {
			EXCEPT("dup2(%d,%d)", err_sock, 2);
		}

		for(i = 3; i<n_fds; i++) {
			(void) close(i);
		}
		/*
		 * Starter must be exec'd as root so it can change to client's
		 * uid and gid.
		 */
		set_root_priv();
		(void)execl(starter, "condor_starter", hostname, 0);
		EXCEPT( "execl(%s, condor_starter, %s, 0)", starter, hostname );
	}
	return pid;
}

static int
calc_disk()
{
	int free_disk;

	free_disk = free_fs_blocks(exec_path);
	if (free_disk <= 512)
		return 0;
	return free_disk - 512;
}

int
event_vacate(resource_id_t rid, job_id_t jid,task_id_t tid)
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "Called vacate_order()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "vacate: could not find resource.\n");
		return -1;
	}
		// Relinquish the match, checkpoint the job, set claimed to
		// false, if the resource is claimed, tell the schedd and
		// accountant the match is going away, etc.
	resource_free( rip->r_rid );
	return 0;
}

int
event_block(resource_id_t rid, job_id_t jid,task_id_t tid)
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "Called vacate_order()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "vacate: could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "block: resource not allocated.\n");
		return -1;
	}

	/* TODO: change states */

	return kill_starter(rip->r_pid, SIGUSR1);
}

int
event_unblock(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	/* TODO: change states to SUSPENDED */
}

int
event_start(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	return 0;
}

int
event_suspend(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;
	int rval;

	dprintf(D_ALWAYS, "Called event_suspend()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "suspend: could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "suspend: resource not allocated.\n");
		return -1;
	}

	rval = kill_starter(rip->r_pid, SIGUSR1);
	if( rval == 0 ) {
		resmgr_changestate(rip->r_rid, SUSPENDED);
	}
	return rval;
}

int
event_continue(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;
	int rval;

	dprintf(D_ALWAYS, "Called event_continue()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "continue: could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "continue: resource not allocated.\n");
		return -1;
	}

	rval = kill_starter(rip->r_pid, SIGCONT);
	if( rval == 0 ) {
		resmgr_changestate(rip->r_rid, JOB_RUNNING);
	}
	return rval;

}

int
event_killjob(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "Called event_killjob()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "checkpoint: could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "kill: resource not allocated.\n");
		return -1;
	}

		// Don't tranisition to state NO_JOB here.  We're still
		// claimed by the schedd.  We only go to NO_JOB when we
		// relinquish a match.  -Derek 9/10/97

	return kill_starter(rip->r_pid, SIGINT);
}

int
event_new_proc_order(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "Called event_new_proc_order()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "Could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "Resource not allocated.\n");
		return -1;
	}

	return kill_starter(rip->r_pid, SIGHUP);
}

int
event_pcheckpoint(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "checkpoint: could not find resource.\n");
		return -1;
	}

	return kill_starter(rip->r_pid, SIGUSR2);
}

int
event_killall(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;
	priv_state priv;

	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "killall: could not find resource.\n");
		return -1;
	}

	if ( rip->r_pid == NO_PID ) {
		dprintf(D_ALWAYS, "killall: could not find starter pid/pgrp.\n");
		return -1;
	}

	dprintf(D_FULLDEBUG, "***** About to kill( -%d, SIGKILL ) *****\n", 
			rip->r_pid );

	priv = set_root_priv();

	if( kill( -(rip->r_pid), SIGKILL ) < 0 ) {
		EXCEPT("kill( starter_process_group ) euid = %d\n", geteuid());
	}

	set_priv( priv );
}

/* The starter has exited.  Careful, you're inside a signal handler! */
int
event_exited(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;

	if( !(rip = resmgr_getbyrid(rid)) ) {
		dprintf(D_ALWAYS, "event_exited: could not find resource.\n");
		return -1;
	}

		// We know the starter exited, so we can finally get rid of r_pid
	rip->r_pid = NO_PID;

	if( rip->r_claimed == FALSE ) {
		resmgr_changestate(rip->r_rid, NO_JOB);
	} else {
		resmgr_changestate(rip->r_rid, CLAIMED);
	}

	cleanup_execute_dir();

	// We're in a signal handler, this is not safe!!! -Derek 9/11/97 
    // event_timeout();
}

static int
kill_starter(int pid, int signo)
{
	struct stat st;
	int 		ret = 0;
	priv_state	priv;

	if ( pid <= 0 ) {
		dprintf(D_ALWAYS,"Invalid pid (%d) in kill_starter, returning.\n", 
				pid );
		return -1;
	}

	dprintf(D_ALWAYS,"In kill_starter() with pid %d, signo %d\n", pid, signo);

	for (errno = 0; (ret = stat(Starter, &st)) < 0; errno = 0) {
		if (errno == ETIMEDOUT)
			continue;
		EXCEPT("kill_starter(%d, %d): cannot stat <%s> - errno = %d\n",
			   pid, signo, Starter, errno);
	}

	priv = set_root_priv();

	if (signo != SIGSTOP && signo != SIGCONT)
		ret = kill(pid, SIGCONT);
	ret = kill(pid, signo);

	set_priv(priv);

	return ret;
}

#if defined(OSF1)
#	define MAXINT ((1<<30)-1)
#else
#	define MAXINT ((1<<31)-1)
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if defined(OSF1)
static char *UtmpName = "/var/adm/utmp";
static char *AltUtmpName = "/etc/utmp";
#elif defined(LINUX)
static char *UtmpName = "/var/run/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#else
static char *UtmpName = "/etc/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#endif

/* calc_idle_time fills in user_idle and console_idle with the number
 * of seconds since there has been activity detected on any tty or on
 * just the console, respectively.  therefore, console_idle will always
 * be equal to or greater than user_idle.  think about it.  also, note
 * that user_idle and console_idle are passed by reference.  Also,
 * on some platforms console_idle is always -1 because it cannot reliably
 * be determined.
 */
static void
calc_idle_time(resource_info_t* rip, int & user_idle, int & console_idle)
{
	int tty_idle;
	int now;
	struct utmp utmp;
	ELEM temp;
#if defined(HPUX9) || defined(LINUX)	/*Linux uses /dev/mouse	*/
	int i;
	char devname[MAXPATHLEN];
#endif

	console_idle = -1;  // initialize

	user_idle = tty_pty_idle_time();

	if (kbd_dev) {
		tty_idle = tty_idle_time(kbd_dev);
		user_idle = MIN(tty_idle, user_idle);
		console_idle = tty_idle;
	}

	if (mouse_dev) {
		tty_idle = tty_idle_time(mouse_dev);
		user_idle = MIN(tty_idle, user_idle);
		if (console_idle != -1)
			console_idle = MIN(tty_idle, console_idle);
	}
/*
** On HP's "/dev/hil1" - "/dev/hil7" are mapped to input devices such as
** a keyboard, mouse, touch screen, digitizer, etc.We can then conveniently
** stat those, and avoid having to run the condor_kbd at all.
** Note: the "old-style" hp keyboards go to /dev/hil?, but the newer
** PC 101-key style keyboard gets mapped to /dev/ps2kbd and /dev/ps2mouse
*/
#if defined(HPUX9)
	for(i = 1; i <= 7; i++) {
		sprintf(devname, "hil%d", i);
		tty_idle = tty_idle_time(devname);
		if (i == 1)
			console_idle = tty_idle;
		else
			console_idle = MIN(tty_idle, console_idle);
	}
	tty_idle = tty_idle_time("ps2kbd");
	console_idle = MIN(tty_idle, console_idle);
	tty_idle = tty_idle_time("ps2mouse");
	console_idle = MIN(tty_idle, console_idle); 

	user_idle = MIN(console_idle, user_idle);
#endif

/*
 * Linux usally has a /dev/mouse linked to ttySx or psaux, etc.  Use it!
 *
 * stat() will follow the link to the apropriate ttySx
 */
#if defined(LINUX)
	sprintf(devname, "mouse");
	tty_idle = tty_idle_time(devname);
	user_idle = MIN( tty_idle, user_idle );
#endif

	now = (int)time((time_t *)0);
	user_idle = MIN(now - Last_X_Event, user_idle);

	/* if Last_X_Event != 0, then condor_kbdd told us someone did something
	 * on the console. but always believe a /dev device over the kbdd */
	if ( (console_idle == -1) && (Last_X_Event != 0) ) {
		console_idle = now - Last_X_Event;
	}

	dprintf(D_FULLDEBUG, "Idle Time: user= %d , console= %d seconds\n",
			user_idle,console_idle);
	
	return;
}

#if !defined(OSF1) /* in tty_idle.OSF1.C for that platform */
#include <limits.h>
static int
tty_pty_idle_time()
{
	FILE *fp;
	int tty_idle;
	int answer = INT_MAX;
	int now;
	static int saved_now;
	static int saved_idle_answer = -1;
	struct utmp utmp;

	if ((fp=fopen(UtmpName,"r")) == NULL) {
		if ((fp=fopen(AltUtmpName,"r")) == NULL) {
			EXCEPT("fopen of \"%s\"", UtmpName);
		}
	}

	while (fread((char *)&utmp, sizeof utmp, 1, fp)) {
#if defined(AIX31) || defined(AIX32) || defined(IRIX331) || defined(IRIX53) || defined(LINUX)
		if (utmp.ut_type != USER_PROCESS)
#else
			if (utmp.ut_name[0] == '\0')
#endif
				continue;

		tty_idle = tty_idle_time(utmp.ut_line);
		answer = MIN(tty_idle, answer);
	}
	fclose(fp);

	/* Here we check to see if we are about to return INT_MAX.  If so,
	 * we recompute via the last pty access we knew about.  -Todd, 2/97 */
	now = (int)time( (time_t *)0 );
	if ( (answer == INT_MAX) && ( saved_idle_answer != -1 ) ) {
		answer = (now - saved_now) + saved_idle_answer;
		if ( answer < 0 )
			answer = 0; /* someone messed with the system date */
	} else {
		if ( answer != INT_MAX ) {
			/* here we are returning an answer we discovered; save it */
			saved_idle_answer = answer;
			saved_now = now;
		}
	}

	return answer;
}
#endif

static int
tty_idle_time(char* file)
{
	struct stat buf;
	long now;
	long answer;
	static char pathname[100] = "/dev/";

	strcpy( &pathname[5], file );
	if (stat(pathname,&buf) < 0) {
		dprintf( D_FULLDEBUG, "Error on stat(%s,0x%x), errno = %d\n",
				 pathname, &buf, errno );
		buf.st_atime = 0;
	}

	now = (int)time((time_t *)0);
	answer = now - buf.st_atime;

	/*
	 * This only happens if somebody screws up the date on the
	 * machine :-<
	 */
	if (answer < 0)
		answer = 0;

	return answer;
}

static void init_static_info(resource_info_t* rip)
{
	char	*ptr;
	char	*host_cell;
	char	*addrname;
	char tmp[80];
	int phys_memory = -1;
	ClassAd* config_classad = rip->r_classad;
	struct sockaddr_in sin;
	int len = sizeof sin;
  
	sprintf(tmp,"Name=\"%s\"",rip->r_name);
	config_classad->Insert(tmp);
  
	host_cell = get_host_cell();
	if(host_cell) {
		sprintf(tmp,"AFS_CELL=\"%s\"",host_cell);
		config_classad->Insert(tmp);
		dprintf(D_ALWAYS, "AFS_Cell = \"%s\"\n", host_cell);
	} else {
		dprintf(D_ALWAYS, "AFS_Cell not set\n");
	}
  
	/*
	 * If the uid domain is not defined, accept uid's only from this
	 * machine.
	 */
	if ((ptr = param("UID_DOMAIN")) == NULL) {
		ptr = get_full_hostname();
	}
  
	/*
	 * If the uid domain is defined as "*", accept uid's from anybody.
	 */
	if( ptr[0] == '*' ) {
		ptr = "";
	}

	dprintf(D_ALWAYS, "UidDomain = \"%s\"\n", ptr);
  
	sprintf(tmp,"UidDomain=\"%s\"",ptr);
	config_classad->Insert(tmp);
  
	/*
	 * If the filesystem domain is not defined, we share files only
	 * with our own machine.
	 */
	if ((ptr = param("FILESYSTEM_DOMAIN")) == NULL) {
		ptr = get_full_hostname();
	}
  
	/*
	 * If the filesystem domain is defined as "*", assume we share
	 * files with everybody.
	 */
	if( ptr[0] == '*' )
		ptr = "";
  
	sprintf(tmp,"FilesystemDomain=\"%s\"",ptr);
	config_classad->Insert(tmp);
	dprintf(D_ALWAYS, "FilesystemDomain = \"%s\"\n", tmp);
  
	sprintf(tmp,"Cpus=%d",calc_ncpus());
	config_classad->Insert(tmp);
  
	
	/*
	 * NEST has nothing to do with the filesystem, but we set it here
	 * instead of in update_central_manager() because it only needs 
	 * to run at startup and at a reconfig (i.e. a HUP signal)
	 * XXX
	 */
  
	if(param("NEST"))
		strcpy(tmp,param("NEST"));
	else
		strcpy(tmp,"NOTSET");
	config_classad->Insert(tmp);
	dprintf(D_ALWAYS, "Nest = \"%s\"\n", tmp);
  
	phys_memory = calc_phys_memory();

	if (phys_memory > 0) {
		sprintf(tmp,"Memory=%d",phys_memory);
		config_classad->Insert(tmp);
	}
  
	getsockname(rip->r_sock, (struct sockaddr *)&sin, &len);
	get_inet_address(&sin.sin_addr);
	
	addrname = strdup( sin_to_string(&sin) );
	sprintf(tmp,"%s=\"%s\"",(char *)ATTR_STARTD_IP_ADDR,addrname);
	config_classad->Insert(tmp);
	free(addrname);

	// Quick hack to refer to the START expression as the machine
	// classad requirements.  Eventually we should do some more
	// intelligent local evaluation or some such here.  -Todd 7/97
	config_classad->SetRequirements("Requirements = START");
}

static char *
get_full_hostname()
{
	static char answer[MAX_STRING];
	char this_host[MAX_STRING];
	struct hostent *host_ptr;

	/* determine our own host name */
	if (gethostname(this_host,MAX_STRING) < 0) {
		EXCEPT("gethostname");
	}

	/* Look up out official host information */
	if ((host_ptr = gethostbyname(this_host)) == NULL) {
		EXCEPT("gethostbyname");
	}

	/* copy out the official hostname */
	strcpy(answer, host_ptr->h_name);

	return answer;
}

static int
calc_ncpus()
{
#ifdef sequent
	int     cpus = 0;
	int     eng;

	if ((eng = tmp_ctl(TMP_NENG,0)) < 0) {
		perror( "tmp_ctl(TMP_NENG,0)" );
		exit(1);
	}

	while (eng--) {
		if( tmp_ctl(TMP_QUERY,eng) == TMP_ENG_ONLINE )
			cpus++;
	}
	return cpus;
#elif defined(Solaris)
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(IRIX53)
	return sysmp(MP_NPROCS);
#else sequent
	return 1;
#endif sequent
}

static float
get_load_avg()
{
	float val = calc_load_avg();
	dprintf(D_ALWAYS, "calc_load_avg -> %f\n", val);
	return val;
}


