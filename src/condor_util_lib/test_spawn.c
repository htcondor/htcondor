#include "condor_common.h"
#include "util_lib_proto.h"

int main( int argc, const char *argv[] )
{
	int		status;
	int		i;

	/* Check the command line */
	if ( argc < 2 ) {
		fprintf( stderr, "usage: test_spawn program [args]\n" );
		exit( 1 );
	}

	/* Test spawnv */
	char *const *targv = (char *const*) argv;
	printf( "\nTesting spawnv / wait=TRUE:\n" );
	printf( "  running: '" );
	for( i = 1;  i < argc;  i++ ) {
		printf( "%s ", argv[i] );
	}
	printf( "'\n" );
	status = my_spawnv( argv[1], TRUE, targv+1 );
	printf( "  spawnv returned %d\n", status );

	printf( "\nTesting spawnv / wait=FALSE:\n" );
	printf( "  running: '" );
	for( i = 1;  i < argc;  i++ ) {
		printf( "%s ", argv[i] );
	}
	printf( "'\n" );
	status = my_spawnv( argv[1], FALSE, targv+1 );
	printf ("  spawnv returned pid = %d; waiting\n", status );
	if ( status > 0 ) {
		status = my_spawn_wait( );
		printf ("  spawn_wait returned %d\n", status );
	}

	/* Test spawnl */
	printf( "\nTesting spawnl / wait=TRUE:\n" );
	printf( "  Running: '/bin/echo a b c d'\n" );
	status = my_spawnl( "/bin/echo", TRUE,
						"echo", "a", "b", "c", "d", NULL );
	printf( "  spawnl returned %d\n", status );

	printf( "\nTesting spawnl / wait=FALSE:\n" );
	printf( "  Running: '/bin/echo 1 2 3'\n" );
	status = my_spawnl( "/bin/echo", FALSE, "echo", "1", "2", "3", NULL );
	printf ("  spawnl returned pid = %d; waiting\n", status );
	if ( status > 0 ) {
		status = my_spawn_wait( );
		printf ("  spawn_wait returned %d\n", status );
	}

	return 0;
}
