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

 

#include "condor_common.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "master.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_collector.h"
#include "cctp_msg.h"
#include "condor_attributes.h"
#include "condor_network.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "exit.h"
#include "string_list.h"
#include "get_daemon_addr.h"

#ifndef WANT_DC_PM
int standard_sigchld(Service *,int);
int all_reaper_sigchld(Service *,int);
#endif

#ifdef WIN32
extern void register_service();
extern void terminate(DWORD);
#endif

typedef void (*SIGNAL_HANDLER)();

// prototypes of library functions
extern "C"
{
	void install_sig_handler( int, SIGNAL_HANDLER );
}


// local function prototypes
void	init_params();
void	init_daemon_list();
void	init_classad();
void	get_lock(char * );
time_t 	GetTimeStamp(char* file);
int 	NewExecutable(char* file, time_t* tsp);
void	RestartMaster();
int		run_preen(Service*);
void	usage(const char* );
int		main_shutdown_graceful();
int		main_shutdown_fast();
int		main_config();
int		admin_command_handler(Service *, int, Stream *);
extern "C" int	DoCleanup();

#if 0
int		GetConfig(char*, char*);
void	StartConfigServer();
char*			configServer;
#endif

// Global variables
ClassAd	*ad = NULL;				// ClassAd to send to collector
int		MasterLockFD;
int		update_interval;
int		check_new_exec_interval;
int		preen_interval;
int		new_bin_delay;
char	*MasterName = NULL;
char	*CollectorHost = NULL;

int		ceiling = 3600;
float	e_factor = 2.0;								// exponential factor
int		r_factor = 300;								// recover factor
char*	config_location;						// config file from server
int		doConfigFromServer = FALSE; 
char	*FS_Preen = NULL;
int		NT_ServiceFlag = FALSE;		// TRUE if running on NT as an NT Service

int		shutdown_graceful_timeout;
int		shutdown_fast_timeout;

int		PublishObituaries;
int		Lines;
int		AllowAdminCommands = FALSE;
int		StartDaemons = TRUE;

char	*default_daemon_list[] = {
	"MASTER",
	"STARTD",
	"SCHEDD",
#if defined(OSF1) || defined(IRIX53)    // Only need KBDD on alpha and sgi
	"KBDD",
#endif
	0};

// create an object of class daemons.
class Daemons daemons;

// for daemonCore
char *mySubSystem = "MASTER";

int
master_exit(int retval)
{
#ifdef WIN32
	if ( NT_ServiceFlag == TRUE ) {
		terminate(retval);
	}
#endif

	DC_Exit(retval);
	return 1;	// just to satisfy vc++
}

void
usage( const char* name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n name]\n", name );
	master_exit( 1 );
}

int
DoCleanup()
{
	static int already_excepted = FALSE;

	// This function gets called as the last thing by EXCEPT().
	// At this point, the MASTER presumably has had an unrecoverable error
	// and is about to die.  We'll attempt to gracefully shutdown our
	// kiddies before we fade away.  Use a static flag to prevent
	// infinite recursion in the case that there is another EXCEPT.

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
		main_shutdown_graceful();  // this will exit if successful,
								   // but will return before all
								   // daemons have exited... what can
								   // we do?!
	}

	if ( NT_ServiceFlag == FALSE ) {
		return 1;	// this will return to the EXCEPT code, which will just exit
	} else {
		master_exit(1); // this does not return
		return 1;		// just to satisfy VC++
	}
}


