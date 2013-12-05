/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_open.h"
#include "condor_config.h"
#include "util_lib_proto.h"
#include "basename.h"
#include "ipv6_hostname.h"
#include "condor_version.h"
#include "limit.h"
#include "condor_email.h"
#include "sig_install.h"
#include "daemon.h"
#include "condor_debug.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "setenv.h"
#include "time_offset.h"
#include "subsystem_info.h"
#include "file_lock.h"
#include "directory.h"
#include "exit.h"
#include "param_functions.h"
#include "match_prefix.h"

#include "file_sql.h"
#include "file_xml.h"

#define _NO_EXTERN_DAEMON_CORE 1	
#include "condor_daemon_core.h"
#include "classad/classadCache.h"

#ifdef WIN32
#include "exception_handling.WINDOWS.h"
#endif
#if HAVE_EXT_COREDUMPER
#include "google/coredumper.h"
#endif

// Externs to Globals
extern DLL_IMPORT_MAGIC char **environ;


// External protos
void (*dc_main_init)(int argc, char *argv[]) = NULL;	// old main
void (*dc_main_config)() = NULL;
void (*dc_main_shutdown_fast)() = NULL;
void (*dc_main_shutdown_graceful)() = NULL;
void (*dc_main_pre_dc_init)(int argc, char *argv[]) = NULL;
void (*dc_main_pre_command_sock_init)() = NULL;

// Internal protos
void dc_reconfig();
void dc_config_auth();       // Configuring GSI (and maybe other) authentication related stuff
int handle_fetch_log_history(ReliSock *s, char *name);
int handle_fetch_log_history_dir(ReliSock *s, char *name);
int handle_fetch_log_history_purge(ReliSock *s);

// Globals
int		Foreground = 0;		// run in background by default
char *_condor_myServiceName;		// name of service on Win32 (argv[0] from SCM)
DaemonCore*	daemonCore;

// Statics (e.g. global only to this file)
static	char*	myFullName;		// set to the full path to ourselves
static const char*	myName;			// set to basename(argv[0])
static	char*	logDir = NULL;
static	char*	pidFile = NULL;
static	char*	addrFile[2] = { NULL, NULL };
static	char*	logAppend = NULL;

static int Termlog = 0;	//Replacing the Termlog in dprintf for daemons that use it

static char *core_dir = NULL;
static char *core_name = NULL;

int condor_main_argc;
char **condor_main_argv;
time_t daemon_stop_time;

/* ODBC object */
//extern ODBC *DBObj;

/* FILESQL object */
extern FILESQL *FILEObj;
/* FILEXML object */
extern FILEXML *XMLObj;

#ifdef WIN32
int line_where_service_stopped = 0;
#endif
bool	DynamicDirs = false;

// runfor is non-zero if the -r command-line option is specified. 
// It is specified to tell a daemon to kill itself after runfor minutes.
// Currently, it is only used for the master, and it is specified for 
// glide-in jobs that know that they can only run for a certain amount
// of time. Glide-in will tell the master to kill itself a minute before 
// it needs to quit, and it will kill the other daemons. 
// 
// The master will check this global variable and set an environment
// variable to inform the children how much time they have left to run. 
// This will mainly be used by the startd, which will advertise how much
// time it has left before it exits. This can be used by a job's requirements
// so that it can decide if it should run at a particular machine or not. 
int runfor = 0; //allow cmd line option to exit after *runfor* minutes

// This flag tells daemoncore whether to do the authorization initialization.
// It can be set to false by calling the DC_Skip_Auth_Init() function.
static bool doAuthInit = true;

// This flag tells daemoncore whether to do the core limit initialization.
// It can be set to false by calling the DC_Skip_Core_Init() function.
static bool doCoreInit = true;


#ifndef WIN32
// This function polls our parent process; if it is gone, shutdown.
void
check_parent( )
{
	if ( daemonCore->Is_Pid_Alive( daemonCore->getppid() ) == FALSE ) {
		// our parent is gone!
		dprintf(D_ALWAYS,
			"Our parent process (pid %d) went away; shutting down\n",
			daemonCore->getppid());
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}
}
#endif

// This function clears expired sessions from the cache
void
check_session_cache( )
{
	daemonCore->getSecMan()->invalidateExpiredCache();
}

bool global_dc_set_cookie(int len, unsigned char* data) {
	if (daemonCore) {
		return daemonCore->set_cookie(len, data);
	} else {
		return false;
	}
}

bool global_dc_get_cookie(int &len, unsigned char* &data) {
	if (daemonCore) {
		return daemonCore->get_cookie(len, data);
	} else {
		return false;
	}
}

void
handle_cookie_refresh( )
{
	unsigned char randomjunk[256];
	char symbols[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	for (int i = 0; i < 128; i++) {
		randomjunk[i] = symbols[rand() % 16];
	}
	
	// good ol null terminator
	randomjunk[127] = 0;

	global_dc_set_cookie (128, randomjunk);	
}

char const* global_dc_sinful() {
	if (daemonCore) {
		return daemonCore->InfoCommandSinfulString();
	} else {
		return NULL;
	}
}

void clean_files()
{
		// If we created a pid file, remove it.
	if( pidFile ) {
		if( unlink(pidFile) < 0 ) {
			dprintf( D_ALWAYS, 
					 "DaemonCore: ERROR: Can't delete pid file %s\n",
					 pidFile );
		} else {
			if( IsDebugVerbose( D_DAEMONCORE ) ) {
				dprintf( D_DAEMONCORE, "Removed pid file %s\n", pidFile );
			}
		}
	}

	for (int i=0; i<2; i++) {
		if( addrFile[i] ) {
			if( unlink(addrFile[i]) < 0 ) {
				dprintf( D_ALWAYS,
						 "DaemonCore: ERROR: Can't delete address file %s\n",
						 addrFile[i] );
			} else {
				if( IsDebugVerbose( D_DAEMONCORE ) ) {
					dprintf( D_DAEMONCORE, "Removed address file %s\n",
							 addrFile[i] );
				}
			}
				// Since we param()'ed for this, we need to free it now.
			free( addrFile[i] );
		}
	}
	
	if(daemonCore) {
		if( daemonCore->localAdFile ) {
			if( unlink(daemonCore->localAdFile) < 0 ) {
				dprintf( D_ALWAYS, 
						 "DaemonCore: ERROR: Can't delete classad file %s\n",
						 daemonCore->localAdFile );
			} else {
				if( IsDebugVerbose( D_DAEMONCORE ) ) {
					dprintf( D_DAEMONCORE, "Removed local classad file %s\n", 
							 daemonCore->localAdFile );
				}
			}
			free( daemonCore->localAdFile );
			daemonCore->localAdFile = NULL;
		}
	}

}


// All daemons call this function when they want daemonCore to really
// exit.  Put any daemon-wide shutdown code in here.   
void
DC_Exit( int status, const char *shutdown_program )
{
		// First, delete any files we might have created, like the
		// address file or the pid file.
	clean_files();

	if(FILEObj) {
		delete FILEObj;
		FILEObj = NULL;
	}
	if(XMLObj) {
		delete XMLObj;
		XMLObj = NULL;
	}


		// See if this daemon wants to be restarted (true by
		// default).  If so, use the given status.  Otherwise, use the
		// special code to tell our parent not to restart us.
	int exit_status;
	if (daemonCore == NULL || daemonCore->wantsRestart()) {
		exit_status = status;
	}
	else {
		exit_status = DAEMON_NO_RESTART;
	}

#ifndef WIN32
	// unregister our signal handlers in case some 3rd-party lib
	// was masking signals on us...no late arrivals
	install_sig_handler(SIGCHLD,SIG_DFL);
	install_sig_handler(SIGHUP,SIG_DFL);
	install_sig_handler(SIGTERM,SIG_DFL);
	install_sig_handler(SIGQUIT,SIG_DFL);
	install_sig_handler(SIGUSR1,SIG_DFL);
	install_sig_handler(SIGUSR2,SIG_DFL);
#endif /* ! WIN32 */

		// Now, delete the daemonCore object, since we allocated it. 
	unsigned long	pid = 0;
	if (daemonCore) {
		pid = daemonCore->getpid( );
		delete daemonCore;
		daemonCore = NULL;
	}

		// Free up the memory from the config hash table, too.
	clear_config();

		// and deallocate the memory from the passwd_cache (uids.C)
	delete_passwd_cache();

	if ( core_dir ) {
		free( core_dir );
		core_dir = NULL;
	}

	if (core_name) {
		free(core_name);
		core_name = NULL;
	}

		/*
		  Log a message.  We want to do this *AFTER* we delete the
		  daemonCore object and free up other memory, just to make
		  sure we don't hit an EXCEPT() or anything in there and end
		  up exiting with something else after we print this.  all the
		  dprintf() code has already initialized everything it needs
		  to know from the config file, so it's ok that we already
		  cleared out our config hashtable, too.  Derek 2004-11-23
		*/
	if ( shutdown_program ) {
#     if (HAVE_EXECL)
		dprintf( D_ALWAYS, "**** %s (%s_%s) pid %lu EXITING BY EXECING %s\n",
				 myName, myDistro->Get(), get_mySubSystem()->getName(), pid,
				 shutdown_program );
		priv_state p = set_root_priv( );
		int exec_status = execl( shutdown_program, shutdown_program, NULL );
		set_priv( p );
		dprintf( D_ALWAYS, "**** execl() FAILED %d %d %s\n",
				 exec_status, errno, strerror(errno) );
#     elif defined(WIN32)
		dprintf( D_ALWAYS,
				 "**** %s (%s_%s) pid %lu EXECING SHUTDOWN PROGRAM %s\n",
				 myName, myDistro->Get(), get_mySubSystem()->getName(), pid,
				 shutdown_program );
		priv_state p = set_root_priv( );
		int exec_status = execl( shutdown_program, shutdown_program, NULL );
		set_priv( p );
		if ( exec_status ) {
			dprintf( D_ALWAYS, "**** _execl() FAILED %d %d %s\n",
					 exec_status, errno, strerror(errno) );
		}
#     else
		dprintf( D_ALWAYS, "**** execl() not available on this system\n" );
#     endif
	}
	dprintf( D_ALWAYS, "**** %s (%s_%s) pid %lu EXITING WITH STATUS %d\n",
			 myName, myDistro->Get(), get_mySubSystem()->getName(), pid,
			 exit_status );

		// Finally, exit with the appropriate status.
	exit( exit_status );
}


void
DC_Skip_Auth_Init()
{
	doAuthInit = false;
}

void
DC_Skip_Core_Init()
{
	doCoreInit = false;
}

static void
kill_daemon_ad_file()
{
	MyString param_name;
	param_name.formatstr( "%s_DAEMON_AD_FILE", get_mySubSystem()->getName() );
	char *ad_file = param(param_name.Value());
	if( !ad_file ) {
		return;
	}

	MSC_SUPPRESS_WARNING_FOREVER(6031) // return value of unlink ignored.
	unlink(ad_file);

	free(ad_file);
}

void
drop_addr_file()
{
	FILE	*ADDR_FILE;
	char	addr_file[100];
	const char* addr[2];

	// Fill in addrFile[0] and addr[0] with info about regular command port
	sprintf( addr_file, "%s_ADDRESS_FILE", get_mySubSystem()->getName() );
	if( addrFile[0] ) {
		free( addrFile[0] );
	}
	addrFile[0] = param( addr_file );
		// Always prefer the local, private address if possible.
	addr[0] = daemonCore->privateNetworkIpAddr();
	if (!addr[0]) {
			// And if not, fall back to the public.
		addr[0] = daemonCore->publicNetworkIpAddr();
	}

	// Fill in addrFile[1] and addr[1] with info about superuser command port
	sprintf( addr_file, "%s_SUPER_ADDRESS_FILE", get_mySubSystem()->getName() );
	if( addrFile[1] ) {
		free( addrFile[1] );
	}
	addrFile[1] = param( addr_file );
	addr[1] = daemonCore->superUserNetworkIpAddr();

	for (int i=0; i<2; i++) {
		if( addrFile[i] ) {
			MyString newAddrFile;
			newAddrFile.formatstr("%s.new",addrFile[i]);
			if( (ADDR_FILE = safe_fopen_wrapper_follow(newAddrFile.Value(), "w")) ) {
				fprintf( ADDR_FILE, "%s\n", addr[i] );
				fprintf( ADDR_FILE, "%s\n", CondorVersion() );
				fprintf( ADDR_FILE, "%s\n", CondorPlatform() );
				fclose( ADDR_FILE );
				if( rotate_file(newAddrFile.Value(),addrFile[i])!=0 ) {
					dprintf( D_ALWAYS,
							 "DaemonCore: ERROR: failed to rotate %s to %s\n",
							 newAddrFile.Value(),
							 addrFile[i]);
				}
			} else {
				dprintf( D_ALWAYS,
						 "DaemonCore: ERROR: Can't open address file %s\n",
						 newAddrFile.Value() );
			}
		}
	}	// end of for loop
}

void
drop_pid_file() 
{
	FILE	*PID_FILE;

	if( !pidFile ) {
			// There's no file, just return
		return;
	}

	if( (PID_FILE = safe_fopen_wrapper_follow(pidFile, "w")) ) {
		fprintf( PID_FILE, "%lu\n", 
				 (unsigned long)daemonCore->getpid() ); 
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
	unsigned long tmp_ul_int = 0;
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
	if( (PID_FILE = safe_fopen_wrapper_follow(pidFile, "r")) ) {
		if (fscanf( PID_FILE, "%lu", &tmp_ul_int ) != 1) {
			fprintf( stderr, "DaemonCore: ERROR: fscanf failed processing pid file %s\n",
					 pidFile );
			exit( 1 );
		}
		pid = (pid_t)tmp_ul_int;
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
					 (unsigned long)pid );
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
				 (unsigned long)pid, pidFile );	
		exit( 1 );
	}
#endif  // of ifndef WIN32
}


