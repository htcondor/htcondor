#include "condor_common.h"
#include "user_log.h"

main( int argc, char *argv[] )
{
	int		i;

	int		cluster, proc, subproc;

	if( argc != 4 ) {
		fprintf( stderr, "Usage: %s cluster proc subproc\n", argv[0] );
		exit( 1 );
	}

	cluster = atoi( argv[1] );
	proc = atoi( argv[2] );
	subproc = atoi( argv[3] );

	UserLog log( "condor", "/tmp/condor", cluster, proc, subproc );
	log.display();
	log.put( "At line %d in %s\n", __LINE__, __FILE__ );
	log.put( "At line %d in %s\n", __LINE__, __FILE__ );

	log.begin_block();
		for( i=0; i<10; i++ ) {
			log.put( "This is line %d of output\n", i );
		}
	log.end_block();


	exit( 0 );
}

extern "C" int SetSyscalls( int mode ) { return mode; }

