/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
** Modified by : Dhaval N. Shah
**				 University of Wisconsin, Computer Sciences Dept.
** Uses <IP:PORT> rather than hostnames
*/ 

#define _POSIX_SOURCE
#include "condor_common.h"
#include "condor_fdset.h"

#include <stdio.h>
#include <string.h>					/* for capability */
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>

#if defined(Solaris)
#include <sys/filio.h>
#endif

#if defined(IRIX331)
#define __EXTENSIONS__
#include <signal.h>
#undef __EXTENSIONS__
#else
#include <signal.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "condor_types.h"
#include <netinet/in.h>
#include "debug.h"
#include "except.h"
#include "trace.h"
#include "expr.h"
#include "sched.h"
#include "manager.h"
#include "clib.h"
#include "proc.h"
#include <sys/wait.h>

#if defined(AIX32)
#	include "condor_fdset.h"
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char		*param(), *strdup(), *index();
XDR			*xdr_Init(), *xdr_Udp_Init();
CONTEXT		*create_context();
MACH_REC	*create_mach_rec(), *find_rec(), *find_server();
EXPR		*build_expr();
void 		say_goodby( XDR *xdrs, const char *name );
void 		unblock_signals();
char*		get_random_string(char**);
void		find_update_prio(struct in_addr, int);
void		reset_priorities();

typedef void (*SIG_HANDLER)();
void install_sig_handler( int sig, SIG_HANDLER handler );

void		alarm_handler();
void		sigpipe_handler();
void		sighup_handler();
void		sigint_handler();
void		sigchld_handler();

#define NEG_INFINITY	(1<<31)	/* ASSUMES 32 bit 2's COMPLIMENT ARITHMETIC! */

extern int	Terse;
extern int	Silent;

char	*Vacate;				/* Program for sending condor_vacate request */
char	*Log;
char	*NegotiatorLog;
char	*CollectorHost;
char	*AccountantHost;		/* weiru */
int		MaxNegotiatorLog;
int		NegotiatorInterval;
int		MachineUpdateInterval;
int		SchedD_Port;
int		ClientTimeout;
int		UpDownIncr;
int		MaxPrio;
int		MinPrio;
int		Foreground;
int		Termlog;
int		ClientSock = -1;
int		CommandSock = -1;
int		ConnectionSock;
int		UdpSock;
struct sockaddr_in      Collector_sin;
struct hostent		*Collector_Hostentp;
struct sockaddr_in      Accountant_sin;
struct hostent		*Accountant_Hostentp;

int     preemptionTime;			/* minimum time for PREEMPTION */
int     VanillapreemptionTime;	/* minimum time for PREEMPTION for VANILLA job */
typedef struct xx				/* dhruba 					   */
{
	char*		name;			/* machine name that has been vacated */
	struct xx*	next;
}	VacateList;
VacateList*		vacateList;
int 			emptyVacateList();
int 			presentInVacate();
int 			insertInVacate();
VacateList*		createVacateList();

int ContextError;


extern int	DebugFlags;

MACH_REC	*MachineList;
CONTEXT		*NegotiatorContext;
PRIO_REC	 DummyPrioRec, *Endmarker = &DummyPrioRec;

char *MyName;

#if !defined(MAX)
#	define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#if !defined(MIN)
#	define MIN(a,b) ((a)<(b)?(a):(b))
#endif

usage( name )
char	*name;
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t]\n", name );
	exit( 1 );
}


main( argc, argv )
int		argc;
char	*argv[];
{
	int		count;
	char	**ptr;
	fd_set	readfds;
	struct timeval timer;
	int		i;


#ifdef NFSFIX
	/* Must be condor to write to log files. */
	set_condor_euid(__FILE__,__LINE__);
#endif NFSFIX

	MyName = *argv;
	NegotiatorContext = create_context();
	config( argv[0], NegotiatorContext );

	init_params();
	Terse = TRUE;
	Silent = TRUE;

	if( argc > 3 ) {
		usage( argv[0] );
	}
	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
			case 'f':
				Foreground++;
				break;
			case 't':
				Termlog++;
				break;
			default:
				usage( argv[0] );
		}
	}

		/* This is so if we dump core it'll go in the log directory */
	if( chdir(Log) < 0 ) {
		EXCEPT( "chdir to log directory <%s>", Log );
	}

		/* Arrange to run in background */
	if( !Foreground ) {
		if( fork() )
			exit( 0 );
	}

		/* Set up logging */
	dprintf_config( "NEGOTIATOR", 2 );

	dprintf( D_ALWAYS, "*************************************************\n" );
	dprintf( D_ALWAYS, "***       CONDOR_NEGOTIATOR STARTING UP       ***\n" );
	dprintf( D_ALWAYS, "*************************************************\n" );
	dprintf( D_ALWAYS, "\n" );

	install_sig_handler( SIGINT, sigint_handler );
	install_sig_handler( SIGHUP, sighup_handler );
	install_sig_handler( SIGCHLD, sigchld_handler );

	ConnectionSock = init_tcp_sock( "condor_negotiator", NEGOTIATOR_PORT );
	UdpSock = udp_connect( CollectorHost, COLLECTOR_UDP_PORT );

	install_sig_handler( SIGPIPE, sigpipe_handler );
	install_sig_handler( SIGALRM, alarm_handler );
	MachineList = create_mach_rec( (MACH_REC *)NULL );
	vacateList  = createVacateList();      /* PREEMPTION */

	unblock_signals();

	for(;;) {
		timer.tv_sec = NegotiatorInterval;
		timer.tv_usec = 0;

		FD_ZERO( &readfds );
		FD_SET( ConnectionSock, &readfds );

		/*
		** dprintf( D_FULLDEBUG, "Selecting\n" );
		*/
#if defined(AIX31) || defined(AIX32)
		errno = EINTR;	/* Shouldn't have to do this... */
#endif
		count = select(FD_SETSIZE, (fd_set*)&readfds, (fd_set*)0, (fd_set*)0,
											(struct timeval *)&timer );
		if( count < 0 ) {
			if( errno == EINTR ) {
				dprintf( D_ALWAYS, "select() interrupted, restarting...\n" );
				continue;
			} else {
				EXCEPT( "select() returns %d, errno = %d", count, errno );
			}
		}
		/*
		** dprintf( D_FULLDEBUG, "Select returned %d\n", NFDS(count) );
		*/

		if( NFDS(count) == 0 ) {
			reschedule();
		} else if( FD_ISSET(ConnectionSock,&readfds) ) {
			accept_tcp_connection();
		} else {
			EXCEPT( "select: unknown connection, readfds = 0x%x, count = %d",
														readfds, NFDS(count) );
		}
	}
	exit( 0 );
}


