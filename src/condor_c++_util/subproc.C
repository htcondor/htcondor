#define _POSIX_SOURCE

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include "subproc.h"
#include "condor_string.h"
#include "condor_constants.h"
#include <limits.h>

static char * search( const char *targ, char *buf );
static char * grab_substr( const char *left, const char *right, char *buf );
void do_exec( const char *pathname, const char *args );
static void eat_sigchld();
static void sigchld_handler( int );


SubProc::SubProc(
	const char *exec_name, const char *arg_string, const char *rw ) :
	is_running( FALSE ),
	has_run( FALSE ),
	saved_errno( 0 ),
	fp( NULL )
{
	name = new char[ strlen(exec_name) + 1 ];
	strcpy( name, exec_name );

	if( arg_string ) {
		args = new char[ strlen(arg_string) + 1 ];
		strcpy( args, arg_string );
	}

	if( rw[0] == 'r' ) {
		direction = IS_READ;
	} else {
		direction = IS_WRITE;
	}
}

void
SubProc::display()
{
	printf( "Subproc name: \"%s\"\n", name );
	printf( "\targs: \"%s\"\n", args );
	if( is_running ) {
		printf( "\tis_running: TRUE\n" );
		printf( "\tpid: %d\n", pid );
	} else {
		printf( "\tis_running: FALSE\n" );
	}
	if( has_run ) {
		printf( "\thas_run: TRUE\n" );
		printf( "\texit_status: %d\n", exit_status );
	} else {
		printf( "\thas_run: FALSE\n" );
	}
	if( saved_errno ) {
		printf( "\terrno: %d\n", saved_errno );
	}
}

SubProc::~SubProc()
{
	terminate();
	delete name;
	delete args;
}

FILE *
SubProc::run()
{
	int	pipe_fds[2];
	int stderr_fds[2];
	FILE	*answer;
	char	cmd[ _POSIX_PATH_MAX ];

		// create pipes
	if( pipe(pipe_fds) < 0 || pipe(stderr_fds) < 0 ) {
		saved_errno = errno;
		return NULL;
	}

		// fork
	if( (pid = fork()) < 0 ) {
		saved_errno = errno;
		return NULL;
	}

	if( pid == 0 ) {
			// the child

				// set up stderr so child can write to it
		close( stderr_fds[0] );
		if( stderr_fds[1] != 2 ) {
			dup2( stderr_fds[1], 2 );
		}

		if( direction == IS_READ ) {
				// set up stdin so child can write to it
			close( pipe_fds[0] );
			if( pipe_fds[1] != 1 ) {
				dup2( pipe_fds[1], 1 );
			}
		} else {
				// set up stdin so child can read from it
			if( pipe_fds[0] != 0 ) {
				dup2( pipe_fds[0], 0 );
			}
			close( pipe_fds[1] );
		}

		strcpy( cmd, name );
		strcat( cmd, " " );
		strcat( cmd, args );
#if 0
		execl( "/bin/sh", "sh", "-c", cmd, 0 );
#else
		do_exec( name, args );
#endif
		exit( 127 );
	} else {
			// the parent
		is_running = TRUE;
			// set up stderr so parent can read from it
		close( stderr_fds[1] );
		stderr_fp = fdopen( stderr_fds[0], "r" );

		if( direction == IS_READ ) {
				// set up stdout so parent can read from it
			close( pipe_fds[1] );
			fp = fdopen( pipe_fds[0] ,"r" );
		} else {
				// set up stdout so parent can write to it
			close( pipe_fds[0] );
			fp = fdopen( pipe_fds[1] ,"w" );
		}
	}
	return fp;
}

int
SubProc::terminate()
{

	if( !is_running ) {
		return -1;
	}
	fclose( fp );
	fp = NULL;

	fgets( err_buf, sizeof(err_buf), stderr_fp );
	fclose( stderr_fp );

	exit_status = do_wait();
	is_running = FALSE;
	has_run = TRUE;
	return exit_status;
}

char *
SubProc::get_stderr()
{
	if( !has_run ) {
		return NULL;
	}
	return err_buf;
}

int
SubProc::do_kill()
{
	if( kill(pid,SIGTERM) < 0 ) {
		saved_errno = errno;
		return -1;
	}
	return 0;
}

