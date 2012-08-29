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
#include "starter_common.h"
#include "condor_debug.h"
#include "condor_string.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "condor_file_info.h"
#include "get_port_range.h"
#include "name_tab.h"
#include "proto.h"
#include "condor_io.h"
#include "startup.h"
#include "fileno.h"
#include "condor_environ.h"
#include "../condor_privsep/condor_privsep.h"
#include "../condor_procd/proc_family_client.h"
#include "condor_pers.h"
#include "procd_config.h"


#if defined(AIX32)
#	include <sys/id.h>
#endif

const mode_t REGULAR_FILE_MODE =
	S_IRUSR | S_IWUSR |					// rw- for user
	S_IRGRP |					        // r-- for group
	S_IROTH;						    // r-- for other

const mode_t EXECUTABLE_FILE_MODE =
	S_IRUSR | S_IWUSR | S_IXUSR |		// rwx for user
	S_IRGRP |           S_IXGRP |		// r-x for group
	S_IROTH |           S_IXOTH;		// r-x for other

const mode_t LOCAL_DIR_MODE =
	S_IRUSR | S_IWUSR | S_IXUSR |		// rwx for user
	S_IRGRP | S_IWGRP | S_IXGRP |		// rwx for group
	S_IROTH | S_IWOTH | S_IXOTH;		// rwx for other

/* Do I know the actual name of the core file yet, or not? On some machines
it is 'core', and on others (namely redhat 9) it is 'core.<pid>'. I'm going
to check for both forms. */
enum 
{
	CORE_NAME_UNKNOWN,
	CORE_NAME_KNOWN,
};

#include "user_proc.h"

extern char	*Execute;			 // Name of directory where user procs execute
extern MyString SlotName;        // Slot where we're allocated (SMP)
extern int ExecTransferAttempts; // attempts at bringing over the initial exec.

extern "C" {
	void _updateckpt( char *, char *, char * );
}
void open_std_file( int which );
void set_iwd();

/*
  With bytestream checkpointing, there is no updating of checkpoints - the
  user process does everything on its own.
*/
#if !defined(NO_CKPT)
extern "C" {
void
_updateckpt( char *, char *, char * )
{
	EXCEPT( "Should never get here" );
}
}
#endif


extern sigset_t	ChildSigMask;
extern NameTable SigNames;
extern char *ThisHost;
extern char *InitiatingHost;
extern StdUnivSock	*SyscallStream;	// stream to shadow for remote system calls
extern int EventSigs[];
int UserProc::proc_index = 1;

/* These are the remote system calls we use in this file */
extern "C" int REMOTE_CONDOR_get_a_out_name(char *&path);
extern "C" int REMOTE_CONDOR_getwd_special (char *&path_name);
extern "C" int REMOTE_CONDOR_free_fs_blocks(char *pathname);
extern "C" int REMOTE_CONDOR_get_std_file_info(int fd, char *&logical_name);
extern "C" int REMOTE_CONDOR_get_file_info_new(char *logical_name,
	char *&actual_url);
extern "C" int REMOTE_CONDOR_std_file_info(int fd, char *&logical_name,
	int *pipe_fd);
extern "C" int REMOTE_CONDOR_report_error(char *msg);
extern "C" int REMOTE_CONDOR_get_iwd(char *&path);

#ifndef MATCH
#define MATCH 0	// for strcmp
#endif

extern NameTable JobClasses;
extern NameTable ProcStates;

int
renice_self( const char* param_name ) {
	int i = 0;
	int r = 0;
#ifndef WIN32
	char* ptmp = param( param_name );
	if ( ptmp ) {
		i = atoi(ptmp);
		if ( i > 0 && i < 20 ) {
			r = nice(i);
		} else if ( i >= 20 ) {
			i = 19;
			r = nice(i);
		} else {
			i = 0;
		}
		free(ptmp);
	}
#endif
	return r;
}

UserProc::~UserProc()
{
	delete [] cmd;
	delete [] local_dir;
	delete [] cur_ckpt;
	if ( core_name ) {
		delete [] core_name;
	}
	free( m_a_out );
}


void
UserProc::display()
{
	dprintf( D_ALWAYS, "User Process %d.%d {\n", cluster, proc );

	dprintf( D_ALWAYS, "  cmd = %s\n", cmd );

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string);
	dprintf( D_ALWAYS, "  args = %s\n", args_string.Value() );

	MyString env_string;
	env_obj.getDelimitedStringForDisplay(&env_string);
	dprintf( D_ALWAYS, "  env = %s\n", env_string.Value() );

	dprintf( D_ALWAYS, "  local_dir = %s\n", local_dir );
	dprintf( D_ALWAYS, "  cur_ckpt = %s\n", cur_ckpt );
	dprintf( D_ALWAYS, "  core_name = %s\n", 
		core_name==NULL?"(either 'core' or 'core.<pid>')":core_name);

	dprintf( D_ALWAYS, "  uid = %d, gid = %d\n", uid, gid );

	dprintf( D_ALWAYS, "  v_pid = %d\n", v_pid );
	if( pid ) {
		dprintf( D_ALWAYS, "  pid = %d\n", pid );
	} else {
		dprintf( D_ALWAYS, "  pid = (NOT CURRENTLY EXECUTING)\n" );
	}

	display_bool( D_ALWAYS, "  exit_status_valid", exit_status_valid );
	if( exit_status_valid ) {
		dprintf( D_ALWAYS, "  exit_status = 0x%x\n", exit_status );
	} else {
		dprintf( D_ALWAYS, "  exit_status = (NEVER BEEN EXECUTED)\n" );
	}

	display_bool( D_ALWAYS, "  ckpt_wanted", ckpt_wanted );
	display_bool( D_ALWAYS, "  coredump_limit_exists", coredump_limit_exists );
	if( coredump_limit_exists ) {
		dprintf( D_ALWAYS, "  coredump_limit = %d\n", coredump_limit );
	}

	dprintf( D_ALWAYS, "  soft_kill_sig = %d\n", soft_kill_sig );
	dprintf( D_ALWAYS, "  job_class = %s\n", JobClasses.get_name(job_class) );

	dprintf( D_ALWAYS, "  state = %s\n", ProcStates.get_name(state) );
	display_bool( D_ALWAYS, "  new_ckpt_created", new_ckpt_created );
	display_bool( D_ALWAYS, "  ckpt_transferred", ckpt_transferred );
	display_bool( D_ALWAYS, "  core_created", core_created );
	display_bool( D_ALWAYS, "  core_transferred", core_transferred );
	display_bool( D_ALWAYS, "  exit_requested", exit_requested );
	dprintf( D_ALWAYS, "  image_size = %d blocks\n", image_size );

	dprintf( D_ALWAYS, "  user_time = %ld\n", (long)user_time );
	dprintf( D_ALWAYS, "  sys_time = %ld\n", (long)sys_time );

	dprintf( D_ALWAYS, "  guaranteed_user_time = %ld\n", (long)guaranteed_user_time );
	dprintf( D_ALWAYS, "  guaranteed_sys_time = %ld\n", (long)guaranteed_sys_time );

	dprintf( D_ALWAYS, "}\n" );

}


