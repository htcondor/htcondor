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
#include "basename.h"
#include "condor_email.h"
#include "condor_environ.h"
#include "daemon_list.h"
#include "sig_name.h"
#include "internet.h"
#include "strupr.h"
#include "condor_netdb.h"
#include "file_lock.h"
#include "stat_info.h"
#include "shared_port_endpoint.h"
#include "condor_fix_access.h"
#include "condor_sockaddr.h"
#include "ipv6_hostname.h"
#include "setenv.h"
#include "systemd_manager.h"
#include "dc_startd.h"
#include "dc_collector.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#include "MasterPlugin.h"
#endif

#ifndef WIN32
#include "largestOpenFD.h"
#endif 

// these are defined in master.C
extern int 		MasterLockFD;
extern FileLock*	MasterLock;
extern int		master_exit(int);
extern int		update_interval;
extern int		check_new_exec_interval;
extern int		preen_interval;
extern int		preen_pid;
extern int		new_bin_delay;
extern StopStateT new_bin_restart_mode;
extern char*	FS_Preen;
extern			ClassAd* ad;
extern int		NT_ServiceFlag; // TRUE if running on NT as an NT Service

extern time_t	GetTimeStamp(char* file);
extern int 	   	NewExecutable(char* file, time_t* tsp);
extern void		tail_log( FILE*, char*, int );
extern void		run_preen(int tid);

extern int condor_main_argc;
extern char **condor_main_argv;
extern time_t daemon_stop_time;

int		hourly_housekeeping(void);

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
extern int			MasterShuttingDown;
extern char*		MasterName;
extern bool			DaemonStartFastPoll;

///////////////////////////////////////////////////////////////////////////
// daemon Class
///////////////////////////////////////////////////////////////////////////
daemon::daemon(const char *name, bool is_daemon_core, bool is_h )
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
	use_collector_port = false;
	m_only_stop_when_master_stops = false;
	flag_in_config_file = NULL;
	controller_name = NULL;

	// Handle configuration
	DoConfig( true );

	// Default log file name...
	snprintf(buf, sizeof(buf), "%s_LOG", name);
	log_filename_in_config_file = strdup(buf);

	// Check the process name (is it me?)
	flag_in_config_file = NULL;
	process_name = NULL;
	watch_name = NULL;
	log_name = NULL;
	ready_state = NULL;
	if( strcmp(name, "MASTER") == MATCH ) {
		runs_here = FALSE;
	} else {
		runs_here = TRUE;
	}
	pid = 0;
	restarts = 0;
	newExec = FALSE; 
	timeStamp = 0;
	startTime = 0;	// startTime of 0 indicates we have not (yet) ever tried to start this daemon
	isDC = is_daemon_core;
	start_tid = -1;
	recover_tid = -1;
	stop_tid = -1;
	stop_fast_tid = -1;
 	hard_kill_tid = -1;
	stop_state = NONE;
	draining = TRUE;
	needs_update = FALSE;
	num_controllees = 0;

	type = stringToDaemonType( name );
	daemons.RegisterDaemon(this);
}

daemon::~daemon()
{
	if( name_in_config_file != NULL ) {
		free( name_in_config_file );
	}
	if( daemon_name != NULL ) {
		free( daemon_name );
	}
	if( log_filename_in_config_file != NULL ) {
		free( log_filename_in_config_file );
	}
	if( flag_in_config_file != NULL ) {
		free( flag_in_config_file );
	}
	if( process_name != NULL ) {
		free( process_name );
	}
	if( watch_name != NULL ) {
		free( watch_name );
	}
	if( log_name != NULL ) {
		free( log_name );
	}
	if( ready_state != NULL ) {
		free( ready_state );
	}
	if( ha_lock != NULL ) {
		delete( ha_lock );
	}
	if( controller_name != NULL ) {
		free( controller_name );
	}
}


void
daemon::Recover( int /* timerID */ )
{
	restarts = 0;
	recover_tid = -1; 
	dprintf(D_FULLDEBUG, "%s recovered\n", name_in_config_file);
}


int
daemon::NextStart() const
{
	int seconds;
	seconds = m_backoff_constant + (int)ceil(::pow(m_backoff_factor, restarts));
	if( seconds > m_backoff_ceiling || seconds < 0 ) {
		seconds = m_backoff_ceiling;
	}
	return seconds;
}


