#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <utmp.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "condor_types.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_attributes.h"
#include "condor_fix_unistd.h"
#include "condor_mach_status.h"
#include "sched.h"
#include "util_lib_proto.h"

#include "cdefs.h"
#include "resource.h"
#include "resmgr.h"
#include "state.h"
#include "event.h"

extern int Last_X_Event;
extern int polls;
extern int polls_per_update_load;
extern int polls_per_update_kbdd;
extern char *exec_path;
extern char *Starter;

extern "C" int resource_initAd(resource_info_t* rip);
extern "C" char *param(char*);
extern "C" char *sin_to_string(sockaddr_in*);
extern "C" char *get_host_cell();
extern "C" int event_killall(resource_id_t rid, job_id_t jid, task_id_t tid);
extern "C" int event_pcheckpoint(resource_id_t rid, job_id_t jid, task_id_t tid);
extern "C" int event_killjob(resource_id_t rid, job_id_t jid, task_id_t tid);

extern "C" int calc_virt_memory();
extern "C" float calc_load_avg();

char *kbd_dev;
char *mouse_dev;

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

static void check_perms __P((void));
static void cleanup_execute_dir __P((void));
static int calc_disk __P((void));
static int calc_idle_time __P((resource_info_t *));
static int calc_ncpus __P((void));
static int tty_idle_time __P((char *));
static int tty_pty_idle_time __P((void));
static float get_load_avg __P((void));
static int kill_starter __P((int, int));
extern "C" int event_vacate(resource_id_t, job_id_t, task_id_t);
static int event_block(resource_id_t, job_id_t, task_id_t);
static int event_unblock(resource_id_t, job_id_t, task_id_t);
static int event_start(resource_id_t, job_id_t, task_id_t);
extern "C" int event_suspend(resource_id_t, job_id_t, task_id_t);
//int event_continue __P((resource_id_t, job_id_t, task_id_t));
extern "C" int event_continue(resource_id_t, job_id_t, task_id_t);
extern "C" int event_checkpoint(resource_id_t, job_id_t, task_id_t);
extern "C" int event_exited(resource_id_t, job_id_t, task_id_t);
static char *get_full_hostname __P((void));
static void init_static_info __P((resource_info_t *));
static int exec_starter __P((char *, char *, int, int));



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

int
resource_params(resource_id_t rid, job_id_t jid, task_id_t tid, 
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
		dprintf(D_ALWAYS, "load avg: %f\n", old->res.res_load);
		old->res.res_memavail = calc_virt_memory();
		old->res.res_diskavail = calc_disk();
		old->res.res_idle = calc_idle_time(rip);
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
	if (rip->r_pid == NO_PID)
		return -1;
// C H A N G E -> N Anand
	if (rip->r_jobcontext)
		delete rip->r_jobcontext;
	/* XXX boundary crossing */
	free(rip->r_clientmachine);
	rip->r_clientmachine = NULL;
	rip->r_pid = NO_PID;
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
    if (rip->r_claimed == TRUE)
      vacate_client(rid);
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

// C H A N G E -> N Anand -> Ask Rajesh
/*
int
resource_initcontext(resource_info_t* rip)
{
	ELEM tmp;

	init_static_info(rip);

	tmp.type = STRING;
	tmp.s_val = "NoJob";
	store_stmt(build_expr("State",&tmp), rip->r_context);

	return 0;
}
*/

int resource_initAd(resource_info_t* rip)
{
  init_static_info(rip);
  return (rip->r_context)->Insert("State=\"NoJob\"");
}

// C H A N G E -> N Anand 
//fill in the classAd of rip with dynamic info
ClassAd* resource_context(resource_info_t* rip)
{
  resource_param_t *rp;
  ClassAd *cp;

  char line[80];

  rp = &rip->r_param;
  cp = rip->r_context;
  
  resource_params(rip->r_rid, NO_JID, NO_TID, rp, NULL);
 
  sprintf(line,"LoadAvg=%f",rp->res.res_load);
  cp->Insert(line);

  sprintf(line,"VirtualMemory=%d",rp->res.res_memavail);
  cp->Insert(line); 

  sprintf(line,"Disk=%d",rp->res.res_diskavail);
  cp->Insert(line); 
  
  sprintf(line,"KeyboardIdle=%d",rp->res.res_idle);
  cp->Insert(line); 
  
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
#ifdef NFSFIX
		/*
		 * Starter must be exec'd as root so it can change to client's
		 * uid and gid.
		 */
		if (getuid() == 0)
			set_root_euid();
#endif NFSFIX
		(void)execl(starter, "condor_starter", hostname, 0);
#ifdef NFSFIX
		/* Must be condor to write to log files. */
		if (getuid() == 0)
			set_condor_euid();
#endif NFSFIX
		EXCEPT( "execl(%s, condor_starter, %s, 0)", starter, hostname );
	}
	return pid;
}