/*
  We have been given a filename describing where we should get our
  executable from.  That name could be of the form "<hostname>:<pathname>",
  or it could just be <pathname>.  Also the <pathname> part could contain
  macros to be looked up in our config file.  We do that macro expansion
  here, and also check the <hostname> part.  If the hostname refers
  to our own host, we set "on_this_host" to TRUE and if it refers to
  the submitting host, we set it to FALSE.  For now, it is an error
  if it refers to some third host.  We then ruturn a pointer to the
  macro expanded pathname in a static data area.

  The pathname could also be an AFS file, in which case we can also access
  the file with a symlink.  The form is "/afs/...".
*/
char *
UserProc::expand_exec_name( int &on_this_host )
{
	char	*host_part;
	char	*path_part;
	char	*tmp;
	char	*a_out;
	int		status;

	a_out = NULL;
	status = REMOTE_CONDOR_get_a_out_name( a_out );
	if( status < 0 || a_out == NULL ) {
		EXCEPT( "Can't get name of a.out file" );
	}
	if( strchr(a_out,':') ) {		// form is <hostname>:<path>
		host_part = strtok( a_out, " \t:" );
		if( host_part == NULL ) {
			EXCEPT( "Can't find host part" );
		}
		path_part = strtok( NULL, " \t:");
		if( path_part == NULL ) {
			EXCEPT( "Can't find path part" );
		}
		if( strcmp(host_part,ThisHost) == MATCH )  {
			on_this_host = TRUE;
		} else {
			if( strcmp(host_part,InitiatingHost) == MATCH ) {
				on_this_host = FALSE;
			} else {
				EXCEPT( "Unexpected host_part \"%s\", ThisHost %s", 
					   host_part, ThisHost );
			}
		}
	} else if( strncmp("/afs",a_out,4) == MATCH ) {
		on_this_host = TRUE;
		path_part = a_out;
	} else {	// form is <path>
		on_this_host = FALSE;
		path_part = a_out;
	}

		// expand macros in the pathname part
	tmp = macro_expand( path_part );
	free( m_a_out );
	m_a_out = strdup( tmp );
	FREE( tmp );
	free(a_out);
			

	if( on_this_host ) {
		if( access(m_a_out,X_OK) == 0 ) {
			dprintf( D_ALWAYS, "Executable is located on this host\n" );
		} else {
			dprintf( D_FAILURE|D_ALWAYS,
				"Executable located on this host - but not executable\n"
			);
			on_this_host = FALSE;
		}
	} else {
		dprintf( D_ALWAYS, "Executable is located on submitting host\n" );
	}
	dprintf( D_ALWAYS, "Expanded executable name is \"%s\"\n", m_a_out );
	return m_a_out;
}

/*
  The executable file is on this host already, so just make a symbolic
  link to it.
*/
int
UserProc::link_to_executable( char *src, int & error_code )
{
	int status;

	errno = 0;
	status =  symlink( src, cur_ckpt );
	if( status < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS,
			"Can't create sym link from \"%s\" to \"%s\", errno = %d\n",
			src, cur_ckpt, errno
		);
	} else {
		dprintf( D_ALWAYS,
			"Created sym link from \"%s\" to \"%s\"\n", src, cur_ckpt
		);
	}
	error_code = errno;
	return status;
}

/*
  The executable file is on the submitting host.  Transfer a copy
  to this machine.
*/
int
UserProc::transfer_executable( char *src, int &error_code )
{
	int		status;
	int		attempts = 0;
	int tmp_errno;

	dprintf( D_ALWAYS, "Going to try %d %s at getting the initial executable\n",
		ExecTransferAttempts, ExecTransferAttempts==1?"attempt":"attempts" );

	do
	{
		errno = 0;
		status = get_file( src, cur_ckpt, EXECUTABLE_FILE_MODE );
		tmp_errno = errno;

		attempts++;

		if( status < 0 ) {
			dprintf( D_FAILURE|D_ALWAYS,
				"Failed to fetch orig ckpt file \"%s\" into \"%s\", "
				"attempt = %d, errno = %d\n", src, cur_ckpt, attempts, errno );
		} else {
			dprintf( D_ALWAYS,
				"Fetched orig ckpt file \"%s\" into \"%s\" with %d %s\n", 
				src, cur_ckpt, attempts, attempts==1?"attempt":"attempts" );
		}
	}
	while( (status < 0) && (attempts < ExecTransferAttempts) );

	error_code = tmp_errno;
	return status;
}