int daemon::Restart(bool by_command)
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
	if (newExec == TRUE) {
		// The path to this daemon has changed in the config file
		restarts = 0;
		newExec = FALSE;
		n = new_bin_delay;
	} else if (by_command) {
		n = 1; // TODO: can this be 0 ?
		restarts = 0;
	} else {
		n = NextStart();
		restarts++;
	}

	SetReadyState(NULL); // Clear the "Ready" state

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
daemon::DoStart( int /* timerID */ )
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

	// Check for the _FLAG parameter
	snprintf(buf, sizeof(buf), "%s_FLAG", name_in_config_file );
	tmp = param(buf);

	// Previously defind?
	if ( NULL != flag_in_config_file ) {
		// Has it changed?
		if (tmp && strcmp( flag_in_config_file, tmp ) ) {
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
	snprintf(buf, sizeof(buf), "%s_ENVIRONMENT", name_in_config_file );
	char *env_string = param( buf );

	Env env_parser;
	std::string env_error_msg;

	if(!env_parser.MergeFromV1RawOrV2Quoted(env_string, env_error_msg)) {
		EXCEPT("ERROR: Failed to parse %s_ENVIRONMENT in config file: %s",
		       name_in_config_file,
			   env_error_msg.c_str());
	}
	free(env_string);

	this->env.Clear();
	this->env.MergeFrom(env_parser);

#ifdef LINUX
	// dprintf calls localtime() (under the hood) a lot.  if the TZ environment
	// variable is not set, localtime() is required to stat /etc/localtime on 
	// every call, to see if it hasn't changed.  On busy schedds, this stat consumes
	// 5 or more percent of our cpu time(!).  Explicitly setting the TZ env var
	// to :/etc/localtime causes localtime to cache the results.
	//
	
	if (param_boolean("MASTER_SET_TZ", true)) {
		// But don't overwrite it, if already set
		if (!env.HasEnv("TZ") && !getenv("TZ")) {

			// /etc/localtime should always exist, but just
			// double check to be sure
			struct stat sb;
			int r = stat("/etc/localtime", &sb);
			if (r == 0) {
				env.SetEnv("TZ", ":/etc/localtime");
			}
		}
	}
#endif

	if( NULL != controller_name ) {
		DetachController();
		free( controller_name );
	}
	snprintf(buf, sizeof(buf), "MASTER_%s_CONTROLLER", name_in_config_file );
	controller_name = param( buf );
	controller = NULL;		// Setup later in Daemons::CheckDaemonConfig()
	if ( controller_name ) {
		dprintf( D_FULLDEBUG, "Daemon %s is controlled by %s\n",
				 name_in_config_file, controller_name );
	}

	// Check for the _INITIAL_STATE parameter (only at init time)
	// Default to on_hold = false, set to true of state eq "off"
	if ( init ) {
		on_hold = 2;
	}

	// XXX These defaults look to be very wrong, compare with 
	// MASTER_BACKOFF_*.
	snprintf(buf, sizeof(buf), "MASTER_%s_BACKOFF_CONSTANT", name_in_config_file );
	m_backoff_constant = param_integer( buf, master_backoff_constant, 1 );

	snprintf(buf, sizeof(buf), "MASTER_%s_BACKOFF_CEILING", name_in_config_file );
	m_backoff_ceiling = param_integer( buf, master_backoff_ceiling, 1 );

	snprintf(buf, sizeof(buf), "MASTER_%s_BACKOFF_FACTOR", name_in_config_file );
	m_backoff_factor = param_double( buf, master_backoff_factor, 0 );
	if( m_backoff_factor <= 0.0 ) {
    	m_backoff_factor = master_backoff_factor;
    }
	
	snprintf(buf, sizeof(buf), "MASTER_%s_RECOVER_FACTOR", name_in_config_file );
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
		snprintf(buf, sizeof(buf), "%s_FLAG", name_in_config_file);
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

	if( strcmp(name_in_config_file,"COLLECTOR") == 0 ) {
			// if a collector address file is configured, don't start any
			// other daemons until the collector has written this file
		param(m_after_startup_wait_for_file,"COLLECTOR_ADDRESS_FILE");
	}
}

int
daemon::Drain(std::string & request_id, int how_fast, int on_completion, const char * reason, ExprTree* check, ExprTree * start)
{
	// Not running or not a STARTD?  can't drain
	if ( ! pid || type != DT_STARTD) {
		return 0;
	}

	const char * addr = daemonCore->InfoCommandSinfulString(pid);
	if ( ! addr) {
		dprintf(D_STATUS, "Ignoring drain request of %s because address is not known\n", daemon_name);
		return -1;
	}

	std::string check_buf, start_buf;
	const char * check_str = nullptr;
	const char * start_str = nullptr;
	if (check) { check_str = ExprTreeToString(check, check_buf); }
	if (start) { start_str = ExprTreeToString(start, start_buf); }

	DCStartd startd(addr);
	if (startd.drainJobs( how_fast, reason, on_completion, check_str, start_str, request_id )) {
		// drain command sent successfully, so mark the daemon as in the process of exiting
		stop_state = PEACEFUL;
		draining = true;
		return 1;
	}

	return -1;
}

void
daemon::Hold( bool hold, bool never_forward )
{
	SetReadyState(NULL); // clear the ready whenever the hold state changes
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
	char const *daemon_sock = NULL;
	std::string daemon_sock_buf;
	std::string default_id;
	char	buf[512];
	ArgList args;

	param(default_id, "SHARED_PORT_DEFAULT_ID");
	if ( !default_id.size() ) {
		default_id = "collector";
	}
		// Windows has a global pipe namespace, meaning that several instances of
		// HTCondor share the same default ID; we make it unique below.
#ifdef WIN32
	formatstr_cat(default_id, "_%d", getpid());
#endif

	// Copy a couple of checks from Start
	dprintf( D_FULLDEBUG, "::RealStart; %s on_hold=%d\n", name_in_config_file, on_hold );
	if( on_hold ) {
		return FALSE;
	}
	if( pid ) {
			// Already running
		return TRUE;
	}
	if( MasterShuttingDown ) {
		dprintf( D_ALWAYS,
				 "Master is shutting down, so skipping startup of %s\n",
				 name_in_config_file );
		return FALSE;
	}

	shortname = condor_basename( process_name );

	if( access(process_name,X_OK) != 0 ) {
		dprintf(D_ALWAYS, "%s: Cannot execute (errno=%d, %s)\n", process_name, errno, strerror(errno) );
		pid = 0; 
		// Schedule to try and restart it a little later
		Restart();
		return 0;
	}

	if( !m_after_startup_wait_for_file.empty() ) {
		if (0 != remove(m_after_startup_wait_for_file.c_str()) && errno != ENOENT) {
			dprintf(D_ALWAYS, "Cannot remove wait-for-startup file %s\n", m_after_startup_wait_for_file.c_str());
			// Now what?  restart?  exit?
		}
	}

	if( m_reload_shared_port_addr_after_startup ) {
			// We removed the shared port server address file above.
			// Now remove the cached result in memory.  This ensures
			// that the parent address passed to shared_port does not
			// reference an old instance of shared_port that is no
			// longer listening on the specified port.  Instead, we
			// want it to use the "local" address mode that uses the
			// named socket directly.
		daemonCore->ClearSharedPortServerAddr();
	}

	// later we will want to take special action when starting shared port or the collector
	bool daemon_is_shared_port = (MATCH == strcasecmp(name_in_config_file,"SHARED_PORT"));
	bool daemon_is_collector = (MATCH == strcasecmp(name_in_config_file,"COLLECTOR"));

		// We didn't want them to use root for any reason, but b/c of
		// evil in the security code where we're looking up host certs
		// in the keytab file, we still need root afterall. :(
	const bool wants_condor_priv = false;
	bool collector_uses_shared_port = param_boolean("COLLECTOR_USES_SHARED_PORT", true) && param_boolean("USE_SHARED_PORT", false);
		// Collector needs to listen on a well known port.
	if ( daemon_is_collector || (daemon_is_shared_port && use_collector_port) ) {

			// Go through all of the
			// collectors until we find the one for THIS machine. Then
			// get the port from that entry
		command_port = -1;

		dprintf ( 
			D_FULLDEBUG, 
			"Looking for matching Collector on '%s' ...\n", 
			get_local_fqdn().c_str());
		CollectorList* collectors = NULL;
		if ((collectors = daemonCore->getCollectorList())) {
			std::string my_fqdn_str = get_local_fqdn();
			const char * my_hostname = my_fqdn_str.c_str();
			for (auto& my_daemon : collectors->getList()) {

				dprintf ( 
					D_FULLDEBUG, 
					"Matching '%s:%d'\n", 
					my_daemon->fullHostname (),
					my_daemon->port () );

				std::string cm_sinful = empty_if_null(my_daemon->addr());
				condor_sockaddr cm_sockaddr;
				cm_sockaddr.from_sinful(cm_sinful);
				std::string cm_hostname;
				if(my_daemon->fullHostname()) {
					cm_hostname = my_daemon->fullHostname();
				}

				if( cm_sockaddr.is_loopback() ||
					same_host (my_hostname, 
							   cm_hostname.c_str())) {
					Sinful sinful( my_daemon->addr() );
					if( sinful.getSharedPortID() ) {
							// collector is using a shared port
						daemon_sock_buf = sinful.getSharedPortID();
						daemon_sock = daemon_sock_buf.c_str();
						command_port = 1;
					}
					else {
							// collector is using its own port
						command_port = sinful.getPortNum();
					}
					dprintf ( D_FULLDEBUG, "Host name matches collector %s.\n",
							  sinful.getSinful() ? sinful.getSinful() : "NULL" );
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
			int default_port = daemon_is_shared_port ? param_integer("SHARED_PORT_PORT", COLLECTOR_PORT) : param_integer("COLLECTOR_PORT", COLLECTOR_PORT);
			command_port = default_port;
			dprintf (D_ALWAYS, "Collector port not defined, will use default: %d\n", default_port);

			// See comment above.
			if( command_port == 0 ) { command_port = 1; }
		}

		if (collector_uses_shared_port && daemon_is_collector) {
			daemon_sock = default_id.c_str();
		}

		if( daemon_sock ) {
			dprintf (D_FULLDEBUG,"Starting collector with shared port id %s\n",
					 daemon_sock);
		}
		else {
			dprintf (D_FULLDEBUG, "Starting Collector on port %d\n", command_port);
		}

	} else if (daemon_is_shared_port) {

		command_port = param_integer("SHARED_PORT_PORT", COLLECTOR_PORT);
		dprintf (D_ALWAYS, "Starting shared port with port: %d\n", command_port);

		// If the user explicitly asked for command port 0, meaning "pick an ephemeral port",
		// we need to pass 1 to CreateProcess, since it special cases command_port=1 to mean "any"
		// and command_port=0 to mean 'no command port' (i.e. child is not DC)
		if( command_port == 0 ) { command_port = 1; }

			// We can't do this b/c of needing to read host certs as root 
			// wants_condor_priv = true;
	} else if( strcasecmp(name_in_config_file,"CONDOR_VIEW") == 0 ||
			   strcasecmp(name_in_config_file,"VIEW_SERVER") == 0 ) {
		Daemon d( DT_VIEW_COLLECTOR );
		command_port = d.port();
			// We can't do this b/c of needing to read host certs as root 
			// wants_condor_priv = true;
	} 
	else if( strcasecmp(name_in_config_file,"NEGOTIATOR") == 0 ) {
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

	// Daemons other than the master no longer default to background mode, so there is no need to pass -f to them.
	// If we *dont* pass -f, then we can valigrind or strace a daemon just by adding two statements to the config file
	// for example:
	//  JOB_ROUTER = /usr/bin/valgrind
	//  JOB_ROUTER_ARGS = --leak-check=full --log-file=$(LOG)/job_router-vg.%p --error-limit=no $(LIBEXEC)/condor_job_router -f $(JOB_ROUTER_ARGS)

	snprintf( buf, sizeof( buf ), "%s_ARGS", name_in_config_file );
	char *daemon_args = param( buf );

	// Automatically set -localname if appropriate.
	bool setLocalName = false;
	if( isDC ) {
		static constexpr const char *viewServerDaemonNames[] {"VIEW_COLLECTOR", "CONDOR_VIEW", "VIEW_SERVER"};
		auto samey = [&](const char *s) {return strcasecmp(s, name_in_config_file) == 0;};
		if (std::any_of(viewServerDaemonNames, std::size(viewServerDaemonNames) + viewServerDaemonNames, samey) ||
			std::none_of(default_dc_daemon_array, std::size(default_dc_daemon_array) + default_dc_daemon_array, samey)) {
			// Since the config's args are appended after this, they should
			// win, but we might as well do it right.
			bool foundLocalName = false;
			ArgList configArgs;
			std::string configError;
			if( configArgs.AppendArgsV1RawOrV2Quoted( daemon_args, configError ) ) {
				for( size_t i = 0; i < configArgs.Count(); ++i ) {
					char const * configArg = configArgs.GetArg( i );
					if( strcmp( configArg, "-local-name" ) == 0 ) {
						foundLocalName = true;
						if( i + 1 < configArgs.Count() ) {
							daemon_sock_buf = configArgs.GetArg(i + 1);
							lower_case(daemon_sock_buf);
							daemon_sock = daemon_sock_buf.c_str();
							localName = daemon_sock_buf;
							setLocalName = true;
						}
						break;
					}
				}
				if(! foundLocalName) {
					args.AppendArg( "-local-name" );
					args.AppendArg( name_in_config_file );

					// Don't set daemon_sock here, since we'll catch it
					// below if we haven't, and that avoids duplicating
					// the code.
					localName = name_in_config_file;
					setLocalName = true;
				}
			}

			// GT#5768: The master should look for the localname-specific
			// version of any param()s it does while doing this start-up.
		}
	}
	if(! setLocalName) { localName.clear(); }

	// If the daemon shares a binary with the HAD or REPLICATION daemons,
	// respect the setting of the corresponding <SUBSYS>_USE_SHARED_PORT.
	if( isDC ) {
		// We param() for the HAD and REPLICATION daemon binaries explicitly,
		// because it's totally valid for the user to have neither of them
		// in their DAEMON_LIST and yet be using each under some other name.
		// For instance, 'DAEMON_LIST = $(DAEMON_LIST) FIRST_HAD SECOND_HAD'.

		std::string hadDaemonBinary;
		param( hadDaemonBinary, "HAD" );
		if( hadDaemonBinary == process_name ) {
			m_never_use_shared_port = ! param_boolean( "HAD_USE_SHARED_PORT", false );
		}

		std::string replicationDaemonBinary;
		param( replicationDaemonBinary, "REPLICATION" );
		if( replicationDaemonBinary == process_name ) {
			m_never_use_shared_port = ! param_boolean( "REPLICATION_USE_SHARED_PORT", false );
		}
	}

	std::string args_error;
	if(!args.AppendArgsV1RawOrV2Quoted(daemon_args, args_error)) {
		dprintf(D_ALWAYS,"ERROR: failed to parse %s daemon arguments: %s\n",
				buf,
				args_error.c_str());
		Restart();
		free(daemon_args);
		return 0;
	}

	free(daemon_args);

    // The below chunk is for HAD support

    // take command port from arguments( buf )
    // Don't mess with buf or tmp (they are not our variables) -
    // allocate them again
    int udp_command_port = command_port;
    if ( isDC ) {
		size_t i;
		for(i=0;i<args.Count();i++) {
			char const *cur_arg = args.GetArg(i);
			if(strcmp( cur_arg, "-p" ) == 0 ) {
				if(i+1<args.Count()) {
					char const *port_arg = args.GetArg(i+1);
					command_port = atoi(port_arg);
				}
			}
			else if(strncmp( cur_arg, "-sock", strlen(cur_arg)) == 0) {
				i++;
				if( i<args.Count() ) {
					daemon_sock = args.GetArg(i);
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
	// give the family session to all daemons, not just those that get command ports
	// we do this so that the credmon(s) can use python send_alive and set_ready_state methods
	jobopts = DCJOBOPT_INHERIT_FAMILY_SESSION;
	if( m_never_use_shared_port ) {
		jobopts |= DCJOBOPT_NEVER_USE_SHARED_PORT;
	}
	if( daemon_is_shared_port ) {
		jobopts |= DCJOBOPT_NO_UDP | DCJOBOPT_NEVER_USE_SHARED_PORT;
	}
	#ifdef WIN32
	// tell the shared port and collector (via the environment) to use "collector_<master-pid>"
	// as the default shared port id.
	if( daemon_is_shared_port || daemon_is_collector ) {
		env.SetEnv("_condor_SHARED_PORT_DEFAULT_ID", default_id.c_str());
	}
	#endif

	if( daemon_is_shared_port )
	{
		const condor_utils::SystemdManager & sd = condor_utils::SystemdManager::GetInstance();
		const std::vector<int> &fds = sd.GetFDs();
		if (fds.size())
		{
			dprintf(D_ALWAYS, "Using passed TCP socket from systemd.\n");
			jobopts |= DCJOBOPT_USE_SYSTEMD_INET_SOCKET;
		}
	}

	// If we are starting a collector and passed a "-sock" command in the command line,
	// but didn't set the COLLECTOR_HOST to use shared port, have the collector listen on
	// the UDP socket and not the TCP socket.  We assume that the TCP socket will be taken
	// by the shared port daemon.
	if( daemon_is_collector && daemon_sock && command_port > 1 && collector_uses_shared_port ) {
		udp_command_port = command_port;
		command_port = 1;
	} else {
		udp_command_port = command_port;
	}
	if( daemon_sock ) {
		dprintf (D_FULLDEBUG,"Starting daemon with shared port id %s\n",
			daemon_sock);
	} else {
		// We checked for local names already, use the config name here.
		if( isDC ) {
			daemon_sock_buf = name_in_config_file;
			lower_case(daemon_sock_buf);
			// Because the master only starts daemons named in the config
			// file, and those names are by definition unique, we don't
			// need to further uniquify them with a sequence number, and
			// not doing so makes it possible to construct certain
			// addresses, rather than discover them.
			daemon_sock_buf = SharedPortEndpoint::GenerateEndpointName( daemon_sock_buf.c_str(), false );
			daemon_sock = daemon_sock_buf.c_str();
			dprintf( D_FULLDEBUG, "Starting daemon with shared port id %s\n", daemon_sock );
		}
	}
	if( command_port > 1 ) {
		dprintf (D_FULLDEBUG, "Starting daemon on TCP port %d\n", command_port);
		if( udp_command_port != command_port ) {
			dprintf (D_FULLDEBUG, "Starting daemon on UDP port %d\n", command_port);
		}
	}

	pid = daemonCore->Create_Process(
				process_name,	// program to exec
				args,			// args
				priv_mode,		// privledge level
				1,				// which reaper ID to use; use default reaper
				command_port,	// port to use for command port; TRUE=choose one dynamically
				udp_command_port,	// port to use for command port; TRUE=choose one dynamically
				&env,			// environment
				NULL,			// current working directory
				&fi,
				NULL,
				NULL,
				NULL,
				0,
				NULL,
				jobopts,
				NULL,
				NULL,
				daemon_sock);

	if ( priv_mode == PRIV_USER_FINAL ) {
		uninit_user_ids();
	}

	if ( pid == FALSE ) {
		// Create_Process failed!
		dprintf( D_ERROR,
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
	if ( IsFulldebug(D_FULLDEBUG) ) {
		std::string	 args_string, tmp;
		args.GetArgsStringForDisplay( tmp, 1 );
		if( tmp.length() ) {
			args_string  = " ";
			args_string += tmp;
		}
		else {
			args_string = tmp;
		}
		dprintf( D_ALWAYS,
				 "Started %sprocess \"%s%s\", pid and pgroup = %d\n",
				 proc_type, process_name, args_string.c_str(), pid );
	}
	else {
		dprintf( D_ALWAYS,
				 "Started %sprocess \"%s\", pid and pgroup = %d\n",
				 proc_type, process_name, pid );
	}
	
		// Make sure we've got the current timestamp for updates, etc.
	timeStamp = GetTimeStamp(watch_name);

		// record the time we were started
	startTime = time(0);

		// Since we just started it, we know it's not a new executable. 
	newExec = FALSE;

	if( needs_update ) {
		needs_update = FALSE;
		daemons.UpdateCollector();
	}

	// If we just started the shared port daemon, update its entry in
	// in the pid table so that when we shut it down, it doesn't forward
	// the shutdown signal on to the collector.
	if( daemon_is_shared_port ) {
		if(! daemonCore->setChildSharedPortID( pid, "self" ) ) {
			EXCEPT( "Unable to update shared port daemon's Sinful string, won't be able to kill it." );
		}
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
	if( !m_after_startup_wait_for_file.empty() ) {
		StatInfo si( m_after_startup_wait_for_file.c_str() );
		if( si.Error() != 0 ) {
			wait = true;
			dprintf(D_ALWAYS,"Waiting for %s to appear.\n",
					m_after_startup_wait_for_file.c_str() );
			if( DaemonStartFastPoll ) {
				Sleep(100);
			}
		}
		else if( !first_time ) {
			dprintf(D_ALWAYS,"Found %s.\n",
					m_after_startup_wait_for_file.c_str() );
		}
	}

	if( !wait && m_waiting_for_startup ) {
		m_waiting_for_startup = false;
		DoActionAfterStartup();
	}

	return wait;
}

void
daemon::DoActionAfterStartup() const
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
	if( !pid ) {
			// We're not running, just return.
		return;
	}
	if( stop_state == GRACEFUL || stop_state == PEACEFUL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = GRACEFUL;

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
	if( !pid ) {
			// We're not running, just return.
		return;
	}
	if (stop_state == PEACEFUL || stop_state == GRACEFUL) {
			// We've already been here, just return.
		return;
	}
	stop_state = PEACEFUL;

	// Ideally, we would somehow tell the daemon to die peacefully
	// (only currently applies to startd).  However, we only have
	// two signals to work with: fast and graceful.  Until a better
	// mechanism comes along, we depend on the peaceful state
	// being pre-set in the daemon via a message sent by condor_off.

	Kill( SIGTERM );
}

void
daemon::StopFastTimer( int /* timerID */ )
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
	if( !pid ) {
			// We're not running, just return.
		return;
	}
	if( stop_state == FAST ) {
			// We've already been here, just return.
		return;
	}
	stop_state = FAST;
	draining = FALSE;

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
daemon::HardKill( int /* timerID */ )
{
	if( type == DT_MASTER ) {
			// Never want to stop master.
		return;
	}
	if( !pid ) {
			// We're not running, just return.
		return;
	}
	if( stop_state == KILL ) {
			// We've already been here, just return.
		return;
	}
	stop_state = KILL;
	draining = FALSE;

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


bool
daemon::Exited(int status)
{
	bool expected = false;
	std::string msg;
	formatstr( msg, "The %s (pid %d) ", name_in_config_file, pid );
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
		msg += std::to_string( WEXITSTATUS(status) );
		if( WEXITSTATUS(status) == DAEMON_NO_RESTART ) {
			had_failure = false;
			msg += " (daemon will not restart automatically)";
				// The "on_hold" flag is the magic that makes this
				// feature work.  Once a daemon has this set, the
				// master won't restart it (Restart() exits
				// immediately), and it doesn't check executable
				// timestamps and restart based on that, either.
			on_hold = true;
		} else if (WEXITSTATUS(status) == 0 && (on_hold || MasterShuttingDown || (stop_state != NONE))) {
			had_failure = false;
			expected = true;
		}
	}
	dprintf(had_failure ? (D_ERROR | D_EXCEPT) : D_ALWAYS, "%s\n", msg.c_str());

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
	SetReadyState(NULL); // Clear the "Ready" state

		// Mark this daemon as gone.
	pid = 0;
	CancelAllTimers();
	stop_state = NONE;
	draining = false;

	return expected;
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

	std::string email_subject;
	formatstr(email_subject, "Problem %s: %s ", get_local_fqdn().c_str(), 
						  condor_basename(process_name));
	if ( was_not_responding ) {
		email_subject += "killed (unresponsive)";
	} else {
		std::string fmt;
		if( WIFSIGNALED(status) ) {
			formatstr(fmt, "died (%d)", WTERMSIG(status));
		} else {
			formatstr(fmt, "exited (%d)", WEXITSTATUS(status));
		}
		email_subject += fmt;
	}

    snprintf( buf, sizeof(buf), "%s_ADMIN_EMAIL", name_in_config_file );
    char *address = param(buf);
    if(address) {
        mailer = email_nonjob_open(address, email_subject.c_str());
        free(address);
    } else {
        mailer = email_admin_open(email_subject.c_str());
    }

    if( mailer == NULL ) {
        return;
    }

	fprintf( mailer, "\"%s\" on \"%s\" ",process_name, 
			 get_local_fqdn().c_str() );

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


	// If the daemon was given a -localname parameter, it becomes nontrivial
	// to determine what its log file should be, and in general the only way
	// to be sure is for the daemon to actually tell us.  Rather than do all
	// that work in the stable series, we'll just make sure that we at least
	// don't tail the _wrong_ file.
	if( log_name && localName.empty() ) {
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

time_t
daemon::GetNextRestart() const
{
	if( start_tid != -1 ) {
		return daemonCore->GetNextRuntime(start_tid);
	}
	return 0;
}

void
daemon::Kill( int sig ) const
{
	if( (!pid) || (pid == -1) ) {
		return;
	}
	int status;
#ifdef WIN32
	// On windows we don't have any way to send a sigterm to a daemon that doesn't have a command port
	// but we can safely generate a Ctrl+Break because we know that the process was started with CREATE_NEW_PROCESS_GROUP
	// We do this here rather than in windows_softkill because generating the ctrl-break works best
	// if sent by a parent process rather than by a sibling process.  This does nothing if the daemon
	// doesn't have a console, so after we do this go ahead and fall down to the code that does a windows_softkill
	if ( ! isDC && (sig == SIGTERM)) {
		BOOL rbrk = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
		dprintf(D_ALWAYS, "Sent Ctrl+Break to non-daemoncore daemon %d, ret=%d\n", pid, rbrk);
	}
#endif
	status = daemonCore->Send_Signal(pid,sig);
	if ( status == FALSE )
		status = -1;
	else
		status = 1;

	const char* sig_name = signalName( sig );
	char buf[32];
	if( ! sig_name ) {
		snprintf( buf, sizeof(buf), "signal %d", sig );
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
daemon::KillFamily( void ) const 
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
	if (stop_state != NONE && stop_state != PEACEFUL) {
			// If we're currently trying to shutdown this daemon,
			// there's no need to reconfig it.
		return;
	}
	DoConfig( false );
	if( pid ) {
		Kill( SIGHUP );
	}
}


void
daemon::InitParams()
{
	char* buf;
	char* tmp = NULL;

	if( process_name ) {
		tmp = process_name;
	}
	process_name = param(name_in_config_file);
	if( !process_name ) {
		dprintf( D_ALWAYS, 
				"%s is in the DAEMON_LIST parameter, but there is no executable path for it defined in the config files!\n", 
				name_in_config_file ); 
		EXCEPT( "Must have the path to %s defined.", name_in_config_file ); 
	}
	if( tmp && strcmp(process_name, tmp) ) {
			// The path to this daemon has changed in the config
			// file, we will need to start the new version.
		newExec = TRUE;
	}
	if (tmp) {
		free( tmp );
		tmp = NULL;
	}
	if( watch_name ) {
		tmp = watch_name;
	}
			
	int length = (int)strlen(name_in_config_file) + 32;
	buf = (char *)malloc(length);
	ASSERT( buf != NULL );
	snprintf( buf, length, "%s_WATCH_FILE", name_in_config_file );
	watch_name = param( buf );
	free(buf);
	if( !watch_name) {
		watch_name = strdup(process_name);
	} 

	if( tmp && strcmp(watch_name, tmp) ) {
		// tmp is what the old watch_name was 
		// The path to what we're watching has changed in the 
		// config file, we will need to start the new version.
		timeStamp = 0;
	}

	if (tmp) {
		free( tmp );
		tmp = NULL;
	}

	// GT#7103: We need to know this for the obituary message.
	if( log_filename_in_config_file != NULL ) {
		if( log_name ) {
			free( log_name );
		}
		log_name = param(log_filename_in_config_file);
	}
}

// set the deamon readiness state text
void daemon::SetReadyState(const char * state)
{
	// if there is existing state text, and the new text fits just overwrite
	// otherwise free the old text and dup the new text into a new allocation.
	if (ready_state) {
		if (state && strlen(state) <= strlen(ready_state)) {
			strcpy(ready_state, state);
			return;
		}
		free (ready_state);
		ready_state = NULL;
	}
	if (state) {
		ready_state = strdup(state);
	}
}

const char * daemon::Stopping()
{
	if ((stop_state == PEACEFUL) && draining) {
		return "Shutdown,Drain";
	} else if (stop_state >= PEACEFUL && stop_state <= FAST) {
		static const char * const aStops[] = { "Shutdown,Peaceful", "Shutdown,Graceful", "Shutdown,Fast", "Shutdown,Hard" };
		return aStops[stop_state];
	}
	return nullptr;
}


int
daemon::SetupHighAvailability( void )
{
	char		*tmp;
	std::string url;
	std::string	name;

	// Get the URL
	formatstr(name, "HA_%s_LOCK_URL", name_in_config_file );
	tmp = param( name.c_str() );
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
	formatstr( name, "HA_%s_LOCK_HOLD_TIME", name_in_config_file );
	tmp = param( name.c_str( ) );
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
	formatstr( name, "HA_%s_POLL_PERIOD", name_in_config_file );
	tmp = param( name.c_str() );
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
		int status = ha_lock->SetLockParams( url.c_str(),
											 name_in_config_file,
											 poll_period,
											 lock_hold_time,
											 true );
		if ( status ) {
			dprintf( D_ALWAYS, "Failed to change HA lock parameters\n" );
		} else {
			dprintf( D_FULLDEBUG,
					 "Set HA lock for %s; URL='%s' poll=%lds hold=%lds\n",
					 name_in_config_file, url.c_str(), (long)poll_period, 
					 (long)lock_hold_time );
		}
	} else {
		ha_lock = new CondorLock( url.c_str(),
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
					 name_in_config_file, url.c_str(), (long)poll_period, 
					 (long)lock_hold_time );
		}
	}

	// And, if it failed, log it
	if ( ! ha_lock ) {
		dprintf( D_ALWAYS, "HA Lock creation failed for URL '%s' / '%s'\n",
				 url.c_str(), name_in_config_file );
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
daemon::DetachController( void )
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
	controller->DeregisterControllee( this );

	// Done
	return 0;
}

int
daemon::RegisterControllee( class daemon *controllee )
{
	bool found = false;

	if ( num_controllees >= MAX_CONTROLLEES ) {
		return -1;
	}
	for ( int num = 0; num < num_controllees; ++num ) {
		if( strncmp(controllee->name_in_config_file, controllees[num]->name_in_config_file, strlen(controllees[num]->name_in_config_file)) == 0 ) {
			found = true;
			break;
		}
	}
	if ( !found ) {
		controllees[num_controllees++] = controllee;
	}
	return 0;
}


void
daemon::DeregisterControllee( class daemon *controllee )
{
	bool found = false;

	for ( int num = 0; num < num_controllees; ++num ) {
		if( strcmp(controllee->name_in_config_file, controllees[num]->name_in_config_file) == 0 ) {
			controllees[num] = NULL;
			found = true;
		}
		else if ( found ) {
			controllees[num-1] = controllees[num];	
			controllees[num] = NULL;
		}
	}
	if ( found ) {
		num_controllees--;
	}
}


///////////////////////////////////////////////////////////////////////////
//  Daemons Class
///////////////////////////////////////////////////////////////////////////

Daemons::Daemons()
	: m_token_requester(&Daemons::token_request_callback, this)
{
	check_new_exec_tid = -1;
	update_tid = -1;
	preen_tid = -1;
	reaper = NO_R;
	all_daemons_gone_action = MASTER_RESET;
	cmd_after_drain = nullptr;
	immediate_restart = FALSE;
	immediate_restart_master = FALSE;
	stop_other_daemons_when_startds_gone = NONE;
	prevLHF = 0;
	m_retry_start_all_daemons_tid = -1;
	m_deferred_query_ready_tid = -1;
	master = NULL;
}


void
Daemons::RegisterDaemon(class daemon *d)
{
	std::pair<std::map<std::string,class daemon*>::iterator,bool> ret;

	ret = daemon_ptr.insert( std::pair<char*, class daemon*>(d->name_in_config_file, d) );
	if( ret.second == false ) {
		EXCEPT( "Registering daemon %s failed", d->name_in_config_file );
	}
}


int
Daemons::SetupControllers( void )
{
	// Find controlling daemons
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if ( iter->second->SetupController( ) < 0 ) {
			dprintf( D_ALWAYS,
					 "SetupControllers: Setup for daemon %s failed\n",
					 iter->first.c_str() );
			return -1;
		}
	}
	return 0;
}

DeferredQuery::DeferredQuery(int /*cmd*/, Stream * stm, ExprTree * req, time_t expires)
	: stream(stm), requirements(NULL), expire_time(expires)
{
	if (stream) { Sock* sock = (Sock*)stream; sock->incRefCount(); }
	if (req) { requirements = req->Copy(); }
}

DeferredQuery::~DeferredQuery()
{
	if (stream) { Sock* sock = (Sock*)stream; sock->decRefCount(); stream = NULL; }
	if (requirements) { delete requirements; requirements = NULL; }
}


bool Daemons::InitDaemonReadyAd(ClassAd & readyAd, bool include_addrs)
{
	bool all_daemons_alive = true;
	int  num_alive = 0;
	int  num_startup = 0;
	int  num_hung = 0;
	int  num_dead = 0;
	int  num_held = 0;
	int  num_daemons = 0;

	std::map<std::string, class daemon*>::iterator it;
	for (it = daemon_ptr.begin(); it != daemon_ptr.end(); it++ ) {
		class daemon* dmn = it->second;

		std::string attr(dmn->name_in_config_file); attr += "_PID";
		readyAd.Assign(attr, dmn->pid);
		++num_daemons;

		if (include_addrs) {
			const char * addr = nullptr;
			if (dmn->type == DT_MASTER) {
				addr = daemonCore->InfoCommandSinfulStringMyself(false);
			} else if (dmn->pid) {
				addr = daemonCore->InfoCommandSinfulString(dmn->pid);
			}
			if (addr) {
				attr = dmn->name_in_config_file; attr += "_Addr";
				readyAd.Assign(attr, addr);
			}
		}

		if (dmn->ready_state) {
			attr = dmn->name_in_config_file; attr += "_State";
			readyAd.Assign(attr, dmn->ready_state);
		}

		const char * state;
		if (dmn->pid) {
			bool hung = false;
			int num_alive_msgs = 1;
			if (dmn->type != DT_MASTER) {
				num_alive_msgs = daemonCore->Got_Alive_Messages(dmn->pid, hung);
				// treat a 'ready' message as evidence of life
				if (dmn->ready_state && !num_alive_msgs) { num_alive_msgs += 1; }
			}
			if ( ! num_alive_msgs) all_daemons_alive = false;
			if (hung) {
				++num_hung;
				state = "Hung";
			} else if (num_alive_msgs) {
				state = "Alive";
				++num_alive;
			} else {
				state = "Startup";
				++num_startup;
			}
			const char * stopping = dmn->Stopping();
			if (stopping) state = stopping;
		} else {
			bool for_file = false;
			int hold = dmn->OnHold();
			if (hold && (hold != 2)) { // hold value of 2 indicates a startup hold not a "real" hold.
				state = "Hold";
				++num_held;
			} else {
				all_daemons_alive = false;
				if (dmn->WaitingforStartup(for_file)) {
					state = for_file ? "WaitForStartupFile" : "WaitForStartup";
					++num_startup;
				} else if ( ! dmn->startTime) {
					// if not on hold or dead, a startTime of 0 indicates we are in very early init
					state = "Startup";
					++num_startup;
				} else {
					state = "Dead";
					++num_dead;
				}
			}
		}
		readyAd.Assign(dmn->name_in_config_file, state);
	}

	readyAd.Assign("NumDaemons", num_daemons);
	readyAd.Assign("NumAlive", num_alive);
	readyAd.Assign("NumStartup", num_startup);
	readyAd.Assign("NumHung", num_hung);
	readyAd.Assign("NumHold", num_held);
	readyAd.Assign("NumDead", num_dead);
	readyAd.Assign("IsReady", all_daemons_alive && ! (num_hung || num_dead));
	if (cmd_after_drain) {
		readyAd.Insert("PostDrain", cmd_after_drain->Copy());
	}

	return all_daemons_alive;
}

#if 0
bool Daemons::GetDaemonReadyStates(std::string & ready)
{
	bool all_daemons_ready = true;

	std::map<std::string, class daemon*>::iterator it;
	for (it = daemon_ptr.begin(); it != daemon_ptr.end(); it++ ) {
		class daemon* dmn = it->second;
		if (dmn->type == DT_MASTER) continue;

		if ( ! ready.empty()) ready += " ";
		ready += dmn->name_in_config_file;
		ready += ":";
		if (dmn->pid) {
			if (dmn->ready_state) { ready += dmn->ready_state; }
			else {
				bool hung = false;
				if (daemonCore->Got_Alive_Messages(dmn->pid, hung)) {
					ready += hung ? "hung" : "alive";
				} else {
					ready += "startup";
				}
			}
		} else {
			bool for_file = false;
			if (dmn->OnHold()) { ready += "hold"; }
			else if (dmn->WaitingforStartup(for_file)) { ready += "waitForStartup"; if (for_file) ready += "File"; }
			else { ready += "dead"; }
		}

		if ( ! dmn->ready_state || MATCH != strcasecmp(dmn->ready_state, "ready")) {
			all_daemons_ready = false;
		}
	}

	return all_daemons_ready;
}
#endif

// timer callback to handle deferred replys for DC_QUERY_READY command
void
Daemons::DeferredQueryReadyReply( int /* timerID */ )
{
	if (deferred_queries.empty()) {
		dprintf(D_ALWAYS, "WARNING: DeferredQueryReadyReply called with empty queries list\n");
	}

	time_t now = time(NULL);

	// make a reply ad containing daemon ready states
	// we will return this, and may also use it to evaluate ready requirements.
	// the default is_ready value is true when all non-held daemons are alive.
	ClassAd readyAd;
	advertise_shutdown_program(readyAd);
	bool is_ready = InitDaemonReadyAd(readyAd, false);
	dprintf(D_FULLDEBUG, "DeferredQueryReadyReply - %d\n", is_ready);

	// check for expired queries, and those that have requirements satisfied
	// if we find any, send the reply and remove it from the list.
	//
	std::list<DeferredQuery*>::iterator it;
	for (it = deferred_queries.begin(); it != deferred_queries.end(); ) {
		DeferredQuery * query = (*it);
		bool expired = query->expire_time > 0 && now >= query->expire_time;

		// for non-expired queries, that have requirements, check the requirements.
		//
		if ( ! expired && query->requirements) {
			classad::Value result;
			dprintf(D_FULLDEBUG, "DeferredQueryReadyReply - %d evaluating %s\n",
				is_ready, ExprTreeToString(query->requirements));
			if (readyAd.EvaluateExpr(query->requirements, result)) {
				bool bb=false;
				is_ready = result.IsBooleanValueEquiv(bb) && bb;
			}
		}

		// send reply for satisfied requirements, or expired queries
		// then remove it from the list, note that removing also increments the iterator
		if (is_ready || expired) {
			dprintf(D_ALWAYS, "DeferredQueryReadyReply - replying %d%s\n", is_ready, expired ? " (timer expired)" : "");
			query->stream->encode();
			if (!putClassAd(query->stream, readyAd) || !query->stream->end_of_message()) {
				dprintf(D_ALWAYS, "Failed to send reply message to ready query.\n");
			}
			it = deferred_queries.erase(it);
			delete query;
		} else {
			it++;
		}
	}

	// when the deferred query list is empty, cancel the timer
	if (m_deferred_query_ready_tid >= 0 && deferred_queries.empty()) {
		dprintf(D_FULLDEBUG, "DeferredQueryReadyReply - no deferred queries, cancelling timer %d.\n", m_deferred_query_ready_tid);
		daemonCore->Cancel_Timer(m_deferred_query_ready_tid);
		m_deferred_query_ready_tid = -1;
	}
}

// handle the DC_QUERY_READY command
int
Daemons::QueryReady(ClassAd & cmdAd, Stream* stm)
{
	time_t wait_time = 0;
	cmdAd.LookupInteger("WaitTimeout", wait_time);
	bool want_addrs = false;
	cmdAd.LookupBool("WantAddrs", want_addrs);
	ExprTree * requirements = cmdAd.Lookup("ReadyRequirements");

	// make a reply ad containing daemon ready states
	// we will return this, and may also use it to evaluate ready requirements.
	// the default is_ready value is true when all non-held daemons are alive.
	ClassAd readyAd;
	advertise_shutdown_program(readyAd);
	if (MasterShuttingDown) {
		const char * action = "Reset";
		if (all_daemons_gone_action == MASTER_EXIT) action = "Exit";
		else if (all_daemons_gone_action == MASTER_RESTART) action = "Restart";
		readyAd.Assign("InShutdownMode", action);
	}
	bool is_ready = InitDaemonReadyAd(readyAd, want_addrs);

	// if there is a requirements expression, check the requirements and use
	// it to override the is_ready flag.  At this time we also validate
	// the requirements expression and return immediately if it is invalid
	if (requirements) {
		classad::Value result;
		dprintf(D_FULLDEBUG, "QueryReady - %d evaluating %s\n", is_ready, ExprTreeToString(requirements));
		if ( ! readyAd.EvaluateExpr(requirements, result)) {
			is_ready = true; // force an immediate reply
			readyAd.Assign("Error", "invalid ReadyRequirements expression");
		} else {
			bool bb = false;
			if ( ! result.IsBooleanValueEquiv(bb)) {
				is_ready = true; // force an immediate reply
				readyAd.Assign("Error", "ReadyRequirements does not evaluate to a bool or equivalent");
			} else {
				is_ready = bb;
			}
		}
	}

	// We send a reply now if ready or if the timeout value is 0 or negative
	// otherwise we add the query to the deferred queries list and start a timer
	// the timer runs fairly quickly (1 second intervals) because we use this
	// query mostly in the test suite where we want timely answers.
	if (stm && wait_time > 0 && ! is_ready) {
		time_t expire_time = time(NULL) + wait_time -1;
		DeferredQuery * query = new DeferredQuery(DC_QUERY_READY, stm, requirements, expire_time);
		if ( ! query) {
			return FALSE;
		}

		dprintf(D_FULLDEBUG, "QueryReady - adding query to deferred queries list\n");
		deferred_queries.push_back(query);

		// start a timer if one is not already running
		if (m_deferred_query_ready_tid < 0) {
			m_deferred_query_ready_tid = daemonCore->Register_Timer(1, 1,
				(TimerHandlercpp)&Daemons::DeferredQueryReadyReply,
				"Daemons::DeferredQueryReadyReply", this);
			if (m_deferred_query_ready_tid < 0) {
				return FALSE;
			}
			//dprintf(D_FULLDEBUG, "QueryReady - registered timer %d\n", m_deferred_query_ready_tid);
		}
		//dprintf(D_FULLDEBUG, "QueryReady - returning KEEP_STREAM\n");
		return KEEP_STREAM;

	} else if (stm) {
		// send a reply now
		dprintf(D_COMMAND, "QueryReady - replying %d\n", is_ready);
		stm->encode();
		if (!putClassAd(stm, readyAd) || !stm->end_of_message())
		{
			dprintf(D_ALWAYS, "Failed to send reply message to ready query.\n");
			return FALSE;
		}
		return TRUE;
	}
	return TRUE;
}


void
Daemons::InitParams()
{
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		iter->second->InitParams();
	}

	// As long as we change the daemon objects before anyone calls
	// RealStart() on them, we should be good.
	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( ! iter->second->isDC ) {
			std::map<std::string, class daemon*>::iterator jter;
			for( jter = daemon_ptr.begin(); jter != daemon_ptr.end(); ++jter ) {
				if( jter->second->isDC &&
				  (strcasecmp( jter->second->name_in_config_file, iter->second->name_in_config_file ) != 0) &&
				  // In practice, all we'll ever see is
				  // 	DAEMON_NAME = $(DC_DAEMON)
				  // so we don't need to canonicalize the filename.  Arguably,
				  // this allows users force a DC daemon to be treated as
				  // non-DC (not that you'd ever want that to happen) by
				  // changing "dirname/" to "dirname/../dirname/" somewhere
				  // in the path, and flexibility is good.
				  (strcmp( jter->second->process_name, iter->second->process_name ) == 0) ) {
					dprintf( D_ALWAYS, "Declaring that %s, "
						"since it shares %s with %s, "
						"is also a DaemonCore daemon.\n",
						iter->second->name_in_config_file,
						iter->second->process_name,
						jter->second->name_in_config_file );
					// We'll make sure this is launch with -localname elsewhere
					// in the master by checking if a daemon we're starting is
					// both a DC daemon and not in the list of DC daemons.
					iter->second->isDC = true;
					break;
				}
			}
		}
	}
}


class daemon *
Daemons::FindDaemon( const char *name )
{
	std::map<std::string, class daemon*>::iterator iter;

	iter = daemon_ptr.find(name);
	if( iter != daemon_ptr.end() ) {
		return iter->second;
	}
	else {
		return NULL;
	}
}


void
Daemons::CheckForNewExecutable( int /* timerID */ )
{
	int found_new = FALSE;
	std::map<std::string, class daemon*>::iterator iter;

	dprintf(D_FULLDEBUG, "enter Daemons::CheckForNewExecutable\n");

	if( master->newExec ) {
			// We already noticed the master has a new binary, so we
			// already started to restart it.  There's nothing else to
			// do here.
		return;
	}
	time_t tspOld = master->timeStamp;
	if( NewExecutable( master->watch_name, &master->timeStamp ) ) {
		master->newExec = TRUE;
		if (NONE == new_bin_restart_mode) {
			dprintf( D_ALWAYS,"%s was modified (%lld != %lld), but master restart mode is NEVER\n",
					 master->watch_name, (long long)master->timeStamp, (long long)tspOld);
			//don't want to do this in case the user later reconfigs the restart mode.
			//CancelNewExecTimer();
		} else {
			dprintf( D_ALWAYS,"%s was modified (%lld != %lld), restarting %s %s.\n", 
					 master->watch_name,
					 (long long)master->timeStamp, (long long)tspOld,
					 master->process_name,
					 (new_bin_restart_mode == PEACEFUL) ? "Peacefully" : "Gracefully");
			// Begin the master restart procedure.
			if (PEACEFUL == new_bin_restart_mode) {
				DoPeacefulShutdown(5, &Daemons::RestartMasterPeaceful, "RestartMasterPeaceful");
			} else {
				RestartMaster();
			}
		}
		return;
	}

	// don't even bother to look at other binaries if the new bin restart
	// mode is GRACEFUL/PEACEFUL or NONE we do this because for NONE we don't
	// want to restart anyway, and for GRACEFUL/PEACEFUL, we don't want to
	// restart in arbitrary order.  it would be better to detect any binary
	// changes, wait for things to settle, and then restart everthing that
	// changed in the correct order. Well save that for a future change
	//
	if (PEACEFUL == new_bin_restart_mode || 
		GRACEFUL == new_bin_restart_mode ||
		NONE == new_bin_restart_mode)
		return;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->runs_here && !iter->second->newExec 
			&& !iter->second->OnHold()
			&& !iter->second->OnlyStopWhenMasterStops() )
		{
			if( NewExecutable( iter->second->watch_name,
						&iter->second->timeStamp ) ) {
				found_new = TRUE;
				iter->second->newExec = TRUE;
				if( immediate_restart ) {
						// If we want to avoid the new_binary_delay,
						// we can just set the newExec flag to false,
						// reset restarts to 0, and kill the daemon.
						// When it gets restarted, the new binary will
						// be used, but we won't think it's a new
						// binary, so we won't use the new_bin_delay.
					iter->second->newExec = FALSE;
					iter->second->restarts = 0;
				}
				if( iter->second->pid ) {
					dprintf( D_ALWAYS,"%s was modified, killing %s.\n", 
							 iter->second->watch_name,
							 iter->second->process_name );
					iter->second->Stop();
				} else {
					if( immediate_restart ) {
							// This daemon isn't running now, but
							// there's a new binary.  Cancel the
							// current start timer and restart it
							// now. 
						iter->second->CancelAllTimers();
						iter->second->Restart();
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
			DaemonStartFastPoll ? 0 : 1,
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
Daemons::RetryStartAllDaemons( int /* timerID */ )
{
	m_retry_start_all_daemons_tid = -1;
	StartAllDaemons();
}

void
Daemons::StartAllDaemons()
{
	class daemon *daemon;

	for (const auto &name: ordered_daemon_names) {
		daemon = FindDaemon(name.c_str());
		if( daemon == NULL ) {
			EXCEPT("Unable to start daemon %s", name.c_str());
		}
		if( daemon->pid > 0 ) {
			if( daemon->WaitBeforeStartingOtherDaemons(false) ) {
				ScheduleRetryStartAllDaemons();
				return;
			}

				// the daemon is already started
			continue;
		} 

		if( StartDaemonHere(daemon) == FALSE ) continue;

		if( daemon->WaitBeforeStartingOtherDaemons(true) ) {
			ScheduleRetryStartAllDaemons();
			return;
		}
	}

	if (m_retry_start_all_daemons_tid != -1) {
		dprintf(D_ALWAYS, "Daemons::StartAllDaemons there were some wait before daemons\n");
	} else {
		dprintf(D_ALWAYS, "Daemons::StartAllDaemons all daemons were started\n");
	#ifndef WIN32
		dc_release_background_parent(0);
	#endif
	}
}


int
Daemons::StartDaemonHere(class daemon *daemon)
{
	if( ! daemon->runs_here ) return FALSE;
	daemon->Hold( FALSE );
	daemon->Start();
	return TRUE;
}


void
Daemons::StopAllDaemons() 
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();

	int any_running = 0;
	int startds_running = 0;

	// first check to see if any startd's are running, if there are, request
	// that they exit
	std::map<std::string, class daemon*>::iterator iter;
	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid && iter->second->runs_here &&
			!iter->second->OnlyStopWhenMasterStops() )
		{
			if (iter->second->type == DT_STARTD) {
				iter->second->Stop();
				++startds_running;
			}
			++any_running;
		}
	}

	// if there are daemons running, but no startd's running.
	// request that the daemons exit.
	//
	if (startds_running) {
		// tell the all-reaper (actually the AllStartdsGone method) to stop
		// the remaining daemons instead of actually stopping them here.
		stop_other_daemons_when_startds_gone = GRACEFUL;
	} else if (any_running) {
		for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
			if( iter->second->pid && iter->second->runs_here &&
				!iter->second->OnlyStopWhenMasterStops() )
			{
				iter->second->Stop();
			}
		}
	}

	if( !any_running ) {
		AllDaemonsGone();
	}
}

// called when a reconfig removes a daemon from the DAEMON_LIST
// We want to move it from the daemons collection
// to the removed_daemons collection until it can be reaped
void
Daemons::RemoveDaemon(const char* name )
{
	std::map<std::string, class daemon*>::iterator iter;

	iter = daemon_ptr.find( name );
	if( iter != daemon_ptr.end() ) {
		auto d = iter->second;
		// take the daemon our of our live daemon collection
		// and then either delete it or add it to the list of removed daemons
		daemon_ptr.erase( iter );
		if (d->pid > 0) {
			removed_daemons[d->pid] = d;
			d->Stop();
		} else {
			d->CancelAllTimers();
			d->DetachController();
			delete d;
		}
	}
}


void
Daemons::StopFastAllDaemons()
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();
	int running = 0;
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid && iter->second->runs_here &&
			!iter->second->OnlyStopWhenMasterStops() )
		{
			iter->second->StopFast();
			running++;
		}
	}
	if( !running ) {
		AllDaemonsGone();
	}
}


int  Daemons::SendSetPeacefulShutdown(class daemon* target, int timeout)
{
	// only STARTDs and SCHEDDs recognise this command.
	if (target->type != DT_STARTD && target->type != DT_SCHEDD)
		return 0;

	// fire and forget, if the command fails, we just end up with a graceful shutdown
	// rather than a peaceful one.
	classy_counted_ptr<Daemon> d = new Daemon(target->type);
	classy_counted_ptr<DCCommandOnlyMsg>  msg = new DCCommandOnlyMsg(DC_SET_PEACEFUL_SHUTDOWN);
	msg->setSuccessDebugLevel(D_ALWAYS);
	msg->setStreamType(Stream::reli_sock);
	msg->setTimeout(timeout);
	d->sendMsg(msg.get());
	return 0;
}

int
Daemons::SetPeacefulShutdown(int timeout)
{
	int messages = 0;

	// tell STARTD's and SCHEDD's that this is to be a peaceful shutdown.
	std::map<std::string, class daemon*>::iterator iter;
	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid && iter->second->runs_here &&
			!iter->second->OnlyStopWhenMasterStops() )
		{
			if (iter->second->type == DT_STARTD || iter->second->type == DT_SCHEDD) {
				SendSetPeacefulShutdown(iter->second, timeout);
				++messages;
			}
		}
	}

	return messages;
}

void
Daemons::DoPeacefulShutdown(
	int             timeout,
	void (Daemons::*pfn)(int),
	const char *    lbl)
{
	int messages = SetPeacefulShutdown(timeout);

	// if we sent any messages, set a timer to do the StopPeaceful call.
	// to give the messages a chance to arrive.
	bool fTimer = false;
	if (messages > 0) {
		int tid = daemonCore->Register_Timer(timeout+1, 0, (TimerHandlercpp)pfn, lbl, this);
		if (tid == -1) {
			dprintf( D_ALWAYS, "ERROR! Can't register DaemonCore timer!\n" );
		} else {
			fTimer = true;
		}
	}

	// if the shutdown/restart command isn't going to happen on a timer,
	// then do it now.
	if ( ! fTimer) {
		((this)->*(pfn))(-1);
	}
}

void
Daemons::StopPeacefulAllDaemons()
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper(false); // false, because we don't assume that there are any startds
	int any_running = 0;
	int startds_running = 0;

	// first check to see if any startd's are running, if there are, request
	// that they peacefully exit.
	std::map<std::string, class daemon*>::iterator iter;
	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid && iter->second->runs_here &&
			!iter->second->OnlyStopWhenMasterStops() )
		{
			if (iter->second->type == DT_STARTD) {
				iter->second->StopPeaceful();
				++startds_running;
			}
			++any_running;
		}
	}

	// if there are daemons running, but no startd's running.
	// request that the daemons peacefully exit.
	//
	if (startds_running) {
		// tell the all-reaper (actually the AllStartdsGone method) to stop-peaceful
		// the remaining daemons instead of actually stopping them here.
		stop_other_daemons_when_startds_gone = PEACEFUL;
	} else if (any_running) {
		for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
			if( iter->second->pid && iter->second->runs_here &&
				!iter->second->OnlyStopWhenMasterStops() )
			{
				iter->second->StopPeaceful();
			}
		}
	}

	// if there were no daemons running, do the AllDaemonsGone dance.
	// if there were startds running
	//
	if( !any_running ) {
		AllDaemonsGone();
	}
}


void
Daemons::HardKillAllDaemons()
{
	CancelRetryStartAllDaemons();
	daemons.SetAllReaper();
	int running = 0;
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid && iter->second->runs_here &&
			!iter->second->OnlyStopWhenMasterStops() )
		{
			iter->second->HardKill();
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
	std::map<std::string, class daemon*>::iterator iter;
	dprintf( D_ALWAYS, "Reconfiguring all managed daemons.\n" );

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->runs_here ) {
			iter->second->Reconfig();
		}
	}
}


