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
