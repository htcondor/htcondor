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
#include "condor_config.h"
#include "util_lib_proto.h"
#include "basename.h"
#include "my_hostname.h"
#include "condor_version.h"
#include "limit.h"
#include "condor_email.h"
#include "sig_install.h"

#include "condor_debug.h"

#define _NO_EXTERN_DAEMON_CORE 1	
#include "condor_daemon_core.h"

#ifdef WIN32
#include "exphnd.WIN32.h"
#endif

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
bool	DynamicDirs = false;


#define CONFIG_ATTRIBUTE_SECURITY 0
/* 
   We need to figure out how we really want to handle this.  As it
   stands now, it's a little clumsy, not fully thought out, and unset
   was broken.  For now, we'll just rely on HOSTALLOW_CONFIG and root
   config files.  We should have a more complete solution at some
   point, just not before 6.2.0.  Derek Wright 9/7/00
*/
#if CONFIG_ATTRIBUTE_SECURITY
static StringList* SecureAttrList = NULL;
static void init_secure_attr_list( void );
#endif

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
		daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGTERM );
	}
}
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
void
DC_Exit( int status )
{
		// First, delete any files we might have created, like the
		// address file or the pid file.
	clean_files();

		// Log a message
	dprintf(D_ALWAYS,"**** %s (CONDOR_%s) EXITING WITH STATUS %d\n",
		myName,mySubSystem,status);

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
handle_log_append( char* logAppend )
{
	if( ! logAppend ) {
		return;
	}
	char *tmp1, *tmp2;
	char buf[100];
	sprintf( buf, "%s_LOG", mySubSystem );
	if( !(tmp1 = param(buf)) ) { 
		EXCEPT( "%s not defined!", buf );
	}
	tmp2 = (char*)malloc( (strlen(tmp1) + strlen(logAppend) + 2)
						  * sizeof(char) );
	if( !tmp2 ) {	
		EXCEPT( "Out of memory!" );
	}
	sprintf( tmp2, "%s.%s", tmp1, logAppend );
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
	char* env_str = (char*) malloc( (strlen(param_name) + 10 +
									 strlen(newdir)) * sizeof(char) ); 
	sprintf( env_str, "_condor_%s=%s", param_name, newdir );
	if( putenv(env_str) < 0 ) {
		fprintf( stderr, "ERROR: Can't add %s to the environment!\n", 
				 env_str );
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
	sprintf( buf, "_condor_STARTD_NAME=%d", mypid );
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
		EXCEPT("No LOG directory specified in config file(s)");
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
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGQUIT );
	return TRUE;
}

	
int
handle_off_graceful( Service*, int, Stream* )
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGTERM );
	return TRUE;
}

	
int
handle_reconfig( Service*, int, Stream* )
{
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGHUP );
	return TRUE;
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



