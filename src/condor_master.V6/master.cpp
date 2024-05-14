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
#include "condor_string.h"
#include "basename.h"
#include "master.h"
#include "subsystem_info.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "directory.h"
#include "exit.h"
#include "string_list.h"
#include "get_daemon_name.h"
#include "daemon_types.h"
#include "daemon_list.h"
#include "strupr.h"
#include "condor_environ.h"
#include "store_cred.h"
#include "setenv.h"
#include "file_lock.h"
#include "shared_port_server.h"
#include "shared_port_endpoint.h"
#include "credmon_interface.h"
#include "../condor_sysapi/sysapi.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#include "MasterPlugin.h"
#if defined(WIN32)
extern int load_master_mgmt(void);
#endif
#endif

#include "systemd_manager.h"

#ifdef WIN32

#include "firewall.WINDOWS.h"

extern DWORD start_as_service();
extern void terminate(DWORD);
#endif

#ifdef LINUX
#include <sys/prctl.h>
#endif

// local function prototypes
void	init_params();
void	init_daemon_list();
void	init_classad();
void	init_firewall_exceptions();
void	lock_or_except(const char * );
time_t 	GetTimeStamp(char* file);
int 	NewExecutable(char* file, time_t* tsp);
void	RestartMaster();
void	run_preen(int tid);	 // timer handler
int		run_preen_now(); // actually start preen if it is not already running.
void	usage(const char* );
void	main_shutdown_graceful();
void	main_shutdown_normal(); // do graceful or peaceful depending on daemonCore state
void	main_shutdown_fast();
void	invalidate_ads();
void	main_config();
int	admin_command_handler(int, Stream *);
int	ready_command_handler(int, Stream *);
int	handle_subsys_command(int, Stream *);
int	handle_flexible_daemons_command(int, Stream *);
int     handle_shutdown_program( int cmd, Stream* stream );
static int set_shutdown_program(char * program, const char * tag);
void	time_skip_handler(void * /*data*/, int delta);

extern "C" int	DoCleanup(int,int,const char*);

// Global variables
ClassAd	*ad = NULL;				// ClassAd to send to collector
int		MasterLockFD;
int		update_interval;
int		check_new_exec_interval;
int		preen_interval;
int		preen_pid = -1;
int		new_bin_delay;
StopStateT new_bin_restart_mode = GRACEFUL;
char	*MasterName = NULL;
char	*shutdown_program = NULL;
std::string shutdown_program_tag;

int		master_backoff_constant = 9;
int		master_backoff_ceiling = 3600;
float	master_backoff_factor = 2.0;		// exponential factor
int		master_recover_time = 300;			// recover factor

bool	 DaemonStartFastPoll = true;

char	*FS_Preen = NULL;
int		NT_ServiceFlag = FALSE;		// TRUE if running on NT as an NT Service

int		shutdown_graceful_timeout;
int		shutdown_fast_timeout;

int		PublishObituaries;
int		Lines;
int		AllowAdminCommands = FALSE;
int		StartDaemons = TRUE;
int		GotDaemonsOff = FALSE;
int		MasterShuttingDown = FALSE;
static int dummyGlobal;

// daemons in this list are used when DAEMON_LIST is not configured
// all will added to the list of daemons that condor_on can use
// auto_start daemons will be started automatically when the master starts up
// this list must contain only DC daemons
static const struct {
	const char * name;
	bool auto_start;           // start automatically on startup
	bool only_if_shared_port;  // start automatically only if shared port is enabled
} default_daemon_list[] = {
	{ "MASTER",       true , false},
	{ "SHARED_PORT",  true,  true},
	{ "COLLECTOR",    false, false},
	{ "NEGOTIATOR",   false, false},
	{ "STARTD",       false, false},
	{ "SCHEDD",       false, false},
	{ NULL, false, false}, // mark of list record
};

// NOTE: When adding something here, also add it to the various condor_config
// examples in src/condor_examples
char	default_dc_daemon_list[] =
"MASTER, STARTD, SCHEDD, KBDD, COLLECTOR, NEGOTIATOR, EVENTD, "
"VIEW_SERVER, CONDOR_VIEW, VIEW_COLLECTOR, CREDD, HAD, "
"REPLICATION, JOB_ROUTER, ROOSTER, SHARED_PORT, "
"DEFRAG, GANGLIAD, ANNEXD";

// create an object of class daemons.
class Daemons daemons;

class SystemdNotifier : public Service {

public:
	SystemdNotifier() : m_watchdog_timer(-1) {}

	void
	config()
	{
		const condor_utils::SystemdManager & sd = condor_utils::SystemdManager::GetInstance();
		int watchdog_secs = sd.GetWatchdogUsecs();
		if ( watchdog_secs > 0 ) {
			watchdog_secs = watchdog_secs / 1e6 / 3;
			if (watchdog_secs <= 0) { watchdog_secs = 1; }
			if (watchdog_secs > 20) { watchdog_secs = 10; }
			Timeslice ts;
			ts.setDefaultInterval(watchdog_secs);
			m_watchdog_timer = daemonCore->Register_Timer(ts,
					static_cast<TimerHandlercpp>(&SystemdNotifier::status_handler),
					"systemd status updater", this);
			if (m_watchdog_timer < 0) {
				dprintf(D_ALWAYS, "Failed to register systemd update timer.\n");
			} else {
				dprintf(D_FULLDEBUG, "Set systemd to be notified once every %d seconds.\n", watchdog_secs);
			}
		} else {
			dprintf(D_FULLDEBUG, "Not setting systemd watchdog timer\n");
		}
	}

	void status_handler(int /* dummy */)
	{
		// assume success
		const char * status = "All daemons are responding";
		const char * format_string = "READY=1\nSTATUS=%s\nWATCHDOG=1";

		class daemon *daemon;

		// check for missing daemons
		auto ordered_it = daemons.ordered_daemon_names.begin();
		bool missing_daemons = false;
		while (ordered_it != daemons.ordered_daemon_names.end()) {
			const char *name = (*ordered_it).c_str();
			daemon = daemons.FindDaemon(name);
			if (!daemon->pid && !daemon->OnHold()) {
				missing_daemons = true;
				break;
			}
			ordered_it++;
		}

		// build a detailed status string if there are missing daemons
		std::string buf;
		if (missing_daemons) {
			buf.reserve(100);
			while (ordered_it != daemons.ordered_daemon_names.end()) {
				const char *name = (*ordered_it).c_str();
				daemon = daemons.FindDaemon( name );
				if (!daemon->pid)
				{
					if ( ! buf.empty()) { buf += ", "; }
					time_t starttime = daemon->GetNextRestart();
					if (starttime)
					{
						time_t secs_to_start = starttime-time(nullptr);
						if (secs_to_start > 0)
						{ formatstr_cat(buf, "%s=RESTART in %llds", name, (long long)secs_to_start); }
						else
						{ formatstr_cat(buf, "%s=RESTARTNG", name); }
					}
					else { formatstr_cat(buf, "%s=STOPPED", name); }
				}
				ordered_it++;
			}
			status = buf.c_str();
			format_string = "STATUS=Problems: %s\nWATCHDOG=1";
		}

		const condor_utils::SystemdManager &sd = condor_utils::SystemdManager::GetInstance();
		int result = sd.Notify(format_string, status);
		if (result == 0)
		{
			dprintf(D_ALWAYS, "systemd watchdog notification support not available.\n");
			daemonCore->Cancel_Timer(m_watchdog_timer);
			m_watchdog_timer = -1;
		}
	}

private:
	int m_watchdog_timer;
};
SystemdNotifier g_systemd_notifier;

// called at exit to deallocate stuff so that memory checking tools are
// happy and don't think we leaked any of this...
static void
cleanup_memory( void )
{
	if ( ad ) {
		delete ad;
		ad = NULL;
	}
	if ( MasterName ) {
		free( MasterName );
		MasterName = NULL;
	}
	if ( FS_Preen ) {
		free( FS_Preen );
		FS_Preen = NULL;
	}
	daemons.UnRegisterAllDaemons();
}

#ifdef WIN32
// we can't use execl on Windows (read the docs), and anyway, we don't want to
// if we are running as a serices because we MUST call our terminate() function to exit
static void Win32RunShutdownProgram(char * shutdown_program)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);

	HANDLE hLog = INVALID_HANDLE_VALUE;

	// Open a log file for the shutdown programs stdout and stderr
	std::string shutdown_log;
	if (param(shutdown_log, "MASTER_SHUTDOWN_LOG") &&  ! shutdown_log.empty()) {
		SECURITY_ATTRIBUTES sa;
		ZeroMemory(&sa, sizeof(sa));
		sa.bInheritHandle = TRUE;

		hLog = CreateFile(shutdown_log.c_str(), FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE != hLog) { si.hStdError = si.hStdOutput = hLog; }

		si.dwFlags = STARTF_USESTDHANDLES;
	}

	// create the shutdown program, we create it suspended so that we can log success or failure
	// and then get a head start to exit before it actually begins running.  That still doesn't
	// guarantee that we have exited before it starts though...
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	priv_state p = set_root_priv( );
	BOOL exec_status = CreateProcess( NULL, shutdown_program, NULL, NULL, TRUE, DETACHED_PROCESS | CREATE_SUSPENDED, NULL, NULL, &si, &pi );
	set_priv( p );

	if (INVALID_HANDLE_VALUE != hLog) {
		formatstr(shutdown_log, "==== Master (pid %d) shutting down. output of shutdown script '%s' (pid %d) follows ====\n",
			GetCurrentProcessId(), shutdown_program, pi.dwProcessId);
		DWORD dwWrote;
		WriteFile(hLog, shutdown_log.c_str(), (DWORD)shutdown_log.size(), &dwWrote, NULL);
		CloseHandle(hLog);
	}

	if ( ! exec_status ) {
		dprintf( D_ALWAYS, "**** CreateProcess(%s) FAILED %d %s\n",
					shutdown_program, GetLastError(), GetLastErrorString(GetLastError()) );
	} else {
		dprintf( D_ALWAYS, "**** CreateProcess(%s) SUCCEEDED pid=%lu\n", shutdown_program, pi.dwProcessId );
		ResumeThread(pi.hThread); // let the shutdown process start.
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}
#endif


