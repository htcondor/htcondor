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
#include <time.h>

#include "condor_types.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_attributes.h"
#include "sched.h"

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

extern char *param();
extern char *sin_to_string();
extern char *state_to_string();
extern char *get_host_cell();
extern float calc_load_avg();

char *kbd_dev;
char *mouse_dev;

static struct partinfo pinf[MAXPART];
static int npart;
static char *_FileName_;

static int part_info __P((void));
static int calc_disk __P((void));
static int calc_idle_time __P((resource_info_t *));
static int calc_ncpus __P((resource_info_t *));
static int tty_idle_time __P((char *));
static int tty_pty_idle_time __P((void));
static float get_load_avg __P((void));
static int kill_starter __P((int, int));
extern "C" int event_vacate(resource_id_t, job_id_t, task_id_t);
extern "C" static int event_block(resource_id_t, job_id_t, task_id_t);
extern "C" static int event_unblock(resource_id_t, job_id_t, task_id_t);
extern "C" static int event_start(resource_id_t, job_id_t, task_id_t);
extern "C" int event_suspend(resource_id_t, job_id_t, task_id_t);
extern "C" int event_continue(resource_id_t, job_id_t, task_id_t);
//int event_continue __P((resource_id_t, job_id_t, task_id_t));
extern "C" int event_checkpoint(resource_id_t, job_id_t, task_id_t);
extern "C" int event_exited(resource_id_t, job_id_t, task_id_t);
static char *get_full_hostname __P((void));
static void init_static_info __P((resource_info_t *));
static int exec_starter __P((char *, char *, int, int, char *));

int
resource_init()
{
	kbd_dev = param("KBD_DEVICE");
	mouse_dev = param("MOUSE_DEVICE");

	return 0;
}

int
resource_names(rnames, nres)
	resource_name_t **rnames;
	int *nres;
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
		part_info();
		*nres = npart;
		rn = (char **)calloc(npart, sizeof (char *));
		for (i = 0; i < npart; i++)
			rn[i] = pinf[i].p_name;
		*rnames = rn;
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
resource_open(name, id)
	resource_name_t name;
	resource_id_t *id;
{
	int i;

	for (i = 0; i < npart; i++) {
		if (!strcmp(name, pinf[i].p_name)) {
			*id = i;
			return 0;
		}
	}
	return -1;
}

int
resource_params(rid, jid, tid, old, new)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
	resource_param_t *old, *new;
{
	resource_info_t *rip;

	if (rid < 0 || rid > MAXPART)
		return -1;
	/*
	 * Can't set any parameters for now.
	 */
	if (new != NULL)
		return -1;

	part_info();

	if (rid != NO_RID) {
		old->res.res_load = pinf[rid].p_state == JOB_RUNNING ? 1.0 : 0.0;
		old->res.res_memavail = 128*1024;
		old->res.res_diskavail = calc_disk();
		old->res.res_idle = 1000000;
	} else if (jid != NO_JID) {
	} else if (tid != NO_TID) {
	}
	return 0;
}

int
resource_allocate(rid, njobs, ntasks)
	resource_id_t rid;
	int njobs, ntasks;
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
resource_activate(rid, njobs, jobs, ntasks, tasks, jobinfo)
	resource_id_t rid;
	int njobs;
	job_id_t *jobs;
	int ntasks;
	task_id_t *tasks;
	start_info_t *jobinfo;
{
	int pid;
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return -1;

	pid = exec_starter(rip->r_starter, jobinfo->ji_hname, jobinfo->ji_sock1,
			   jobinfo->ji_sock2, rip->r_name);

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
resource_free(rid)
	resource_id_t rid;
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return -1;
	if (rip->r_pid == NO_PID)
		return -1;
	if (rip->r_jobcontext)
		free_context(rip->r_jobcontext);
	/* XXX boundary crossing */
	free(rip->r_clientmachine);
	rip->r_clientmachine = NULL;
	rip->r_pid = NO_PID;
	return 0;
}

int
resource_event(rid, jid, tid, ev)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
	event_t ev;
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid)))
		return -1;
	switch (ev) {
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
	default:
	}
	return 0;
}

int
resource_initcontext(rip)
	resource_info_t *rip;
{
	ELEM tmp;

	init_static_info(rip);

	tmp.type = STRING;
	tmp.s_val = "NoJob";
	store_stmt(build_expr("State",&tmp), rip->r_context);

	return 0;
}