void
Daemons::InitMaster()
{
		// initialize master data structure
	master = FindDaemon("MASTER");
	if ( master == NULL ) {
		EXCEPT("InitMaster: MASTER not Specified");
	}
	master->timeStamp = GetTimeStamp(master->watch_name);
	master->startTime = time(0);
	master->pid = daemonCore->getpid();

	// to aid in debugging unwanted daylight savings time restarts, log the restart mode and timestamp.
	if (new_bin_restart_mode != NONE) {
		dprintf(D_ALWAYS, "Master restart (%s) is watching %s (mtime:%lld)\n", 
				StopStateToString(new_bin_restart_mode),
				master->watch_name, (long long)master->timeStamp);
	}
}


void
Daemons::RestartMaster()
{
	MasterShuttingDown = TRUE;
	immediate_restart_master = immediate_restart;
	all_daemons_gone_action = MASTER_RESTART;
	StartDaemons = FALSE;
	StopAllDaemons();
}

void
Daemons::RestartMasterPeaceful( int /* timerID */ )
{
	MasterShuttingDown = TRUE;
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

		// Release file lock on the log file.
	if ( MasterLock->release() == FALSE ) {
		dprintf( D_ALWAYS,
				 "Can't remove lock on \"%s\"\n",master->log_name);
		EXCEPT( "file_lock(%d)", MasterLockFD );
	}
	(void)close( MasterLockFD );

		// Now set close-on-exec on all fds so our new invocation of
		// condor_master does not inherit them.
		// Note: Not needed (or wanted) on Win32, as CEDAR creates 
		//		Winsock sockets as non-inheritable by default.
		// Also not wanted for stderr, since we might be logging
		// to that.  No real need for stdin or stdout either.
	int	max_fds = largestOpenFD();
	for (int i=3; i < max_fds; i++) {
		int flag = fcntl(i,F_GETFD,0);
		if( flag != -1 ) {
			(void) fcntl(i,F_SETFD,flag | 1);
		}
	}
#endif

		// Get rid of the CONDOR_INHERIT env variable.  If it is there,
		// when the master restarts daemon core will think it's parent
		// is a daemon core process.  but, its parent is gone ( we are doing
		// an exec, so we disappear), thus we must blank out the 
		// CONDOR_INHERIT env variable.
	UnsetEnv(ENV_CONDOR_INHERIT);
}