int
master_exit(int retval)
{
	// If a shutdown_program has not been setup via the admin
	// command-line tool, see if the condor_config specifies
	// a default shutdown program to use.
	if ( !shutdown_program ) {
		char *defshut = param("DEFAULT_MASTER_SHUTDOWN_SCRIPT");
		if (defshut) {
			set_shutdown_program(defshut, NULL);
		}
	}

	cleanup_memory();

#ifdef WIN32
	if ( NT_ServiceFlag == TRUE ) {
		// retval == 2 indicates that we never actually started
		// so we don't want to invoke the shutdown program in that case.
		if (retval != 2 && shutdown_program) {
			dprintf( D_ALWAYS,
						"**** %s (%s_%s) pid %lu EXECING SHUTDOWN PROGRAM %s\n",
						"condor_master", "condor", "MASTER", GetCurrentProcessId(), shutdown_program );
			Win32RunShutdownProgram(shutdown_program);
			shutdown_program = NULL; // in case terminate doesn't kill us, make sure we don't run the shutdown program more than once.
		}
		terminate(retval);
	}
#endif

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	MasterPluginManager::Shutdown();
#endif

		// If we're positive that we are going to shut down,
		// we should clean out the shared port directory if
		// we created it.
	std::string dirname;
	if ( SharedPortEndpoint::CreatedSharedPortDirectory() &&
		 SharedPortEndpoint::GetDaemonSocketDir(dirname) ) {

		TemporaryPrivSentry tps(PRIV_CONDOR);
		Directory d(dirname.c_str());
		d.Remove_Entire_Directory();
		if (-1 == rmdir(dirname.c_str())) {
			dprintf(D_ALWAYS, "ERROR: failed to remove shared port temporary directory: %s (errno=%d).\n", strerror(errno), errno);
		}
	}

	// Exit via specified shutdown_program UNLESS retval is 2, which
	// means the master never started and we are exiting via usage() message.
#ifdef WIN32
	if (retval != 2 && shutdown_program) {
		dprintf( D_ALWAYS,
					"**** %s (%s_%s) pid %lu RUNNING SHUTDOWN PROGRAM %s\n",
					"condor_master", "condor", "MASTER", GetCurrentProcessId(), shutdown_program );
		Win32RunShutdownProgram(shutdown_program);
		shutdown_program = NULL; // don't let DC_Exit run the shutdown program on Windows.
	}
#endif
	DC_Exit(retval, retval == 2 ? NULL : shutdown_program );

	return 1;	// just to satisfy vc++
}

void
usage( const char* name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n name]\n", name );
	// Note: master_exit with value of 2 means do NOT run any shutdown_program
	master_exit( 2 );
}

int
DoCleanup(int,int,const char*)
{
	static int already_excepted = FALSE;

	// This function gets called as the last thing by EXCEPT().
	// At this point, the MASTER presumably has had an unrecoverable error
	// and is about to die.  We'll attempt to forcibly shutdown our
	// kiddies, and all their offspring before we fade away.  
	// Use a static flag to prevent infinite recursion in the case 
	// that there is another EXCEPT.

	// Once here, we are never returning to daemon core, so reapers, timers
	// etc. will never fire again, so we need to get everything we need
	// to do done now.

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
		daemons.HardKillAllDaemons();
	}

	if ( NT_ServiceFlag == FALSE ) {
		return 1;	// this will return to the EXCEPT code, which will just exit
	} else {
		master_exit(1); // this does not return
		return 1;		// just to satisfy VC++
	}
}

#if defined( LINUX )
void do_linux_kernel_tuning() {
	std::string kernelTuningScript;
	if(! param( kernelTuningScript, "LINUX_KERNEL_TUNING_SCRIPT" )) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: LINUX_KERNEL_TUNING_SCRIPT not defined.\n" );
		return;
	}

	std::string kernelTuningLogFile;
	param( kernelTuningLogFile, "KERNEL_TUNING_LOG" );
	if( kernelTuningLogFile.empty() ) {
		kernelTuningLogFile = "/dev/null";
	}

	if(! can_switch_ids() ) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: can't switch IDs.\n" );
		return;
	}

	priv_state prev = set_root_priv();
	int fd = safe_open_no_create( kernelTuningScript.c_str(), O_RDONLY );
	set_priv( prev );
	if( -1 == fd ) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: can't open file '%s'\n", kernelTuningScript.c_str() );
		return;
	}

	struct stat stats;
	int rv = fstat( fd, & stats );
	if( rv != 0 ) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: can't stat file '%s'\n", kernelTuningScript.c_str() );
		close( fd );
		return;
	}
	if( stats.st_uid != 0 ) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: file '%s' must be owned by root.\n", kernelTuningScript.c_str() );
		close( fd );
		return;
	}
	if( (stats.st_mode & S_IWGRP) || (stats.st_mode & S_IWOTH) ) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: file '%s' is group or world -writeable\n", kernelTuningScript.c_str() );
		close( fd );
		return;
	}
	if(! (stats.st_mode & S_IXUSR)) {
		dprintf( D_FULLDEBUG, "Not tuning kernel parameters: file '%s' is not executable.\n", kernelTuningScript.c_str() );
		close( fd );
		return;
	}
	close( fd );

	// Redirect the script's output into the log directory, in case anybody
	// else cares about it.  This simplifies our code, since we don't.
	// Do NOT block waiting for the script to exit -- shoot it in the head
	// after twenty seconds and report an error.  Otherwise, use its exit
	// code appropriately (may report an error).
	pid_t childPID = fork();
	if( childPID == 0 ) {
		daemonCore->Forked_Child_Wants_Fast_Exit( true );
		dprintf_init_fork_child();

		priv_state prev = set_root_priv();

		int fd = open( "/dev/null", O_RDONLY );
		if( fd == -1 ) {
			dprintf( D_FULLDEBUG, "Not tuning kernel parameters: child failed to open /dev/null: %d.\n", errno );
			exit( 1 );
		}
		if( dup2( fd, 0 ) == -1 ) {
			dprintf( D_FULLDEBUG, "Not tuning kernel parameters: child failed to dup /dev/null: %d.\n", errno );
			exit( 1 );
		}
		if( fd > 0 ) {
			close( fd );
		}
		fd = open( kernelTuningLogFile.c_str(), O_WRONLY | O_APPEND, 0644 );
		if ((fd < 0) && (errno == ENOENT)) {
			fd = open( kernelTuningLogFile.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644 );
		}
		if( fd == -1 ) {
			dprintf( D_FULLDEBUG, "Not tuning kernel parameters: child failed to open '%s': %d.\n", kernelTuningLogFile.c_str(), errno );
			exit( 1 );
		}
		if( dup2( fd, 1 ) == -1 ) {
			dprintf( D_FULLDEBUG, "Not tuning kernel parameters: child failed to dup log file '%s' to stdout: %d.\n", kernelTuningLogFile.c_str(), errno );
			exit( 1 );
		}
		if( dup2( fd, 2 ) == -1 ) {
			dprintf( D_FULLDEBUG, "Not tuning kernel parameters: child failed to dup log file '%s' to stderr: %d.\n", kernelTuningLogFile.c_str(), errno );
			exit( 1 );
		}
		if( fd > 2 ) {
			close( fd );
		}

		execl( kernelTuningScript.c_str(), kernelTuningScript.c_str(), (char *) NULL );

		set_priv( prev );
		dprintf( D_ALWAYS, "Did not tune kernel paramters: execl() failed: %d.\n", errno );
		exit( 1 );
	} else {
		int status = 0;
		pid_t wait = 0;
		unsigned timer = 20;

		while( true ) {
			wait = waitpid( childPID, & status, WNOHANG );
			if( wait == childPID ) {
				if( WIFEXITED(status) && WEXITSTATUS(status) == 0 ) {
					dprintf( D_FULLDEBUG, "Kernel parameters tuned.\n" );
					return;
				} else {
					dprintf( D_FULLDEBUG, "Failed to tune kernel parameters, status code %d.\n", status );
					return;
				}
			} else if( wait == 0 ) {
				if( --timer > 0 ) {
					dprintf( D_FULLDEBUG, "Sleeping one second for kernel parameter tuning (pid %d).\n", childPID );
					sleep( 1 );
				} else {
					dprintf( D_FULLDEBUG, "Waited too long for kernel parameters to be tuned, hard-killing script.\n" );
					kill( childPID, SIGKILL );
					// Collect the zombie.
					wait = waitpid( childPID, &status, WNOHANG );
					ASSERT( wait != childPID );
					return;
				}
			} else {
				dprintf( D_FULLDEBUG, "waitpid() failed while waiting for kernel tuning (%d).  Killing child %d.\n", errno, childPID );
				kill( childPID, SIGKILL );
				// Try again to collect the zombie?
				waitpid( childPID, & status, WNOHANG );
				return;
			}
		}
	}

}
#endif

void
main_init( int argc, char* argv[] )
{
    extern int runfor;
	char	**ptr;

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
				EXCEPT( "-n requires another argument" );
			}
			if (MasterName) {
				free(MasterName);
			}
			MasterName = build_valid_daemon_name( *ptr );
			dprintf( D_ALWAYS, "Using name: %s\n", MasterName );
			break;
		default:
			usage( argv[0] );
		}
	}

