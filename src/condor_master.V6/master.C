/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "condor_string.h"
#include "basename.h"
#include "master.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_collector.h"
#include "condor_attributes.h"
#include "condor_network.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "exit.h"
#include "string_list.h"
#include "get_daemon_name.h"
#include "daemon_types.h"
#include "daemon_list.h"
#include "strupr.h"
#include "condor_environ.h"

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
void	lock_or_except(const char * );
time_t 	GetTimeStamp(char* file);
int 	NewExecutable(char* file, time_t* tsp);
void	RestartMaster();
int		run_preen(Service*);
void	usage(const char* );
int		main_shutdown_graceful();
int		main_shutdown_fast();
int		main_config( bool is_full );
int		agent_starter(ReliSock *);
int		handle_agent_fetch_log(ReliSock *);
int		admin_command_handler(Service *, int, Stream *);
int		handle_subsys_command(int, Stream *);

extern "C" int	DoCleanup(int,int,char*);

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
CollectorList *Collectors = NULL;
DaemonList* secondary_collectors = NULL;

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
int		GotDaemonsOff = FALSE;

char	*default_daemon_list[] = {
	"MASTER",
	"STARTD",
	"SCHEDD",
#if defined(OSF1) || defined(IRIX)    // Only need KBDD on alpha and sgi
	"KBDD",
#endif
	0};

char	default_dc_daemon_list[] =
"MASTER, STARTD, SCHEDD, KBDD, COLLECTOR, NEGOTIATOR, EVENTD, "
"VIEW_SERVER, CONDOR_VIEW, VIEW_COLLECTOR, HAWKEYE";

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
DoCleanup(int,int,char*)
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
    extern int runfor;
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
			if( !(ptr && *ptr) ) {
				EXCEPT( "-n requires another arugment" );
			}
			MasterName = build_valid_daemon_name( *ptr );
			dprintf( D_ALWAYS, "Using name: %s\n", MasterName );
			break;
		default:
			usage( argv[0] );
		}
	}

    if (runfor != 0) {
        // We will construct an environment variable that 
        // tells the daemon what time it will be shut down. 
        // We'll give it an absolute time, though runfor is a 
        // relative time. This means that we don't have to update
        // the time each time we restart the daemon.
        time_t      death_time;
        MyString    runfor_env_source;
        const char  *env_name;
        char        *runfor_env;
        
        env_name   = EnvGetName(ENV_DAEMON_DEATHTIME);
        death_time = time(NULL) + (runfor * 60);

        runfor_env_source.sprintf("%s=%d\n", env_name, death_time);
        runfor_env = strdup(runfor_env_source.Value());
        putenv(runfor_env);
        // Note that we don't free runfor_env, because it lives on 
        // forever in the environment. 
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
	daemonCore->Register_Command( DAEMONS_OFF_PEACEFUL, "DAEMONS_OFF_PEACEFUL",
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
	daemonCore->Register_Command( DAEMON_ON, "DAEMON_ON",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_OFF, "DAEMON_OFF",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_OFF_FAST, "DAEMON_OFF_FAST",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_OFF_PEACEFUL, "DAEMON_OFF_PEACEFUL",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler", 0, ADMINISTRATOR );

	/*
	daemonCore->Register_Command( START_AGENT, "START_AGENT",
					  (CommandHandler)admin_command_handler, 
					  "admin_command_handler", 0, ADMINISTRATOR );
	*/

	_EXCEPT_Cleanup = DoCleanup;

#if !defined(WIN32)
	if( !Termlog ) {
			// If we're not connected to a terminal, start our own
			// process group.
		setsid();
	}
#endif

	if( StartDaemons ) {
		daemons.StartAllDaemons();
	}
	daemons.StartTimers();
	return TRUE;
}


int
admin_command_handler( Service*, int cmd, Stream* stream )
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
		daemonCore->Send_Signal( daemonCore->getpid(), SIGHUP );
		return TRUE;
	case RESTART:
		daemons.immediate_restart = TRUE;
		daemons.RestartMaster();
		return TRUE;
	case RESTART_PEACEFUL:
		daemons.immediate_restart = TRUE;
		daemons.RestartMasterPeaceful();
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
	case DAEMONS_OFF_PEACEFUL:
		daemons.DaemonsOff();
		return TRUE;
	case MASTER_OFF:
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
		return TRUE;
	case MASTER_OFF_FAST:
		daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
		return TRUE;

			// These commands are special, since they all need to read
			// off the subsystem before they know what to do.  So, we
			// handle them with a special function.
	case DAEMON_ON:
	case DAEMON_OFF:
	case DAEMON_OFF_FAST:
	case DAEMON_OFF_PEACEFUL:
		return handle_subsys_command( cmd, stream );

			// This function is also special, since it needs to read
			// off more info.  So, it is handled with a special function.
	case START_AGENT:
		if (daemonCore->Create_Thread(
				(ThreadStartFunc)&agent_starter, (void*)stream, 0 )) {
			return TRUE;
		} else {
			dprintf( D_ALWAYS, "ERROR: unable to create agent thread!\n");
			return FALSE;
		}
	default: 
		EXCEPT( "Unknown admin command (%d) in handle_admin_commands",
				cmd );
	}
	return FALSE;
}

