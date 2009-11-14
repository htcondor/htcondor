/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "master.h"
#include "condor_daemon_core.h"
#include "exit.h"
#include "my_hostname.h"
#include "basename.h"
#include "condor_email.h"
#include "condor_environ.h"
#include "condor_parameters.h"
#include "daemon_list.h"
#include "sig_name.h"
#include "internet.h"
#include "strupr.h"
#include "condor_netdb.h"
#include "file_sql.h"
#include "file_lock.h"
#include "stat_info.h"
#include "shared_port_endpoint.h"

#if HAVE_DLOPEN
#include "MasterPlugin.h"
#endif

// these are defined in master.C
extern int 		MasterLockFD;
extern FileLock*	MasterLock;
extern int		master_exit(int);
extern int		update_interval;
extern int		check_new_exec_interval;
extern int		preen_interval;
extern int		new_bin_delay;
extern char*	FS_Preen;
extern			ClassAd* ad;
extern int		NT_ServiceFlag; // TRUE if running on NT as an NT Service

extern time_t	GetTimeStamp(char* file);
extern int 	   	NewExecutable(char* file, time_t* tsp);
extern void		tail_log( FILE*, char*, int );
extern void		run_preen();

extern FILESQL *FILEObj;
extern int condor_main_argc;
extern char **condor_main_argv;
extern time_t daemon_stop_time;

int		hourly_housekeeping(void);

// to add a new process as a condor daemon, just add one line in 
// the structure below. The first elemnt is the string that is 
// looked for in the config file for the executable, and the
// second element is the parameter looked for in the confit
// file for the name of the corresponding log file.If no log
// file need be there, then put a zero in the second column.
// The third parameter is the name of a condor_config variable
// that is ckecked before the process is created. If it is zero, 
// then the process is created always. If it is a valid name,
// this name shud be set to true in the condor_config file for the
// process to be created.

// make sure that the master does not start itself : set runs_here off

extern Daemons 		daemons;
extern int			master_backoff_constant;	// Backoff time constant
extern int			master_backoff_ceiling;		// Backoff time ceiling
extern float		master_backoff_factor;		// exponential factor
extern int			master_recover_time;		// recovering time
extern int			shutdown_graceful_timeout;
extern int			shutdown_fast_timeout;
extern int			Lines;
extern int			PublishObituaries;
extern int			StartDaemons;
extern int			GotDaemonsOff;
extern char*		MasterName;

///////////////////////////////////////////////////////////////////////////
// daemon Class
///////////////////////////////////////////////////////////////////////////
daemon::daemon(char *name, bool is_daemon_core, bool is_h )
{
	char	buf[1000];

	name_in_config_file = strdup(name);
	daemon_name = strchr(name_in_config_file, '-');
	if(daemon_name)
	{
		daemon_name++;
		if(*daemon_name == '_')
		{
			daemon_name++;
			if(*daemon_name == '\0')
			{
				daemon_name = NULL;
			}
		}
		else
		{
			daemon_name = NULL;
		}
	} 

	// Store our HA state
	this->ha_lock = NULL;
	this->is_ha = is_h;

	// Default to not on hold (will be set to true if controlled (i.e by HAD))
	on_hold = FALSE;

	m_waiting_for_startup = false;
	m_reload_shared_port_addr_after_startup = false;
	m_never_use_shared_port = false;
	m_only_stop_when_master_stops = false;

	// Handle configuration
	DoConfig( true );

	// Default log file name...
	sprintf(buf, "%s_LOG", name);
	log_filename_in_config_file = strdup(buf);

	// Check the process name (is it me?)
	process_name = NULL;
    watch_name = NULL;
	log_name = NULL;
	if( strcmp(name, "MASTER") == MATCH ) {
		runs_here = FALSE;
	} else {
		runs_here = TRUE;
	}
	runs_on_this_host();
	pid = 0;
	restarts = 0;
	newExec = FALSE; 
	timeStamp = 0;
	startTime = 0;
	isDC = is_daemon_core;
	start_tid = -1;
	recover_tid = -1;
	stop_tid = -1;
	stop_fast_tid = -1;
 	hard_kill_tid = -1;
	stop_state = NONE;
	needs_update = FALSE;
	num_controllees = 0;

#if 0
	port = NULL;
	config_info_file = NULL;
#endif
	type = stringToDaemonType( name );
	daemons.RegisterDaemon(this);
}

int
daemon::runs_on_this_host()
{
	char	*tmp;
	char	hostname[512];
	static char	**this_host_addr_list = 0;
	static int		host_addr_count;
	struct hostent	*hp;
	int		i, j;


	if ( flag_in_config_file != NULL ) {
		if (strncmp(flag_in_config_file, "BOOL_", 5) == MATCH) {
			tmp	= param( flag_in_config_file);
			if ( tmp && (*tmp == 't' || *tmp == 'T')) {
				runs_here = TRUE;
			} else {
				runs_here = FALSE;
			}
		} else {
			if (this_host_addr_list == 0) {
				if (condor_gethostname(hostname, sizeof(hostname)) < 0) {
					EXCEPT( "gethostname(0x%p,%d)", hostname, 
						   sizeof(hostname));
				}
				if( (hp=condor_gethostbyname(hostname)) == 0 ) {
					EXCEPT( "gethostbyname(%s)", hostname );
				}
				for (host_addr_count = 0; hp->h_addr_list[host_addr_count];
					 host_addr_count++ ) {
					/* Empty Loop */
				}
				this_host_addr_list = (char **) malloc(host_addr_count * 
													   sizeof (char *));
				for (i = 0; i < host_addr_count; i++) {
					this_host_addr_list[i] = (char *) malloc(hp->h_length);
					memcpy(this_host_addr_list[i], hp->h_addr_list[i],
						   hp->h_length);
				}
			}
			/* Get the name of the host on which this daemon should run */
			tmp = param( flag_in_config_file );
			if (!tmp) {
				dprintf(D_ALWAYS, "config file parameter %s not specified",
						flag_in_config_file);
				return FALSE;
			}
			runs_here = FALSE;
			hp = condor_gethostbyname( tmp );
			if (hp == 0) {
				dprintf(D_ALWAYS, "Master couldn't lookup host %s\n", tmp);
				return FALSE;
			} 
			for (i = 0; i < host_addr_count; i++) {
				for (j = 0; hp->h_addr_list[j]; j++) {
					if (memcmp(this_host_addr_list[i], hp->h_addr_list[j],
							   hp->h_length) == MATCH) {
						runs_here = TRUE;
						break;
					}
				}
			}
		}
	}
	if(strcmp(name_in_config_file, "KBDD") == 0)
	// X_RUNS_HERE controls whether or not to run kbdd if it's presented in
	// the config file
	{
		tmp = param("X_RUNS_HERE");
		if(tmp)
		{
			if(*tmp == 'T' || *tmp == 't')
			{
				runs_here = TRUE;
			}
			else
			{
				runs_here = FALSE;
			}
		free(tmp);
		}
	}
	return runs_here;
}


void
daemon::Recover()
{
	restarts = 0;
	recover_tid = -1; 
	dprintf(D_FULLDEBUG, "%s recovered\n", name_in_config_file);
}


int
daemon::NextStart()
{
	int seconds;
	seconds = m_backoff_constant + (int)ceil(::pow(m_backoff_factor, restarts));
	if( seconds > m_backoff_ceiling || seconds < 0 ) {
		seconds = m_backoff_ceiling;
	}
	return seconds;
}


int daemon::Restart()
{
	int		n;

	if ( controller && ( restarts >= 2 ) ) {
		dprintf( D_ALWAYS, "Telling %s's controller (%s)\n",
				 name_in_config_file, controller->name_in_config_file );
		controller->Stop( );
		on_hold = true;
	}
	if( on_hold ) {
		return FALSE;
	}
	if(newExec == TRUE) {
		restarts = 0;
		newExec = FALSE; 
		n = new_bin_delay;
	} else {
		n = NextStart();
		restarts++;
	}

	if( recover_tid != -1 ) {
		dprintf( D_FULLDEBUG, 
				 "Cancelling recovering timer (%d) for %s\n", 
				 recover_tid, process_name );
		daemonCore->Cancel_Timer( recover_tid );
		recover_tid = -1;
	}
	if( start_tid != -1 ) {
		daemonCore->Cancel_Timer( start_tid );
	}
	start_tid = daemonCore->
		Register_Timer( n, (TimerHandlercpp)&daemon::DoStart,
						"daemon::DoStart()", this );
	dprintf(D_ALWAYS, "restarting %s in %d seconds\n", process_name, n);

		// Update the CM now so we see that this daemon is down.
	daemons.UpdateCollector();

		// Set a flag so Start() knows to update once it's up again.
	needs_update = TRUE;

	return 1;
}


// This little function is used so when the start timer goes off, we
// can set the start_tid back to -1 before we actually call Start().
void
daemon::DoStart()
{
	start_tid = -1;
	Start( true );		// Don't forward this on to the controller!
}


