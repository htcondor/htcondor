/*
 * Main routine and function for the startd.
 */

/*
 * System include files.
 */
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/select.h>

/*
 * Condor include files.
 */
#include "cdefs.h"
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "sched.h"
#include "condor_common.h"
#include "condor_types.h"
#include "expr.h"
#include "clib.h"
#include "util_lib_proto.h"

#include "resource.h"
#include "resmgr.h"

#include "CondorPrefExps.h"

/*
 * Polling vars.
 */
int	polling_freq;
int	polls_per_update;
int	polls_per_update_kbdd;
int	polls_per_update_load;

/*
 * Flags from the commandline.
 */
int	fgflag;
int	tflag;

/*
 * Paths.
 */
char		*log_path;
char		*spool_path;
char		*exec_path;
char		*mail_path;
char		*client;
char		*AccountantHost;
extern char	*uptime_path;
extern	char	*pstat_path;

/*
 * Hosts.
 */
char	*collector_host;
char	*alt_collector_host;

/*
 * Others.
 */
int	last_timeout;
int	capab_timeout;
int	memory;
int	termlog;
char	*MyName;
char	*admin;
char	*def_owner = "Owner = \"nobody\"";

ClassAd* template_ClassAd;

char	*PrimaryStarter;
char	*AlternateStarter[10];
char	*Starter;
char	*IP;

static char *_FileName_ = __FILE__;

extern volatile int want_reconfig;
extern int HasSigchldHandler;


/*
 * XXX these should not be global.
 */
int coll_sock, intcp_sock, inudp_sock;

/*
 * XXX this should not be needed. Messy include files.
 */

extern char *get_host_cell();

extern void event_sigint(int);
extern void event_sighup(int);
extern void event_sigterm(int);
extern void event_sigchld(int);

/*
 * Prototypes of static functions.
 */

static void usage		__P((const char *s));
static void init_params		__P((void));
static void mainloop		__P((void));
static char *get_full_hostname	__P((void));
static int res_config_context   __P((resource_info_t *));

int main(int argc, char** argv)
{

	if (argc > 3) {
		usage(argv[0]);
		exit(1);
	}


	MyName = argv[0];
	// MyName is now 'startd' or 'startd_t'

	while (*++argv) {
		if (*argv[0] != '-') {
			usage(argv[0]);
			exit(1);
		}
		switch(argv[0][1]) {
		case 'f':
			fgflag = 1;
			break;
		case 't':
			termlog++;
			break;
		default:
			usage(argv[0]);
			exit(1);
		}
	}

	if (getuid() == 0)
		set_condor_euid();

	// build the classAd, create a hash table

	/* conv all contexts to classAds - N Anand*/
	template_ClassAd = new ClassAd();
	configAd(MyName, template_ClassAd);

	// CHANGE -> N Anand
	template_ClassAd->Insert(def_owner);

	dprintf_config("STARTD", 2);

	dprintf(D_ALWAYS, "************************************************\n");
	dprintf(D_ALWAYS, "***       CONDOR_STARTD STARTING UP          ***\n");
	dprintf(D_ALWAYS, "************************************************\n");
	dprintf(D_ALWAYS, "\n");

	CondorPrefExps::Initialize(template_ClassAd);

	resmgr_init();
	// call *res_config_context for all resources
	// *res_config_context fills in the classAd by reading 
	// the config file
	resmgr_walk(res_config_context);
	init_params();
	resource_init();
	
	event_timeout();

	if (chdir(log_path) < 0) {
		EXCEPT("chdir to log directory %s", log_path);
	}

	if (!fgflag) {
		if (fork())
			exit(0);
	}

	/*
	 * XXX Use DaemonCore for signal handling.
	 */
	install_sig_handler(SIGINT, event_sigint);
	install_sig_handler(SIGHUP, event_sighup);
	install_sig_handler(SIGTERM, event_sigterm);
	install_sig_handler(SIGCHLD, event_sigchld);
	install_sig_handler(SIGPIPE, SIG_IGN );
	HasSigchldHandler = 1;	/* XXX yucky */

	intcp_sock = create_tcpsock("condor_startd", START_PORT);
	inudp_sock = create_udpsock("condor_startd", START_UDP_PORT);
	coll_sock = udp_unconnect();

	/*
	 * XXX replace with DaemonCore
	 */

	mainloop();

	return 0;
}

static int res_config_context(resource_info_t* rip)
{
  //CHANGE -> N Anand
  configAd(MyName,rip->r_context);
}

static void usage(const char* s)
{
	dprintf(D_ALWAYS, "Usage: %s [-f] [-t]\n", s);
}

