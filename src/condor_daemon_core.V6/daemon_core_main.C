/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/* Copyright Condor Team, 1997 */

#include "condor_common.h"
#include "condor_config.h"
#include "basename.h"
#include "condor_version.h"
#include "limit.h"
#include "sig_install.h"

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
static int is_master = 0;
char*	logDir = NULL;
char*	pidFile = NULL;
char*	addrFile = NULL;
#ifdef WIN32
int line_where_service_stopped = 0;
#endif


void
clean_files()
{
		// If we created a pid file, remove it.
	if( pidFile ) {
		if( unlink(pidFile) < 0 ) {
			dprintf( D_ALWAYS, 
					 "DaemonCore: ERROR: Can't delete pid file %s\n",
					 pidFile );
		} else {
			if( DebugFlags & (D_FULLDEBUG | D_DAEMONCORE) ) {
				dprintf( D_DAEMONCORE, "Removed pid file %s\n", pidFile );
			}
		}
	}

	if( addrFile ) {
		if( unlink(addrFile) < 0 ) {
			dprintf( D_ALWAYS, 
					 "DaemonCore: ERROR: Can't delete address file %s\n",
					 addrFile );
		} else {
			if( DebugFlags & (D_FULLDEBUG | D_DAEMONCORE) ) {
				dprintf( D_DAEMONCORE, "Removed address file %s\n", 
						 addrFile );
			}
		}
			// Since we param()'ed for this, we need to free it now.
		free( addrFile );
	}
}


// All daemons call this function when they want daemonCore to really
// exit.  Put any daemon-wide shutdown code in here.   
extern "C" {
void
DC_Exit( int status )
{
		// First, delete any files we might have created, like the
		// address file or the pid file.
	clean_files();

		// Now, delete the daemonCore object, since we allocated it. 
	delete daemonCore;
	daemonCore = NULL;

		// Finally, exit with the status we were given.
	exit( status );
}
} /* extern "C" */

void
drop_addr_file()
{
	FILE	*ADDR_FILE;
	char	addr_file[100];

	sprintf( addr_file, "%s_ADDRESS_FILE", mySubSystem );

	if( addrFile ) {
		free( addrFile );
	}
	addrFile = param( addr_file );

	if( addrFile ) {
		if( (ADDR_FILE = fopen(addrFile, "w")) ) {
			fprintf( ADDR_FILE, "%s\n", 
					 daemonCore->InfoCommandSinfulString() );
			fclose( ADDR_FILE );
		} else {
			dprintf( D_ALWAYS,
					 "DaemonCore: ERROR: Can't open address file %s\n",
					 addrFile );
		}
	}
}


void
drop_pid_file() 
{
	FILE	*PID_FILE;

	if( !pidFile ) {
			// There's no file, just return
		return;
	}

	if( (PID_FILE = fopen(pidFile, "w")) ) {
		fprintf( PID_FILE, "%lu\n", daemonCore->getpid() );
		fclose( PID_FILE );
	} else {
		dprintf( D_ALWAYS,
				 "DaemonCore: ERROR: Can't open pid file %s\n",
				 pidFile );
	}
}