void
Daemons::ExecMaster()
{
	int i=0,j;
	char **argv = (char **)malloc((condor_main_argc+2)*sizeof(char *));

	ASSERT( argv != NULL && condor_main_argc > 0 );
	argv[i++] = condor_main_argv[0];

		// insert "-f" argument so that new master does not fork
	argv[i++] = const_cast<char *>("-f");
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
			std::string runfor_str;
			formatstr(runfor_str, "%d",runfor);
			argv[i++] = strdup(runfor_str.c_str());
		}
	}
	argv[i++] = NULL;

	(void)execv(master->process_name, argv);

	free(argv);
}

// Function that actually does the restart of the master.
void
Daemons::FinalRestartMaster( int /* timerID */ )
{
	int i;
	const condor_utils::SystemdManager & sd = condor_utils::SystemdManager::GetInstance();

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
			std::string command;
			formatstr(command, "net stop %s & net start %s", 
				_condor_myServiceName, _condor_myServiceName);
			dprintf( D_ALWAYS, "Doing exec( \"%s /Q /C %s\" )\n", 
				 systemshell,command.c_str());
			(void)execl(systemshell, "/Q", "/C",
				command.c_str(), 0);
#endif
		} else if ( !sd.PrepareForExec() ) {
			dprintf( D_ALWAYS, "Systemd services in use, exiting to be restarted by systemd\n" );
			master_exit( 1 );
		} else {
			dprintf( D_ALWAYS, "Doing exec( \"%s\" )\n", 
				 master->process_name);

				// It is important to switch to ROOT_PRIV, in case we are
				// executing a master wrapper script that expects to be
				// root rather than effective uid condor.
			priv_state saved_priv = set_priv(PRIV_ROOT);

			ExecMaster();

			set_priv(saved_priv);
		}
		master->restarts++;
		if ( NT_ServiceFlag == TRUE && master->restarts > 7 ) {
			dprintf(D_ALWAYS,"Unable to restart Condor service, aborting.\n");
			master_exit(1);
		}
		i = master->NextStart();
		dprintf( D_ALWAYS, 
				 "Cannot execute condor_master (errno=%d), will try again in %d seconds\n", 
				 errno, i );
		sleep(i);
	}
}


