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
#include "condor_config.h"
#include "util_lib_proto.h"
#include "basename.h"
#include "my_hostname.h"
#include "condor_version.h"
#include "limit.h"
#include "condor_email.h"
#include "sig_install.h"
#include "daemon.h"
#include "condor_debug.h"
#include "condor_distribution.h"
#include "condor_environ.h"

#define _NO_EXTERN_DAEMON_CORE 1	
#include "condor_daemon_core.h"

#ifdef WIN32
#include "exphnd.WIN32.h"
#endif

// Externs to Globals
extern char* mySubSystem;	// the subsys ID, such as SCHEDD, STARTD, etc. 
extern DLL_IMPORT_MAGIC char **environ;

// External protos
extern int main_init(int argc, char *argv[]);	// old main()
extern int main_config(bool is_full);
extern int main_shutdown_fast();
extern int main_shutdown_graceful();
extern void main_pre_dc_init(int argc, char *argv[]);
extern void main_pre_command_sock_init();

// Internal protos
void dc_reconfig( bool is_full );
void dc_config_auth();       // Configuring GSI (and maybe other) authentication related stuff
// Globals
int		Foreground = 0;		// run in background by default
static char*	myName;			// set to basename(argv[0])
static char*	myFullName;		// set to the full path to ourselves
DaemonCore*	daemonCore;
static int is_master = 0;
char*	logDir = NULL;
char*	pidFile = NULL;
char*	addrFile = NULL;
static	char*	logAppend = NULL;
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


#ifndef WIN32
// This function polls our parent process; if it is gone, shutdown.
void
check_parent()
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
check_session_cache()
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
handle_cookie_refresh()
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

char* global_dc_sinful() {
	if (daemonCore) {
		return daemonCore->InfoCommandSinfulString();
	} else {
		return NULL;
	}
}

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
void
DC_Exit( int status )
{
		// First, delete any files we might have created, like the
		// address file or the pid file.
	clean_files();

		// Log a message
	dprintf(D_ALWAYS,"**** %s (%s_%s) EXITING WITH STATUS %d\n",
		myName,myDistro->Get(),mySubSystem,status);

		// Now, delete the daemonCore object, since we allocated it. 
	delete daemonCore;
	daemonCore = NULL;

		// Free up the memory from the config hash table, too.
	clear_config();
	
		// Finally, exit with the status we were given.
	exit( status );
}


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
			fprintf( ADDR_FILE, "%s\n", CondorVersion() );
			fprintf( ADDR_FILE, "%s\n", CondorPlatform() );
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
	if( (PID_FILE = fopen(pidFile, "r")) ) {
		fscanf( PID_FILE, "%lu", &tmp_ul_int ); 
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
	sprintf( buf, "%s_LOG", mySubSystem );
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
set_dynamic_dir( char* param_name, const char* append_str )
{
	char* val;
	char newdir[_POSIX_PATH_MAX];

	val = param( (char *)param_name );
	if( ! val ) {
			// nothing to do
		return;
	}

		// First, create the new name.
	sprintf( newdir, "%s.%s", val, append_str );
	
		// Next, try to create the given directory, if it doesn't
		// already exist.
	make_dir( newdir );

		// Now, set our own config hashtable entry so we start using
		// this new directory.
	config_insert( (char *) param_name, newdir );

	// Finally, insert the _condor_<param_name> environment
	// variable, so our children get the right configuration.
	MyString env_str( "_" );
	env_str += myDistro->Get();
	env_str += "_";
	env_str += param_name;
	env_str += "=";
	env_str += newdir;
	char *env_cstr = strdup( env_str.Value() );
	if( putenv(env_cstr) < 0 ) {
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
	sprintf( buf, "%s-%d", inet_ntoa(*(my_sin_addr())), mypid );

	set_dynamic_dir( "LOG", buf );
	set_dynamic_dir( "SPOOL", buf );
	set_dynamic_dir( "EXECUTE", buf );

		// Final, evil hack.  Set the _condor_STARTD_NAME environment
		// variable, so that the startd will have a unique name. 
	sprintf( buf, "_%s_STARTD_NAME=%d", myDistro->Get(), mypid );
	char* env_str = strdup( buf );
	if( putenv(env_str) < 0 ) {
		fprintf( stderr, "ERROR: Can't add %s to the environment!\n", 
				 env_str );
		exit( 4 );
	}
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
			EXCEPT("cannot chdir to dir <%s>",ptmp);
		}
	} else {
		dprintf( D_FULLDEBUG, 
				 "No LOG directory specified in config file(s), "
				 "not calling chdir()\n" );
		return;
	}
#ifdef WIN32
	{
		// give our Win32 exception handler a filename for the core file
		char pseudoCoreFileName[MAX_PATH];
		sprintf(pseudoCoreFileName,"%s\\core.%s.WIN32",ptmp,
			mySubSystem);
		g_ExceptionHandler.SetLogFileName(pseudoCoreFileName);

		// set the path where our Win32 exception handler can find
		// debug symbols
		char *binpath = param("BIN");
		if ( binpath ) {
			sprintf(pseudoCoreFileName,"_NT_SYMBOL_PATH=%s",
				binpath);
			putenv( strdup(pseudoCoreFileName) );
			free(binpath);
		}
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
	char* tmp;
	int want_set_error_mode = TRUE;

	if( (tmp = param("CREATE_CORE_FILES")) ) {
#ifndef WIN32	
		if( *tmp == 't' || *tmp == 'T' ) {
			limit( RLIMIT_CORE, RLIM_INFINITY );
		} else {
			limit( RLIMIT_CORE, 0 );
		}
#endif
		if( *tmp == 'f' || *tmp == 'F' ) {
			want_set_error_mode = FALSE;
		}
		free( tmp );
	}

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


int
handle_off_fast( Service*, int, Stream* )
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
	return TRUE;
}

	
int
handle_off_graceful( Service*, int, Stream* )
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	return TRUE;
}