void
do_kill() 
{
#ifndef WIN32
	FILE	*PID_FILE;
	pid_t 	pid = 0;
	char	*log, *tmp;

	if( !pidFile ) {
		fprintf( stderr, 
				 "DaemonCore: ERROR: no pidfile specified for -kill\n" );
		exit( 1 );
	}
	if( pidFile[0] != '/' ) {
			// There's no absolute path, append the LOG directory
		if( (log = param("LOG")) ) {
			tmp = (char*)malloc( (strlen(log) + strlen(pidFile) + 2) * 
								 sizeof(char) );
			sprintf( tmp, "%s/%s", log, pidFile );
			free( log );
			pidFile = tmp;
		}
	}
	if( (PID_FILE = fopen(pidFile, "r")) ) {
		fscanf( PID_FILE, "%lu", &pid ); 
		fclose( PID_FILE );
	} else {
		fprintf( stderr, 
				 "DaemonCore: ERROR: Can't open pid file %s for reading\n",
				 pidFile );
		exit( 1 );
	}
	if( pid > 0 ) {
			// We have a valid pid, try to kill it.
		if( kill(pid, SIGTERM) < 0 ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: can't send SIGTERM to pid (%lu)\n",  
					 pid );
			fprintf( stderr, 
					 "\terrno: %d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		} 
			// kill worked, now, make sure the thing is gone.  Keep
			// trying to send signal 0 (test signal) to the process
			// until that fails.  
		while( kill(pid, 0) == 0 ) {
			sleep( 3 );
		}
			// Everything's cool, exit normally.
		exit( 0 );
	} else {  	// Invalid pid
		fprintf( stderr, 
				 "DaemonCore: ERROR: pid (%lu) in pid file (%s) is invalid.\n",
				 pid, pidFile );	
		exit( 1 );
	}
#endif  // of ifndef WIN32
}


void
set_log_dir()
{
	if( !logDir ) {
		return;
	}
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
	struct stat stats;
	if( stat(logDir, &stats) >= 0 ) {
		if( ! S_ISDIR(stats.st_mode) ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: %s exists and is not a directory.\n", 
					 logDir );
			exit( 1 );
		}
	} else {
		if( mkdir(logDir, mode) < 0 ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: can't create directory %s\n", 
					 logDir );
			fprintf( stderr, 
					 "\terrno: %d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		}
	}
	config_insert( "LOG", logDir );
}


// See if we should set the limits on core files.  If the parameter is
// defined, do what it says.  Otherwise, do nothing.
void
check_core_files()
{
#ifndef WIN32	// no "core" files on NT
	char* tmp;
	if( (tmp = param("CREATE_CORE_FILES")) ) {
		if( *tmp == 't' || *tmp == 'T' ) {
			limit( RLIMIT_CORE, RLIM_INFINITY );
		} else {
			limit( RLIMIT_CORE, 0 );
		}
		free( tmp );
	}
#endif
}


int
handle_config_val( Service*, int, Stream* stream ) 
{
	char *param_name = NULL, *tmp;

	stream->decode();

	if( ! stream->code(param_name) ) {
		dprintf( D_ALWAYS, "Can't read parameter name\n" );
		free( param_name );
		return FALSE;
	}

	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( param_name );
		return FALSE;
	}

	stream->encode();

	tmp = param( param_name );
	if( ! tmp ) {
		dprintf( D_FULLDEBUG, 
				 "Got CONFIG_VAL request for unknown parameter (%s)\n", 
				 param_name );
		if( ! stream->put("Not defined") ) {
			dprintf( D_ALWAYS, "Can't send reply for CONFIG_VAL\n" );
			return FALSE;
		}
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for CONFIG_VAL\n" );
			return FALSE;
		}
		return FALSE;
	} else {
		if( ! stream->code(tmp) ) {
			dprintf( D_ALWAYS, "Can't send reply for CONFIG_VAL\n" );
			free( tmp );
			return FALSE;
		}
		free( tmp );
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for CONFIG_VAL\n" );
			return FALSE;
		}
	}
	return TRUE;
}

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

void
unix_sigusr1(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGUSR1 );
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

		// If we're supposed to be using our own log file, reset that here. 
	if( logDir ) {
		set_log_dir();
	}

		// See if we're supposed to be allowing core files or not
	check_core_files();

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

	// call daemon-core's ReInit
	daemonCore->ReInit();

	// Re-drop the address file, if it's defined, just to be safe.
	drop_addr_file();

		// Re-drop the pid file, if it's requested, just to be safe.
	if( pidFile ) {
		drop_pid_file();
	}

	// call this daemon's specific main_config()
	main_config();

	return TRUE;
}