// This function handles all of the configuration stuff; at startup and
// at reconfig time.
void
daemon::DoConfig( bool init )
{
	char	*tmp;
	char	buf[1000];

	// Initialize some variables the first time through
	if ( init ) {
		flag_in_config_file = NULL;
	}

	// Check for the _FLAG parameter
	sprintf(buf, "%s_FLAG", name_in_config_file );
	tmp = param(buf);

	// Previously defind?
	if ( NULL != flag_in_config_file ) {
		// Has it changed?
		if ( strcmp( flag_in_config_file, tmp ) ) {
			free( flag_in_config_file );
			flag_in_config_file = tmp;
		} else if ( NULL != tmp ) {
			// Unchanged, just free up tmp
			free( tmp );
		} else {
			// Do nothing
		}
	} else {
		// Not previously defined; just use tmp whatever it is
		flag_in_config_file = tmp;
	}

	// High Availability?
	if ( is_ha ) {
		if ( SetupHighAvailability( ) < 0 ) {
			// What should we do here????
		}
	}

	// get env settings from config file if present
	sprintf(buf, "%s_ENVIRONMENT", name_in_config_file );
	char *env_string = param( buf );

	Env env_parser;
	MyString env_error_msg;

	if(!env_parser.MergeFromV1RawOrV2Quoted(env_string,&env_error_msg)) {
		EXCEPT("ERROR: Failed to parse %s_ENVIRONMENT in config file: %s\n",
		       name_in_config_file,
			   env_error_msg.Value());
	}
	free(env_string);

	this->env.Clear();
	this->env.MergeFrom(env_parser);

	// Check for the _INITIAL_STATE parameter (only at init time)
	// Default to on_hold = false, set to true of state eq "off"
	if ( init ) {
		sprintf(buf, "MASTER_%s_CONTROLLER", name_in_config_file );
		controller_name = param( buf );
		controller = NULL;		// Setup later in Daemons::CheckDaemonConfig()
		on_hold = true;
		if ( controller_name ) {
			dprintf( D_FULLDEBUG, "Daemon %s is controlled by %s\n",
					 name_in_config_file, controller_name );
		}
	}

	// XXX These defaults look to be very wrong, compare with 
	// MASTER_BACKOFF_*.
	sprintf(buf, "MASTER_%s_BACKOFF_CONSTANT", name_in_config_file );
	m_backoff_constant = param_integer( buf, master_backoff_constant, 1 );

	sprintf(buf, "MASTER_%s_BACKOFF_CEILING", name_in_config_file );
	m_backoff_ceiling = param_integer( buf, master_backoff_ceiling, 1 );

	sprintf(buf, "MASTER_%s_BACKOFF_FACTOR", name_in_config_file );
	m_backoff_factor = param_double( buf, master_backoff_factor, 0 );
	if( m_backoff_factor <= 0.0 ) {
    	m_backoff_factor = master_backoff_factor;
    }
	
	sprintf(buf, "MASTER_%s_RECOVER_FACTOR", name_in_config_file );
	m_recover_time = param_integer( buf, master_recover_time, 1 );


	// Weiru
	// In the case that we have several for example schedds running on the
	// same machine, we specify SCHEDD__1, SCHEDD_2, ... in the config file.
	// We want to be able to specify only one executable for them as well as
	// different executables, or one flag for all of them as well as
	// different flags for each. So we instead of specifying in config file
	// SCHEDD__1 = /unsup/condor/bin/condor_schedd, SCHEDD__2 = /unsup/condor/
	// bin/condor_schedd, ... we can simply say SCHEDD = /unsup/...
	if( ( NULL == flag_in_config_file ) && ( NULL != daemon_name ) ) {
		*(daemon_name - 2) = '\0';
		sprintf(buf, "%s_FLAG", name_in_config_file);
		flag_in_config_file = param(buf);
		*(daemon_name - 2) = '_';
	} 

	if( strcmp(name_in_config_file,"SHARED_PORT") == 0 ) {
			// It doesn't make sense for the shared port server to
			// itself try to exist behind a shared port.  It needs
			// its own port.
		m_never_use_shared_port = true;

			// It is not essential to wait for the shared port server
			// to be ready for other daemons to start, but it makes
			// for a cleaner start, avoiding a minute or so during
			// which some daemons advertise themselves without an
			// address.

		ASSERT( param(m_after_startup_wait_for_file,"SHARED_PORT_DAEMON_AD_FILE") );
		m_reload_shared_port_addr_after_startup = true;

		if( SharedPortEndpoint::UseSharedPort() ) {
				// If the master is using the shared port server, stopping
				// the shared port server would make the master inaccessible,
				// so never stop it unless the master itself is stopping.
				// Also, even if the master is shutting down, do not stop
				// shared port server until all other daemons have exited,
				// because they may be depending on it.

			m_only_stop_when_master_stops = true;
		}
	}
}

void
daemon::Hold( bool hold, bool never_forward )
{
	if ( controller && !never_forward ) {
		dprintf( D_FULLDEBUG, "Forwarding Hold to %s's controller (%s)\n",
				 name_in_config_file, controller->name_in_config_file );
		controller->Hold( hold );
	} else {
		this->on_hold = hold;
	}
}

