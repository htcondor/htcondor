/* Copyright Condor Team, 1997 */

#include "condor_common.h"
#include "condor_config.h"

#include "condor_debug.h"
static char *_FileName_ = __FILE__;  // used by EXCEPT 

#define _NO_EXTERN_DAEMON_CORE 1	
#include "condor_daemon_core.h"


// Externs to Globals
extern char* mySubSystem;	// the subsys ID, such as SCHEDD, STARTD, etc. 

// External protos
extern int main_init(int argc, char *argv[]);	// old main()
extern int main_config();
extern int main_shutdown_fast();
extern int main_shutdown_graceful();

// Globals
int		Foreground = 0;		// run in background by default
char*	myName;				// set to argv[0]
DaemonCore*	daemonCore;


#ifndef WIN32
void
unix_sighup(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGHUP );
}

void
unix_sigterm(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGTERM );
}

void
unix_sigquit(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGQUIT );
}

void
unix_sigchld(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGCHLD );
}
#endif WIN32


int
handle_dc_sighup( Service*, int )
{
	char *ptmp;

	dprintf( D_ALWAYS, "Got SIGHUP.  Re-reading config files.\n" );

		// Actually re-read the files...  Added by Derek Wright on
		// 12/8/97 (long after this function was first written... 
		// nice goin', Todd).  *grin*
	config(NULL);

	// Reinitialize logging system; after all, LOG may have been changed.
	dprintf_config(mySubSystem,2);
	
	// again, chdir to the LOG directory so that if we dump a core
	// it will go there.  the location of LOG may have changed, so redo it here.
	ptmp = param("LOG");
	if ( ptmp ) {
		if ( chdir(ptmp) < 0 ) {
			EXCEPT("cannot chdir to dir <%s>",ptmp);
		}
	} else {
		EXCEPT("No LOG directory specified in config file(s)");
	}
	free(ptmp);

	// call this daemon's specific main_config()
	main_config();

	return TRUE;
}


int
handle_dc_sigterm( Service*, int )
{
	dprintf(D_ALWAYS, "Got SIGTERM. Performing graceful shutdown.\n");
	main_shutdown_graceful();
	return TRUE;
}


int
handle_dc_sigquit( Service*, int )
{
	dprintf(D_ALWAYS, "Got SIGQUIT.  Performing fast shutdown.\n");
	main_shutdown_fast();
	return TRUE;
}

