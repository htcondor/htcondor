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
	dprintf( D_ALWAYS, "%s: %d K free\n", name, sysapi_disk_space(name) );
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