/*
  Check to see if an executable file is properly linked for execution
  with the Condor remote system call library.
*/
int
UserProc::linked_for_condor()
{
		// if env var _CONDOR_NOCHECK=1, then do not do this check.
		// this allows expert users to submit shell scripts to the
		// STANDARD universe.
	MyString env_nocheck_var = EnvGetName( ENV_NOCHECK );
	MyString env_nocheck_val;
	if(env_obj.GetEnv( env_nocheck_var, env_nocheck_val )) {
		if(env_nocheck_val.Value()[0] == '1') {
			return TRUE;
		}
	}

	if( sysapi_magic_check(cur_ckpt) < 0 ) {
		state = BAD_MAGIC;
		dprintf( D_FAILURE|D_ALWAYS, "sysapi_magic_check() failed\n" );
		return FALSE;
	}

	// Don't look for symbol "MAIN" in vanilla jobs or PVM processes
	if( job_class != CONDOR_UNIVERSE_PVM && job_class !=  CONDOR_UNIVERSE_VANILLA ) {	
		if( sysapi_symbol_main_check(cur_ckpt) < 0 ) {
			state = BAD_LINK;
			dprintf( D_ALWAYS, "symbol_main_check() failed\n" );
			return FALSE;
		}
	}

	dprintf( D_FULLDEBUG, "Done verifying executable file\n" );
	return TRUE;
}

int
UserProc::fetch_ckpt()
{
	int		on_this_host;
	char	*new_path;
	int		status;
	int		error_code;

	new_path = expand_exec_name( on_this_host );

	if( on_this_host ) {
		status = link_to_executable( new_path, error_code );
	} else {
		status = transfer_executable( new_path, error_code );
	}

	if( status < 0 ) {
		state = CANT_FETCH;
		exit_status = error_code;
		return FALSE;
	}

	switch( job_class ) {

	  case CONDOR_UNIVERSE_STANDARD:
		if( linked_for_condor() ) {
			state = RUNNABLE;
			return TRUE;
		} else {
			return FALSE;
		}

	  case CONDOR_UNIVERSE_VANILLA:
	  case CONDOR_UNIVERSE_PVM:
	  case CONDOR_UNIVERSE_PVMD:
		state = RUNNABLE;
		return TRUE;

	  default:
		EXCEPT( "Unknown job class (%d)", job_class );
	}

	// We can never get here
	return FALSE;
}

#define MATCH 0			/* result of strcmp */
#define EQUAL '='		/* chars looked for during parsing */
#define SPACE ' '
#define TAB '\t'
#define SEMI ';'


