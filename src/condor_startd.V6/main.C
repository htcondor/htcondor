/*
 * Main routine and function for the startd.
 */

#include "condor_common.h"


/*
 * System include files.
 */
#include <netdb.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#if !defined(LINUX) && !defined(HPUX9)
#include <sys/select.h>
#endif

/*
 * Condor include files.
 */
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "sched.h"
#include "condor_types.h"
#include "expr.h"
#include "clib.h"
#include "util_lib_proto.h"
#include "condor_uid.h"

#include "resource.h"
#include "resmgr.h"

#include "CondorPrefExps.h"

/*
 * Polling vars.
 */
int polling_interval;
int update_interval;

/*
 * Flags from the commandline.
 */
int	fgflag;
int	tflag;

/*
 * Paths.
 */
char		*log_path=NULL;
char		*spool_path=NULL;
char		*exec_path=NULL;
char		*mail_path=NULL;
char		*AccountantHost=NULL;

/*
 * Hosts.
 */
char	*negotiator_host=NULL;
char	*collector_host=NULL;
char	*alt_collector_host=NULL;

/*
 * Others.
 */
int	capab_timeout;
extern int	Termlog;
char	*MyName;
char	*admin;
int run_benchmarks;

ClassAd* template_ClassAd;

char	*PrimaryStarter=NULL;
char	*AlternateStarter[10];
char	*Starter=NULL;
char	*IP=NULL;

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
extern void event_sigquit(int);
extern void event_sighup(int);
extern void event_sigterm(int);
extern void event_sigchld(int);

/*
 * Prototypes of static functions.
 */

static void usage(const char *s);
static void init_params(void);
static void mainloop(void);
static char *get_full_hostname(void);
static int res_config_classad(resource_info_t *);

int main(int argc, char** argv)
{

	if (argc > 3) {
		usage(argv[0]);
		exit(1);
	}

	MyName = argv[0];

		// run as condor.condor at all times unless root privilege is needed
	set_condor_priv();

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
			Termlog++;
			break;
		default:
			usage(argv[0]);
			exit(1);
		}
	}

		// build the classAd, create a hash table
	template_ClassAd = new ClassAd();
	config( template_ClassAd );

	dprintf_config("STARTD", 2);

	dprintf(D_ALWAYS, "************************************************\n");
	dprintf(D_ALWAYS, "***       CONDOR_STARTD STARTING UP          ***\n");
	dprintf(D_ALWAYS, "************************************************\n");
	dprintf(D_ALWAYS, "\n");

	CondorPrefExps::Initialize(template_ClassAd);

	init_params();
	resmgr_init();
	resource_init();
	
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
		// For now, install these signal handlers with a default mask
		// of all signals blocked when we're in the handlers.
	sigset_t set;
	sigfillset( &set );
	install_sig_handler_with_mask(SIGINT, &set, event_sigint);
	install_sig_handler_with_mask(SIGQUIT, &set, event_sigquit);
	install_sig_handler_with_mask(SIGHUP, &set, event_sighup);
	install_sig_handler_with_mask(SIGTERM, &set, event_sigterm);
	install_sig_handler_with_mask(SIGCHLD, &set, event_sigchld);
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

static int res_config_classad(resource_info_t* rip)
{
	config( rip->r_classad );
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
	int stashed_errno;
	sigset_t fullset, emptyset;
	int next_timeout = (int)time((time_t *)0);
			// initialize next_timeout so that we do a timeout right away. 

	sigfillset( &fullset );
	sigemptyset( &emptyset );

	for(;;) {
		FD_ZERO(&readfds);
		resmgr_setsocks(&readfds);
		timer.tv_usec = 0;
		timer.tv_sec = next_timeout - (int)time((time_t *)0);
		if( timer.tv_sec < 0 ) timer.tv_sec = 0;

			// Unblock all signals before going into the select
		sigprocmask( SIG_SETMASK, &emptyset, NULL );

#if defined(AIX31) || defined(AIX32)
		errno = EINTR;
#endif
		count = select(FD_SETSIZE, &readfds, (fd_set *)0, (fd_set *)0,
			       &timer);

			// Block all signals until we're going to select again.
		sigprocmask( SIG_SETMASK, &fullset, NULL );

		stashed_errno = errno;	// reconfig will trample our errno

		if( want_reconfig ) {
			dprintf( D_ALWAYS, "Re reading config file\n" );
			dprintf_config( "STARTD", 2 );
			resmgr_walk( res_config_classad );
			init_params();
			want_reconfig = 0;
		}

		if (count < 0) {
			if( stashed_errno == EINTR ) {
				continue;
			} else {
				EXCEPT( "select(FD_SETSIZE,0%o,0,0,%d sec), errno: %d",
						readfds, timer.tv_sec, errno);
			}
		}

		if( NFDS(count) == 0 ) {
			next_timeout = event_timeout();
		} else {
			resmgr_call(&readfds);
		}
	}
}

static void init_params()
{
	char *tmp, *pval, buf[1024];
	int i;
	struct hostent *hp;

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

	// make sure we have the canonical name for the negotiator host
	tmp = param("NEGOTIATOR_HOST");
	if (tmp == NULL) {
		EXCEPT("No Negotiator host specified in config file");
	}
	if ((hp = gethostbyname(tmp)) == NULL) {
		EXCEPT("gethostbyname failed for negotiator host");
	}
	free(tmp);
	negotiator_host = strdup(hp->h_name);

	if (collector_host)
		free(collector_host);
	collector_host = param("COLLECTOR_HOST");
	if (collector_host == NULL) {
		EXCEPT("No Collector host specified in config file");
	}

	if (alt_collector_host)
		free(alt_collector_host);
	alt_collector_host = param("CONDOR_VIEW_HOST");

	tmp = param("POLLING_INTERVAL");
	if (tmp == NULL) {
		polling_interval = 5;
	} else {
		polling_interval = atoi(tmp);
		free(tmp);
	}

	tmp = param("UPDATE_INTERVAL");
	if (tmp == NULL) {
		update_interval = 60;
	} else {
		update_interval = atoi(tmp);
		free(tmp);
	}

	tmp = param("STARTD_DEBUG");
	if (tmp == NULL ) {
		EXCEPT("STARTD_DEBUG not defined in config file");
	}
	free(tmp);

		/* XXX  What's going on here?  This is weird! -Derek */
	if (!fgflag)
		fgflag = boolean("STARTD_DEBUG", "Foreground");

	if (admin)
		free(admin);
	admin = param("CONDOR_ADMIN");
	if (admin == NULL ) {
		EXCEPT("CONDOR_ADMIN not specified in config file");
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
	if (!tmp) {
		capab_timeout = 120;
	} else {
		capab_timeout = atoi(tmp);
		free(tmp);
	}
	
	tmp = param("RUN_BENCHMARKS");
	if (!tmp)
		run_benchmarks = 1;	  // default to True !
	else {
		if ( *tmp == 'T' || *tmp == 't' )
			run_benchmarks = 1;
		else
			run_benchmarks = 0;
		free(tmp);
	}
}


