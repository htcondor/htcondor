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
#include "my_popen.h"

int main( int argc, const char *argv[] )
{
	int		status;
	int		i;

	/* Check the command line */
	if ( argc < 2 ) {
		fprintf( stderr, "usage: test_spawn program [args]\n" );
		exit( 1 );
	}

		/* set our effective uid/gid for testing */
	setegid( 99 );
	seteuid( 99 );



	/* Test spawnv */
	char *const *targv = (char *const*) argv;
	printf( "\nTesting spawnv:\n" );
	printf( "  running: '" );
	for( i = 1;  i < argc;  i++ ) {
		printf( "%s ", argv[i] );
	}
	printf( "'\n" );
	status = my_spawnv( argv[1], targv+1 );
	printf( "  spawnv returned %d\n", status );


	/* Test spawnl */
	printf( "\nTesting spawnl:\n" );
	printf( "  Running: '/bin/echo a b c d'\n" );
	status = my_spawnl( "/bin/echo", TRUE,
						"echo", "a", "b", "c", "d", NULL );
	printf( "  spawnl returned %d\n", status );

	return 0;
}