#if CONFIG_ATTRIBUTE_SECURITY
bool
check_config_security( const char* config, Sock* sock )
{
	char *name, *tmp;
	char buf[128];
	char* ip_str;

	if( ! (name = strdup(config)) ) {
		EXCEPT( "Out of memory!" );
	}
	tmp = strchr( name, '=' );
	if( ! tmp ) {
		tmp = strchr( name, ':' );
	}
	if( ! tmp ) {
		dprintf( D_ALWAYS, 
				 "ERROR in handle_config(): can't parse config string \"%s\"\n", 
				 config );
		free( name );
		return false;
	}

		// Trim off white space
	*tmp = ' ';
	while( isspace(*tmp) ) {
		*tmp = '\0';
		tmp--;
	}

		// Now, name should point to a NULL-terminated version of the
		// attribute name we're trying to set.  This is what we must
		// compare against the NETWORK_SECURE_ATTRIBUTES list.
	if( ! SecureAttrList->contains_anycase(name) ) {
			// Everything's cool.  Continue on.
		free( name );
		return true;
	}

		// If we're still here, someone is trying to set one of the
		// special settings they shouldn't be.  Try to let as many
		// people know as possible.  

		// Grab a pointer to this string, since it's a little bit
		// expensive to re-compute.
	ip_str = sock->endpoint_ip_str();
		// Upper-case-ify the string for everything we print out.
	strupr(name);

		// First, log it.
	dprintf( D_ALWAYS,
			 "WARNING: Someone at %s is trying to set \"%s\"\n",
			 ip_str, name );
	dprintf( D_ALWAYS, 
			 "WARNING: Potential security problem, request refused\n" );
	dprintf( D_ALWAYS, 
			 "WARNING: Emailing the Condor Administrator\n" );

		// Now, try to email the Condor Admins
	sprintf( buf, "Potential security attack from %s", ip_str );
	FILE* email = email_admin_open( buf );
	if( ! email ) {
		dprintf( D_ALWAYS, 
				 "ERROR: Can't send email to the Condor Administrator\n" );
		free( name );
		return false;
	}
	fprintf( email, 
			 "Someone at %s is attempting to modify the value of:\n", ip_str );
	fprintf( email, "\"%s\" on %s (%s)\n\n", name, my_full_hostname(),
			 inet_ntoa(*(my_sin_addr())) );
	fprintf( email, 
			 "This is a potential security attack.  You should probably add\n" );
	fprintf( email, 
			 "\"%s\" to your HOSTDENY_READ, HOSTDENY_WRITE, and\n",
			 ip_str );
	fprintf( email, "HOSTDENY_ADMINSITRATOR settings.  "
			 "See the Condor Manual for\n" );
	fprintf( email, "details on these config file entries.\n\n" );
	fprintf( email, "If possible, try to further investigate this potential "
			 "attack.\n" );
	email_close( email );

	free( name );
	return false;
}


void
init_secure_attr_list( void )
{
	char buf[64];
	int i;
	char* tmp;

	if( SecureAttrList ) {
		delete( SecureAttrList );
	}
	SecureAttrList = new StringList();

	if( (tmp = param("NETWORK_SECURE_ATTRIBUTES")) ) {
		SecureAttrList->initializeFromString( tmp );
	} else {
			// The defaults

			// Paths
		SecureAttrList->append( "RELEASE_DIR" );
		SecureAttrList->append( "BIN" );
		SecureAttrList->append( "SBIN" );
		
			// Daemons and programs spawned by Condor
		SecureAttrList->append( "MASTER" );
		SecureAttrList->append( "STARTD" );
		SecureAttrList->append( "SCHEDD" );
		SecureAttrList->append( "COLLECTOR" );
		SecureAttrList->append( "NEGOTIATOR" );
		SecureAttrList->append( "KBDD" );
		SecureAttrList->append( "EVENTD" );
		SecureAttrList->append( "CKPT_SERVER" );
		SecureAttrList->append( "PREEN" );
		SecureAttrList->append( "MAIL" );
		SecureAttrList->append( "PVMD" );
		SecureAttrList->append( "PVMGS" );
		SecureAttrList->append( "SHADOW" );
		SecureAttrList->append( "SHADOW_CARMI" );
		SecureAttrList->append( "SHADOW_GLOBUS" );
		SecureAttrList->append( "SHADOW_MPI" );
		SecureAttrList->append( "SHADOW_NT" );
		SecureAttrList->append( "SHADOWNT" );
		SecureAttrList->append( "SHADOW_PVM" );
		SecureAttrList->append( "STARTER" );
		for( i=0; i<10; i++ ) {
			sprintf( buf, "STARTER_%d", i );
			SecureAttrList->append( buf );
			sprintf( buf, "ALTERNATE_STARTER_%d", i );
			SecureAttrList->append( buf );
		}

			// Other settings
		SecureAttrList->append( "DAEMON_LIST" );
		SecureAttrList->append( "LOCAL_ROOT_CONFIG_FILE" );
		SecureAttrList->append( "PREEN_ARGS" );
	}
		// In either event, we don't want to let condor_config_val
		// -set change NETWORK_SECURE_ATTRIBUTES itself!
	SecureAttrList->append( "NETWORK_SECURE_ATTRIBUTES" );
}
#endif /* CONFIG_ATTRIBUTE_SECURITY */