int
agent_starter( ReliSock * s )
{
	ReliSock* stream = (ReliSock*)s;
	char *subsys = NULL;

	stream->decode();
	if( ! stream->code(subsys) ||
		! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read subsystem name\n" );
		free( subsys );
		return FALSE;
	}

	dprintf ( D_ALWAYS, "Starting agent '%s'\n", subsys );

	if( stricmp(subsys, "fetch_log") == 0 ) {
		free (subsys);
		return handle_agent_fetch_log( stream );
	}

	// default:

	free (subsys);
	dprintf( D_ALWAYS, "WARNING: unrecognized agent name\n" );
	return FALSE;
}

int
handle_agent_fetch_log (ReliSock* stream) {

	char *daemon_name = NULL;
	char *daemon_paramname = NULL;
	char *daemon_filename = NULL;
	int  res = FALSE;

	if( ! stream->code(daemon_name) ||
		! stream->end_of_message()) {
		dprintf( D_ALWAYS, "ERROR: fetch_log can't read daemon name\n" );
		free( daemon_name );
		return FALSE;
	}

	dprintf( D_ALWAYS, "INFO: daemon_name: %s\n", daemon_name );

	daemon_paramname = (char*)malloc (strlen(daemon_name) + 5);
	strcpy (daemon_paramname, daemon_name);
	strcat (daemon_paramname, "_LOG");

	dprintf( D_ALWAYS, "INFO: daemon_paramname: %s\n", daemon_paramname );

	if( (daemon_filename = param(daemon_paramname)) ) {
		filesize_t	size;
		dprintf( D_ALWAYS, "INFO: daemon_filename: %s\n", daemon_filename );
		stream->encode();
		res = (stream->put_file(&size, daemon_filename) < 0);
		free (daemon_filename);
	} else {
		dprintf( D_ALWAYS, "ERROR: fetch_log can't param for log name\n" );
	}

	free (daemon_paramname);
	free (daemon_name);

	return res;
}

