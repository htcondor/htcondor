/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "debug.h"
#include "condor_constants.h"
#include "condor_string.h"
#include "condor_config.h"
#include "condor_jobqueue.h"
#include "condor_uid.h"
#include "condor_file_info.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "name_tab.h"
#include "proto.h"
#include "condor_sys.h"
#include "startup.h"

#if defined(AIX32)
#	include <sys/id.h>
#endif

typedef unsigned short u_short;
typedef unsigned char u_char;
typedef unsigned long u_long;
#include "condor_fix_socket.h"

extern "C" {
#include <netinet/in.h>
}
#include "fileno.h"
#include "condor_rsc.h"

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


#include "user_proc.h"
#include "condor_sys.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

extern char	*Execute;			// Name of directory where user procs execute

extern "C" {
	void _updateckpt( char *, char *, char * );
	int free_fs_blocks(char *filename);
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
_updateckpt( char *a, char *b, char *c )
{
	EXCEPT( "Should never get here" );
}
}
#endif

extern sigset_t	ChildSigMask;
extern NameTable SigNames;
extern char *ThisHost;
extern char *InitiatingHost;
extern XDR	*SyscallStream;			// XDR stream to shadow for remote system calls
int UserProc::proc_index = 1;

#ifndef MATCH
#define MATCH 0	// for strcmp
#endif

extern NameTable JobClasses;
extern NameTable ProcStates;
extern int Running_PVMD;

int connect_to_port( int );

UserProc::~UserProc()
{
	delete [] cmd;
	delete [] args;
	delete [] env;
	delete [] local_dir;
	delete [] cur_ckpt;
	delete [] core_name;
}