int
handle_off_peaceful( Service*, int, Stream* )
{
	// Peaceful shutdown is the same as graceful, except
	// there is no timeout waiting for things to finish.

	daemonCore->SetPeacefulShutdown(true);
	daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	return TRUE;
}

int
handle_set_peaceful_shutdown( Service*, int, Stream* )
{
	// If the master could send peaceful shutdown signals, it would
	// not be necessary to have a message for turning on the peaceful
	// shutdown toggle.  Since the master only sends fast and graceful
	// shutdown signals, condor_off is responsible for first turning
	// on peaceful shutdown in appropriate daemons.

	daemonCore->SetPeacefulShutdown(true);
	return TRUE;
}


int
handle_reconfig( Service*, int cmd, Stream* )
{
	if( cmd == DC_RECONFIG_FULL ) {
		dc_reconfig( true );
	} else {
		dc_reconfig( false );
	}		
	return TRUE;
}

int
handle_fetch_log( Service *service, int cmd, Stream *s )
{
	char *name = NULL;
	ReliSock *stream = (ReliSock*) s;
	int  total_bytes = 0;
	int result;
	int type;

	if( ! stream->code(type) ||
		! stream->code(name) || 
		! stream->end_of_message()) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: can't read log request\n" );
		free( name );
		return FALSE;
	}

	stream->encode();

	if(type!=DC_FETCH_LOG_TYPE_PLAIN) {
		dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: I don't know about log type %d!\n",type);
		result = DC_FETCH_LOG_RESULT_BAD_TYPE;
		stream->code(result);
		stream->end_of_message();
		return FALSE;
	}

	char *pname = (char*)malloc (strlen(name) + 5);
	strcpy (pname, name);
	strcat (pname, "_LOG");

	char *filename = param(pname);
	if(!filename) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: no parameter named %s\n",pname);
		result = DC_FETCH_LOG_RESULT_NO_NAME;
		stream->code(result);
		stream->end_of_message();
		return FALSE;
	}

	int fd = open(filename,O_RDONLY);
	if(fd<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: can't open file %s\n",filename);
		result = DC_FETCH_LOG_RESULT_CANT_OPEN;
		stream->code(result);
		stream->end_of_message();
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
handle_nop( Service*, int, Stream* )
{
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
				 "Got DC_CONFIG_VAL request for unknown parameter (%s)\n", 
				 param_name );
		free( param_name );
		if( ! stream->put("Not defined") ) {
			dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
			return FALSE;
		}
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
			return FALSE;
		}
		return FALSE;
	} else {
		free( param_name );
		if( ! stream->code(tmp) ) {
			dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
			free( tmp );
			return FALSE;
		}
		free( tmp );
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
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

	if( config && config[0] ) {
		to_check = config;
	} else {
		to_check = admin;
	}
	if( ! daemonCore->CheckConfigSecurity(to_check, (Sock*)stream) ) {
			// This request is insecure, so don't try to do anything
			// with it.  We can't return yet, since we want to send
			// back an rval indicating the error.
		free( admin );
		free( config );
		rval = -1;
		failed = true;
	} 

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
	daemonCore->Send_Signal( daemonCore->getpid(), SIGHUP );
}