#ifdef LINUX
	if ( can_switch_ids() &&
		 param_boolean("DISCARD_SESSION_KEYRING_ON_STARTUP",true) )
	{
#ifndef KEYCTL_JOIN_SESSION_KEYRING
  #define KEYCTL_JOIN_SESSION_KEYRING 1
#endif
		// Create a new empty session keyring, discarding the old one.
		// We do this in the master so a session keyring containing
		// root keys is not inadvertantly leaked to jobs
		// (esp scheduler universe jobs, since
		// KEYCTL_JOIN_SESSION_KEYRING will fail in RHEL6 if the calling
		// process has more than one thread, which is true when spawning
		// scheduler universe jobs as the schedd clones by default instead
		// of forking).  We also do this in the master instead
		// of daemonCore, since we don't want to create a brand new
		// session keyring for every shadow (each keyring uses up kernel resources).
		// If the syscall fails due to ENOSYS, don't worry about it,
		// since that simply says the keyring facility is not installed.
		if (syscall(__NR_keyctl, KEYCTL_JOIN_SESSION_KEYRING, "htcondor")==-1 &&
			errno != ENOSYS)
		{
			int saved_errno = errno;
#if defined(EDQUOT)
			if (saved_errno == EDQUOT) {
				dprintf(D_ERROR,
				   "Error during DISCARD_SESSION_KEYRING_ON_STARTUP, suggest "
				   "increasing /proc/sys/kernel/keys/root_maxkeys\n");
			}
#endif /* defined EDQUOT */

			if (saved_errno == EPERM) {
				dprintf(D_ALWAYS, "Permission denied error during DISCARD_SESSION_KEYRING_ON_STARTUP, continuing anyway\n");
			} else {
				EXCEPT("Failed DISCARD_SESSION_KEYRING_ON_STARTUP=True errno=%d",
						saved_errno);
			}
		}
	}
#endif /* defined LINUX */

    if (runfor != 0) {
        // We will construct an environment variable that 
        // tells the daemon what time it will be shut down. 
        // We'll give it an absolute time, though runfor is a 
        // relative time. This means that we don't have to update
        // the time each time we restart the daemon.
		std::string runfor_env;
		formatstr(runfor_env,"%s=%ld", ENV_DAEMON_DEATHTIME,
						   time(NULL) + (runfor * 60));
		SetEnv(runfor_env.c_str());
    }

	daemons.SetDefaultReaper();
	
		// Grab all parameters needed by the master.
	init_params();
	validate_config(0, CONFIG_OPT_DEPRECATION_WARNINGS);
		// param() for DAEMON_LIST and initialize our daemons object.
	init_daemon_list();
	if ( daemons.SetupControllers() < 0 ) {
		EXCEPT( "Daemon initialization failed" );
	}
		// Lookup the paths to all the daemons we now care about.
	daemons.InitParams();
		// Initialize our classad;
	init_classad();  
		// Initialize the master entry in the daemons data structure.
	daemons.InitMaster();
		// open up the windows firewall 
	init_firewall_exceptions();

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(UNIX)
	MasterPluginManager::Load();
#elif defined(WIN32)
	load_master_mgmt();
#endif
	MasterPluginManager::Initialize();