int
handle_dc_sigterm( Service*, int )
{
	static int been_here = FALSE;
	if( been_here ) {
		dprintf( D_FULLDEBUG, 
				 "Got SIGTERM, but we've already done graceful shutdown.  Ignoring.\n" );
		return TRUE;
	}
	been_here = TRUE;

	dprintf(D_ALWAYS, "Got SIGTERM. Performing graceful shutdown.\n");

#ifdef WIN32
	if ( line_where_service_stopped != 0 ) {
		dprintf(D_ALWAYS,"Line where service stopped = %d\n",
			line_where_service_stopped);
	}
#endif

	int timeout = 30 * MINUTE;
	char* tmp = param( "SHUTDOWN_GRACEFUL_TIMEOUT" );
	if( tmp ) {
		timeout = atoi( tmp );
		free( tmp );
	}
	daemonCore->Register_Timer( timeout, 0, 
								(TimerHandler)main_shutdown_fast,
								"main_shutdown_fast" );
	dprintf( D_FULLDEBUG, 
			 "Started timer to call main_shutdown_fast in %d seconds\n", 
			 timeout );
	main_shutdown_graceful();
	return TRUE;
}


int
handle_dc_sigquit( Service*, int )
{
	static int been_here = FALSE;
	if( been_here ) {
		dprintf( D_FULLDEBUG, 
				 "Got SIGQUIT, but we've already done fast shutdown.  Ignoring.\n" );
		return TRUE;
	}
	been_here = TRUE;
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
	char	*ptmp, *ptmp1;
	int		i;
	AuthSock* rsock = NULL;	// tcp command socket
	SafeSock* ssock = NULL;	// udp command socket
	int		wantsKill = FALSE, wantsQuiet = FALSE;
	char	*logAppend = NULL;

#ifdef WIN32
		// Call SetErrorMode so that Win32 "critical errors" and such
		// do not open up a dialog window!
	::SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
#else // UNIX

		// Set a umask value so we get reasonable permissions on the
		// files we create.  Derek Wright <wright@cs.wisc.edu> 3/3/98
	umask( 022 );

		// Handle Unix signals
		// Block all signals now.  We'll unblock them right before we
		// do the select.
	sigset_t fullset;
	sigfillset( &fullset );
	// We do not want to block the following signals ----
	   sigdelset(&fullset, SIGSEGV);	// so we get a core right away
	   sigdelset(&fullset, SIGABRT);	// so assert() failures drop core right away
	   sigdelset(&fullset, SIGILL);		// so we get a core right away
	   sigdelset(&fullset, SIGBUS);		// so we get a core right away
	   sigdelset(&fullset, SIGFPE);		// so we get a core right away
	   sigdelset(&fullset, SIGTRAP);	// so gdb works when it uses SIGTRAP
	sigprocmask( SIG_SETMASK, &fullset, NULL );

		// Install these signal handlers with a default mask
		// of all signals blocked when we're in the handlers.
	install_sig_handler_with_mask(SIGQUIT, &fullset, unix_sigquit);
	install_sig_handler_with_mask(SIGHUP, &fullset, unix_sighup);
	install_sig_handler_with_mask(SIGTERM, &fullset, unix_sigterm);
	install_sig_handler_with_mask(SIGCHLD, &fullset, unix_sigchld);
	install_sig_handler_with_mask(SIGUSR1, &fullset, unix_sigusr1);
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif // WIN32

		// Figure out if we're the master or not.  The master is a
		// special case daemon in many ways, so we want to just set a
		// flag that we can check instead of doing this strcmp all the
		// time.  -Derek Wright 1/13/98
	if ( strcmp(mySubSystem,"MASTER") == 0 ) {
		is_master = 1;
	}

		// set myName to be argv[0] with the path stripped off
	myName = basename(argv[0]);

	// strip off any daemon-core specific command line arguments
	// from the front of the command line.
	i = 0;
	for(ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++) {
		int done = FALSE;

		if(ptr[0][0] != '-') {
			break;
		}
		switch(ptr[0][1]) {
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
		case 'p':		
			if( ptr[0][2] && ptr[0][2] == 'i' ) {
					// Specify a pidfile
				ptr++;
				if( ptr && *ptr ) {
					pidFile = *ptr;
					dcargs += 2;
				} else {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -pidfile needs another argument.\n" );
					fprintf( stderr, 
							 "   Please specify a filename to store the pid.\n" );
					exit( 1 );
				}
			} else {
					// use well-known port for command socket				
					// note: "-p 0" means _no_ command socket
				ptr++;
				if( ptr && *ptr ) {
					command_port = atoi( *ptr );
					dcargs += 2;
				} else {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -port needs another argument.\n" );
					fprintf( stderr, 
					   "   Please specify the port to use for the command socket.\n" );

					exit( 1 );
				}
			}
			break;
		case 'c':		// specify directory where config file lives
			ptr++;
			if( ptr && *ptr ) {
				ptmp = *ptr;
				dcargs += 2;
				ptmp1 = (char *)malloc( strlen(ptmp) + 25 );
				if ( ptmp1 ) {
					sprintf(ptmp1,"CONDOR_CONFIG=%s\0",ptmp);
					putenv(ptmp1);
				}
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -config needs another argument.\n" );
				fprintf( stderr, 
						 "   Please specify the filename of the config file.\n" );
				exit( 1 );
			}				  
			break;
		case 'v':		// display version info and exit
			printf( "%s\n", CondorVersion() );
			exit(0);
			break;
		case 'l':
			ptr++;
			if( ptr && *ptr ) {
				logDir = *ptr;
				dcargs += 2;
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -log needs another argument.\n" );
				fprintf( stderr, 
						 "   Please specify a directory to use for logging.\n" );
				exit( 1 );
			}				  
			break;
#ifndef WIN32
		case 'k':
			ptr++;
			if( ptr && *ptr ) {
				pidFile = *ptr;
				wantsKill = TRUE;
				dcargs += 2;
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -kill needs another argument.\n" );
				fprintf( stderr, 
				  "   Please specify a file that holds the pid you want to kill.\n" );
				exit( 1 );
			}
			break;
#endif
		case 'q':
			wantsQuiet = TRUE;
			dcargs++;
			break;			
		case 'a':
			ptr++;
			if( ptr && *ptr ) {
				logAppend = *ptr;
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -append needs another argument.\n" );
				fprintf( stderr, 
					 "   Please specify a string to append to our log's filename.\n" );
				exit( 1 );
			}
			break;
		default:
			done = TRUE;
			break;	
		}
		if ( done == TRUE ) {
			break;		// break out of for loop
		}
	}

	// using "-t" also implicitly sets "-f"; i.e. only to stderr in the foreground
	if ( Termlog ) {
		Foreground = 1;
	}

		// call config so we can call param.  
	config(NULL, wantsQuiet);

		// If want to override which thing is our config file, we've
		// got to set that here, between where we config and where we
		// setup logging.  Note: we also have to do this in reconfig.
	if( logDir ) {
		set_log_dir();
	}

		// If we're told on the command-line to append something to
		// the name of our log file, we do that here, so that when we
		// setup logging, we get the right filename.  -Derek Wright
		// 11/20/98

	if( logAppend ) {
		char *tmp1, *tmp2;
		char buf[100];
		sprintf( buf, "%s_LOG", mySubSystem );
		if( !(tmp1 = param(buf)) ) { 
			EXCEPT( "%s not defined!", buf );
		}
		tmp2 = (char*)malloc( (strlen(tmp1) + strlen(logAppend) + 2) *
							  sizeof(char) );
		if( !tmp2 ) {
			EXCEPT( "Out of memory!" );
		}
		sprintf( tmp2, "%s.%s", tmp1, logAppend );
		config_insert( buf, tmp2 );
		free( tmp1 );
		free( tmp2 );
	}

		// See if we're supposed to be allowing core files or not
	check_core_files();

		// If we want to kill something, do that now.
	if( wantsKill ) {
		do_kill();
	}

		// Set up logging
	dprintf_config(mySubSystem,2);

	dprintf(D_ALWAYS,"******************************************************\n");
	dprintf(D_ALWAYS,"** %s (CONDOR_%s) STARTING UP\n",myName,mySubSystem);
	dprintf(D_ALWAYS,"** %s\n", CondorVersion());

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
#else	// UNIX
		// on unix, background means just fork ourselves
		if ( fork() ) {
			// parent
			exit(0);
		}
		// and close stdin, out, err if we are the MASTER.  
		if ( is_master ) {
			close(0); close(1); close(2);
		}
		// and detach from the controlling tty
		detach();
#endif	// of else of ifdef WIN32
	}	// if if !Foreground


		// Now that we've potentially forked, we have our real pid, so
		// we can instantiate a daemon core and it'll have the right
		// pid.  Have lots of pid table hash buckets if we're the
		// SCHEDD, since the SCHEDD could have lots of children... 
	if ( strcmp(mySubSystem,"SCHEDD") == 0 ) {
		daemonCore = new DaemonCore(500);
	} else {
		daemonCore = new DaemonCore();
	}

		// Now that we have the daemonCore object, we can finally
		// print out what our pid is.

	dprintf(D_ALWAYS,"** PID = %lu\n",daemonCore->getpid());
	dprintf(D_ALWAYS,"******************************************************\n");

		// Now that we have our pid, we could dump our pidfile, if we
		// want it. 
	if( pidFile ) {
		drop_pid_file();
	}