void
unix_sigterm(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
}


void
unix_sigquit(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
}


void
unix_sigchld(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGCHLD );
}


void
unix_sigusr1(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGUSR1 );
}

void
unix_sigusr2(int)
{
	daemonCore->Send_Signal( daemonCore->getpid(), SIGUSR2 );
}

#endif /* ! WIN32 */



void
dc_reconfig( bool is_full )
{
		// Actually re-read the files...  Added by Derek Wright on
		// 12/8/97 (long after this function was first written... 
		// nice goin', Todd).  *grin*

		/* purify flags this as a stack bounds array read violation when 
			we're expecting to use the default argument. However, due to
			_craziness_ in the header file that declares this function
			as an extern "C" linkage with a default argument(WTF!?) while
			being called in a C++ context, something goes wrong. So, we'll
			just supply the errant argument. */
	config(0);

		// See if we're supposed to be allowing core files or not
	check_core_files();

	if( DynamicDirs ) {
		handle_dynamic_dirs();
	}

		// If we're supposed to be using our own log file, reset that here. 
	if( logDir ) {
		set_log_dir();
	}

	if( logAppend ) {
		handle_log_append( logAppend );
	}

	// Reinitialize logging system; after all, LOG may have been changed.
	dprintf_config(mySubSystem,2);
	
	// again, chdir to the LOG directory so that if we dump a core
	// it will go there.  the location of LOG may have changed, so redo it here.
	drop_core_in_log();

	// If we're doing a full reconfig, call ReInit to clear out the
	// DNS info we have cashed for the IP verify code. Also clear out
	// the passwd cache.
	if( is_full ) {
		daemonCore->ReInit();
		clear_passwd_cache();
	}

	// Re-drop the address file, if it's defined, just to be safe.
	drop_addr_file();

		// Re-drop the pid file, if it's requested, just to be safe.
	if( pidFile ) {
		drop_pid_file();
	}

	daemonCore->InitSettableAttrsLists();

		// If requested to do so in the config file, do a segv now.
		// This is to test our handling/writing of a core file.
	char* ptmp;
	if ( (ptmp=param("DROP_CORE_ON_RECONFIG")) && 
		 (*ptmp=='T' || *ptmp=='t') ) {
			// on purpose, derefernce a null pointer.
			ptmp = NULL;
			char segfault;	
			segfault = *ptmp; // should blow up here
			ptmp[0] = 'a';
			
			// should never make it to here!
			EXCEPT("FAILED TO DROP CORE");	
	}

	// call this daemon's specific main_config()
	main_config( is_full );
}