const char* Daemons::DaemonLog( int pid )
{
	std::map<std::string, class daemon*>::iterator iter;

	// be careful : a pointer to data in this class is returned
	// posibility of getting tampered
	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if ( iter->second->pid == pid )
			return (iter->second->log_name);
	}
	return "Unknown Program!!!";
}


// This function returns the number of active child processes with the given daemon type
// currently being supervised by the master.
int Daemons::ChildrenOfType(daemon_t type, std::vector<std::string> *names /*=nullptr*/)
{
	int result = 0;
	for (auto iter = daemon_ptr.begin(); iter != daemon_ptr.end(); ++iter) {
		if( iter->second->runs_here && iter->second->pid
			&& !iter->second->OnlyStopWhenMasterStops()
			&& (type == DT_ANY || type == iter->second->type)) {
			if (names) { names->emplace_back(iter->second->name_in_config_file); }
			result++;
		}
	}
	dprintf(D_FULLDEBUG,"ChildrenOfType(%ld) returning %d\n",type,result);
	return result;
}

int
Daemons::ReapPreen(int /*pid*/, int status)
{
	if (WIFSIGNALED(status)) {
		dprintf(D_ALWAYS, "Preen (pid %d) died due to %s\n", preen_pid,
			daemonCore->GetExceptionString(status));
	} else {
		dprintf(D_ALWAYS, "Preen (pid %d) exited with status %d\n",
			preen_pid, WEXITSTATUS(status));
	}
	preen_pid = -1;
	return TRUE;
}

