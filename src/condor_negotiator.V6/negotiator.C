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
#include <sys/param.h>

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
#include "condor_debug.h"
//#include "expr.h"
#include "sched.h"
#include "clib.h"
#include "proc.h"
#include "condor_query.h"
#include "manager.h"
#include "condor_io.h"
#include "condor_uid.h"

#include <sys/wait.h>

#if defined(AIX32)
#	include "condor_fdset.h"
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char		*strdup(), *index();
CONTEXT		*create_context();
MACH_REC	*create_mach_rec(MACH_REC*), *find_rec(MACH_REC*), *find_server(CONTEXT*);
void 		say_goodby( ReliSock *s, const char *name );
void 		unblock_signals();
char*		get_random_string(char**);
void		find_update_prio(const char*, int);
void		reset_priorities();
void		init_params();
int			init_tcp_sock(char*, int);
void		reschedule();
void		accept_tcp_connection();
void		do_command(ReliSock *);
int			connect_to_collector();
void		update_machine_info(MACH_REC*);
void		do_negotiations();
void		update_accountant();
int			connect_to_accountant();
void		update_context(CONTEXT*, CONTEXT*);
void		create_prospective_list(int, CONTEXT*);

typedef void (*SIG_HANDLER)();

void		alarm_handler();
void		sigpipe_handler();
void		sighup_handler();
void		sigint_handler();
void		sigchld_handler();

extern "C"
{
	void	dprintf(int, char*...);
	int		config_from_server(char*, char*, CONTEXT*);
	void	_EXCEPT_(char*...);
	int		udp_connect(char*, int);	
	int		xdr_mach_rec(XDR*, MACH_REC*); 
	int		snd_int(XDR*, int, int);
	int		xdr_in_addr(XDR*, struct in_addr*); 
	int		boolean(char*, char*); 
	void	insque(struct qelem*, struct qelem*); 
	int		rcv_int(XDR*, int*, int);
	int		rcv_context(XDR*, CONTEXT*, int); 
	int		evaluate_string(char*, char**, CONTEXT*, CONTEXT*);
	int		evaluate_bool(char*, int*, CONTEXT*, CONTEXT*); 
	int		evaluate_int(char*, int*, CONTEXT*, CONTEXT*);
	int		snd_int(XDR*, int, int); 
	int		snd_string(XDR*, const char*, int); 
	void	string_to_sin(char*, struct sockaddr_in*); 
	void	display_context(CONTEXT*);
	int		check_string(char*, char*, CONTEXT*, CONTEXT*); 
	void	install_sig_handler( int sig, SIG_HANDLER handler );
	char*	param(char*); 
	EXPR   	*build_expr(char*, ELEM*);
	int		get_random_int();
}

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
int 			presentInVacate(char*);
int 			insertInVacate(char*);
VacateList*		createVacateList();

int 			ContextError;


extern int		DebugFlags;

MACH_REC*		MachineList;
CONTEXT*		NegotiatorContext;
PRIO_REC	 	DummyPrioRec, *Endmarker = &DummyPrioRec;
CondorQuery		queryObj(SCHEDD_AD);
ClassAdList*	scheddAds;


char*			MyName;

#if !defined(MAX)
#	define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#if !defined(MIN)
#	define MIN(a,b) ((a)<(b)?(a):(b))
#endif

usage( char* name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t]\n", name );
	exit( 1 );
}


main( int argc, char** argv )
{
	int		count;
	char	**ptr;
	fd_set	readfds;
	struct timeval timer;
	ClassAd	*NegotiatorAd;

	if( argc > 5 ) {
		usage( argv[0] );
	}
	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
			case 'f':
				Foreground = 1;
				break;
			case 't':
				Termlog++;
				break;
			default:
				usage( argv[0] );
		}
	}

	MyName = *argv;

	set_condor_priv();

		// Evil hack: since the negotiator still uses contexts, we've
		// got to configure with a classad, then convert it to a
		// context.  -Derek Wright 7/14/97

	NegotiatorContext = create_context();
	NegotiatorAd = new ClassAd;
	config( NegotiatorAd );
	NegotiatorAd->MakeContext( NegotiatorContext );
	delete( NegotiatorAd );

	init_params();
	Terse = TRUE;
	Silent = TRUE;

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