// This function initialize GSI (maybe other) authentication related stuff
void
dc_config_auth()
{
#if !defined(SKIP_AUTHENTICATION) && defined(GSI_AUTHENTICATION)
    int i;
	char *x509_env = "X509_USER_PROXY=";

    // First, if there is X509_USER_PROXY, we clear it.
    for (i=0;environ[i] != NULL;i++) {
         if (strncmp(environ[i], x509_env, strlen(x509_env) ) == 0) {
             for (;environ[i] != NULL;i++) {
                 environ[i] = environ[i+1];
			}
             break;
         }
    }

    // Next, we param the configuration file for GSI related stuff and 
    // set the corresponding environment variables for it

    char *pbuf = 0;
	char *proxy_buf = 0;
	char *cert_buf = 0;
	char *key_buf = 0;
	char *trustedca_buf = 0;

	char buffer[(_POSIX_PATH_MAX * 3)];
	memset(buffer, 0, (_POSIX_PATH_MAX * 3));

    
    // Here's how it works. If you define any of 
	// GSI_DAEMON_CERT, GSI_DAEMON_KEY, GSI_DAEMON_PROXY, or 
	// GSI_DAEMON_TRUSTED_CA_DIR, those will get stuffed into the
	// environment. 
	//
	// Everything else depends on GSI_DAEMON_DIRECTORY. If 
	// GSI_DAEMON_DIRECTORY is not defined, then only settings that are
	// defined above will be placed in the environment, so if you 
	// want the cert and host in a non-standard location, but want to use 
	// /etc/grid-security/certifcates as the trusted ca dir, only 
	// define GSI_DAEMON_CERT and GSI_DAEMON_KEY, and not
	// GSI_DAEMON_DIRECTORY and GSI_DAEMON_TRUSTED_CA_DIR
	//
	// If GSI_DAEMON_DIRECTORY is defined, condor builds a "reasonable" 
	// default out of what's already been defined and what it can 
	// construct from GSI_DAEMON_DIRECTORY  - ie  the trusted CA dir ends 
	// up as in $(GSI_DAEMON_DIRECTORY)/certificates, and so on
	// The proxy is not included in the "reasonable defaults" section

	// First, let's get everything we might want
    pbuf = param( STR_GSI_DAEMON_DIRECTORY );
    proxy_buf = param( STR_GSI_DAEMON_PROXY );
    cert_buf = param( STR_GSI_DAEMON_CERT );
    key_buf = param( STR_GSI_DAEMON_KEY );
    trustedca_buf = param( STR_GSI_DAEMON_TRUSTED_CA_DIR );


    if (pbuf) {

		if( !trustedca_buf) {
       	 sprintf( buffer, "%s=%s%ccertificates", STR_GSI_CERT_DIR, pbuf, 
					DIR_DELIM_CHAR);
       	 putenv( strdup( buffer ) );
		}

		if( !cert_buf ) {
        	sprintf( buffer, "%s=%s%chostcert.pem", STR_GSI_USER_CERT, pbuf, 
						DIR_DELIM_CHAR);
        	putenv( strdup ( buffer ) );
		}
	
		if (!key_buf ) {
        	sprintf(buffer,"%s=%s%chostkey.pem",STR_GSI_USER_KEY,pbuf, 
					DIR_DELIM_CHAR);
        	putenv( strdup ( buffer  ) );
		}

        free( pbuf );
    }

	if(proxy_buf) { 
		sprintf( buffer, "%s=%s", STR_GSI_USER_PROXY, proxy_buf);
		putenv (strdup( buffer ) );
		free(proxy_buf);
	}

	if(cert_buf) { 
		sprintf( buffer, "%s=%s", STR_GSI_USER_CERT, cert_buf);
		putenv (strdup( buffer ) );
		free(cert_buf);
	}

	if(key_buf) { 
		sprintf( buffer, "%s=%s", STR_GSI_USER_KEY, key_buf);
		putenv (strdup( buffer ) );
		free(key_buf);
	}

	if(trustedca_buf) { 
		sprintf( buffer, "%s=%s", STR_GSI_CERT_DIR, trustedca_buf);
		putenv (strdup( buffer ) );
		free(trustedca_buf);
	}


    pbuf = param( STR_GSI_MAPFILE );
    if (pbuf) {
        sprintf( buffer, "%s=%s", STR_GSI_MAPFILE, pbuf);
        putenv( strdup (buffer) );
        free(pbuf);
    }
#endif
}