int
daemon::Start( bool never_forward )
{
	if ( controller ) {
		if ( !never_forward ) {
			dprintf( D_FULLDEBUG,
					 "Forwarding Start to %s's controller (%s)\n",
					 name_in_config_file, controller->name_in_config_file );
			return controller->Start( );
		} else {
			dprintf( D_FULLDEBUG,
					 "Handling Start for %s myself\n",
					 name_in_config_file );
		}
	}
	if( start_tid != -1 ) {
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
	if( on_hold ) {
		return FALSE;
	}
	if( pid ) {
			// Already running
		return TRUE;
	}

		// If this is an HA service, we need to lock it first.
	if ( is_ha && ha_lock ) {
		int lockstat = ha_lock->AcquireLock( true );
		if ( lockstat < 0 ) {
			dprintf(D_ALWAYS, "%s: HA lock error\n", name_in_config_file );
		} else if ( lockstat > 0 ) {
			dprintf(D_FULLDEBUG, "%s: HA lock busy\n", name_in_config_file );
		}

			// Here, we have the lock // The LockAcquired callback
			// will have already fired up the daemon for us.
		return 0;
	}

	// Now, go start the process
	return RealStart( );
}

// The real start logic; this is called by the LockAcquired callback when
// we've acquired the lock
int daemon::RealStart( )
{
	const char	*shortname;
	int 	command_port = isDC ? TRUE : FALSE;
	char	buf[512];
	ArgList args;

	// Copy a couple of checks from Start
	dprintf( D_FULLDEBUG, "::RealStart; %s on_hold=%d\n", name_in_config_file, on_hold );
	if( on_hold ) {
		return FALSE;
	}
	if( pid ) {
			// Already running
		return TRUE;
	}
		// Check for a controller. If one exists, e.g. HAD, it is
		// likely this daemon has stop_state = FAST, but we want to
		// let it start anyway. The stop_state != NONE check is to
		// avoid letting the master start HA daemons it is
		// maintaining. A controller indicates something else is
		// maintaining the daemon.
	if( !controller && stop_state != NONE ) {
			// Currently trying to shutdown, this might happen for an
			// HA daemon who acquires a lock just before shutdown.
		dprintf( D_FULLDEBUG, "::RealStart; %s stop_state=%d, ignoring\n",
				 name_in_config_file, stop_state );
		return FALSE;
	}

	shortname = condor_basename( process_name );

	if( access(process_name,X_OK) != 0 ) {
		dprintf(D_ALWAYS, "%s: Cannot execute\n", process_name );
		pid = 0; 
		// Schedule to try and restart it a little later
		Restart();
		return 0;
	}

		// Collector needs to listen on a well known port, as does the
		// Negotiator.  
		// We didn't want them to use root for any reason, but b/c of
		// evil in the security code where we're looking up host certs
		// in the keytab file, we still need root afterall. :(
	bool wants_condor_priv = false;
	if ( strcmp(name_in_config_file,"COLLECTOR") == 0 ) {
			// **** OLD
			// If we're spawning a collector, we can get the right
			// port by asking the global Collector object for it,
			// since we've already instantiated that with the info for
			// the local pool's collector.  This also saves the
			// trouble of instantiating a new DCCollector object,
			// which duplicates some effort and is less efficient. 
			// **** END OLD

			// ckireyev 09/10/04
			// Now that we have multiple collectors, the way to figure out
			// the port on this machine, is to go through all of the
			// collectors until we find the one for THIS machine. Then
			// get the port from that entry
		command_port = -1;

		dprintf ( 
			D_FULLDEBUG, 
			"Looking for matching Collector on '%s' ...\n", 
			my_full_hostname () );
		CollectorList* collectors = NULL;
		if ((collectors = daemonCore->getCollectorList())) {
			char * my_hostname = my_full_hostname();
			Daemon * my_daemon;
			collectors->rewind();
			while (collectors->next (my_daemon)) {

				dprintf ( 
					D_FULLDEBUG, 
					"Matching '%s:%d'\n", 
					my_daemon->fullHostname (),
					my_daemon->port () );

				if (same_host (my_hostname, 
							   my_daemon->fullHostname())) {
					command_port = my_daemon->port();
					dprintf ( D_FULLDEBUG, "Host name matches.\n" );
					break;
				}
			}
				// If the user explicitly asked for command port 0,
				// meaning "pick an ephemeral port", we need to pass
				// 1 to CreateProcess, as the command_port is overloaded
				// to mean "is this a DC process", and if we pass 0 in
				// DC will start the collector as a non DC process.
				// DC special cases command_port = 1 to mean "any"
			if (command_port == 0) {
				command_port = 1;
			}
		}
		dprintf ( 
			D_FULLDEBUG, 
			"Finished looking for Collectors.\n" );

		if (command_port == -1) {
				// strange....
			command_port = COLLECTOR_PORT;
			dprintf (D_ALWAYS, "Collector port not defined, will use default: %d\n", COLLECTOR_PORT);
		}

		dprintf (D_FULLDEBUG, "Starting Collector on port %d\n", command_port);


			// We can't do this b/c of needing to read host certs as root 
			// wants_condor_priv = true;
	} else if( stricmp(name_in_config_file,"CONDOR_VIEW") == 0 ||
			   stricmp(name_in_config_file,"VIEW_SERVER") == 0 ) {
		Daemon d( DT_VIEW_COLLECTOR );
		command_port = d.port();
			// We can't do this b/c of needing to read host certs as root 
			// wants_condor_priv = true;
	} 
	else if( strcmp(name_in_config_file,"NEGOTIATOR") == 0 ) {
		char* host = getCmHostFromConfig( "NEGOTIATOR" );
		if( host ) {
			free (host);
			Daemon d( DT_NEGOTIATOR );
			command_port = d.port();
			// We can't do this b/c of needing to read host certs as root 
			// wants_condor_priv = true;
		}
	}

	priv_state priv_mode = PRIV_ROOT;
	
	snprintf(buf, sizeof(buf), "%s_USERID",name_in_config_file);
	char * username = param( buf );
	if(username) {
		// domain is set to NULL since we don't care about the domain
		// unless we're on Windows, and on Windows the master
		// always runs as SYSTEM (as a service). --stolley
		int result = init_user_ids(username, NULL);
		if(result) {
			priv_mode = PRIV_USER_FINAL;
		} else {
			dprintf(D_ALWAYS,"couldn't switch to user %s!\n",username);
		}
		free(username);
	}
	if( wants_condor_priv && priv_mode == PRIV_ROOT ) {

#ifndef	WIN32

			// we go this route on UNIX b/c it's safer, easier, and it
			// automatically gets it right if CONDOR_IDS is defined.
		uid_t cuid = get_condor_uid();
		gid_t cgid = get_condor_gid();
		int result = set_user_ids( cuid, cgid );
		if( result ) { 
			priv_mode = PRIV_USER_FINAL;
		} else {
			dprintf( D_ALWAYS, 
					 "couldn't switch to \"condor\" user %d.%d!\n",
					 cuid, cgid );
		}

#else /* WIN32 */

			// don't know what to do here just yet...

#endif /* WIN32 */
	}

	args.AppendArg(shortname);
	if(isDC) {
		args.AppendArg("-f");
	}

	snprintf( buf, sizeof( buf ), "%s_ARGS", name_in_config_file );
	char *daemon_args = param( buf );

	MyString args_error;
	if(!args.AppendArgsV1RawOrV2Quoted(daemon_args,&args_error)) {
		dprintf(D_ALWAYS,"ERROR: failed to parse %s daemon arguments: %s\n",
				buf,
				args_error.Value());
		Restart();
		free(daemon_args);
		return 0;
	}

	free(daemon_args);

    // The below chunk is for HAD support

    // take command port from arguments( buf )
    // Don't mess with buf or tmp (they are not our variables) -
    // allocate them again
    if ( isDC ) {
		int i;
		for(i=0;i<args.Count();i++) {
			char const *cur_arg = args.GetArg(i);
			if(strcmp( cur_arg, "-p" ) == 0 ) {
				if(i+1<args.Count()) {
					char const *port_arg = args.GetArg(i+1);
					command_port = atoi(port_arg);
				}
			}
		}
    }

	// set up a FamilyInfo structure so Daemon Core registers a process family
	// for this daemon with the procd
	//
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 60);

	int jobopts = 0;
	if( m_never_use_shared_port ) {
		jobopts |= DCJOBOPT_NEVER_USE_SHARED_PORT;
	}

	pid = daemonCore->Create_Process(
				process_name,	// program to exec
				args,			// args
				priv_mode,		// privledge level
				1,				// which reaper ID to use; use default reaper
				command_port,	// port to use for command port; TRUE=choose one dynamically
				&env,			// environment
				NULL,			// current working directory
				&fi,
				NULL,
				NULL,
				NULL,
				0,
				NULL,
				jobopts);			// we want a new process family

	if ( pid == FALSE ) {
		// Create_Process failed!
		dprintf( D_FAILURE|D_ALWAYS,
				 "ERROR: Create_Process failed trying to start %s\n",
				 process_name);
		pid = 0;
		// Schedule to try and start it a litle later
		Restart();
		return 0;
	}

	m_waiting_for_startup = true;

	// if this is a restart, start recover timer
	if (restarts > 0) {
		recover_tid = daemonCore->Register_Timer( m_recover_time,
						(TimerHandlercpp)&daemon::Recover,
						"daemon::Recover()", this );
		dprintf(D_FULLDEBUG, "start recover timer (%d)\n", recover_tid);
	}

	const char	*proc_type = command_port ? "DaemonCore " : "";
	if ( DebugFlags & D_FULLDEBUG ) {
		MyString	 args_string, tmp;
		args.GetArgsStringForDisplay( &tmp, 1 );
		if( tmp.Length() ) {
			args_string  = " ";
			args_string += tmp;
		}
		else {
			args_string = tmp;
		}
		dprintf( D_FAILURE|D_ALWAYS,
				 "Started %sprocess \"%s%s\", pid and pgroup = %d\n",
				 proc_type, process_name, args_string.Value(), pid );
	}
	else {
		dprintf( D_FAILURE|D_ALWAYS,
				 "Started %sprocess \"%s\", pid and pgroup = %d\n",
				 proc_type, process_name, pid );
	}
	
		// Make sure we've got the current timestamp for updates, etc.
	timeStamp = GetTimeStamp(watch_name);

		// record the time we were started
	startTime = time(0);

		// Since we just started it, we know it's not a new executable. 
	newExec = FALSE;

		// Be sure to reset the stop_state, if the daemon is HAD
		// managed it will have been set to FAST on startup
	stop_state = NONE;

		// If starting the collector, give it a few seconds to get
		// going before starting other daemons or talking to ti
		// ourselves
	if ( strcmp(name_in_config_file,"COLLECTOR") == 0 ) {
		dprintf(D_FULLDEBUG, "Pausing to allow the collector to start\n");
		sleep(3);
	}

	if( needs_update ) {
		needs_update = FALSE;
		daemons.UpdateCollector();
	}

	return pid;	
}

bool
daemon::WaitBeforeStartingOtherDaemons(bool first_time)
{

	if( !m_waiting_for_startup ) {
		return false;
	}

	bool wait = false;
	if( !m_after_startup_wait_for_file.IsEmpty() ) {
		StatInfo si( m_after_startup_wait_for_file.Value() );
		if( si.Error() != 0 ) {
			wait = true;
			dprintf(D_ALWAYS,"Waiting for %s to appear.\n",
					m_after_startup_wait_for_file.Value() );
		}
		else if( !first_time ) {
			dprintf(D_ALWAYS,"Found %s.\n",
					m_after_startup_wait_for_file.Value() );
		}
	}

	if( !wait && m_waiting_for_startup ) {
		m_waiting_for_startup = false;
		DoActionAfterStartup();
	}

	return wait;
}

void
daemon::DoActionAfterStartup()
{
	if( m_reload_shared_port_addr_after_startup ) {
		daemonCore->ReloadSharedPortServerAddr();
	}
}