// Create the directory we were given, with extra error checking.  
void
make_dir( const char* logdir )
{
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
	struct stat stats;
	if( stat(logdir, &stats) >= 0 ) {
		if( ! S_ISDIR(stats.st_mode) ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: %s exists and is not a directory.\n", 
					 logdir );
			exit( 1 );
		}
	} else {
		if( mkdir(logdir, mode) < 0 ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: can't create directory %s\n", 
					 logdir );
			fprintf( stderr, 
					 "\terrno: %d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		}
	}
}


// Set our log directory in our config hash table, and create it.
void
set_log_dir()
{
	if( !logDir ) {
		return;
	}
	config_insert( "LOG", logDir );
	make_dir( logDir );
}


void
handle_log_append( char* append_str )
{
	if( ! append_str ) {
		return;
	}
	char *tmp1, *tmp2;
	char buf[100];
	sprintf( buf, "%s_LOG", get_mySubSystem()->getName() );
	if( !(tmp1 = param(buf)) ) { 
		EXCEPT( "%s not defined!", buf );
	}
	tmp2 = (char*)malloc( (strlen(tmp1) + strlen(append_str) + 2)
						  * sizeof(char) );
	if( !tmp2 ) {	
		EXCEPT( "Out of memory!" );
	}
	sprintf( tmp2, "%s.%s", tmp1, append_str );
	config_insert( buf, tmp2 );
	free( tmp1 );
	free( tmp2 );
}


void
dc_touch_log_file( )
{
	dprintf_touch_log();

	daemonCore->Register_Timer( param_integer( "TOUCH_LOG_INTERVAL", 60 ),
				dc_touch_log_file, "dc_touch_log_file" );
}

void
dc_touch_lock_files( )
{
	priv_state p;

	// For any outstanding lock objects that are associated with the real lock
	// file, update their timestamps. Do it as Condor, and ignore files that we
	// don't have permissions to alter. This allows things like the Instance
	// lock to be updated, but doesn't update things like the job event log.

	// do this here to make the priv switching operations fast in the 
	// FileLock Object.
	p = set_condor_priv();

	FileLock::updateAllLockTimestamps();

	set_priv(p);

	// reset the timer for next incarnation of the update.
	daemonCore->Register_Timer(
		param_integer("LOCK_FILE_UPDATE_INTERVAL", 3600 * 8, 60, INT_MAX),
		dc_touch_lock_files, "dc_touch_lock_files" );
}


void
set_dynamic_dir( const char* param_name, const char* append_str )
{
	char* val;
	MyString newdir;

	val = param( param_name );
	if( ! val ) {
			// nothing to do
		return;
	}

		// First, create the new name.
	newdir.formatstr( "%s.%s", val, append_str );
	
		// Next, try to create the given directory, if it doesn't
		// already exist.
	make_dir( newdir.Value() );

		// Now, set our own config hashtable entry so we start using
		// this new directory.
	config_insert( param_name, newdir.Value() );

	// Finally, insert the _condor_<param_name> environment
	// variable, so our children get the right configuration.
	MyString env_str( "_" );
	env_str += myDistro->Get();
	env_str += "_";
	env_str += param_name;
	env_str += "=";
	env_str += newdir;
	char *env_cstr = strdup( env_str.Value() );
	if( SetEnv(env_cstr) != TRUE ) {
		fprintf( stderr, "ERROR: Can't add %s to the environment!\n", 
				 env_cstr );
		exit( 4 );
	}
}


void
handle_dynamic_dirs()
{
		// We want the log, spool and execute directory of ourselves
		// and our children to have our pid appended to them.  If the
		// directories aren't there, we should create them, as Condor,
		// if possible.
	if( ! DynamicDirs ) {
		return;
	}
	int mypid = daemonCore->getpid();
	char buf[256];
	sprintf( buf, "%s-%d", get_local_ipaddr().to_ip_string().Value(), mypid );

	set_dynamic_dir( "LOG", buf );
	set_dynamic_dir( "SPOOL", buf );
	set_dynamic_dir( "EXECUTE", buf );

		// Final, evil hack.  Set the _condor_STARTD_NAME environment
		// variable, so that the startd will have a unique name. 
	sprintf( buf, "_%s_STARTD_NAME=%d", myDistro->Get(), mypid );
	char* env_str = strdup( buf );
	if( SetEnv(env_str) != TRUE ) {
		fprintf( stderr, "ERROR: Can't add %s to the environment!\n", 
				 env_str );
		exit( 4 );
	}
}

#if HAVE_EXT_COREDUMPER
void
linux_sig_coredump(int signum)
{
	struct sigaction sa;
	static bool down = false;

	/* It turns out that the abort() call will unblock the sig
		abort signal and allow the handler to be called again. So,
		in a real world case, which led me to write this test,
		glibc decided something was wrong and called abort(),
		then, in this signal handler, we tickled the exact
		thing glibc didn't like in the first place and so it
		called abort() again, leading back to this handler. A
		segmentation fault happened finally when the stack was
		exhausted. This guard is here to prevent that type
		of scenario from happening again with this handler.
		This fixes ticket number #183 in the condor-wiki. NOTE:
		We never set down to false again, because this handler
		is meant to exit() and not return. */

	if (down == true) {
		return;
	}
	down = true;

	dprintf_dump_stack();

	// Just in case we're running as condor or a user.
	setuid(0);
	setgid(0);

	if (core_dir != NULL) {
		if (chdir(core_dir)) {
			dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n", core_dir, strerror(errno));
		}
	}

	WriteCoreDump(core_name ? core_name : "core");

	// It would be a good idea to actually terminate for the same reason.
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(signum, &sa, NULL);
	sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

	raise(signum);

	// If for whatever reason the second raise doesn't kill us properly, 
	// we shall exit with a non-zero code so if anything depends on us,
	// at least they know there was a problem.
	exit(1);
}
#endif


void
install_core_dump_handler()
{
#if HAVE_EXT_COREDUMPER
		// We only need to do this if we're root.
		if( getuid() == 0) {
			dprintf(D_FULLDEBUG, "Running as root.  Enabling specialized core dump routines\n");
			sigset_t fullset;
			sigfillset( &fullset );
			install_sig_handler_with_mask(SIGSEGV, &fullset, linux_sig_coredump);
			install_sig_handler_with_mask(SIGABRT, &fullset, linux_sig_coredump);
			install_sig_handler_with_mask(SIGILL, &fullset, linux_sig_coredump);
			install_sig_handler_with_mask(SIGFPE, &fullset, linux_sig_coredump);
			install_sig_handler_with_mask(SIGBUS, &fullset, linux_sig_coredump);
		}
#	endif // of ifdef HAVE_EXT_COREDUMPER
}

