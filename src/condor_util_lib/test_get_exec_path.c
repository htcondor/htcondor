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

/* 
   This is not really meant to be an automated test (though it could
   possibly be modified to be one.  It was just written to verify that
   this function worked while it was being developed.

   If you give this program any argument (it doesn't matter what it
   is), it will fork off a child process which will exec() the test
   program again, attempting to fool it as best it can (giving a
   faulty argv[0], not giving a full path to exec() and relying on the
   PATH, etc.)
*/

#include "condor_common.h"

extern char* getExecPath();

void fool_child( void );

int
main( int argc, char* argv[] )
{
	char* my_path = getExecPath();
	char* my_cwd = getcwd( NULL, _POSIX_PATH_MAX );

	int i;
	for( i=0; i<argc; i++ ) {
		printf( "argv[%d]: %s\n", i, argv[i] );
	}

	printf( "my_path: %s\n", my_path );
	printf( "my_cwd: %s\n", my_cwd );

	if( argc > 1 ) {
		fool_child();
	}

	exit( 0 );
}


void
fool_child( void )
{
	int pid;
	int status;
	pid = fork();
	if( pid ) {
			/* parent */
		waitpid( pid, &status, 0 );
		printf( "in parent: child returned %s %d\n",
				WIFSIGNALED(status) ? "signal" : "status",
				WIFSIGNALED(status) ? WTERMSIG(status) : 
				WEXITSTATUS(status) );
	} else {
			/* child */
		char* const argv[2] = { "this_is_a_lie", NULL };
		execvp( "test_get_exec_path", argv );
		fprintf( stderr, "execv() failed: %d (%s)\n",
				 errno, strerror(errno) );
	}
}



/* stub so we don't try to bring in all the C++ code */
void
dprintf(int flags, char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