void
UserProc::execute()
{
	ArgList new_args;
	char    **argv;
	char	**argp;
	char	**envp;
	sigset_t	sigmask;
	MyString	a_out_name;
	MyString	shortname;
	int		user_syscall_fd;
	const	int READ_END = 0;
	const	int WRITE_END = 1;
	int		pipe_fds[2];
	FILE	*cmd_fp;
	char	buf[128];
	ReliSock	*new_reli = NULL;

	pipe_fds[0] = -1;
	pipe_fds[1] = -1;

	shortname.formatstr( "condor_exec.%d.%d", cluster, proc );
	a_out_name.formatstr( "%s/%s/%s", Execute, local_dir, shortname.Value() );

		// Set up arg vector according to class of job
	switch( job_class ) {

	  case CONDOR_UNIVERSE_STANDARD:
		if( pipe(pipe_fds) < 0 ) {
			EXCEPT( "pipe()" );}

			dprintf( D_ALWAYS, "Pipe built\n" );
		
			// The user process should not try to read commands from
			// 0, 1, or 2 since we'll be using the commands to redirect
			// those.
		if( pipe_fds[READ_END] < 14 ) {
			dup2( pipe_fds[READ_END], 14 );
			close( pipe_fds[READ_END] );
			pipe_fds[READ_END] = 14;
		}
		dprintf( D_ALWAYS, "New pipe_fds[%d,%d]\n", pipe_fds[0], pipe_fds[1] );
		sprintf( buf, "%d", pipe_fds[READ_END] );
		dprintf( D_ALWAYS, "cmd_fd = %s\n", buf );

		new_args.AppendArg(shortname);
		new_args.AppendArg("-_condor_cmd_fd");
		new_args.AppendArg(buf);
		break;

	  case CONDOR_UNIVERSE_PVM:
#if 1
		EXCEPT( "Don't know how to deal with PVM jobs" );
#else
		new_args.AppendArg(shortname);
		new_args.AppendArg("-1");
		new_args.AppendArg(in);
		new_args.AppendArg(out);
		new_args.AppendArg(err);
#endif
		break;

	  case CONDOR_UNIVERSE_VANILLA:
	  	if (privsep_enabled()) {
			EXCEPT("Don't know how to deal with Vanilla jobs");
		}
		new_args.AppendArg(shortname.Value());
		break;
	}

	new_args.AppendArgsFromArgList(args);

		// take care of USER_JOB_WRAPPER
	support_job_wrapper(a_out_name,&new_args);

	MyString exec_name;
	exec_name = a_out_name;

	// If privsep is turned on, then we need to use the PrivSep
	// Switchboard to launch the job
	//
	FILE* switchboard_in_fp;
	FILE* switchboard_err_fp;
	int switchboard_child_in_fd;
	int switchboard_child_err_fd;
	if (privsep_enabled()) {

		// create the pipes that we'll use to communicate
		//
		if (!privsep_create_pipes(switchboard_in_fp,
		                          switchboard_child_in_fd,
		                          switchboard_err_fp,
		                          switchboard_child_err_fd)) {
			EXCEPT("can't launch job: privsep_create_pipes failure");
		}
	}

	argv = new_args.GetStringArray();

		// Set an environment variable that tells the job where it may put scratch data
		// even if it moves to a different directory.

		// get the environment vector
	envp = env_obj.getStringArray();

		// We may run more than one of these, so each needs its own
		// remote system call connection to the shadow
	if( job_class == CONDOR_UNIVERSE_PVM ) {
		new_reli = NewConnection( v_pid );
		user_syscall_fd = new_reli->get_file_desc();
	}

		// print out arguments to execve
	dprintf( D_ALWAYS, "Calling execve( \"%s\"", exec_name.Value() );
	for( argp = argv; *argp; argp++ ) {							// argv
		dprintf( D_ALWAYS | D_NOHEADER, ", \"%s\"", *argp );
	}
	dprintf( D_ALWAYS | D_NOHEADER, ", 0" );
	for( argp = envp; *argp; argp++ ) {							// envp
		dprintf( D_ALWAYS | D_NOHEADER, ", \"%s\"", *argp );
	}
	dprintf( D_ALWAYS | D_NOHEADER, ", 0 )\n" );


	if( (pid = fork()) < 0 ) {
		EXCEPT( "fork" );
	}

	if( pid == 0 ) {	// the child

			// Block only these 3 signals which have special meaning for
			// checkpoint/restart purposes.  Leave other signals ublocked
			// so that if we get an exception during the restart process,
			// we will get a core file to debug.
		sigemptyset( &sigmask );
		// for some reason if we block these, the user process is unable
		// to unblock some or all of them.
#if 0
		sigaddset( &sigmask, SIGUSR1 );
		sigaddset( &sigmask, SIGUSR2 );
		sigaddset( &sigmask, SIGTSTP );
#endif
		sigprocmask( SIG_SETMASK, &sigmask, 0 );

			// renice
		renice_self( "JOB_RENICE_INCREMENT" );

			// make certain the syscall sockets which are being passed
			// to the user job are setup to be blocking sockets.  this
			// is done by calling timeout(0) CEDAR method.
			// we must do this because the syscall lib does _not_ 
			// expect to see any failures due to errno EAGAIN...
		if ( SyscallStream ) {
			SyscallStream->timeout(0);
		}
		if ( new_reli ) {
			new_reli->timeout(0);
		}

			// If I'm using privledge separation, connect to the procd.
			// we need to register a family with the procd for the newly
			// created process, so that the ProcD will allow us to send
			// signals to it
			//
		if (privsep_enabled() == true) {
			MyString procd_address;
			bool response;
			bool ret;
			ProcFamilyClient pfc;

			procd_address = get_procd_address();
			ret = pfc.initialize(procd_address.Value());
			if (ret == false) {
				EXCEPT("Failure to initialize the ProcFamilyClient object");
			}

			ret = pfc.register_subfamily(getpid(), getppid(), 60, response);

			if (ret == false) {
				EXCEPT("Could not communicate with procd. Aborting.");
			}

			if (response == false) {
				EXCEPT("Procd refused to register job subfamily. Aborting.");
			}
		}

			// If there is a requested coresize for this job, enforce it.
			// Do it before the set_priv_final to ensure root can alter 
			// the coresize to the requested amount. Otherwise, just
			// use whatever the current default is.
		if (coredump_limit_exists == TRUE) {
			limit( RLIMIT_CORE, coredump_limit, CONDOR_HARD_LIMIT, "max core size" );
		}

			// child process should have only it's submitting uid, and cannot
			// switch back to root or some other uid.  
			// It'd be nice to check for errors here, but
			// unfortunately, we can't, since this only returns the
			// previous priv state, not whether it worked or not. 
			//  -Derek Wright 4/30/98
		set_user_priv_final();

		switch( job_class ) {
		  
		  case CONDOR_UNIVERSE_STANDARD:
			// if we're using PrivSep, the chdir here could fail. instead,
			// we pass the job's IWD to the switchboard via pipe
			//
		  	if (!privsep_enabled()) {
				if( chdir(local_dir) < 0 ) {
					EXCEPT( "chdir(%s)", local_dir );
				}
			}
			close( pipe_fds[WRITE_END] );
			break;

		  case CONDOR_UNIVERSE_PVM:
			if( chdir(local_dir) < 0 ) {
				EXCEPT( "chdir(%s)", local_dir );
			}
			close( pipe_fds[WRITE_END] );
			dup2( user_syscall_fd, RSC_SOCK );
			break;

		  case CONDOR_UNIVERSE_VANILLA:
			set_iwd();
			open_std_file( 0 );
			open_std_file( 1 );
			open_std_file( 2 );

			(void)close( RSC_SOCK );
			(void)close( CLIENT_LOG );

			break;
		}

			// Make sure we're not root
		if( getuid() == 0 ) {
				// EXCEPT( "We're about to start as root, aborting." );
				// You can't see this error message at all.  So, just
				// exit(4), which is what EXCEPT normally gives. 
			exit( 4 );
		}

#if defined( LINUX ) && (defined(I386) || defined(X86_64))
		// adjust the execution domain of the child to be suitable for
		// checkpointing.
		patch_personality();
#endif 

			// if we're using privsep, we'll exec the PrivSep Switchboard
			// first, which is setuid; it will then setuid to the user we
			// give it and exec the real job
			//
		if (privsep_enabled()) {
			close(fileno(switchboard_in_fp));
			close(fileno(switchboard_err_fp));
			privsep_get_switchboard_command("exec",
			                                switchboard_child_in_fd,
			                                switchboard_child_err_fd,
			                                exec_name,
			                                new_args);
			deleteStringArray(argv);
			argv = new_args.GetStringArray();
		}

			// Everything's ready, start it up...
		errno = 0;
		execve( exec_name.Value(), argv, envp );

			// A successful call to execve() never returns, so it is an
			// error if we get here.  A number of errors are possible
			// but the most likely is that there is insufficient swap
			// space to start the new process.  We don't try to log
			// anything, since we have the UID/GID of the job's owner
			// and cannot write into the log files...
		exit( JOB_EXEC_FAILED );
	}

		// The parent

		// PrivSep - we have at this point only spawned the switchboard
		// with the "exec" command. we need to use our pipe to it in
		// order to tell it how to execute the user job, and then use
		// the error pipe to make sure everything worked
		//
	if (privsep_enabled()) {

		close(switchboard_child_in_fd);
		close(switchboard_child_err_fd);

		privsep_exec_set_uid(switchboard_in_fp, uid);
		privsep_exec_set_path(switchboard_in_fp, exec_name.Value());
		privsep_exec_set_args(switchboard_in_fp, new_args);
		privsep_exec_set_env(switchboard_in_fp, env_obj);
		privsep_exec_set_iwd(switchboard_in_fp, local_dir);
		privsep_exec_set_inherit_fd(switchboard_in_fp, pipe_fds[0]);
		privsep_exec_set_inherit_fd(switchboard_in_fp, RSC_SOCK);
		privsep_exec_set_inherit_fd(switchboard_in_fp, CLIENT_LOG);
		privsep_exec_set_is_std_univ(switchboard_in_fp);
		fclose(switchboard_in_fp);

		if (!privsep_get_switchboard_response(switchboard_err_fp)) {
			EXCEPT("error starting job: "
			           "privsep get_switchboard_response failure");
		}
	}

	dprintf( D_ALWAYS, "Started user job - PID = %d\n", pid );
	if( job_class != CONDOR_UNIVERSE_VANILLA ) {
			// Send the user process its startup environment conditions
		close( pipe_fds[READ_END] );
		cmd_fp = fdopen( pipe_fds[WRITE_END], "w" );
		dprintf( D_ALWAYS, "cmd_fp = %p\n", cmd_fp );

		if( is_restart() ) {
#if 1
			fprintf( cmd_fp, "restart\n" );
			dprintf( D_ALWAYS, "restart\n" );
#else
			fprintf( cmd_fp, "restart %s\n", target_ckpt );
			dprintf( D_ALWAYS, "restart %s\n", target_ckpt );
#endif
			fprintf( cmd_fp, "end\n" );
			dprintf( D_ALWAYS, "end\n" );
		} else {
			fprintf( cmd_fp, "end\n" );
			dprintf( D_ALWAYS, "end\n" );
		}
		fclose( cmd_fp );
	}

	deleteStringArray(argv);
	deleteStringArray(envp);
	state = EXECUTING;

	if( new_reli ) {
		delete new_reli;
	}

	// removed some vanilla-specific code here
	//
	ASSERT(job_class != CONDOR_UNIVERSE_VANILLA);
}