void
daemon::Stop( bool never_forward )
{
	if( type == DT_MASTER ) {
			// Never want to stop master.
		return;
	}
	if ( controller ) {
		if ( !never_forward ) {
			dprintf( D_FULLDEBUG,
					 "Forwarding Stop to %s's controller (%s)\n",
					 name_in_config_file, controller->name_in_config_file );
			controller->Stop();
			return;
		} else {
			dprintf( D_FULLDEBUG,
					 "Handling Stop for %s myself\n",
					 name_in_config_file );
		}
	}
	if( start_tid != -1 ) {
			// If we think we need to start this in the future, don't. 
		dprintf( D_ALWAYS, "Canceling timer to re-start %s\n", 
				 name_in_config_file );
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
	if( stop_state == GRACEFUL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = GRACEFUL;
		// Test for pid after setting state so HA daemons that aren't
		// running get notified that they are shutting down
	if( !pid ) {
			// We're not running, just return.
		return;
	}

	Kill( SIGTERM );

	stop_fast_tid = 
		daemonCore->Register_Timer( shutdown_graceful_timeout, 0, 
									(TimerHandlercpp)&daemon::StopFastTimer,
									"daemon::StopFastTimer()", this );
}


void
daemon::StopPeaceful() 
{
	// Peaceful shutdown is the same as graceful shutdown, but
	// we never time out waiting for the process to finish.

	if( type == DT_MASTER ) {
			// Never want to stop master.
		return;
	}
	if( start_tid != -1 ) {
			// If we think we need to start this in the future, don't. 
		dprintf( D_ALWAYS, "Canceling timer to re-start %s\n", 
				 name_in_config_file );
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
	if( stop_state == PEACEFUL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = PEACEFUL;
		// Test for pid after setting state so HA daemons that aren't
		// running get notified that they are shutting down
	if( !pid ) {
			// We're not running, just return.
		return;
	}

	// Ideally, we would somehow tell the daemon to die peacefully
	// (only currently applies to startd).  However, we only have
	// two signals to work with: fast and graceful.  Until a better
	// mechanism comes along, we depend on the peaceful state
	// being pre-set in the daemon via a message sent by condor_off.

	Kill( SIGTERM );
}

void
daemon::StopFastTimer()
{
	StopFast(false);
}

void
daemon::StopFast( bool never_forward )
{
	if( type == DT_MASTER ) {
			// Never want to stop master.
		return;
	}
	if ( controller ) {
		if ( !never_forward ) {
			dprintf( D_FULLDEBUG,
					 "Forwarding StopFast to %s's controller (%s)\n",
					 name_in_config_file, controller->name_in_config_file );
			controller->StopFast();
			return;
		} else {
			dprintf( D_FULLDEBUG,
					 "Handling StopFast for %s myself\n",
					 name_in_config_file );
		}
	}
	if( start_tid != -1 ) {
			// If we think we need to start this in the future, don't. 
		dprintf( D_ALWAYS, "Canceling timer to re-start %s\n", 
				 name_in_config_file );
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
	if( stop_state == FAST ) {
			// We've already been here, just return.
		return;
	}
	stop_state = FAST;
		// Test for pid after setting state so HA daemons that aren't
		// running get notified that they are shutting down
	if( !pid ) {
			// We're not running, just return.
		return;
	}

	if( stop_fast_tid != -1 ) {
		dprintf( D_ALWAYS, 
				 "Timeout for graceful shutdown has expired for %s.\n", 
				 name_in_config_file );
		stop_fast_tid = -1;
	}

	Kill( SIGQUIT );

	hard_kill_tid = 
		daemonCore->Register_Timer( shutdown_fast_timeout, 0, 
									(TimerHandlercpp)&daemon::HardKill,
									"daemon::HardKill()", this );
}

void
daemon::HardKill()
{
	if( type == DT_MASTER ) {
			// Never want to stop master.
		return;
	}
	if( stop_state == KILL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = KILL;
		// Test for pid after setting state so HA daemons that aren't
		// running get notified that they are shutting down
	if( !pid ) {
			// We're not running, just return.
		return;
	}

	if( hard_kill_tid != -1 ) {
		dprintf( D_ALWAYS, 
				 "Timeout for fast shutdown has expired for %s.\n", 
				 name_in_config_file );
		hard_kill_tid = -1;
	}

		// Actually do the kill.
	KillFamily();
	dprintf( D_ALWAYS, 
			 "Sent SIGKILL to %s (pid %d) and all its children.\n",
			 name_in_config_file, pid );
}


void
daemon::Exited( int status )
{
	MyString msg;
	msg.sprintf( "The %s (pid %d) ", name_in_config_file, pid );
	bool had_failure = true;
	if (daemonCore->Was_Not_Responding(pid)) {
		msg += "was killed because it was no longer responding";
	}
	else if (WIFSIGNALED(status)) {
		msg += "died due to ";
		msg += daemonCore->GetExceptionString(status);
	}
	else {
		msg += "exited with status ";
		msg += WEXITSTATUS(status);
		if( WEXITSTATUS(status) == DAEMON_NO_RESTART ) {
			had_failure = false;
			msg += " (daemon will not restart automatically)";
				// The "on_hold" flag is the magic that makes this
				// feature work.  Once a daemon has this set, the
				// master won't restart it (Restart() exits
				// immediately), and it doesn't check executable
				// timestamps and restart based on that, either.
			on_hold = true;
		}
	}
	int d_flag = D_ALWAYS;
	if( had_failure ) {
		d_flag |= D_FAILURE;
    }
	dprintf(d_flag, "%s\n", msg.Value());

		// For HA, release the lock
	if ( is_ha && ha_lock ) {
		ha_lock->ReleaseLock( );
	}

		// Let my controller know what's happenned
	if ( controller && ( restarts >= 2 ) ) {
		on_hold = true;
		if ( stop_state == NONE ) {
			dprintf( D_ALWAYS, "Telling %s's controller (%s)\n",
					 name_in_config_file, controller->name_in_config_file );
			controller->Stop( );
		}
	}

		// Kill any controllees I might have
	for( int num = 0;  num < num_controllees;  num++ ) {
		dprintf( D_ALWAYS, "Killing %s's controllee (%s)\n",
				 name_in_config_file,
				 controllees[num]->name_in_config_file );
		controllees[num]->StopFast( true );
	}

		// For good measure, try to clean up any dead/hung children of
		// the daemon that just died by sending SIGKILL to it's
		// entire process family.
	KillFamily();

		// Set flag saying if it exited cuz it was not responding
	was_not_responding = daemonCore->Was_Not_Responding(pid);

		// Mark this daemon as gone.
	pid = 0;
	CancelAllTimers();
	stop_state = NONE;
}


void
daemon::Obituary( int status )
{
    FILE    *mailer;

	/* If daemon with a serious bug gets installed, we may end up
	 ** doing many restarts in rapid succession.  In that case, we
	 ** don't want to send repeated mail to the CONDOR administrator.
	 ** This could overwhelm the administrator's machine.
	 */
    if ( restarts > 3 ) return;

    // always return for KBDD
    if( strcmp( name_in_config_file, "KBDD") == 0 ) {
        return;
	}

#ifndef WIN32
	// Just return if process was killed with SIGKILL.  This means the
	// admin did it, and thus no need to send email informing the
	// admin about something they did...

	// Unless, that is, the master killed it because it was unresponsive
	// Then, send the obit...
	if ( (WIFSIGNALED(status)) && (WTERMSIG(status) == SIGKILL) 
		&& !was_not_responding) {
		return;
	}
#endif

	// Just return if process exited with status 0, if we failed to
	// execute it in the first place, or if it exited intentionally
	// telling us not to restart it.  If everthing's ok, why bother
	// sending email?
	if ( (WIFEXITED(status)) && 
		 ( (WEXITSTATUS(status) == 0 ) || 
		   (WEXITSTATUS(status) == JOB_EXEC_FAILED) ||
		   (WEXITSTATUS(status) == DAEMON_NO_RESTART) ) ) {
		return;
	}

    dprintf( D_ALWAYS, "Sending obituary for \"%s\"\n",
			process_name);

    char buf[1000];

	MyString email_subject;
	email_subject.sprintf("Problem %s: %s ", my_full_hostname(), condor_basename(process_name));
	if ( was_not_responding ) {
		email_subject += "killed (unresponsive)";
	} else {
		MyString fmt;
		if( WIFSIGNALED(status) ) {
			fmt.sprintf("died (%d)", WTERMSIG(status));
		} else {
			fmt.sprintf("exited (%d)", WEXITSTATUS(status));
		}
		email_subject += fmt;
	}

    sprintf( buf, "%s_ADMIN_EMAIL", name_in_config_file );
    char *address = param(buf);
    if(address) {
        mailer = email_open(address, email_subject.Value());
        free(address);
    } else {
        mailer = email_admin_open(email_subject.Value());
    }

    if( mailer == NULL ) {
        return;
    }

	fprintf( mailer, "\"%s\" on \"%s\" ",process_name,
		my_full_hostname());

	if ( was_not_responding ) {
		fprintf( mailer, "was killed because\nit was no longer responding.\n");
	} else {
		if( WIFSIGNALED(status) ) {
			fprintf( mailer, "died due to %s.\n",
				daemonCore->GetExceptionString(status) );
		} else {
			fprintf( mailer,"exited with status %d.\n",WEXITSTATUS(status) );
		}
	}

	fprintf( mailer, 
		"Condor will automatically restart this process in %d seconds.\n",
		NextStart());


	if( log_name ) {
		email_asciifile_tail( mailer, log_name, Lines );
	}

	// on NT, we will now email the last entry in our pseudo-core file,
	// since it is ascii...  note: email_corefile_tail() is a no-op on Unix
	if ( WIFSIGNALED(status) ) {
		email_corefile_tail( mailer, name_in_config_file );
	}

	email_close(mailer);
}


void
daemon::CancelAllTimers()
{
	if( stop_tid  != -1 ) {
		daemonCore->Cancel_Timer( stop_tid );
		stop_tid = -1;
	}
	if( stop_fast_tid  != -1 ) {
		daemonCore->Cancel_Timer( stop_fast_tid );
		stop_fast_tid = -1;
	}
	if( hard_kill_tid  != -1 ) {
		daemonCore->Cancel_Timer( hard_kill_tid );
		hard_kill_tid = -1;
	}
	if( recover_tid  != -1 ) {
		daemonCore->Cancel_Timer( recover_tid );
		recover_tid = -1;
	}
	if( start_tid != -1 ) {
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
}


void
daemon::CancelRestartTimers()
{
	if( recover_tid  != -1 ) {
		daemonCore->Cancel_Timer( recover_tid );
		recover_tid = -1;
	}
	if( start_tid != -1 ) {
		dprintf( D_ALWAYS, "Canceling timer to re-start %s\n", 
				 name_in_config_file );
		daemonCore->Cancel_Timer( start_tid );
		start_tid = -1;
	}
}


void
daemon::Kill( int sig )
{
	if( (!pid) || (pid == -1) ) {
		return;
	}
	int status;
	status = daemonCore->Send_Signal(pid,sig);
	if ( status == FALSE )
		status = -1;
	else
		status = 1;

	const char* sig_name = signalName( sig );
	char buf[32];
	if( ! sig_name ) {
		sprintf( buf, "signal %d", sig );
		sig_name = buf;
	}
	if( status < 0 ) {
		dprintf( D_ALWAYS, "ERROR: failed to send %s to pid %d\n",
				 sig_name, pid );
	} else {
		dprintf( D_ALWAYS, "Sent %s to %s (pid %d)\n",
				 sig_name, name_in_config_file, pid );
	}
}


void
daemon::KillFamily( void ) 
{
	if( pid == 0 ) {
		return;
	}
	if (daemonCore->Kill_Family(pid) == FALSE) {
		dprintf(D_ALWAYS,
		        "error killing process family of daemon with pid %u\n",
		        pid);
	}
}


void
daemon::Reconfig()
{
	if( stop_state != NONE ) {
			// If we're currently trying to shutdown this daemon,
			// there's no need to reconfig it.
		return;
	}
	DoConfig( false );
	Kill( SIGHUP );
}

int
daemon::SetupHighAvailability( void )
{
	char		*tmp;
	char		*url;
	MyString	name;

	// Get the URL
	name.sprintf("HA_%s_LOCK_URL", name_in_config_file );
	tmp = param( name.Value() );
	if ( ! tmp ) {
		tmp = param( "HA_LOCK_URL" );
	}
	if ( ! tmp ) {
		dprintf(D_ALWAYS, "Warning: %s listed in HA, "
				"but no HA lock URL provided!!\n",
				name_in_config_file );
		return -1;
	}
	url = tmp;

	// Get the length of the lock
	time_t		lock_hold_time = 60 * 60;	// One hour
	name.sprintf( "HA_%s_LOCK_HOLD_TIME", name_in_config_file );
	tmp = param( name.Value( ) );
	if ( ! tmp ) {
		tmp = param( "HA_LOCK_HOLD_TIME" );
	}
	if ( tmp ) {
		if ( !isdigit( tmp[0] ) ) {
			dprintf(D_ALWAYS,
					"HA time '%s' is not a number; using default %ld\n",
					tmp, (long)lock_hold_time );
		} else {
			lock_hold_time = (time_t) atol( tmp );
		}
		free( tmp );
	}

	// Get the lock poll time
	time_t		poll_period = 5 * 60;		// Five minutes
	name.sprintf( "HA_%s_POLL_PERIOD", name_in_config_file );
	tmp = param( name.Value() );
	if ( ! tmp ) {
		tmp = param( "HA_POLL_PERIOD" );
	}
	if ( tmp ) {
		if ( !isdigit( tmp[0] ) ) {
			dprintf(D_ALWAYS,
					"HA time '%s' is not a number; using default %ld\n",
					tmp, (long)poll_period );
		} else {
			poll_period = (time_t) atol( tmp );
		}
		free( tmp );
	}

	// Now, create the lock object
	if ( ha_lock ) {
		int status = ha_lock->SetLockParams( url,
											 name_in_config_file,
											 poll_period,
											 lock_hold_time,
											 true );
		if ( status ) {
			dprintf( D_ALWAYS, "Failed to change HA lock parameters\n" );
		} else {
			dprintf( D_FULLDEBUG,
					 "Set HA lock for %s; URL='%s' poll=%lds hold=%lds\n",
					 name_in_config_file, url, (long)poll_period, 
					 (long)lock_hold_time );
		}
	} else {
		ha_lock = new CondorLock( url,
								  name_in_config_file,
								  this,
								  (LockEvent) &daemon::HaLockAcquired,
								  (LockEvent) &daemon::HaLockLost,
								  poll_period,
								  lock_hold_time,
								  true );

			// Log the new lock creation (if successful)
		if ( ha_lock ) {
			dprintf( D_FULLDEBUG,
					 "Created HA lock for %s; URL='%s' poll=%lds hold=%lds\n",
					 name_in_config_file, url, (long)poll_period, 
					 (long)lock_hold_time );
		}
	}
	free( url );

	// And, if it failed, log it
	if ( ! ha_lock ) {
		dprintf( D_ALWAYS, "HA Lock creation failed for URL '%s' / '%s'\n",
				 url, name_in_config_file );
		return -1;
	}

	return 0;
}


int
daemon::HaLockAcquired( LockEventSrc src )
{
	dprintf( D_FULLDEBUG, "%s: Got HA lock (%s); starting\n",
			 name_in_config_file, ha_lock->EventSrcString(src) );

		// Start the deamon in either case
	return RealStart( );
}


int
daemon::HaLockLost( LockEventSrc src )
{
	dprintf( D_FULLDEBUG, "%s: Lost HA lock (%s); stoping\n",
			 name_in_config_file, ha_lock->EventSrcString(src) );
	if ( LOCK_SRC_APP == src ) {
			// We released the lock from the ReleaseLock() call; we already
			// know about it.  Do nothing.
		return 0;
	}

		// Poll detected that we've lost the lock; pull the plug NOW!!
	dprintf( D_ALWAYS,
			 "%s: HA poll detected broken lock: emergency stop!!\n",
			 name_in_config_file );
	StopFast( );
	return 0;
}


int
daemon::SetupController( void )
{
	if ( !controller_name ) {
		return 0;
	}

	// Find the matching daemon by name
	controller = daemons.FindDaemon( controller_name );
	if ( ! controller ) {
		dprintf( D_ALWAYS,
				 "%s: Can't find my controller daemon '%s'\n",
				 name_in_config_file, controller_name );
		return -1;
	}
	if ( controller->RegisterControllee( this ) < 0 ) {
		dprintf( D_ALWAYS,
				 "%s: Can't register controller daemon '%s'\n",
				 name_in_config_file, controller_name );
		return -1;
	}

	// Done
	return 0;
}

int
daemon::RegisterControllee( class daemon *controllee )
{
	if ( num_controllees >= MAX_CONTROLLEES ) {
		return -1;
	}
	controllees[num_controllees++] = controllee;
	return 0;
}


///////////////////////////////////////////////////////////////////////////
//  Daemons Class
///////////////////////////////////////////////////////////////////////////

Daemons::Daemons()
{
	daemon_ptr = NULL;
	no_daemons  =  0;
	daemon_list_size = 0;
	check_new_exec_tid = -1;
	update_tid = -1;
	preen_tid = -1;
	reaper = NO_R;
	all_daemons_gone_action = MASTER_RESET;
	immediate_restart = FALSE;
	immediate_restart_master = FALSE;
	prevLHF = 0;
	m_retry_start_all_daemons_tid = -1;
}


void
Daemons::RegisterDaemon(class daemon *d)
{
	int i;
	if( !daemon_ptr ) {
		daemon_list_size = 10;
		daemon_ptr = (class daemon **) malloc(daemon_list_size * sizeof(class daemon *));
		for( i=0; i<daemon_list_size; i++ ) {
			daemon_ptr[i] = NULL;
		}
	}

	if (no_daemons >= daemon_list_size) {
		i = daemon_list_size;
		daemon_list_size *= 2;
		daemon_ptr = (class daemon **) realloc(daemon_ptr, 
										daemon_list_size * sizeof(class daemon *));
		for( ; i<daemon_list_size; i++ ) {
			daemon_ptr[i] = NULL;
		}

	}
	daemon_ptr[no_daemons] = d;
	no_daemons++;
}

int
Daemons::SetupControllers( void )
{
	// Find controlling daemons
	for( int i=0; i < no_daemons; i++ ) {
		if ( daemon_ptr[i]->SetupController( ) < 0 ) {
			dprintf( D_ALWAYS,
					 "SetupControllers: Setup for daemon %s failed\n",
					 daemon_ptr[i]->daemon_name );
			return -1;
		}
	}
	return 0;
}

void
Daemons::InitParams()
{
	char* buf;
	char* tmp = NULL;
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->process_name ) {
			tmp = daemon_ptr[i]->process_name;
		}
		daemon_ptr[i]->process_name = param(daemon_ptr[i]->name_in_config_file);
		if( !daemon_ptr[i]->process_name ) {
			dprintf( D_ALWAYS, 
					 "%s from your DAEMON_LIST is not defined the config files!\n", 
					 daemon_ptr[i]->name_in_config_file ); 
			EXCEPT( "Must have the path to %s defined.", 
					daemon_ptr[i]->name_in_config_file ); 
		}
		if( tmp && strcmp(daemon_ptr[i]->process_name, tmp) ) {
				// The path to this daemon has changed in the config
				// file, we will need to start the new version.
			daemon_ptr[i]->newExec = TRUE;
		}
		if (tmp) {
			free( tmp );
			tmp = NULL;
		}
		if( daemon_ptr[i]->watch_name ) {
			tmp = daemon_ptr[i]->watch_name;
		}
			
		int length = strlen(daemon_ptr[i]->name_in_config_file) + 32;
		buf = (char *)malloc(length);
		snprintf( buf, length, "%s_WATCH_FILE", 
				daemon_ptr[i]->name_in_config_file );
		daemon_ptr[i]->watch_name = param( buf );
		free(buf);
		if( !daemon_ptr[i]->watch_name)	{
			daemon_ptr[i]->watch_name = strdup(daemon_ptr[i]->process_name);
		} 

		if( tmp && strcmp(daemon_ptr[i]->watch_name, tmp) ) {
			// tmp is what the old watch_name was 
			// The path to what we're watching has changed in the 
			// config file, we will need to start the new version.
			daemon_ptr[i]->timeStamp = 0;
		}

		if (tmp) {
			free( tmp );
			tmp = NULL;
		}

			// check that log file is necessary
		if ( daemon_ptr[i]->log_filename_in_config_file != NULL) {
			if( daemon_ptr[i]->log_name ) {
				free( daemon_ptr[i]->log_name );
			}
			daemon_ptr[i]->log_name = 
					param(daemon_ptr[i]->log_filename_in_config_file);
			if ( daemon_ptr[i]->log_name == NULL && daemon_ptr[i]->runs_here ) {
				dprintf(D_ALWAYS, "Log file not found in config file: %s\n", 
						daemon_ptr[i]->log_filename_in_config_file);
			}
		}
	}
}


int
Daemons::GetIndex(const char* name)
{
	for ( int i=0; i < no_daemons; i++)
		if ( strcmp(daemon_ptr[i]->name_in_config_file,name) == 0 )
			return i;	
	return -1;
}


class daemon *
Daemons::FindDaemon( const char *name )
{
	int		index = GetIndex( name );
	if ( index < 0 ) {
		return NULL;
	} else {
		return daemon_ptr[index];
	}
}


void
Daemons::CheckForNewExecutable()
{
	int found_new = FALSE;

	dprintf(D_FULLDEBUG, "enter Daemons::CheckForNewExecutable\n");

	if( daemon_ptr[master]->newExec ) {
			// We already noticed the master has a new binary, so we
			// already started to restart it.  There's nothing else to
			// do here.
		return;
	}
    if( NewExecutable( daemon_ptr[master]->watch_name,
					   &daemon_ptr[master]->timeStamp ) ) {
		dprintf( D_ALWAYS,"%s was modified, restarting %s.\n", 
				 daemon_ptr[master]->watch_name, 
				 daemon_ptr[master]->process_name );
		daemon_ptr[master]->newExec = TRUE;
			// Begin the master restart procedure.
		
        RestartMaster();
		return;
    }

    for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->runs_here && !daemon_ptr[i]->newExec 
			&& ! daemon_ptr[i]->OnHold()
			&& !daemon_ptr[i]->OnlyStopWhenMasterStops())
		{
			if( NewExecutable( daemon_ptr[i]->watch_name,
							   &daemon_ptr[i]->timeStamp ) ) {
				found_new = TRUE;
				daemon_ptr[i]->newExec = TRUE;				
				if( immediate_restart ) {
						// If we want to avoid the new_binary_delay,
						// we can just set the newExec flag to false,
						// reset restarts to 0, and kill the daemon.
						// When it gets restarted, the new binary will
						// be used, but we won't think it's a new
						// binary, so we won't use the new_bin_delay.
					daemon_ptr[i]->newExec = FALSE;
					daemon_ptr[i]->restarts = 0;
				}
				if( daemon_ptr[i]->pid ) {
					dprintf( D_ALWAYS,"%s was modified, killing %s.\n", 
							 daemon_ptr[i]->watch_name,
							 daemon_ptr[i]->process_name );
					daemon_ptr[i]->Stop();
				} else {
					if( immediate_restart ) {
							// This daemon isn't running now, but
							// there's a new binary.  Cancel the
							// current start timer and restart it
							// now. 
						daemon_ptr[i]->CancelAllTimers();
						daemon_ptr[i]->Restart();
					}
				}
			}
		}
	}
	if( found_new ) {
		UpdateCollector();
	}
	dprintf(D_FULLDEBUG, "exit Daemons::CheckForNewExecutable\n");
}


void
Daemons::DaemonsOn()
{
		// Maybe someday we'll add code here to edit the config file.
	StartDaemons = TRUE;
	GotDaemonsOff = FALSE;
	StartAllDaemons();
	StartNewExecTimer();
}


void
Daemons::DaemonsOff( int fast )
{
		// Maybe someday we'll add code here to edit the config file.
	StartDaemons = FALSE;
	GotDaemonsOff = TRUE;
	all_daemons_gone_action = MASTER_RESET;
	CancelNewExecTimer();
	if( fast ) {
		StopFastAllDaemons();
	} else {
		StopAllDaemons();
	}
}

void
Daemons::DaemonsOffPeaceful( )
{
		// Maybe someday we'll add code here to edit the config file.
	StartDaemons = FALSE;
	GotDaemonsOff = TRUE;
	all_daemons_gone_action = MASTER_RESET;
	CancelNewExecTimer();
	StopPeacefulAllDaemons();
}

void
Daemons::ScheduleRetryStartAllDaemons()
{
	if( m_retry_start_all_daemons_tid == -1 ) {
		m_retry_start_all_daemons_tid = daemonCore->Register_Timer(
			1,
			(TimerHandlercpp)&Daemons::RetryStartAllDaemons,
			"Daemons::RetryStartAllDaemons",
			this);
		ASSERT( m_retry_start_all_daemons_tid != -1 );
	}
}

void
Daemons::CancelRetryStartAllDaemons()
{
	if( m_retry_start_all_daemons_tid != -1 ) {
		daemonCore->Cancel_Timer(m_retry_start_all_daemons_tid);
		m_retry_start_all_daemons_tid = -1;
	}
}

void
Daemons::RetryStartAllDaemons()
{
	m_retry_start_all_daemons_tid = -1;
	StartAllDaemons();
}

void
Daemons::StartAllDaemons()
{
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->pid > 0 ) {
			if( daemon_ptr[i]->WaitBeforeStartingOtherDaemons(false) ) {
				ScheduleRetryStartAllDaemons();
				return;
			}

				// the daemon is already started
			continue;
		} 
		if( ! daemon_ptr[i]->runs_here ) continue;
		daemon_ptr[i]->Hold( FALSE );
		daemon_ptr[i]->Start();

		if( daemon_ptr[i]->WaitBeforeStartingOtherDaemons(true) ) {
			ScheduleRetryStartAllDaemons();
			return;
		}
	}
}


void
Daemons::StopAllDaemons() 
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();
	int running = 0;
	for( int i=0; i < no_daemons; i++ ) {
			// Need to stop HA daemons from trying to start during
			// shutdown
		if( ( daemon_ptr[i]->pid || daemon_ptr[i]->IsHA() ) &&
			daemon_ptr[i]->runs_here &&
			!daemon_ptr[i]->OnlyStopWhenMasterStops() )
		{
			daemon_ptr[i]->Stop();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}	   
}


void
Daemons::StopFastAllDaemons()
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();
	int running = 0;
	for( int i=0; i < no_daemons; i++ ) {
			// Need to stop HA daemons from trying to start during
			// shutdown
		if( ( daemon_ptr[i]->pid || daemon_ptr[i]->IsHA() ) &&
			daemon_ptr[i]->runs_here &&
			!daemon_ptr[i]->OnlyStopWhenMasterStops() )
		{
			daemon_ptr[i]->StopFast();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}	   
}

void
Daemons::StopPeacefulAllDaemons()
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();
	int running = 0;
	for( int i=0; i < no_daemons; i++ ) {
			// Need to stop HA daemons from trying to start during
			// shutdown
		if( ( daemon_ptr[i]->pid || daemon_ptr[i]->IsHA() ) &&
			daemon_ptr[i]->runs_here &&
			!daemon_ptr[i]->OnlyStopWhenMasterStops() )
		{
			daemon_ptr[i]->StopPeaceful();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}	   
}


void
Daemons::HardKillAllDaemons()
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();
	int running = 0;
	for( int i=0; i < no_daemons; i++ ) {
			// Need to stop HA daemons from trying to start during
			// shutdown
		if( ( daemon_ptr[i]->pid || daemon_ptr[i]->IsHA() ) &&
			daemon_ptr[i]->runs_here &&
			!daemon_ptr[i]->OnlyStopWhenMasterStops() )
		{
			daemon_ptr[i]->HardKill();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}	   
}

void
Daemons::ReconfigAllDaemons()
{
	dprintf( D_ALWAYS, "Reconfiguring all running daemons.\n" );
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->pid && daemon_ptr[i]->runs_here ) {
			daemon_ptr[i]->Reconfig();
		}
	}
}