init_tcp_sock( service, port )
char	*service;
int		port;
{
	struct sockaddr_in	sin;
	struct servent *servp;
	caddr_t		on = (caddr_t)1;
	int		sock;

	memset( (char *)&sin, 0,sizeof sin );
	servp = getservbyname(service, "tcp");
	if( servp ) {
		sin.sin_port = htons( (u_short)servp->s_port );
	} else {
		sin.sin_port = htons( (u_short)port );
	}

	if( (sock=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

	if( setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0) {
		EXCEPT( "setsockopt" );
	}

	if( bind(sock,(struct sockaddr *)&sin,sizeof(sin)) < 0 ) {
		if( errno == EADDRINUSE ) {
			EXCEPT( "CONDOR_NEGOTIATOR ALREADY RUNNING" );
		} else {
			EXCEPT( "bind" );
		}
	}

	if( listen(sock,5) < 0 ) {
		EXCEPT( "listen" );
	}

	return sock;
}

accept_tcp_connection()
{
	struct sockaddr_in	from;
	int		len;
	XDR		xdr, *xdrs, *xdr_Init();

	len = sizeof from;
	memset( (char *)&from, 0,sizeof from );
	CommandSock = accept( ConnectionSock, (struct sockaddr *)&from, &len );
	dprintf( D_FULLDEBUG,
		"Opened CommandSock (%d) at %d\n",
		CommandSock, __LINE__
	);

	if( CommandSock < 0 && errno != EINTR ) {
		EXCEPT( "accept" );
	}

	xdrs = xdr_Init( &CommandSock, &xdr );

	(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
	do_command( xdrs );
	(void)alarm( 0 );				/* cancel alarm */

	xdr_destroy( xdrs );
	if( CommandSock >= 0 ) {
		(void)close( CommandSock );
		dprintf( D_FULLDEBUG,
			"Closed CommandSock (%d) at %d\n",
			CommandSock, __LINE__
		);
		CommandSock = -1;
	}
}

/*
** Somebody has connected to our socket with a request.  Read the request
** and handle it.
*/
do_command( xdrs )
XDR		*xdrs;
{
	int		cmd;

		/* Read the request */
	xdrs->x_op = XDR_DECODE;
	if( !xdr_int(xdrs,&cmd) ) {
		dprintf( D_ALWAYS, "Can't receive command from client\n" );
		return;
	}

	switch( cmd ) {
		case RESCHEDULE:	/* Please do your scheduling early (now) */
			dprintf( D_ALWAYS, "Got RESCHEDULE request\n" );
			(void)alarm( 0 );	/* cancel alarm, this could take awhile */
				/* close connection to process requesting the
				   reschedule, it won't wait anyway */
			(void)close( CommandSock );
			dprintf( D_FULLDEBUG,
				"Closed CommandSock (%d) at %d\n",
				CommandSock, __LINE__
			);
			CommandSock = -1;

			reschedule();
			break;
		default:
			EXCEPT( "Got unknown command (%d)\n", cmd );
	}
}

#define CLOSE_XDR_STREAM \
	xdr_destroy( xdrs ); \
	(void)close( sock ); \
	dprintf( D_FULLDEBUG, "Closed %d at %d\n", sock, __LINE__ );

reschedule()
{
	int			sock = -1;
	int			cmd;
	XDR			xdr, *xdrs = NULL;
	MACH_REC	record, *rec = &record;

	/* empty vacateList : PREEMPTION(dhruba) */
	emptyVacateList();

	/* Connect to the collector */
	if( (sock = connect_to_collector()) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to CONDOR Collector\n" );
		return;
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = GIVE_STATUS;
	if( !xdr_int(xdrs, &cmd) ) {
		dprintf( D_ALWAYS, "1. Can't send GIVE_STATUS command\n" );
		CLOSE_XDR_STREAM;
		return;
	}
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		dprintf( D_ALWAYS, "2. Can't send GIVE_STATUS command\n" );
		CLOSE_XDR_STREAM;
		return;
	}

	xdrs->x_op = XDR_DECODE;
	for(;;) {
		memset( (char *)rec, 0,sizeof(MACH_REC) );
		rec->machine_context = create_context();
		if( !xdr_mach_rec(xdrs,rec) ) {
			dprintf( D_ALWAYS, "Can't get machine record from collector\n" );
			/* We can't safely free this cause it's got garbage in it.  It's
			** a "memory leak" but let it go... -- mike */
			/*
			free_context( rec->machine_context );
			*/
			CLOSE_XDR_STREAM;
			return;
		}
		if( !rec->name ) {
			free_context( rec->machine_context );
			break;
		}
		if( !rec->name[0] ) {
			FREE( rec->name );
			free_context( rec->machine_context );
			break;
		}
		update_machine_info( rec );
		free_context( rec->machine_context );
		FREE( rec->name );
	}

	CLOSE_XDR_STREAM;
	do_negotiations();
	update_accountant();
}
#undef CLOSE_XDR_STREAM

update_accountant()
{
	struct in_addr	endMarker;
	int			cmd;
	int			sock = -1;
	XDR			xdr, *xdrs;
	MACH_REC	*rec;

	dprintf( D_ALWAYS, "Sending updated priorities to accountant...\n" );

		/* Connect to the accountant */
	if( (sock = connect_to_accountant()) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to CONDOR accountant\n" );
		return;
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

		/* Send the command */
	cmd = PRIORITY_INFO;
	if( !snd_int(xdrs, cmd, FALSE) ) {
		xdr_destroy( xdrs );
		close( sock );
		return;
	}
	dprintf(D_FULLDEBUG, "sent command %d to accountant\n", cmd);

	for( rec = MachineList->next; rec->name; rec = rec->next ) {
		if( !xdr_in_addr(xdrs, &rec->net_addr) ) {
			dprintf( D_ALWAYS, "Can't send machine addr to accountant\n" );
			xdr_destroy( xdrs );
			close( sock );
			return;
		}
		if( !xdr_int(xdrs,&rec->prio) ) {
			dprintf( D_ALWAYS, "Can't send machine prio to accountant\n" );
			xdr_destroy( xdrs );
			close( sock );
			return;
		}
		dprintf( D_FULLDEBUG, "Sent %s (%d, %d)\n", rec->name,
				 rec->net_addr.s_addr, rec->prio );
	}
	endMarker.s_addr = 0;
	if( !xdr_in_addr(xdrs,&endMarker) ) {
		dprintf( D_ALWAYS, "Can't send ENDMARKER to accountant\n" );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		xdr_destroy( xdrs );
		close( sock );
		return;
	}

	xdr_destroy( xdrs );
	close( sock );
	dprintf( D_FULLDEBUG, "Done sending priorities\n" );
}

init_params()
{
	struct sockaddr_in	sin;
	struct servent 		*servp;
	char				*tmp;
	struct hostent      *Collector_Hostentp;

	tmp = param( "NEGOTIATOR_DEBUG" );
	if( tmp == NULL ) {
		EXCEPT( "NEGOTIATOR_DEBUG not specified in config file\n" );
	} else {
		free( tmp );
	}
	Foreground = boolean( "NEGOTIATOR_DEBUG", "Foreground" );

	Log = param( "LOG" );
	if( Log == NULL )  {
		EXCEPT( "No log directory specified in config file\n" );
	}

	NegotiatorLog = param( "NEGOTIATOR_LOG" );
	if( NegotiatorLog == NULL ) {
		EXCEPT( "No log file specified in config file\n" );
	}

	tmp = param( "MAX_NEGOTIATOR_LOG" );
	if( tmp == NULL ) {
		MaxNegotiatorLog = 64000;
	} else {
		MaxNegotiatorLog = atoi( tmp );
		free( tmp );
	}

	tmp = param( "NEGOTIATOR_INTERVAL" );
	if( tmp == NULL ) {
		NegotiatorInterval = 120;
	} else {
		NegotiatorInterval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "MACHINE_UPDATE_INTERVAL" );
	if( tmp == NULL ) {
		MachineUpdateInterval = 300;
	} else {
		MachineUpdateInterval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "CLIENT_TIMEOUT" );
	if( tmp == NULL ) {
		ClientTimeout = 30;
	} else {
		ClientTimeout = atoi( tmp );
		free( tmp );
	}

	memset( (char *)&sin, 0,sizeof sin );
/*	servp = getservbyname("condor_schedd", "tcp");
	if( servp ) {
		SchedD_Port = servp->s_port;
	} else {
		SchedD_Port = htons(SCHED_PORT);
	} this seems redundant since the port ip is in the context itself dhaval */


	if( (CollectorHost = param("COLLECTOR_HOST")) == NULL ) {
		EXCEPT( "COLLECTOR_HOST not specified in config file\n" );
	}
	Collector_Hostentp = gethostbyname( CollectorHost );
	if( Collector_Hostentp == NULL ) {
#if defined(vax) && !defined(ultrix)
		herror( "gethostbyname" );
#endif defined(vax) && !defined(ultrix)
		EXCEPT( "Can't find host \"%s\" (Nameserver down?)\n", CollectorHost );
	}
	memset( (char *)&Collector_sin, 0,sizeof Collector_sin );
	memcpy( (char *)&Collector_sin.sin_addr, Collector_Hostentp->h_addr,
		  (unsigned)Collector_Hostentp->h_length);
	Collector_sin.sin_family = Collector_Hostentp->h_addrtype;
	Collector_sin.sin_port = htons(COLLECTOR_PORT);

	/* weiru */
	if( (AccountantHost = param("ACCOUNTANT_HOST")) != NULL ) {
		Accountant_Hostentp = gethostbyname( AccountantHost );
		if( Accountant_Hostentp == NULL ) {
		#if defined(vax) && !defined(ultrix)
			herror( "gethostbyname" );
		#endif defined(vax) && !defined(ultrix)
			dprintf(D_ALWAYS, "Can't find host \"%s\" (Nameserver down?)\n", AccountantHost );
		}
		memset( (char *)&Accountant_sin, 0,sizeof Accountant_sin );
		memcpy( (char *)&Accountant_sin.sin_addr, Accountant_Hostentp->h_addr,
			  (unsigned)Accountant_Hostentp->h_length);
		Accountant_sin.sin_family = Accountant_Hostentp->h_addrtype;
		Accountant_sin.sin_port = htons(ACCOUNTANT_PORT);
	}


	tmp = param( "UP_DOWN_INCR" );
	if( tmp == NULL ) {
		UpDownIncr = 10;
	} else {
		UpDownIncr = atoi( tmp );
		free( tmp );
	}

	tmp = param( "MAX_PRIO" );
	if( tmp == NULL ) {
		MaxPrio = 0;		/* No max applied if this is zero */
	} else {
		MaxPrio = atoi( tmp );
		free( tmp );
	}

	tmp = param( "MIN_PRIO" );
	if( tmp == NULL ) {
		MinPrio = 0;		/* No min applied if this is zero */
	} else {
		MinPrio = atoi( tmp );
		free( tmp );
	}

	tmp = param("PREEMPTION_LIMIT");         /* This is in minutes */
	if ( tmp == NULL ) {
		preemptionTime = 60*60  ;  /* 1 hour */
	} else {
		preemptionTime = atoi(tmp)*60;
		free( tmp );
	}

	tmp = param("PREEMPTION_LIMIT_VANILLA");         /* This is in minutes */
	if ( tmp == NULL ) {
		VanillapreemptionTime = 60*60*24*7  ;  /* 1 week */
	} else {
		VanillapreemptionTime = atoi(tmp)*60;
		free( tmp );
	}
}

SetSyscalls(){}

update_machine_info( template )
MACH_REC	*template;
{
	MACH_REC	*rec;

	if( (rec = find_rec(template)) == NULL ) {
		rec = create_mach_rec( template );
		insque( (struct qelem *)rec, (struct qelem *)MachineList );
		/*
		** dprintf(D_FULLDEBUG,"Createing new record for \"%s\"\n",rec->name);
		*/
	} else {
		/*
		** dprintf( D_FULLDEBUG, "Updating record for \"%s\"\n", rec->name );
		*/
	}

	update_context( template->machine_context, rec->machine_context );
	rec->time_stamp = template->time_stamp;
	rec->busy = template->busy;
}

MACH_REC	*
create_mach_rec( template )
MACH_REC	*template;
{
	MACH_REC	*new;

	new = (MACH_REC *)CALLOC( 1, sizeof(MACH_REC) );

	new->next = new;
	new->prev = new;

	if( template == NULL ) {
		return new;
	}

	new->name = strdup( template->name );
	new->net_addr = template->net_addr;
	new->net_addr_type = template->net_addr_type;
	new->machine_context = create_context();

		/* possible bug fix */
	new->prio = template->prio;

	return new;
}

MACH_REC	*
find_rec( new )
MACH_REC	*new;
{
	MACH_REC	*ptr;

	for( ptr = MachineList->next; ptr->name; ptr = ptr->next ) {
		if( ptr->net_addr.s_addr == new->net_addr.s_addr ) {
			return ptr;
		}
	}
	return NULL;
}

update_context( src, dst )
CONTEXT	*src;
CONTEXT	*dst;
{
	int		i;

	for( i=0; i<src->len; i++ ) {
		store_stmt( src->data[i], dst );
		src->data[i] = NULL;
	}
	src->len = 0;
}

do_negotiations()
{
	MACH_REC	*ptr, *find_next();
	int			idle;

	get_priorities();
	/*update_priorities();*/
	sort_machine_list();

	dprintf(D_ALWAYS, "---------- Begin negotiating\n");
	for( ptr = MachineList->next; ptr->name; ptr = ptr->next ) {

		if( (int)time( (time_t *)0 ) - ptr->time_stamp >
													MachineUpdateInterval ) {
			/*
			** dprintf( D_FULLDEBUG,
			** "Not negotiating with %s - stale record\n", ptr->name );
			*/
			continue;
		}

		if( evaluate_int("Idle",&idle,ptr->machine_context,(CONTEXT *)0) < 0 ) {
			dprintf( D_ALWAYS,
			"Not negotiating with %s - can't evaluate \"Idle\"\n", ptr->name );
			continue;
		}
		if( idle <= 0 ) {
			/*
			** dprintf( D_FULLDEBUG,
			** "Not negotiating with %s - no idle jobs\n", ptr->name );
			*/
			continue;
		}

		dprintf( D_FULLDEBUG, "Negotiating with %s\n", ptr->name );
		negotiate( ptr );
	}
	dprintf(D_ALWAYS, "---------- End negotiating\n");
}

#define SUCCESS 1
#define FAIL 0
#undef TRACE
#define TRACE dprintf(D_ALWAYS,"%s:%d\n",__FILE__,__LINE__)

negotiate( rec )
MACH_REC	*rec;
{

	MACH_REC	*next, *find_next();
	XDR			xdr, *xdrs = NULL, *xdr_Init();
	int		next_prio;
	int		status;

	if( next = find_next(rec) ) {
		next_prio = next->prio;
	} else {
		next_prio = NEG_INFINITY;
	}

	if( (ClientSock = call_schedd(rec,10)) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to SchedD on %s\n", rec->name );
		return;
	}
	dprintf( D_FULLDEBUG,
		"\tOpened ClientSock (%d) at %d\n",
		ClientSock, __LINE__
	);

	(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
	xdrs = xdr_Init( &ClientSock, &xdr );
	status = snd_int( xdrs, NEGOTIATE, FALSE );
	(void)alarm( 0 );				/* cancel alarm */

	if( !status ) {
		dprintf( D_ALWAYS, "\t1.Error negotiating with %s\n", rec->name );
		xdr_destroy( xdrs );
		return;
	}

	while( rec->prio >= next_prio ) {

		(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
		status = simple_negotiate( rec, xdrs );
		(void)alarm( (unsigned)0 );

		if( status != SUCCESS ) {
			break;
		}
	}

		/* If we stop the negotiation while the scheduler still has jobs,
		   we must tell it we are done.  Otherwise, it ran out of jobs
		   to send us, and has already broken the connection. */
	if( status == SUCCESS ) {
		say_goodby( xdrs, rec->name );
	}

	xdr_destroy( xdrs );
	if( ClientSock != -1 ) {
		(void)close( ClientSock );
		dprintf( D_FULLDEBUG,
			"Closed ClientSock (%d) to %s\n",
			ClientSock, rec->name
		);
		ClientSock = -1;
	}
}

simple_negotiate( rec, xdrs )
MACH_REC	*rec;
XDR			*xdrs;
{
	int			op;
	MACH_REC	*server;
	CONTEXT		*job_context = NULL;
	char 		*address,*final_string, *random_string;
	int 		sequence_number=0;

	for(;;) {
		(void)alarm( (unsigned)ClientTimeout );	/* reset the alarm every time */
		if( !snd_int(xdrs,SEND_JOB_INFO,TRUE) ) {
			dprintf( D_ALWAYS, "\t3.Error negotiating with %s\n", rec->name );
			return ERROR;
		}

		errno = 0;
		if( !rcv_int(xdrs,&op,FALSE) ) {
			dprintf( D_ALWAYS, "\t4.Error negotiating with %s\n", rec->name );
			dprintf( D_ALWAYS, "\terrno = %d\n", errno );
			return ERROR;
		}

		switch( op ) {
			case JOB_INFO:
				job_context = create_context();
				if( !rcv_context(xdrs,job_context,TRUE) ) {
					dprintf(D_ALWAYS,
							"\t5.Error negotiating with %s\n", rec->name );
					free_context( job_context );
					return ERROR;
				}


				if( server = find_server(job_context) ) {
					dprintf( D_ALWAYS, "\tAllowing %s to run on %s\n",
						rec->name, server->name);
					if( evaluate_string("STARTD_IP_ADDR",&address,server->machine_context,0 ) < 0 ) {
						dprintf(D_ALWAYS,"\tDidn't receive startd_ip_addr\n");
					}

					final_string=(char *)(malloc(SIZE_OF_FINAL_STRING*sizeof(char)));
					random_string=(char *)(malloc(SIZE_OF_CAPABILITY_STRING*sizeof(char)));
					if(get_random_string(&random_string)!=0)
					{ 
						EXCEPT("Error in getting random string\n");
					}

					sprintf(final_string, "%s %s#%d", address, random_string,sequence_number);
					dprintf(D_ALWAYS, "\tmatch %s\n", final_string);
					if(send_to_startd(final_string)!=0) {
						dprintf( D_ALWAYS,
								"\t6.Error negotiating with %s\n", rec->name );
						free_context( job_context );
						free(final_string);
						free(random_string);
						free(address);
						return ERROR;
					}
					if(send_to_accountant(final_string) < 0) {
						dprintf(D_ALWAYS,
								"\tCan't send capability to accountant.\n");
					}

					if( !snd_int(xdrs,PERMISSION,TRUE) ) {
						dprintf( D_ALWAYS,
								"\t8.Error negotiating with %s\n", rec->name );
						free_context( job_context );
						free(final_string);
						free(random_string);
						free(address);
						return ERROR;
					}
					if( !snd_string(xdrs,final_string,TRUE) ) { /* dhaval */
						dprintf( D_ALWAYS,
								"\t9.Error negotiating with %s\n", rec->name );
						free_context( job_context );
						free(final_string);
						free(random_string);
						free(address);
						return ERROR;
					}
					dprintf(D_FULLDEBUG, "\tsend %s to schedd\n", final_string);
					free(final_string);
					free(address);
					free(random_string);
					decrement_idle( rec );
					server->busy = TRUE;
					rec->prio -= 1;
					free_context( job_context );
					return SUCCESS;
				} else {	/* Can't find a server for it */
					/* PREEMPTION : dhruba */
					create_prospective_list(rec->prio,job_context);

					if( !snd_int(xdrs,REJECTED,TRUE) ) {
						dprintf( D_ALWAYS,
						"\t9.Error negotiating with %s\n", rec->name );
						free_context( job_context );
						return ERROR;
					}
				}
				free_context( job_context );
				break;
			case NO_MORE_JOBS:
				xdrrec_skiprecord( xdrs );
				dprintf( D_FULLDEBUG, "Done negotiating with %s\n", rec->name );
				return FAIL;
			default:
				dprintf( D_ALWAYS, "\t%s sent unknown op (%d)\n", rec->name, op );
				return ERROR;
		}
	}
}

get_priorities()
{
    int         	sock;
	XDR         	xdr, *xdrs;
    int         	cmd = GIVE_PRIORITY;
    struct in_addr 	mach;
    int         	prio;

	dprintf(D_FULLDEBUG, "Getting priorities from the accountant...\n");
    reset_priorities();
	if((sock = connect_to_accountant()) < 0)
	{
		dprintf(D_ALWAYS, "Can't connect to accountant\n");
	}
	else
	{
		dprintf(D_FULLDEBUG, "connected to accountant\n");
		xdrs = xdr_Init(&sock,&xdr);
		xdrs->x_op = XDR_ENCODE;
		if(!snd_int(xdrs, cmd, TRUE))
		{
			xdr_destroy( xdrs );
			close(sock);
			dprintf(D_ALWAYS, "can't send GIVE_PRIORITY to accountant\n");
			return -1;
		}
		dprintf(D_FULLDEBUG, "sent GIVE_PRIORITY to accountant\n");
		xdrs->x_op = XDR_DECODE;
		while(1)
		{
			if(!xdr_in_addr(xdrs, &mach))
			{
				xdr_destroy( xdrs );
				close(sock);
				dprintf(D_ALWAYS, "can't receive machine addr\n");
				return -1;
			}
			if(mach.s_addr == 0)
			/* end of record */
			{
				if(!xdrrec_skiprecord(xdrs))
				{
					xdr_destroy(xdrs);
					close(sock);
					dprintf(D_ALWAYS, "xdrrec_skiprecord, errno = %d\n", errno);
					return -1;
				}
				xdr_destroy(xdrs);
				close(sock);
				dprintf(D_FULLDEBUG, "Done getting priority info\n");
				return 0;
			}
			if(!xdr_int(xdrs, &prio))
			{
				xdr_destroy(xdrs);
				close(sock);
				dprintf(D_ALWAYS, "can't receive priority info\n");
				return -1;
			}
			dprintf(D_FULLDEBUG, "Got (%d, %d)\n", mach.s_addr, prio);
			find_update_prio(mach, prio);
		}
	}
	return 0;
}

void find_update_prio(struct in_addr mach, int p)
{
	MACH_REC*	rec;

    for(rec = MachineList->next; rec->name; rec = rec->next )
	{
        if( rec->net_addr.s_addr == mach.s_addr )
		{
			rec->prio = p;
			dprintf(D_FULLDEBUG, "%s new priority %d\n", rec->name, rec->prio);
			return;
        }
    }
}

update_priorities()
{
	MACH_REC	*ptr;

	dprintf( D_FULLDEBUG, "Entered update_priorities()\n" );
	for( ptr=MachineList->next; ptr->name; ptr = ptr->next ) {
		/*
		dprintf( D_FULLDEBUG,
			"Updating priority for \"%s\" ", ptr->name );
		*/
		ptr->prio = update_prio( NegotiatorContext, ptr );
	}
}


update_prio( nego, rec )
CONTEXT		*nego;
MACH_REC	*rec;
{
	int		new_prio;
	int		inactive;
	ELEM	tmp;

	tmp.type = INT;
	tmp.i_val = rec->prio;
	store_stmt( build_expr("Prio",&tmp), rec->machine_context );

	if( evaluate_bool("INACTIVE",&inactive,nego,rec->machine_context) < 0 ) {
		EXCEPT( "Can't evaluate \"INACTIVE\"" );
	}
	if( inactive ) {
		new_prio = up_down_incr( rec->prio );
	} else {
		if(evaluate_int("UPDATE_PRIO",&new_prio,nego,rec->machine_context)< 0) {
			EXCEPT( "Can't evaluate \"UPDATE_PRIO\"" );
		}
	}
	/*
	dprintf( D_FULLDEBUG | D_NOHEADER, "from %d to %d\n", rec->prio, new_prio );
	*/

	if( MaxPrio && (new_prio > MaxPrio) ) {
		new_prio = MaxPrio;
	}

	if( MinPrio && (new_prio < MinPrio) ) {
		new_prio = MinPrio;
	}

	tmp.i_val = new_prio;
	store_stmt( build_expr("Prio",&tmp), rec->machine_context );
	return new_prio;
}

void reset_priorities(int prio)
{
	MACH_REC	*ptr;

	for(ptr = MachineList->next; ptr->name; ptr = ptr->next)
	{
		ptr->prio = prio;
	}
}

sort_machine_list()
{
	MACH_REC	*sorted_list;
	MACH_REC	*tmp;
	MACH_REC	*ptr;

	sorted_list = create_mach_rec( (MACH_REC *)0 );

	for( ptr=MachineList->next; ptr->name; ptr = tmp ) {
		tmp = ptr->next;
		remque( (struct qelem *)ptr );
		prio_insert( ptr, sorted_list );
	}
	tmp = MachineList;
	MachineList = sorted_list;
	free_mach_rec( tmp );
}

prio_insert( elem, list )
MACH_REC	*elem;
MACH_REC	*list;
{
	MACH_REC	*ptr;

	for( ptr=list->prev; ptr->name; ptr = ptr->prev ) {
		if( ptr->prio > elem->prio ) {
			insque( (struct qelem *)elem, (struct qelem *)ptr );
			return;
		}
	}
	insque( (struct qelem *)elem, (struct qelem *)list );
}

MACH_REC *
find_next( ptr )
MACH_REC	*ptr;
{
	int		idle;

	for( ptr=ptr->next; ptr->name; ptr = ptr->next ) {
		if( time((time_t *)0) - ptr->time_stamp > MachineUpdateInterval ) {
			continue;
		}
		if( evaluate_int("Idle",&idle,ptr->machine_context,(CONTEXT *)0) < 0 ) {
			continue;
		}
		if( idle <= 0 ) {
			continue;
		}
		return ptr;
	}
	return NULL;
}

call_schedd( host, timeout )
MACH_REC *host;
int		timeout; /* seconds */
{
	struct sockaddr_in	sin;
	int					scheduler;
	int					on = 1, off = 0;
	struct timeval		timer;
	fd_set				writefds;
	int					nfound;
	int					nfds;
	int					tmp_errno;
	char 				*schedd_address;

	if( (scheduler=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

    if( evaluate_string("SCHEDD_IP_ADDR",&schedd_address,host->machine_context,0
 ) < 0 ) {
		dprintf(D_ALWAYS, "\tdid'nt receive schedd_ip_addr and using backward compatibility mode\n");
		memset( (char *)&sin, 0,sizeof sin );
		memcpy( (char *)&sin.sin_addr, (char *)&host->net_addr,
				sizeof(sin.sin_addr) );
		sin.sin_family = host->net_addr_type;
		sin.sin_port = SchedD_Port;
	} else {
		dprintf(D_FULLDEBUG, "SCHEDD_IP_ADDR = %s\n", schedd_address);
		string_to_sin(schedd_address,&sin); /* dhaval */
	}

	/* We want to attempt the connect with the socket in non-blocking
	   mode.  That way if there is some problem on the other end we
	   won't get hung up indefinitely waiting to connect to one host.
	   For some reason on AIX if you set this here, the connect()
	   fails.  In that case errno doesn't get set either... */
#if !defined(AIX31) && !defined(AIX32)
	if( ioctl(scheduler,FIONBIO,(char *)&on) < 0 ) {
		EXCEPT( "ioctl" );
	}
#endif

	if( connect(scheduler,(struct sockaddr *)&sin,sizeof(sin)) < 0 ) {
		tmp_errno = errno;
		switch( errno ) {
			case EINPROGRESS:
				break;
			default:
				dprintf( D_ALWAYS,
					"Can't connect to host \"%s\", errno = %d\n",
					host->name, tmp_errno );
				(void)close( scheduler );
				return -1;
		}
	}

#ifdef AIX31	/* see note above */
	if( ioctl(scheduler,FIONBIO,(char *)&on) < 0 ) {
		EXCEPT( "ioctl" );
	}
#endif AIX31

	timer.tv_sec = timeout;
	timer.tv_usec = 0;
	nfds = scheduler + 1;
	FD_ZERO( &writefds );
	FD_SET( scheduler, &writefds );
	nfound = select( nfds, (fd_set*)0, (fd_set*)&writefds, (fd_set*)0,
													(struct timeval *)&timer );
	switch( nfound ) {
	case 0:
		(void)close( scheduler );
		return -1;
	case 1:
		if( ioctl(scheduler,FIONBIO,(char *)&off) < 0 ) {
			EXCEPT( "ioctl" );
		}
		return scheduler;
	default:
		EXCEPT( "Select returns %d", nfound );
		return -1;
	}
}

MACH_REC	*
find_server( job_context )
CONTEXT		*job_context;
{
	MACH_REC	*ptr;
	int			curTime;
	
	curTime = time( (time_t *) 0 );
	dprintf( D_JOB, "\n" );
	dprintf( D_JOB, "Looking for server for job:\n" );
	if( DebugFlags & D_JOB ) {
		display_context( job_context );
	}
	dprintf( D_JOB, "\n" );


		/* Look for a server which meets both requirements and preferences */
	for( ptr=MachineList->next; ptr->name; ptr = ptr->next ) {

		dprintf( D_MACHINE, "\n" );
		dprintf( D_MACHINE, "Checking %s\n", ptr->name );
		if( DebugFlags & D_MACHINE ) {
			display_context( ptr->machine_context );
		}
		dprintf( D_MACHINE, "\n" );

		if( ptr->busy ) {
			dprintf( D_FULLDEBUG, "Busy\n" );
			continue;
		}

    /* We have to have a recent time stamp in order to use this machine JCP */
		if (curTime - ptr->time_stamp > 65 * MINUTE) {
			continue;
		}

		if( !check_bool("START",ptr->machine_context,job_context) ) {
			dprintf( D_FULLDEBUG, "START is FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "START is TRUE\n" );

		if( !check_bool("JOB_REQUIREMENTS",ptr->machine_context,job_context)) {
			dprintf( D_FULLDEBUG, "JOB_REQUIREMENTS is FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "JOB_REQUIREMENTS is TRUE\n" );

		if( !check_bool("JOB_PREFERENCES",ptr->machine_context,job_context) ) {
			dprintf( D_FULLDEBUG, "JOB_PREFERENCES is FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "JOB_PREFERENCES is TRUE\n" );

		if( !check_string("State","NoJob",ptr->machine_context,(CONTEXT *)0)) {
			dprintf( D_FULLDEBUG, "State != NoJob\n" );
			continue;
		}

		dprintf( D_FULLDEBUG, "\tfind_server() returning \"%s\"\n", 
				ptr->name );

		return ptr;
	}

		/* Look for a server which just meets the requirements */
	for( ptr=MachineList->next; ptr->name; ptr = ptr->next ) {
		/*
		** dprintf( D_FULLDEBUG, "\n" );
		** dprintf( D_FULLDEBUG, "Checking %s\n", ptr->name );
		** display_context( ptr->machine_context );
		** dprintf( D_FULLDEBUG, "\n" );
		*/
		if( ptr->busy ) {
			continue;
		}
    /* We have to have a recent time stamp in order to use this machine JCP */
		if (curTime - ptr->time_stamp > 65 * MINUTE) {
			continue;
		}
		if( !check_bool("START",ptr->machine_context,job_context) ) {
			continue;
		}
		if( !check_bool("JOB_REQUIREMENTS",ptr->machine_context,job_context) ) {
			continue;
		}
		if( !check_string("State","NoJob",ptr->machine_context,(CONTEXT *)0) ) {
			/*
			** dprintf( D_FULLDEBUG, "State != NoJob\n" );
			*/
			continue;
		}

		/*
		** dprintf( D_FULLDEBUG, "Returning 0x%x\n", ptr );
		*/
		return ptr;
	}

	/*
	** dprintf( D_FULLDEBUG, "Returning NULL\n" );
	*/
	return NULL;
}

check_bool( name, context_1, context_2 )
char	*name;
CONTEXT	*context_1;
CONTEXT	*context_2;
{
	int		answer;

	if( evaluate_bool(name,&answer,context_1,context_2) < 0 ) {
		/*
		** dprintf( D_FULLDEBUG, "Can't evaluate \"%s\"\n", name );
		** dprintf( D_FULLDEBUG, "Context1:\n" );
		** display_context( context_1 );
		** dprintf( D_FULLDEBUG, "\nContext2:\n" );
		** display_context( context_2 );
		** dprintf( D_FULLDEBUG, "\n" );
		*/
		return FALSE;
	}
	return answer;
}

check_string( name, pattern, context_1, context_2 )
char	*name;
char	*pattern;
CONTEXT	*context_1;
CONTEXT	*context_2;
{
	char	*tmp;
	int		answer;

	if( evaluate_string(name,&tmp,context_1,context_2) < 0 ) {
		dprintf(
			D_ALWAYS,
			"\tCan't evaluate \"%s\" while looking for \"%s\"\n",
			name,
			pattern
		);
		ContextError = TRUE;
		return FALSE;
	}
	if( strcmp(pattern,tmp) == MATCH ) {
		answer = TRUE;
	} else {
		answer = FALSE;
	}
	FREE( tmp );
	return answer;
}

decrement_idle( rec )
MACH_REC	*rec;
{
	int		idle;
	ELEM	tmp;

	if( evaluate_int("Idle",&idle,rec->machine_context,(CONTEXT *)0) < 0 ) {
		dprintf( D_ALWAYS, "Can't evaluate \"Idle\" for %s\n", rec->name );
		return;
	}

	tmp.type = INT;
	tmp.i_val = idle - 1;;
	store_stmt( build_expr("Idle",&tmp), rec->machine_context );
}

free_mach_rec( rec )
MACH_REC	*rec;
{
	if( rec->name ) {
		FREE( rec->name );
	}

	if( rec->machine_context ) {
		free_context( rec->machine_context );
	}
	FREE( (char *)rec );
}

connect_to_collector()
{
	int					status;
	int					fd;

	if( (fd=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}
	dprintf( D_FULLDEBUG, "Opened %d at %d\n", fd, __LINE__ );

	if( (status = connect(fd,(struct sockaddr *)&Collector_sin,
						  sizeof(Collector_sin))) == 0 ) {
		return fd;
	} else {
		dprintf( D_ALWAYS, "connect returns %d, errno = %d\n", status, errno );
		(void)close( fd );
		return -1;
	}
}

connect_to_accountant()
{
	int					status;
	int					fd;

	if( (fd=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}
	dprintf( D_FULLDEBUG, "Opened %d at %d\n", fd, __LINE__ );

	if( (status = connect(fd,(struct sockaddr *)&Accountant_sin,
						  sizeof(Accountant_sin))) == 0 ) {
		return fd;
	} else {
		dprintf( D_ALWAYS, "connect returns %d, errno = %d\n", status, errno );
		(void)close( fd );
		return -1;
	}
}

void
sigint_handler()
{
	dprintf( D_ALWAYS, "Killed by SIGINT\n" );
	exit( 0 );
}

/*
** The other end of our connection to the client has gone away.  Close our
** end.  Then the read()/write() will fail, and whichever client routine
** is involved will clean up.
*/
void
sigpipe_handler()
{
	dprintf( D_ALWAYS, "\tGot SIGPIPE\n" );

	if( CommandSock >= 0 ) {
		(void)close( CommandSock );
		dprintf( D_FULLDEBUG,
			"\tClosed CommandSock (%d) at %d\n",
			CommandSock, __LINE__
		);
	}
	CommandSock = -1;

	if( ClientSock >= 0 ) {
		(void)close( ClientSock );
		dprintf( D_FULLDEBUG,
			"\tClosed ClientSock (%d) at %d\n",
			ClientSock, __LINE__
		);
	}
	ClientSock = -1;
}

/*
** The machine we are talking to has stopped responding, close our
** end.  Then the read()/write() will fail, and whichever client routine
** is involved will clean up.
*/
void
alarm_handler()
{
	dprintf( D_ALWAYS,
		"No response in %d seconds, breaking connection\n", ClientTimeout
	);

	if( CommandSock >= 0 ) {
		(void)close( CommandSock );
		dprintf( D_FULLDEBUG,
			"Closed CommandSock (%d) at %d\n",
			CommandSock, __LINE__
		);
	}
	CommandSock = -1;

	if( ClientSock >= 0 ) {
		(void)close( ClientSock );
		dprintf( D_FULLDEBUG,
			"\tClosed ClientSock (%d) at %d\n",
			ClientSock, __LINE__
		);
	}
	ClientSock = -1;
}

void
sighup_handler()
{
	dprintf( D_ALWAYS, "Re reading config file\n" );

	config( MyName, (CONTEXT *)0 );
	init_params();
}

void
sigchld_handler()
{
	pid_t		pid;
	int			status;

	dprintf( D_ALWAYS, "Entering SIGCHLD handler\n" );
	for(;;) {
		pid = waitpid( -1, &status, WNOHANG );
		if( pid == 0 || pid == -1 ) {
			break;
		}
		if( WIFEXITED(status) ) {
			dprintf( D_ALWAYS,
				"Reaped child status - pid %d exited with status %d\n",
				pid, WEXITSTATUS(status)
			);
		} else {
			dprintf( D_ALWAYS,
				"Reaped child status - pid %d killed by signal %d\n",
				pid, WTERMSIG(status)
			);
		}
			
	}
}

/*
** In the up-down algorithm, if there are no jobs in the queue, then
** we adjust the priority toward zero by a fixed amount.  Here given the
** previous priority value, and assming there are no jobs in the queue,
** we return what the new priority should be.
*/
int
up_down_incr( old_prio )
int		old_prio;
{
	if( old_prio > 0 ) {
		return MAX( old_prio - UpDownIncr, 0 );
	}
	if( old_prio < 0 ) {
		return MIN( old_prio + UpDownIncr, 0 );
	}
	return 0;
}

/* parameter : priority of machine from where this job is submitted*/
typedef struct data
{
	int 	 prio;   /* priority of machine who's job is currently */
					/*  hosted by this machine                     */
	MACH_REC *rec;
}Prospect;

static int compare(a1, a2) /* compares priorities */
void*	a1, *a2;
{
	if ( *(int*)a1 > *(int*)a2 ) return 1;/*a1 points to prospect.prio*/
	if ( *(int*)a1 < *(int*)a2 ) return -1;
	return 0;
}
	

create_prospective_list( prio , job_context)
int 		prio;
CONTEXT*	job_context;
{
	MACH_REC    *ptr, *p; 
	Prospect*	prospect;
	char* cli, flag;
	int count = 0, curPrio,i;
	int curTime, jobTime;
	int     sock;
	XDR     xdr, *xdrs;
	int     cmd;
	int 	jobUniverse;

	curTime =  time((time_t *)0);
	dprintf(D_PREEMPT, "PREEMPTION_LIMIT = %d minutes\n", preemptionTime/60);
	dprintf(D_PREEMPT, "PREEMPTION_LIMIT_VANILLA = %d minutes\n", VanillapreemptionTime/60);

	for( ptr=MachineList->next; ptr->name; ptr = ptr->next ) 
		count++;
	if ( count == 0 ) return;
	prospect = malloc(count * sizeof(Prospect));
	if ( prospect == NULL ) return ;
	count = 0;

	dprintf(D_PREEMPT,"prospect_list: negotiating job's prio = %d\n",prio);

	for( ptr=MachineList->next; ptr->name; ptr = ptr->next ) 
	{
    	if( ptr->busy ) {  /* some job scehduled in this iteration */
			dprintf(D_PREEMPT,"%s busy\n", ptr->name);
        	continue;	   /* leave it alone */
    	}
		/* START is false but condor is running on it => this is a */
		/* possible candidate for preemption                       */
    	if( !check_bool("START",ptr->machine_context,job_context) ) {
			ContextError = FALSE;
        	if( !check_string("State","Running",ptr->machine_context,(CONTEXT *)0)) 
			{
				dprintf(D_PREEMPT,"%s START = false\n", ptr->name);
				if( ContextError ) {
					dprintf( D_FULLDEBUG, "\t\tmachine = \"%s\"\n", ptr->name );
				}
        		continue;
			}
    	}
    	if( !check_bool("JOB_REQUIREMENTS",ptr->machine_context,job_context) ) {
			dprintf(D_PREEMPT,"%s REQUIREMENTS = false\n", ptr->name);
        	continue;
    	}
        if( check_string("State","NoJob",ptr->machine_context,(CONTEXT *)0) ) {
			dprintf(D_PREEMPT,"%s is Idle \n", ptr->name);
            continue;      /* free machine s were checked already */
        }

		/* extract name of submitting machine of the job that is */
		/* being  currently hosted                                   */
		if(evaluate_string("ClientMachine",&cli,ptr->machine_context,NULL)<0)
		{
			dprintf(D_PREEMPT,"Can't evaluate ClientMachine for %s\n",
					ptr->name);
			continue;
		}
		dprintf(D_PREEMPT,"%s machine is hosting job from %s\n", 
				ptr->name,cli);

		/* extract priority of the current jobs' submitting machine */
		for( p=MachineList->next, curPrio=0, flag=0; p->name; p = p->next ) {
			if ( strcmp(p->name, cli) == 0 ) {
				curPrio = p->prio;
				flag= 1;
				break;
			}
		}

		if ( flag == 0 ) {
			dprintf(D_PREEMPT,"%s does no exists in MachineList\n", cli);
		}

		free( cli );
		
		if ((flag==1 ) && ( prio <= curPrio )) {
			dprintf(D_PREEMPT,"Ignoring machine %s(currently running job's prio %d\n", ptr->name, curPrio);
			continue;
		}

		/* preemption should be done only if the job has run on this */
		/* machine for PREEMPTION_LIMIT minutes at a stretch         */
		if(evaluate_int("JobStart",&jobTime,ptr->machine_context,(CONTEXT *)0)<0) 
		{
			dprintf(D_PREEMPT,"%s could not evaluate JobStart\n",ptr->name);
			continue;
		}

		/* look in the machine context to see if this is a VANILLA job, and
		 * set appropiate preempt time -Todd */
		if(evaluate_int("JobUniverse",&jobUniverse,ptr->machine_context,(CONTEXT *)0)<0) 
		{
			dprintf(D_PREEMPT,"%s could not evaluate JobUniverse\n",ptr->name);
			continue;
		}

		if ( jobUniverse == VANILLA ) {
			if (( curTime - jobTime ) < VanillapreemptionTime ) 
			{
				dprintf(D_PREEMPT,"%s PREEMPTION_LIMIT_VANILLA is false\n",ptr->name);
				continue;
			}
		} else {
			if (( curTime - jobTime ) < preemptionTime ) 
			{
				dprintf(D_PREEMPT,"%s PREEMPTION_LIMIT is false\n",ptr->name);
				continue;
			}
		}

		/* if this machine has been vacated for another job */
		if ( presentInVacate(ptr->name) ) 
		{
			dprintf(D_PREEMPT,"%s been vacated by a previous job\n",ptr->name);
			continue;
		}

		dprintf(D_PREEMPT,"Inserting %s into prospective list\n", ptr->name);

		prospect[count].rec    =  ptr;
		prospect[count++].prio =  curPrio;
	}
	/* sort this list */
	qsort((void *)prospect,count, sizeof(prospect[0]), compare);
	for( i=0; i < count; i ++)
	{
		dprintf(D_PREEMPT, "prospect = %s ", prospect[i].rec->name);
		dprintf(D_PREEMPT, "hosting job from machine whose prio = %d\n",
						prospect[i].prio);
	}
	if ( count > 0 )
	{
		dprintf(D_ALWAYS,"\tVACATING %s\n", prospect[0].rec->name);
		insertInVacate(prospect[0].rec->name);
		if( (sock = do_connect(prospect[0].rec->name, "condor_startd", 
							   START_PORT)) < 0 ) {
			dprintf( D_ALWAYS, 
					"Can't connect to startd on %s\n", 
					prospect[0].rec->name );
		} else {
			xdrs = xdr_Init( &sock, &xdr );
			xdrs->x_op = XDR_ENCODE;

			cmd = CKPT_FRGN_JOB;
			ASSERT( xdr_int(xdrs, &cmd) );
			ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

			dprintf( D_ALWAYS,
					"Sent CKPT_FRGN_JOB command to startd on %s\n", 
					prospect[0].rec->name );
			xdr_destroy( xdrs );
			close( sock );
		}
	}
	free(prospect);
}

emptyVacateList()
{
	VacateList*	ptr = vacateList->next, *tmp;
	while ( ptr != NULL )
	{
		if ( ptr->name) free(ptr->name);
		tmp = ptr;
		ptr = ptr->next;
		free(tmp);
	}
	vacateList->next = NULL;
}

int insertInVacate(char* name)
{
	VacateList*	cell = (VacateList *)malloc(sizeof(VacateList));
	if ( cell == NULL ){
		EXCEPT("Malloc failure\n");
	}
	cell->name   = strdup(name);
	cell->next   = vacateList->next;
	vacateList->next = cell;
}

presentInVacate(char* name)
{
	VacateList*	ptr = vacateList;
	while ( ptr->next != NULL )
		if ( strcmp(ptr->next->name, name ) == 0) return 1;
		else ptr = ptr->next;

	return 0;
}

VacateList*
createVacateList()
{
	VacateList* tmp = (VacateList*) malloc(sizeof(VacateList));
	if ( tmp == NULL ){
		EXCEPT("Malloc failure\n");
	}
	tmp->next = NULL;
	return tmp;
}

void
say_goodby( XDR *xdrs, const char *name )
{
	int		status;

	dprintf( D_FULLDEBUG, "\tSending END_NEGOTIATE to %s\n", name );

	(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
	status = snd_int( xdrs, END_NEGOTIATE, TRUE );
	(void)alarm( 0 );				/* cancel alarm */

	if( status ) {
		dprintf( D_FULLDEBUG,
			"\tDone Sending END_NEGOTIATE to %s\n", name );
	} else {
		dprintf( D_ALWAYS, "\tError Sending END_NEGOTIATE to %s\n", name );
	}
}

void
unblock_signals()
{
	sigset_t	set;

	sigemptyset( &set );
	sigaddset( &set, SIGINT );
	sigaddset( &set, SIGHUP );
	sigaddset( &set, SIGCHLD );
	sigaddset( &set, SIGPIPE );
	sigaddset( &set, SIGALRM );

	if( sigprocmask(SIG_UNBLOCK,&set,NULL) < 0 ) {
		EXCEPT( "Can't unblock signals" );
	}
}

/* Just now I am just writing a simple get_random_string function that just
concatenates 5 randomly generated numbers between 0 to 100 to form the random 
string. This function can be improved to generate the random string by some 
better method as it is just a black box as far as the negotiator is concerned 
..dhaval */


char *get_random_string(char **temp)
{
int random1;
int random2;
int random3;
int random4;
int random5;
char str[SIZE_OF_CAPABILITY_STRING];

random1=get_random_number();
random2=get_random_number();
random3=get_random_number();
random4=get_random_number();
random5=get_random_number();

sprintf(str,"%d%d%d%d%d",random1,random2,random3,random4,random5);
*temp=strdup(str);
return 0;
}

int get_random_number()
{
	return rand()%100;
}

/* addr is in the format "<xxx.xxx.xxx.xxx:xxxx> xxxxxxxxxxxxxxx#xxx" */
int send_to_startd(const char *addr)
	{
	XDR 	*xdrs, xdr;
	int		sock,port;
	int		cmd=MATCH_INFO;
	char	*str;

	if((sock = do_connect(addr,"condor_startd",START_PORT))<0)
	{ 
		return -1;
	}
	dprintf(D_FULLDEBUG, "\tconnected to startd\n");

	xdrs = xdr_Init(&sock,&xdr);
	xdrs->x_op = XDR_ENCODE;

	if(!snd_int(xdrs,cmd,FALSE) ) {
		xdr_destroy( xdrs );
		close(sock);
		dprintf(D_ALWAYS, "can't send command MATCH_INFO to startd\n");
		return -1;
	}

	str = strchr(addr, ' ');
	str++;
	if(!snd_string(xdrs,str,TRUE)) {
		xdr_destroy( xdrs );
		close(sock);
		return -1;
	}

	dprintf(D_FULLDEBUG, "\tsend match %s to startd\n", str);

	xdr_destroy( xdrs );
	close(sock);
	return 0;
}

int send_to_accountant(char* capability)
{
	int		sock;
	XDR		xdr, *xdrs;
	int		cmd = MATCH_INFO;

	if(!AccountantHost)
	{
		dprintf(D_FULLDEBUG, "\tno accountant in configuration file\n");
		return 0;
	}
	if((sock = connect_to_accountant()) < 0)
	{
		dprintf(D_ALWAYS, "\tCan't connect to accountant\n");
		return 0;
	}
	dprintf(D_FULLDEBUG, "\tconnected to accountant\n");

	xdrs = xdr_Init(&sock,&xdr);
	xdrs->x_op = XDR_ENCODE;

	if(!xdr_int(xdrs,&cmd) ) {
		xdr_destroy( xdrs );
		close(sock);
		dprintf(D_ALWAYS, "can't send command MATCH_INFO to startd\n");
		return -1;
	}

	if(!snd_string(xdrs,capability,TRUE)) {
		xdr_destroy( xdrs );
		close(sock);
		return -1;
	}

	dprintf(D_FULLDEBUG, "\tsend match %s to accountant\n", capability);
	xdr_destroy( xdrs );
	close(sock);
	return 0;
}
