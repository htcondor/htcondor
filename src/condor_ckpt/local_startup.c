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

 

/*
	This is the startup routine for condor "standalone"
	checkpointing programs - i.e. checkpointing, but no remote
	system calls.

	Note: such programs must be linked so that
		int MAIN( int argc, char *argv[] )
	is called at the beginning of execution rather than
		int main( int argc, char *argv[] )

	In the case of an original invocation of the program, MAIN()
	will do the necessary initialization of the checkpoint
	mechanism, then call the user's main().  In the case of a
	restart, MAIN() will arrange to read in the checkpoint
	information and effect the restart.
*/



#define _POSIX_SOURCE

#if defined(OSF1) && !defined(__STDC__)
#	define __STDC__
#endif

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_syscall_mode.h"

#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include "condor_debug.h"

int main( int argc, char *argv[], char **envp );

extern volatile int InRestart;
int _condor_in_file_stream;

static char	*executable_name;

int
#if defined(HPUX)
_START( int argc, char *argv[], char **envp )
#else
MAIN( int argc, char *argv[], char **envp )
#endif
{
	char	buf[_POSIX_PATH_MAX];
	static char	init_working_dir[_POSIX_PATH_MAX];
	static char	init_working_dir2[_POSIX_PATH_MAX]; /* Greger */
	int		i;

	_condor_prestart( SYS_LOCAL );

	getcwd( init_working_dir, sizeof(init_working_dir) );
	if (argv[0][0] == '/') {
		executable_name = argv[0];
		/* Greger */
		strcpy(init_working_dir2, init_working_dir);
	} else {
		/* Greger */
		strcpy(init_working_dir2, init_working_dir);
		strcat(init_working_dir, "/");
		strcat(init_working_dir, argv[0]);
		executable_name = init_working_dir;
	}

		/*
		If the command line looks like 
			exec_name -_condor_ckpt <ckpt_file> ...
		then we set up to checkpoint to the given file name
		*/
	if( argc >= 3 && strcmp("-_condor_ckpt",argv[1]) == MATCH ) {
		init_image_with_file_name( argv[2] );
		fprintf(stderr, "Checkpointing to file %s\n", argv[2]);
		fflush(stderr);
#if 0
		if (access( argv[2], R_OK ) == 0) {
			restart();
		}
#endif
		Set_CWD( init_working_dir2 );
		argc -= 2;
		argv += 2;
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		InRestart = FALSE;
#if defined(HPUX)
		return(_start( argc, argv, envp ));
#else
		return main( argc, argv, envp );
#endif
	}

		/*
		If the command line looks like 
			exec_name -_condor_restart <ckpt_file> ...
		then we effect a restart from the given file name
		*/
	if( argc >= 3 && strcmp("-_condor_restart",argv[1]) == MATCH ) {
		init_image_with_file_name( argv[2] );
		restart();
	}

		/*
		If we aren't given instructions on the command line, assume
		we are an original invocation, and that we should write any
		checkpoints to the name by which we were invoked with a
		".ckpt" extension.
		*/
	if( argc < 3 || 1) {
		sprintf( buf, "%s.ckpt", argv[0] );
		init_image_with_file_name( buf );
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		InRestart = FALSE;
#if defined(HPUX)
		return(_start( argc, argv, envp ));
#else
		return main( argc, argv, envp );
#endif
	}

}

/*
  Open a stream for writing our checkpoint information.  Since we are in
  the "standalone" startup file, this is the local version.  We do it with
  a simple "open" call.  Also, we can ignore the "n_bytes" parameter here -
  we will get an error on the "write" if something goes wrong.
*/
int
open_ckpt_file( const char *ckpt_file, int flags, size_t n_bytes )
{
	if( flags & O_WRONLY ) {
		return open( ckpt_file, O_CREAT | O_TRUNC | O_WRONLY, 0664 );
	} else {
		return open( ckpt_file, O_RDONLY );
	}
}

void
report_image_size( int kbytes )
{
	/* noop in local mode */
}

int
get_ckpt_mode(int sig)
{
	/* should never be called in local mode */
	return 0;
}

int
get_ckpt_speed()
{
	/* should never be called in local mode */
	return 0;
}


char	*
condor_get_executable_name()
{
	return executable_name;
}

sigset_t
block_condor_signals()
{
	sigset_t mask, omask;

	sigfullset(&mask);
	if( sigprocmask(SIG_BLOCK,&mask,&omask) < 0 ) {
		EXCEPT( "sigprocmask" );
	}
	return omask;
}

void restore_condor_sigmask(sigset_t omask)
{
	if( sigprocmask(SIG_SETMASK,&omask,0) < 0 ) {
		EXCEPT( "sigprocmask" );
	}
}	

int
ioserver_open(const char *path, int oflag, mode_t mode)
{
	return -1;
}

off_t
ioserver_lseek(int filedes, off_t offset, int whence)
{
	return -1;
}

int
ioserver_close(int filedes)
{
	return -1;
}

#if !defined( HAS_DYNAMIC_USER_JOBS )


/* static ckpting needs to have these always fail so no dynamic segments
	get created */
int
__mmap()
{
	return -1;
}

int
_mmap()
{
	return -1;
}

int
mmap()
{
	return -1;
}

#endif