int
SubProc::do_wait()
{


	if( waitpid(pid,&exit_status,0) < 0 ) {
		saved_errno = errno;
		return -1;
	}

		// System may or may not have sent a SIGCHLD, get rid of it if so
	eat_sigchld();

	return 0;
}

/*
  Run the subprocess, and do simple parsing of its output.  We're looking
  for a simple string which is the "result" of the process, and
  return a pointer to that.  We assume the process may produce multiple
  lines of output, and we first identify the "interesting" line as the
  one containing the pattern "targ".  Once we have identified the
  correct output line, we need to find the result string.  This is
  the part inbetween the strings "left", and "right".  All of these
  are simple strings - no regular expression matching.
*/
char *
SubProc::parse_output( const char *targ, const char *left, const char *right )
{
	FILE	*fp;
	static char buf[512];

	if( (fp=run()) == NULL ) {
		return NULL;
	}

	while( fgets(buf,sizeof(buf),fp) ) {
		if( search(targ,buf) ) {
			terminate();
			return grab_substr( left, right, buf );
		}
	}
	terminate();
	return NULL;
}

static char *
search( const char *targ, char *buf )
{
	char	*ptr;
	int		n;

	n = strlen( targ );

	for( ptr=buf; *ptr; ptr++ ) {
		if( strncmp(targ,ptr,n) == 0 ) {
			return ptr;
		}
	}
	return FALSE;
}

static char *
grab_substr( const char *left, const char *right, char *buf )
{
	char	*answer;
	char	*ptr;

	answer = search( left, buf );
	if( answer == NULL ) {
		return answer;
	}

	answer += strlen( left );

	if( ptr = search(right,answer) ) {
		*ptr = '\0';
		return answer;
	} else {
		return NULL;
	}
}
static SubProc	*Proc;

extern "C" {

FILE *
execute_program( const char *cmd, const char *args, const char *mode )
{
	FILE	*fp;

	Proc = new SubProc( cmd, args, mode );
	fp = Proc->run();
	return fp;
}

terminate_program()
{
	int		status;

	if( !Proc ) {
		return -1;
	}
	status = Proc->terminate();
	delete Proc;
	return status;
}

}

void
do_exec( const char *pathname, const char *args )
{
	char	buf[ 1024 ];
	char	shortname[ _POSIX_PATH_MAX ];
	char	*argv[ 64 ];
	int		argc;
	char	*ptr;

		// get simple program name
	if( ptr = strrchr(pathname,'/') ) {
		strcpy( shortname, ptr + 1 );
	} else {
		strcpy( shortname, pathname );
	}

		// set up arg vector
	strcpy( buf, args );
	argv[0] = shortname;
	mkargv( &argc, &argv[1], buf );

	execv( pathname, argv );

		// Should not get here
	exit( 127 );
}

/*
  If SIGCHLD was blocked at the time we ran our subprocess, the system
  may have sent us one, which is now pending.  If that's true, we
  clean it up here so that code which calls this package doesn't have
  to worry about it.

  N.B. This tries hard, but is still not completely safe.  If we have
  an occurrence of SIGCHLD pending, and before we get here, another
  child exits, then we would get another occurrence of the signal, but
  since only one occurrence can be pending at a time, we would loose
  that information here, i.e. we end up with no occurrences of SIGCHLD
  pending, but the calling code may wait for one...
*/

static void
eat_sigchld()
{
	sigset_t	sig_set;
	struct sigaction	action, old_action;

		// Find out of SIGCHLD is pending
	sigpending( &sig_set );
	if( !sigismember(&sig_set,SIGCHLD) ) {
		return;
	}

		// set up our own handler for SIGCHLD
	action.sa_handler = sigchld_handler;
	sigemptyset( &action.sa_mask );
	action.sa_flags = 0;
	sigaction( SIGCHLD, &action, &old_action );

		// let the SIGCHLD through 
	sigfillset( &sig_set );
	sigdelset( &sig_set, SIGCHLD );
	sigsuspend( &sig_set );

		// restore previous SIGCHLD handling
	sigaction( SIGCHLD, &old_action, 0 );
}

static void
sigchld_handler( int signo )
{
}