int
handle_dc_sighup( Service*, int )
{
	dprintf( D_ALWAYS, "Got SIGHUP.  Re-reading config files.\n" );
	dc_reconfig( true );
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
	}
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
	int		wantsKill = FALSE, wantsQuiet = FALSE;


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

	// set myName to be argv[0] with the path stripped off
	myName = basename(argv[0]);
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
	main_pre_dc_init( argc, argv );

		// Make sure this is set, since DaemonCore needs it for all
		// sorts of things, and it's better to clearly EXCEPT here
		// than to seg fault down the road...
	if( ! mySubSystem ) {
		EXCEPT( "Programmer error: mySubSystem is NULL!" );
	}

		// Figure out if we're the master or not.  The master is a
		// special case daemon in many ways, so we want to just set a
		// flag that we can check instead of doing this strcmp all the
		// time.  -Derek Wright 1/13/98
	if( strcmp(mySubSystem,"MASTER") == 0 ) {
		is_master = 1;
	}

	// strip off any daemon-core specific command line arguments
	// from the front of the command line.
	i = 0;
	bool done = false;

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
		case 'l':		// specify Log directory 
			ptr++;
			if( ptr && *ptr ) {
				logDir = *ptr;
				dcargs += 2;
			} else {
				fprintf( stderr, "DaemonCore: ERROR: -log needs another "
						 "argument\n" );
				exit( 1 );
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
	config( wantsQuiet );

    // call dc_config_GSI to set GSI related parameters so that all
    // the daemons will know what to do.
    dc_config_auth();

		// See if we're supposed to be allowing core files or not
	check_core_files();

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
		dprintf_config(mySubSystem,2);
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
		// and close stdin, out, err if we are the MASTER.  
		if ( is_master ) {
			int	fd_null = open( NULL_FILE, 0 );
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
				close( fd );
			}

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
		daemonCore = new DaemonCore(503);
	} else {
		daemonCore = new DaemonCore();
	}

	if( DynamicDirs ) {
			// If we want to use dynamic dirs for log, spool and
			// execute, we now have our real pid, so we can actually
			// give it the correct name.

		handle_dynamic_dirs();

		if( logAppend ) {
			handle_log_append( logAppend );
		}
		
			// Actually set up logging.
		dprintf_config(mySubSystem,2);
	}

		// Now that we have the daemonCore object, we can finally
		// know what our pid is, so we can print out our opening
		// banner.  Plus, if we're using dynamic dirs, we have dprintf
		// configured now, so the dprintf()s will work.
	dprintf(D_ALWAYS,"******************************************************\n");
	dprintf(D_ALWAYS,"** %s (%s_%s) STARTING UP\n",
			myName,myDistro->GetUc(),mySubSystem);
	if( myFullName ) {
		dprintf( D_ALWAYS, "** %s\n", myFullName );
		free( myFullName );
		myFullName = NULL;
	}
	dprintf(D_ALWAYS,"** %s\n", CondorVersion());
	dprintf(D_ALWAYS,"** %s\n", CondorPlatform());
	dprintf(D_ALWAYS,"** PID = %lu\n",daemonCore->getpid());

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

	if (global_config_file != "") {
		dprintf(D_ALWAYS, "Using config file: %s\n", 
				global_config_file.GetCStr());
	} else {
		const char* env_name = EnvGetName( ENV_CONFIG );
		char* env = getenv( env_name );
		if( env ) {
			dprintf(D_ALWAYS, 
					"%s is set to '%s', not reading a config file\n",
					env_name, env );
		}
	}
	if (global_root_config_file != "") {
		dprintf(D_ALWAYS, "Using root config file: %s\n", 
				global_root_config_file.GetCStr());
	}
	if (local_config_files != "") {
		dprintf(D_ALWAYS, "Using local config files: %s\n", 
				local_config_files.GetCStr());
	}
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
	AllocConsole();
#endif

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

	main_pre_command_sock_init();

		// SETUP COMMAND SOCKET
	daemonCore->InitCommandSocket( command_port );
	
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
#ifndef WIN32
	daemonCore->Register_Signal( DC_SERVICEWAITPIDS, "DC_SERVICEWAITPIDS",
								(SignalHandlercpp)&DaemonCore::HandleDC_SERVICEWAITPIDS,
								"HandleDC_SERVICEWAITPIDS()",daemonCore,IMMEDIATE_FAMILY);
	daemonCore->Register_Signal( SIGCHLD, "SIGCHLD",
								 (SignalHandlercpp)&DaemonCore::HandleDC_SIGCHLD,
								 "HandleDC_SIGCHLD()",daemonCore,IMMEDIATE_FAMILY);
#endif

		// Install DaemonCore timers common to all daemons.

		//if specified on command line, set timer to gracefully exit
	if ( runfor ) {
		daemonCore->Register_Timer( runfor * 60, 0, 
				(TimerHandler)handle_dc_sigterm, "handle_dc_sigterm" );
		dprintf(D_ALWAYS,"Registered Timer for graceful shutdown in %d minutes\n",
				runfor );
	}