// inline void
void
do_unlink( const char *name )
{
	while( unlink(name) < 0 ) {
		if( errno != ETXTBSY ) {
			dprintf( D_ALWAYS,
				"Can't unlink \"%s\" - errno = %d\n", name, errno
			);
			return;
		}
	}
	dprintf( D_ALWAYS, "Unlinked \"%s\"\n", name );
}


void
UserProc::delete_files()
{
	do_unlink( cur_ckpt );

	if (core_name != NULL) {
		do_unlink( core_name );
	}

	dprintf( D_ALWAYS, "Removing directory \"%s\"\n", local_dir );
	if (privsep_enabled()) {
		// again, the PrivSep Switchboard expects a full path for
		// the "remove dir" operation
		//
		MyString local_dir_path;
		local_dir_path.formatstr("%s/%s", Execute, local_dir);
		if (!privsep_remove_dir(local_dir_path.Value())) {
			dprintf(D_ALWAYS,
			        "privsep_remove_dir failed to remove %s\n",
			        local_dir_path.Value());
		}
	}
	else {
		if( rmdir(local_dir) < 0 ) {
			dprintf( D_ALWAYS,
				"Can't remove directory \"%s\" - errno = %d\n",
			        local_dir,
			        errno);
		}
	}
}

void
UserProc::handle_termination( int exit_st )
{
	/* XXX I wonder what the kernel does if a core file is created such that 
		the entire path up to the core file is path max, and when you add the
		pid, it overshoots the path max... */
	MyString corebuf;
	struct stat sbuf;
	int core_name_type = CORE_NAME_UNKNOWN;
	pid_t user_job_pid;

	exit_status = exit_st;
	exit_status_valid = TRUE;
	accumulate_cpu_time();

	if( exit_requested && job_class == CONDOR_UNIVERSE_VANILLA ) { // job exited by request
		dprintf( D_FAILURE|D_ALWAYS, "Process exited by request\n" );
		state = NON_RUNNABLE;
	} else if( WIFEXITED(exit_status) ) { 
                                     // exited on own accord with some status
		dprintf( D_FAILURE|D_ALWAYS,
			"Process %d exited with status %d\n", pid, WEXITSTATUS(exit_status)
		);
		if( WEXITSTATUS(exit_status) == JOB_EXEC_FAILED ) {
			dprintf( D_FAILURE|D_ALWAYS,
				"EXEC of user process failed, probably insufficient swap\n"
			);
			state = NON_RUNNABLE;
		} else {
			state = NORMAL_EXIT;
			commit_cpu_time();
		}
	} else {
		dprintf( D_ALWAYS,
			"Process %d killed by signal %d\n", pid, WTERMSIG(exit_status)
		);
		switch( WTERMSIG(exit_status) ) {
		  case SIGUSR2:			// ckpt and vacate exit 
			dprintf( D_FAILURE|D_ALWAYS, "Process exited for checkpoint\n" );
			state = CHECKPOINTING;
			commit_cpu_time();
			break;
		  case SIGQUIT:			// exited for a checkpoint
			dprintf( D_FAILURE|D_ALWAYS, "Process exited for checkpoint\n" );
			state = CHECKPOINTING;
			/*
			  For bytestream checkpointing:  the only way the process exits
			  with signal SIGQUIT is if has transferred a checkpoint
			  successfully.
			*/
			ckpt_transferred = TRUE;
			break;
		  case SIGUSR1:
		  case SIGKILL:				// exited by request - no ckpt
			dprintf( D_FAILURE|D_ALWAYS, "Process exited by request\n" );
			state = NON_RUNNABLE;
			break;
		  default:					// exited abnormally due to signal
			dprintf( D_FAILURE|D_ALWAYS, "Process exited abnormally\n" );
			state = ABNORMAL_EXIT;
			commit_cpu_time();		// should this be here on ABNORMAL_EXIT ???? -Todd
		}
			
	}
	user_job_pid = pid;
	pid = 0;

	// removed some vanilla-specific code here
	//
	ASSERT(job_class != CONDOR_UNIVERSE_VANILLA);

	priv_state priv;

	switch( state ) {

	    case CHECKPOINTING:
			core_created = FALSE;
			ckpt_transferred = TRUE;
			break;

        case ABNORMAL_EXIT:
			priv = set_root_priv();	// need to be root to access core file

			/* figure out the name of the core file, it could be either
				"core.pid" which we will try first, or "core". We'll take
				the first one we find, and ignore the other one. */

			/* the default */
			core_name_type = CORE_NAME_UNKNOWN;

			/* try the 'core.pid' form first */
			corebuf.formatstr( "%s/core.%lu", local_dir, 
				(unsigned long)user_job_pid);
			if (stat(corebuf.Value(), &sbuf) >= 0) {
				core_name_type = CORE_NAME_KNOWN;
			} else {
				/* now try the normal 'core' form */
				corebuf.formatstr( "%s/core", local_dir);
				if (stat(corebuf.Value(), &sbuf) >= 0) {
					core_name_type = CORE_NAME_KNOWN;
				}
			}

			/* If I know a core file name, then set up its retrival */
			core_created = FALSE; // assume false if unknown
			if (core_name_type == CORE_NAME_KNOWN) {

				/* AFAICT, This is where core_name should be set up,
					if it had already been setup somehow, then that is a logic
					error and I'd need to find out why/where */
				if (core_name != NULL) {
					EXCEPT( "core_name should have been NULL!\n" );
				}

				/* this gets deleted when this objects destructs */
				core_name = new char [ corebuf.Length() + 1 ];
				if(core_name != NULL) strcpy( core_name,corebuf.Value() );

				/* core_is_valid checks to make sure it isn't a symlink and
					returns false if it is */
					if( core_is_valid(corebuf.Value()) == TRUE) {
					dprintf( D_FAILURE|D_ALWAYS, 
						"A core file was created: %s\n", corebuf.Value());
					core_created = TRUE;
				} else {
					dprintf( D_FULLDEBUG, "No core file was created\n" );
					core_created = FALSE;
					if (core_name != NULL) {
						// remove any incomplete core, or possible symlink
						(void)unlink(corebuf.Value());	
					}
				}
			}

			set_priv(priv);

			break;

		default:
			dprintf( D_FULLDEBUG, "No core file was created\n" );
			core_created = FALSE;
			break;
	}

}