int
Daemons::AllReaper(int pid, int status)
{
#if 1
	if (pid == preen_pid) {
		return ReapPreen(pid, status);
	}

	// if the pid belongs to a daemon object we have already removed we can now forget it
	auto removed_it = removed_daemons.find(pid);
	if (removed_it != removed_daemons.end()) {
		auto d = removed_it->second;
		removed_daemons.erase(removed_it);
		d->Exited(status);
		delete d;
	}

	// we need a count of the daemons that we are still waiting to reap
	// and we need to include the daemons that have been removed from the daemon
	// list but have not yet been reaped.
	int daemons = 0, startds = 0;
	for (auto & it : removed_daemons) {
		const auto d = it.second;
		if (d->runs_here && d->pid && !d->OnlyStopWhenMasterStops()) {
			++daemons;
			if (d->type == DT_STARTD) ++startds;
		}
	}
#else
	std::map<std::string, class daemon*>::iterator iter;
	std::map<int, class daemon*>::iterator valid_iter;
		// find out which daemon died
	valid_iter = exit_allowed.find(pid);
	if( valid_iter != exit_allowed.end() ) {
		valid_iter->second->Exited( status );
 		delete valid_iter->second;
		exit_allowed.erase( valid_iter );
		if( NumberOfChildren() == 0 ) {
			AllDaemonsGone();
		} else if ( ChildrenOfType(DT_STARTD) == 0 ) {
			AllStartdsGone();
		}
		return TRUE;
	} else {
		dprintf( D_ALWAYS, "AllReaper unexpectedly called on pid %i, status %i.\n", pid, status);
	}
#endif

	// if we have not  yet recorded this pid as reaped, do that now
	// at this same time count the daemons that are still alive and in the daemon list
	for (const auto& it : daemon_ptr) {
		auto *d = it.second;
		if (pid == d->pid) { d->Exited(status); }
		else if (d->runs_here && d->pid && !d->OnlyStopWhenMasterStops()) {
			++daemons;
			if (d->type == DT_STARTD) ++startds;
		}
	}

	// when we get to 0 startd's and/or 0 children, we have stuff to do
	if ( ! daemons) {
		AllDaemonsGone();
	} else if ( ! startds) {
		AllStartdsGone();
	}

	return TRUE;
}