#ifndef WIN32
		// This timer checks if our parent is dead; if so, we shutdown.
		// We only do this on Unix; on NT we watch our parent via a different mechanism.
		// Also note: we do not want the master to exibit this behavior!
	if ( ! is_master ) {
		daemonCore->Register_Timer( 15, 120, 
				(TimerHandler)check_parent, "check_parent" );
	}
#endif

	daemonCore->Register_Timer( 0, 5 * 60,
				(TimerHandler)check_session_cache, "check_session_cache" );
	

	// set the timer for half the session duration, 
	// since we retain the old cookie. Also make sure
	// the value is atleast 1.
	int cookie_refresh = (param_integer("SEC_DEFAULT_SESSION_DURATION", 3600)/2)+1;

	daemonCore->Register_Timer( 0, cookie_refresh, 
				(TimerHandler)handle_cookie_refresh, "handle_cookie_refresh");

		// Install DaemonCore command handlers common to all daemons.
	daemonCore->Register_Command( DC_RECONFIG, "DC_RECONFIG",
								  (CommandHandler)handle_reconfig,
								  "handle_reconfig()", 0, WRITE );

	daemonCore->Register_Command( DC_RECONFIG_FULL, "DC_RECONFIG_FULL",
								  (CommandHandler)handle_reconfig,
								  "handle_reconfig()", 0, ADMINISTRATOR );

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

	daemonCore->Register_Command( DC_OFF_PEACEFUL, "DC_OFF_PEACEFUL",
								  (CommandHandler)handle_off_peaceful,
								  "handle_off_peaceful()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_SET_PEACEFUL_SHUTDOWN, "DC_SET_PEACEFUL_SHUTDOWN",
								  (CommandHandler)handle_set_peaceful_shutdown,
								  "handle_set_peaceful_shutdown()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_NOP, "DC_NOP",
								  (CommandHandler)handle_nop,
								  "handle_nop()", 0, READ );

	daemonCore->Register_Command( DC_FETCH_LOG, "DC_FETCH_LOG",
								  (CommandHandler)handle_fetch_log,
								  "handle_fetch_log()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_INVALIDATE_KEY, "DC_INVALIDATE_KEY",
								  (CommandHandler)handle_invalidate_key,
								  "handle_invalidate_key()", 0, ALLOW );

	// Call daemonCore's ReInit(), which clears the cached DNS info.
	// It also initializes some stuff, which is why we call it now. 
	// This function will set a timer to call itself again 8 hours
	// after it's been called, so that even after an asynchronous
	// reconfig, we still wait 8 hours instead of just using a
	// periodic timer.  -Derek Wright <wright@cs.wisc.edu> 1/28/99 
	daemonCore->ReInit();


		// Initialize the StringLists that contain the attributes we
		// will allow people to set with condor_config_val from
		// various kinds of hosts (ADMINISTRATOR, CONFIG, WRITE, etc). 
	daemonCore->InitSettableAttrsLists();

	// call the daemon's main_init()
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
	bool done = false;
	for( ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++)
	{
		if( ptr[0][0] != '-' ) {
			break;
		}
		switch( ptr[0][1] ) {
		case 'a':		// Append to the log file name.
			ptr++;
			break;
		case 'b':		// run in Background (default)
			break;
		case 'c':		// specify directory where Config file lives
			ptr++;
			break;
		case 'd':		// Dynamic local directories
			break;
		case 'f':		// run in Foreground
			Foreground = 1;
			break;
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
		case 't':		// log to Terminal (stderr)
			break;
		case 'v':		// display Version info and exit
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
			break;
		default:
			done = true;
			break;	
		}
		if( done ) {
			break;		// break out of for loop
		}
	}
	if ( (Foreground != 1) && 
			// the starter sets mySubSystem in main_pre_dc_init(), so 
			// be careful when handling it this early in the game.
			( mySubSystem != NULL && 
			(strcmp(mySubSystem,"MASTER") == 0)) ) {
		main_init(-1,NULL);	// passing the master main_init a -1 will register as an NT service
		return 1;
	} else {
		return(dc_main(argc,argv));
	}
}
#endif // of ifdef WIN32

