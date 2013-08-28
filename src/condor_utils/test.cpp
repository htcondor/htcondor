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

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include <signal.h>
#include <sys/wait.h>



void spawn( int );
void reap();

volatile int	N_Children;

main( argc, argv )
int		argc;
char	*argv[];
{
	char	**ptr;
	struct sigaction	act;
	sigset_t	mask;

	config();

	act.sa_handler = reap;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;

	if( sigaction(SIGCHLD,&act,0) < 0 ) {
		EXCEPT( "sigaction" );
	}

	spawn( 1 );

	for( ptr = ++argv; *ptr; ptr++ ) {
		do_it( *ptr );
	}

	sigemptyset( &mask );
	while( N_Children ) {
		sigsuspend( &mask );
	}
}

do_it( name )
char	*name;
{
	dprintf( D_ALWAYS, "%s: %" PRIi64d " K free\n", name, sysapi_disk_space(name) );
}

int
SetSyscalls( foo )
int	foo;
{
	return foo;
}

void
reap()
{
	pid_t	pid;
	int		status;

	if( (pid=wait(&status)) < 0 ) {
		EXCEPT( "wait" );
	}

	if( WIFEXITED(status) ) {
		printf(
			"Process %d exited with status %d\n",
			pid,
			WEXITSTATUS(status)
		);
	} else {
		printf(
			"Process %d terminated by signal %d\n",
			pid,
			WTERMSIG(status)
		);
	}
	N_Children -= 1;
}

void
spawn( int n ) 
{
	pid_t	pid;
	int		i;
	long	usec;

	for( i=0; i<n; i++ ) {

		N_Children += 1;

		switch( pid = fork() ) {

		  case -1:	/* Error */
			EXCEPT( "fork" );

		  case 0:	/* the child */
			srandom( getpid() );
			usec = random() % 1000000;
			usleep( usec );
			exit( 0 );

		  default:	/* the parent */
			printf( "Created child process %d\n", pid );

		}
	}
}