int
handle_subsys_command( int cmd, Stream* stream )
{
	char* subsys = NULL;
	class daemon* daemon;
	daemon_t dt;

	stream->decode();
	if( ! stream->code(subsys) ) {
		dprintf( D_ALWAYS, "Can't read subsystem name\n" );
		free( subsys );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( subsys );
		return FALSE;
	}
	if( !(dt = stringToDaemonType(subsys)) ) {
		dprintf( D_ALWAYS, "Error: got unknown subsystem string \"%s\"\n", 
				 subsys );
		free( subsys );
		return FALSE;
	}
	if( !(daemon = daemons.FindDaemon(dt)) ) {
		dprintf( D_ALWAYS, "Error: Can't find daemon of type \"%s\"\n", 
				 subsys );
		free( subsys );
		return FALSE;
	}
	dprintf( D_COMMAND, "Handling daemon-specific command for \"%s\"\n", 
			 subsys );
	free( subsys );
	switch( cmd ) {
	case DAEMON_ON:
		daemon->on_hold = FALSE;
		return daemon->Start();
	case DAEMON_OFF:
		daemon->on_hold = TRUE;
		daemon->Stop();
		return TRUE;
	case DAEMON_OFF_FAST:
		daemon->on_hold = TRUE;
		daemon->StopFast();
		return TRUE;
	case DAEMON_OFF_PEACEFUL:
		daemon->on_hold = TRUE;
		daemon->StopPeaceful();
		return TRUE;
	default:
		EXCEPT( "Unknown command (%d) in handle_subsys_command", cmd );
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
				MasterName = build_valid_daemon_name( tmp );
				master_name_in_config = 1;
				free( tmp );
			} 
		}
	} else {
		delete [] MasterName;
		tmp = param( "MASTER_NAME" );
		MasterName = build_valid_daemon_name( tmp );
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

	if( Collectors ) {
		delete( Collectors );
	}
	Collectors = CollectorList::create();
		
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
		// If we were sent the daemons_off command, don't forget that
		// here. 
	if( GotDaemonsOff ) {
		StartDaemons = FALSE;
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
		if( tmp[0] != '0' && check_new_exec_interval == 0 ) { 
				// They put something there other than "0", but atoi()
				// got confused.  Warn them and set it.
			dprintf( D_ALWAYS, "Warning, can't parse value in "
					 "%s: \"%s\", using default of 5 minutes\n",
					 "MASTER_CHECK_NEW_EXEC_INTERVAL", tmp );
			check_new_exec_interval = 5 * MINUTE;
		}
		free( tmp );
    } else {
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

	tmp = param( "SECONDARY_COLLECTOR_LIST" );
	if( tmp ) {
		if( secondary_collectors ) {
			delete secondary_collectors;
		}
		secondary_collectors = new DaemonList();
		secondary_collectors->init( DT_COLLECTOR, tmp );
		free(tmp);
	} else {
		if( secondary_collectors ) {
			delete secondary_collectors;
			secondary_collectors = NULL;
		}
	}
}


void
init_daemon_list()
{
	char	*daemon_name;
	class daemon	*new_daemon;
	StringList daemon_names, dc_daemon_names;

	char* dc_daemon_list = param("DC_DAEMON_LIST");
	if( dc_daemon_list ) {
		dc_daemon_names.initializeFromString(dc_daemon_list);
	} else {
		dc_daemon_names.initializeFromString(default_dc_daemon_list);
	}

	char* ha_list = param("MASTER_HA_LIST");
	if( ha_list ) {
			// Make MASTER_HA_LIST case insensitive by always converting
			// what we get to uppercase.
		StringList	ha_names;
		ha_list = strupr( ha_list );
		ha_names.initializeFromString(ha_list);
		free( ha_list );

		ha_names.rewind();
		while( (daemon_name = ha_names.next()) ) {
			if(daemons.GetIndex(daemon_name) < 0) {
				if( dc_daemon_names.contains(daemon_name) ) {
					new_daemon = new class daemon(daemon_name, true, true );
				} else {
					new_daemon = new class daemon(daemon_name, false, true );
				}
			}
		}
	}

	char* daemon_list = param("DAEMON_LIST");
	if( daemon_list ) {
			// Make DAEMON_LIST case insensitive by always converting
			// what we get to uppercase.
		daemon_list = strupr( daemon_list );
		daemon_names.initializeFromString(daemon_list);
		free( daemon_list );

		daemon_names.rewind();
		while( (daemon_name = daemon_names.next()) ) {
			if(daemons.GetIndex(daemon_name) < 0) {
				if( dc_daemon_names.contains(daemon_name) ) {
					new_daemon = new class daemon(daemon_name);
				} else {
					new_daemon = new class daemon(daemon_name, false);
				}
			}
		}
	} else {
		for(int i = 0; default_daemon_list[i]; i++) {
			new_daemon = new class daemon(default_daemon_list[i]);
		}
	}
}


void
check_daemon_list()
{
	char	*daemon_name;
	class daemon	*new_daemon;
	StringList daemon_names;
	char* daemon_list = param("DAEMON_LIST");
	if( !daemon_list ) {
			// Without a daemon list, there's no way it could be
			// different than what we've got now.
		return;
	}

		// Make DAEMON_LIST case insensitive by always converting what
		// we get to uppercase.
	daemon_list = strupr( daemon_list );

	daemon_names.initializeFromString(daemon_list);
	free( daemon_list );

	daemon_names.rewind();
	while( (daemon_name = daemon_names.next()) ) {
		if(daemons.GetIndex(daemon_name) < 0) {
			new_daemon = new class daemon(daemon_name);
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

	char* defaultName = NULL;
	if( MasterName ) {
		sprintf(line, "%s = \"%s\"", ATTR_NAME, MasterName );
	} else {
		defaultName = default_daemon_name();
		if( ! defaultName ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
		sprintf(line, "%s = \"%s\"", ATTR_NAME, defaultName );
		delete [] defaultName;
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

#ifndef WIN32
FileLock *MasterLock;
#endif

void
lock_or_except( const char* file_name )
{
#ifndef WIN32	// S_IRUSR and S_IWUSR don't exist on WIN32, and 
				// we don't need to worry about multiple masters anyway
				// because it's a service.
	MasterLockFD=_condor_open_lock_file(file_name,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
	if( MasterLockFD < 0 ) {
		EXCEPT( "can't open(%s,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR) - errno %i", 
			file_name, errno );
	}

	// This must be a global so that it doesn't go out of scope
	// cause the destructor releases the lock.
	MasterLock = new FileLock( MasterLockFD );
	MasterLock->set_blocking( FALSE );
	if( !MasterLock->obtain(WRITE_LOCK) ) {
		EXCEPT( "Can't get lock on \"%s\"", file_name );
	}
#endif
}


/*
 ** Re read the config file, and send all the daemons a signal telling
 ** them to do so also.
 */
int
main_config( bool is_full )
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
    // Invalide session if necessary
    daemonCore->invalidateSessionCache();
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

	daemons.CancelRestartTimers();
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

	daemons.CancelRestartTimers();
	daemons.StopAllDaemons();
	return TRUE;
}


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

int
run_preen(Service*)
{
	int		child_pid, size;
	char	*preen_args, *tmp, *preen_base;

	dprintf(D_FULLDEBUG, "Entered run_preen.\n");

	if( FS_Preen == NULL ) {
		return 0;
	}
	preen_base = basename( FS_Preen );
	if( (tmp = param("PREEN_ARGS")) ) {
		size = strlen(tmp) + strlen(preen_base) + 2;
		preen_args = new char[size];
		sprintf( preen_args, "%s %s", preen_base, tmp );
		free( tmp );
	} else {
		preen_args = strnewp( preen_base );
	}
	child_pid = daemonCore->Create_Process(
					FS_Preen,		// program to exec
					preen_args,		// args
					PRIV_ROOT,		// privledge level
					1,				// which reaper ID to use; use default reaper
					FALSE );		// we do _not_ want this process to have a command port; PREEN is not a daemon core process
	dprintf( D_ALWAYS, "Preen pid is %d\n", child_pid );
	delete [] preen_args;
	return child_pid;
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


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init()
{
	/* Make sure we are the only copy of condor_master running */
#ifndef WIN32
	MyString lock_file;
	char*  p;

	// see if a file is given explicitly
	p = param ("MASTER_INSTANCE_LOCK");
	if (p) {
		lock_file = p;
		free (p);
	} else {
		// no filename given.  use $(LOCK)/InstanceLock.
		p = param ("LOCK");
		if (p) {
			lock_file = p;
			lock_file = lock_file + "/InstanceLock";
			free (p);
		} else {
			// no LOCK dir?  strange.  fall back to the
			// old behavior which is to lock the log file
			// itself.
			p = param("MASTER_LOG");
			if (p) {
				lock_file = p;
				free (p);
			} else {
				// i give up.  have a hardcoded default and like it. :)
				lock_file = "/tmp/InstanceLock";
			}
		}
	}
	dprintf (D_FULLDEBUG, "Attempting to lock %s.\n", lock_file.Value() );
	lock_or_except( lock_file.Value() );
	dprintf (D_FULLDEBUG, "Obtained lock on %s.\n", lock_file.Value() );
#endif

}