void
drop_core_in_log( void )
{
	// chdir to the LOG directory so that if we dump a core
	// it will go there.
	// and on Win32, tell our ExceptionHandler class to drop
	// its pseudo-core file to the LOG directory as well.
	char* ptmp = param("LOG");
	if ( ptmp ) {
		if ( chdir(ptmp) < 0 ) {
#ifdef WIN32
			if (MATCH == strcmpi(get_mySubSystem()->getName(), "KBDD")) {
				dprintf (D_FULLDEBUG, "chdir() to LOG directory failed for KBDD, "
					     "cannot drop core in LOG dir\n");
				free(ptmp);
				return;
			}
#endif
    	EXCEPT("cannot chdir to dir <%s>",ptmp);
		}
	} else {
		dprintf( D_FULLDEBUG, 
				 "No LOG directory specified in config file(s), "
				 "not calling chdir()\n" );
		return;
	}

	if ( core_dir ) {
		free( core_dir );
		core_dir = NULL;
	}
	core_dir = strdup(ptmp);

	// get the name for core files, we need to access this pointer
	// later in the exception handlers, so keep it around in a module static
	// the core dump handler is expected to deal with the case of core_name == NULL
	// by using a default name.
	if (core_name) {
		free(core_name);
		core_name = NULL;
	}
	core_name = param("CORE_FILE_NAME");

	// in some case we need to hook up our own handler to generate
	// core files.
	install_core_dump_handler();

#ifdef WIN32
	{
		// give our Win32 exception handler a filename for the core file
		MyString pseudoCoreFileName;
		formatstr(pseudoCoreFileName,"%s\\%s", ptmp, core_name ? core_name : "core.WIN32");
		g_ExceptionHandler.SetLogFileName(pseudoCoreFileName.c_str());

		// set the path where our Win32 exception handler can find
		// debug symbols
		char *binpath = param("BIN");
		if ( binpath ) {
			SetEnv( "_NT_SYMBOL_PATH", binpath );
			free(binpath);
		}

		// give the handler our pid
		g_ExceptionHandler.SetPID ( daemonCore->getpid () );
	}
#endif
	free(ptmp);
}


// See if we should set the limits on core files.  If the parameter is
// defined, do what it says.  Otherwise, do nothing.
// On NT, if CREATE_CORE_FILES is False, then we will use the
// default NT exception handler which brings up the "Abort or Debug"
// dialog box, etc.  Otherwise, we will just write out a core file
// "summary" in the log directory and not display the dialog.
void
check_core_files()
{
	bool want_set_error_mode = param_boolean_crufty("CREATE_CORE_FILES", true);

#ifndef WIN32
	if( want_set_error_mode ) {
		limit( RLIMIT_CORE, RLIM_INFINITY, CONDOR_SOFT_LIMIT,"max core size" );
	} else {
		limit( RLIMIT_CORE, 0, CONDOR_SOFT_LIMIT,"max core size" );
	}
#endif

#ifdef WIN32
		// Call SetErrorMode so that Win32 "critical errors" and such
		// do not open up a dialog window!
	if ( want_set_error_mode ) {
		::SetErrorMode( 
			SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
		g_ExceptionHandler.TurnOn();
	} else {
		::SetErrorMode( 0 );
		g_ExceptionHandler.TurnOff();
	}
#endif

}


static int
handle_off_fast( Service*, int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_fast: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
	}
	return TRUE;
}

	
static int
handle_off_graceful( Service*, int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_graceful: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}
	return TRUE;
}

class SigtermContinue {
public:
	static bool should_sigterm_continue() { return should_continue; };
	static void sigterm_should_not_continue() { should_continue = false; }
	static void sigterm_should_continue() { should_continue = true; }
private:
	static bool should_continue;
};

bool SigtermContinue::should_continue = true;


static int
handle_off_force( Service*, int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_force: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->SetPeacefulShutdown( false );
		SigtermContinue::sigterm_should_continue();
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}
	return TRUE;
}

static int
handle_off_peaceful( Service*, int, Stream* stream)
{
	// Peaceful shutdown is the same as graceful, except
	// there is no timeout waiting for things to finish.

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_peaceful: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->SetPeacefulShutdown(true);
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}
	return TRUE;
}

static int
handle_set_peaceful_shutdown( Service*, int, Stream* stream)
{
	// If the master could send peaceful shutdown signals, it would
	// not be necessary to have a message for turning on the peaceful
	// shutdown toggle.  Since the master only sends fast and graceful
	// shutdown signals, condor_off is responsible for first turning
	// on peaceful shutdown in appropriate daemons.

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_set_peaceful_shutdown: failed to read end of message\n");
		return FALSE;
	}
	daemonCore->SetPeacefulShutdown(true);
	return TRUE;
}

static int
handle_set_force_shutdown( Service*, int, Stream* stream)
{
	// If the master could send peaceful shutdown signals, it would
	// not be necessary to have a message for turning on the peaceful
	// shutdown toggle.  Since the master only sends fast and graceful
	// shutdown signals, condor_off is responsible for first turning
	// on peaceful shutdown in appropriate daemons.

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_set_force_shutdown: failed to read end of message\n");
		return FALSE;
	}
	daemonCore->SetPeacefulShutdown( false );
	SigtermContinue::sigterm_should_continue();
	return TRUE;
}


static int
handle_reconfig( Service*, int /* cmd */, Stream* stream )
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_reconfig: failed to read end of message\n");
		return FALSE;
	}
	if (!daemonCore->GetDelayReconfig()) {
		dc_reconfig();
	} else {
        dprintf(D_FULLDEBUG, "Delaying reconfig.\n");
 		daemonCore->SetNeedReconfig(true);
	}
	return TRUE;
}

int
handle_fetch_log( Service *, int, ReliSock *stream )
{
	char *name = NULL;
	int  total_bytes = 0;
	int result;
	int type = -1;

	if( ! stream->code(type) ||
		! stream->code(name) || 
		! stream->end_of_message()) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: can't read log request\n" );
		free( name );
		return FALSE;
	}

	stream->encode();

	switch (type) {
		case DC_FETCH_LOG_TYPE_PLAIN:
			break; // handled below
		case DC_FETCH_LOG_TYPE_HISTORY:
			return handle_fetch_log_history(stream, name);
		case DC_FETCH_LOG_TYPE_HISTORY_DIR:
			return handle_fetch_log_history_dir(stream, name);
		case DC_FETCH_LOG_TYPE_HISTORY_PURGE:
			free(name);
			return handle_fetch_log_history_purge(stream);
		default:
			dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: I don't know about log type %d!\n",type);
			result = DC_FETCH_LOG_RESULT_BAD_TYPE;
			stream->code(result);
			stream->end_of_message();
			free(name);
			return FALSE;
	}

	char *pname = (char*)malloc (strlen(name) + 5);
	char *ext = strchr(name,'.');

	//If there is a dot in the name, it is of the form "<SUBSYS>.<ext>"
	//Otherwise, it is "<SUBSYS>".  The file extension is used to
	//handle such things as "StarterLog.slot1" and "StarterLog.cod"

	if(ext) {
		strncpy(pname, name, ext-name);
		pname[ext-name] = '\0';
	}
	else {
		strcpy(pname, name);
	}

	strcat (pname, "_LOG");

	char *filename = param(pname);
	if(!filename) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: no parameter named %s\n",pname);
		result = DC_FETCH_LOG_RESULT_NO_NAME;
		stream->code(result);
		stream->end_of_message();
        free(pname);
        free(name);
		return FALSE;
	}

	MyString full_filename = filename;
	if(ext) {
		full_filename += ext;

		if( strchr(ext,DIR_DELIM_CHAR) ) {
			dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: invalid file extension specified by user: ext=%s, filename=%s\n",ext,full_filename.Value() );
			free(pname);
			return FALSE;
		}
	}

	int fd = safe_open_wrapper_follow(full_filename.Value(),O_RDONLY);
	if(fd<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: can't open file %s\n",full_filename.Value());
		result = DC_FETCH_LOG_RESULT_CANT_OPEN;
		stream->code(result);
		stream->end_of_message();
        free(filename);
        free(pname);
        free(name);
		return FALSE;
	}

	result = DC_FETCH_LOG_RESULT_SUCCESS;
	stream->code(result);

	filesize_t size;
	stream->put_file(&size, fd);
	total_bytes += size;

	stream->end_of_message();

	if(total_bytes<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: couldn't send all data!\n");
	}

	close(fd);
	free(filename);
	free(pname);
	free(name);

	return total_bytes>=0;
}

int
handle_fetch_log_history(ReliSock *stream, char *name) {
	int result = DC_FETCH_LOG_RESULT_BAD_TYPE;

	const char *history_file_param = "HISTORY";
	if (strcmp(name, "STARTD_HISTORY") == 0) {
		history_file_param = "STARTD_HISTORY";
	}

	free(name);
	char *history_file = param(history_file_param);

	if (!history_file) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history: no parameter named %s\n", history_file_param);
		stream->code(result);
		stream->end_of_message();
		return FALSE;
	}
	int fd = safe_open_wrapper_follow(history_file,O_RDONLY);
	free(history_file);
	if(fd<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history: can't open history file\n");
		result = DC_FETCH_LOG_RESULT_CANT_OPEN;
		stream->code(result);
		stream->end_of_message();
		return FALSE;
	}

	result = DC_FETCH_LOG_RESULT_SUCCESS;
	stream->code(result);

	filesize_t size;
	stream->put_file(&size, fd);

	stream->end_of_message();

	if(size<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history: couldn't send all data!\n");
	}

	close(fd);
	return TRUE;
}

int
handle_fetch_log_history_dir(ReliSock *stream, char *paramName) {
	int result = DC_FETCH_LOG_RESULT_BAD_TYPE;

	free(paramName);
	char *dirName = param("STARTD.PER_JOB_HISTORY_DIR"); 
	if (!dirName) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: no parameter named PER_JOB\n");
		stream->code(result);
		stream->end_of_message();
		return FALSE;
	}

	Directory d(dirName);
	const char *filename;
	int one=1;
	int zero=0;
	while ((filename = d.Next())) {
		stream->code(one); // more data
		stream->put(filename);
		MyString fullPath(dirName);
		fullPath += "/";
		fullPath += filename;
		int fd = safe_open_wrapper_follow(fullPath.Value(),O_RDONLY);
		if (fd >= 0) {
			filesize_t size;
			stream->put_file(&size, fd);
			close(fd);
		}
	}

	free(dirName);

	stream->code(zero); // no more data
	stream->end_of_message();
	return 0;
}