void
UserProc::send_sig( int sig )
{
	if( !pid ) {
		dprintf( D_FULLDEBUG, "UserProc::send_sig(): "
			"Can't send signal to user job pid 0!\n");
		return;
	}

	if (privsep_enabled() == true) {
		send_sig_privsep(sig);
	} else {
		send_sig_no_privsep(sig);
	}

	/* record the fact we unsuspended the job. */
	if (sig == SIGCONT) {
		pids_suspended = 0;
	}

	if (SigNames.get_name(sig) != NULL) {
		dprintf( D_ALWAYS, "UserProc::send_sig(): "
			"Sent signal %s to user job %d\n",
			SigNames.get_name(sig), pid);
	} else {
		dprintf( D_ALWAYS, "UserProc::send_sig(): "
			"Unknown signum %d sent to user job %d\n",
			sig, pid);
	}
}


// This function assumes that privledge separation has already been determined
// to be available.
void
UserProc::send_sig_privsep( int sig )
{
	ProcFamilyClient pfc;
	char *tmp = NULL;
	bool response;
	bool ret;

	tmp = param("PROCD_ADDRESS");
	if (tmp != NULL) {
		ret = pfc.initialize(tmp);
		if (ret == false) {
			EXCEPT("Failure to initialize the ProcFamilyClient object");
		}
		free(tmp);
	} else {
		EXCEPT("privsep is enabled, but PROCD_ADDRESS is not defined!");
	}

	// continue the family first
	ret = pfc.continue_family(pid, response);
	if (ret == false) {
		EXCEPT("UserProc::send_sig_privsep(): Couldn't talk to procd while "
			"trying to continue the user job.");
	}

	if (response == false) {
		EXCEPT("UserProc::send_sig_privsep(): "
			"Procd refused to continue user job");
	}

	// now (unless it is a continue) send the requested signal
	if (sig != SIGCONT) {
		ret = pfc.signal_process(pid, sig, response);

		if (ret == false) {
			EXCEPT("UserProc::send_sig_privsep(): Couldn't talk to procd while "
				"trying to send signal %d to the user job.", sig);
		}

		if (response == false) {
			EXCEPT("UserProc::send_sig_privsep(): "
				"Procd refused to send signal %d to user job", sig);
		}
	}
}

void
UserProc::send_sig_no_privsep( int sig )
{
	priv_state	priv;

		// We don't want to be root going around killing things or we
		// might do something we'll regret in the morning. -Derek 8/29/97
	priv = set_user_priv();  

	// removed some vanilla-specific code here
	//
	ASSERT(job_class != CONDOR_UNIVERSE_VANILLA);

	if ( job_class != CONDOR_UNIVERSE_VANILLA )
	{
		// Make sure the process can receive the signal. So let's send it a
		// SIGCONT first if applicable.
		if( sig != SIGCONT ) {
			if( kill(pid,SIGCONT) < 0 ) {
				set_priv(priv);
				if( errno == ESRCH ) {	// User proc already exited
					dprintf( D_ALWAYS, "UserProc::send_sig_no_privsep(): "
						"Tried to send signal SIGCONT to user "
						"job %d, but that process doesn't exist.\n", pid);
					return;
				}
				perror("kill");
				EXCEPT( "kill(%d,SIGCONT)", pid  );
			}
			/* standard jobs can't fork, so.... */
			pids_suspended = 1;
			dprintf( D_ALWAYS, "UserProc::send_sig_no_privsep(): "
				"Sent signal SIGCONT to user job %d\n", pid);
		}

		if( kill(pid,sig) < 0 ) {
			set_priv(priv);
			if( errno == ESRCH ) {	// User proc already exited
				dprintf( D_ALWAYS, "UserProc::send_sig_no_privsep(): "
					"Tried to send signal %d to user job "
				 	"%d, but that process doesn't exist.\n", sig, pid);
				return;
			}
			perror("kill");
			EXCEPT( "kill(%d,%d)", pid, sig );
		}
	}

	set_priv(priv);
}



void
display_bool( int debug_flags, const char *name, int value )
{
	dprintf( debug_flags, "%s = %s\n", name, value ? "TRUE" : "FALSE" );
}


