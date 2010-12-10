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
	char* my_cwd = getcwd( NULL, 0 );

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