int
handle_fetch_log_history_purge(ReliSock *s) {

	int result = 0;
	time_t cutoff = 0;
	s->code(cutoff);
	s->end_of_message();

	s->encode();

	char *dirName = param("STARTD.PER_JOB_HISTORY_DIR"); 
	if (!dirName) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: no parameter named PER_JOB\n");
		s->code(result);
		s->end_of_message();
		return FALSE;
	}

	Directory d(dirName);

	result = 1;
	while (d.Next()) {
		time_t last = d.GetModifyTime();
		if (last < cutoff) {
			d.Remove_Current_File();
		}
	}

    free(dirName);

    s->code(result); // no more data
    s->end_of_message();
    return 0;
}


int
handle_nop( Service*, int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_FULLDEBUG, "handle_nop: failed to read end of message\n");
		return FALSE;
	}
	return TRUE;
}


int
handle_invalidate_key( Service*, int, Stream* stream)
{
    int result = 0;
	char *key_id = NULL;

	stream->decode();
	if ( ! stream->code(key_id) ) {
		dprintf ( D_ALWAYS, "DC_INVALIDATE_KEY: unable to receive key id!.\n");
		return FALSE;
	}

	if ( ! stream->end_of_message() ) {
		dprintf ( D_ALWAYS, "DC_INVALIDATE_KEY: unable to receive EOM on key %s.\n", key_id);
		return FALSE;
	}

    result = daemonCore->getSecMan()->invalidateKey(key_id);
    free(key_id);
    return result;
}

int
handle_config_val( Service*, int idCmd, Stream* stream ) 
{
	char *param_name = NULL, *tmp;

	stream->decode();

	if ( ! stream->code(param_name)) {
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

	// DC_CONFIG_VAL command has extended behavior not shared by CONFIG_VAL.
	// if param name begins with ? then it is a command name rather than a param name.
	//   ?names[:pattern] - return a set of strings containing the names of all paramters in the param table.
	if ((DC_CONFIG_VAL == idCmd) && ('?' == param_name[0])) {
		int retval = TRUE; // assume success

		const char * pcolon;
		if (is_arg_colon_prefix(param_name, "?names", &pcolon, -1)) {
			const char * restr = ".*";
			if (pcolon) { restr = ++pcolon; }
			Regex re; int err = 0; const char * pszMsg = 0;

			if ( ! re.compile(restr, &pszMsg, &err, PCRE_CASELESS)) {
				dprintf( D_ALWAYS, "Can't compile regex for DC_CONFIG_VAL ?names query\n" );
				MyString errmsg; errmsg.formatstr("!error:regex:%d: %s", err, pszMsg ? pszMsg : "");
				stream->code(errmsg);
				retval = FALSE;
			} else {
				std::vector<std::string> names;
				if (param_names_matching(re, names)) {
					for (int ii = 0; ii < (int)names.size(); ++ii) {
						if ( ! stream->code(names[ii])) {
							dprintf( D_ALWAYS, "Can't send ?names reply for DC_CONFIG_VAL\n" );
							retval = FALSE;
							break;
						}
					}
				} else {
					MyString empty("");
					if ( ! stream->code(empty)) {
						dprintf( D_ALWAYS, "Can't send ?names reply for DC_CONFIG_VAL\n" );
						retval = FALSE;
					}
				}

				if (retval && ! stream->end_of_message() ) {
					dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
					retval = FALSE;
				}
				names.clear();
			}
		} else if (is_arg_prefix(param_name, "?stats", -1)) {

			struct _macro_stats stats;
			int cQueries = get_config_stats(&stats);
			// for backward compatility, we have to put a single string on the wire
			// before we can put the stats classad.
			MyString queries;
			queries.formatstr("%d", cQueries);
			if ( ! stream->code(queries)) {
				dprintf(D_ALWAYS, "Can't send param stats for DC_CONFIG_VAL\n");
				retval = false;
			} else {
				ClassAd ad; ad.Clear(); // remove time()
				ad.Assign("Macros", stats.cEntries);
				ad.Assign("Used", stats.cUsed);
				ad.Assign("Referenced", stats.cReferenced);
				ad.Assign("Files", stats.cFiles);
				ad.Assign("StringBytes", stats.cbStrings);
				ad.Assign("TablesBytes", stats.cbTables);
				ad.Assign("Sorted", stats.cSorted);
				if ( ! putClassAd(stream, ad)) {
					dprintf(D_ALWAYS, "Can't send param stats ad for DC_CONFIG_VAL\n");
					retval = false;
				}
			}
			if (retval && ! stream->end_of_message()) {
				retval = false;
			}

		} else { // unrecognised ?command

			MyString errmsg; errmsg.formatstr("!error:unsup:1: '%s' is not supported", param_name);
			if ( ! stream->code(errmsg)) retval = FALSE;
			if (retval && ! stream->end_of_message()) retval = FALSE;
		}

		free (param_name);
		return retval;
	}

	// DC_CONFIG_VAL command has extended behavior not shared by CONFIG_VAL.
	// in addition to returning. the param() value, it returns consecutive strings containing
	//   <NAME_USED> = <raw_value>
	//   <filename>[, line <num>]
	//   <default_value>
	//note: ", line <num>" is present only when the line number is meaningful
	//and "<default_value>" may be NULL
	if (idCmd == DC_CONFIG_VAL) {
		int retval = TRUE; // assume success

#if 1
		MyString name_used, value;
		const char * def_val = NULL;
		const MACRO_META * pmet = NULL;
		const char * subsys = get_mySubSystem()->getName();
		const char * local_name  = get_mySubSystem()->getLocalName();
		const char * val = param_get_info(param_name, subsys, local_name, name_used, &def_val, &pmet);
		if (name_used.empty()) {
			dprintf( D_FULLDEBUG,
					 "Got DC_CONFIG_VAL request for unknown parameter (%s)\n",
					 param_name );
			// send a NULL to indicate undefined. (val is NULL here)
			if( ! stream->code(const_cast<char*&>(val)) ) {
				dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
				retval = FALSE;
			}
		} else {

			dprintf(D_CONFIG | D_FULLDEBUG, "DC_CONFIG_VAL(%s) def: %s = %s\n", param_name, name_used.Value(), def_val ? def_val : "NULL");

			if (val) { tmp = expand_param(val, subsys, 0); } else { tmp = NULL; }
			if( ! stream->code(tmp) ) {
				dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
				retval = FALSE;
			}
			if (tmp) free(tmp); tmp = NULL;

			name_used.upper_case();
			name_used += " = ";
			if (val) name_used += val;
			if ( ! stream->code(name_used)) {
				dprintf( D_ALWAYS, "Can't send raw reply for DC_CONFIG_VAL\n" );
			}
			param_get_location(pmet, value);
			if ( ! stream->code(value)) {
				dprintf( D_ALWAYS, "Can't send filename reply for DC_CONFIG_VAL\n" );
			}
			if ( ! stream->code(const_cast<char*&>(def_val))) {
				dprintf( D_ALWAYS, "Can't send default reply for DC_CONFIG_VAL\n" );
			}
			if (pmet->ref_count) { value.formatstr("%d / %d", pmet->use_count, pmet->ref_count);
			} else  { value.formatstr("%d", pmet->use_count); }
			if ( ! stream->code(value)) {
				dprintf( D_ALWAYS, "Can't send use count reply for DC_CONFIG_VAL\n" );
			}
		}
#else
		MyString value, name_used, filename;
		int line_number, use_count, ref_count;
		const char * subsys = get_mySubSystem()->getName();
		const char * local_name  = get_mySubSystem()->getLocalName();
		if (subsys && !subsys[0]) subsys = NULL;
		if (local_name && !local_name[0]) local_name = NULL;
		const char * def_val = NULL;
		const char * val = param_get_info(param_name, subsys, local_name,
							&def_val, name_used, use_count, ref_count, filename, line_number);
		if (val || ! filename.empty()) {
			dprintf(D_CONFIG | D_FULLDEBUG, "DC_CONFIG_VAL(%s) def: %s = %s\n", param_name, name_used.Value(), def_val ? def_val : "NULL");

			if (val) { tmp = expand_param(val, subsys, 0); } else { tmp = NULL; }
			if( ! stream->code(tmp) ) {
				dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
				retval = FALSE;
			}
			if (tmp) free(tmp); tmp = NULL;

			name_used.upper_case();
			name_used += " = ";
			if (val) name_used += val;
			if ( ! stream->code(name_used)) {
				dprintf( D_ALWAYS, "Can't send raw reply for DC_CONFIG_VAL\n" );
			}
			if (line_number >= 0)
				filename.formatstr_cat(", line %d", line_number);
			if ( ! stream->code(filename)) {
				dprintf( D_ALWAYS, "Can't send filename reply for DC_CONFIG_VAL\n" );
			}
			if ( ! stream->code(const_cast<char*&>(def_val))) {
				dprintf( D_ALWAYS, "Can't send default reply for DC_CONFIG_VAL\n" );
			}
			if (ref_count) { value.formatstr("%d / %d", use_count, ref_count);
			} else  { value.formatstr("%d", use_count); }
			if ( ! stream->code(value)) {
				dprintf( D_ALWAYS, "Can't send use count reply for DC_CONFIG_VAL\n" );
			}
		} else {
			dprintf( D_FULLDEBUG,
					 "Got DC_CONFIG_VAL request for unknown parameter (%s)\n",
					 param_name );
			// send a NULL to indicate undefined. (val is NULL here)
			if( ! stream->code(const_cast<char*&>(val)) ) {
				dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
				retval = FALSE;
			}
		}
#endif
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
			retval = FALSE;
		}
		free (param_name);
		return retval;
	}

	tmp = param( param_name );
	if( ! tmp ) {
		dprintf( D_FULLDEBUG, 
				 "Got CONFIG_VAL request for unknown parameter (%s)\n", 
				 param_name );
		free( param_name );
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
			free( param_name );
			free( tmp );
			return FALSE;
		}

		free( param_name );
		free( tmp );
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for CONFIG_VAL\n" );
			return FALSE;
		}
	}
	return TRUE;
}