#ifndef WIN32
		// Now that logging is setup, create a pipe to deal with unix 
		// async signals.  We do this after logging is setup so that
		// we can EXCEPT (and really log something) on failure...
	if ( pipe(daemonCore->async_pipe) == -1 ||
		 fcntl(daemonCore->async_pipe[0],F_SETFL,O_NONBLOCK) == -1 ||
		 fcntl(daemonCore->async_pipe[1],F_SETFL,O_NONBLOCK) == -1 ) {
			EXCEPT("Failed to create async pipe");
	}
#endif

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
			rsock = new AuthSock;
			ssock = new SafeSock;
			if ( !rsock ) {
				EXCEPT("Unable to create command Relisock");
			}
			if ( !ssock ) {
				EXCEPT("Unable to create command SafeSock");
			}
			if ( command_port == -1 ) {
				// choose any old port (dynamic port)
				if (!BindAnyCommandPort(rsock, ssock)) {
					EXCEPT("BindAnyCommandPort failed");
				}
				if ( !rsock->listen() ) {
					EXCEPT("Failed to post listen on command AuthSock");
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

		// now register these new command sockets.
		// Note: In other parts of the code, we assume that the
		// first command socket registered is TCP, so we must
		// register the rsock socket first.
		daemonCore->Register_Command_Socket( (Stream*)rsock );
		daemonCore->Register_Command_Socket( (Stream*)ssock );		

		dprintf( D_ALWAYS,"DaemonCore: Command Socket at %s\n",
				 daemonCore->InfoCommandSinfulString());

			// Now, drop this sinful string into a file, if
			// mySubSystem_ADDRESS_FILE is defined.
		drop_addr_file();

		// now register any DaemonCore "default" handlers

#ifdef WANT_DC_PM
		// register the command handler to take care of signals
		daemonCore->Register_Command(DC_RAISESIGNAL,"DC_RAISESIGNAL",
				(CommandHandlercpp)daemonCore->HandleSigCommand,
				"HandleSigCommand()",daemonCore,IMMEDIATE_FAMILY);

		// this handler receives process exit info
		daemonCore->Register_Command(DC_PROCESSEXIT,"DC_PROCESSEXIT",
				(CommandHandlercpp)daemonCore->HandleProcessExitCommand,
				"HandleProcessExitCommand()",daemonCore,IMMEDIATE_FAMILY);
#endif
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
#ifndef WIN32
	daemonCore->Register_Signal( DC_SIGCHLD, "DC_SIGCHLD",
								 (SignalHandlercpp)&DaemonCore::HandleDC_SIGCHLD,
								 "HandleDC_SIGCHLD",daemonCore,IMMEDIATE_FAMILY);
#endif

		// Install handler for the CONFIG_VAL 
	daemonCore->Register_Command( CONFIG_VAL, "CONFIG_VAL",
								  (CommandHandler)handle_config_val,
								  "handle_config_val()", 0, ADMINISTRATOR );

	// call daemon-core ReInit
	daemonCore->ReInit();

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
	// or a "-v", then we want to register as an NT Service.
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
		  case 'v':		// version info
			printf( "%s\n", CondorVersion() );
			exit(0);
			break;
		  case 'l':		// specify log directory
			 ptr++;
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

