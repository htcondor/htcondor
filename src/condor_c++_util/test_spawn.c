/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