int
handle_config( Service *, int cmd, Stream *stream )
{
	char *admin = NULL, *config = NULL;
	char *to_check = NULL;
	int rval = 0;
	bool failed = false;

	stream->decode();

	if ( ! stream->code(admin) ) {
		dprintf( D_ALWAYS, "Can't read admin string\n" );
		free( admin );
		return FALSE;
	}

	if ( ! stream->code(config) ) {
		dprintf( D_ALWAYS, "Can't read configuration string\n" );
		free( admin );
		free( config );
		return FALSE;
	}

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_config: failed to read end of message\n");
		return FALSE;
	}
	if( config && config[0] ) {
		to_check = parse_param_name_from_config(config);
	} else {
		to_check = strdup(admin);
	}
	if (!is_valid_param_name(to_check)) {
		dprintf( D_ALWAYS, "Rejecting attempt to set param with invalid name (%s)\n", to_check);
		free(admin); free(config);
		rval = -1;
		failed = true;
	} else if( ! daemonCore->CheckConfigSecurity(to_check, (Sock*)stream) ) {
			// This request is insecure, so don't try to do anything
			// with it.  We can't return yet, since we want to send
			// back an rval indicating the error.
		free( admin );
		free( config );
		rval = -1;
		failed = true;
	} 
	free(to_check);

		// If we haven't hit an error yet, try to process the command  
	if( ! failed ) {
		switch(cmd) {
		case DC_CONFIG_PERSIST:
			rval = set_persistent_config(admin, config);
				// set_persistent_config will free admin and config
				// when appropriate  
			break;
		case DC_CONFIG_RUNTIME:
			rval = set_runtime_config(admin, config);
				// set_runtime_config will free admin and config when
				// appropriate 
			break;
		default:
			dprintf( D_ALWAYS, "unknown DC_CONFIG command!\n" );
			free( admin );
			free( config );
			return FALSE;
		}
	}

	stream->encode();
	if ( ! stream->code(rval) ) {
		dprintf (D_ALWAYS, "Failed to send rval for DC_CONFIG.\n" );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG.\n" );
		return FALSE;
	}

	return (failed ? FALSE : TRUE);
}


#ifndef WIN32
void
unix_sighup(int)
{
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGHUP );
	}
}


void
unix_sigterm(int)
{
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}
}


void
unix_sigquit(int)
{
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
	}
}


void
unix_sigchld(int)
{
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGCHLD );
	}
}


void
unix_sigusr1(int)
{
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGUSR1 );
	}
	
}

void
unix_sigusr2(int)
{
	// This is a debug only param not to be advertised.
	if (param_boolean( "DEBUG_CLASSAD_CACHE", false))
	{
	  std::string szFile = param("LOG");
	  szFile +="/";
	  szFile += get_mySubSystem()->getName();
	  szFile += "_classad_cache";
	  
	  if (!classad::CachedExprEnvelope::_debug_dump_keys(szFile))
	  {
	    dprintf( D_FULLDEBUG, "FAILED to write file %s\n",szFile.c_str() );
	  }
	}
	
  
	if (daemonCore) {
		daemonCore->Send_Signal( daemonCore->getpid(), SIGUSR2 );
	}
}



#endif /* ! WIN32 */



void
dc_reconfig()
{
		// do this first in case anything else depends on DNS
	daemonCore->refreshDNS();

		// Actually re-read the files...  Added by Derek Wright on
		// 12/8/97 (long after this function was first written... 
		// nice goin', Todd).  *grin*

		/* purify flags this as a stack bounds array read violation when 
			we're expecting to use the default argument. However, due to
			_craziness_ in the header file that declares this function
			as an extern "C" linkage with a default argument(WTF!?) while
			being called in a C++ context, something goes wrong. So, we'll
			just supply the errant argument. */
	config();

		// See if we're supposed to be allowing core files or not
	if ( doCoreInit ) {
		check_core_files();
	}

		// If we're supposed to be using our own log file, reset that here. 
	if( logDir ) {
		set_log_dir();
	}

	if( logAppend ) {
		handle_log_append( logAppend );
	}

	// Reinitialize logging system; after all, LOG may have been changed.
	dprintf_config(get_mySubSystem()->getName());
	
	// again, chdir to the LOG directory so that if we dump a core
	// it will go there.  the location of LOG may have changed, so redo it here.
	drop_core_in_log();

	// Re-read everything from the config file DaemonCore itself cares about.
	// This also cleares the DNS cache.
	daemonCore->reconfig();

	// Clear out the passwd cache.
	clear_passwd_cache();

	// Re-drop the address file, if it's defined, just to be safe.
	drop_addr_file();

		// Re-drop the pid file, if it's requested, just to be safe.
	if( pidFile ) {
		drop_pid_file();
	}

		// If requested to do so in the config file, do a segv now.
		// This is to test our handling/writing of a core file.
	char* ptmp;
	if ( param_boolean_crufty("DROP_CORE_ON_RECONFIG", false) ) {
			// on purpose, derefernce a null pointer.
			ptmp = NULL;
			char segfault;	
			MSC_SUPPRESS_WARNING_FOREVER(6011) // warning about NULL pointer deref.
			segfault = *ptmp; // should blow up here
			if (segfault) {} // Line to avoid compiler warnings.
			ptmp[0] = 'a';
			
			// should never make it to here!
			EXCEPT("FAILED TO DROP CORE");	
	}

	// call this daemon's specific main_config()
	dc_main_config();
}

int
handle_dc_sighup( Service*, int )
{
	dprintf( D_ALWAYS, "Got SIGHUP.  Re-reading config files.\n" );
	dc_reconfig();
	return TRUE;
}


void
TimerHandler_main_shutdown_fast()
{
	dc_main_shutdown_fast();
}


int
handle_dc_sigterm( Service*, int )
{
		// Introduces a race condition.
		// What if SIGTERM received while we are here?
	if( !SigtermContinue::should_sigterm_continue() ) {
		dprintf( D_FULLDEBUG, 
				 "Got SIGTERM, but we've already done graceful shutdown.  Ignoring.\n" );
		return TRUE;
	}
	SigtermContinue::sigterm_should_not_continue(); // After this

	dprintf(D_ALWAYS, "Got SIGTERM. Performing graceful shutdown.\n");

#if defined(WIN32) && 0
	if ( line_where_service_stopped != 0 ) {
		dprintf(D_ALWAYS,"Line where service stopped = %d\n",
			line_where_service_stopped);
	}
#endif

	if( daemonCore->GetPeacefulShutdown() ) {
		dprintf( D_FULLDEBUG, 
				 "Peaceful shutdown in effect.  No timeout enforced.\n");
	}
	else {
		int timeout = param_integer("SHUTDOWN_GRACEFUL_TIMEOUT", 30 * MINUTE);
		daemonCore->Register_Timer( timeout, 0, 
									TimerHandler_main_shutdown_fast,
									"main_shutdown_fast" );
		dprintf( D_FULLDEBUG, 
				 "Started timer to call main_shutdown_fast in %d seconds\n", 
				 timeout );
	}
	dc_main_shutdown_graceful();
	return TRUE;
}

void
TimerHandler_dc_sigterm()
{
	handle_dc_sigterm(NULL, SIGTERM);
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
	dc_main_shutdown_fast();
	return TRUE;
}

const size_t OOM_RESERVE = 2048;
static char *oom_reserve_buf;
static void OutOfMemoryHandler()
{
	std::set_new_handler(NULL);

		// free up some memory to improve our chances of
		// successfully logging
	delete [] oom_reserve_buf;

	int monitor_age = 0;
	unsigned long vsize = 0;
	unsigned long rss = 0;

	if( daemonCore && daemonCore->monitor_data.last_sample_time != -1 ) {
		monitor_age = (int)(time(NULL)-daemonCore->monitor_data.last_sample_time);
		vsize = daemonCore->monitor_data.image_size;
		rss = daemonCore->monitor_data.rs_size;
	}

	dprintf_dump_stack();

	EXCEPT("Out of memory!  %ds ago: vsize=%lu KB, rss=%lu KB",
		   monitor_age,
		   vsize,
		   rss);
}

static void InstallOutOfMemoryHandler()
{
	if( !oom_reserve_buf ) {
		oom_reserve_buf = new char[OOM_RESERVE];
		memset(oom_reserve_buf,0,OOM_RESERVE);
	}

	std::set_new_handler(OutOfMemoryHandler);
}

