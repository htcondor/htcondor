/* Copyright Condor Team, 1997 */

#include "condor_common.h"

#ifndef WIN32
#include <std.h>
#if defined(Solaris251)
#include <strings.h>
#endif
#include "_condor_fix_types.h"
#include <sys/socket.h>
#include "condor_fix_signal.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif   /* ifndef WIN32 */

#include "condor_debug.h"
#include "condor_config.h"
#define _NO_EXTERN_DAEMON_CORE 1	
#include "condor_daemon_core.h"

static char *_FileName_ = __FILE__;  // used by EXCEPT 

// Externs to Globals
extern char* mySubSystem;	// the subsys ID, such as SCHEDD, STARTD, etc. 
extern "C" int Termlog;
extern int DebugFlags;

// External protos
extern int main_init(int argc, char *argv[]);	// old main()

// Globals
int		Foreground = 0;		// run in background by default
char*	myName;				// set to argv[0]
DaemonCore*	daemonCore;

main( int argc, char** argv )
{
	char**	ptr;
	int		command_port = -1;
	int		dcargs = 0;		// number of daemon core command-line args found
	char*	ptmp;
	ReliSock* rsock;	// tcp command socket
	SafeSock* ssock;	// udp command socket
#ifdef NOT_YET_WIN32
	char	buf[_POSIX_PATH_MAX];
	char	*inbuf, *outbuf;
	int		i,j;
#endif

	// instantiate a daemon core
	daemonCore = new DaemonCore();

#ifndef WIN32
		// Handle Unix signals

		// Block all signals now.  We'll unblock them right before we
		// do the select.
	sigset_t fullset;
	sigfillset( &fullset );
	sigprocmask( SIG_SETMASK, &fullset, NULL );

		// TODO: Install signal handlers for SIGTERM, SIGQUIT and
		// SIGHUP to raise the DaemonCore versions, and register
		// handlers for those DC signals to call main_shutdown_* and
		// main_config. 
#endif

	// set myName to be argv[0] with the path stripped off
	if ( (ptmp=strrchr(argv[0],'/')) == NULL )			
		ptmp=strrchr(argv[0],'\\');
	if ( ptmp )
		myName = ++ptmp;
	else
		myName = argv[0];

	// strip off any daemon-core specific command line arguments
	// from the front of the command line.
	for(ptr = argv + 1; *ptr; ptr++)
	{
		int done = FALSE;

		if(ptr[0][0] != '-') {
			break;
		}
		switch(ptr[0][1])
		{
		  case 'f':		// run in foreground
			Foreground = 1;
			dcargs++;
			break;
		  case 't':		// log to terminal (stderr)
			Termlog = 1;
			dcargs++;
			break;
		  case 'b':		// run in background (default)
			Foreground = 0;
			dcargs++;
			break;
		  case 'p':		// use well-known port for command socket				
						// note: "-p 0" means _no_ command socket
			command_port = atoi( *(++ptr) );
			dcargs += 2;
			break;
		  default:
			done = TRUE;
			break;	
		}
		if ( done == TRUE )
			break;		// break out of for loop
	}

	// call config so we can call param
	config(NULL);

	// Set up logging
	dprintf_config(mySubSystem,2);
	dprintf(D_ALWAYS,"******************************************************\n");
	dprintf(D_ALWAYS,"** %s (CONDOR_%s) STARTING UP\n",myName,mySubSystem);
	dprintf(D_ALWAYS,"******************************************************\n");

	// we want argv[] stripped of daemoncore options
	ptmp = argv[0];			// save a temp pointer to argv[0]
	argv = --ptr;			// make space for argv[0]
	argv[0] = ptmp;			// set argv[0]
	argc -= dcargs;
	if ( argc < 1 )
		argc = 1;

	// run as condor 99.9% of the time, so studies tell us.
	set_condor_priv();

	// chdir to the LOG directory so that if we dump a core
	// it will go there.
	ptmp = param("LOG");
	if ( ptmp ) {
		if ( chdir(ptmp) < 0 ) {
			EXCEPT("cannot chdir to dir <%s>",ptmp);
		}
	} else {
		EXCEPT("No LOG directory specified in config file(s)");
	}
	free(ptmp);

	// Arrange to run in the background.
	// SPECIAL CASE: if this is the MASTER, and we are to run in the background,
	// then register ourselves as an NT Service.
	if (!Foreground) {
#ifdef WIN32
		if ( strcmp(mySubSystem,"MASTER") == 0 ) {
			// TODO here we call the Service code
		}
#else	// UNIX
		// on unix, background means just fork ourselves
		if ( fork() ) {
			// parent
			exit(0);
		}
#endif	// of else of ifdef WIN32
	}	// if if !Foreground




#ifdef NOT_YET_WIN32
	// Here we handle inheritance of sockets, file descriptors, and/or handles
	// from our parent.  This is done via an environment variable "CONDOR_INHERIT".
	// If this variable does not exist, it usually means our parent is not a daemon core
	// process.  
	// CONDOR_INHERIT has the following structure:
	//		parent handle/pid
	//		
	{
	DWORD	EnvVarSize;		// size in bytes of environment variable
	LPWSAPROTOCOL_INFO	pProtoInfo;	// pointer to WSAPROTOCOL_INFO structure

	inbuf = new char[ 3000 ];
	if ( EnvVarSize=GetEnvironmentVariable("CONDOR_INHERIT",inbuf,3000) ) {

		// found it, now convert it from ascii to binary
		pProtoInfo = (LPWSAPROTOCOL_INFO) malloc(sizeof(WSAPROTOCOL_INFO));
		outbuf = (char *) pProtoInfo;
		for (i=0, ptmp=bigbuf; i < (EnvVarSize / 3); i++, ptmp+=3) {
			sscanf(ptmp,"%x-",&j);
			outbuf[i] = (char) j;
		}
		delete []inbuf;
	
		// now open it by calling the special "duplicate" Relisock constructor
	}
	}
#endif

	// SETUP COMMAND SOCKET

	if ( command_port != 0 ) {
		dprintf(D_DAEMONCORE,"Setting up command socket\n");
		
		// we want a command port for this process, so create
		// a tcp and a udp socket to listen on.
		if ( command_port == -1 ) {
			// choose any old port (dynamic port)
			rsock = new ReliSock( 0 );
			// now open a SafeSock _on the same port_ choosen above
			if ( rsock )
				ssock = new SafeSock( rsock->get_port() );
		} else {
			// use well-known port specified by command_port
			rsock = new ReliSock( command_port );
			ssock = new SafeSock( command_port );
		}
		if ( !rsock ) {
			EXCEPT("Unable to create command Relisock");
		}
		if ( !ssock ) {
			EXCEPT("Unable to create command SafeSock");
		}

		// now register these new command sockets
		daemonCore->Register_Command_Socket( (Stream*)rsock );
		daemonCore->Register_Command_Socket( (Stream*)ssock );

		// now register any DaemonCore "default" handlers

		// register the command handler to take care of signals
		daemonCore->Register_Command(DC_RAISESIGNAL,"DC_RAISESIGNAL",
				(CommandHandlercpp)daemonCore->HandleSigCommand,
				"HandleSigCommand()",daemonCore,WRITE);

		
	}
	
	// call the daemons main_init()
	main_init( argc, argv );

	// now call the driver.  we never return from the driver (infinite loop).
	daemonCore->Driver();

	// should never get here
	EXCEPT("returned from Driver()");
	return FALSE;
}	
