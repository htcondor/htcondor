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

#define _POSIX_SOURCE
#include "condor_common.h"
#include "condor_debug.h"
#include "util_lib_proto.h"

/* For test program, use printf() & exit in place of EXCEPT */
#ifdef TEST_SPAWN
#undef EXCEPT
#define EXCEPT(__s__)	{ fprintf(stderr,"%s\n",__s__); exit(1); }
#endif

static pid_t	ChildPid = 0;

static int	READ_END = 0;
static int	WRITE_END = 1;


/*
  This is just like popen(3) except it isn't quite as smart in some
  ways, but is smarter in others.  This one is less smart in that it
  only allows for one process at a time.  It is smarter than at least
  some implementation of standard popen() in that it handles
  termination of the child process more carefully.  It is careful
  how it waits for it's child's status so that it doesn't reap status
  information for other processes which the calling code may want to
  reap.

  However, it does *NOT* "eat" SIGCHLD, since a) SIGCHLD is
  probably blocked when this method is invoked, b) there are cases
  where our attempt to eat SIGCHLD might result in eating too many of
  them, and that's really bad, c) to try to do this right would be
  very complicated code in here and, d) the worst thing that happens
  is our caller gets and extra SIGCHLD, which, in the case of
  DaemonCore, just results in a D_FULLDEBUG and nothing more.  the
  potential harm of doing this wrong far outweighs the harm of this
  extra dprintf()... Derek Wright <wright@cs.wisc.edu> 2004-05-27
*/

FILE *
my_popen( const char *cmd, const char * mode )
{
	int		pipe_d[2];
	int		parent_reads;
	uid_t	euid;
	gid_t	egid;

		/* Use ChildPid as a simple semaphore-like lock */
	if ( ChildPid ) {
		return NULL;
	}

		/* Figure out who reads and who writes on the pipe */
	parent_reads = mode[0] == 'r';

		/* Creat the pipe */
	if( pipe(pipe_d) < 0 ) {
		return NULL;
	}

		/* Create a new process */
	if( (ChildPid=fork()) < 0 ) {
			/* Clean up file descriptors */
		close( pipe_d[0] );
		close( pipe_d[1] );
		return NULL;
	}

		/* The child */
	if( ChildPid == 0 ) {
		if( parent_reads ) {
				/* Close stdin, dup pipe to stdout */
			close( pipe_d[READ_END] );
			if( pipe_d[WRITE_END] != 1 ) {
				dup2( pipe_d[WRITE_END], 1 );
				close( pipe_d[WRITE_END] );
			}
		} else {
				/* Close stdout, dup pipe to stdin */
			close( pipe_d[WRITE_END] );
			if( pipe_d[READ_END] != 0 ) {
				dup2( pipe_d[READ_END], 0 );
				close( pipe_d[READ_END] );
			}
		}
			/* to be safe, we want to switch our real uid/gid to our
			   effective uid/gid (shedding any privledges we've got).
			   we also want to drop any supplimental groups we're in.
			   we want to run this popen()'ed thing as our effective
			   uid/gid, dropping the real uid/gid.  all of these calls
			   will fail if we don't have a ruid of 0 (root), but
			   that's harmless.  also, note that we have to stash our
			   effective uid, then switch our euid to 0 to be able to
			   set our real uid/gid
			*/
		euid = geteuid();
		egid = getegid();
		seteuid( 0 );
		setgroups( 1, &egid );
		setgid( egid );
		setuid( euid );

		execl( "/bin/sh", "sh", "-c", cmd, 0 );
		_exit( ENOEXEC );		/* This isn't safe ... */
	}

		/* The parent */
	if( parent_reads ) {
		close( pipe_d[WRITE_END] );
		return( fdopen(pipe_d[READ_END],mode) );
	} else {
		close( pipe_d[READ_END] );
		return( fdopen(pipe_d[WRITE_END],mode) );
	}
}
		
int
my_pclose(FILE *fp)
{
	int			status;

		/* Close the pipe */
	(void)fclose( fp );

		/* Wait for child process to exit and get its status */
	while( waitpid(ChildPid,&status,0) < 0 ) {
		if( errno != EINTR ) {
			status = -1;
			break;
		}
	}

		/* Now return status from child process */
	return status;
}


/*
  This is similar to the UNIX system(3) call, except it doesn't invoke
  a shell.  This is much more of a fork/exec/wait call.  Perhaps you
  should think of it as the "spawn" call on old MS-DOG systems.

  It shares the child handling with my_popen(), which is why it's in
  the same source file.  See the comments for it for more details.

  Returns:
    -1: failure
    >0: Pid of child (wait == false)
    0: Success (wait == true)
*/
#define MAXARGS	32
int
my_spawnl( const char* cmd, ... )
{
	int		rval;
	int		argno = 0;

    va_list va;
	const char * argv[MAXARGS + 1];

	/* Convert the args list into an argv array */
    va_start( va, cmd );
	for( argno = 0;  argno < MAXARGS;  argno++ ) {
		const char	*p;
		p = va_arg( va, const char * );
		argv[argno] = p;
		if ( ! p ) {
			break;
		}
	}
	argv[MAXARGS] = NULL;
    va_end( va );

	/* Invoke the real spawnl to do the work */
    rval = my_spawnv( cmd, (char *const*) argv );

	/* Done */
	return rval;
}

/*
  This is similar to the UNIX system(3) call, except it doesn't invoke
  a shell.  This is much more of a fork/exec/wait call.  Perhaps you
  should think of it as the "spawn" call on old MS-DOG systems.

  It shares the child handling with my_popen(), which is why it's in
  the same source file.  See the comments for it for more details.

  Returns:
    -1: failure
    >0 == Return status of child
*/
int
my_spawnv( const char* cmd, char *const argv[] )
{
	int					status;
	uid_t euid;
	gid_t egid;

		/* Use ChildPid as a simple semaphore-like lock */
	if ( ChildPid ) {
		return -1;
	}

		/* Create a new process */
	ChildPid = fork();
	if( ChildPid < 0 ) {
		ChildPid = 0;
		return -1;
	}

		/* Child: create an ARGV array, exec the binary */
	if( ChildPid == 0 ) {
			/* to be safe, we want to switch our real uid/gid to our
			   effective uid/gid (shedding any privledges we've got).
			   we also want to drop any supplimental groups we're in.
			   the whole point of my_spawn*() is that we want to run
			   something as our effective uid/gid, and we want to do
			   it safely.  all of these calls will fail if we don't
			   have a ruid of 0 (root), but that's harmless.  also,
			   note that we have to stash our effective uid, then
			   switch our euid to 0 to be able to set our real uid/gid 
			*/
		euid = geteuid();
		egid = getegid();
		seteuid( 0 );
		setgroups( 1, &egid );
		setgid( egid );
		setuid( euid );

			/* Now it's safe to exec whatever we were given */
		execv( cmd, argv );
		_exit( ENOEXEC );		/* This isn't safe ... */
	}

		/* Wait for child process to exit and get its status */
	while( waitpid(ChildPid,&status,0) < 0 ) {
		if( errno != EINTR ) {
			status = -1;
			break;
		}
	}

		/* Now return status from child process */
	ChildPid = 0;
	return status;
}