// This is the main entry point for daemon core.  On WinNT, however, we
// have a different, smaller main which checks if "-f" is ommitted from
// the command line args of the condor_master, in which case it registers as 
// an NT service.
int dc_main( int argc, char** argv )
{
	char**	ptr;
	int		command_port = -1;
	char const *daemon_sock_name = NULL;
	int		dcargs = 0;		// number of daemon core command-line args found
	char	*ptmp, *ptmp1;
	int		i;
	int		wantsKill = FALSE, wantsQuiet = FALSE;
	bool	done;


	condor_main_argc = argc;
	condor_main_argv = (char **)malloc((argc+1)*sizeof(char *));
	for(i=0;i<argc;i++) {
		condor_main_argv[i] = strdup(argv[i]);
	}
	condor_main_argv[i] = NULL;

#ifdef WIN32
	/** Enable support of the %n format in the printf family 
		of functions. */
	_set_printf_count_output(TRUE);
#endif

#ifndef WIN32
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
	install_sig_handler_with_mask(SIGUSR2, &fullset, unix_sigusr2);
	install_sig_handler(SIGPIPE, SIG_IGN );

#endif // of ifndef WIN32

	_condor_myServiceName = argv[0];
	// set myName to be argv[0] with the path stripped off
	myName = condor_basename(argv[0]);
	myFullName = getExecPath();
	if( ! myFullName ) {
			// if getExecPath() didn't work, the best we can do is try
			// saving argv[0] and hope for the best...
		if( argv[0][0] == '/' ) {
				// great, it's already a full path
			myFullName = strdup(argv[0]);
		} else {
				// we don't have anything reliable, so forget it.  
			myFullName = NULL;
		}
	}

	myDistro->Init( argc, argv );
	if ( EnvInit() < 0 ) {
		exit( 1 );
	}

		// call out to the handler for pre daemonCore initialization
		// stuff so that our client side can do stuff before we start
		// messing with argv[]
	if ( dc_main_pre_dc_init ) {
		dc_main_pre_dc_init( argc, argv );
	}

		// Make sure this is set, since DaemonCore needs it for all
		// sorts of things, and it's better to clearly EXCEPT here
		// than to seg fault down the road...
	if( ! get_mySubSystem() ) {
		EXCEPT( "Programmer error: get_mySubSystem() is NULL!" );
	}
	if( !get_mySubSystem()->isValid() ) {
		get_mySubSystem()->printf( );
		EXCEPT( "Programmer error: get_mySubSystem() info is invalid(%s,%d,%s)!",
				get_mySubSystem()->getName(),
				get_mySubSystem()->getType(),
				get_mySubSystem()->getTypeName() );
	}

	if ( !dc_main_init ) {
		EXCEPT( "Programmer error: dc_main_init is NULL!" );
	}
	if ( !dc_main_config ) {
		EXCEPT( "Programmer error: dc_main_config is NULL!" );
	}
	if ( !dc_main_shutdown_fast ) {
		EXCEPT( "Programmer error: dc_main_shutdown_fast is NULL!" );
	}
	if ( !dc_main_shutdown_graceful ) {
		EXCEPT( "Programmer error: dc_main_shutdown_graceful is NULL!" );
	}

	// strip off any daemon-core specific command line arguments
	// from the front of the command line.
	i = 0;
	done = false;

	for(ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++) {
		if(ptr[0][0] != '-') {
			break;
		}

			/*
			  NOTE!  If you're adding a new command line argument to
			  this switch statement, YOU HAVE TO ADD IT TO THE #ifdef
			  WIN32 VERSION OF "main()" NEAR THE END OF THIS FILE,
			  TOO.  You should actually deal with the argument here,
			  but you need to add it there to be skipped over because
			  of NT weirdness with starting as a service, etc.  Also,
			  please add your argument in alphabetical order, so we
			  can maintain some semblance of order in here, and make
			  it easier to keep the two switch statements in sync.
			  Derek Wright <wright@cs.wisc.edu> 11/11/99
			*/
		switch(ptr[0][1]) {
		case 'a':		// Append to the log file name.
			ptr++;
			if( ptr && *ptr ) {
				logAppend = *ptr;
				dcargs += 2;
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -append needs another argument.\n" );
				fprintf( stderr, 
					 "   Please specify a string to append to our log's filename.\n" );
				exit( 1 );
			}
			break;
		case 'b':		// run in Background (default)
			Foreground = 0;
			dcargs++;
			break;
		case 'c':		// specify directory where Config file lives
			ptr++;
			if( ptr && *ptr ) {
				ptmp = *ptr;
				dcargs += 2;

				ptmp1 = 
					(char *)malloc( strlen(ptmp) + myDistro->GetLen() + 10 );
				if ( ptmp1 ) {
					sprintf(ptmp1,"%s_CONFIG=%s", myDistro->GetUc(), ptmp);
					SetEnv(ptmp1);
				}
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -config needs another argument.\n" );
				fprintf( stderr, 
						 "   Please specify the filename of the config file.\n" );
				exit( 1 );
			}				  
			break;
		case 'd':		// Dynamic local directories
			DynamicDirs = true;
			dcargs++;
			break;
		case 'f':		// run in Foreground
			Foreground = 1;
			dcargs++;
			break;
#ifndef WIN32
		case 'k':		// Kill the pid in the given pid file
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
		case 'l':	// -local-name or -log
			// specify local name
			if ( strcmp( &ptr[0][1], "local-name" ) == 0 ) {
				ptr++;
				if( ptr && *ptr ) {
					get_mySubSystem()->setLocalName( *ptr );
					dcargs += 2;
				}
				else {
					fprintf( stderr, "DaemonCore: ERROR: "
							 "-local-name needs another argument.\n" );
					fprintf( stderr, 
							 "   Please specify the local config to use.\n" );
					exit( 1 );
				}
			}

			// specify Log directory 
			else {
				ptr++;
				if( ptr && *ptr ) {
					logDir = *ptr;
					dcargs += 2;
				} else {
					fprintf( stderr, "DaemonCore: ERROR: -log needs another "
							 "argument\n" );
					exit( 1 );
				}
			}
			break;

		case 'h':		// -http <port> : specify port for HTTP and SOAP requests
			if ( ptr[0][2] && ptr[0][2] == 't' ) {
					// specify an HTTP port
				ptr++;
				if( ptr && *ptr ) {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -http no longer accepted.\n" );
					exit( 1 );
				}
			} else {
					// it's not -http, so do NOT consume this arg!!
					// in fact, we're done w/ DC args, since it's the
					// first option we didn't recognize.
				done = true;
			}
			break;
		case 'p':		// use well-known Port for command socket, or
				        // specify a Pidfile to drop your pid into
			if( ptr[0][2] && ptr[0][2] == 'i' ) {
					// Specify a Pidfile
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
					// use well-known Port for command socket				
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
		case 'q':		// Quiet output
			wantsQuiet = TRUE;
			dcargs++;
			break;			
		case 'r':	   // Run for <arg> minutes, then gracefully exit
			ptr++;
			if( ptr && *ptr ) {
				runfor = atoi( *ptr );
				dcargs += 2;
			}
			else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -runfor needs another argument.\n" );
				fprintf( stderr, 
				   "   Please specify the number of minutes to run for.\n" );

				exit( 1 );
			}
			//call Register_Timer below after intialized...
			break;
		case 's':
				// the c-gahp uses -s, so for backward compatibility,
				// do not allow abbreviations of -sock
			if( strcmp("-sock",*ptr) ) {
				done = true;
				break;
			}
			else {
				ptr++;
				if( *ptr ) {
					daemon_sock_name = *ptr;
					dcargs += 2;
				} else {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -sock needs another argument.\n" );
					fprintf( stderr, 
							 "   Please specify a socket name.\n" );
					exit( 1 );
				}
			}
			break;
		case 't':		// log to Terminal (stderr)
			Termlog = 1;
			dcargs++;
			break;
		case 'v':		// display Version info and exit
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
			break;
		default:
			done = true;
			break;	
		}
		if ( done ) {
			break;		// break out of for loop
		}
	}

	// using "-t" also implicitly sets "-f"; i.e. only to stderr in the foreground
	if ( Termlog ) {
		Foreground = 1;
	}

	// call config so we can call param.  
	// Try to minimize shadow footprint by not loading the metadata from the config file
	int config_options = get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHADOW) ? 0 : CONFIG_OPT_WANT_META;
	const bool abort_if_invalid = true;
	config_ex(wantsQuiet, abort_if_invalid, config_options);


    // call dc_config_GSI to set GSI related parameters so that all
    // the daemons will know what to do.
	if ( doAuthInit ) {
		condor_auth_config( true );
	}

		// See if we're supposed to be allowing core files or not
	if ( doCoreInit ) {
		check_core_files();
	}

		// If we want to kill something, do that now.
	if( wantsKill ) {
		do_kill();
	}

	if( ! DynamicDirs ) {

			// We need to setup logging.  Normally, we want to do this
			// before the fork(), so that if there are problems and we
			// fprintf() to stderr, we'll still see them.  However, if
			// we want DynamicDirs, we can't do this yet, since we
			// need our pid to specify the log directory...

		// If want to override what's set in our config file, we've
		// got to do that here, between where we config and where we
		// setup logging.  Note: we also have to do this in reconfig.
		if( logDir ) {
			set_log_dir();
		}

		// If we're told on the command-line to append something to
		// the name of our log file, we do that here, so that when we
		// setup logging, we get the right filename.  -Derek Wright
		// 11/20/98
		if( logAppend ) {
			handle_log_append( logAppend );
		}
		
			// Actually set up logging.
		if(Termlog)
			dprintf_set_tool_debug(get_mySubSystem()->getName(), 0);
		else
			dprintf_config(get_mySubSystem()->getName());
	}

		// run as condor 99.9% of the time, so studies tell us.
	set_condor_priv();

	// we want argv[] stripped of daemoncore options
	ptmp = argv[0];			// save a temp pointer to argv[0]
	argv = --ptr;			// make space for argv[0]
	argv[0] = ptmp;			// set argv[0]
	argc -= dcargs;
	if ( argc < 1 )
		argc = 1;

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

		// And close stdin, out, err if we are the MASTER.

		// NRL 2006-08-10: Here's what and why we're doing this.....
		//
		// In days of yore, we'd simply close FDs 0,1,2 and be done
		// with it.  Unfortunately, some 3rd party tools were writing
		// directly to these FDs.  Now, you might not that that this
		// would be a problem, right?  Unfortunately, once you close
		// an FD (say, 1 or 2), then next safe_open_wrapper() or socket()
		// call can / will now return 1 (or 2).  So, when libfoo.a thinks
		// it fprintf()ing an error message to fd 2 (the FD behind
		// stderr), it's actually sending that message over a socket
		// to a process that has no idea how to handle it.
		//
		// So, here's what we do.  We open /dev/null, and then use
		// dup2() to copy this "safe" fd to these decriptors.  Note
		// that if you don't open it with O_RDWR, you can cause writes
		// to the FD to return errors.
		//
		// Finally, we only do this in the master.  Why?  Because
		// Create_Process() has similar logic, so when the master
		// starts, say, condor_startd, the Create_Process() logic
		// takes over and does The Right Thing(tm).
		//
		// Regarding the std in/out/err streams (the FILE *s, not the
		// FDs), we leave those open.  Otherwise, if libfoo.a tries to
		// fwrite to stderr, it would get back an error from fprintf()
		// (or fwrite(), etc.).  If it checks this, it might Do
		// Something Bad(tm) like abort() -- who knows.  In any case,
		// it seems safest to just leave them open but attached to
		// /dev/null.

		if ( get_mySubSystem()->isType( SUBSYSTEM_TYPE_MASTER ) ) {
			int	fd_null = safe_open_wrapper_follow( NULL_FILE, O_RDWR );
			if ( fd_null < 0 ) {
				fprintf( stderr, "Unable to open %s: %s\n", NULL_FILE, strerror(errno) );
				dprintf( D_ALWAYS, "Unable to open %s: %s\n", NULL_FILE, strerror(errno) );
			}
			int	fd;
			for( fd=0;  fd<=2;  fd++ ) {
				close( fd );
				if ( ( fd_null >= 0 ) && ( fd_null != fd ) &&
					 ( dup2( fd_null, fd ) < 0 )  ) {
					dprintf( D_ALWAYS, "Error dup2()ing %s -> %d: %s\n",
							 NULL_FILE, fd, strerror(errno) );
				}
			}
			// Close the /dev/null descriptor _IF_ it's not stdin/out/err
			if ( fd_null > 2 ) {
				close( fd_null );
			}
		}
		// and detach from the controlling tty
		detach();