void
Daemons::InitMaster()
{
		// initialize master data structure
	master = GetIndex("MASTER");
	if ( master == -1 ) {
		EXCEPT("InitMaster: MASTER not Specified");
	}
	daemon_ptr[master]->timeStamp = 
		GetTimeStamp(daemon_ptr[master]->watch_name);
	daemon_ptr[master]->startTime = time(0);
	daemon_ptr[master]->pid = daemonCore->getpid();
}


void
Daemons::RestartMaster()
{
	immediate_restart_master = immediate_restart;
	all_daemons_gone_action = MASTER_RESTART;
	StartDaemons = FALSE;
	StopAllDaemons();
}

void
Daemons::RestartMasterPeaceful()
{
	immediate_restart_master = immediate_restart;
	all_daemons_gone_action = MASTER_RESTART;
	StartDaemons = FALSE;
	StopPeacefulAllDaemons();
}

// This function is called when all the children have finally exited
// and we're ready to actually do the restart.  It checks whether we
// want to immediately restart or not, and either sets a daemonCore
// timer to call FinalRestartMaster() after MASTER_NEW_BINARY_DELAY or
// just calls FinalRestartMaster directly.
void
Daemons::FinishRestartMaster()
{
	if( immediate_restart_master ) {
		dprintf( D_ALWAYS, "Restarting master right away.\n" );
		FinalRestartMaster();
	} else {
		dprintf( D_ALWAYS, "Restarting master in %d seconds.\n",
				 new_bin_delay );
		daemonCore->
			Register_Timer( new_bin_delay, 0, 
							(TimerHandlercpp)&Daemons::FinalRestartMaster,
							"Daemons::FinalRestartMaster()", this );
	}
}