ClassAd *
resource_context(rip)
	resource_info_t *rip;
{
	char line[80];
	resource_param_t *rp;
	ClassAd *cp;

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

	/*
	 * State may have changed externally due to partition allocation,
	 * if we're not doing anything on it.
	 */
	if (rip->r_pid == NO_PID) {
	  sprintf(line,"State=%s",state_to_string(pinf[rip->r_rid].p_state));
	  cp->Insert(line); 
	}
	/*
	 * XXX baaad way to handle architecture.
	 */

	sprintf(line,"OpSys=%s",(!strncmp(rip->r_name, "aix", 3))?"EPX-AIX":"EPX-NK");
	cp->Insert(line); 

	sprintf(line,"Machine=%s",rip->r_name);
	cp->Insert(line); 

	return cp;
}

int
resource_close(rid)
	resource_id_t rid;
{
	return 0;
}

/* TODO */
static int
exec_starter(starter, hostname, main_sock, err_sock, resname)
	char *starter;
	char *hostname;
	int main_sock;
	int err_sock;
	char *resname;
{
	int i;
	int pid;
	int n_fds = getdtablesize();
	int omask;
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
			set_root_euid(__FILE__,__LINE__);
#endif NFSFIX
		(void)execl(starter, "condor_starter", hostname, resname, 0);
#ifdef NFSFIX
		/* Must be condor to write to log files. */
		if (getuid() == 0)
			set_condor_euid(__FILE__,__LINE__);
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
event_vacate(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
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
event_block(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
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
event_unblock(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
{
	/* TODO: change states to SUSPENDED */
}

int
event_start(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
{
	return 0;
}

int
event_suspend(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
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
event_continue(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
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
event_killjob(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
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

	resmgr_changestate(rip->r_rid, CHECKPOINTING);

	return kill_starter(rip->r_pid, SIGINT);
}

int
event_pcheckpoint(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
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
event_killall(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
{
	resource_info_t *rip;

	if (!(rip = resmgr_getbyrid(rid))) {
		dprintf(D_ALWAYS, "kill: could not find resource.\n");
		return -1;
	}

	if (getuid() == 0)
		set_root_euid();
#if defined(HPUX9) || defined(LINUX)
	if( kill(-getpgrp(),SIGKILL) < 0 ) {
#else
	if( kill(-getpgrp(0),SIGKILL) < 0 ) {
#endif
		EXCEPT("kill( my_process_group ) euid = %d\n", geteuid());
	}
	sigpause(0);	/* huh? */
}

int
event_exited(rid, jid, tid)
	resource_id_t rid;
	job_id_t jid;
	task_id_t tid;
{
	resource_free(rid);
	event_timeout();
}

static int
kill_starter(pid, signo)
	int pid, signo;
{
	struct stat st;
	int ret = 0;

	for (errno = 0; stat(Starter, &st) < 0; errno = 0) {
		if (errno == ETIMEDOUT)
			continue;
		EXCEPT("kill_starter(%d, %d): cannot stat <%s> -- errno = %d\n",
			pid, signo, Starter, errno);
	}

#ifdef NFSFIX
	if (getuid() == 0)
		set_root_euid(__FILE__,__LINE__);
#endif
	if (signo != SIGSTOP && signo != SIGCONT)
		ret = kill(pid, SIGCONT);
#ifdef NFSFIX
	if (getuid() == 0)
		set_condor_euid(__FILE__, __LINE__);
#endif
	return ret;
}

static void
init_static_info(rip)
	resource_info_t *rip;
{
	char	tmp[80];
	char	*ptr;
	char	*host_cell;
	char	*addrname;
	int phys_memory = -1;
	ClassAd *config_context = rip->r_context;
	struct sockaddr_in sin;
	int len = sizeof sin;

	sprintf(tmp,"Name=%s",rip->r_name);
	config_context->Insert(tmp);

	host_cell = get_host_cell();

	if(host_cell)
	{
	  sprintf(tmp,"AFS_Cell=%s",host_cell);
	  config_context->Insert(tmp);
	  dprintf(D_ALWAYS, "AFS_Cell = \"%s\"\n", tmp.s_val);
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


	sprintf(tmp,"UidDomain=%s",ptr);
	config_context->Insert(tmp);

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

	sprintf(tmp,"FilesystemDomain=%s",ptr);
	config_context->Insert(tmp);

	sprintf(tmp,"Cpus=%d",calc_ncpus(rip));
	config_context->Insert(tmp);

	/*
	 * NEST has nothing to do with the filesystem, but we set it here
	 * instead of in update_central_manager() because it only needs 
	 * to run at startup and at a reconfig (i.e. a HUP signal)
	 * XXX
	 */
	
	if(!(tmp=param("NEST")))
	  tmp="NOTSET";
	config_context->Insert(tmp);
	dprintf(D_ALWAYS, "Nest = \"%s\"\n", tmp);
	
	if (getuid() == 0)
		set_root_euid(__FILE__, __LINE__);
	phys_memory = 128*1024*1024;
	if (getuid() == 0)
		set_condor_euid(__FILE__, __LINE__);
	if (phys_memory > 0) {
		tmp.type = INT;
		tmp.i_val = phys_memory;
		store_stmt(build_expr("Memory",&tmp), config_context);
	}
	addrname = (char *)malloc(32);
	getsockname(rip->r_sock, (struct sockaddr *)&sin, &len);
	get_inet_address(&sin.sin_addr);
#ifdef AIX41_HACK
	sprintf(addrname, "<192.87.74.36:%d>", sin.sin_port);
#else
	strcpy(addrname, sin_to_string(&sin));
#endif
	sprintf(tmp,"%s=%s",(char *)ATTR_STARTD_IP_ADDR,addrname);
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

static int
calc_ncpus(rip)
	resource_info_t *rip;
{
	int x, y;

	if (!strcmp(rip->r_name, "ccall"))	/* XXXXXXXXXXXX */
		return 40;
	if (sscanf(rip->r_name, "aix%d_%d", &x, &y) != 2 &&
	    sscanf(rip->r_name, "cc%d_%d", &x, &y) != 2)
		return 1;
	return x;
}

#define TOPLINES 3
#define MAXPART 50

static char *mons[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
			  "Aug", "Sep", "Oct", "Nov", "Dec" };

time_t
cvt_time(dstr, mstr, day, hour, min, sec, year)
	char *dstr, *mstr;
	int day, hour, min, sec, year;
{
	int i;
	int month;
	struct tm tms;

	for (i = 0; i < 12; i++) {
		if (!strcmp(mons[i], mstr)) {
			month = i;
			break;
		}
	}

	tms.tm_sec = sec;
	tms.tm_min = min;
	tms.tm_hour = hour;
	tms.tm_mday = day;
	tms.tm_mon = month;
	tms.tm_year = year - 1900;
	return mktime(&tms);
}

static int
part_info(void)
{
	FILE *fp;
	int i;
	char buf[512];
	char rname[12], c1[2], user[24], c3[2], day[4], month[4];
	char **names;
	int dy, hr, min, sec, year;

	names = (char **)calloc(MAXPART, sizeof (char *));

	if ((fp = popen("/usr/bin/px nrm -pa", "r")) < 0) {
		perror("popen");
		exit(1);
	}

	for (i = 0; i < TOPLINES; i++)
		fgets(buf, sizeof buf, fp);

	npart = 0;
	while (fgets(buf, sizeof buf, fp)) {
		sscanf(buf, "%s %s %s %s %s %s %d %d:%d:%d %d",
			pinf[npart].p_name, c1, user, c3, day,
			month, &dy, &hr, &min, &sec, &year);

		pinf[npart].p_date = cvt_time(month, day, hr, min, sec, year);

		if (!strcmp(user, "*")) {
			pinf[npart].p_user[0] = '\0';
			pinf[npart].p_overlap[0] = '\0';
			pinf[npart].p_state = NO_JOB;
		} else if (strchr(user, '@')) {
			strcpy(pinf[npart].p_user, user);
			pinf[npart].p_overlap[0] = '\0';
			pinf[npart].p_state = JOB_RUNNING;
		}
		else {
			strcpy(pinf[npart].p_overlap, user);
			pinf[npart].p_user[0] = '\0';
			pinf[npart].p_state = SYSTEM;
		}
		npart++;
	}
	pclose(fp);
	return 0;
}