int
main_init( int argc, char* argv[] )
{
	char	**ptr;

#ifdef WIN32
	// Daemon Core will call main_init with argc -1 if need to register
	// as a WinNT Service.
	if ( argc == -1 ) {
		NT_ServiceFlag = TRUE;
		register_service();
		return TRUE;
	}
#endif

	if ( argc > 3 ) {
		usage( argv[0] );
	}
	
	int argc_count = 1;
	for( ptr=argv+1, argc_count = 1; argc_count<argc && *ptr; ptr++,argc_count++) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
		case 'n':
			ptr++;
			if( ! *ptr ) {
				EXCEPT( "-n requires another arugment" );
			}
			MasterName = strdup( build_valid_daemon_name(*ptr) );
			dprintf( D_ALWAYS, "Using name: %s\n", MasterName );
			break;
		default:
			usage( argv[0] );
		}
	}

	daemons.SetDefaultReaper();
	
		// Grab all parameters needed by the master.
	init_params();
		// param() for DAEMON_LIST and initialize our daemons object.
	init_daemon_list();
		// Lookup the paths to all the daemons we now care about.
	daemons.InitParams();
		// Initialize our classad;
	init_classad();  
		// Initialize the master entry in the daemons data structure.
	daemons.InitMaster();

		// Register admin commands
	daemonCore->Register_Command( RECONFIG, "RECONFIG",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( RESTART, "RESTART",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_OFF, "DAEMONS_OFF",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_OFF_FAST, "DAEMONS_OFF_FAST",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_ON, "DAEMONS_ON",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( MASTER_OFF, "MASTER_OFF",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( MASTER_OFF_FAST, "MASTER_OFF_FAST",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );

	_EXCEPT_Cleanup = DoCleanup;

#if !defined(WIN32)
	if( !Termlog ) {
			// If we're not connected to a terminal, start our own
			// process group.
		setsid();
	}
#endif

	/* Make sure we are the only copy of condor_master running */
#ifndef WIN32
	char*	log_file = daemons.DaemonLog( daemonCore->getpid() );
	get_lock( log_file);  
#endif

	if( StartDaemons ) {
		daemons.StartAllDaemons();
	}
	daemons.StartTimers();
	return TRUE;
}


int
admin_command_handler(Service *, int cmd, Stream *)
{
	if(! AllowAdminCommands ) {
		dprintf( D_FULLDEBUG, 
				 "Got admin command (%d) while not allowed. Ignoring.\n",
				 cmd );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, 
			 "Got admin command (%d) and allowing it.\n", cmd );
	switch( cmd ) {
	case RECONFIG:
		daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGHUP );
		return TRUE;
	case RESTART:
		daemons.immediate_restart = TRUE;
		daemons.RestartMaster();
		return TRUE;
	case DAEMONS_ON:
		daemons.DaemonsOn();
		return TRUE;
	case DAEMONS_OFF:
		daemons.DaemonsOff();
		return TRUE;
	case DAEMONS_OFF_FAST:
		daemons.DaemonsOff( 1 );
		return TRUE;
	case MASTER_OFF:
		main_shutdown_graceful();
		return TRUE;
	case MASTER_OFF_FAST:
		main_shutdown_fast();
		return TRUE;
	default: 
		EXCEPT( "Unknown admin command in handle_admin_commands" );
	}
	return FALSE;
}


void
init_params()
{
	char	*tmp;
	static	int	master_name_in_config = 0;

	if( ! master_name_in_config ) {
			// First time, or we know it's not in the config file. 
		if( ! MasterName ) {
				// Not set on command line
			tmp = param( "MASTER_NAME" );
			if( tmp ) {
				MasterName = strdup( build_valid_daemon_name(tmp) );
				master_name_in_config = 1;
				free( tmp );
			} 
		}
	} else {
		free( MasterName );
		tmp = param( "MASTER_NAME" );
		MasterName = strdup( build_valid_daemon_name(tmp) );
		free( tmp );
	}
	if( MasterName ) {
		dprintf( D_FULLDEBUG, "Using name: %s\n", MasterName );
	}
			
		// In general, for numeric values, we set our variable to 0,
		// param for it, and if there's an entry, we call atoi() on
		// the string to convert it to an int.  If the string doesn't
		// contain a valid number, atoi() will return 0.  So, if our
		// variable is still 0 after everything is done, either there
		// was no entry, or the entry was an invalid number.  In both
		// cases, we'll want to use our default.  -Derek

	tmp = param( "START_MASTER" );
	if( tmp ) {
		if( *tmp == 'F' || *tmp == 'f' ) {
			dprintf( D_ALWAYS, "START_MASTER was set to %s, shutting down.\n", tmp );
			StartDaemons = FALSE;
			main_shutdown_graceful();
		}
		free( tmp );
	}

	if( CollectorHost ) {
		free( CollectorHost );
	}
	if( (CollectorHost = param("COLLECTOR_HOST")) == NULL ) {
		EXCEPT( "COLLECTOR_HOST not specified in config file" );
	}
	
	StartDaemons = TRUE;
	tmp = param("START_DAEMONS");
	if( tmp ) {
		if( *tmp == 'f' || *tmp == 'F' ) {
			dprintf( D_ALWAYS, 
					 "START_DAEMONS flag was set to %s.  Not starting daemons.\n",
					 tmp );
			StartDaemons = FALSE;
		}
		free( tmp );
	} 

	PublishObituaries = TRUE;
	tmp = param("PUBLISH_OBITUARIES");
	if( tmp ) {
		if( (*tmp == 'f' || *tmp == 'F') ) {
			PublishObituaries = FALSE;
		}
		free( tmp );
	}

	Lines = 0;
	tmp = param("OBITUARY_LOG_LENGTH");
	if( tmp ) {
		Lines = atoi( tmp );
		free( tmp );
	} 
	if( !Lines ) {
		Lines = 20;
	}

	ceiling = 0;
	tmp = param( "MASTER_BACKOFF_CEILING" );
	if( tmp ) {
		ceiling = atoi( tmp );
		free( tmp );
	} 
	if( !ceiling ) {
		ceiling = 3600;
	}

	e_factor = 0;
	tmp = param( "MASTER_BACKOFF_FACTOR" );
    if( tmp ) {
        e_factor = atof( tmp );
		free( tmp );
    } 
	if( !e_factor ) {
    	e_factor = 2.0;
    }
	
	r_factor = 0;
	tmp = param( "MASTER_RECOVER_FACTOR" );
    if( tmp ) {
        r_factor = atoi( tmp );
		free( tmp );
    } 
	if( !r_factor ) {
    	r_factor = 300;
    }
	
	update_interval = 0;
	tmp = param( "MASTER_UPDATE_INTERVAL" );
    if( tmp ) {
        update_interval = atoi( tmp );
		free( tmp );
    } 
	if( !update_interval ) {
        update_interval = 5 * MINUTE;
    }

	check_new_exec_interval = 0;
	tmp = param( "MASTER_CHECK_NEW_EXEC_INTERVAL" );
    if( tmp ) {
        check_new_exec_interval = atoi( tmp );
		free( tmp );
    }
	if( !check_new_exec_interval ) {
        check_new_exec_interval = 5 * MINUTE;
    }

	new_bin_delay = 0;
	tmp = param( "MASTER_NEW_BINARY_DELAY" );
	if( tmp ) {
		new_bin_delay = atoi( tmp );
		free( tmp );
	}
	if( !new_bin_delay ) {
		new_bin_delay = 2 * MINUTE;
	}

	preen_interval = 0;
	tmp = param( "PREEN_INTERVAL" );
	if( tmp ) {
		preen_interval = atoi( tmp );
		free( tmp );
	} 
	if( !preen_interval ) {
		preen_interval = 24 * HOUR;
	}

	shutdown_fast_timeout = 0;
	tmp = param( "SHUTDOWN_FAST_TIMEOUT" );
	if( tmp ) {
		shutdown_fast_timeout = atoi( tmp );
		free( tmp );
	}
	if( !shutdown_fast_timeout ) {
		shutdown_fast_timeout = 5 * MINUTE;
	}

	shutdown_graceful_timeout = 0;
	tmp = param( "SHUTDOWN_GRACEFUL_TIMEOUT" );
	if( tmp ) {
		shutdown_graceful_timeout = atoi( tmp );
		free( tmp );
	}
	if( !shutdown_graceful_timeout ) {
		shutdown_graceful_timeout = 30 * MINUTE;
	}

	AllowAdminCommands = TRUE;
	tmp = param( "ALLOW_ADMIN_COMMANDS" );
	if( tmp ) {
		if( *tmp == 'F' || *tmp == 'f' ) {
			AllowAdminCommands = FALSE;
		}
		free( tmp );	
	}

	if( FS_Preen ) {
		free( FS_Preen );
	}
	FS_Preen = param( "PREEN" );
}


void
init_daemon_list()
{
	char	*daemon_name;
	daemon	*new_daemon;
	StringList daemon_names;

	char* daemon_list = param("DAEMON_LIST");
	if( daemon_list ) {
		daemon_names.initializeFromString(daemon_list);
		free( daemon_list );

		daemon_names.rewind();
		while( (daemon_name = daemon_names.next()) ) {
			if(daemons.GetIndex(daemon_name) < 0) {
				new_daemon = new daemon(daemon_name);
			}
		}
	} else {
		for(int i = 0; default_daemon_list[i]; i++) {
			new_daemon = new daemon(default_daemon_list[i]);
		}
	}
}


void
check_daemon_list()
{
	char	*daemon_name;
	daemon	*new_daemon;
	StringList daemon_names;
	char* daemon_list = param("DAEMON_LIST");
	if( !daemon_list ) {
			// Without a daemon list, there's no way it could be
			// different than what we've got now.
		return;
	}
	daemon_names.initializeFromString(daemon_list);
	free( daemon_list );

	daemon_names.rewind();
	while( (daemon_name = daemon_names.next()) ) {
		if(daemons.GetIndex(daemon_name) < 0) {
			new_daemon = new daemon(daemon_name);
		}
	}
}


void
init_classad()
{
	if( ad ) delete( ad );
	ad = new ClassAd();

	ad->SetMyTypeName(MASTER_ADTYPE);
	ad->SetTargetTypeName("");

	char line[100];
	sprintf(line, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() );
	ad->Insert(line);

	if( MasterName ) {
		sprintf(line, "%s = \"%s\"", ATTR_NAME, MasterName );
	} else {
		sprintf(line, "%s = \"%s\"", ATTR_NAME, my_full_hostname() );
	}
	ad->Insert(line);

	sprintf(line, "%s = \"%s\"", ATTR_MASTER_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );
	ad->Insert(line);

#if !defined(WIN32)
	sprintf(line, "%s = %d", ATTR_REAL_UID, (int)getuid() );
	ad->Insert(line);
#endif

		// In case MASTER_EXPRS is set, fill in our ClassAd with those
		// expressions. 
	config_fill_ad( ad ); 	
}

FileLock *MasterLock;

void
get_lock( char* file_name )
{
	if( (MasterLockFD=open(file_name,O_RDWR,0)) < 0 ) {
		EXCEPT( "open(%s,0,0)", file_name );
	}

	// This must be a global so that it doesn't go out of scope
	// cause the destructor releases the lock.
	MasterLock = new FileLock( MasterLockFD );
	MasterLock->set_blocking( FALSE );
	if( !MasterLock->obtain(WRITE_LOCK) ) {
		EXCEPT( "Can't get lock on \"%s\"", file_name );
	}
}


/*
 ** Re read the config file, and send all the daemons a signal telling
 ** them to do so also.
 */
int
main_config()
{
		// Re-read the config files and create a new classad
	init_classad(); 

		// Reset our config values
	init_params();

		// Re-read the paths to our executables.  If any paths
		// changed, the daemons will be marked as having a new
		// executable.
	daemons.InitParams();

	if( StartDaemons ) {
			// Restart any daemons who's executables are new or ones 
			// that the path to the executable has changed.  
		daemons.immediate_restart = TRUE;
		daemons.CheckForNewExecutable();
		daemons.immediate_restart = FALSE;
			// Tell all the daemons that are running to reconfig.
		daemons.ReconfigAllDaemons();
	} else {
		daemons.DaemonsOff();
	}
		// Re-register our timers, since their intervals might have
		// changed.
	daemons.StartTimers();
	daemons.UpdateCollector();
	return TRUE;
}

/*
 ** Kill all daemons and go away.
 */
int
main_shutdown_fast()
{
	daemons.SetAllGoneAction( MASTER_EXIT );

	if( daemons.NumberOfChildren() == 0 ) {
		daemons.AllDaemonsGone();
	}

	daemons.StopFastAllDaemons();
	return TRUE;
}


/*
 ** Cause job(s) to vacate, kill all daemons and go away.
 */
int
main_shutdown_graceful()
{
	daemons.SetAllGoneAction( MASTER_EXIT );

	if( daemons.NumberOfChildren() == 0 ) {
		daemons.AllDaemonsGone();
	}

	daemons.StopAllDaemons();
	return TRUE;
}


#if !defined(WANT_DC_PM)
int
all_reaper_sigchld( Service*, int )
{
	int		pid = 0;
	int	status;

	while( (pid=waitpid(-1,&status,WNOHANG)) > 0 ) {
		if( WIFSTOPPED(status) ) {
			continue;
		}	
		daemons.AllReaper( pid, status );
	}
	return TRUE;
}	


int
standard_sigchld( Service *, int )
{
	int		pid = 0;
	int	status;

	while( (pid=waitpid(-1,&status,WNOHANG)) > 0 ) {
		if( WIFSTOPPED(status) ) {
			continue;
		}
		daemons.DefaultReaper( pid, status );
	}
	return TRUE;
}

#endif  // of !defined(WANT_DC_PM)

time_t
GetTimeStamp(char* file)
{
	struct stat sbuf;

	if( stat(file, &sbuf) < 0 ) {
		return( (time_t) -1 );
	}

	return( sbuf.st_mtime );
}

int
NewExecutable(char* file, time_t *tsp)
{
	time_t cts = *tsp;
	*tsp = GetTimeStamp(file);
	dprintf(D_FULLDEBUG, "Time stamp of running %s: %ld\n",
			file, cts);
	dprintf(D_FULLDEBUG, "GetTimeStamp returned: %ld\n",*tsp);

	if( *tsp == (time_t) -1 ) {
		/*
		 **	We could have been in the process of installing a new
		 **	version, and that's why the 'stat' failed.  Catch it
		 **  next time around.
		 */
		*tsp = cts;
		return( FALSE );
	}
	return( cts != *tsp );
}

char	*Shell = "/bin/sh";

int
run_preen(Service*)
{
	int		child_pid;

	dprintf(D_FULLDEBUG, "Entered run_preen.\n");

	if( FS_Preen == NULL ) {
		return 0;
	}

	/* Exec Preen to check log, spool, and execute for any junk files left lying around */
#ifdef WANT_DC_PM
	child_pid = daemonCore->Create_Process(
					FS_Preen,		// program to exec
					FS_Preen,			// args
					PRIV_CONDOR,	// privledge level
					1,				// which reaper ID to use; use default reaper
					FALSE );		// we do _not_ want this process to have a command port; PREEN is not a daemon core process
	dprintf( D_ALWAYS, "Preen pid is %d\n", child_pid );
#else
	dprintf( D_ALWAYS,
			"Calling execl( \"%s\", \"sh\", \"-c\", \"%s\", 0 )\n", Shell, FS_Preen );

	if( (child_pid=vfork()) == 0 ) {	/* The child */
		execl( Shell, "sh", "-c", FS_Preen, 0 );
		_exit( 127 );
	} else {				/* The parent */
		dprintf( D_ALWAYS, "Shell (preen) pid is %d\n", child_pid );
		return 0;
	}
#endif
	/*
	   Note: can't use system() here.  That calls wait(), but the child's
	   status will be captured our own sigchld_handler().  This would
	   cause the wait() called by system() to hang forever...
	   -- mike

	   (void)system( FS_Preen );
	   */
	return TRUE;
}


void
RestartMaster()
{
	daemons.RestartMaster();
}


#if 0
void StartConfigServer()
{
	daemon*			newDaemon;
	
	newDaemon = new daemon("CONFIG_SERVER");
	newDaemon->process_name = param(newDaemon->name_in_config_file);
	if(newDaemon->process_name == NULL && newDaemon->runs_here)
	{
		dprintf(D_ALWAYS, "Process not found in config file: %s\n",
				newDaemon->name_in_config_file);
		EXCEPT("Can't continue...");
	}
	newDaemon->config_info_file = param("CONFIG_SERVER_FILE");
	newDaemon->port = param("CONFIG_SERVER_PORT");

	// check that log file is necessary
	if(newDaemon->log_filename_in_config_file != NULL)
	{
		newDaemon->log_name = param(newDaemon->log_filename_in_config_file);
		if(newDaemon->log_name == NULL && newDaemon->runs_here)
		{
			dprintf(D_ALWAYS, "Log file not found in config file: %s\n",
					newDaemon->log_filename_in_config_file);
		}
	}

	newDaemon->StartDaemon();
	
	// sleep for a while because we want the config server to stable down
	// before doing anything else
	sleep(5); 
}
#endif