static void mainloop()
{
	fd_set readfds;
	int count;
	struct timeval timer;

	for (;;) {
		FD_ZERO(&readfds);
		resmgr_setsocks(&readfds);
		timer.tv_usec = 0;
		timer.tv_sec = polling_freq -
		    ((int)time((time_t *)0) - last_timeout);

		if(timer.tv_sec < 0)
			timer.tv_sec = 0;

		if (timer.tv_sec > polling_freq)
			timer.tv_sec = 0;
#if defined(AIX31) || defined(AIX32)
		errno = EINTR;
#endif
		dprintf(D_ALWAYS, "timeout = %d sec\n", timer.tv_sec);
		count = select(FD_SETSIZE, &readfds, (fd_set *)0, (fd_set *)0,
			       &timer);

		dprintf(D_ALWAYS, "out of select()\n");
		if (want_reconfig) {
			dprintf( D_ALWAYS, "Re reading config file\n" );
			resmgr_walk(res_config_context);
			init_params();
			want_reconfig = 0;
		}

		if (count < 0) {
			if (errno == EINTR)
				continue;
			else {
				EXCEPT("select(FD_SETSIZE,0%o,0,0,%d sec)",
					readfds, timer.tv_sec);
			}
		}

		block_signal(SIGCHLD);	/* XXX */
		if (NFDS(count) == 0)
			event_timeout();
		else
			resmgr_call(&readfds);
		sigsetmask(0);		/* XXX */
	}
}

static void init_params()
{
	char *tmp, *pval, buf[1024];
	int i;

	if (log_path)
		free(log_path);
	log_path = param("LOG");
	if (log_path == NULL) {
		EXCEPT("No log directory specified in config file");
	}

	if (exec_path)
		free(exec_path);
	exec_path = param("EXECUTE");
	if (exec_path == NULL) {
		EXCEPT("No Execute file specified in config file");
	}

	if (collector_host)
		free(collector_host);
	collector_host = param("COLLECTOR_HOST");
	if (collector_host == NULL) {
		EXCEPT("No Collector host specified in config file");
	}

	if (alt_collector_host)
		free(alt_collector_host);
	alt_collector_host = param("CONDOR_VIEW_HOST");

	tmp = param("POLLING_FREQUENCY");
	if (tmp == NULL) {
		polling_freq = 30;
	} else {
		polling_freq = atoi(tmp);
		free(tmp);
	}

	tmp = param("POLLS_PER_UPDATE");
	if (tmp == NULL) {
		polls_per_update = 4;
	} else {
		polls_per_update = atoi(tmp);
	}

	tmp = param("POLLS_PER_UPDATE_KBDD");
	if (tmp == NULL) {
		polls_per_update_kbdd = 6;
	} else {
		polls_per_update_kbdd = atoi(tmp);
		free(tmp);
	}

	tmp = param( "POLLS_PER_UPDATE_LOAD" );
	if(tmp == NULL) {
		polls_per_update_load = 12;
	} else {
		polls_per_update_load = atoi( tmp );
		free(tmp);
	}

	tmp = param("STARTD_DEBUG");
	if (tmp == NULL ) {
		EXCEPT("STARTD_DEBUG not defined in config file");
	}
	free(tmp);
	if (!fgflag)
		fgflag = boolean("STARTD_DEBUG", "Foreground");

	if (admin)
		free(admin);
	admin = param("CONDOR_ADMIN");
	if (admin == NULL ) {
		EXCEPT("CONDOR_ADMIN not specified in config file");
	}

	tmp = param("MEMORY");
	if (tmp != NULL) {
		memory = atoi(tmp);
		free(tmp);
	}
	if (memory <= 0) {
		memory =  DEFAULT_MEMORY;
	}

	if (spool_path)
		free(spool_path);
	spool_path = param("SPOOL");
	if (spool_path == NULL) {
		EXCEPT("SPOOL not specified in config file");
	}

	if (mail_path)
		free(mail_path);
	mail_path = param("MAIL");
	if (mail_path == NULL) {
		EXCEPT("MAIL not specified in config file");
	}

	PrimaryStarter = param("STARTER");
	if (PrimaryStarter == NULL) {
		EXCEPT("No Starter file specified in config file\n");
	}
	Starter = PrimaryStarter;

	for (i = 0; i < 10; i++ ) {
		sprintf(buf, "ALTERNATE_STARTER_%d", i);
		AlternateStarter[i] = param(buf);
	}

	if (IP)	
		free(IP);
	IP = param("STARTD_IP");

	if (AccountantHost)
		free(AccountantHost);
	AccountantHost = param("ACCOUNTANT_HOST");

	tmp = param("CAPABILITY_TIMEOUT");
	if (!tmp)
		capab_timeout = 120;
	else {
		capab_timeout = atoi(tmp);
		free(tmp);
	}
}