void
Daemons::CleanupBeforeRestart()
{
		// Tell DaemonCore to cleanup the ProcFamily subsystem. We need
		// to do this here since we are about to restart without calling
		// DC_Exit
		//
	daemonCore->Proc_Family_Cleanup();
	
#ifndef WIN32
	int	max_fds = getdtablesize();

		// Release file lock on the log file.
	if ( MasterLock->release() == FALSE ) {
		dprintf( D_ALWAYS,
				 "Can't remove lock on \"%s\"\n",daemon_ptr[master]->log_name);
		EXCEPT( "file_lock(%d)", MasterLockFD );
	}
	(void)close( MasterLockFD );

		// Now set close-on-exec on all fds so our new invocation of
		// condor_master does not inherit them.
		// Note: Not needed (or wanted) on Win32, as CEDAR creates 
		//		Winsock sockets as non-inheritable by default.
		// Also not wanted for stderr, since we might be logging
		// to that.  No real need for stdin or stdout either.
	for (int i=3; i < max_fds; i++) {
		int flag = fcntl(i,F_GETFD,0);
		if( flag != -1 ) {
			fcntl(i,F_SETFD,flag | 1);
		}
	}
#endif

		// Get rid of the CONDOR_INHERIT env variable.  If it is there,
		// when the master restarts daemon core will think it's parent
		// is a daemon core process.  but, its parent is gone ( we are doing
		// an exec, so we disappear), thus we must blank out the 
		// CONDOR_INHERIT env variable.
	char	tmps[256];
	sprintf( tmps, "%s=", EnvGetName( ENV_INHERIT ) );
	putenv( tmps );
}