int
handle_config( Service *, int cmd, Stream *stream )
{
	char *admin = NULL, *config = NULL;
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

#if CONFIG_ATTRIBUTE_SECURITY 
	if( config && config[0] ) {
			// They're trying to set something, so check to make sure
			// we want to allow that
		if( ! check_config_security(config, (Sock*)stream) ) {
				// This config string is insecure, so don't try to do 
				// anything with it.  We can't return yet, since we
				// want to send back an rval indicating the error. 
			free( admin );
			free( config );
			rval = -1;
			failed = true;
		}
	} 
#endif /* CONFIG_ATTRIBUTE_SECURITY */

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

#endif /* ! WIN32 */


int
handle_dc_sighup( Service*, int )
{
	dprintf( D_ALWAYS, "Got SIGHUP.  Re-reading config files.\n" );

		// Actually re-read the files...  Added by Derek Wright on
		// 12/8/97 (long after this function was first written... 
		// nice goin', Todd).  *grin*
	config();

		// See if we're supposed to be allowing core files or not
	check_core_files();

	if( DynamicDirs ) {
		handle_dynamic_dirs();
	}

		// If we're supposed to be using our own log file, reset that here. 
	if( logDir ) {
		set_log_dir();
	}

	// Reinitialize logging system; after all, LOG may have been changed.
	dprintf_config(mySubSystem,2);
	
	// again, chdir to the LOG directory so that if we dump a core
	// it will go there.  the location of LOG may have changed, so redo it here.
	drop_core_in_log();

	// call daemon-core's ReInit
	daemonCore->ReInit();

	// Re-drop the address file, if it's defined, just to be safe.
	drop_addr_file();

		// Re-drop the pid file, if it's requested, just to be safe.
	if( pidFile ) {
		drop_pid_file();
	}

#if CONFIG_ATTRIBUTE_SECURITY
	init_secure_attr_list();
#endif

		// If requested to do so in the config file, do a segv now.
		// This is to test our handling/writing of a core file.
	char* ptmp;
	if ( (ptmp=param("DROP_CORE_ON_RECONFIG")) && 
		 (*ptmp=='T' || *ptmp=='t') ) {
			// on purpose, derefernce a null pointer.
			ptmp = NULL;
			char segfault;	
			segfault = *ptmp; // should blow up here
			
			// should never make it to here!
			EXCEPT("FAILED TO DROP CORE");	
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

#if defined(WIN32) && 0
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
	int		wantsKill = FALSE, wantsQuiet = FALSE;
	char	*logAppend = NULL;
	int runfor = 0; //allow cmd line option to exit after *runfor* minutes


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
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif // of ifndef WIN32

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
				ptmp1 = (char *)malloc( strlen(ptmp) + 25 );
				if ( ptmp1 ) {
					sprintf(ptmp1,"CONDOR_CONFIG=%s",ptmp);
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
				fprintf( stderr, 
						 "DaemonCore: ERROR: -log needs another argument.\n" );
				fprintf( stderr, 
						 "   Please specify a directory to use for logging.\n" );
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
		daemonCore = new DaemonCore(503);
	} else {
		daemonCore = new DaemonCore();
	}

	if( DynamicDirs ) {
			// If we want to use dynamic dirs for log, spool and
			// execute, we now have our real pid, so we can actually
			// give it the correct name.

		handle_dynamic_dirs();

			// Actually set up logging.
		dprintf_config(mySubSystem,2);
	}

		// Now that we have the daemonCore object, we can finally
		// know what our pid is, so we can print out our opening
		// banner.  Plus, if we're using dynamic dirs, we have dprintf
		// configured now, so the dprintf()s will work.
	dprintf(D_ALWAYS,"******************************************************\n");
	dprintf(D_ALWAYS,"** %s (CONDOR_%s) STARTING UP\n",myName,mySubSystem);
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

		// SETUP COMMAND SOCKET
	daemonCore->InitCommandSocket( command_port );
	
		// Install DaemonCore signal handlers common to all daemons.
	daemonCore->Register_Signal( DC_SIGHUP, "DC_SIGHUP", 
								 (SignalHandler)handle_dc_sighup,
								 "handle_dc_sighup()" );
	daemonCore->Register_Signal( DC_SIGQUIT, "DC_SIGQUIT", 
								 (SignalHandler)handle_dc_sigquit,
								 "handle_dc_sigquit()" );
	daemonCore->Register_Signal( DC_SIGTERM, "DC_SIGTERM", 
								 (SignalHandler)handle_dc_sigterm,
								 "handle_dc_sigterm()" );
#ifndef WIN32
	daemonCore->Register_Signal( DC_SERVICEWAITPIDS, "DC_SERVICEWAITPIDS",
								(SignalHandlercpp)&DaemonCore::HandleDC_SERVICEWAITPIDS,
								"HandleDC_SERVICEWAITPIDS()",daemonCore,IMMEDIATE_FAMILY);
	daemonCore->Register_Signal( DC_SIGCHLD, "DC_SIGCHLD",
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

		// Install DaemonCore command handlers common to all daemons.
	daemonCore->Register_Command( DC_RECONFIG, "DC_RECONFIG",
								  (CommandHandler)handle_reconfig,
								  "handle_reconfig()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_CONFIG_VAL, "DC_CONFIG_VAL",
								  (CommandHandler)handle_config_val,
								  "handle_config_val()", 0, READ );
		// Deprecated name for it.
	daemonCore->Register_Command( CONFIG_VAL, "CONFIG_VAL",
								  (CommandHandler)handle_config_val,
								  "handle_config_val()", 0, READ );

	daemonCore->Register_Command( DC_CONFIG_PERSIST, "DC_CONFIG_PERSIST",
								  (CommandHandler)handle_config,
								  "handle_config()", 0, CONFIG_PERM );

	daemonCore->Register_Command( DC_CONFIG_RUNTIME, "DC_CONFIG_RUNTIME",
								  (CommandHandler)handle_config,
								  "handle_config()", 0, CONFIG_PERM );

	daemonCore->Register_Command( DC_OFF_FAST, "DC_OFF_FAST",
								  (CommandHandler)handle_off_fast,
								  "handle_off_fast()", 0, ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_GRACEFUL, "DC_OFF_GRACEFUL",
								  (CommandHandler)handle_off_graceful,
								  "handle_off_graceful()", 0, ADMINISTRATOR );

	// Call daemonCore's ReInit(), which clears the cached DNS info.
	// It also initializes some stuff, which is why we call it now. 
	// This function will set a timer to call itself again 8 hours
	// after it's been called, so that even after an asynchronous
	// reconfig, we still wait 8 hours instead of just using a
	// periodic timer.  -Derek Wright <wright@cs.wisc.edu> 1/28/99 
	daemonCore->ReInit();


#if CONFIG_ATTRIBUTE_SECURITY
		// Initialize the StringList that contains the attributes we
		// don't want to allow anyone to set with condor_config_val. 
	init_secure_attr_list();
#endif

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
	if ( (Foreground != 1) && (strcmp(mySubSystem,"MASTER") == 0) ) {
		main_init(-1,NULL);	// passing the master main_init a -1 will register as an NT service
		return 1;
	} else {
		return(dc_main(argc,argv));
	}
}
#endif // of ifdef WIN32