int
Daemons::DefaultReaper(int pid, int status)
{
#if 1
	if (pid == preen_pid) {
		return ReapPreen(pid, status);
	}

	// if the pid belongs to a daemon object we have already removed we can now forget it
	auto removed_it = removed_daemons.find(pid);
	if (removed_it != removed_daemons.end()) {
		auto d = removed_it->second;
		removed_daemons.erase(removed_it);
		d->Exited(status);
		delete d;
	}

#else
	std::map<std::string, class daemon*>::iterator iter;
	std::map<int, class daemon*>::iterator valid_iter;
	valid_iter = exit_allowed.find(pid);
	if( valid_iter != exit_allowed.end() ) {
		valid_iter->second->Exited( status );
 		delete valid_iter->second;
		exit_allowed.erase(valid_iter);
		return TRUE;
	} else if ( pid == preen_pid ) {
		if ( WIFSIGNALED( status ) ) {
			dprintf( D_ALWAYS, "Preen (pid %d) died due to %s\n", preen_pid,
					 daemonCore->GetExceptionString( status ) );
		} else {
			dprintf( D_ALWAYS, "Preen (pid %d) exited with status %d\n",
					 preen_pid, WEXITSTATUS( status ) );
		}
		preen_pid = -1;
		return TRUE;
	} else {
		dprintf( D_ALWAYS, "DefaultReaper unexpectedly called on pid %i, status %i.\n", pid, status);
	}
#endif

	for(auto iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( pid == iter->second->pid ) {
			auto d = iter->second;
			bool expected_exit = d->Exited(status);
			if (PublishObituaries) { d->Obituary(status); }
			d->Restart(expected_exit);
			break;
		}
	}

	// if we have a post-drain command to run, check to see if we have drained all of the startds
	// TODO: handle post drain shutdown command
	// TODO: handle cancel of post drain command
	if (cmd_after_drain) {
		int startds = 0;
		for (auto it : removed_daemons) { if (it.second->type == DT_STARTD) ++startds; }
		for (auto it : daemon_ptr) { if (it.second->pid && (it.second->type == DT_STARTD)) ++startds; }
		dprintf(D_FULLDEBUG, "Reaper has PostDrainCmd, and %d living STARTDS\n", startds);
		if ( ! startds) {
			ClassAd * cmdAd = cmd_after_drain;
			cmd_after_drain = nullptr;
			do_flexible_daemons_command(DAEMONS_OFF_FLEX, *cmdAd);
			delete cmdAd;
		}
	}

	return TRUE;
}