#endif	// of else of ifdef WIN32
	}	// if if !Foreground

	// See if the config tells us to wait on startup for a debugger to attach.
	MyString debug_wait_param;
	debug_wait_param.formatstr("%s_DEBUG_WAIT", get_mySubSystem()->getName() );
	if (param_boolean(debug_wait_param.Value(), false, false)) {
		volatile int debug_wait = 1;
		dprintf(D_ALWAYS,
				"%s is TRUE, waiting for debugger to attach to pid %d.\n", 
				debug_wait_param.Value(), (int)::getpid());
		while (debug_wait) {
			sleep(1);
		}
	}

#ifdef WIN32
	debug_wait_param.formatstr("%s_WAIT_FOR_DEBUGGER", get_mySubSystem()->getName() );
	int wait_for_win32_debugger = param_integer(debug_wait_param.Value(), 0);
	if (wait_for_win32_debugger) {
		UINT ms = GetTickCount() - 10;
		BOOL is_debugger = IsDebuggerPresent();
		while ( ! is_debugger) {
			if (GetTickCount() > ms) {
				dprintf(D_ALWAYS,
						"%s is %d, waiting for debugger to attach to pid %d.\n", 
						debug_wait_param.Value(), wait_for_win32_debugger, GetCurrentProcessId());
				ms = GetTickCount() + (1000 * 60 * 1); // repeat message every 1 minute
			}
			sleep(10);
			is_debugger = IsDebuggerPresent();
		}
	}
#endif

		// Now that we've potentially forked, we have our real pid, so
		// we can instantiate a daemon core and it'll have the right
		// pid. 
	daemonCore = new DaemonCore();

	if( DynamicDirs ) {
			// If we want to use dynamic dirs for log, spool and
			// execute, we now have our real pid, so we can actually
			// give it the correct name.

		handle_dynamic_dirs();

		if( logAppend ) {
			handle_log_append( logAppend );
		}
		
			// Actually set up logging.
		dprintf_config(get_mySubSystem()->getName());
	}

		// Now that we have the daemonCore object, we can finally
		// know what our pid is, so we can print out our opening
		// banner.  Plus, if we're using dynamic dirs, we have dprintf
		// configured now, so the dprintf()s will work.
	dprintf(D_ALWAYS,"******************************************************\n");
	dprintf(D_ALWAYS,"** %s (%s_%s) STARTING UP\n",
			myName,myDistro->GetUc(), get_mySubSystem()->getName() );
	if( myFullName ) {
		dprintf( D_ALWAYS, "** %s\n", myFullName );
		free( myFullName );
		myFullName = NULL;
	}
	dprintf(D_ALWAYS,"** %s\n", get_mySubSystem()->getString() );
	dprintf(D_ALWAYS,"** Configuration: subsystem:%s local:%s class:%s\n",
			get_mySubSystem()->getName(),
			get_mySubSystem()->getLocalName("<NONE>"),
			get_mySubSystem()->getClassName( )
			);
	dprintf(D_ALWAYS,"** %s\n", CondorVersion());
	dprintf(D_ALWAYS,"** %s\n", CondorPlatform());
	dprintf(D_ALWAYS,"** PID = %lu\n", (unsigned long) daemonCore->getpid());
	time_t log_last_mod_time = dprintf_last_modification();
	if ( log_last_mod_time <= 0 ) {
		dprintf(D_ALWAYS,"** Log last touched time unavailable (%s)\n",
				strerror(-log_last_mod_time));
	} else {
		struct tm *tm = localtime( &log_last_mod_time );
		dprintf(D_ALWAYS,"** Log last touched %d/%d %02d:%02d:%02d\n",
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				tm->tm_sec);
	}

#ifndef WIN32
		// Want to do this dprintf() here, since we can't do it w/n 
		// the priv code itself or we get major problems. 
		// -Derek Wright 12/21/98 
	if( getuid() ) {
		dprintf(D_PRIV, "** Running as non-root: No privilege switching\n");
	} else {
		dprintf(D_PRIV, "** Running as root: Privilege switching in effect\n");
	}
#endif

	dprintf(D_ALWAYS,"******************************************************\n");

	if (global_config_source != "") {
		dprintf(D_ALWAYS, "Using config source: %s\n", 
				global_config_source.Value());
	} else {
		const char* env_name = EnvGetName( ENV_CONFIG );
		char* env = getenv( env_name );
		if( env ) {
			dprintf(D_ALWAYS, 
					"%s is set to '%s', not reading a config file\n",
					env_name, env );
		}
	}

	if (!local_config_sources.isEmpty()) {
		dprintf(D_ALWAYS, "Using local config sources: \n");
		local_config_sources.rewind();
		char *source;
		while( (source = local_config_sources.next()) != NULL ) {
			dprintf(D_ALWAYS, "   %s\n", source );
		}
	}
	_macro_stats stats;
	get_config_stats(&stats);
	dprintf(D_ALWAYS, "config Macros = %d, Sorted = %d, StringBytes = %d, TablesBytes = %d\n",
				stats.cEntries, stats.cSorted, stats.cbStrings, stats.cbTables);


	// TJ: Aug/2013 we are going to turn on classad caching by default in 8.1.1 we want to see in the log that its on.
	dprintf (D_ALWAYS, "CLASSAD_CACHING is %s\n", param_boolean("ENABLE_CLASSAD_CACHING", false) ? "ENABLED" : "OFF");

		// chdir() into our log directory so if we drop core, that's
		// where it goes.  We also do some NT-specific stuff in here.
	drop_core_in_log();

#ifdef WIN32
		// On NT, we need to make certain we have a console allocated,
		// since many standard mechanisms do not work without a console
		// attached [like popen(), stdin/out/err, GenerateConsoleEvent].
		// There are several reasons why we may not have a console
		// right now: a) we are running as a service, and services are 
		// born without a console, or b) the user did not specify "-f" 
		// or "-t", and thus we called FreeConsole() above.

		// for now, don't create a console for the kbdd, as that daemon
		// is now a win32 app and not a console app.
	BOOL is_kbdd = (0 == strcmp(get_mySubSystem()->getName(), "KBDD"));
	if(!is_kbdd)
		AllocConsole();
#endif

#ifndef WIN32
		// Now that logging is setup, create a pipe to deal with unix 
		// async signals.  We do this after logging is setup so that
		// we can EXCEPT (and really log something) on failure...
	if ( pipe(daemonCore->async_pipe) == -1 ||
		 fcntl(daemonCore->async_pipe[0],F_SETFL,O_NONBLOCK) == -1 ||
		 fcntl(daemonCore->async_pipe[1],F_SETFL,O_NONBLOCK) == -1 ) {
			EXCEPT("Failed to create async pipe");
	}
#else
	if ( daemonCore->async_pipe[1].connect_socketpair(daemonCore->async_pipe[0])==false )
	{
		EXCEPT("Failed to create async pipe socket pair");
	}
#endif

	if ( dc_main_pre_command_sock_init ) {
		dc_main_pre_command_sock_init();
	}

		/* NOTE re main_pre_command_sock_init:
		  *
		  * The Master uses main_pre_command_sock_init to check for
		  * the InstanceLock. Any operation that is distructive before
		  * this point will possibly change the state/environment for
		  * an already running master.
		  *
		  *  In the pidfile case, a second Master will start, drop its
		  *  pid in the file and then exit, see GT343.
		  *
		  *  In the DAEMON_AD_FILE case, which isn't actually a
		  *  problem as of this comment because the Master does not
		  *  drop one, there would be a period of time when the file
		  *  would be missing.
		  *
		  * PROBLEM: TRUNC_*_LOG_ON_OPEN=TRUE will get to delete the
		  * log file before getting here.
		  */

		// Now that we have our pid, we could dump our pidfile, if we
		// want it. 
	if( pidFile ) {
		drop_pid_file();
	}

		// Avoid possibility of stale info sticking around from previous run.
		// For example, we had problems in 7.0.4 and earlier with reconnect
		// shadows in parallel universe reading the old schedd ad file.
	kill_daemon_ad_file();


		// SETUP COMMAND SOCKET
	daemonCore->SetDaemonSockName( daemon_sock_name );
	daemonCore->InitDCCommandSocket( command_port );

		// Install DaemonCore signal handlers common to all daemons.
	daemonCore->Register_Signal( SIGHUP, "SIGHUP", 
								 (SignalHandler)handle_dc_sighup,
								 "handle_dc_sighup()" );
	daemonCore->Register_Signal( SIGQUIT, "SIGQUIT", 
								 (SignalHandler)handle_dc_sigquit,
								 "handle_dc_sigquit()" );
	daemonCore->Register_Signal( SIGTERM, "SIGTERM", 
								 (SignalHandler)handle_dc_sigterm,
								 "handle_dc_sigterm()" );

	daemonCore->Register_Signal( DC_SERVICEWAITPIDS, "DC_SERVICEWAITPIDS",
								(SignalHandlercpp)&DaemonCore::HandleDC_SERVICEWAITPIDS,
								"HandleDC_SERVICEWAITPIDS()",daemonCore);
#ifndef WIN32
	daemonCore->Register_Signal( SIGCHLD, "SIGCHLD",
								 (SignalHandlercpp)&DaemonCore::HandleDC_SIGCHLD,
								 "HandleDC_SIGCHLD()",daemonCore);