void
Daemons::ExecMaster()
{
	int i=0,j;
	char **argv = (char **)malloc((condor_main_argc+2)*sizeof(char *));

	ASSERT( condor_main_argc>0 );
	argv[i++] = condor_main_argv[0];

		// insert "-f" argument so that new master does not fork
	argv[i++] = "-f";
	for(j=1;j<condor_main_argc;j++) {
		size_t len = strlen(condor_main_argv[j]);
		if( strncmp(condor_main_argv[j],"-foreground",len)==0 ) {
			i--; // do not need to insert -f, because it is already there
			break;
		}
	}

		// preserve all additional arguments (e.g. -n, -pid, ...)
		// except for "-background"
	for(j=1;j<condor_main_argc;j++) {
		size_t len = strlen(condor_main_argv[j]);
		if( strncmp(condor_main_argv[j],"-background",len)!=0 ) {
			argv[i++] = condor_main_argv[j];
		}

		if( daemon_stop_time && strncmp(condor_main_argv[j],"-runfor",len)==0 && j+1<condor_main_argc ) {
				// adjust "runfor" time (minutes)
			j++;

			int runfor = (daemon_stop_time-time(NULL))/60;
			if( runfor <= 0 ) {
				runfor = 1; // minimum 1
			}
			MyString runfor_str;
			runfor_str.sprintf("%d",runfor);
			argv[i++] = strdup(runfor_str.Value());
		}
	}
	argv[i++] = NULL;

	(void)execv(daemon_ptr[master]->process_name, argv);

	free(argv);
}

// Function that actually does the restart of the master.
void
Daemons::FinalRestartMaster()
{
	int i;

	CleanupBeforeRestart();

#ifdef WIN32
	dprintf(D_ALWAYS,"Running as NT Service = %d\n", NT_ServiceFlag);
#endif
		// Make sure the exec() of the master works.  If it fails,
		// we'll fall past the execl() call, and hit the exponential
		// backoff code.
	while(1) {
		// On Win32, if we are running as an NT service,
		// we cannot just exec ourselves again.  This is because
		// the SCM (Service Control Manager) needs to know our PID,
		// and doing an exec() will change our PID on NT.  So, we
		// exec cmd.exe (which is the NT Command interpreter, i.e. the
		// default shell) and issue a "net stop" and "net start" command.
		if ( NT_ServiceFlag == TRUE ) {
#ifdef WIN32
			char systemshell[MAX_PATH];
			extern char* _condor_myServiceName; // created by daemonCore

			::GetSystemDirectory(systemshell,MAX_PATH);
			strcat(systemshell,"\\cmd.exe");
			MyString command;
			command.sprintf("net stop %s & net start %s", 
				_condor_myServiceName, _condor_myServiceName);
			dprintf( D_ALWAYS, "Doing exec( \"%s /Q /C %s\" )\n", 
				 systemshell,command.Value());
			(void)execl(systemshell, "/Q", "/C",
				command.Value(), 0);
#endif
		} else {
			dprintf( D_ALWAYS, "Doing exec( \"%s\" )\n", 
				 daemon_ptr[master]->process_name);

				// It is important to switch to ROOT_PRIV, in case we are
				// executing a master wrapper script that expects to be
				// root rather than effective uid condor.
			priv_state saved_priv = set_priv(PRIV_ROOT);

			ExecMaster();

			set_priv(saved_priv);
		}
		daemon_ptr[master]->restarts++;
		if ( NT_ServiceFlag == TRUE && daemon_ptr[master]->restarts > 7 ) {
			dprintf(D_ALWAYS,"Unable to restart Condor service, aborting.\n");
			master_exit(1);
		}
		i = daemon_ptr[master]->NextStart();
		dprintf( D_ALWAYS, 
				 "Cannot execute condor_master (errno=%d), will try again in %d seconds\n", 
				 errno, i );
		sleep(i);
	}
}


char* Daemons::DaemonLog( int pid )
{
	// be careful : a pointer to data in this class is returned
	// posibility of getting tampered

	for ( int i=0; i < no_daemons; i++)
		if ( daemon_ptr[i]->pid == pid )
			return (daemon_ptr[i]->log_name);
	return "Unknown Program!!!";
}


#if 0
void
Daemons::SignalAll( int signal )
{
	// Sends the given signal to all daemons except the master
	// itself.  (Master has runs_here set to false).
	for ( int i=0; i < no_daemons; i++) {
		if( daemon_ptr[i]->runs_here && (daemon_ptr[i]->pid > 0) ) {
			daemon_ptr[i]->Kill(signal);
		}
	}
}
#endif