// This is the main entry point for daemon core.  On WinNT, however, we
// have a different, smaller main which checks if "-f" is ommitted from
// the command line args of the condor_master, in which case it registers as 
// an NT service.
#ifdef WIN32
int dc_main( int argc, char** argv )
#else
int main( int argc, char** argv )
#endif
{
	char**	ptr;
	int		command_port = -1;
	int		dcargs = 0;		// number of daemon core command-line args found
	char*	ptmp;
	char*	ptmp1;
	int		i;
	ReliSock* rsock = NULL;	// tcp command socket
	SafeSock* ssock = NULL;	// udp command socket

#ifdef WIN32
	// Call SetErrorMode so that Win32 "critical errors" and such do not open up a
	// dialog window!
	::SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
#endif

	// instantiate a daemon core
	// have lots of pid table hash buckets if we're the SCHEDD, since the
	// SCHEDD could have lots of children...
	if ( strcmp(mySubSystem,"SCHEDD") == 0 )
		daemonCore = new DaemonCore(500);
	else
		daemonCore = new DaemonCore();

#ifndef WIN32
		// Handle Unix signals

		// Block all signals now.  We'll unblock them right before we
		// do the select.
	sigset_t fullset;
	sigfillset( &fullset );
	sigprocmask( SIG_SETMASK, &fullset, NULL );

		// Install these signal handlers with a default mask
		// of all signals blocked when we're in the handlers.
	install_sig_handler_with_mask(SIGQUIT, &fullset, unix_sigquit);
	install_sig_handler_with_mask(SIGHUP, &fullset, unix_sighup);
	install_sig_handler_with_mask(SIGTERM, &fullset, unix_sigterm);
	install_sig_handler_with_mask(SIGCHLD, &fullset, unix_sigchld);
	install_sig_handler(SIGPIPE, SIG_IGN );
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
	i = 0;
	for(ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++)
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
		  case 'c':		// specify directory where config file lives
			ptmp = *(++ptr);
			dcargs += 2;
			ptmp1 = (char *)malloc( strlen(ptmp) + 25 );
			if ( ptmp1 ) {
				sprintf(ptmp1,"CONDOR_CONFIG=%s\0",ptmp);
				putenv(ptmp1);
			}
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
	dprintf(D_ALWAYS,"** PID = %lu\n",daemonCore->getpid());
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
		// Disconnect from the console
		FreeConsole();
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

	// Now take care of inheriting resources from our parent
	daemonCore->Inherit(rsock,ssock);

	// SETUP COMMAND SOCKET

	if ( command_port != 0 ) {
		dprintf(D_DAEMONCORE,"Setting up command socket\n");
		
		// we want a command port for this process, so create
		// a tcp and a udp socket to listen on if we did not
		// already inherit them above.
		// If rsock/ssock are not NULL, it means we inherited them from our parent
		if ( rsock == NULL && ssock == NULL ) {
			rsock = new ReliSock;
			ssock = new SafeSock;
			if ( !rsock ) {
				EXCEPT("Unable to create command Relisock");
			}
			if ( !ssock ) {
				EXCEPT("Unable to create command SafeSock");
			}
			if ( command_port == -1 ) {
				// choose any old port (dynamic port)
				if ( !rsock->bind() ) {
					EXCEPT("Failed to bind to command ReliSock");
				}
				// now open a SafeSock _on the same port_ choosen above
				if ( !ssock->bind(rsock->get_port()) ) {
					// failed to bind on the same port -- find free UDP port first
					if ( !ssock->bind() ) {
						EXCEPT("Failed to bind on SafeSock");
					}
					rsock->close();
					if ( !rsock->bind(ssock->get_port()) ) {
						// failed again -- keep trying
						bool bind_succeeded = false;
						for (int temp_port=1024; !bind_succeeded && temp_port < 4096; temp_port++) {
							rsock->close();
							ssock->close();
							if ( rsock->bind(temp_port) && ssock->bind(temp_port) ) {
								bind_succeeded = true;
							}
						}
						if (!bind_succeeded) {
							EXCEPT("Failed to find available port for command sockets");
						}
					}
				}
				if ( !rsock->listen() ) {
					EXCEPT("Failed to post listen on command ReliSock");
				}
			} else {
				// use well-known port specified by command_port
				int on = 1;
	
				// Set options on this socket, SO_REUSEADDR, so that
				// if we are binding to a well known port, and we crash, we can be
				// restarted and still bind ok back to this same port. -Todd T, 11/97
				if( (!rsock->setsockopt(SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) ||
					(!ssock->setsockopt(SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) ) {
					EXCEPT("setsockopt() SO_REUSEADDR failed\n");
				}
				
				if ( (!rsock->listen(command_port)) ||
					 (!ssock->bind(command_port)) ) {
					EXCEPT("Failed to bind to or post listen on command socket(s)");
				}
				
			}
		}

		// now register these new command sockets
		daemonCore->Register_Command_Socket( (Stream*)rsock );
		daemonCore->Register_Command_Socket( (Stream*)ssock );
		dprintf(D_ALWAYS,"DaemonCore: Command Socket at %s\n",
			daemonCore->InfoCommandSinfulString());

		// now register any DaemonCore "default" handlers

		// register the command handler to take care of signals
		daemonCore->Register_Command(DC_RAISESIGNAL,"DC_RAISESIGNAL",
				(CommandHandlercpp)daemonCore->HandleSigCommand,
				"HandleSigCommand()",daemonCore,IMMEDIATE_FAMILY);

		// this handler receives process exit info
		daemonCore->Register_Command(DC_PROCESSEXIT,"DC_PROCESSEXIT",
				(CommandHandlercpp)daemonCore->HandleProcessExitCommand,
				"HandleProcessExitCommand()",daemonCore,IMMEDIATE_FAMILY);
	}
	
		// Install DaemonCore signal handlers common to all daemons.
	daemonCore->Register_Signal( DC_SIGHUP, "DC_SIGHUP", 
								 (SignalHandler)handle_dc_sighup,
								 "handle_dc_sighup" );
	daemonCore->Register_Signal( DC_SIGQUIT, "DC_SIGQUIT", 
								 (SignalHandler)handle_dc_sigquit,
								 "handle_dc_sigquit" );
	daemonCore->Register_Signal( DC_SIGTERM, "DC_SIGTERM", 
								 (SignalHandler)handle_dc_sigterm,
								 "handle_dc_sigterm" );

	// call the daemons main_init()
	main_init( argc, argv );

	// now call the driver.  we never return from the driver (infinite loop).
	daemonCore->Driver();

	// should never get here
	EXCEPT("returned from Driver()");
	return FALSE;
}	

#ifdef WIN32
int 
main( int argc, char** argv)
{
	char **ptr;
	int i;

	// Scan our command line arguments for a "-f".  If we don't find a "-f",
	// then we want to register as an NT Service.
	i = 0;
	for( ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++)
	{
		int done = FALSE;

		if(ptr[0][0] != '-') {
			break;
		}
		switch(ptr[0][1])
		{
		  case 'f':		// run in foreground
			Foreground = 1;
			break;
		  case 't':		// log to terminal (stderr)
			break;
		  case 'b':		// run in background (default)
			break;
		  case 'p':		// use well-known port for command socket				
			ptr++;
			break;
		  case 'c':		// specify directory where config file lives
			ptr++;
			break;
		  default:
			done = TRUE;
			break;	
		}
		if ( done == TRUE )
			break;		// break out of for loop
	}
	if ( (Foreground != 1) && (strcmp(mySubSystem,"MASTER") == 0) ) {
		main_init(-1,NULL);	// passing the master main_init a -1 will register as an NT service
		return 1;
	} else {
		return(dc_main(argc,argv));
	}
}
#endif // of ifdef WIN32