void
UserProc::store_core()
{
	int		core_size;
	char	*virtual_working_dir = NULL;
	MyString new_name;
	int		free_disk;
	priv_state	priv;

	if( !core_created ) {
		dprintf( D_FAILURE|D_ALWAYS, "No core file to send - probably ran out of disk\n");
		return;
	}

	if ( core_name == NULL ) {
		EXCEPT( "UserProc::store_core() asked to store unnamed core file that we knew existed!" );
	}

	priv = set_root_priv();
	core_size = physical_file_size( core_name );
	set_priv(priv);

	if( coredump_limit_exists ) {
		if( core_size > coredump_limit ) {
			dprintf( D_ALWAYS, "Core file size exceeds user defined limit\n" );
			dprintf( D_ALWAYS, "*NOT* sending core file.\n" );
			return;
		}
	}

	if( REMOTE_CONDOR_getwd_special(virtual_working_dir) < 0 ) {
		EXCEPT( "REMOTE_CONDOR_getwd_special(virtual_working_dir = %s)",
			(virtual_working_dir != NULL)?virtual_working_dir:"(null)");
	}

	free_disk = REMOTE_CONDOR_free_fs_blocks( virtual_working_dir);
	if( free_disk < 0 ) {
		EXCEPT( "REMOTE_CONDOR_free_fs_blocks(virtual_working_dir = %s)",
			(virtual_working_dir != NULL)?virtual_working_dir:"(null)");
	}

	dprintf( D_ALWAYS, "Core file size is %d kbytes\n", core_size );
	dprintf( D_ALWAYS, "Free disk on submitting machine is %d kbytes\n",
																free_disk );

	if( free_disk > core_size ) {
		new_name.formatstr( "%s/core.%d.%d", virtual_working_dir, cluster, proc);
		dprintf( D_ALWAYS, "Transferring core file to \"%s\"\n", new_name.Value() );
		priv = set_root_priv();
		send_a_file( core_name, new_name.Value(), REGULAR_FILE_MODE );
		set_priv(priv);
		core_transferred = TRUE;
	} else {
		dprintf( D_ALWAYS, "*NOT* Transferring core file\n" );
		core_transferred = FALSE;
	}
	free( virtual_working_dir );
}

void
UserProc::accumulate_cpu_time()
{
	struct tms	current;
	static struct tms	previous;

	(void)times( &current );
	user_time += current.tms_cutime - previous.tms_cutime;
	sys_time += current.tms_cstime - previous.tms_cstime;
	previous = current;
}

void
UserProc::commit_cpu_time()
{
	guaranteed_user_time = user_time;
	guaranteed_sys_time = sys_time;
}

void *
UserProc::accumulated_rusage()
{
	return bsd_rusage(user_time,sys_time);
}

void *
UserProc::guaranteed_rusage()
{
	return bsd_rusage(guaranteed_user_time, guaranteed_sys_time);
}

void *
UserProc::bsd_exit_status()
{
	return bsd_status( exit_status, state, ckpt_transferred, core_transferred );
}

void
UserProc::suspend()
{
	send_sig( SIGSTOP );
	state = _SUSPENDED;
}

void
UserProc::resume()
{
	send_sig( SIGCONT );
	state = EXECUTING;
}

void
UserProc::request_ckpt()
{
	send_sig( SIGTSTP );
}

void
UserProc::request_periodic_ckpt()
{
	send_sig( SIGUSR2 );
}

void
UserProc::request_exit()
{
	exit_requested = TRUE;
	send_sig( soft_kill_sig );
}

void
UserProc::kill_forcibly()
{
	send_sig( SIGKILL );
}

void
UserProc::make_runnable()
{
	if( state != CHECKPOINTING ) {
		EXCEPT( "make_runnable() - state != CHECKPOINTING" );
	}
	state = RUNNABLE;
	restart = TRUE;
}


extern "C"
{
int
pre_open( int, int, int ) { return 0; }
}


/*
  Open a standard file (0, 1, or 2), given its fd number.
*/
void
open_std_file( int fd )
{
	char	*logical_name = NULL;
	char	*physical_name = NULL;
	char	*file_name;
	int	flags;
	int	success;
	int	real_fd;

	/* First, try the new set of remote lookups */

	success = REMOTE_CONDOR_get_std_file_info( fd, logical_name );
	if(success>=0) {
		success = REMOTE_CONDOR_get_file_info_new(logical_name, physical_name);
	}

	/* If either of those fail, fall back to the old way */

	if(success<0) {
		success = REMOTE_CONDOR_std_file_info( fd, logical_name, &real_fd );
		if(success<0) {
			EXCEPT("Couldn't get standard file info!");
		}
		physical_name = (char *)malloc(strlen(logical_name)+7);
		sprintf(physical_name,"local:%s",logical_name);
	}

	if(fd==0) {
		flags = O_RDONLY;
	} else {
		flags = O_CREAT|O_WRONLY|O_TRUNC;
	}

	/* The starter doesn't have the whole elaborate url mechanism. */
	/* So, just strip off the pathname from the end */

	file_name = strrchr(physical_name,':');
	if(file_name) {
		file_name++;
	} else {
		file_name = physical_name;
	}

	/* Check to see if appending is forced */

	if(strstr(physical_name,"append:")) {
		flags = flags | O_APPEND;
		flags = flags & ~O_TRUNC;
	}

	/* Now, really open the file. */

	real_fd = safe_open_wrapper_follow(file_name,flags,0);
	if(real_fd<0) {
		// Some things, like /dev/null, can't be truncated, so
		// try again w/o O_TRUNC. Jim, Todd and Derek 5/26/99
		flags = flags & ~O_TRUNC;
		real_fd = safe_open_wrapper_follow(file_name,flags,0);
	}

	if(real_fd<0) {
		MyString err;
		err.formatstr("Can't open \"%s\": %s", file_name,strerror(errno));
		dprintf(D_ALWAYS,"%s\n",err.Value());
		REMOTE_CONDOR_report_error(const_cast<char *>(err.Value()));
		exit( 4 );
	} else {
		if( real_fd != fd ) {
			dup2( real_fd, fd );
		}
	}
	free( logical_name );
	free( physical_name );
}