void
UserProc::display()
{
	dprintf( D_ALWAYS, "User Process %d.%d {\n", cluster, proc );

	dprintf( D_ALWAYS, "  cmd = %s\n", cmd );
	dprintf( D_ALWAYS, "  args = %s\n", args );
	dprintf( D_ALWAYS, "  env = %s\n", env );

	dprintf( D_ALWAYS, "  local_dir = %s\n", local_dir );
	dprintf( D_ALWAYS, "  cur_ckpt = %s\n", cur_ckpt );
	dprintf( D_ALWAYS, "  core_name = %s\n", core_name );

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
	dprintf( D_ALWAYS, "  image_size = %d blocks\n", image_size );

	dprintf( D_ALWAYS, "  user_time = %d\n", user_time );
	dprintf( D_ALWAYS, "  sys_time = %d\n", sys_time );

	dprintf( D_ALWAYS, "  guaranteed_user_time = %d\n", guaranteed_user_time );
	dprintf( D_ALWAYS, "  guaranteed_sys_time = %d\n", guaranteed_sys_time );

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
	static char answer[ _POSIX_PATH_MAX ];
	char	*host_part;
	char	*path_part;
	char	*tmp;
	char	a_out[ _POSIX_PATH_MAX ];
	int		status;

	status = REMOTE_syscall( CONDOR_get_a_out_name, a_out );
	if( status < 0 ) {
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
	strcpy( answer, tmp );
	FREE( tmp );
			

	if( on_this_host ) {
		if( access(answer,X_OK) == 0 ) {
			dprintf( D_ALWAYS, "Executable is located on this host\n" );
		} else {
			dprintf( D_ALWAYS,
				"Executable located on this host - but not executable\n"
			);
			on_this_host = FALSE;
		}
	} else {
		dprintf( D_ALWAYS, "Executable is located on submitting host\n" );
	}
	dprintf( D_ALWAYS, "Expanded executable name is \"%s\"\n", answer );
	return answer;
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
		dprintf( D_ALWAYS,
			"Can't create sym link from \"%s\" to \"%s\", errno = %d",
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

	errno = 0;
	status = get_file( src, cur_ckpt, EXECUTABLE_FILE_MODE );

	if( status < 0 ) {
		dprintf( D_ALWAYS,
			"Failed to fetch orig ckpt file \"%s\" into \"%s\", errno = %d\n",
			src, cur_ckpt, errno
		);
	} else {
		dprintf( D_ALWAYS,
			"Fetched orig ckpt file \"%s\" into \"%s\"\n", src, cur_ckpt
		);
	}

	error_code = errno;
	return status;
}

/*
  Check to see if an executable file is properly linked for execution
  with the Condor remote system call library.
*/
int
UserProc::linked_for_condor()
{
	if( magic_check(cur_ckpt) < 0 ) {
		state = BAD_MAGIC;
		dprintf( D_ALWAYS, "magic_check() failed\n" );
		return FALSE;
	}

	// Don't look for symbol "MAIN" in vanilla jobs or PVM processes
	if( job_class != PVM && job_class != VANILLA ) {	
		if( symbol_main_check(cur_ckpt) < 0 ) {
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

	  case STANDARD:
	  case PIPE:
		if( linked_for_condor() ) {
			state = RUNNABLE;
			return TRUE;
		} else {
			return FALSE;
		}

	  case VANILLA:
	  case PVM:
	  case PVMD:
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
	int		argc;
	char	*argv[ 2048 ];
	char	**argp;
	char	**envp;
	sigset_t	sigmask;
	long	arg_max;
	char	*tmp;
	char	a_out_name[ _POSIX_PATH_MAX ];
	char	shortname[ _POSIX_PATH_MAX ];
	int		user_syscall_fd;
	const	int READ_END = 0;
	const	int WRITE_END = 1;
	const 	int USER_CMD_FD = 3;
	int		pipe_fds[2];
	FILE	*cmd_fp;
	char	buf[128];
	int		wait_for = TRUE;
	int		fd;

	pipe_fds[0] = -1;
	pipe_fds[1] = -1;

		// We will use mkargv() which modifies its arguments in place
		// so we not use the original copy of the arguments
	if( (arg_max=sysconf(_SC_ARG_MAX)) == -1 ) {
		arg_max = _POSIX_ARG_MAX;
	}
	tmp = new char [arg_max];
	strncpy( tmp, args, arg_max );

	sprintf( shortname, "condor_exec.%d.%d", cluster, proc );
	sprintf( a_out_name, "%s/%s/%s", Execute, local_dir, shortname );

		// Set up arg vector according to class of job
	switch( job_class ) {

	  case STANDARD:
	  case PIPE:
		if( pipe(pipe_fds) < 0 ) {
			EXCEPT( "pipe()" );
			dprintf( D_ALWAYS, "Pipe built\n" );
		}
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

		argv[0] = shortname;
		argv[1] = "-_condor_cmd_fd";
		argv[2] = buf;
		mkargv( &argc, &argv[3], tmp );
		break;

	  case PVM:
#if 1
		EXCEPT( "Don't know how to deal with PVM jobs" );
#else
		argv[0] = shortname;
		argv[1] = "-1";
		argv[2] = in;
		argv[3] = out;
		argv[4] = err;
		mkargv( &argc, &argv[5], tmp );
#endif
		break;

	  case VANILLA:
		argv[0] = shortname;
		mkargv( &argc, &argv[1], tmp );
		break;
	}

		// set up environment vector
	envp = env_obj.get_vector();

		// We may run more than one of these, so each needs its own
		// remote system call connection to the shadow
	if( job_class == PVM || job_class == PIPE ) {
		user_syscall_fd = NewConnection( v_pid );
	}

		// print out arguments to execve
	dprintf( D_ALWAYS, "Calling execve( \"%s\"", a_out_name );
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


			// Posix says that a privileged process executing setuid()
			// will change real, effective, and saved uid's.  Thus the
			// child process will have only it's submitting uid, and cannot
			// switch back to root or some other uid.
		if( set_root_euid() < 0 ) {
			EXCEPT( "set_root_euid()" );
		}
		if( setgid( gid ) < 0 ) {
			EXCEPT( "setgid(%d)", gid );
		}
		if( setuid( uid ) < 0 ) {
			EXCEPT( "setuid(%d)", uid );
		}

		switch( job_class ) {
		  
		  case STANDARD:
			if( chdir(local_dir) < 0 ) {
				EXCEPT( "chdir(%s)", local_dir );
			}
			close( pipe_fds[WRITE_END] );
			break;

		  case PVM:
		  case PIPE:
			if( chdir(local_dir) < 0 ) {
				EXCEPT( "chdir(%s)", local_dir );
			}
			close( pipe_fds[WRITE_END] );
			dup2( user_syscall_fd, RSC_SOCK );
			break;

		  case VANILLA:
			set_iwd();
			open_std_file( 0 );
			open_std_file( 1 );
			open_std_file( 2 );

			(void)close( RSC_SOCK );
			(void)close( CLIENT_LOG );

			break;
		}


			// Everything's ready, start it up...
		errno = 0;
		execve( a_out_name, argv, envp );

			// A successful call to execve() never returns, so it is an
			// error if we get here.  A number of errors are possible
			// but the most likely is that there is insufficient swap
			// space to start the new process.  We don't try to log
			// anything, since we have the UID/GID of the job's owner
			// and cannot write into the log files...
		dprintf( D_ALWAYS, "Exec failed - errno = %d\n", errno );
		exit( EXECFAILED );
	}

		// The parent
	dprintf( D_ALWAYS, "Started user job - PID = %d\n", pid );
	if( job_class != VANILLA ) {
			// Send the user process its startup environment conditions
		close( pipe_fds[READ_END] );
		cmd_fp = fdopen( pipe_fds[WRITE_END], "w" );
		dprintf( D_ALWAYS, "cmd_fp = 0x%x\n", cmd_fp );

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

	delete [] tmp;
	state = EXECUTING;
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
	do_unlink( core_name );

	if( rmdir(local_dir) < 0 ) {
		dprintf( D_ALWAYS,
			"Can't remove directory \"%s\" - errno = %d\n", local_dir, errno
		);
	} else {
		dprintf( D_ALWAYS, "Removed directory \"%s\"\n", local_dir );
	}
}

void
UserProc::handle_termination( int exit_st )
{
	struct stat	buf;

	exit_status = exit_st;
	exit_status_valid = TRUE;
	accumulate_cpu_time();

	if( WIFEXITED(exit_status) ) {	// exited on own accord with some status
		dprintf( D_ALWAYS,
			"Process %d eixted with status %d\n", pid, WEXITSTATUS(exit_status)
		);
		if( WEXITSTATUS(exit_status) == EXECFAILED ) {
			dprintf( D_ALWAYS,
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
		  case SIGUSR2:			// synchronous ckpt exit - execute again
			dprintf( D_ALWAYS, "Process eixted for checkpoint\n" );
			state = CHECKPOINTING;
			commit_cpu_time();
			break;
		  case SIGQUIT:			// exited for a checkpoint
			dprintf( D_ALWAYS, "Process eixted for checkpoint\n" );
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
			dprintf( D_ALWAYS, "Process eixted by request\n" );
			state = NON_RUNNABLE;
			break;
		  default:					// exited abnormally due to signal
			dprintf( D_ALWAYS, "Process eixted abnormally\n" );
			state = ABNORMAL_EXIT;
			commit_cpu_time();
		}
			
	}
	pid = 0;

	switch( state ) {

	    case CHECKPOINTING:
			core_created = FALSE;
			ckpt_transferred = TRUE;
			break;

        case ABNORMAL_EXIT:
		    if( core_is_valid(core_name) ) {
				dprintf( D_ALWAYS, "A core file was created\n" );
				core_created = TRUE;
			} else {
				dprintf( D_FULLDEBUG, "No core file was created\n" );
				core_created = FALSE;
				(void)unlink( core_name );	// remove any incomplete core
			}
			break;

		default:
			dprintf( D_FULLDEBUG, "No core file was created\n" );
			core_created = FALSE;
			break;
	}

	if( get_class() == PVMD ) {
		Running_PVMD = FALSE;
	}

}

void
UserProc::send_sig( int sig )
{
	if( !pid ) {
		dprintf( D_FULLDEBUG,
		"UserProc::send_signal() called, but user job pid NULL\n" );
		return;
	}

	set_root_euid();

	if( sig != SIGCONT ) {
		if( kill(pid,SIGCONT) < 0 ) {
			if( errno == ESRCH ) {	// User proc already exited
				return;
			}
			set_condor_euid();
			EXCEPT( "kill(%d,SIGCONT)", pid  );
		}
	}

	if( kill(pid,sig) < 0 ) {
		if( errno == ESRCH ) {	// User proc already exited
			return;
		}
		set_condor_euid();
		EXCEPT( "kill(%d,%d)", pid, sig );
	}

	set_condor_euid();

	dprintf( D_ALWAYS, "Sent signal %s to user job %d\n", SigNames.get_name(sig), pid);
}

void
UserProc::commit_ckpt()
{
	// We'd like to just rename() the file, but if we don't do the unlink(),
	// then on some platforms the "tmp_ckpt" file gets removed from the
	// directory, but the disk space doesn't get freed.

#if 0
	(void)unlink( cur_ckpt );

	if( rename(tmp_ckpt,cur_ckpt) < 0 ) {
		EXCEPT( "rename" );
	}

#endif
	state = RUNNABLE;
}

void
UserProc::store_ckpt()
{
	char	tmp_name[ _POSIX_PATH_MAX ];
	int		status;

#if 0
	if( new_ckpt_created ) {
		sprintf( tmp_name, "%s.tmp", target_ckpt );
		delay( 15 );
		status = send_file( cur_ckpt, tmp_name, REGULAR_FILE_MODE );
		if( status < 0 ) {
			dprintf( D_ALWAYS, "File NOT xferred\n" );
			dprintf( D_ALWAYS, "(Insufficient space on submitting machine?)\n");
			dprintf( D_ALWAYS, "status = %d, errno = %d\n", status, errno );
			(void)REMOTE_syscall(CONDOR_unlink,tmp_name);  // rm partial file
			ckpt_transferred = FALSE;
		} else {
			if( REMOTE_syscall(CONDOR_rename,tmp_name,target_ckpt) < 0 ) {
				EXCEPT("Remote rename \"%s\" to \"%s\"", tmp_name, target_ckpt);
			}
			commit_cpu_time();
			ckpt_transferred = TRUE;
		}
	} else {
		dprintf( D_ALWAYS, "Checkpoint file never updated - not sending\n" );
		ckpt_transferred = FALSE;
	}
#endif
}


int
UserProc::update_ckpt()
{
	int		size_estimate;
	int		free_disk;
	int		answer;
	char	*old_brk, *new_brk;

#if 0
	free_disk = free_fs_blocks( "." );

	if( !core_created ) {
		dprintf( D_ALWAYS, "No core file - checkpoint *NOT* updated\n" );
		image_size = free_disk;	// assume ran out of disk trying to creat core
		return FALSE;
	}

	size_estimate = estimate_image_size();
	dprintf( D_ALWAYS, "Estimating new ckpt needs %d kbytes\n", size_estimate );
	dprintf( D_ALWAYS, "Found %d kbytes of free disk\n", free_disk );

	if( free_disk > size_estimate ) {
		delay( 5 );
		dprintf( D_ALWAYS, "Updating checkpoint file...\n" );

		old_brk = (char *)sbrk(0);
		dprintf( D_ALWAYS, "before update break is 0x%x\n", old_brk );
		_updateckpt( tmp_ckpt, cur_ckpt, core_name );
		new_brk = (char *)sbrk(0);
		dprintf( D_ALWAYS, "after update break is 0x%x\n", new_brk );
		dprintf( D_ALWAYS,
				"difference is %d megabytes\n",
				(new_brk - old_brk) / (1024 * 1024)
                );

		image_size = physical_file_size( tmp_ckpt );  // Get real size
		if( image_size < 0 ) {
			answer = FALSE;
			image_size = free_disk;	// Assume we ran out of disk space
			dprintf( D_ALWAYS, "Checkpoint file *NOT* updated\n" );
			dprintf( D_ALWAYS, "Probably ran out of disk\n" );
		} else {
			answer = TRUE;
			new_ckpt_created = TRUE;
			dprintf( D_ALWAYS, "Checkpoint file *IS* updated\n" );
			dprintf( D_ALWAYS, "Actual ckpt file size %d kbytes\n", image_size);
		}
	} else {
		answer = FALSE;
		image_size = size_estimate;
		dprintf( D_ALWAYS, "Checkpoint file *NOT* updated\n" );
		dprintf( D_ALWAYS, "Estimated we would run out of disk\n" );
	}

	if( unlink(core_name) < 0 ) {
		EXCEPT( "unlink(%s)", core_name );
	}

	return answer;
#else
	return FALSE;
#endif
}

inline void
display_bool( int debug_flags, char *name, int value )
{
	dprintf( debug_flags, "%s = %s\n", name, value ? "TRUE" : "FALSE" );
}


/*
Look at the core file the previous checkpoint and try to guess how big
a new checkpoint file would be created from them.  Return the answer in
1024 byte units.
*/
#define SLOP 50
int
UserProc::estimate_image_size()
{
	int		answer;
	int		text_blocks;
	int		hdr_blocks;
	int		core_blocks;

	core_blocks = physical_file_size( core_name );
	text_blocks = calc_text_blocks( cur_ckpt );
	hdr_blocks = calc_hdr_blocks();

	answer = hdr_blocks + text_blocks + core_blocks + SLOP;

	dprintf( D_FULLDEBUG, "text: %d blocks\n", text_blocks );
	dprintf( D_FULLDEBUG, "core: %d blocks\n", core_blocks );
	dprintf( D_FULLDEBUG, "a.out hdr: %d blocks\n", hdr_blocks );
	dprintf( D_FULLDEBUG, "SLOP: %d blocks\n", SLOP );
	dprintf( D_FULLDEBUG, "Calculated image size: %dK\n", answer );

	return answer;

}

void
UserProc::store_core()
{
	int		core_size;
	char	virtual_working_dir[ _POSIX_PATH_MAX ];
	char	new_name[ _POSIX_PATH_MAX ];
	int		free_disk;

	if( !core_created ) {
		dprintf( D_ALWAYS, "No core file to send - probably ran out of disk\n");
		return;
	}

	core_size = physical_file_size( core_name );

	if( coredump_limit_exists ) {
		if( core_size > coredump_limit ) {
			dprintf( D_ALWAYS, "Core file size exceeds user defined limit\n" );
			dprintf( D_ALWAYS, "*NOT* sending core file.\n" );
			return;
		}
	}

	if( REMOTE_syscall(CONDOR_getwd,virtual_working_dir) < 0 ) {
		EXCEPT( "REMOTE_syscall(CONDOR_getwd)" );
	}

	free_disk = REMOTE_syscall( CONDOR_free_fs_blocks, virtual_working_dir);
	if( free_disk < 0 ) {
		EXCEPT( "REMOTE_syscall(CONDOR_free_fs_blocks)" );
	}

	dprintf( D_ALWAYS, "Core file size is %d kbytes\n", core_size );
	dprintf( D_ALWAYS, "Free disk on submitting machine is %d kbytes\n",
																free_disk );

	if( free_disk > core_size ) {
		sprintf( new_name, "%s/core.%d.%d", virtual_working_dir, cluster, proc);
		dprintf( D_ALWAYS, "Transferring core file to \"%s\"\n", new_name );
		delay( 15 );
		send_file( core_name, new_name, REGULAR_FILE_MODE );
		core_transferred = TRUE;
	} else {
		dprintf( D_ALWAYS, "*NOT* Transferring core file\n" );
		core_transferred = FALSE;
	}
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
	state = SUSPENDED;
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
UserProc::request_exit()
{
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

/*
  Create a new connection to the shadow using the existing remote
  system call stream.
*/
int
NewConnection( int id )
{
	int 	portno;
	int		syscall = CONDOR_new_connection;
	int		answer;

	SyscallStream->x_op = XDR_ENCODE;

		// Send the request
	if( !xdr_int(SyscallStream,&syscall) ) {
		EXCEPT( "Can't send CONDOR_new_connection request" );
	}
	if( !xdr_int(SyscallStream,&id) ) {
		EXCEPT( "Can't send process id for CONDOR_new_connection request" );
	}

		// Turn the stream around
	xdrrec_endofrecord( SyscallStream, TRUE);
	SyscallStream->x_op = XDR_DECODE;
	xdrrec_skiprecord( SyscallStream );

		// Read the port number
	if( !xdr_int(SyscallStream,&portno) ) {
		EXCEPT( "Can't read port number for new connection" );
	}
	if( portno < 0 ) {
		xdr_int( SyscallStream ,&errno );
		EXCEPT( "Can't get port for new connection" );
	}

	answer = connect_to_port( portno );
	dprintf(D_FULLDEBUG, "New Socket: %d\n", answer);
	return answer;
}

/*
** Make a new TCP connection with the shadow on a given port number.  Return
** the new file descriptor.
*/
int
connect_to_port( int portnum )
{
	struct sockaddr_in		sin;
	int		fd;
	int		addr_len = sizeof(sin);

	if( getpeername(RSC_SOCK,(struct sockaddr *)&sin,&addr_len) < 0 ) {
		EXCEPT( "getpeername(%d,0x%x,0x%x)", RSC_SOCK, &sin, &addr_len);
	}

	if( (fd=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

	sin.sin_port = htons( (u_short)portnum );

	if( connect(fd,(struct sockaddr *)&sin,addr_len) < 0 ) {
		EXCEPT( "connect(%d,0x%x,%d)", fd, &sin, addr_len );
	}
	return fd;
}


extern "C"
{
int
pre_open( int, int, int ) { return 0; }
}

#if 0
void
open_std_file( const char *name, int mode, int needed_fd )
{
	int		fd;

	if( (fd = open(name,mode)) < 0 ) {
		EXCEPT( "open(%s)", name );
	}
	if( fd != needed_fd ) {
		dup2( fd, needed_fd );
		close( fd );
	}
}
#endif

/*
  Open a standard file (0, 1, or 2), given its fd number.
*/
void
open_std_file( int which )
{
	char	name[ _POSIX_PATH_MAX ];
	char	buf[ _POSIX_PATH_MAX + 50 ];
	int		pipe_fd;
	int		answer;
	int		status;

	status =  REMOTE_syscall( CONDOR_std_file_info, which, name, &pipe_fd );
	if( status == IS_PRE_OPEN ) {
		EXCEPT( "Don't know how to deal with pipelined VANILLA jobs" );
	} else {
		switch( which ) {			/* it's an ordinary file */
		  case 0:
			answer = open( name, O_RDONLY, 0 );
			break;
		  case 1:
		  case 2:
			answer = open( name, O_WRONLY, 0 );
			break;
		}
	}
	if( answer < 0 ) {
		sprintf( buf, "Can't open \"%s\"", name );
		REMOTE_syscall(CONDOR_perm_error, buf );
		exit( 4 );
	} else {
		if( answer != which ) {
			dup2( answer, which );
		}
	}
}

UserProc::UserProc( STARTUP_INFO &s ) :
	cluster( s.cluster ),
	proc( s.proc ),
	uid( s.uid ),
	gid( s.gid ),
	v_pid( s.virt_pid ),
	pid( 0 ),
	exit_status_valid( FALSE ),
	exit_status( 0 ),
	soft_kill_sig( s.soft_kill_sig ),
	job_class( s.job_class ),
	ckpt_wanted( s.ckpt_wanted ),
	state( NEW ),
	new_ckpt_created( FALSE ),
	ckpt_transferred( FALSE ),
	core_created( FALSE ),
	core_transferred( FALSE ),
	image_size( -1 ),
	user_time( 0 ),
	sys_time( 0 ),
	guaranteed_user_time( 0 ),
	guaranteed_sys_time( 0 )
{
	char	buf[ _POSIX_PATH_MAX ];
	char	*value;

	cmd = new char [ strlen(s.cmd) + 1 ];
	strcpy( cmd, s.cmd );

	args = new char [ strlen(s.args) + 1 ];
	strcpy( args, s.args );

	env = new char [ strlen(s.env) + 1 ];
	strcpy( env, s.env );

		// Generate a directory where process can run and do its checkpointing
	sprintf( buf, "dir_%d", proc_index++ );
	local_dir = new char [ strlen(buf) + 1 ];
	strcpy( local_dir, buf );
	if( mkdir(local_dir,LOCAL_DIR_MODE) < 0 ) {
		EXCEPT( "mkdir(%s,0%o)", local_dir, LOCAL_DIR_MODE );
	}

	sprintf( buf, "%s/condor_exec.%d.%d", local_dir, cluster, proc );
	cur_ckpt = new char [ strlen(buf) + 1 ];
	strcpy( cur_ckpt, buf );

	sprintf( buf, "%s/core", local_dir );
	core_name = new char [ strlen(buf) + 1 ];
	strcpy( core_name, buf );

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
	char	iwd[ _POSIX_PATH_MAX ];
	char	buf[ _POSIX_PATH_MAX + 50 ];

	if( REMOTE_syscall(CONDOR_get_iwd,iwd) < 0 ) {
		REMOTE_syscall(
			CONDOR_perm_error,
			"Can't determine initial working directory"
		);
		exit( 4 );
	}
	if( chdir(iwd) < 0 ) {
		sprintf( buf, "Can't open working directory \"%s\"", iwd );
		REMOTE_syscall( CONDOR_perm_error, buf );
		exit( 4 );
	}
}

extern "C"	int MappingFileDescriptors();

extern "C" int
MarkOpen( const char *file, int flags, int fd, int is_remote )
{
	if( MappingFileDescriptors() ) {
		EXCEPT( "MarkOpen() called, but should never be used!" );
	}
	return fd;
}