#endif

		// Install DaemonCore timers common to all daemons.

		//if specified on command line, set timer to gracefully exit
	if ( runfor ) {
		daemon_stop_time = time(NULL)+runfor*60;
		daemonCore->Register_Timer( runfor * 60, 0, 
				TimerHandler_dc_sigterm, "handle_dc_sigterm" );
		dprintf(D_ALWAYS,"Registered Timer for graceful shutdown in %d minutes\n",
				runfor );
	}
	else {
		daemon_stop_time = 0;
	}

#ifndef WIN32
		// This timer checks if our parent is dead; if so, we shutdown.
		// We only do this on Unix; on NT we watch our parent via a different mechanism.
		// Also note: we do not want the master to exibit this behavior!
	if ( ! get_mySubSystem()->isType(SUBSYSTEM_TYPE_MASTER) ) {
		daemonCore->Register_Timer( 15, 120, 
				check_parent, "check_parent" );
	}
#endif

	daemonCore->Register_Timer( 0,
				dc_touch_log_file, "dc_touch_log_file" );

	daemonCore->Register_Timer( 0,
				dc_touch_lock_files, "dc_touch_lock_files" );

	daemonCore->Register_Timer( 0, 5 * 60,
				check_session_cache, "check_session_cache" );
	

	// set the timer for half the session duration, 
	// since we retain the old cookie. Also make sure
	// the value is atleast 1.
	int cookie_refresh = (param_integer("SEC_DEFAULT_SESSION_DURATION", 3600)/2)+1;

	daemonCore->Register_Timer( 0, cookie_refresh, 
				handle_cookie_refresh, "handle_cookie_refresh");
 

	if( get_mySubSystem()->isType( SUBSYSTEM_TYPE_MASTER ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_COLLECTOR ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_NEGOTIATOR ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_SCHEDD ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_STARTD ) ) {
        daemonCore->monitor_data.EnableMonitoring();
    }

		// Install DaemonCore command handlers common to all daemons.
	daemonCore->Register_Command( DC_RECONFIG, "DC_RECONFIG",
								  (CommandHandler)handle_reconfig,
								  "handle_reconfig()", 0, WRITE );

	daemonCore->Register_Command( DC_RECONFIG_FULL, "DC_RECONFIG_FULL",
								  (CommandHandler)handle_reconfig,
								  "handle_reconfig()", 0, WRITE );

	daemonCore->Register_Command( DC_CONFIG_VAL, "DC_CONFIG_VAL",
								  (CommandHandler)handle_config_val,
								  "handle_config_val()", 0, READ );
		// Deprecated name for it.
	daemonCore->Register_Command( CONFIG_VAL, "CONFIG_VAL",
								  (CommandHandler)handle_config_val,
								  "handle_config_val()", 0, READ );

		// The handler for setting config variables does its own
		// authorization, so these two commands should be registered
		// as "ALLOW" and the handler will do further checks.
	daemonCore->Register_Command( DC_CONFIG_PERSIST, "DC_CONFIG_PERSIST",
								  (CommandHandler)handle_config,
								  "handle_config()", 0, ALLOW );

	daemonCore->Register_Command( DC_CONFIG_RUNTIME, "DC_CONFIG_RUNTIME",
								  (CommandHandler)handle_config,
								  "handle_config()", 0, ALLOW );

	daemonCore->Register_Command( DC_OFF_FAST, "DC_OFF_FAST",
								  (CommandHandler)handle_off_fast,
								  "handle_off_fast()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_GRACEFUL, "DC_OFF_GRACEFUL",
								  (CommandHandler)handle_off_graceful,
								  "handle_off_graceful()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_FORCE, "DC_OFF_FORCE",
								  (CommandHandler)handle_off_force,
								  "handle_off_force()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_PEACEFUL, "DC_OFF_PEACEFUL",
								  (CommandHandler)handle_off_peaceful,
								  "handle_off_peaceful()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_SET_PEACEFUL_SHUTDOWN, "DC_SET_PEACEFUL_SHUTDOWN",
								  (CommandHandler)handle_set_peaceful_shutdown,
								  "handle_set_peaceful_shutdown()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_SET_FORCE_SHUTDOWN, "DC_SET_FORCE_SHUTDOWN",
								  (CommandHandler)handle_set_force_shutdown,
								  "handle_set_force_shutdown()", 0, ADMINISTRATOR );

		// DC_NOP is for waking up select.  There is no need for
		// security here, because anyone can wake up select anyway.
		// This command is also used to gracefully close a socket
		// that has been registered to read a command.
	daemonCore->Register_Command( DC_NOP, "DC_NOP",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, ALLOW );

		// the next several commands are NOPs registered at all permission
		// levels, for security testing and diagnostics.  for now they all
		// invoke the same function, but how they are authorized before calling
		// it may vary depending on the configuration
	daemonCore->Register_Command( DC_NOP_READ, "DC_NOP_READ",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, READ );

	daemonCore->Register_Command( DC_NOP_WRITE, "DC_NOP_WRITE",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, WRITE );

	daemonCore->Register_Command( DC_NOP_NEGOTIATOR, "DC_NOP_NEGOTIATOR",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, NEGOTIATOR );

	daemonCore->Register_Command( DC_NOP_ADMINISTRATOR, "DC_NOP_ADMINISTRATOR",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_NOP_OWNER, "DC_NOP_OWNER",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, OWNER );

	daemonCore->Register_Command( DC_NOP_CONFIG, "DC_NOP_CONFIG",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, CONFIG_PERM );

	daemonCore->Register_Command( DC_NOP_DAEMON, "DC_NOP_DAEMON",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, DAEMON );

	daemonCore->Register_Command( DC_NOP_ADVERTISE_STARTD, "DC_NOP_ADVERTISE_STARTD",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, ADVERTISE_STARTD_PERM );

	daemonCore->Register_Command( DC_NOP_ADVERTISE_SCHEDD, "DC_NOP_ADVERTISE_SCHEDD",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, ADVERTISE_SCHEDD_PERM );

	daemonCore->Register_Command( DC_NOP_ADVERTISE_MASTER, "DC_NOP_ADVERTISE_MASTER",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, ADVERTISE_MASTER_PERM );


	daemonCore->Register_Command( DC_FETCH_LOG, "DC_FETCH_LOG",
								  (CommandHandler)handle_fetch_log,
								  "handle_fetch_log()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_PURGE_LOG, "DC_PURGE_LOG",
								  (CommandHandler)handle_fetch_log_history_purge,
								  "handle_fetch_log_history_purge()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_INVALIDATE_KEY, "DC_INVALIDATE_KEY",
								  (CommandHandler)handle_invalidate_key,
								  "handle_invalidate_key()", 0, ALLOW );
								  
		//
		// The time offset command is used to figure out what
		// the range of the clock skew is between the daemon code and another
		// entity calling into us
		//
	daemonCore->Register_Command( DC_TIME_OFFSET, "DC_TIME_OFFSET",
								  (CommandHandler)time_offset_receive_cedar_stub,
								  "time_offset_cedar_stub", 0, DAEMON );

	// Call daemonCore's reconfig(), which reads everything from
	// the config file that daemonCore cares about and initializes
	// private data members, etc.
	daemonCore->reconfig();

	// zmiller
	// look in the env for ENV_PARENT_ID
	const char* envName = EnvGetName ( ENV_PARENT_ID );
	MyString parent_id;

	GetEnv( envName, parent_id );

	// send it to the SecMan object so it can include it in any
	// classads it sends.  if this is NULL, it will not include
	// the attribute.
	daemonCore->sec_man->set_parent_unique_id(parent_id.Value());

	// now re-set the identity so that any children we spawn will have it
	// in their environment
	SetEnv( envName, daemonCore->sec_man->my_unique_id() );

	// create a database connection object
	//DBObj = createConnection();

	// create a sql log object. We always have one defined, but 
	// if quill is not enabled we never write data to the logfile
	bool use_sql_log = param_boolean( "QUILL_USE_SQL_LOG", false );

	FILEObj = FILESQL::createInstance(use_sql_log); 
    // create an xml log object
    XMLObj = FILEXML::createInstanceXML();

	InstallOutOfMemoryHandler();

	// call the daemon's main_init()
	dc_main_init( argc, argv );

	// now call the driver.  we never return from the driver (infinite loop).
	daemonCore->Driver();

	// should never get here
	EXCEPT("returned from Driver()");
	return FALSE;
}	

// Parse argv enough to decide if we are starting as foreground or as background
// We need this for windows because when starting as background, we need to actually
// start via the Service Control Manager rather than calling dc_main directly.
//
bool dc_args_is_background(int argc, char** argv)
{
    bool ForegroundFlag = false; // default to background

	// Scan our command line arguments for a "-f".  If we don't find a "-f",
	// or a "-v", then we want to register as an NT Service.
	int i = 0;
	bool done = false;
	for(char ** ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++)
	{
		if( ptr[0][0] != '-' ) {
			break;
		}
		switch( ptr[0][1] ) {
		case 'a':		// Append to the log file name.
			ptr++;
			break;
		case 'b':		// run in Background (default)
			ForegroundFlag = false;
			break;
		case 'c':		// specify directory where Config file lives
			ptr++;
			break;
		case 'd':		// Dynamic local directories
			break;
		case 't':		// log to Terminal (stderr), implies -f
		case 'f':		// run in ForegroundFlag
			ForegroundFlag = true;
			break;
		case 'h':		// -http
			if ( ptr[0][2] && ptr[0][2] == 't' ) {
					// specify an HTTP port
				ptr++;
			} else {
				done = true;
			}
            break;
#ifndef WIN32
		case 'k':		// Kill the pid in the given pid file
			ptr++;
			break;
#endif
		case 'l':		// specify Log directory
			 ptr++;
			 break;
		case 'p':		// Use well-known Port for command socket.
			ptr++;		// Also used to specify a Pid file, but both
			break;		// versions require a 2nd arg, so we're safe. 
		case 'q':		// Quiet output
			break;
		case 'r':		// Run for <arg> minutes, then gracefully exit
			ptr++;
			break;
		case 's':       // the c-gahp uses -s, so for backward compatibility,
						// do not allow abbreviations of -sock
			if(0 == strcmp("-sock",*ptr)) {
				ptr++;
			} else {
				done = true;
			}
			break;
		case 'v':		// display Version info and exit
			ForegroundFlag = true;
			break;
		default:
			done = true;
			break;	
		}
		if( done ) {
			break;		// break out of for loop
		}
	}

    return ! ForegroundFlag;
}