UserProc::UserProc( STARTUP_INFO &s ) :
	cluster( s.cluster ),
	proc( s.proc ),
	m_a_out( NULL ),
	core_name( NULL ),
	uid( s.uid ),
	gid( s.gid ),
	v_pid( s.virt_pid ),
	pid( 0 ),
	job_class( s.job_class ),
	state( NEW ),
	user_time( 0 ),
	sys_time( 0 ),
	exit_status_valid( FALSE ),
	exit_status( 0 ),
	ckpt_wanted( s.ckpt_wanted ),
	soft_kill_sig( s.soft_kill_sig ),
	new_ckpt_created( FALSE ),
	ckpt_transferred( FALSE ),
	core_created( FALSE ),
	core_transferred( FALSE ),
	exit_requested( FALSE ),
	image_size( -1 ),
	guaranteed_user_time( 0 ),
	guaranteed_sys_time( 0 ),
	pids_suspended( -1 )
{
	MyString	buf;
	mode_t 	omask;

	cmd = new char [ strlen(s.cmd) + 1 ];
	strcpy( cmd, s.cmd );

	// Since we are adding to the argument list, we may need to deal
	// with platform-specific arg syntax in the user's args in order
	// to successfully merge them with the additional args.
	args.SetArgV1SyntaxToCurrentPlatform();

	MyString args_errors;
	if(!args.AppendArgsV1or2Raw(s.args_v1or2,&args_errors)) {
		EXCEPT("ERROR: Failed to parse arguments string: %s\n%s\n",
			   args_errors.Value(),s.args_v1or2);
	}

		// set up environment as an object
	MyString env_errors;
	if(!env_obj.MergeFromV1or2Raw( s.env_v1or2,&env_errors )) {
		EXCEPT("ERROR: Failed to parse environment string: %s\n%s\n",
			   env_errors.Value(),s.env_v1or2);
	}

		// add name of SMP slot (from startd) into environment
	setSlotEnv(&env_obj);

	/* Port regulation for user job */
	int low_port, high_port;

		// assume outgoing port range
	if (get_port_range(TRUE, &low_port, &high_port) == TRUE) {
		buf.formatstr( "_condor_LOWPORT=%d", low_port);
		env_obj.SetEnv(buf.Value());
		buf.formatstr( "_condor_HIGHPORT=%d", high_port);
		env_obj.SetEnv(buf.Value());
	}
	/* end - Port regulation for user job */

	if( param_boolean("BIND_ALL_INTERFACES", true) ) {
		buf.formatstr( "_condor_BIND_ALL_INTERFACES=TRUE" );
	} else {
		buf.formatstr( "_condor_BIND_ALL_INTERFACES=FALSE" );
	}
	env_obj.SetEnv(buf.Value());
	

		// Generate a directory where process can run and do its checkpointing
	omask = umask(0);
	buf.formatstr( "dir_%d", getpid() );
	local_dir = new char [ buf.Length() + 1 ];
	strcpy( local_dir, buf.Value() );
	if (privsep_enabled()) {
		// the Switchboard expects a full path to privsep_create_dir
		MyString local_dir_path;
		local_dir_path.formatstr("%s/%s", Execute, local_dir);
		if (!privsep_create_dir(get_condor_uid(), local_dir_path.Value())) {
			EXCEPT("privsep_create_dir failure");
		}
		if (chmod(local_dir_path.Value(), LOCAL_DIR_MODE) == -1) {
			EXCEPT("chmod failure after privsep_create_dir");
		}
	}
	else {
		if( mkdir(local_dir,LOCAL_DIR_MODE) < 0 ) {
			EXCEPT( "mkdir(%s,0%o)", local_dir, LOCAL_DIR_MODE );
		}
	}
	(void)umask(omask);

		// Now that we know what the local_dir is, put the path into
		// the environment so the job knows where it is
	MyString scratch_env;
	scratch_env.formatstr("CONDOR_SCRATCH_DIR=%s/%s",Execute,local_dir);
	env_obj.SetEnv(scratch_env.Value());

	buf.formatstr( "%s/condor_exec.%d.%d", local_dir, cluster, proc );
	cur_ckpt = new char [ buf.Length() + 1 ];
	strcpy( cur_ckpt, buf.Value() );

		// Find out if user wants checkpointing
#if defined(NO_CKPT)
	ckpt_wanted = FALSE;
	dprintf(D_ALWAYS,
			"This platform doesn't implement checkpointing yet\n"
	);
#else
	ckpt_wanted = s.ckpt_wanted;
#endif

	restart = s.is_restart;
	coredump_limit_exists = s.coredump_limit_exists;
	coredump_limit = s.coredump_limit;
}

void
set_iwd()
{
	char	*iwd = NULL;

	if( REMOTE_CONDOR_get_iwd(iwd) < 0 ) {
		REMOTE_CONDOR_report_error(const_cast<char*>("Can't determine initial working directory"));
		exit( 4 );
	}

	if( chdir(iwd) < 0 ) {

		int rval = -1;
		for (int count = 0; count < 360 && rval == -1 &&
				 (errno == ETIMEDOUT || errno == ENODEV); count++) {
			dprintf(D_FAILURE|D_ALWAYS, "Connection timed out on chdir(%s), trying again"
					" in 5 seconds\n", iwd);
			sleep(5);
			rval = chdir(iwd);
		}
		if (errno == ETIMEDOUT || errno == ENODEV) {
			EXCEPT("Connection timed out for 30 minutes on chdir(%s)", iwd);
		}
			
		MyString err;
		err.formatstr( "Can't open working directory \"%s\": %s", iwd,
			    strerror(errno) );
		REMOTE_CONDOR_report_error(const_cast<char *>(err.Value()));
		exit( 4 );
	}
	free( iwd );
}

extern "C"	int MappingFileDescriptors();

extern "C" int
MarkOpen( const char * /* file */, int /* flags */, int fd, int /* is_remote */ )
{
	if( MappingFileDescriptors() ) {
		EXCEPT( "MarkOpen() called, but should never be used!" );
	}
	return fd;
}