// This function will set the reaper to one that calls
// AllDaemonsGone() when all the daemons have exited.
void
Daemons::SetAllReaper(bool fStartdsFirst)
{
	if( reaper == ALL_R ) {
			// All reaper is already set.
		return;
	}
	daemonCore->Reset_Reaper( 1, "All Daemon Reaper",
							  (ReaperHandlercpp)&Daemons::AllReaper,
							  "Daemons::AllReaper()",this);
	reaper = ALL_R;
	stop_other_daemons_when_startds_gone = fStartdsFirst ? GRACEFUL : NONE;
	// setting the all reaper cancels a post-drain command
	if (cmd_after_drain) {
		std::string buf;
		dprintf(D_STATUS, "Discarding post-drain command due to later shutdown command:\n%s\n",
			formatAd(buf, *cmd_after_drain, "\t"));
		delete cmd_after_drain; cmd_after_drain = nullptr;
	}
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

/*static*/ void
Daemons::ProcdStopped(void* me, int pid, int status)
{
	dprintf(D_FULLDEBUG, "ProcD (pid %d) is gone. status=%d\n", pid, status);
	((Daemons*)me)->AllDaemonsGone();
}

bool
Daemons::StopDaemonsBeforeMasterStops()
{
		// now shut down all the daemons that should only stop right
		// before the master stops
	int running = 0;
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid && iter->second->runs_here )
		{
			iter->second->Stop();
			running++;
		}
	}

	// If we didn't stop any daemons (I'm looking at you shared-port)
	// then we can now stop the procd if it is running
	if ( ! running) {
		if (daemonCore && daemonCore->Proc_Family_QuitProcd(ProcdStopped, this)) {
			++running;
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
Daemons::AllStartdsGone()
{
	StopStateT stop = stop_other_daemons_when_startds_gone;
	stop_other_daemons_when_startds_gone = NONE;
	if ( GRACEFUL == stop || PEACEFUL == stop ) {
		dprintf( D_ALWAYS, "All STARTDs are gone.  Stopping other daemons %s\n",
				(GRACEFUL == stop) ? "Gracefully" : "Peacefully");

		std::map<std::string, class daemon*>::iterator iter;
		for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
			if( iter->second->pid && iter->second->runs_here &&
				!iter->second->OnlyStopWhenMasterStops() )
			{
				if (PEACEFUL == stop)
					iter->second->StopPeaceful();
				else
					iter->second->Stop();
			}
		}
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
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->runs_here || iter->second == master ) {
			snprintf( buf, sizeof(buf), "%s_Timestamp",
					 iter->second->name_in_config_file );
			ca->Assign( buf, (long)iter->second->timeStamp );
			if( iter->second->pid ) {
				snprintf( buf, sizeof(buf), "%s_StartTime",
						 iter->second->name_in_config_file );
				ca->Assign( buf, (long)iter->second->startTime );
			} else {
					// No pid, but daemon's supposed to be running.
				snprintf( buf, sizeof(buf), "%s_StartTime",
						 iter->second->name_in_config_file );
				ca->Assign( buf, 0 );
			}
		}
	}

		// CRUFT
	ad->Assign(ATTR_MASTER_IP_ADDR, daemonCore->InfoCommandSinfulString());

		// Initialize all the DaemonCore-provided attributes
	daemonCore->publish( ad ); 	
}


void
Daemons::UpdateCollector( int /* timerID */ )
{
	// If we are shutting down, we've already send our invalidation
	// and we shouldn't further update and un-invalidate or worse,
	// try to update a collector in a personal condor we just killed.

	if (MasterShuttingDown) {
		return;
	}

	dprintf(D_FULLDEBUG, "enter Daemons::UpdateCollector\n");

	Update(ad);
    daemonCore->publish(ad);
    daemonCore->dc_stats.Publish(*ad);
    daemonCore->monitor_data.ExportData(ad);
	daemonCore->sendUpdates(UPDATE_MASTER_AD, ad, NULL, true, &m_token_requester,
		DCTokenRequester::default_identity, "ADVERTISE_MASTER");

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	MasterPluginManager::Update(ad);
#endif

		// Reset the timer so we don't do another period update until
	if( update_tid != -1 ) {
		daemonCore->Reset_Timer( update_tid, update_interval, update_interval );
	}

	dprintf( D_FULLDEBUG, "exit Daemons::UpdateCollector\n" );
}


class daemon*
Daemons::FindDaemon( daemon_t dt )
{
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->type == dt ) {
			return iter->second;
		}
	}
	return NULL;
}

class daemon*
Daemons::FindDaemonByPID( int pid )
{
	std::map<std::string, class daemon*>::iterator iter;

	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		if( iter->second->pid == pid ) {
			return iter->second;
		}
	}
	return NULL;
}

void
Daemons::CancelRestartTimers( void )
{
	std::map<std::string, class daemon*>::iterator iter;

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
	for( iter = daemon_ptr.begin(); iter != daemon_ptr.end(); iter++ ) {
		iter->second->CancelRestartTimers();
	}
}

void
Daemons::token_request_callback(bool success, void *miscdata)
{
	auto self = reinterpret_cast<Daemons *>(miscdata);
		// In the successful case, instantly re-fire the timer
		// that will send an update to the collector.
	if (success && (self->update_tid != -1)) {
		daemonCore->Reset_Timer( self->update_tid, 0,
		update_interval );
}
}