static int
calc_disk()
{
	int free_disk;

	free_disk = free_fs_blocks(".");
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

	if (rip->r_claimed)
		vacate_client(rip->r_rid);

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "vacate: resource not allocated.\n");
		return -1;
	}

	resmgr_changestate(rid, CHECKPOINTING);

	return kill_starter(rip->r_pid, SIGTSTP);
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

	dprintf(D_ALWAYS, "Called event_suspend()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "suspend: could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "suspend: resource not allocated.\n");
		return -1;
	}

	resmgr_changestate(rip->r_rid, SUSPENDED);

	return kill_starter(rip->r_pid, SIGUSR1);
}

int
event_continue(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;

	dprintf(D_ALWAYS, "Called event_continue()\n");
	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "continue: could not find resource.\n");
		return -1;
	}

	if (rip->r_pid == NO_PID || rip->r_pid == getpid()) {
		dprintf(D_ALWAYS, "continue: resource not allocated.\n");
		return -1;
	}

	resmgr_changestate(rip->r_rid, JOB_RUNNING);

	return kill_starter(rip->r_pid, SIGCONT);
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

	// state change changed to NO_JOB, was CHECKPOINT prev (an
	// error on th epart of prev person?) -> NA
	resmgr_changestate(rip->r_rid, NO_JOB);

	return kill_starter(rip->r_pid, SIGINT);
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

/*
 * XXX this kill function is way too harsh (why kill myself? such a shame)
 */
int
event_killall(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "kill: could not find resource.\n");
		return -1;
	}

	if (getuid() == 0)
		set_root_euid();
#if defined(HPUX9) || defined(LINUX) || defined(Solaris)
	if( kill(-getpgrp(),SIGKILL) < 0 ) {
#else
	if( kill(-getpgrp(0),SIGKILL) < 0 ) {
#endif
		EXCEPT("kill( my_process_group ) euid = %d\n", geteuid());
	}
	sigpause(0);	/* huh? */
}

int
event_exited(resource_id_t rid, job_id_t jid, task_id_t tid)
{
	resource_free(rid);
	cleanup_execute_dir();
	event_timeout();
}