#endif

	g_systemd_notifier.config();

		// Register admin commands
	daemonCore->Register_Command( DAEMONS_OFF_FLEX, "DAEMONS_OFF_FLEX",
								  admin_command_handler,
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( RESTART, "RESTART",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( RESTART_PEACEFUL, "RESTART_PEACEFUL",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_OFF, "DAEMONS_OFF",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_OFF_FAST, "DAEMONS_OFF_FAST",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_OFF_PEACEFUL, "DAEMONS_OFF_PEACEFUL",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMONS_ON, "DAEMONS_ON",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( MASTER_OFF, "MASTER_OFF",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( MASTER_OFF_FAST, "MASTER_OFF_FAST",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_ON, "DAEMON_ON",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_OFF, "DAEMON_OFF",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_OFF_FAST, "DAEMON_OFF_FAST",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( DAEMON_OFF_PEACEFUL, "DAEMON_OFF_PEACEFUL",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( CHILD_ON, "CHILD_ON",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( CHILD_OFF, "CHILD_OFF",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( CHILD_OFF_FAST, "CHILD_OFF_FAST",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	daemonCore->Register_Command( SET_SHUTDOWN_PROGRAM, "SET_SHUTDOWN_PROGRAM",
								  admin_command_handler, 
								  "admin_command_handler", ADMINISTRATOR );
	// Command handler for stashing the pool password
	daemonCore->Register_Command( STORE_POOL_CRED, "STORE_POOL_CRED",
								&store_pool_cred_handler,
								"store_pool_cred_handler", ADMINISTRATOR );

	// Command handler for handling the ready state
	daemonCore->Register_CommandWithPayload( DC_SET_READY, "DC_SET_READY",
								  ready_command_handler,
								  "ready_command_handler", DAEMON );
	daemonCore->Register_CommandWithPayload( DC_QUERY_READY, "DC_QUERY_READY",
								  ready_command_handler,
								  "ready_command_handler", READ );

	daemonCore->RegisterTimeSkipCallback(time_skip_handler,0);

	_EXCEPT_Cleanup = DoCleanup;

#if !defined(WIN32)
	if( !dprintf_to_term_check() && param_boolean( "USE_PROCESS_GROUPS", true ) ) {
			// If we're not connected to a terminal, start our own
			// process group, unless the config file says not to.
		setsid();
	}
#endif

	if( StartDaemons ) {
		daemons.StartAllDaemons();
	} else {
	#ifndef WIN32
		// StartAllDaemons does this , but if we don't call that ...
		dc_release_background_parent(0);
	#endif
	}
	daemons.StartTimers();
}


int
ready_command_handler(int cmd, Stream* stm )
{
	ReliSock* stream = (ReliSock*)stm;
	ClassAd cmdAd;

	stream->decode();
	stream->timeout(15);
	if( !getClassAd(stream, cmdAd) || !stream->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive ready command (%d) on TCP: aborting\n", cmd );
		return FALSE;
	}
	std::string daemon_name;
	cmdAd.LookupString("DaemonName", daemon_name);
	int daemon_pid = 0;
	cmdAd.LookupInteger("DaemonPID", daemon_pid);

	dprintf( D_FULLDEBUG, "Got ready command (%d) from %s pid=%d\n", cmd, daemon_name.c_str(), daemon_pid );

	switch (cmd) {
		case DC_SET_READY:
		{
			std::string state;
			cmdAd.LookupString("DaemonState", state);
			class daemon* daemon = daemons.FindDaemonByPID(daemon_pid);
			if ( ! daemon) {
				dprintf(D_ALWAYS, "Cant find daemon %s to set ready state '%s'\n", daemon_name.c_str(), state.c_str());
			} else {
				dprintf(D_ALWAYS, "Setting ready state '%s' for %s\n", state.c_str(), daemon_name.c_str());
				daemon->SetReadyState(state.c_str());
			}
		}
		return TRUE;

		case DC_QUERY_READY:
			return daemons.QueryReady(cmdAd, stm);
		return FALSE;

		default:
			EXCEPT("Unknown ready command (%d) in ready_command_handler", cmd);
	}

	return FALSE;
}


int
do_basic_admin_command(int cmd)
{
	switch (cmd) {
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
		daemons.DaemonsOff(1);
		return TRUE;
	case DAEMONS_OFF_PEACEFUL:
		daemons.DaemonsOffPeaceful();
		return TRUE;
	case DC_OFF_PEACEFUL: // internal use, the real command handler is in daemoncore
		daemonCore->SetPeacefulShutdown(true);
		// fall through
	case MASTER_OFF:
#if 0
		// DC pump will call our main_shutdown_normal via dc_main_shutdown_graceful
		daemonCore->Signal_Myself(SIGTERM);
#else
		// if we are doing peaceful tell the children, and set a timer to do the real shutdown
		// so the children have a chance to notice the messages
		//
		if (daemonCore->GetPeacefulShutdown()) {
			int timeout = 5;
			if (daemons.SetPeacefulShutdown(timeout) > 0) {
				int tid = daemonCore->Register_Timer(timeout+1, 0,
								(TimerHandler)main_shutdown_graceful,
								"main_shutdown_graceful");
				if (tid != -1)
					return TRUE;

				dprintf(D_ALWAYS, "ERROR! Can't register DaemonCore timer for peaceful shutdown. Shutting down now...\n");
			}
		}
		// fall through
	case DC_OFF_GRACEFUL: // used internally to ignore daemoncore peaceful/graceful flag
		if (MasterShuttingDown)
			return TRUE;
		invalidate_ads();
		MasterShuttingDown = TRUE;
		daemons.SetAllGoneAction( MASTER_EXIT );
		daemons.CancelRestartTimers();
		daemons.StopAllDaemons();
#endif
		return TRUE;
	case MASTER_OFF_FAST:
#if 0
		// DC pump will call our main_shutdown_fast
		daemonCore->Signal_Myself(SIGQUIT);
#else
	case DC_OFF_FAST:  // internal use, the real command handler for this is in daemoncore
		invalidate_ads();
		MasterShuttingDown = TRUE;
		daemons.SetAllGoneAction( MASTER_EXIT );
		daemons.CancelRestartTimers();
		daemons.StopFastAllDaemons();
#endif
		return TRUE;
	}
	return FALSE;
}

int
admin_command_handler(int cmd, Stream* stream )
{
	if(! AllowAdminCommands ) {
		dprintf(D_STATUS, "Got admin command %s while not allowed. Ignoring.\n", getCommandStringSafe(cmd));
		return FALSE;
	}
	if (cmd != DAEMONS_OFF_FLEX && (cmd < DAEMON_OFF || cmd > DAEMON_OFF_PEACEFUL)) {
		// log the command, we let handle_subsys_command and handle_flexible_daemons_command
		// do their own logging. (the CHILD_* commands are for HAD, not worth a special case here..)
		dprintf(D_STATUS, "Handling admin command %s.\n", getCommandStringSafe(cmd));
	}

	switch( cmd ) {
	case RESTART:
	case RESTART_PEACEFUL:
	case DAEMONS_ON:
	case DAEMONS_OFF:
	case DAEMONS_OFF_FAST:
	case DAEMONS_OFF_PEACEFUL:
	case MASTER_OFF:
	case MASTER_OFF_FAST:
		return do_basic_admin_command(cmd);

	case SET_SHUTDOWN_PROGRAM:
		return handle_shutdown_program( cmd, stream );

	case DAEMONS_OFF_FLEX:
		return handle_flexible_daemons_command(cmd, stream);

			// These commands are special, since they all need to read
			// off the subsystem before they know what to do.  So, we
			// handle them with a special function.
	case DAEMON_ON:
	case DAEMON_OFF:
	case DAEMON_OFF_FAST:
	case DAEMON_OFF_PEACEFUL:
	case CHILD_ON:
	case CHILD_OFF:
	case CHILD_OFF_FAST:
		return handle_subsys_command( cmd, stream );

	default: 
		EXCEPT( "Unknown admin command (%d) in handle_admin_commands",
				cmd );
	}
	return FALSE;
}

int do_flexible_daemons_command(int cmd, ClassAd & cmdAd, ClassAd * replyAd /* = nullptr */)
{
	int rval = TRUE;
	bool success = false;
	int how_fast = DRAIN_GRACEFUL;
	int on_completion = DRAIN_EXIT_ON_COMPLETION;
	std::string drain, drain_reason;
	ExprTree *drain_check = nullptr, *drain_start = nullptr;
	std::vector<std::string> drain_list;
	ClassAd drain_request_ids;
	auto_free_ptr shutdown_path; // full path to the shutdown exe
	std::string shutdown_task; // knob tag name
	bool set_shutdown = false;
	class daemon* daemon;

	int real_cmd = 0;
	cmdAd.LookupInteger("Command", real_cmd);
	switch (real_cmd) {
		case 0:
		case RESTART:
		case MASTER_OFF:
		case MASTER_OFF_FAST:
		case DAEMONS_OFF:
			break;
		case DC_RECONFIG_FULL:
			on_completion = DRAIN_RECONFIG_ON_COMPLETION;
			break;
		default:
			dprintf(D_STATUS, "Invalid payload command %s in %s\n", getCommandStringSafe(real_cmd), getCommandStringSafe(cmd));
			rval = FALSE;
			goto bail;
			break;
	}

	//dprintf(D_STATUS, "Got %s in %s\n", getCommandStringSafe(real_cmd), getCommandStringSafe(cmd));

	if (cmdAd.LookupString("Drain", drain)) {
		upper_case(drain); // so we can use this with FindDaemon
		cmdAd.LookupInteger(ATTR_HOW_FAST, how_fast);
		cmdAd.LookupString(ATTR_DRAIN_REASON, drain_reason);
		drain_check = cmdAd.Lookup(ATTR_CHECK_EXPR);
		drain_start = cmdAd.Lookup(ATTR_START_EXPR);
	}

	// SetShutdown should be either the empty string or a valid MASTER_SHUTDOWN_<tag> tag value
	// the empty string will remove an existing shutdown task, otherwise it will set the
	// shutdown task after draining completes but before shutdown.
	set_shutdown = cmdAd.LookupString("ShutdownTask", shutdown_task);
	if ( ! shutdown_task.empty()) {
		std::string pname("MASTER_SHUTDOWN_"); pname += shutdown_task.c_str();
		shutdown_path.set(param(pname.c_str()));
		// it is not an error for a dummy shutdown task to exist, but we want to report
		// if they try to use a task for which there is no knob at all
		if (! shutdown_path && ! param_defined(pname.c_str())) {
			dprintf(D_STATUS, "Aborting %s because no shutdown program defined for '%s'\n", getCommandStringSafe(real_cmd), pname.c_str());
			rval = FALSE;
			goto bail;
		}
	}

	if ( ! drain.empty()) {
		dprintf(D_STATUS, "Handling %s : %s Drain=%s\n", getCommandStringSafe(cmd), getCommandStringSafe(real_cmd), drain.c_str());
		if (drain == "STARTDS") {
			// build a list of daemon names for the startds
			daemons.ChildrenOfType(DT_STARTD, &drain_list);
		} else {
			drain_list = split(drain);
		}
		dprintf(D_FULLDEBUG, "Effective drain list is %s\n", join(drain_list, ",").c_str());
	} else {
		dprintf(D_STATUS, "Handling %s : %s\n", getCommandStringSafe(cmd), getCommandStringSafe(real_cmd));
	}

	drain_request_ids.Clear();
	for (auto &subsys: drain_list) {
		if (!(daemon = daemons.FindDaemon(subsys.c_str()))) {
			dprintf(D_ERROR, "Error: Can't find daemon %s to drain for %s\n", subsys.c_str(), getCommandStringSafe(real_cmd));
		} else {
			dprintf(D_STATUS, "Sending drain command to %s\n", subsys.c_str());
			std::string request_id;
			int drain_started = daemon->Drain(request_id, how_fast, on_completion, drain_reason.c_str(), drain_check, drain_start);
			if (drain_started > 0) {
				drain_request_ids.Assign(daemon->name_in_config_file, request_id);
				dprintf(D_FULLDEBUG, "Drain of %s started, id=%s\n", subsys.c_str(), request_id.c_str());
			} else if (drain_started < 0) {
				// unable to drain a child, abort the command
				dprintf(D_ERROR, "Aborting %s because of failure to start drain of %s\n", getCommandStringSafe(real_cmd), daemon->daemon_name);
				rval = FALSE;
				goto bail;
			} else {
				dprintf(D_FULLDEBUG, "%s did not need to drain\n", subsys.c_str());
			}
		}
	}

	if (drain_request_ids.size() > 0) {
		// If children are draining, we want to setup a deferred handler for the
		// command that should execute when draining completes
		ClassAd * ad = new ClassAd();
		CopyAttribute("Command", *ad, cmdAd);
		CopyAttribute("ShutdownTask", *ad, cmdAd);
		daemons.cmd_after_drain = ad;

		success = true;
		if (replyAd) {
			replyAd->Insert("Draining", drain_request_ids.Copy());
		}
	} else {
		if (set_shutdown) {
			set_shutdown_program(shutdown_path.detach(), shutdown_task.c_str());
			if (replyAd) { advertise_shutdown_program(*replyAd); }
		}
		if (real_cmd) {
			rval = do_basic_admin_command(real_cmd);
		}
		success = rval == TRUE;
	}

bail:
	if (replyAd) {
		daemons.InitDaemonReadyAd(*replyAd, false);
		replyAd->Assign("Success", success);
	}

	return rval;
}

int
handle_flexible_daemons_command( int cmd, Stream* stream )
{
	ClassAd cmdAd, replyAd;

	stream->decode();
	if( ! getClassAd(stream, cmdAd) ) {
		dprintf( D_ERROR, "Can't read command payload for %s\n", getCommandStringSafe(cmd) );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ERROR, "Can't read end_of_message for %s\n", getCommandStringSafe(cmd) );
		return FALSE;
	}

	bool want_reply = false;
	cmdAd.LookupBool("WantReply", want_reply);
	int real_cmd = 0; // for logging
	cmdAd.LookupInteger("Command", real_cmd);

	int rval = do_flexible_daemons_command(cmd, cmdAd, want_reply ? &replyAd : nullptr);

	if (want_reply) {
		stream->encode();
		if (!putClassAd(stream, replyAd) || !stream->end_of_message()) {
			dprintf(D_ERROR, "Error: can't send reply ad for %s in %s\n",
				getCommandStringSafe(real_cmd), getCommandStringSafe(cmd));
			rval = FALSE;
		} else {
			std::string buf;
			dprintf(D_STATUS, "Reply for %s in %s :\n%s\n",
				getCommandStringSafe(real_cmd), getCommandStringSafe(cmd),
				formatAd(buf, replyAd, "\t"));
		}
	}

	return rval;
}


int
handle_subsys_command( int cmd, Stream* stream )
{
	std::string subsys;
	class daemon* daemon;

	stream->decode();
	if( ! stream->code(subsys) ) {
		dprintf( D_ALWAYS, "Can't read subsystem name\n" );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		return FALSE;
	}

	upper_case(subsys); // so we can use this with FindDaemon

	// for testing condor_on -preen is allowed, but preen is not really a daemon
	// so we intercept it here and 
	if (strcasecmp(subsys.c_str(), "preen") == MATCH) {
		return run_preen_now();
	}

	if( !(daemon = daemons.FindDaemon(subsys.c_str())) ) {
		dprintf( D_ALWAYS, "Error: Can't find daemon of type %s\n", subsys.c_str());
		return FALSE;
	}
	dprintf(D_STATUS, "Handling %s command for %s\n", getCommandStringSafe(cmd), subsys.c_str());

	switch( cmd ) {
	case DAEMON_ON:
		daemon->Hold( false );
		return daemon->Start();
	case DAEMON_OFF:
		daemon->Hold( true );
		daemon->Stop();
		return TRUE;
	case DAEMON_OFF_FAST:
		daemon->Hold( true );
		daemon->StopFast();
		return TRUE;
	case DAEMON_OFF_PEACEFUL:
		daemon->Hold( true );
		daemon->StopPeaceful();
		return TRUE;
	case CHILD_ON:
		daemon->Hold( false, true );
		return daemon->Start( true );
	case CHILD_OFF:
		daemon->Hold( true, true );
		daemon->Stop( true );
		return TRUE;
	case CHILD_OFF_FAST:
		daemon->Hold( true, true );
		daemon->StopFast( true );
		return TRUE;
	default:
		EXCEPT( "Unknown command %s in handle_subsys_command", getCommandStringSafe(cmd));
	}
	return FALSE;
}


int
handle_shutdown_program( int cmd, Stream* stream )
{
	if ( cmd != SET_SHUTDOWN_PROGRAM ) {
		EXCEPT( "Unknown command (%d) in handle_shutdown_program", cmd );
	}

	std::string name;
	stream->decode();
	if( ! stream->code(name) ) {
		dprintf( D_ALWAYS, "Can't read program name in handle_shutdown_program\n" );
	}

	if ( name.empty() ) {
		return FALSE;
	}
	upper_case(name); // because we print this

	// Can we find it in the configuration?
	std::string pname("MASTER_SHUTDOWN_");
	pname += name.c_str();
	char * path = param(pname.c_str());
	if ( ! path && ! param_defined(pname.c_str())) {
		dprintf( D_ALWAYS, "No shutdown program defined for '%s'\n", pname.c_str() );
		return FALSE;
	}

	// path will be NULL here when MASTER_SHUTDOWN_<NAME> is configured but expands to nothing
	// we still want to override a previous shutdown program
	return set_shutdown_program(path, name.c_str());
}

static int
set_shutdown_program(char *path, const char * tag)
{
	shutdown_program_tag.clear();
	if (tag) { shutdown_program_tag = tag; }

	if (path) {
		// Try to access() it
	#if defined(HAVE_ACCESS)
		priv_state	priv = set_root_priv();
		int status = access(path, X_OK);
		if (status) {
			dprintf(D_ALWAYS,
				"WARNING: no execute access to shutdown program (%s)"
				": %d/%s\n", path, errno, strerror(errno));
		}
		set_priv(priv);
	#endif
	}

	// OK, let's run with that
	if (shutdown_program) { free( shutdown_program ); }
	shutdown_program = path;
	dprintf(D_ALWAYS, "Setting shutdown program %s path to %s\n",
			shutdown_program_tag.c_str(),
			shutdown_program ? shutdown_program : "");
	return TRUE;
}

bool advertise_shutdown_program(ClassAd & ad)
{
	// if a MASTER_SHUTDOWN_<tag> knob will be used when the master exits
	// advertise that into the given ad as
	// MASTER_SHUTDOWN = "TAG"
	// MASTER_SHUTDOWN_TAG = "/path/to/tag_shutdown_program"
	if ( ! shutdown_program_tag.empty()) {
		ad.Assign("MASTER_SHUTDOWN", shutdown_program_tag);
		std::string knob("MASTER_SHUTDOWN_"); knob += shutdown_program_tag;
		if (shutdown_program) {
			ad.Assign(knob, shutdown_program);
		} else {
			ad.AssignExpr(knob, "undefined");
		}
		return true;
	}
	return false;
}


void
init_params()
{
	char	*tmp;
	static	int	master_name_in_config = 0;

	// To do fast polling for a daemon's address file, the master sleeps
	// for 100ms before registering a 0-second timer for the next file
	// existence check.  On Darwin, without an ugly hack in
	// do_shared_port_local_connect(), this causes the shared port daemon's
	// child-alive message to be lost frequently.  I'm leaving the fall-back
	// to the older (slower) code in place in case we discover problems with
	// the ugly hack.
#if defined(DARWIN)
	DaemonStartFastPoll = true;
#else
	DaemonStartFastPoll = true;
#endif

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
		free( MasterName );
		tmp = param( "MASTER_NAME" );
		MasterName = build_valid_daemon_name( tmp );
		free( tmp );
	}
	if( MasterName ) {
		dprintf( D_FULLDEBUG, "Using name: %s\n", MasterName );
	}
			
	if (!param_boolean_crufty("START_MASTER", true)) {
			dprintf( D_ALWAYS, "START_MASTER was set to FALSE, shutting down.\n" );
			StartDaemons = FALSE;
			main_shutdown_graceful();
	}

		
	StartDaemons = TRUE;
	if (!param_boolean_crufty("START_DAEMONS", true)) {
			dprintf( D_ALWAYS, 
					 "START_DAEMONS flag was set to FALSE.  Not starting daemons.\n" );
			StartDaemons = FALSE;
	} 
		// If we were sent the daemons_off command, don't forget that
		// here. 
	if( GotDaemonsOff ) {
		StartDaemons = FALSE;
	}

	PublishObituaries = param_boolean_crufty("PUBLISH_OBITUARIES", true) ? TRUE : FALSE;

	Lines = param_integer("OBITUARY_LOG_LENGTH",20);

	master_backoff_constant = param_integer( "MASTER_BACKOFF_CONSTANT", 9, 1 );

	master_backoff_ceiling = param_integer( "MASTER_BACKOFF_CEILING", 3600,1 );

	master_backoff_factor = param_double( "MASTER_BACKOFF_FACTOR", 2.0, 0 );
	if( master_backoff_factor <= 0.0 ) {
    	master_backoff_factor = 2.0;
    }
	
	master_recover_time = param_integer( "MASTER_RECOVER_FACTOR", 300, 1 );

	update_interval = param_integer( "MASTER_UPDATE_INTERVAL", 5 * MINUTE, 1 );

	check_new_exec_interval = param_integer( "MASTER_CHECK_NEW_EXEC_INTERVAL", 5*MINUTE );

	new_bin_delay = param_integer( "MASTER_NEW_BINARY_DELAY", 2*MINUTE, 1 );

	new_bin_restart_mode = GRACEFUL;
	char * restart_mode = param("MASTER_NEW_BINARY_RESTART");
	if (restart_mode) {
		StopStateT mode = StringToStopState(restart_mode);
		if (mode == (StopStateT)-1)	{
			dprintf(D_ALWAYS, "%s is not a valid value for MASTER_NEW_BINARY_RESTART. using GRACEFUL\n", restart_mode);
		}
		if (mode >= 0 && mode <= NONE)
			new_bin_restart_mode = mode;
		free(restart_mode);
	}

	preen_interval = param_integer( "PREEN_INTERVAL", 24*HOUR, 0 );
	if(preen_interval == 0) {
		EXCEPT("PREEN_INTERVAL in the condor configuration is too low (0).  Please set it to an integer in the range 1 to %d (default %d).  To disable condor_preen entirely, comment out PREEN.", INT_MAX, 24*HOUR);

	}

	shutdown_fast_timeout = param_integer( "SHUTDOWN_FAST_TIMEOUT", 5*MINUTE, 1 );

	shutdown_graceful_timeout = param_integer( "SHUTDOWN_GRACEFUL_TIMEOUT", 30*MINUTE, 1 );

	AllowAdminCommands = param_boolean( "ALLOW_ADMIN_COMMANDS", true );

	if( FS_Preen ) {
		free( FS_Preen );
	}
	FS_Preen = param( "PREEN" );
}

// helper function to determin if the executable of a daemon matches the executable of a DC daemon
static bool same_exe_as_daemon(const char * daemon_name, StringList & dc_names, std::set<std::string> & dc_exes)
{
	// first time we call this, build a collection of daemon executables paths
	if (dc_exes.empty() && ! dc_names.isEmpty()) {
		for (const char * name = dc_names.first(); name != nullptr; name = dc_names.next()) {
			auto_free_ptr program(param(name));
			if (program) { dc_exes.insert(program.ptr()); }
		}
	}

	// check to see if the give daemon has the same binary as one of the DC daemons
	auto_free_ptr program(param(daemon_name));
	if (program && dc_exes.count(program.ptr())) {
		return true;
	}
	return false;
}

void
init_daemon_list()
{
	char	*daemon_name;
	StringList daemon_names, dc_daemon_names;
	bool have_primary_collector = false; // daemon list has COLLECTOR (just that - VIEW_COLLECTOR or COLLECTOR_B doesn't count)
	std::set<std::string> dc_daemon_exes; // paths to daemon binaries for the daemon core daemons, used if needed

	daemons.ordered_daemon_names.clear();
	char* dc_daemon_list = param("DC_DAEMON_LIST");

	if( !dc_daemon_list ) {
		dc_daemon_names.initializeFromString(default_dc_daemon_list);
	}
	else {
		if ( *dc_daemon_list == '+' ) {
			std::string	dclist;
			dclist = default_dc_daemon_list;
			dclist += ", ";
			dclist += &dc_daemon_list[1];
			dc_daemon_names.initializeFromString( dclist.c_str() );
		}
		else {
			dc_daemon_names.initializeFromString(dc_daemon_list);

			StringList default_list(default_dc_daemon_list);
			default_list.rewind();
			char *default_entry;
			int	missing = 0;
			while( (default_entry=default_list.next()) ) {
				if( !dc_daemon_names.contains_anycase(default_entry) ) {
					dprintf(D_ALWAYS,
							"WARNING: expected to find %s in"
							" DC_DAEMON_LIST, but it is not there.\n",
							default_entry );
					missing++;
				}
			}
			if ( missing ) {
				dprintf( D_ALWAYS,
						 "WARNING: "
						 "%d entries are missing from DC_DAEMON_LIST.  "
						 "Unless you know what you are doing, it "
						 "is best to leave DC_DAEMON_LIST undefined "
						 "so that the default settings are used, "
						 "or use the new 'DC_DAEMON_LIST = "
						 "+<list>' syntax.\n", missing );
			}
				
		}
		free(dc_daemon_list);
	}

		// Tolerate a trailing comma in the list
	dc_daemon_names.remove( "" );

	char* ha_list = param("MASTER_HA_LIST");
	if( ha_list ) {
			// Make MASTER_HA_LIST case insensitive by always converting
			// what we get to uppercase.
		ha_list = strupr( ha_list );
		std::vector<std::string> ha_names = split(ha_list);

		for (const auto &ha_name: ha_names) {
			if (!contains(daemons.ordered_daemon_names, ha_name)) {
				daemons.ordered_daemon_names.emplace_back(ha_name);
			}
		}

		for (const auto &daemon_name: ha_names) {
			if(daemons.FindDaemon(daemon_name.c_str()) == nullptr) {
				if( dc_daemon_names.contains(daemon_name.c_str()) ) {
					new class daemon(daemon_name.c_str(), true, true );
				} else {
					new class daemon(daemon_name.c_str(), false, true );
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
			// Tolerate a trailing comma in the list
		daemon_names.remove( "" );

			// if the master is not in the list, pretend it is.
			// so we end up creating a daemon object for it, and don't just abort
		if ( ! daemon_names.contains("MASTER")) {
			daemon_names.append("MASTER");
		}


			/*
			  Make sure that if COLLECTOR is in the list, put it at
			  the front...  unfortunately, our List template (what
			  StringList is defined in terms of) is amazingly broken
			  with regard to insert() and append().  :( insert()
			  usually means: insert *before* the current position.
			  however, if you're at the end, it inserts before the
			  last valid entry, instead of working like append() as
			  you'd expect.  OR, if you just called rewind() and
			  insert() (to insert at the begining, right?) it works
			  like append() and sticks it at the end!!  ARGH!!!  so,
			  we've got to call next() after rewind() so we really
			  insert it at the front of the list.  UGH!  EVIL!!!
			  Derek Wright <wright@cs.wisc.edu> 2004-12-23
			*/
		if( daemon_names.contains("COLLECTOR") ) {
			daemon_names.deleteCurrent();
			daemon_names.rewind();
			daemon_names.next();
			daemon_names.insert( "COLLECTOR" );
			have_primary_collector = true;
		}

			// start shared_port first for a cleaner startup
		if( daemon_names.contains("SHARED_PORT") ) {
			daemon_names.deleteCurrent();
			daemon_names.rewind();
			daemon_names.next();
			daemon_names.insert( "SHARED_PORT" );
		}
		else if( SharedPortEndpoint::UseSharedPort() ) {
			if( param_boolean("AUTO_INCLUDE_SHARED_PORT_IN_DAEMON_LIST",true) ) {
				dprintf(D_ALWAYS,"Adding SHARED_PORT to DAEMON_LIST, because USE_SHARED_PORT=true (to disable this, set AUTO_INCLUDE_SHARED_PORT_IN_DAEMON_LIST=False)\n");
				daemon_names.rewind();
				daemon_names.next();
				daemon_names.insert( "SHARED_PORT" );
			}
		}

		if( param_boolean("AUTO_INCLUDE_CREDD_IN_DAEMON_LIST", false)) {
			if (daemon_names.contains("SCHEDD")) {
				if (!daemon_names.contains("CREDD")) {
					dprintf(D_ALWAYS, "Adding CREDD to DAEMON_LIST.  This machine is running a SCHEDD and AUTO_INCLUDE_CREDD_IN_DAEMON_LIST is TRUE)\n");
					daemon_names.append("CREDD");
				} else {
					dprintf(D_SECURITY|D_VERBOSE, "Not modifying DAEMON_LIST. This machine is running a SCHEDD and CREDD is already explicitly listed.\n");
				}
			} else {
				dprintf(D_SECURITY|D_VERBOSE, "Not modifying DAEMON_LIST.  AUTO_INCLUDE_CREDD_IN_DAEMON_LIST is TRUE, but this machine is not running a SCHEDD.\n");
			}
		} else {
			dprintf(D_SECURITY|D_VERBOSE, "Not modifying DAEMON_LIST.  AUTO_INCLUDE_CREDD_IN_DAEMON_LIST is false.\n");
		}

		daemon_names.rewind();
		while ((daemon_name = daemon_names.next())) {
			if (!contains(daemons.ordered_daemon_names, daemon_name)) {
				daemons.ordered_daemon_names.emplace_back(daemon_name);
			}
		}	
		daemon_names.rewind();

		while( (daemon_name = daemon_names.next()) ) {
			if(daemons.FindDaemon(daemon_name) == NULL) {
				bool is_DC = dc_daemon_names.contains(daemon_name);
				if ( ! is_DC) {
					// daemon is not in the DC list, check to see if uses the same executable
					// as on of the DC daemons, if so, we presume it is DC
					is_DC = same_exe_as_daemon(daemon_name, dc_daemon_names, dc_daemon_exes);
				}
				new class daemon(daemon_name, is_DC);
			}
		}
	} else {
		// some daemons should start only if shared port is enabled
		bool shared_port = SharedPortEndpoint::UseSharedPort() && param_boolean("AUTO_INCLUDE_SHARED_PORT_IN_DAEMON_LIST", true);

		// use the default daemon list to decide which daemons to start now, and which will we allow to start
		for(int i = 0; default_daemon_list[i].name; i++) {
			const char * name = default_daemon_list[i].name;
			daemon_names.append(name); // add to list of allowed daemons
			// should we start it now?
			if (default_daemon_list[i].auto_start) {
				if (shared_port || ! default_daemon_list[i].only_if_shared_port) {
					new class daemon(default_daemon_list[i].name);
				}
			}
		}
		daemon_names.rewind();
		while ((daemon_name = daemon_names.next())) {
			if (!contains(daemons.ordered_daemon_names, daemon_name)) {
				daemons.ordered_daemon_names.emplace_back(daemon_name);
			}
		}
	}

	// if we have a primary collector, and it is behind a shared port daemon
	// then we need to let the SHARED_PORT daemon know that it should be using the configured collector port
	// and not the configured shared port port
	if (have_primary_collector) {
		bool collector_uses_shared_port = param_boolean("COLLECTOR_USES_SHARED_PORT", true) && param_boolean("USE_SHARED_PORT", false);
		if (collector_uses_shared_port) {
			class daemon* d = daemons.FindDaemon("SHARED_PORT");
			if (d != NULL) {
				d->use_collector_port = d->isDC;
				dprintf(D_ALWAYS,"SHARED_PORT is in front of a COLLECTOR, so it will use the configured collector port\n");
			}
		}
	}
}


void
init_classad()
{
	if( ad ) delete( ad );
	ad = new ClassAd();

	SetMyTypeName(*ad, MASTER_ADTYPE);

	if (MasterName) {
		ad->Assign(ATTR_NAME, MasterName);
	} else {
		char* default_name = default_daemon_name();
		if( ! default_name ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
		ad->Assign(ATTR_NAME, default_name);
		free(default_name);
	}

#if !defined(WIN32)
	ad->Assign(ATTR_REAL_UID, (int)getuid());
#ifdef LINUX
	//caps = capabilities
	uint64_t caps = sysapi_get_process_caps_mask(getpid());
	if (caps != UINT64_MAX) { //If max for uint64 then a failure happended
		std::string caps_hex;
		formatstr(caps_hex,"0x%llx",(long long)caps);
		ad->Assign(ATTR_LINUX_CAPS,caps_hex.c_str());
	} else { dprintf(D_ALWAYS,"Error: Failed to get Linux process capabilites mask.\n"); }
#endif
#endif
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
		EXCEPT( "can't safe_open_wrapper(%s,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR) - errno %i", 
			file_name, errno );
	}

	// This must be a global so that it doesn't go out of scope
	// cause the destructor releases the lock.
	MasterLock = new FileLock( MasterLockFD, NULL, file_name );
	MasterLock->setBlocking( FALSE );
	if( !MasterLock->obtain(WRITE_LOCK) ) {
		EXCEPT( "Can't get lock on \"%s\"", file_name );
	}
#endif
}


/*
 ** Re read the config file, and send all the daemons a signal telling
 ** them to do so also.
 */
void
main_config()
{
	std::vector<std::string> old_daemon_list(daemons.ordered_daemon_names);

		// Re-read the config files and create a new classad
	init_classad(); 

		// Reset our config values
	init_params();
	validate_config(0, CONFIG_OPT_DEPRECATION_WARNINGS);

		// Reset the daemon list
	init_daemon_list();

		// Remove daemons that should no longer be running
	for (const auto &daemon_name: old_daemon_list) {
		if (!contains(daemons.ordered_daemon_names, daemon_name)) {
			if( nullptr != daemons.FindDaemon(daemon_name.c_str())) {
				daemons.RemoveDaemon(daemon_name.c_str());
			}
		}
	}

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

			// Setup and configure controllers for all daemons in
			// case the reconfig changed controller setup.  Start
			// any new daemons as well
		for (const auto &daemon_name: daemons.ordered_daemon_names) {
			class daemon *adaemon = daemons.FindDaemon(daemon_name.c_str());
			if ( adaemon == nullptr ) {
				dprintf( D_ALWAYS, "ERROR: Setup for daemon %s failed\n", daemon_name.c_str());
			}
			else if ( adaemon->SetupController() < 0 ) {
				dprintf( D_ALWAYS,
						"ERROR: Setup of controller for daemon %s failed\n",
						daemon_name.c_str());
				daemons.RemoveDaemon(daemon_name.c_str());
			}
			else if(!contains(old_daemon_list, daemon_name)) {
				daemons.StartDaemonHere(adaemon);
			}

		}

	} else {
		daemons.DaemonsOff();
	}
    // Invalide session if necessary
    daemonCore->invalidateSessionCache();
		// Re-register our timers, since their intervals might have
		// changed.
	daemons.StartTimers();
	daemons.UpdateCollector();
}

/*
 ** SIGQUIT callback from daemon-core Kill all daemons and go away.
 */
void
main_shutdown_fast()
{
#if 1
	do_basic_admin_command(MASTER_OFF_FAST);
#else
	invalidate_ads();
	
	MasterShuttingDown = TRUE;
	daemons.SetAllGoneAction( MASTER_EXIT );

	if( daemons.NumberOfChildren() == 0 ) {
		daemons.AllDaemonsGone();
	}

	daemons.CancelRestartTimers();
	daemons.StopFastAllDaemons();
#endif
}

/*
 ** SIGTERM Callback from daemon-core kill all daemons and go away. 
 */
void
main_shutdown_normal()
{
#if 1
	// peaceful or graceful depending on daemoncore variable
	do_basic_admin_command(MASTER_OFF);
#else
	// if we are doing peaceful tell the children, and set a timer to do the real shutdown
	// so the children have a chance to notice the messages
	//
	bool fTimer = false;
	if (daemonCore->GetPeacefulShutdown()) {
		int timeout = 5;
		if (daemons.SetPeacefulShutdown(timeout) > 0) {
			int tid = daemonCore->Register_Timer(timeout+1, 0,
							(TimerHandler)main_shutdown_graceful,
							"main_shutdown_graceful");
			if (tid == -1)
				dprintf( D_ALWAYS, "ERROR! Can't register DaemonCore timer!\n" );
			else
				fTimer = true;
		}
	}

	if ( ! fTimer) {
		main_shutdown_graceful();
	}
#endif
}

/*
 ** Cause job(s) to vacate, kill all daemons and go away.
 */
void
main_shutdown_graceful()
{
#if 1
	do_basic_admin_command(DC_OFF_GRACEFUL);
#else
	invalidate_ads();
	
	MasterShuttingDown = TRUE;
	daemons.SetAllGoneAction( MASTER_EXIT );

	if( daemons.NumberOfChildren() == 0 ) {
		daemons.AllDaemonsGone();
	}

	daemons.CancelRestartTimers();
	daemons.StopAllDaemons();
#endif
}

void
invalidate_ads() {
	ClassAd cmd_ad;
	SetMyTypeName( cmd_ad, QUERY_ADTYPE );
	cmd_ad.Assign(ATTR_TARGET_TYPE, MASTER_ADTYPE);
	
	std::string line;
	std::string escaped_name;
	char* default_name = MasterName ? ::strdup(MasterName) : NULL;
	if(!default_name) {
		default_name = default_daemon_name();
	}
	
	QuoteAdStringValue( default_name, escaped_name );
	formatstr( line, "( TARGET.%s == %s )", ATTR_NAME, escaped_name.c_str() );
	cmd_ad.AssignExpr( ATTR_REQUIREMENTS, line.c_str() );
	cmd_ad.Assign( ATTR_NAME, default_name );
	cmd_ad.Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr());
	daemonCore->sendUpdates( INVALIDATE_MASTER_ADS, &cmd_ad, NULL, false );
	free( default_name );
}

static const struct {
	StopStateT state;
	const char * name;
} aStopStateNames[] = {
	{ PEACEFUL, "PEACEFUL" },
	{ GRACEFUL, "GRACEFUL" },
	{ FAST,     "FAST" },
	{ KILL,     "KILL" },
	{ NONE,     "NEVER" }, { NONE, "NONE" }, { NONE, "NO" },
};

const char * StopStateToString(StopStateT state)
{
	// start by assuming that the index into the names table matches the state
	if ((int)state < (int)COUNTOF(aStopStateNames) && state == aStopStateNames[state].state) {
		return aStopStateNames[state].name;
	}
	// if the names table isn't packed and sorted, the brute force search it.
	for (auto aStopStateName : aStopStateNames) {
		if (aStopStateName.state == state) {
			return aStopStateName.name;
		}
	}
	return "??";
}

StopStateT StringToStopState(const char * psz)
{
	StopStateT state = (StopStateT)-1; // prime with -1 so we can detect bad input.
	for (int ii = 0; ii < (int)COUNTOF(aStopStateNames); ++ii) {
		if (MATCH == strcasecmp(psz, aStopStateNames[ii].name)) {
			state = aStopStateNames[ii].state;
			break;
		}
	}
	return state;
}


time_t
GetTimeStamp(char* file)
{
#ifdef WIN32
	ULARGE_INTEGER nanos;
	HANDLE hfile = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hfile == INVALID_HANDLE_VALUE) {
		return (time_t)-1;
	} else {
		BOOL fGotTime = GetFileTime(hfile, NULL, NULL, (FILETIME*)&nanos);
		CloseHandle(hfile);
		if ( ! fGotTime) {
			return (time_t)-1;
		}
	}
	// Windows filetimes are in 100 nanosecond intervals since January 1, 1601 (UTC)
	// NOTE: FAT records times on-disk in localtime, so daylight savings time can end up changing what is reported
	// the good news is NTFS stores UTC, so it doesn't have that problem.
	ULONGLONG nt_sec = nanos.QuadPart / (10 * 1000 * 1000); // convert to seconds,
	time_t epoch_sec = nt_sec - 11644473600; // convert from Windows 1600 epoch, to unix 1970 epoch
	return epoch_sec;
#else
	struct stat sbuf;
	
	if( stat(file, &sbuf) < 0 ) {
		return( (time_t) -1 );
	}
	
	return( sbuf.st_mtime );
#endif
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
run_preen_now()
{
	char *args=NULL;
	const char	*preen_base;
	ArgList arglist;
	std::string error_msg;

	dprintf(D_FULLDEBUG, "Entered run_preen.\n");
	if ( preen_pid > 0 ) {
		dprintf( D_ALWAYS, "WARNING: Preen is already running (pid %d), ignoring command to run Preen.\n", preen_pid );
		return FALSE;
	}

	if( FS_Preen == NULL ) {
		dprintf( D_ALWAYS, "WARNING: PREEN has no configured value, ignoring command to run Preen.\n" );
		return FALSE;
	}
	preen_base = condor_basename( FS_Preen );
	arglist.AppendArg(preen_base);

	args = param("PREEN_ARGS");
	if(!arglist.AppendArgsV1RawOrV2Quoted(args, error_msg)) {
		EXCEPT("ERROR: failed to parse preen args: %s",error_msg.c_str());
	}
	free(args);

	preen_pid = daemonCore->Create_Process(
					FS_Preen,		// program to exec
					arglist,   		// args
					PRIV_ROOT,		// privledge level
					1,				// which reaper ID to use; use default reaper
					FALSE );		// we do _not_ want this process to have a command port; PREEN is not a daemon core process
	dprintf( D_ALWAYS, "Preen pid is %d\n", preen_pid );
	return TRUE;
}

void
create_dirs_at_master_startup()
{
#ifndef WIN32
    //
    // Create the necessary directories.
    //

    typedef struct {
        const char * param;
        unsigned int uid;
        unsigned int gid;
        mode_t mode;
    } required_directories_t;

    uid_t u = get_condor_uid();
    gid_t g = get_condor_gid();

    uid_t r = 0;
    gid_t s = 0;
    if(! can_switch_ids()) {
        r= u;
        s= g;
    }

	// Note: until such time that we create the parent directories, 
	// be aware that the order of the below list is significant.  For instance,
	// we will create LOCAL_DIR before we create subdirectories typically
	// found in LOCAL_DIR like EXECUTE and SPOOL.
    std::vector<required_directories_t> required_directories {
        { "SEC_PASSWORD_DIRECTORY",         r, s, 00700 },
        { "SEC_TOKEN_SYSTEM_DIRECTORY",     r, s, 00700 },
        { "LOCAL_DIR",                      u, g, 00755 },
        { "EXECUTE",                        u, g, 00755 },
        { "SEC_CREDENTIAL_DIRECTORY_KRB",   r, s, 00755 },
        { "SEC_CREDENTIAL_DIRECTORY_OAUTH", r, g, 02770 },
        { "SPOOL",                          u, g, 00755 },
        { "LOCAL_UNIV_EXECUTE",             u, g, 00755 },
        { "LOCK",                           u, g, 00755 },
        { "LOCAL_DISK_LOCK_DIR",            u, g, 01777 },  // typically never defined
        { "LOG",                            u, g, 00755 },
        { "RUN",                            u, g, 00755 }
    };

    struct stat sbuf;
    for( const auto & dir : required_directories ) {
        std::string name;
        param( name, dir.param );
        // fprintf( stderr, "%s (%s) %u %u %o\n", dir.param, name.c_str(), dir.uid, dir.gid, dir.mode );
		if( name.empty() ) {
			continue;
		}
		TemporaryPrivSentry tps(PRIV_ROOT);
        if( stat( name.c_str(), &sbuf ) != 0 && errno == ENOENT ) {
			// TODO: should we be calling mkdir_and_parents_if_neeed() instead of mkdir() ?
            if( mkdir( name.c_str(), dir.mode ) == 0 ) {
                dummyGlobal = chown( name.c_str(), dir.uid, dir.gid );
                dummyGlobal = chmod( name.c_str(), dir.mode ); // Override umask
            }
        }
    }
#endif
	return;
}

// this is the preen timer callback
void run_preen(int /* tid */) { run_preen_now(); }

void
RestartMaster()
{
	daemons.RestartMaster();
}

void
main_pre_command_sock_init()
{

	// Create directories as needed... we need to do this as early as possible,
	// but after daemonCore initializes the uids (priv) code and config code.
	// So first thing in main_pre_command_sock_init() seems to be about the best spot.
	create_dirs_at_master_startup();

	/* Make sure we are the only copy of condor_master running */
	char*  p;
#ifndef WIN32
	std::string lock_file;

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
	dprintf (D_FULLDEBUG, "Attempting to lock %s.\n", lock_file.c_str() );
	lock_or_except( lock_file.c_str() );
	dprintf (D_FULLDEBUG, "Obtained lock on %s.\n", lock_file.c_str() );
#endif

	// Do any kernel tuning we've been configured to do.
	if( param_boolean( "ENABLE_KERNEL_TUNING", false ) ) {
#ifdef LINUX
		do_linux_kernel_tuning();
#endif
	}

#ifdef LINUX
	if (param_boolean("DISABLE_SETUID", false)) {
		prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	}
#endif
	// If using CREDENTIAL_DIRECTORY, blow away the CREDMON_COMPLETE file
	// to force the credmon to refresh everything and to prevent the schedd
	// from starting up until credentials are ready.
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	if (cred_dir) {
		credmon_clear_completion(credmon_type_KRB, cred_dir);
	}
	cred_dir.set(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
	if (cred_dir) {
		credmon_clear_completion(credmon_type_OAUTH, cred_dir);
	}

 	// in case a shared port address file got left behind by an
 	// unclean shutdown, clean it up now before we create our
 	// command socket to avoid confusion
	// Do so unconditionally, because the master will decide later (when
	// it's ready to start daemons) if it will be starting the shared
	// port daemon.
 	SharedPortServer::RemoveDeadAddressFile();

	// The master and its daemons may disagree on if they're using shared
	// port, so make sure everything's ready, just in case.
	//
	// FIXME: condor_preen doesn't look to know about "auto" directories.
	SharedPortEndpoint::InitializeDaemonSocketDir();
}

#ifdef WIN32
bool main_has_console() 
{
    DWORD displayMode;
    if (GetConsoleDisplayMode(&displayMode))
       return true;

        // if you need to debug service startup code
        // recompile with this code enabled, then attach the debugger
        // within 90 seconds of running "net start condor"
   #ifdef _DEBUG
    //for (int ii = 0; ii < 90*1000/500 && ! IsDebuggerPresent(); ++ii) { Sleep(500); } DebugBreak();
   #endif

    return false;
}
#endif

int
main( int argc, char **argv )
{
    // as of 8.9.7 daemon core defaults to foreground
    // for the master (and only the master) we change it to default to background
    dc_args_default_to_background(true);
#ifndef WIN32
	// tell daemon core that if we fork into the background
	// let the forked child decide when to allow the forked parent to exit
	dc_set_background_parent_mode(true);
#endif

    // parse args to see if we have been asked to run as a service.
    // services are started without a console, so if we have one
    // we can't possibly run as a service.
#ifdef WIN32
    bool has_console = main_has_console();
    bool is_daemon = dc_args_is_background(argc, argv);
#endif

	set_mySubSystem( "MASTER", true, SUBSYSTEM_TYPE_MASTER );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_normal;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;

#ifdef WIN32
    // if we have been asked to start as a daemon, on Windows
    // first try and start as a service, if that doesn't work try
    // to start as a background process. Note that we don't return
    // from the call to start_as_service() if the service successfully
    // started - just like dc_main().
    //
    NT_ServiceFlag = FALSE;
    if (is_daemon) {
       NT_ServiceFlag = TRUE;
	   DWORD err = start_as_service();
       if (err == 0x666) {
          // 0x666 is a special error code that tells us 
          // the Service Control Manager didn't create this process
          // so we should go ahead run as normal background 'daemon'
          NT_ServiceFlag = FALSE;
       } else {
          return (int)err;
       }
    }
#endif

	return dc_main( argc, argv );
}

void init_firewall_exceptions() {
#ifdef WIN32

	bool add_exception;
	char *master_image_path, *schedd_image_path, *startd_image_path,
		 *dagman_image_path, *negotiator_image_path, *collector_image_path, 
		 *starter_image_path, *shadow_image_path, *gridmanager_image_path, 
		 *gahp_image_path, *gahp_worker_image_path, *credd_image_path, 
		 *vmgahp_image_path, *kbdd_image_path, *bin_path;
	const char* dagman_exe = "condor_dagman.exe";

	WindowsFirewallHelper wfh;
	
	add_exception = param_boolean("ADD_WINDOWS_FIREWALL_EXCEPTION", NT_ServiceFlag);

	if ( add_exception == false ) {
		dprintf(D_FULLDEBUG, "ADD_WINDOWS_FIREWALL_EXCEPTION is false, skipping firewall configuration\n");
		return;
	}
	dprintf(D_ALWAYS, "Adding/Checking Windows firewall exceptions for all daemons\n");

	// We use getExecPath() here instead of param() since it's
	// possible the the Windows Service Control Manager
	// (SCM) points to one location for the master (which
	// is exec'd), while MASTER points to something else
	// (and ignored).
	
	master_image_path = getExecPath();
	if ( !master_image_path ) {	
		dprintf(D_ALWAYS, 
				"WARNING: Failed to get condor_master image path.\n"
				"Condor will not be excepted from the Windows firewall.\n");
		return;
	}

	// We want to add exceptions for the SCHEDD and the STARTD
	// so that (1) shadows can accept incoming connections on their 
	// command port and (2) so starters can do the same.

	schedd_image_path = param("SCHEDD");
	startd_image_path = param("STARTD");

	// And add exceptions for all the other daemons, since they very well
	// may need to open a listen port for mechanisms like CCB, or HTTPS
	negotiator_image_path = param("NEGOTIATOR");
	collector_image_path = param("COLLECTOR");
	starter_image_path = param("STARTER");
	shadow_image_path = param("SHADOW");
	gridmanager_image_path = param("GRIDMANAGER");
	gahp_image_path = param("CONDOR_GAHP");
	gahp_worker_image_path = param("CONDOR_GAHP_WORKER");
	credd_image_path = param("CREDD");
	kbdd_image_path = param("KBDD");
	vmgahp_image_path = param("VM_GAHP_SERVER");
	
	// We also want to add exceptions for the DAGMan we ship
	// with Condor:

	dagman_image_path = NULL; // make sure it's initialized.
	bin_path = param ( "BIN" );
	if ( bin_path ) {
		dagman_image_path = (char*) malloc (
			strlen ( bin_path ) + strlen ( dagman_exe ) + 2 );
		if ( dagman_image_path ) {
			sprintf ( dagman_image_path, "%s\\%s", bin_path, dagman_exe );
		}
		free ( bin_path );
	}

	// Insert the master
	if ( !SUCCEEDED(wfh.addTrusted(master_image_path)) ) {
		dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				master_image_path);
	}

	// Insert daemons needed on a central manager
	if ( (daemons.FindDaemon("NEGOTIATOR") != NULL) && negotiator_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted(negotiator_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				negotiator_image_path);
		}
	}
	if ( (daemons.FindDaemon("COLLECTOR") != NULL) && collector_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted(collector_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				collector_image_path);
		}
	}

	// Insert daemons needed on a submit node
	if ( (daemons.FindDaemon("SCHEDD") != NULL) && schedd_image_path ) {
		// put in schedd
		if ( !SUCCEEDED(wfh.addTrusted(schedd_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				schedd_image_path);
		}
		// put in shadow
		if ( shadow_image_path && !SUCCEEDED(wfh.addTrusted(shadow_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				shadow_image_path);
		}
		// put in gridmanager
		if ( gridmanager_image_path && !SUCCEEDED(wfh.addTrusted(gridmanager_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				gridmanager_image_path);
		}
		// put in condor gahp
		if ( gahp_image_path && !SUCCEEDED(wfh.addTrusted(gahp_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				gahp_image_path);
		}
		// put in condor worker gahp
		if ( gahp_worker_image_path && !SUCCEEDED(wfh.addTrusted(gahp_worker_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				gahp_worker_image_path);
		}
	}

	// Insert daemons needed on a execute node.
	// Note we include the starter and friends seperately, since the
	// starter could run on either execute or submit nodes (think 
	// local universe jobs).
	if ( (daemons.FindDaemon("STARTD") != NULL) && startd_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted(startd_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				startd_image_path);
		}
		if ( !SUCCEEDED(wfh.addTrusted(kbdd_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				kbdd_image_path);
		}
	}

	if ( starter_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted(starter_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				starter_image_path);
		}
	}

	if ( (daemons.FindDaemon("CREDD") != NULL) && credd_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted(credd_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				credd_image_path);
		}
	}

	if ( vmgahp_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted(vmgahp_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to the "
				"windows firewall exception list.\n",
				vmgahp_image_path);
		}
	}

	if ( dagman_image_path ) {
		if ( !SUCCEEDED(wfh.addTrusted (dagman_image_path)) ) {
			dprintf(D_FULLDEBUG, "WinFirewall: unable to add %s to "
				"the windows firewall exception list.\n",
				dagman_image_path);
		}
	}

	if ( master_image_path ) { free(master_image_path); }
	if ( schedd_image_path ) { free(schedd_image_path); }
	if ( startd_image_path ) { free(startd_image_path); }
	if ( dagman_image_path ) { free(dagman_image_path); }
	if ( negotiator_image_path ) { free(negotiator_image_path); }
	if ( collector_image_path ) { free(collector_image_path); }
	if ( shadow_image_path ) { free(shadow_image_path); }
	if ( gridmanager_image_path ) { free(gridmanager_image_path); }
	if ( gahp_image_path ) { free(gahp_image_path); }	
	if ( credd_image_path ) { free(credd_image_path); }	
	if ( vmgahp_image_path ) { free(vmgahp_image_path); }
	if ( kbdd_image_path ) { free(kbdd_image_path); }
#endif
}

	// We could care less about our arguments.
void time_skip_handler(void * /*data*/, int delta)
{
	dprintf(D_ALWAYS, "The system clocked jumped %d seconds unexpectedly.  Restarting all daemons\n", delta);
	do_basic_admin_command(RESTART);
}