init_tcp_sock( char* service, int port )
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

	if( setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char*)&on,sizeof(on)) < 0) {
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

void accept_tcp_connection()
{
	struct sockaddr_in	from;
	int					len;
	ReliSock			sock; 

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

	sock.attach_to_file_desc(CommandSock);

	(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
	do_command( &sock );
	(void)alarm( 0 );				/* cancel alarm */
}

/*
** Somebody has connected to our socket with a request.  Read the request
** and handle it.
*/
void do_command( ReliSock* sock )
{
	int		cmd;

		/* Read the request */
	sock->decode();
	if( !sock->code(cmd) ) {
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
	sock->end_of_message();
}

#define CLOSE_XDR_STREAM \
	xdr_destroy( xdrs ); \
	(void)close( sock ); \
	dprintf( D_FULLDEBUG, "Closed %d at %d\n", sock, __LINE__ );

void reschedule()
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
	scheddAds = new ClassAdList();
	queryObj.fetchAds(*scheddAds); 
	do_negotiations();
	update_accountant();
	delete scheddAds;
}
#undef CLOSE_XDR_STREAM

void update_accountant()
{
	struct in_addr	endMarker;
	int			cmd;
	int			sock = -1;
	XDR			xdr, *xdrs;
	MACH_REC	*rec;

	dprintf(D_PROTOCOL,"## 3. Updating priorities on accountant...\n" );

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

void init_params()
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
	Foreground += boolean( "NEGOTIATOR_DEBUG", "Foreground" );
	
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
	
	char	constraint[100];
	sprintf(constraint, "%s > 0", ATTR_IDLE_JOBS); 
	queryObj.addConstraint(constraint); 
}

extern "C"
{
	SetSyscalls(){}
}

void update_machine_info( MACH_REC* templateRec )
{
	MACH_REC	*rec;

	if( (rec = find_rec(templateRec)) == NULL ) {
		rec = create_mach_rec( templateRec );
		insque( (struct qelem *)rec, (struct qelem *)MachineList );
		/*
		** dprintf(D_FULLDEBUG,"Createing new record for \"%s\"\n",rec->name);
		*/
	} else {
		/*
		** dprintf( D_FULLDEBUG, "Updating record for \"%s\"\n", rec->name );
		*/
	}

	update_context( templateRec->machine_context, rec->machine_context );
	rec->time_stamp = templateRec->time_stamp;
	rec->busy = templateRec->busy;
}

MACH_REC	*
create_mach_rec( MACH_REC* templateRec )
{
	MACH_REC	*newRec;

	newRec = (MACH_REC *)CALLOC( 1, sizeof(MACH_REC) );

	newRec->next = newRec;
	newRec->prev = newRec;

	if( templateRec == NULL ) {
		return newRec;
	}

	newRec->name = strdup( templateRec->name );
	newRec->net_addr = templateRec->net_addr;
	newRec->net_addr_type = templateRec->net_addr_type;
	newRec->machine_context = create_context();

		/* possible bug fix */
	newRec->prio = templateRec->prio;

	return newRec;
}

MACH_REC	*
find_rec( MACH_REC* newRec )
{
	MACH_REC	*ptr;

	for( ptr = MachineList->next; ptr->name; ptr = ptr->next ) {
		if( ptr->net_addr.s_addr == newRec->net_addr.s_addr ) {
			return ptr;
		}
	}
	return NULL;
}

void update_context( CONTEXT* src, CONTEXT* dst )
{
	int		i;

	for( i=0; i<src->len; i++ ) {
		store_stmt( src->data[i], dst );
		src->data[i] = NULL;
	}
	src->len = 0;
}

int		get_priorities();
void	sort_schedd_list();
void	negotiate(char*, ClassAd*);

void do_negotiations()
{
	ClassAd*	ptr; 
	int			idle;
	char*		scheddAddr;
	
	get_priorities();
	/*update_priorities();*/
	sort_schedd_list();

	dprintf(D_ALWAYS, "---------- Begin negotiating ----------\n");
	scheddAds->Open(); 
	for( ptr = scheddAds->Next(); ptr; ptr = scheddAds->Next() ) {
/*
		if( (int)time( (time_t *)0 ) - ptr->GetIntValue(ATTR_TIME >
													MachineUpdateInterval ) {
			continue;
		}
*/
		scheddAddr = new char[100]; 
		if(ptr->EvalString(ATTR_SCHEDD_IP_ADDR, NULL, scheddAddr) == 0)
		{	
			dprintf(D_ALWAYS,"Can't evaluate \"%s\"\n", ATTR_SCHEDD_IP_ADDR );
			delete scheddAddr;
			continue;
		}
		
		dprintf( D_FULLDEBUG, "Negotiating with %s\n", scheddAddr );
		negotiate( scheddAddr, ptr );
		delete scheddAddr; 
	}
	dprintf(D_ALWAYS, "---------- End negotiating\n");
}

#define SUCCESS 1
#define FAIL 0
#undef TRACE
#define TRACE dprintf(D_ALWAYS,"%s:%d\n",__FILE__,__LINE__)

int		call_schedd(char*, int);
int		simple_negotiate(char*, ClassAd*, ReliSock*, int&);

void negotiate( char* addr, ClassAd* rec )
{

	ReliSock	sock;
	int			prio, next_prio;
	int			status;
	ClassAd*	next; 

	if(next = rec->FindNext()) {
		if(next->LookupInteger(ATTR_PRIO, next_prio) == FALSE)
		{
			next_prio = NEG_INFINITY;
		} 
	} else {
		next_prio = NEG_INFINITY;
	}

	if( (ClientSock = call_schedd(addr,10)) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to SchedD on %s\n", addr );
		return;
	}
	dprintf( D_FULLDEBUG,
			"\tOpened ClientSock (%d) at %d\n",
			ClientSock, __LINE__
			);

	(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
	sock.attach_to_file_desc(ClientSock);
	sock.encode();
	status = sock.put(NEGOTIATE);
	sock.end_of_message();
	(void)alarm( 0 );				/* cancel alarm */

	if( !status ) {
		dprintf( D_ALWAYS, "\t1.Error negotiating with %s\n", addr); 
		return;
	}

	if(rec->LookupInteger(ATTR_PRIO, prio) == FALSE)
	{
		prio = 0;
	}
		
	while( prio >= next_prio ) {

		(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
		status = simple_negotiate( addr, rec, &sock, prio );
		(void)alarm( (unsigned)0 );

		if( status != SUCCESS ) {
			break;
		}
	}

		/* If we stop the negotiation while the scheduler still has jobs,
		   we must tell it we are done.  Otherwise, it ran out of jobs
		   to send us, and has already broken the connection. */
	if( status == SUCCESS ) {
		say_goodby( &sock, addr );
	}
}

int		send_to_startd(const char*);
int		send_to_accountant(const char*);
void	decrement_idle(char*, ClassAd*);

int simple_negotiate( char* addr, ClassAd* rec, ReliSock* sock, int& prio )
{
	int			op;
	MACH_REC	*server;
	CONTEXT		*job_context = NULL;
	ClassAd		ad;
	char 		*address,*final_string, *random_string;
	int 		sequence_number=0;

	for(;;) {
		(void)alarm( (unsigned)ClientTimeout );	/* reset the alarm every time */
		dprintf (D_PROTOCOL, "## 2. Negotiating with schedd %s\n", addr);
		sock->encode();
		if( !sock->put(SEND_JOB_INFO) ) {
			dprintf( D_ALWAYS, "\t3.Error negotiating with %s\n", addr );
			return ERROR;
		}
		sock->end_of_message();

		errno = 0;
		sock->decode();
		if( !sock->get(op) ) {
			dprintf( D_ALWAYS, "\t4.Error negotiating with %s\n", addr );
			dprintf( D_ALWAYS, "\terrno = %d\n", errno );
			return ERROR;
		}

		switch( op ) {
			case JOB_INFO:
				if( !ad.get(*sock) ) {
					dprintf(D_ALWAYS,
							"\t5.Error negotiating with %s\n", addr );
					return ERROR;
				}
				if (!sock->end_of_message()) {
					dprintf(D_ALWAYS, "end_of_message failed\n");
					return ERROR;
				}
				job_context = create_context();
				ad.MakeContext(job_context);

				if( server = find_server(job_context) ) {
					dprintf( D_ALWAYS, "\tAllowing %s to run on %s\n",
						addr, server->name);
					if( evaluate_string((char *) ATTR_STARTD_IP_ADDR, &address,
							server->machine_context,0 ) < 0 ) {
						dprintf(D_ALWAYS,"\tDidn't receive startd_ip_addr\n");
					}

					final_string=(char *)(malloc(SIZE_OF_FINAL_STRING*
							sizeof(char)));
					random_string=(char *)(malloc(SIZE_OF_CAPABILITY_STRING*
							sizeof(char)));
					if(get_random_string(&random_string)!=0)
					{ 
						EXCEPT("Error in getting random string\n");
					}

					sprintf(final_string, "%s %s#%d", address, 
								random_string,sequence_number);
					dprintf(D_ALWAYS, "\tmatch %s\n", final_string);
					dprintf(D_PROTOCOL, "\t## 4. Sending %s to startd ...\n", 
										final_string);
					if(send_to_startd(final_string)!=0) {
						dprintf( D_ALWAYS,
								"\t6.Error negotiating with %s\n", addr );
						free_context( job_context );
						free(final_string);
						free(random_string);
						free(address);
						return ERROR;
					}
					dprintf(D_PROTOCOL, "\t## 4. Sending %s to acntnt ...\n", 
							final_string);
					if(send_to_accountant(final_string) < 0) {
						dprintf(D_ALWAYS,
								"\tCan't send capability to accountant.\n");
					}

					sock->encode();
					if( !sock->put(PERMISSION) ) {
						dprintf( D_ALWAYS,
								"\t8.Error negotiating with %s\n", addr );
						free_context( job_context );
						free(final_string);
						free(random_string);
						free(address);
						return ERROR;
					}
					if (!sock->eom()) {
						dprintf(D_ALWAYS, "eom() failed\n");
						return ERROR;
					}
					dprintf(D_PROTOCOL, "\t## 3. Sending %s to schedd ...\n", 
						final_string);
					if( !sock->put(final_string) ) { /* dhaval */
						dprintf( D_ALWAYS,
								"\t9.Error negotiating with %s\n", addr );
						free_context( job_context );
						free(final_string);
						free(random_string);
						free(address);
						return ERROR;
					}
					if (!sock->eom()) {
						dprintf(D_ALWAYS, "eom() failed\n");
						return ERROR;
					}
					free(final_string);
					free(address);
					free(random_string);
					decrement_idle( addr, rec );
					server->busy = TRUE;
					prio -= 1;
					free_context( job_context );
					return SUCCESS;
				} else {	/* Can't find a server for it */
					/* PREEMPTION : dhruba */
					create_prospective_list(prio,job_context);

					sock->encode();
					if( !sock->put(REJECTED) ) {
						dprintf( D_ALWAYS,
						"\t9.Error negotiating with %s\n", addr );
						free_context( job_context );
						return ERROR;
					}
				}
				free_context( job_context );
				sock->end_of_message();
				break;
			case NO_MORE_JOBS:
				sock->end_of_message();
				/* xdrrec_skiprecord( xdrs ); */
				dprintf( D_FULLDEBUG, "Done negotiating with %s\n", addr );
				return FAIL;
			default:
				dprintf( D_ALWAYS, "\t%s sent unknown op (%d)\n", addr, op );
				return ERROR;
		}
	}
}

// Weiru
// Get priorities from the accountant for each schedd
int get_priorities()
{
    int         	sock;
	XDR         	xdr, *xdrs;
    int         	cmd = GIVE_PRIORITY;
    char*		 	scheddName;
    int         	prio;

	dprintf(D_PROTOCOL, "## 1. Getting priorities from the accountant...\n");
	if((sock = connect_to_accountant()) < 0)
	{
		dprintf(D_ALWAYS, "Can't connect to accountant\n");
		return 0; 
	}
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
   		if(!xdr_string(xdrs, &scheddName, 1000))
   		{
   			xdr_destroy( xdrs );
   			close(sock);
   			dprintf(D_ALWAYS, "can't receive machine addr\n");
   			return -1;
   		}
   		if(scheddName == NULL)
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
   			free(scheddName);
			return -1;
   		}
		dprintf(D_FULLDEBUG, "Got (%s, %d)\n", scheddName, prio);
	   	find_update_prio(scheddName, prio);
		free(scheddName); 
	}
   	return 0;
}

void find_update_prio(const char* scheddName, int p)
{
	ClassAd*	ad;
	char		tmp[100];
	
	if(scheddAds == NULL)
	{
		EXCEPT("ERROR: Can't find schedd ads");
	}
	ad = scheddAds->Lookup(scheddName);
	if(ad == NULL)
	{
		dprintf(D_ALWAYS, "Can't find schedd %s\n", scheddName);
		return;
	}
	sprintf(tmp, "%s = %d", ATTR_PRIO, p); 
	ad->InsertOrUpdate(tmp);
   	dprintf(D_FULLDEBUG, "%s new priority %d\n", scheddName, p);
}

int		update_prio(CONTEXT*, MACH_REC*);

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

int		up_down_incr(int); 

int update_prio( CONTEXT* nego, MACH_REC* rec )
{
	int		new_prio;
	int		inactive;
	ELEM	tmp;

	tmp.type = INT;
	tmp.val.integer_val = rec->prio;
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

	tmp.val.integer_val = new_prio;
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

int ScheddAdCmp(ClassAd* ad1, ClassAd* ad2, void*)
{
	int		prio1 = 0, prio2 = 0;
	char	name[100];
	
    ad1->LookupInteger(ATTR_PRIO, prio1);
	ad2->LookupInteger(ATTR_PRIO, prio2);
	return prio1 > prio2;
}

void sort_schedd_list()
{
	scheddAds->Sort(ScheddAdCmp); 
}

void prio_insert( MACH_REC* elem, MACH_REC* list )
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
find_next( MACH_REC* ptr )
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

call_schedd( char* schedd_address, int timeout )
{
	struct sockaddr_in	sin;
	int					scheduler;
	int					on = 1, off = 0;
	struct timeval		timer;
	fd_set				writefds;
	int					nfound;
	int					nfds;
	int					tmp_errno;

	if( (scheduler=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

	if(!schedd_address)
	{
		dprintf(D_ALWAYS, "\tdid'nt receive schedd_ip_addr\n");
		close(scheduler); 
		return -1; 
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
					schedd_address, tmp_errno );
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
		errno = 0;
		if( ioctl(scheduler,FIONBIO,(char *)&off) < 0 ) {
			if(errno == ECONNREFUSED)
			{
				(void)close(scheduler);
				return -1;
			} 
			EXCEPT( "ioctl, errno = %d", errno );
		}
		return scheduler;
	default:
		EXCEPT( "Select returns %d", nfound );
		return -1;
	}
}

int		check_bool(char*, CONTEXT*, CONTEXT*); 

MACH_REC	*
find_server( CONTEXT* job_context )
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
			dprintf( D_FULLDEBUG, "time stamp %d too old\n", ptr->time_stamp );
			continue;
		}

		if( !check_bool(ATTR_REQUIREMENTS,ptr->machine_context,job_context) ) {
			dprintf( D_FULLDEBUG, "machine requirements evaluate to FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "machine requirements evaluate to TRUE\n" );

		if( !check_bool(ATTR_REQUIREMENTS,job_context,ptr->machine_context)) {
			dprintf( D_FULLDEBUG, "job requirements evaluate to FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "job requirements evaluate to TRUE\n" );

		if( !check_bool(ATTR_PREFERENCES,job_context,ptr->machine_context) ) {
			dprintf( D_FULLDEBUG, "job preferences evaluate to FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "job preferences evaluate to TRUE\n" );

		/* TODO: Don't look at State in V6! */
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

		if( ptr->busy ) {
			continue;
		}

    /* We have to have a recent time stamp in order to use this machine JCP */
		if (curTime - ptr->time_stamp > 65 * MINUTE) {
			dprintf( D_FULLDEBUG, "time stamp %d too old\n", ptr->time_stamp );
			continue;
		}
		if( !check_bool(ATTR_REQUIREMENTS,ptr->machine_context,job_context) ) {
			dprintf( D_FULLDEBUG, "machine requirements evaluate to FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "machine requirements evaluate to TRUE\n" );

		if( !check_bool(ATTR_REQUIREMENTS,job_context,ptr->machine_context)) {
			dprintf( D_FULLDEBUG, "job requirements evaluate to FALSE\n" );
			continue;
		}
		dprintf( D_FULLDEBUG, "job requirements evaluate to TRUE\n" );

		/* TODO: Don't look at State in V6! */
		if( !check_string("State","NoJob",ptr->machine_context,(CONTEXT *)0)) {
			dprintf( D_FULLDEBUG, "State != NoJob\n" );
			continue;
		}

		return ptr;
	}

	return NULL;
}

check_bool( char* name, CONTEXT* context_1, CONTEXT* context_2 )
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

check_string(char* name, char* pattern, CONTEXT* context_1, CONTEXT* context_2)
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

void decrement_idle( char* addr, ClassAd* rec )
{
	int		idle;
	char	tmp[100];
	
	if( rec->LookupInteger(ATTR_IDLE_JOBS, idle) == FALSE ) {
		dprintf( D_ALWAYS, "Can't evaluate \"Idle\" for %s\n", addr );
		return;
	}

	sprintf(tmp, "%s = %d", ATTR_IDLE_JOBS, idle - 1); 
	rec->InsertOrUpdate(tmp); 
}

void free_mach_rec( MACH_REC* rec )
{
	if( rec->name ) {
		FREE( rec->name );
	}

	if( rec->machine_context ) {
		free_context( rec->machine_context );
	}
	FREE( (char *)rec );
}

int connect_to_collector()
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
		dprintf(D_ALWAYS,"\t(connect returns %d, errno = %d)\n",status,errno);
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
up_down_incr(int old_prio )
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

static int compare(const void* a1, const void* a2) /* compares priorities */
{
	if ( *(int*)a1 > *(int*)a2 ) return 1;/*a1 points to prospect.prio*/
	if ( *(int*)a1 < *(int*)a2 ) return -1;
	return 0;
}
	

void create_prospective_list( int prio , CONTEXT* job_context)
{
	MACH_REC    *ptr, *p; 
	Prospect*	prospect;
	char* cli, flag;
	int count = 0, curPrio,i;
	int curTime, jobTime;
	int     sd;
	ReliSock	sock;
	int     cmd;
	int 	jobUniverse;

	curTime =  time((time_t *)0);
	dprintf(D_PREEMPT, "PREEMPTION_LIMIT = %d minutes\n", preemptionTime/60);
	dprintf(D_PREEMPT, "PREEMPTION_LIMIT_VANILLA = %d minutes\n", VanillapreemptionTime/60);

	for( ptr=MachineList->next; ptr->name; ptr = ptr->next ) 
		count++;
	if ( count == 0 ) return;
	prospect = (Prospect*)malloc(count * sizeof(Prospect));
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
		if(evaluate_int(ATTR_JOB_UNIVERSE,&jobUniverse,ptr->machine_context,(CONTEXT *)0)<0) 
		{
			dprintf(D_PREEMPT,"%s could not evaluate %s\n",ptr->name,
					ATTR_JOB_UNIVERSE);
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
		if( (sd = do_connect(prospect[0].rec->name, "condor_startd", 
							   START_PORT)) < 0 ) {
			dprintf( D_ALWAYS, 
					"Can't connect to startd on %s\n", 
					prospect[0].rec->name );
		} else {
			sock.attach_to_file_desc(sd);
			sock.encode();

			cmd = CKPT_FRGN_JOB;
			ASSERT( sock.code(cmd) );
			ASSERT( sock.end_of_message() );

			dprintf( D_ALWAYS,
					"Sent CKPT_FRGN_JOB command to startd on %s\n", 
					prospect[0].rec->name );
			/* xdr_destroy( xdrs ); */
			/* close( sd ); */
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
say_goodby( ReliSock *sock, const char *name )
{
	int		status;

	dprintf( D_FULLDEBUG, "\tSending END_NEGOTIATE to %s\n", name );

	(void)alarm( (unsigned)ClientTimeout );	/* don't hang here forever */
	status = sock->put( END_NEGOTIATE );
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

/* Use get_random_int from condor_util_lib.  -Jim B. */

int get_random_number()
{
	return get_random_int()%100;
}

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

sprintf(str,"%02d%02d%02d%02d%02d",random1,random2,random3,random4,random5);
*temp=strdup(str);
return 0;
}

/* addr is in the format "<xxx.xxx.xxx.xxx:xxxx> xxxxxxxxxxxxxxx#xxx" */
int send_to_startd(const char *addr)
{
	ReliSock sock;
	int		sd,port;
	int		cmd=MATCH_INFO;
	char	*str;

	if((sd = do_connect(addr,"condor_startd",START_PORT))<0)
	{ 
		return -1;
	}
	dprintf(D_FULLDEBUG, "\tconnected to startd\n");

	sock.attach_to_file_desc(sd);
	sock.encode();

	if(!sock.code(cmd) ) {
		dprintf(D_ALWAYS, "can't send command MATCH_INFO to startd\n");
		return -1;
	}

	str = strchr(addr, ' ');
	str++;
	if(!sock.code(str)) {
		return -1;
	}

	if(!sock.eom() ) {
		dprintf(D_ALWAYS, "can't send eom to startd\n");		
		return -1;
	}

	dprintf(D_FULLDEBUG, "\tsend match %s to startd\n", str);

	return 0;
}

int send_to_accountant(const char* capability)
{
	int		sd;
	XDR		xdr, *xdrs;
	int		cmd = MATCH_INFO;

	if(!AccountantHost)
	{
		dprintf(D_FULLDEBUG, "\tno accountant in configuration file\n");
		return 0;
	}
	if((sd = connect_to_accountant()) < 0)
	{
		dprintf(D_ALWAYS, "\tCan't connect to accountant\n");
		return 0;
	}
	dprintf(D_FULLDEBUG, "\tconnected to accountant\n");

	xdrs = xdr_Init(&sd,&xdr);
	xdrs->x_op = XDR_ENCODE;

	if(!xdr_int(xdrs,&cmd) ) {
		xdr_destroy( xdrs );
		close(sd);
		dprintf(D_ALWAYS, "can't send command MATCH_INFO to startd\n");
		return -1;
	}

	if(!snd_string(xdrs,capability,TRUE)) {
		xdr_destroy( xdrs );
		close(sd);
		return -1;
	}

	dprintf(D_FULLDEBUG, "\tsend match %s to accountant\n", capability);
	xdr_destroy( xdrs );
	close(sd);
	return 0;
}