static int
kill_starter(int pid, int signo)
{
	struct stat st;
	int ret = 0;

	dprintf(D_ALWAYS,"In kill_starter() with signo %d\n",signo);

	for (errno = 0; (ret = stat(Starter, &st)) < 0; errno = 0) {
		if (errno == ETIMEDOUT)
			continue;
		EXCEPT("kill_starter(%d, %d): cannot stat <%s> -- errno = %d\n",
			pid, signo, Starter, errno);
	}

#ifdef NFSFIX
	if (getuid() == 0)
		set_root_euid();
#endif
	if (signo != SIGSTOP && signo != SIGCONT)
		ret = kill(pid, SIGCONT);
	ret = kill(pid, signo);
#ifdef NFSFIX
	if (getuid() == 0)
		set_condor_euid();
#endif
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
#else
	static char *UtmpName = "/etc/utmp";
	static char *AltUtmpName = "/var/adm/utmp";
#endif

/* TODO: console_idle */
static int
calc_idle_time(resource_info_t* rip)
{
	int tty_idle;
	int user_idle;
	int console_idle = -1;
	int now;
	struct utmp utmp;
	ELEM temp;
	static int oldval = 0, kbdd_counter = 0;
#if defined(HPUX9) || defined(LINUX)	/*Linux uses /dev/mouse	*/
	int i;
	char devname[MAXPATHLEN];
#endif

	if (oldval != 0 && (rip == NULL || rip->r_state != JOB_RUNNING) &&
	    ++kbdd_counter != polls_per_update_kbdd)
		return oldval;

	user_idle = tty_pty_idle_time();
	dprintf( D_FULLDEBUG, "ttys/ptys idle %d seconds\n",  user_idle );

	if (kbd_dev) {
		tty_idle = tty_idle_time(kbd_dev);
		user_idle = MIN(tty_idle, user_idle);
		console_idle = tty_idle;
		dprintf(D_FULLDEBUG, "/dev/%s idle %d seconds\n",
			kbd_dev, tty_idle);
	}

	if (mouse_dev) {
		tty_idle = tty_idle_time(mouse_dev);
		user_idle = MIN(tty_idle, user_idle);
		if (console_idle != -1)
			console_idle = MIN(tty_idle, console_idle);
		dprintf(D_FULLDEBUG, "/dev/%s idle %d seconds\n", mouse_dev,
			tty_idle);
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

		dprintf(D_FULLDEBUG, "%s idle %d seconds\n", devname, tty_idle);
        }
	tty_idle = tty_idle_time("ps2kbd");
	console_idle = MIN(tty_idle, console_idle);
	tty_idle = tty_idle_time("ps2mouse");
	console_idle = MIN(tty_idle, console_idle); 

	user_idle = MIN(console_idle, user_idle);
#endif

/*
 * Linux usally has a /dev/mouse linked to ttySx.  Use it!
 *
 * stat() will follow the link to the apropriate ttySx
 */
#if defined(LINUX)
	sprintf(devname, "mouse");
	tty_idle = tty_idle_time(devname);
	user_idle = MIN( tty_idle, user_idle );
	dprintf( D_FULLDEBUG, "/dev/mouse idle for %d seconds\n", tty_idle);
#endif

#if 0
	/* XXX TODO */
	if (console_idle != -1) {
		temp.type = INT;
		temp.i_val = console_idle;
		store_stmt(build_expr("ConsoleIdle",&temp), cp);
	}
#endif

	now = (int)time((time_t *)0);
	user_idle = MIN(now - Last_X_Event, user_idle);

	dprintf(D_FULLDEBUG, "User Idle Time is %d seconds\n", user_idle);
	oldval = user_idle;
	return user_idle;
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
	struct utmp utmp;

	if ((fp=fopen(UtmpName,"r")) == NULL) {
		if ((fp=fopen(AltUtmpName,"r")) == NULL) {
			EXCEPT("fopen of \"%s\"", UtmpName);
		}
	}

	while (fread((char *)&utmp, sizeof utmp, 1, fp)) {
#if defined(AIX31) || defined(AIX32) || defined(IRIX331) || defined(IRIX53)
		if (utmp.ut_type != USER_PROCESS)
#else
		if (utmp.ut_name[0] == '\0')
#endif
			continue;

		tty_idle = tty_idle_time(utmp.ut_line);
		answer = MIN(tty_idle, answer);
	}
	fclose(fp);

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
  ClassAd* config_context = rip->r_context;
  struct sockaddr_in sin;
  int len = sizeof sin;
  
  sprintf(tmp,"Name=\"%s\"",rip->r_name);
  config_context->Insert(tmp);
  
  host_cell = get_host_cell();
  if(host_cell)
  {
    sprintf(tmp,"AFS_CELL=\"%s\"",host_cell);
    config_context->Insert(tmp);
    dprintf(D_ALWAYS, "AFS_Cell = \"%s\"\n", host_cell);
  }
  else
  {
    dprintf(D_ALWAYS, "AFS_Cell not set\n");
  }
  
  /*
   * If the uid domain is not defined, accept uid's only from this
   * machine.
   */
  if ((ptr = param("UID_DOMAIN")) == NULL) 
  {
    ptr = get_full_hostname();
  }
  
  /*
   * If the uid domain is defined as "*", accept uid's from anybody.
   */
  if( ptr[0] == '*' ) 
  {
    ptr = "";
  }

  dprintf(D_ALWAYS, "UidDomain = \"%s\"\n", ptr);
  
  sprintf(tmp,"UidDomain=\"%s\"",ptr);
  config_context->Insert(tmp);
  
  /*
   * If the filesystem domain is not defined, we share files only
   * with our own machine.
   */
  if ((ptr = param("FILESYSTEM_DOMAIN")) == NULL) 
  {
    ptr = get_full_hostname();
  }
  
  /*
   * If the filesystem domain is defined as "*", assume we share
   * files with everybody.
   */
  if( ptr[0] == '*' )
    ptr = "";
  
  sprintf(tmp,"FilesystemDomain=\"%s\"",ptr);
  config_context->Insert(tmp);
  dprintf(D_ALWAYS, "FilesystemDomain = \"%s\"\n", tmp);
  
  sprintf(tmp,"Cpus=%d",calc_ncpus());
  config_context->Insert(tmp);
  
	
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
  config_context->Insert(tmp);
  dprintf(D_ALWAYS, "Nest = \"%s\"\n", tmp);
  
  if (getuid() == 0)
    set_root_euid();
  phys_memory = calc_phys_memory();
  if (getuid() == 0)
    set_condor_euid();
  if (phys_memory > 0) 
  {
    sprintf(tmp,"Memory=%d",phys_memory);
    config_context->Insert(tmp);
  }
  
  addrname = (char *)malloc(32);
  getsockname(rip->r_sock, (struct sockaddr *)&sin, &len);
  get_inet_address(&sin.sin_addr);
  strcpy(addrname, sin_to_string(&sin));
  
  sprintf(tmp,"%s=\"%s\"",(char *)ATTR_STARTD_IP_ADDR,addrname);
  config_context->Insert(tmp);
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

static float
get_load_avg()
{
	static int first = 1;
	static float oldval;
	static int load_counter = 0;

	if (++load_counter != polls_per_update_load && !first)
		return oldval;
	first = 0;
	oldval = calc_load_avg();
	dprintf(D_ALWAYS, "calc_load_avg -> %f\n", oldval);
	return oldval;
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