// This function returns the number of active child processes currently being
// supervised by the master.
int Daemons::NumberOfChildren()
{
	int result = 0;
	for( int i=0; i < no_daemons; i++) {
		if( daemon_ptr[i]->runs_here && daemon_ptr[i]->pid
			&& !daemon_ptr[i]->OnlyStopWhenMasterStops() ) {
			result++;
		}
	}
	dprintf(D_FULLDEBUG,"NumberOfChildren() returning %d\n",result);
	return result;
}


int
Daemons::AllReaper(int pid, int status)
{
		// find out which daemon died
	for( int i=0; i < no_daemons; i++) {
		if( pid == daemon_ptr[i]->pid ) {
			daemon_ptr[i]->Exited( status );
			if( NumberOfChildren() == 0 ) {
				AllDaemonsGone();
			}
			return TRUE;
		}
	}
	return TRUE;
}


int
Daemons::DefaultReaper(int pid, int status)
{
	for( int i=0; i < no_daemons; i++) {
		if( pid == daemon_ptr[i]->pid ) {
			daemon_ptr[i]->Exited( status );
			if( PublishObituaries ) {
				daemon_ptr[i]->Obituary( status );
			}
			daemon_ptr[i]->Restart();
			return TRUE;
		}
	}
	return TRUE;
}



// This function will set the reaper to one that calls
// AllDaemonsGone() when all the daemons have exited.
void
Daemons::SetAllReaper()
{
	if( reaper == ALL_R ) {
			// All reaper is already set.
		return;
	}
	daemonCore->Reset_Reaper( 1, "All Daemon Reaper",
							  (ReaperHandlercpp)&Daemons::AllReaper,
							  "Daemons::AllReaper()",this);
	reaper = ALL_R;
}


// This function will set the reaper to one that calls
// AllDaemonsGone() when all the daemons have exited.
void
Daemons::SetDefaultReaper()
{
	if( reaper == DEFAULT_R ) {
			// The default reaper is already set.
		return;
	}
	static int already_registered_reaper = 0;
	if ( already_registered_reaper ) {
		daemonCore->Reset_Reaper( 1, "Default Daemon Reaper",
							  (ReaperHandlercpp)&Daemons::DefaultReaper,
							  "Daemons::DefaultReaper()",this);
	} else {
		already_registered_reaper = 1;
		daemonCore->Register_Reaper( "Default Daemon Reaper",
								 (ReaperHandlercpp)&Daemons::DefaultReaper,
								 "Daemons::DefaultReaper()", this );
	}
	reaper = DEFAULT_R;
}

bool
Daemons::StopDaemonsBeforeMasterStops()
{
		// now shut down all the daemons that should only stop right
		// before the master stops
	int running = 0;
	for( int i=no_daemons; i--; ) {
		if( ( daemon_ptr[i]->pid || daemon_ptr[i]->IsHA() )
			&& daemon_ptr[i]->runs_here )
		{
			daemon_ptr[i]->Stop();
			running++;
		}
	}
	return !running;
}

void
Daemons::AllDaemonsGone()
{
	switch( all_daemons_gone_action ) {
	case MASTER_RESET:
		dprintf( D_ALWAYS, "All daemons are gone.\n" );
		SetDefaultReaper();
		break;
	case MASTER_RESTART:
		if( !StopDaemonsBeforeMasterStops() ) {
			return;
		}
		dprintf( D_ALWAYS, "All daemons are gone.  Restarting.\n" );
		FinishRestartMaster();
		break;
	case MASTER_EXIT:
		if( !StopDaemonsBeforeMasterStops() ) {
			return;
		}
		dprintf( D_ALWAYS, "All daemons are gone.  Exiting.\n" );
		master_exit(0);
		break;
	}
}


void
Daemons::StartTimers()
{
	static int old_update_int = -1;
	static int old_check_int = -1;
	static int old_preen_int = -1;

	if( old_update_int != update_interval ) {
		if( update_tid != -1 ) {
			daemonCore->Cancel_Timer( update_tid );
		}
			// Start updating the CM in 5 seconds, since if we're the
			// CM, we want to give ourself the chance to start the
			// collector before we try to send an update to it.
		update_tid = daemonCore->
			Register_Timer( 5, update_interval,
							(TimerHandlercpp)&Daemons::UpdateCollector,
							"Daemons::UpdateCollector()", this );
		old_update_int = update_interval;
	}

	int new_preen = (int)( old_preen_int != preen_interval );
	int first_preen = MIN( HOUR, preen_interval );
	if( !FS_Preen || new_preen ) {
		if( preen_tid != -1 ) {
			daemonCore->Cancel_Timer( preen_tid );
		}
	}
	if( new_preen && FS_Preen ) {
		preen_tid = daemonCore->
			Register_Timer( first_preen, preen_interval,
							run_preen,
							"run_preen()" );
	}
	old_preen_int = preen_interval;

	if( old_check_int != check_new_exec_interval ) {
			// new value, restart timer
		CancelNewExecTimer();
		StartNewExecTimer();
	}
	old_check_int = check_new_exec_interval;
}	


void
Daemons::StartNewExecTimer( void )
{
	if( ! check_new_exec_interval ) {
			// Nothing to do.
		return;
	}
	if( check_new_exec_tid != -1 ) {
			// Timer already on, nothing to do.
		return;
	}
	if( ! StartDaemons ) {
			// Don't want to check for new executables if we're not
			// supposed to be running daemons.
		return;
	}
	check_new_exec_tid = daemonCore->
		Register_Timer( 5, check_new_exec_interval,
						(TimerHandlercpp)&Daemons::CheckForNewExecutable,
						"Daemons::CheckForNewExecutable()", this );
	if( check_new_exec_tid == -1 ) {
		dprintf( D_ALWAYS, "ERROR! Can't register DaemonCore timer!\n" );
	}
}


void
Daemons::CancelNewExecTimer( void )
{
	if( check_new_exec_tid != -1 ) {
		daemonCore->Cancel_Timer( check_new_exec_tid );
		check_new_exec_tid = -1;
	}
}


// Fill in Timestamp and startTime for all daemons info into a
// classad.  If a daemon is supposed to be running but isn't, put a 0
// for the startTime to signify that daemon is down.
void
Daemons::Update( ClassAd* ca ) 
{
	char buf[128];
	for( int i=0; i < no_daemons; i++) {
		if( daemon_ptr[i]->runs_here || i == master ) {
			sprintf( buf, "%s_Timestamp = %ld", 
					 daemon_ptr[i]->name_in_config_file, 	
					 (long)daemon_ptr[i]->timeStamp );
			ca->Insert( buf );
			if( daemon_ptr[i]->pid ) {
				sprintf( buf, "%s_StartTime = %ld", 
						 daemon_ptr[i]->name_in_config_file, 	
						 (long)daemon_ptr[i]->startTime );
				ca->Insert( buf );
			} else {
					// No pid, but daemon's supposed to be running.
				sprintf( buf, "%s_StartTime = 0", 
						 daemon_ptr[i]->name_in_config_file );
				ca->Insert( buf );
			}
		}
	}

		// CRUFT
	ad->Assign(ATTR_MASTER_IP_ADDR, daemonCore->InfoCommandSinfulString());

		// Initialize all the DaemonCore-provided attributes
	daemonCore->publish( ad ); 	
}


void
Daemons::UpdateCollector()
{
	dprintf(D_FULLDEBUG, "enter Daemons::UpdateCollector\n");

	Update(ad);
    daemonCore->publish(ad);
    daemonCore->monitor_data.ExportData(ad);
	daemonCore->sendUpdates(UPDATE_MASTER_AD, ad, NULL, true);

#if HAVE_DLOPEN
	MasterPluginManager::Update(ad);
#endif

		// log classad into sql log so that it can be updated to DB
	FILESQL::daemonAdInsert(ad, "MasterAd", FILEObj, prevLHF);
	
		// Reset the timer so we don't do another period update until 
	daemonCore->Reset_Timer( update_tid, update_interval, update_interval );

	dprintf( D_FULLDEBUG, "exit Daemons::UpdateCollector\n" );
}


class daemon*
Daemons::FindDaemon( daemon_t dt )
{
	for( int i=0; i < no_daemons; i++ ) {
		if( daemon_ptr[i]->type == dt ) {
			return daemon_ptr[i];
		}
	}
	return NULL;
}


void
Daemons::CancelRestartTimers( void )
{
		// We don't need to be checking for new executables anymore. 
	if( check_new_exec_tid != -1 ) {
		daemonCore->Cancel_Timer( check_new_exec_tid );
		check_new_exec_tid = -1;
	}
	if( preen_tid != -1 ) {
		daemonCore->Cancel_Timer( preen_tid );
		preen_tid = -1;
	}

		// Finally, cancel the start/restart timers for each daemon.  
	for( int i=0; i < no_daemons; i++ ) {
		daemon_ptr[i]->CancelRestartTimers();
	}
}
