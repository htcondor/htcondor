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
	int		i, should_restart = FALSE;
	char*	ckpt_file = NULL;
	char*	arg;
	

		/*
		  These will hold the argc and argv we actually give to the
		  user's code, with any Condor-specific flags stripped out. 
		*/
	int		user_argc;
	char*	user_argv[_POSIX_ARG_MAX];

		/* The first arg will always be the same */
	user_argv[0] = argv[0];
	user_argc = 1;

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
		   Now, process our command-line args to see if there are
		   any ones that begin with "-_condor" and deal with them. 
		 */
	for( i=1; (i<argc && argv[i]); i++ ) {
		if( (strncmp(argv[i], "-_condor_", 9) != MATCH ) ) {
				/* Non-Condor arg so save it. */
			user_argv[user_argc] = argv[i];
			user_argc++;
			continue;
		}
		
			/* 
			   This must be a Condor arg.  Let's just deal with the 
			   part of it that'll be unique...
			*/
		arg = argv[i]+9;
		
			/* 
			   '-_condor_ckpt <ckpt_filename>' is used to specify the 
			   file we want to checkpoint to.
			*/
		if( (strcmp(arg, "ckpt") == MATCH) ) {
			if( ! argv[i+1] ) {
				fprintf( stderr, 
						 "ERROR: -_condor_ckpt requires another argument\n" );
				exit( 1 );
			} else {
				i++;
				ckpt_file = argv[i];
				fprintf( stderr, "Checkpointing to file %s\n", ckpt_file );
				fflush( stderr );
			 }	
			continue;
		}
		
			/*
			  '-_condor_restart <ckpt_file>' is used to specify that
			  we should restart from the given checkpoint file.
			*/
		if( (strcmp(arg, "restart") == MATCH) ) {
			if( ! argv[i+1] ) {
				fprintf( stderr, 
						 "ERROR: -_condor_restart requires another argument\n" );
				exit( 1 );
			} else {
				i++;
				ckpt_file = argv[i];
				should_restart = TRUE;
			}
			continue;
		}
		
			 /*
			   '-_condor_D_XXX' can be set to add the D_XXX
			   (e.g. D_ALWAYS) debug level for this job's output.
			   This is for debug messages inside our checkpointing and
			   remote syscalls code.  If a user sets a given level,
			   messages from that level go to stderr.  
			   -Derek Wright 9/30/99
			 */
		if( (strncmp(arg, "D_", 2) == MATCH) ) {
			_condor_set_debug_flags( arg );
			continue;
		}
	}

		/*
		  We're done processing all the args, so copy the 
		  user_arg* stuff back into the real argv and argc. 
		  First, we must terminate the final element.
		*/
	user_argv[user_argc] = NULL;
	argc = user_argc;
	for( i=0; i<=argc; i++ ) {
		argv[i] = user_argv[i];
	}

		/* 
		   Now, if the user wants it, dprintf() is configured.  So,
		   we're safe to dprintf() our version string.  This serves
		   many purposes: 1) it means anything linked with any Condor
		   library, standalone or regular, will reference the
		   CondorVersion() symbol, so our magic-ident string will
		   always be included by the linker, 2) In standalone jobs,
		   the user only gets the clutter if they want it.
		   -Derek Wright 5/26/99
		*/
	dprintf( D_ALWAYS | D_NOHEADER , "User Job - %s\n", CondorVersion() );
	dprintf( D_ALWAYS | D_NOHEADER , "User Job - %s\n", CondorPlatform() );

	if( ! ckpt_file ) {
			/*
			  If we aren't given instructions on the command line,
			  assume we are an original invocation, and that we
			  should write any checkpoints to the name by which we
			  were invoked with a ".ckpt" extension.  */
		sprintf( buf, "%s.ckpt", argv[0] );
		ckpt_file = buf;
	}
	init_image_with_file_name( ckpt_file );
	 
	if( should_restart ) {
		restart();
	} else {
		Set_CWD( init_working_dir2 );
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
